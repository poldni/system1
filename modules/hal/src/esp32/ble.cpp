#include "hal/ble.hpp"
#include "hal/ble_profile.hpp"
#include "hal/logger.hpp"

// Workaround for missing configuration symbol in ESP-IDF NimBLE port
#ifndef MYNEWT_VAL_BLE_GATT_CSFC_SIZE
#define MYNEWT_VAL_BLE_GATT_CSFC_SIZE 3
#endif

#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <mutex>
#include <optional>

namespace system1::hal
{

namespace
{
    // BLE State
    bool g_stack_initialized = false;
    bool g_device_connected = false;
    uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    
    // Attribute Handles
    uint16_t g_tracking_handle = 0;
    uint16_t g_settings_handle = 0;

    // Settings synchronization
    std::mutex g_settings_mutex;
    std::optional<DeviceSettings> g_pending_settings;
    DeviceSettings g_current_settings = { .sensitivity = 50, .led_brightness = 128, .reporting_interval_ms = 70 };

    // Task Resources for NimBLE Host
    constexpr size_t BLE_HOST_STACK_SIZE = 4096;
    StackType_t g_ble_host_stack[BLE_HOST_STACK_SIZE];
    StaticTask_t g_ble_host_task_tcb;

    // UUIDs
    // Helper to convert Big Endian std::array to Little Endian ble_uuid128_t required by NimBLE
    ble_uuid128_t make_uuid128(const std::array<std::uint8_t, 16>& uuid_bytes) {
        ble_uuid128_t uuid;
        uuid.u.type = BLE_UUID_TYPE_128;
        for (size_t i = 0; i < 16; ++i) {
            uuid.value[i] = uuid_bytes[15 - i];
        }
        return uuid;
    }

    const ble_uuid128_t g_svc_uuid = make_uuid128(BleProfile::ServiceUuid);
    const ble_uuid128_t g_tracking_uuid = make_uuid128(BleProfile::TrackingDataCharUuid);
    const ble_uuid128_t g_settings_uuid = make_uuid128(BleProfile::SettingsCharUuid);

    int gatt_svr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
    {
        const ble_uuid_t* uuid = ctxt->chr->uuid;

        // Handle Settings Characteristic
        if (ble_uuid_cmp(uuid, &g_settings_uuid.u) == 0) {
            std::lock_guard lock(g_settings_mutex);

            if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
                // Respond with current settings
                int rc = os_mbuf_append(ctxt->om, &g_current_settings, sizeof(g_current_settings));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            } 
            else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
                if (OS_MBUF_PKTLEN(ctxt->om) == sizeof(DeviceSettings)) {
                    DeviceSettings settings;
                    // Copy data from mbuf to struct
                    int rc = os_mbuf_copydata(ctxt->om, 0, sizeof(DeviceSettings), &settings);
                    if (rc == 0) {
                        g_current_settings = settings;
                        g_pending_settings = settings;
                        hal::log(LogLevel::Info, "BLE", "Received Settings Update");
                        return 0;
                    }
                }
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
        }

        return BLE_ATT_ERR_UNLIKELY;
    }

    const struct ble_gatt_svc_def g_gatt_svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &g_svc_uuid.u,
            .characteristics = (struct ble_gatt_chr_def[]){
                {
                    .uuid = &g_tracking_uuid.u,
                    .access_cb = gatt_svr_access_cb,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &g_tracking_handle,
                },
                {
                    .uuid = &g_settings_uuid.u,
                    .access_cb = gatt_svr_access_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &g_settings_handle,
                },
                {0} // No more characteristics
            },
        },
        {0} // No more services
    };

    void gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
    {
        char buf[BLE_UUID_STR_LEN];

        switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            hal::log(LogLevel::Debug, "BLE", "Registered service {} handle={}",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                     ctxt->svc.handle);
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            hal::log(LogLevel::Debug, "BLE", "Registered characteristic {} def_handle={} val_handle={}",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                     ctxt->chr.def_handle,
                     ctxt->chr.val_handle);
            break;
        case BLE_GATT_REGISTER_OP_DSC:
            hal::log(LogLevel::Debug, "BLE", "Registered descriptor {} handle={}",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                     ctxt->dsc.handle);
            break;
        }
    }

    void ble_app_advertise()
    {
        struct ble_gap_adv_params adv_params;
        struct ble_hs_adv_fields fields;
        int rc;

        memset(&fields, 0, sizeof fields);
        fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
        fields.tx_pwr_lvl_is_present = 1;
        fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
        fields.name = (uint8_t *)"System1_BLE";
        fields.name_len = 11;
        fields.name_is_complete = 1;

        rc = ble_gap_adv_set_fields(&fields);
        if (rc != 0) {
            hal::log(LogLevel::Error, "BLE", "Error setting adv fields: {}", rc);
            return;
        }

        memset(&adv_params, 0, sizeof adv_params);
        adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

        rc = ble_gap_adv_start(0, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
        if (rc != 0) {
            hal::log(LogLevel::Error, "BLE", "Error starting adv: {}", rc);
        }
    }

    int ble_gap_event(struct ble_gap_event *event, void *arg)
    {
        switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_conn_handle = event->connect.conn_handle;
                g_device_connected = true;
                hal::log(LogLevel::Info, "BLE", "Connected");
            } else {
                ble_app_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            g_device_connected = false;
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            hal::log(LogLevel::Info, "BLE", "Disconnected");
            ble_app_advertise();
            break;
        }
        return 0;
    }

    void ble_host_task(void *param)
    {
        nimble_port_run();
        nimble_port_freertos_deinit();
    }

    void on_sync()
    {
        int rc = ble_hs_util_ensure_addr(0);
        if (rc != 0) {
            hal::log(LogLevel::Error, "BLE", "Device addr check failed: {}", rc);
            return;
        }
        ble_app_advertise();
    }
}

BleSender::BleSender()
{
    if (g_stack_initialized) return;

    nimble_port_init();
    
    ble_hs_cfg.sync_cb = on_sync;
    
    int rc = ble_gatts_count_cfg(g_gatt_svcs);
    if (rc != 0) return;

    rc = ble_gatts_add_svcs(g_gatt_svcs);
    if (rc != 0) return;

    ble_hs_cfg.gatts_register_cb = gatt_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Create Static Task for NimBLE Host instead of dynamic nimble_port_freertos_init()
    xTaskCreateStatic(ble_host_task, "ble_host", BLE_HOST_STACK_SIZE, 
                      NULL, (configMAX_PRIORITIES - 1), g_ble_host_stack, &g_ble_host_task_tcb);

    g_stack_initialized = true;
}

std::expected<void, BleError> BleSender::send(std::span<const std::uint8_t> data)
{
    if (!g_device_connected) {
        return std::unexpected(BleError::NotConnected);
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data.data(), data.size());
    if (!om) {
        return std::unexpected(BleError::InternalError);
    }

    int rc = ble_gattc_notify_custom(g_conn_handle, g_tracking_handle, om);
    if (rc != 0) {
        return std::unexpected(BleError::TransmissionFailed);
    }

    return {};
}

std::optional<DeviceSettings> BleSender::get_pending_settings()
{
    std::lock_guard lock(g_settings_mutex);
    auto settings = g_pending_settings;
    g_pending_settings.reset(); // Clear after reading
    return settings;
}

bool BleSender::is_connected() const { return g_device_connected; }

} // namespace system1::hal
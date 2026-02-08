#include "hal/ble.hpp"
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

namespace system1::hal
{

namespace
{
    // BLE State
    bool g_stack_initialized = false;
    bool g_device_connected = false;
    uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    uint16_t g_attr_handle = 0;

    // Task Resources for NimBLE Host
    constexpr size_t BLE_HOST_STACK_SIZE = 4096;
    StackType_t g_ble_host_stack[BLE_HOST_STACK_SIZE];
    StaticTask_t g_ble_host_task_tcb;

    // UUIDs
    // Service: 59 5a 08 e4 - 86 2a - 46 34 - 8d 99 - 39 16 57 76 dd 5a
    // Char:    59 5a 08 e5 - ...
    const ble_uuid128_t g_svc_uuid = BLE_UUID128_INIT(
        0x5a, 0xdd, 0x76, 0x57, 0x16, 0x39, 0x99, 0x8d,
        0x34, 0x46, 0x2a, 0x86, 0xe4, 0x08, 0x5a, 0x59);

    const ble_uuid128_t g_chr_uuid = BLE_UUID128_INIT(
        0x5a, 0xdd, 0x76, 0x57, 0x16, 0x39, 0x99, 0x8d,
        0x34, 0x46, 0x2a, 0x86, 0xe5, 0x08, 0x5a, 0x59);

    int gatt_svr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
    {
        // Handle Write Request from Phone
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            // Access the data sent by the phone
            // uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            // void *data = ctxt->om->om_data;
            
            // Log receiving data (Example)
            hal::log(LogLevel::Info, "BLE", "Received Write Request");
            
            return 0; // Success
        }

        return 0; // No read/write supported, only notify
    }

    const struct ble_gatt_svc_def g_gatt_svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &g_svc_uuid.u,
            .characteristics = (struct ble_gatt_chr_def[]){
                {
                    .uuid = &g_chr_uuid.u,
                    .access_cb = gatt_svr_access_cb,
                    .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE,
                    .val_handle = &g_attr_handle,
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

    int rc = ble_gattc_notify_custom(g_conn_handle, g_attr_handle, om);
    if (rc != 0) {
        return std::unexpected(BleError::TransmissionFailed);
    }

    return {};
}

bool BleSender::is_connected() const { return g_device_connected; }

} // namespace system1::hal
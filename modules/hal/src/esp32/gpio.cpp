#include "hal/gpio.hpp"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <array>
#include <algorithm>

namespace system1::hal
{

namespace 
{
    // ESP32-C6 LEDC Resource Management
    // We use a simple static allocation strategy for LEDC channels to avoid heap usage.
    constexpr int MAX_LEDC_CHANNELS = LEDC_CHANNEL_MAX;
    
    struct LedcResource {
        std::array<int8_t, GPIO_NUM_MAX> pin_map;
        int next_channel = 0;
        bool timer_initialized = false;

        constexpr LedcResource() : pin_map(), next_channel(0), timer_initialized(false) {
            pin_map.fill(-1);
        }
    };

    // Static instance to manage LEDC resources
    LedcResource& get_ledc_resource() {
        static LedcResource resource;
        return resource;
    }

    int configure_pwm(int pin_id) {
        auto& res = get_ledc_resource();

        if (pin_id < 0 || pin_id >= GPIO_NUM_MAX) return -1;

        // Check if already allocated
        if (res.pin_map[pin_id] != -1) {
            return res.pin_map[pin_id];
        }

        if (res.next_channel >= MAX_LEDC_CHANNELS) {
            return -1; // Exhausted channels
        }

        // Initialize Timer 0 on first use
        if (!res.timer_initialized) {
            ledc_timer_config_t ledc_timer = {
                .speed_mode       = LEDC_LOW_SPEED_MODE,
                .duty_resolution  = LEDC_TIMER_13_BIT,
                .timer_num        = LEDC_TIMER_0,
                .freq_hz          = 5000,  // 5 kHz
                .clk_cfg          = LEDC_AUTO_CLK,
                .deconfigure      = false
            };
            if (ledc_timer_config(&ledc_timer) != ESP_OK) {
                return -1;
            }
            res.timer_initialized = true;
        }

        int channel = res.next_channel++;
        res.pin_map[pin_id] = static_cast<int8_t>(channel);

        ledc_channel_config_t ledc_channel = {
            .gpio_num       = pin_id,
            .speed_mode     = LEDC_LOW_SPEED_MODE,
            .channel        = static_cast<ledc_channel_t>(channel),
            .intr_type      = LEDC_INTR_DISABLE,
            .timer_sel      = LEDC_TIMER_0,
            .duty           = 0,
            .hpoint         = 0,
            .flags          = { .output_invert = 0 }
        };
        
        if (ledc_channel_config(&ledc_channel) != ESP_OK) {
            return -1;
        }

        return channel;
    }
}

GpioPin::GpioPin(int pin_id, Mode mode) : id_(pin_id)
{
    if (pin_id < 0 || pin_id >= GPIO_NUM_MAX) return;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin_id);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    switch (mode) {
        case Mode::Input:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Default to pull-up for buttons
            gpio_config(&io_conf);
            break;
        case Mode::Output:
            io_conf.mode = GPIO_MODE_OUTPUT;
            gpio_config(&io_conf);
            break;
        case Mode::Pwm:
            configure_pwm(pin_id);
            break;
    }
}

void GpioPin::set(State state)
{
    gpio_set_level(static_cast<gpio_num_t>(id_), state == State::High ? 1 : 0);
}

GpioPin::State GpioPin::get() const
{
    return gpio_get_level(static_cast<gpio_num_t>(id_)) ? State::High : State::Low;
}

void GpioPin::set_pwm(float duty_cycle)
{
    auto& res = get_ledc_resource();
    if (id_ < 0 || id_ >= GPIO_NUM_MAX) return;

    int channel = res.pin_map[id_];
    if (channel == -1) return;

    // 13-bit resolution: 0 to 8191
    uint32_t duty = static_cast<uint32_t>(duty_cycle * 8191.0f);
    if (duty > 8191) duty = 8191;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel), duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel));
}

} // namespace system1::hal
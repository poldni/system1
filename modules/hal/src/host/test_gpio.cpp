#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <cstdint>
#include <ftdi.h>

// Include the source file under test's header
#include "hal/gpio.hpp"
// Include the context header to access the singleton for resetting state
#include "hal/src/host/ftdi_context.hpp"

// --- Mock Infrastructure ---

// Global state to simulate the FTDI device registers
static bool g_mock_connected = true;
static uint8_t g_adbus_val = 0;
static uint8_t g_adbus_dir = 0;
static uint8_t g_acbus_val = 0;
static uint8_t g_acbus_dir = 0;
static std::vector<uint8_t> g_read_queue;

// Mock implementations of libftdi functions
extern "C" {
    int ftdi_init(struct ftdi_context *ftdi) {
        return 0;
    }

    void ftdi_deinit(struct ftdi_context *ftdi) {
    }

    int ftdi_set_interface(struct ftdi_context *ftdi, enum ftdi_interface interface) {
        return 0;
    }

    int ftdi_usb_open(struct ftdi_context *ftdi, int vendor, int product) {
        return g_mock_connected ? 0 : -1;
    }

    int ftdi_usb_close(struct ftdi_context *ftdi) {
        return 0;
    }

    int ftdi_usb_reset(struct ftdi_context *ftdi) {
        return 0;
    }

    int ftdi_set_bitmode(struct ftdi_context *ftdi, unsigned char bitmask, unsigned char mode) {
        return 0;
    }
    
    const char* ftdi_get_error_string(struct ftdi_context *ftdi) {
        return "Mock Error";
    }

    // Mock write: parses MPSSE commands to update mock registers
    int ftdi_write_data(struct ftdi_context *ftdi, const unsigned char *buf, int size) {
        if (!g_mock_connected) return -1;
        
        int i = 0;
        while (i < size) {
            uint8_t cmd = buf[i++];
            if (cmd == 0x80 && i + 1 < size) { // Set Data bits LowByte (ADBUS)
                g_adbus_val = buf[i++];
                g_adbus_dir = buf[i++];
            } else if (cmd == 0x82 && i + 1 < size) { // Set Data bits HighByte (ACBUS)
                g_acbus_val = buf[i++];
                g_acbus_dir = buf[i++];
            } else if (cmd == 0x81) { // Get Data bits LowByte (ADBUS)
                g_read_queue.push_back(g_adbus_val);
            } else if (cmd == 0x83) { // Get Data bits HighByte (ACBUS)
                g_read_queue.push_back(g_acbus_val);
            }
            // Other commands are ignored in this simple mock
        }
        return size;
    }

    // Mock read: returns data from the read queue populated by write commands
    int ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size) {
        if (!g_mock_connected) return -1;
        if (g_read_queue.empty()) return 0;
        
        int count = 0;
        while (count < size && !g_read_queue.empty()) {
            buf[count++] = g_read_queue.front();
            g_read_queue.erase(g_read_queue.begin());
        }
        return count;
    }
}

// Helper to reset the singleton FtdiContext state between tests
namespace system1::hal {
    void reset_ftdi_context_state() {
        auto& dev = get_ftdi_instance();
        dev.adbus_val = 0x08; // Default initial state
        dev.adbus_dir = 0xFB; // Default initial state
        dev.acbus_val = 0x00;
        dev.acbus_dir = 0x00;
        dev.is_open = false; // Force re-open logic
    }
}

// --- Test Fixture ---

class GpioHostTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock hardware state
        g_mock_connected = true;
        g_adbus_val = 0;
        g_adbus_dir = 0;
        g_acbus_val = 0;
        g_acbus_dir = 0;
        g_read_queue.clear();
        
        // Reset software state
        system1::hal::reset_ftdi_context_state();
    }
};

// --- Tests ---

TEST_F(GpioHostTest, Initialization_ADBUS_Output) {
    // Pin 0 is ADBUS0
    system1::hal::GpioPin pin(0, system1::hal::GpioPin::Mode::Output);
    
    // Check if direction bit 0 is set in the mock register
    EXPECT_TRUE(g_adbus_dir & 0x01);
}

TEST_F(GpioHostTest, Initialization_ADBUS_Input) {
    // Pin 1 is ADBUS1
    system1::hal::GpioPin pin(1, system1::hal::GpioPin::Mode::Input);
    
    // Check if direction bit 1 is cleared in the mock register
    EXPECT_FALSE(g_adbus_dir & 0x02);
}

TEST_F(GpioHostTest, Initialization_ACBUS_Output) {
    // Pin 8 maps to ACBUS0 (Pin ID >= 8)
    system1::hal::GpioPin pin(8, system1::hal::GpioPin::Mode::Output);
    
    // Check if direction bit 0 of ACBUS is set
    EXPECT_TRUE(g_acbus_dir & 0x01);
}

TEST_F(GpioHostTest, Set_High_Low) {
    system1::hal::GpioPin pin(2, system1::hal::GpioPin::Mode::Output); // ADBUS2
    
    pin.set(system1::hal::GpioPin::State::High);
    EXPECT_TRUE(g_adbus_val & 0x04); // Bit 2 set
    
    pin.set(system1::hal::GpioPin::State::Low);
    EXPECT_FALSE(g_adbus_val & 0x04); // Bit 2 cleared
}

TEST_F(GpioHostTest, Get_Output_ReturnsCachedState) {
    system1::hal::GpioPin pin(3, system1::hal::GpioPin::Mode::Output); // ADBUS3
    
    pin.set(system1::hal::GpioPin::State::High);
    
    // Clear read queue to ensure no device read happens
    g_read_queue.clear();
    
    // Should return High from cache without talking to device (no read cmd sent)
    EXPECT_EQ(pin.get(), system1::hal::GpioPin::State::High);
    
    // Ensure no read command was sent (queue still empty)
    EXPECT_TRUE(g_read_queue.empty());
}

TEST_F(GpioHostTest, Get_Input_ReadsFromDevice) {
    system1::hal::GpioPin pin(4, system1::hal::GpioPin::Mode::Input); // ADBUS4
    
    // Simulate device state: Pin 4 High
    g_adbus_val |= 0x10;
    
    // GpioPin::get() should send a read command, which populates the read queue
    // via our mock ftdi_write_data, and then read it back via ftdi_read_data.
    EXPECT_EQ(pin.get(), system1::hal::GpioPin::State::High);
    
    // Simulate device state: Pin 4 Low
    g_adbus_val &= ~0x10;
    
    EXPECT_EQ(pin.get(), system1::hal::GpioPin::State::Low);
}

TEST_F(GpioHostTest, SetPwm_ThresholdLogic) {
    system1::hal::GpioPin pin(5, system1::hal::GpioPin::Mode::Pwm);
    
    // Host simulation uses simple threshold for PWM
    pin.set_pwm(0.6f); // > 0.5 -> High
    EXPECT_EQ(pin.get(), system1::hal::GpioPin::State::High);
    
    pin.set_pwm(0.4f); // <= 0.5 -> Low
    EXPECT_EQ(pin.get(), system1::hal::GpioPin::State::Low);
}

TEST_F(GpioHostTest, DeviceDisconnected_SafeFailure) {
    g_mock_connected = false;
    system1::hal::GpioPin pin(6, system1::hal::GpioPin::Mode::Output);
    
    // Should not crash, operations should be no-ops
    pin.set(system1::hal::GpioPin::State::High);
    
    // Default return value when disconnected is Low
    EXPECT_EQ(pin.get(), system1::hal::GpioPin::State::Low); 
}

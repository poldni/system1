#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <ftdi.h>

#include "hal/spi.hpp"
#include "hal/src/host/ftdi_context.hpp"

// --- Mock Infrastructure ---

// Global state to simulate the FTDI device registers and buffers
static bool g_mock_connected = true;
static std::vector<uint8_t> g_write_history;
static std::vector<uint8_t> g_read_queue;

// Mock implementations of libftdi functions
extern "C" {
    int ftdi_init(struct ftdi_context *ftdi) { return 0; }
    void ftdi_deinit(struct ftdi_context *ftdi) {}
    int ftdi_set_interface(struct ftdi_context *ftdi, enum ftdi_interface interface) { return 0; }
    int ftdi_usb_open(struct ftdi_context *ftdi, int vendor, int product) { return g_mock_connected ? 0 : -1; }
    int ftdi_usb_close(struct ftdi_context *ftdi) { return 0; }
    int ftdi_usb_reset(struct ftdi_context *ftdi) { return 0; }
    int ftdi_set_bitmode(struct ftdi_context *ftdi, unsigned char bitmask, unsigned char mode) { return 0; }
    const char* ftdi_get_error_string(struct ftdi_context *ftdi) { return "Mock Error"; }

    // Capture all writes to verify commands sent to MPSSE
    int ftdi_write_data(struct ftdi_context *ftdi, const unsigned char *buf, int size) {
        if (!g_mock_connected) return -1;
        g_write_history.insert(g_write_history.end(), buf, buf + size);
        return size;
    }

    // Return data from the read queue
    int ftdi_read_data(struct ftdi_context *ftdi, unsigned char *buf, int size) {
        if (!g_mock_connected) return -1;
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
        dev.adbus_val = 0x08; // Default initial state (CS High)
        dev.adbus_dir = 0xFB; 
        dev.acbus_val = 0x00;
        dev.acbus_dir = 0x00;
        dev.is_open = false; // Force re-open logic
    }
}

// --- Test Fixture ---

class SpiHostTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_connected = true;
        g_write_history.clear();
        g_read_queue.clear();
        system1::hal::reset_ftdi_context_state();
    }
};

// --- Tests ---

TEST_F(SpiHostTest, Transfer_FullDuplex_Success) {
    system1::hal::SpiMaster spi;
    std::vector<uint8_t> tx = {0xAA, 0xBB, 0xCC};
    std::vector<uint8_t> rx(3);
    
    // Queue data to be read back by the driver
    g_read_queue = {0x11, 0x22, 0x33};

    auto result = spi.transfer(tx, rx);
    EXPECT_TRUE(result.has_value());
    
    // Verify RX data was populated
    EXPECT_EQ(rx[0], 0x11);
    EXPECT_EQ(rx[1], 0x22);
    EXPECT_EQ(rx[2], 0x33);

    // Verify Write Sequence contains the SPI command
    // Command 0x31: Bytes Out -ve, Bytes In +ve
    bool found_spi_cmd = false;
    for (size_t i = 0; i < g_write_history.size() - 5; ++i) {
        if (g_write_history[i] == 0x31) {
            // Length bytes: Length - 1 (Little Endian)
            // Len = 3, so param = 2 (0x02 0x00)
            if (g_write_history[i+1] == 0x02 && g_write_history[i+2] == 0x00) {
                found_spi_cmd = true;
                // Verify TX data follows command
                EXPECT_EQ(g_write_history[i+3], 0xAA);
                EXPECT_EQ(g_write_history[i+4], 0xBB);
                EXPECT_EQ(g_write_history[i+5], 0xCC);
                break;
            }
        }
    }
    EXPECT_TRUE(found_spi_cmd) << "SPI Command 0x31 with length 2 and data not found";
}

TEST_F(SpiHostTest, Transfer_TxOnly) {
    system1::hal::SpiMaster spi;
    std::vector<uint8_t> tx = {0xDE, 0xAD};
    std::span<uint8_t> rx = {}; // Empty RX

    auto result = spi.transfer(tx, rx);
    EXPECT_TRUE(result.has_value());

    // Look for command 0x11 (Bytes Out -ve edge)
    bool found_cmd = false;
    for (size_t i = 0; i < g_write_history.size(); ++i) {
        if (g_write_history[i] == 0x11) {
            found_cmd = true;
            break;
        }
    }
    EXPECT_TRUE(found_cmd) << "TX Only command 0x11 not found";
}

TEST_F(SpiHostTest, Transfer_RxOnly) {
    system1::hal::SpiMaster spi;
    std::span<const uint8_t> tx = {}; // Empty TX
    std::vector<uint8_t> rx(2);
    
    g_read_queue = {0x01, 0x02};

    auto result = spi.transfer(tx, rx);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(rx[0], 0x01);

    // Look for command 0x20 (Bytes In +ve edge)
    bool found_cmd = false;
    for (size_t i = 0; i < g_write_history.size(); ++i) {
        if (g_write_history[i] == 0x20) {
            found_cmd = true;
            break;
        }
    }
    EXPECT_TRUE(found_cmd) << "RX Only command 0x20 not found";
}

TEST_F(SpiHostTest, ChipSelect_Toggles) {
    system1::hal::SpiMaster spi;
    std::vector<uint8_t> tx = {0x00};
    std::vector<uint8_t> rx(1);
    g_read_queue = {0x00};

    // Perform one transfer to ensure initialization is done
    spi.transfer(tx, rx); 
    
    // Clear history to focus on the transaction logic
    g_write_history.clear();
    g_read_queue = {0x00}; // Refill read queue
    
    spi.transfer(tx, rx);
    
    // Expect sequence:
    // 1. CS Low (MPSSE 0x80, val & ~0x08, dir)
    // 2. SPI Command ...
    // 3. CS High (MPSSE 0x80, val | 0x08, dir)
    
    ASSERT_GE(g_write_history.size(), 6);
    
    // Check first bytes for CS Low
    // 0x80 is Set Data Bits LowByte
    EXPECT_EQ(g_write_history[0], 0x80);
    // Bit 3 (0x08) should be 0 for Low
    EXPECT_EQ(g_write_history[1] & 0x08, 0x00); 
    
    // Check last bytes for CS High
    size_t last = g_write_history.size() - 3;
    EXPECT_EQ(g_write_history[last], 0x80);
    // Bit 3 (0x08) should be 1 for High
    EXPECT_EQ(g_write_history[last+1] & 0x08, 0x08); 
}

TEST_F(SpiHostTest, ConnectionFailure) {
    g_mock_connected = false;
    system1::hal::SpiMaster spi;
    std::vector<uint8_t> tx = {0x00};
    std::vector<uint8_t> rx(1);
    
    auto result = spi.transfer(tx, rx);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), system1::hal::SpiError::BusError);
}

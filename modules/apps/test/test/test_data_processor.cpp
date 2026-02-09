#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <algorithm>
#include <iostream>

#include "apps/data_processor.hpp"
#include "hal/spi.hpp"
#include "hal/ble.hpp"

// --- Dummy Logger Implementation ---
// Required to satisfy the linker as DataProcessor calls hal::log
namespace system1::hal {
    void log_write(LogLevel level, std::string_view tag, std::string_view message, const std::source_location& loc) {
        // Uncomment to see logs during test execution
        // std::cout << "[" << tag << "] " << message << "\n";
    }
}

namespace system1::app::test {

using namespace testing;
using namespace system1::hal;

// --- Mock Classes ---

class MockSpiDriver {
public:
    // Mocking the transfer method. 
    // Note: Parentheses around return type are required because of the comma in std::expected.
    MOCK_METHOD((std::expected<void, SpiError>), transfer, 
                (std::span<const std::uint8_t>, std::span<std::uint8_t>));
};

class MockBleDriver {
public:
    MOCK_METHOD((std::expected<void, BleError>), send, 
                (std::span<const std::uint8_t>));
    
    MOCK_METHOD(std::optional<DeviceSettings>, get_pending_settings, ());
};

// --- Custom Actions & Matchers ---

// Action to fill the RX buffer (2nd argument of transfer) with a specific byte value
ACTION_P(FillRxBuffer, fill_value) {
    std::span<uint8_t> rx_span = arg1;
    std::fill(rx_span.begin(), rx_span.end(), fill_value);
}

// Matcher to verify the content of a std::span passed to send()
MATCHER_P(SpanHasValue, value, "") {
    // arg is std::span<const uint8_t>
    if (arg.empty()) return false;
    return std::all_of(arg.begin(), arg.end(), [value](uint8_t b){ return b == value; });
}

// --- Test Fixture ---

class DataProcessorTest : public Test {
protected:
    // Use StrictMock to ensure no uninteresting calls happen (e.g., unexpected BLE sends)
    StrictMock<MockSpiDriver> spi;
    StrictMock<MockBleDriver> ble;
    
    // Instantiate DataProcessor with Mock types
    DataProcessor<MockSpiDriver, MockBleDriver> processor{spi, ble};
};

// --- Tests ---

TEST_F(DataProcessorTest, Step_NormalFlow_HighSignal) {
    // 1. No settings update
    EXPECT_CALL(ble, get_pending_settings())
        .WillOnce(Return(std::nullopt));

    // 2. SPI Transfer succeeds and returns strong signal (100 > default sensitivity 50)
    EXPECT_CALL(spi, transfer(_, _))
        .WillOnce(DoAll(FillRxBuffer(100), Return(std::expected<void, SpiError>{})));

    // 3. Expect BLE send to be called with the processed data
    // The pipeline copies input to output, so we expect 100s.
    EXPECT_CALL(ble, send(SpanHasValue(100)))
        .WillOnce(Return(std::expected<void, BleError>{}));

    processor.step();
}

TEST_F(DataProcessorTest, Step_Filtered_LowSignal) {
    // 1. No settings update
    EXPECT_CALL(ble, get_pending_settings())
        .WillOnce(Return(std::nullopt));

    // 2. SPI Transfer succeeds but returns weak signal (10 < default sensitivity 50)
    EXPECT_CALL(spi, transfer(_, _))
        .WillOnce(DoAll(FillRxBuffer(10), Return(std::expected<void, SpiError>{})));

    // 3. Expect BLE send to NOT be called (StrictMock will enforce this)
    
    processor.step();
}

TEST_F(DataProcessorTest, Step_SettingsUpdate_ChangesSensitivity) {
    // 1. Settings update: Increase sensitivity threshold to 200
    DeviceSettings new_settings;
    new_settings.sensitivity = 200;
    
    EXPECT_CALL(ble, get_pending_settings())
        .WillOnce(Return(new_settings));

    // 2. SPI Transfer returns signal (100). 
    // This is > old default (50) but < new sensitivity (200).
    EXPECT_CALL(spi, transfer(_, _))
        .WillOnce(DoAll(FillRxBuffer(100), Return(std::expected<void, SpiError>{})));

    // 3. Expect BLE send to NOT be called because 100 < 200
    
    processor.step();
}

TEST_F(DataProcessorTest, Step_SpiError_NoProcessing) {
    EXPECT_CALL(ble, get_pending_settings())
        .WillOnce(Return(std::nullopt));

    // 1. SPI Transfer fails
    EXPECT_CALL(spi, transfer(_, _))
        .WillOnce(Return(std::unexpected(SpiError::BusError)));

    // 2. Expect BLE send to NOT be called
    
    processor.step();
}

TEST_F(DataProcessorTest, Step_BleError_HandledGracefully) {
    EXPECT_CALL(ble, get_pending_settings())
        .WillOnce(Return(std::nullopt));

    EXPECT_CALL(spi, transfer(_, _))
        .WillOnce(DoAll(FillRxBuffer(200), Return(std::expected<void, SpiError>{})));

    // 1. BLE Send fails
    EXPECT_CALL(ble, send(_))
        .WillOnce(Return(std::unexpected(BleError::TransmissionFailed)));

    // 2. Processor should catch error and log it (verified by not crashing/throwing)
    processor.step();
}

} // namespace system1::app::test

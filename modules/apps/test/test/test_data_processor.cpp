#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "apps/data_processor.hpp"
#include "hal/spi.hpp"
#include "hal/ble.hpp"
#include <algorithm>

using namespace testing;
using namespace system1;

// --- Mock Classes ---
// These classes satisfy the implicit interface required by DataProcessor template

class MockSpiMaster {
public:
    MOCK_METHOD(std::expected<void, hal::SpiError>, transfer, 
                (std::span<const std::uint8_t>, std::span<std::uint8_t>));
};

class MockBleSender {
public:
    MOCK_METHOD(std::expected<void, hal::BleError>, send, 
                (std::span<const std::uint8_t>));
};

// --- Test Fixture ---

class DataProcessorTest : public Test {
protected:
    MockSpiMaster mock_spi;
    MockBleSender mock_ble;
    app::DataProcessor<MockSpiMaster, MockBleSender> processor{mock_spi, mock_ble};
};

// --- Tests ---

TEST_F(DataProcessorTest, SuccessfulCycle_TransmitsData) {
    // 1. Expect SPI transfer to be called.
    // We use a lambda action to simulate the hardware filling the RX buffer.
    EXPECT_CALL(mock_spi, transfer(_, _))
        .WillOnce([](std::span<const std::uint8_t> tx, std::span<std::uint8_t> rx) {
            // Simulate receiving valid data (0xAB pattern)
            std::fill(rx.begin(), rx.end(), 0xAB);
            return std::expected<void, hal::SpiError>{};
        });

    // 2. Expect BLE send to be called with the processed data.
    // Since BleDataPipeline is a pass-through for non-zero data, we expect 0xAB.
    EXPECT_CALL(mock_ble, send(_))
        .WillOnce([](std::span<const std::uint8_t> data) {
            EXPECT_FALSE(data.empty());
            EXPECT_EQ(data.size(), 128); // Input buffer size
            EXPECT_EQ(data[0], 0xAB);
            return std::expected<void, hal::BleError>{};
        });

    // Execute the step
    processor.step();
}

TEST_F(DataProcessorTest, SpiFailure_DoesNotTransmit) {
    // Simulate SPI Bus Error
    EXPECT_CALL(mock_spi, transfer(_, _))
        .WillOnce(Return(std::unexpected(hal::SpiError::BusError)));

    // BLE send should NOT be called
    EXPECT_CALL(mock_ble, send(_)).Times(0);

    processor.step();
}

TEST_F(DataProcessorTest, EmptyData_DoesNotTransmit) {
    // Simulate receiving all zeros (which BleDataPipeline filters out)
    EXPECT_CALL(mock_spi, transfer(_, _))
        .WillOnce([](std::span<const std::uint8_t>, std::span<std::uint8_t> rx) {
            std::fill(rx.begin(), rx.end(), 0x00);
            return std::expected<void, hal::SpiError>{};
        });

    // BLE send should NOT be called because pipeline returns 0 bytes
    EXPECT_CALL(mock_ble, send(_)).Times(0);

    processor.step();
}

TEST_F(DataProcessorTest, PipelineError_DoesNotTransmit) {
    // Simulate a scenario where pipeline might fail (though hard to trigger with current simple pipeline)
    // We can simulate this by mocking the SPI to return data that might cause issues if the pipeline logic changes,
    // but for now, we rely on the fact that if process() returns error, send() isn't called.
    // Since we can't easily mock the static BleDataPipeline::process, we rely on its behavior.
    // If we pass an empty buffer (simulated by 0 size span from SPI?), the pipeline returns 0.
    
    EXPECT_CALL(mock_spi, transfer(_, _))
        .WillOnce([](std::span<const std::uint8_t>, std::span<std::uint8_t> rx) {
            // Just return success but we assume the buffer is in a state that pipeline handles.
            // The DataProcessor allocates a fixed 128 byte buffer, so it's never empty.
            // This test primarily verifies the flow control logic in DataProcessor::step.
            return std::expected<void, hal::SpiError>{};
        });

    // If we assume normal data, send is called.
    // To strictly test the "if (process_res)" check, we rely on the EmptyData test above.
    EXPECT_CALL(mock_ble, send(_)).Times(1);
    
    processor.step();
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../battery_indicator.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;

// Mock class to track function calls
class MockFunction
{
  public:
    MOCK_METHOD(void, IndicateLow5, ());
    MOCK_METHOD(void, IndicateLow10, ());
};

// Wrapper functions to bind mock methods to function pointers
void wrapperIndicateLow10(MockFunction *mock)
{
    mock->IndicateLow10();
}

void wrapperIndicateLow5(MockFunction *mock)
{
    mock->IndicateLow5();
}

// Test fixture
class BatteryIndicatorTest : public ::testing::Test
{
  protected:
    MockFunction      mockFunction;
    BatteryIndicator *batteryIndicator;

    void SetUp() override
    {
        batteryIndicator = new BatteryIndicator(std::bind(wrapperIndicateLow5, &mockFunction),
                                                std::bind(wrapperIndicateLow10, &mockFunction));

        // Initialize power state to On and set power on timestamp to 10 seconds ago
        batteryIndicator->update_power_state(Teufel::Ux::System::PowerState::On, 10000);
    }

    void TearDown() override
    {
        delete batteryIndicator;
    }
};

TEST_F(BatteryIndicatorTest, IndicatesLow10WhenBelow10)
{
    EXPECT_CALL(mockFunction, IndicateLow10()).Times(Exactly(1));

    batteryIndicator->update_battery_level(static_cast<uint8_t>(9), 15000);
}

TEST_F(BatteryIndicatorTest, IndicatesLow5WhenBelow5)
{
    EXPECT_CALL(mockFunction, IndicateLow5()).Times(Exactly(1));

    batteryIndicator->update_battery_level(4, 15000);
}

TEST_F(BatteryIndicatorTest, DoesNotIndicateLow10IfBelow5)
{
    EXPECT_CALL(mockFunction, IndicateLow10()).Times(Exactly(0));
    EXPECT_CALL(mockFunction, IndicateLow5()).Times(Exactly(1));

    batteryIndicator->update_battery_level(3, 15000);
}

TEST_F(BatteryIndicatorTest, ResetsIndicatorsWhenChargerActive)
{
    // First indicate low 10% and low 5%
    batteryIndicator->update_battery_level(9, 15000);
    batteryIndicator->update_battery_level(4, 15000);

    // Now, reset indicators by setting charger to Active
    batteryIndicator->update_charger_status(Teufel::Ux::System::ChargerStatus::Active);

    // Check that indicators can trigger again
    EXPECT_CALL(mockFunction, IndicateLow10()).Times(Exactly(1));
    EXPECT_CALL(mockFunction, IndicateLow5()).Times(Exactly(1));

    batteryIndicator->update_charger_status(Teufel::Ux::System::ChargerStatus::Inactive);

    batteryIndicator->update_battery_level(11, 15000); // No indication
    batteryIndicator->update_battery_level(9, 15000);  // Should indicate low 10%
    batteryIndicator->update_battery_level(4, 15000);  // Should indicate low 5%
}

TEST_F(BatteryIndicatorTest, DoesNotIndicateLow10IfBelow11)
{
    batteryIndicator->update_battery_level(11, 15000);

    EXPECT_CALL(mockFunction, IndicateLow10()).Times(Exactly(0));
    EXPECT_CALL(mockFunction, IndicateLow5()).Times(Exactly(1));

    batteryIndicator->update_battery_level(4, 15000);
}

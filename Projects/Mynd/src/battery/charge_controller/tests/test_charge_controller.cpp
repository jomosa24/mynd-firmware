#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../charge_controller.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;

class MockChargerLLController : public IChargerLLController
{
  public:
    MOCK_METHOD(void, on_full_charge, (), ());
    MOCK_METHOD(void, enable, (bool), ());
    MOCK_METHOD(void, disable, (), ());
};

class TestableChargeController : public ChargeController
{
  public:
    // Access and modify protected members in the derived test class
    void set_charger_status(Teufel::Ux::System::ChargerStatus status)
    {
        m_charger_status = status;
    }
    Teufel::Ux::System::ChargerStatus get_charger_status() const
    {
        return m_charger_status;
    }
};

class ChargeControllerTest : public ::testing::Test
{
  protected:
    MockChargerLLController   mock_charger_ll_controller;
    TestableChargeController *charge_controller_test;

    void SetUp() override
    {
        charge_controller_test = new TestableChargeController{mock_charger_ll_controller};
    }

    void TearDown() override
    {
        delete charge_controller_test;
    }
};

TEST_F(ChargeControllerTest, InactiveToActiveEnable)
{
    EXPECT_CALL(mock_charger_ll_controller, enable(_)).Times(Exactly(1));

    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::Inactive);

    uint16_t battery_voltage_mv  = 8000;
    int16_t  battery_current_ma  = 0;
    bool     charger_ntc_allowed = true;
    bool     ac_plugged          = true;
    bool     bfc_enabled         = false;

    // 1st call inits the counter
    charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed, ac_plugged,
                                    bfc_enabled);

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::Active);
}

TEST_F(ChargeControllerTest, NotConnectedToInactiveDisable)
{
    EXPECT_CALL(mock_charger_ll_controller, disable()).Times(Exactly(1));

    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::NotConnected);

    uint16_t battery_voltage_mv  = 8000;
    int16_t  battery_current_ma  = 0;
    bool     charger_ntc_allowed = true;
    bool     ac_plugged          = true;
    bool     bfc_enabled         = false;

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::Inactive);
}

TEST_F(ChargeControllerTest, InactiveToNotConnected)
{
    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::Inactive);

    uint16_t battery_voltage_mv  = 8000;
    int16_t  battery_current_ma  = 0;
    bool     charger_ntc_allowed = true;
    bool     ac_plugged          = false;
    bool     bfc_enabled         = false;

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::NotConnected);
}

TEST_F(ChargeControllerTest, InactiveToActiveNoBfc)
{
    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::Inactive);

    uint16_t battery_voltage_mv  = 8000;
    int16_t  battery_current_ma  = 0;
    bool     charger_ntc_allowed = true;
    bool     ac_plugged          = true;
    bool     bfc_enabled         = false;

    // 1st call inits the counter
    charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed, ac_plugged,
                                    bfc_enabled);

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::Active);
}

TEST_F(ChargeControllerTest, InactiveToActiveBfc)
{
    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::Inactive);

    uint16_t battery_voltage_mv  = 7800;
    int16_t  battery_current_ma  = 0;
    bool     charger_ntc_allowed = true;
    bool     ac_plugged          = true;
    bool     bfc_enabled         = true;

    // 1st call inits the counter
    charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed, ac_plugged,
                                    bfc_enabled);

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::Active);
}

TEST_F(ChargeControllerTest, ActiveToInactiveOnFullCharge)
{
    EXPECT_CALL(mock_charger_ll_controller, disable()).Times(Exactly(1));
    EXPECT_CALL(mock_charger_ll_controller, on_full_charge()).Times(Exactly(1));

    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::Active);

    uint16_t battery_voltage_mv  = 8350;
    int16_t  battery_current_ma  = 400;
    bool     charger_ntc_allowed = true;
    bool     ac_plugged          = true;
    bool     bfc_enabled         = false;

    // 1st call inits the counter
    charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed, ac_plugged,
                                    bfc_enabled);

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::Inactive);
}

TEST_F(ChargeControllerTest, ActiveToInactiveOnNTCNotAllowed)
{
    EXPECT_CALL(mock_charger_ll_controller, disable()).Times(Exactly(1));

    charge_controller_test->set_charger_status(Teufel::Ux::System::ChargerStatus::Active);

    uint16_t battery_voltage_mv  = 8000;
    int16_t  battery_current_ma  = 0;
    bool     charger_ntc_allowed = false;
    bool     ac_plugged          = true;
    bool     bfc_enabled         = false;

    auto status = charge_controller_test->process(battery_voltage_mv, battery_current_ma, charger_ntc_allowed,
                                                  ac_plugged, bfc_enabled);

    ASSERT_EQ(status, Teufel::Ux::System::ChargerStatus::Inactive);
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../soc_estimator.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;

class TestableSocEstimator : public SocEstimator
{
  public:
    // Access and modify protected members in the derived test class
    float vbat_to_charge_convert(uint16_t vbat_mv)
    {
        return vbat_to_charge::convert(vbat_mv);
    }
    uint16_t get_vbat() const
    {
        return m_battery_voltage_mv;
    }
};

class SocEstimatorTest : public ::testing::Test
{
  protected:
    TestableSocEstimator *soc_estimator_test;

    void SetUp() override
    {
        soc_estimator_test = new TestableSocEstimator{};
    }

    void TearDown() override
    {
        delete soc_estimator_test;
    }
};

TEST_F(SocEstimatorTest, VBatToChargeConverterValue1)
{
    ASSERT_EQ(soc_estimator_test->vbat_to_charge_convert(6350), 997.396973f);
}

TEST_F(SocEstimatorTest, VBatToChargeConverterValue2)
{
    ASSERT_EQ(soc_estimator_test->vbat_to_charge_convert(7752), 14593.574219f);
}

TEST_F(SocEstimatorTest, VBatToChargeConverterValue3)
{
    ASSERT_EQ(soc_estimator_test->vbat_to_charge_convert(8400), 17640.f);
}

TEST_F(SocEstimatorTest, VBatToChargeConverterValue4)
{
    ASSERT_EQ(soc_estimator_test->vbat_to_charge_convert(6000), 0.000841f);
}
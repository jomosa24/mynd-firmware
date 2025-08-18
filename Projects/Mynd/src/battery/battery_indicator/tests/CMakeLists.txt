set(SOURCES
    ../battery_indicator.cpp
    ../battery_indicator.h
)

set(TEST_SOURCES
    test_battery_indicator.cpp
)

define_test_target(battery_indicator_test "${SOURCES}" "${TEST_SOURCES}" "${INCLUDES_FOR_TESTS}")

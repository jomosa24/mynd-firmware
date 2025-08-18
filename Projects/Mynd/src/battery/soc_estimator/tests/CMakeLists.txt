set(SOURCES
    ../soc_estimator.cpp
    ../soc_estimator.h
)

set(TEST_SOURCES
    test_soc_estimator.cpp
    mock_board.c
    mock_virtual_eeprom.c
)

define_test_target(battery_soc_test "${SOURCES}" "${TEST_SOURCES}" "${INCLUDES_FOR_TESTS}")
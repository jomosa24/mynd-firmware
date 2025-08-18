set(SOURCES
    ../charge_controller.cpp
    ../charge_controller.h
)

set(TEST_SOURCES
    test_charge_controller.cpp
    mock_board.c
)

define_test_target(battery_charge_controller "${SOURCES}" "${TEST_SOURCES}" "${INCLUDES_FOR_TESTS}")
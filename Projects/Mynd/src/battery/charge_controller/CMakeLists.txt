set(API_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/charge_controller.h
)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/charge_controller.cpp
)

# Add test directory instead of target_sources and target_include_directories if
# GTEST_BUILD is enabled
if(GTEST_BUILD)
    add_subdirectory(tests)
else()
    target_sources(${projectTarget} PRIVATE ${API_HEADERS} ${SOURCES})

    target_include_directories(${projectTarget} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    )
endif()
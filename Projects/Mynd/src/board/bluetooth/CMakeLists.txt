set(API_HEADERS
    board_link_bluetooth.h
)

if (MYND_RPI_MODIFICATION)
    message(STATUS "Replacing ${CMAKE_CURRENT_SOURCE_DIR}/board_link_bluetooth.cpp with stub for RPI modification")
    set(SOURCES
        board_link_bluetooth_stub.c
    )
else()
    set(SOURCES
        board_link_bluetooth.c
    )
endif()

target_sources(${projectTarget} PRIVATE ${API_HEADERS} ${SOURCES})

target_include_directories(${projectTarget} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

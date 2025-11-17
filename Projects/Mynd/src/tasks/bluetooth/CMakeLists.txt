set(API_HEADERS
    task_bluetooth.h
)

if(MYND_RPI_MODIFICATION)
    message(STATUS "Replacing ${CMAKE_CURRENT_SOURCE_DIR}/task_bluetooth.cpp with stub for RPi build")
    set(SOURCES
        task_bluetooth_stub.cpp
    )
else()
    set(SOURCES
        task_bluetooth.cpp
    )
endif()

target_sources(${projectTarget} PRIVATE ${API_HEADERS} ${SOURCES})

target_include_directories(${projectTarget} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

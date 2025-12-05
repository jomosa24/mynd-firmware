set(API_HEADERS
    task_priorities.h
)

target_sources(${projectTarget} PRIVATE ${API_HEADERS})

target_include_directories(${projectTarget} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_subdirectory(audio)
add_subdirectory(bluetooth)
add_subdirectory(system)
if (CMAKE_ENABLE_RPI_MOD)
    add_subdirectory(rpi)
endif()

include(ExternalProject)

function(ADD_BIN_TARGET TARGET)
    if(EXECUTABLE_OUTPUT_PATH)
        set(FILENAME "${EXECUTABLE_OUTPUT_PATH}/${TARGET}")
    else()
        set(FILENAME "${TARGET}")
    endif()

    add_custom_target(${TARGET}.bin DEPENDS ${TARGET} COMMAND ${CMAKE_OBJCOPY} -Obinary ${FILENAME} ${FILENAME}.bin)

    set_property(
            TARGET ${TARGET}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${FILENAME}.bin
    )
endfunction()

function(ADD_HEX_TARGET TARGET)
    if(EXECUTABLE_OUTPUT_PATH)
        set(FILENAME "${EXECUTABLE_OUTPUT_PATH}/${TARGET}")
    else()
        set(FILENAME "${TARGET}")
    endif()

    add_custom_target(${TARGET}.hex DEPENDS ${TARGET} COMMAND ${CMAKE_OBJCOPY} -Oihex ${FILENAME} ${FILENAME}.hex)

    set_property(
            TARGET ${TARGET}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${FILENAME}.hex
    )
endfunction()

function(ADD_ELF_HEX_BIN_TARGETS TARGET)
    if(EXECUTABLE_OUTPUT_PATH)
        set(FILENAME "${EXECUTABLE_OUTPUT_PATH}/${TARGET}")
    else()
        set(FILENAME "${TARGET}")
    endif()
    add_custom_target(${TARGET}.hex DEPENDS ${TARGET} COMMAND ${CMAKE_OBJCOPY} -Oihex ${FILENAME} ${FILENAME}.hex)
    add_custom_target(${TARGET}.bin DEPENDS ${TARGET} COMMAND ${CMAKE_OBJCOPY} -Obinary ${FILENAME} ${FILENAME}.bin)
    add_custom_target(${TARGET}.elf DEPENDS ${TARGET} COMMAND ${CMAKE_COMMAND} -E copy ${FILENAME} ${FILENAME}.elf)

    set_property(
            TARGET ${TARGET}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${FILENAME}.bin ${FILENAME}.hex ${FILENAME}.elf
    )
endfunction()

function(PRINT_SIZE_OF_TARGETS TARGET)
    if(EXECUTABLE_OUTPUT_PATH)
        set(FILENAME "${EXECUTABLE_OUTPUT_PATH}/${TARGET}")
    else()
        set(FILENAME "${TARGET}")
    endif()
    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_SIZE} ${FILENAME})
endfunction()

function(calculate_sha256sum TARGET)
    if(EXECUTABLE_OUTPUT_PATH)
        set(FILENAME "${EXECUTABLE_OUTPUT_PATH}/${TARGET}")
    else()
        set(FILENAME "${TARGET}")
    endif()
    add_custom_command(TARGET ${TARGET}
        POST_BUILD
        COMMAND sha256sum ${FILENAME} > ${FILENAME}.sha256
        COMMAND sha256sum ${FILENAME}
        VERBATIM
        COMMENT "Calculate sha256sum of ${TARGET}"
    )
endfunction()

function(add_external_binary_dependency filename url)
    # Check if running in GitLab CI
    if(DEFINED ENV{CI})
        message(STATUS "Running in GitLab CI environment, credentials required, downloading binary dependency using script.")
        set(JFROG_DOWNLOAD_SCRIPT "${CMAKE_SOURCE_DIR}/support/scripts/cicd/shell_scripts/jfrog_download_artifact.sh")
        execute_process(
            COMMAND sh ${JFROG_DOWNLOAD_SCRIPT} ${filename} ${url} "${CMAKE_SOURCE_DIR}/dl"
        )
    else()
        message(STATUS "Not running in GitLab CI environment, using default method to add external binary dependency.")
        ExternalProject_Add(${filename}_dl-from-artifactory
            URL ${url}/${filename}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            DOWNLOAD_NO_EXTRACT TRUE
            DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/dl
        )
    endif()
endfunction()

# macro for fetching gtest
macro(fetch_gtest_if_needed PROJECT_NAME)

    FetchContent_Declare(
        googletest_${PROJECT_NAME}
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest_${PROJECT_NAME})

    include(GoogleTest) # Required for gtest_discover_tests

    if (googletest_${PROJECT_NAME}_POPULATED)
        target_compile_options(gtest PRIVATE
            -DGTEST_HAS_PTHREAD=0
            -DGTEST_HAS_RTTI=0
            -DGTEST_HAS_EXCEPTIONS=0
            -D_POSIX_PATH_MAX=100
            -DGTEST_HAS_POSIX_RE=0
        )
    endif()
endmacro()

function(replace_dashes_with_underscores input_string output_string)
    string(REPLACE "-" "_" tmp_result "${input_string}")
    set(${output_string} ${tmp_result} PARENT_SCOPE)
endfunction()

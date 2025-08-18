#!/bin/sh -ex

flash_target_binary()
{
    # Generate the full JLink script with command file
    jlink_flash_script=$("$SOURCE_DIR/support/scripts/cicd/shell/generate_jlink_script.sh" "$TEST_DIR" "$jlink_flash_cmd" "$JLINK_CONFIG" "flash-target")

    # Flash target binary
    $jlink_flash_script > "$jlink_output"
}

verify_jlink_flash()
{
    # Print and verify JLink output
    cat "$jlink_output"

    error_string="Error occurred" # J-Link error string
    if grep -q "$error_string" "$jlink_output"; then
        printf "\033[31mJ-Link Error\033[0m\n"
        exit 1
    fi
}

### MAIN ###

SOURCE_DIR="$1"
TEST_DIR="$2"
JLINK_CONFIG="$3"
PATH_TO_TARGET_BIN="$4"

jlink_flash_cmd="h\nerase\nloadfile $PATH_TO_TARGET_BIN\nr\nq"
jlink_output="$TEST_DIR/jlink_flash_log.txt"

flash_target_binary

verify_jlink_flash

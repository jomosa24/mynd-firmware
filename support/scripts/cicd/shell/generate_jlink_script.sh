#!/bin/sh -ex

TEST_DIR="$1"
JLINK_CMD="$2"
JLINK_CONFIG="$3"
SCRIPT_NAME="$4"

# Build JLink command
command_file="$TEST_DIR/${SCRIPT_NAME}.JLinkScript"
printf "$JLINK_CMD" > "$command_file" # Do not use printf "%s" here, otherwise jlink cmd will not execute since it will not be interpreted correctly
echo "$JLINK_CONFIG $command_file" # return the command to be executed

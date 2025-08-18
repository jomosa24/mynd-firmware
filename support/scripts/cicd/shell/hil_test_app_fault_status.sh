#!/bin/sh -ex

get_jlink_read_reg_cmd()
{
    jlink_read_reg_script="RReg IPSR\ng"
    jlink_read_reg_cmd=$("$SOURCE_DIR/support/scripts/cicd/shell/generate_jlink_script.sh" "$TEST_DIR" "$jlink_read_reg_script" "$JLINK_CONFIG" "fault-status")
    echo "$jlink_read_reg_cmd"
}

dut_power()
{
    state="$1"
    ftdi_device=$(dmesg | grep -i 'FTDI' | grep -oP 'ttyUSB[0-9]+' | tail -n1)

    if [ -n "$ftdi_device" ]; then
        stty -F "/dev/$ftdi_device" 115200
        printf "\r\np %s\r\n" "$state" > "/dev/$ftdi_device"
    else
        printf "FTDI device not found\n"
    fi
}

run_app_and_get_fault_status()
{
    app_run_time="$1"
    jlink_get_fault_status_cmd="$2"

    sleep "$app_run_time"

    ipsr_value=$($jlink_get_fault_status_cmd | grep "IPSR =" | awk '{print $NF}' | sed 's/0x//')
    echo "$ipsr_value"
}

check_fault_status()
{
    fault_status="$1"
    printf "IPSR Reg: 0x%s\n" "$fault_status"

    if [ -z "$fault_status" ]; then
        printf "\033[31mError: IPSR value not found or invalid.\033[0m\n"
        result=1
    elif [ "$fault_status" -gt 0 ]; then
        printf "\033[31mError: Fault detected.\033[0m\n"
        result=1
    else
        printf "Success: No fault detected.\n"
        result=0
    fi

    exit $result
}

### MAIN ###

SOURCE_DIR="$1"
TEST_DIR="$2"
JLINK_CONFIG="$3"

mkdir -p "$TEST_DIR"

jlink_cmd=$(get_jlink_read_reg_cmd)

dut_power "on"

fault_status=$(run_app_and_get_fault_status 10 "$jlink_cmd")

dut_power "off"

check_fault_status "$fault_status"

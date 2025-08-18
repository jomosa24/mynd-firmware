#!/bin/sh -e

check_if_devices_connected()
{
    if ! lsusb | grep -q J-Link; then
        printf "\033[31mJLink device not detected.\033[0m\n"
        exit 1
    fi

    if ! lsusb | grep -q Teufel; then
        printf "\033[31mTeufel device not detected.\033[0m\n"
        exit 1
    fi
}

prepare_tested_fw()
{
    fpath="$1"
    fsize="$2"
    dd if=/dev/urandom of="$fpath" bs="$fsize" count=1
}

prepare_test_data()
{
    random_bin_fn="$1"
    update_bin_fn="$2"

    prepare_tested_fw "$TEST_DIR/$random_bin_fn" "$TEST_BIN_SZ"

    "$SOURCE_DIR/support/scripts/cicd/shell/generate_update_bin.sh" \
      "$SOURCE_DIR" \
      "$PROJECT_ID" \
      "$TEST_DIR/$random_bin_fn" \
      "$update_bin_fn" \
      "$TEST_DIR"
}

check_if_disk_mounted()
{
    target_disk="$1"
    if mount | grep "$target_disk on $MOUNT_DIR" > /dev/null; then
        return 0
    else
        return 1
    fi
}

mount_and_perform_update()
{
    update_bin_fn="$1"

    mkdir -p "$MOUNT_DIR"
    target_disk="/dev/$(lsblk -f | grep TEUFEL | awk '{print $1}')"

    if ! check_if_disk_mounted "$target_disk"; then
        sudo mount -t vfat "$target_disk" "$MOUNT_DIR"
    fi

    cp -rf "$TEST_DIR/$update_bin_fn" "$MOUNT_DIR"
    sync
    if [ $? -ne 0 ]; then
        printf "\033[31mCopy %s failed.\033[0m\n" "$update_bin_fn"
        exit 1
    fi

    sleep 5
}

parse_jlink_output()
{
    file_path="$1"

    if [ ! -f "$file_path" ]; then
        printf "\033[31mFile '%s' does not exist.\033[0m\n" "$file_path"
        result=1
    else
        success_string="Verify successful."
        if grep -q "$success_string" "$file_path"; then
            printf "SUCCESS: Binary files match.\n"
            result=0
        else
            printf "\033[31mBinary files do not match.\033[0m\n"
            result=1
        fi
    fi

    rm -rf "$TEST_DIR"
    exit $result
}

verify_bootloader_update()
{
    verify_bin_path="$1"

    jlink_verify_script="h\nverifybin $verify_bin_path $MEM_ADDR_START\nr\nq"
    jlink_verify_cmd=$("$SOURCE_DIR/support/scripts/cicd/shell/generate_jlink_script.sh" \
                            "$TEST_DIR" \
                            "$jlink_verify_script" \
                            "$JLINK_CONFIG" \
                            "verify-target")

    $jlink_verify_cmd > "$TEST_DIR/verify.txt"
    cat "$TEST_DIR/verify.txt"
    parse_jlink_output "$TEST_DIR/verify.txt"
}

### MAIN ###

SOURCE_DIR="$1"
TEST_DIR="$2"
JLINK_CONFIG="$3"
MOUNT_DIR="$4"
TEST_BIN_SZ="$5"
PROJECT_ID="$6"
MEM_ADDR_START="$7"

check_if_devices_connected

mkdir -p "$TEST_DIR"
random_bin_fn="random.bin"
update_bin_fn="update.bin"

prepare_test_data "$TEST_DIR/$random_bin_fn" "$TEST_DIR/$update_bin_fn"
mount_and_perform_update "$TEST_DIR/$update_bin_fn"
verify_bootloader_update "$TEST_DIR/$random_bin_fn"

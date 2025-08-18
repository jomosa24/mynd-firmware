#!/bin/sh -e

execute_renode_simulation()
{
    renode_log_dir="$1"
    renode_log_file="$2"
    renode_timeout="$3"
    renode_script="$4"
    target_bin="$5"

    mkdir -p "$renode_log_dir"
    touch "/tmp/$renode_log_file"
    touch /tmp/renode_pid.txt

    # Background process to kill Renode after timeout and move log
    (
        sleep "$renode_timeout" &&
        kill "$(cat /tmp/renode_pid.txt)" &&
        cat "/tmp/$renode_log_file" &&
        mv "/tmp/$renode_log_file" "$renode_log_dir/"
    ) &

    # Run Renode simulation
    renode --pid-file /tmp/renode_pid.txt --disable-xwt "$renode_script" \
        -e "sysbus LoadELF @$target_bin; start" \
        > "/tmp/$renode_log_file" || true
}

### MAIN ###

for filepath in "${CI_PROJECT_DIR}/build/Projects/${TFL_PROJECT_NAME}"/*.elf; do
    if [ -e "$filepath" ]; then
        elf_binary_path="$filepath"
        execute_renode_simulation \
            "${CI_PROJECT_DIR}/build/Projects/$TFL_PROJECT_NAME" \
            "$TFL_RENODE_LOG" \
            "$TFL_RENODE_TIMEOUT" \
            "$CI_PROJECT_DIR/support/scripts/cicd/renode/stm32f072/stm32f072.resc" \
            "$elf_binary_path"
    else
        printf "\033[31mElf binary not found in %s directory! Testing skipped.\033[0m\n" "${CI_PROJECT_DIR}/build/Projects/$TFL_PROJECT_NAME"
        exit 1
    fi
done

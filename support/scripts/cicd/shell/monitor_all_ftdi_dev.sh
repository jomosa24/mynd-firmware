#!/bin/sh -ex

get_ftdi_ports()
{
    # Find FTDI USB Vendor and Product ID dynamically
    ftdi_ids=$(lsusb | grep "Future Technology Devices International" | awk '{print $6}')

    ports=""
    for ftdi_id in $ftdi_ids; do
        ftdi_vid=$(echo "$ftdi_id" | cut -d: -f1)
        ftdi_pid=$(echo "$ftdi_id" | cut -d: -f2)
        for dev in /dev/ttyUSB* /dev/ttyACM*; do
            [ -e "$dev" ] || continue
            if udevadm info -a -n "$dev" 2>/dev/null | grep -q "ATTRS{idVendor}==\"$ftdi_vid\"" && \
               udevadm info -a -n "$dev" 2>/dev/null | grep -q "ATTRS{idProduct}==\"$ftdi_pid\""; then
                ports="$ports $dev"
            fi
        done
    done

    echo "$ports"
}

### MAIN ###

BAUD_RATE="$1"
WORK_DIR="$2"

ftdi_ports=$(get_ftdi_ports)

if [ -z "$ftdi_ports" ]; then
    printf "\033[31mNo FTDI ports found.\033[0m\n"
    exit 1
fi

log_idx=1
for port in $ftdi_ports; do
    # Configure the port with stty (adjust settings as needed)
    stty -F "$port" "$BAUD_RATE" cs8 -cstopb -parenb
    # Read from the port and write to logfile in background
    cat "$port" > "$WORK_DIR/ftdi_log_${log_idx}.txt" &
    log_idx=$((log_idx + 1))
done
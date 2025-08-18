#!/bin/sh -ex

SOURCE_DIR="$1"
PROJECT_ID="$2"
SRC_FILE_PATH="$3"
UPDATE_FN="$4"
DEST_DIR="$5"

python3 "$SOURCE_DIR/support/scripts/prepare_update.py" \
    -p "$PROJECT_ID" --no-encryption \
    -k "$SOURCE_DIR/support/keys/teufel_dev_private.pem" \
    --mcu="$SRC_FILE_PATH" -o "$DEST_DIR/$UPDATE_FN"
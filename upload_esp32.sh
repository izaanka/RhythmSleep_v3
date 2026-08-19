#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_DIR="$DIR/esp32s3_rhythmSleep_arduinoide"
BUILD_DIR="$SKETCH_DIR/build/esp32.esp32.esp32s3"
ESPTOOL="/home/izaan/.arduino15/packages/esp32/tools/esptool_py/4.6/esptool.py"
PORT="${1:-/dev/ttyACM0}"

echo "======================================================="
echo " RhythmSleep ESP32-S3 Firmware Uploader"
echo "======================================================="
echo "Target Port: $PORT"
echo ""

if [ ! -f "$BUILD_DIR/esp32s3_rhythmSleep_arduinoide.ino.bin" ]; then
    echo "[BUILD] Compiling firmware..."
    ~/.local/bin/arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app,CDCOnBoot=cdc --export-binaries "$SKETCH_DIR"
fi

echo "[FLASH] Flashing binaries to $PORT..."
echo "NOTE: If connecting hangs, hold the BOOT button on the ESP32 and press the RST button."
echo ""

python3 "$ESPTOOL" \
    --chip esp32s3 \
    --port "$PORT" \
    --baud 921600 \
    --before default_reset \
    --after hard_reset \
    write_flash -z \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 4MB \
    0x0 "$BUILD_DIR/esp32s3_rhythmSleep_arduinoide.ino.bootloader.bin" \
    0x8000 "$BUILD_DIR/esp32s3_rhythmSleep_arduinoide.ino.partitions.bin" \
    0x10000 "$BUILD_DIR/esp32s3_rhythmSleep_arduinoide.ino.bin"

echo ""
echo "======================================================="
echo " Upload Complete! ESP32 is running new firmware."
echo "======================================================="

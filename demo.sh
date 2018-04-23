#!/bin/sh

USAGE_LINE="Usage: ./demo.sh <KEY_FILE> <POLLING(us)> <ENC_BLOCK(b)> <IP_ADDR> <PORT>"

if test $# != 5; then
    echo 'All 5 arguments are required!'
    echo "$USAGE_LINE"
    exit 1
fi

gst-launch-1.0 -q autovideosrc ! videoconvert  ! queue ! avenc_mjpeg ! fdsink \
| aes128 -k "$1" -p "$2" -f "$3" \
| nc "$4" "$5"


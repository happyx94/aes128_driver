#!/bin/sh

USAGE_LINE="Usage: ./receiver.sh <PORT> <KEY_FILE> <DEC_BLOCK(B)>"

if test $# != 3; then
    echo 'All 3 arguments are required!'
    echo "$USAGE_LINE"
    exit 1
fi

nc -l $1 | ./aes128 -dnr -k $2 -f $3 | gst-launch-1.0 fdsrc ! queue ! decodebin ! autovideosink


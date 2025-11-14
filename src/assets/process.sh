#!/bin/bash

# npm install terser -g

terser wsjpeg.js -o wsjpeg.min.js -m -c
terser mjpeg.js -o mjpeg.min.js -m -c

OUTPUT=include

[ -e "$OUTPUT" ] || mkdir $OUTPUT

tohex() {
    local path=$1
    if [ ! -f "$path" ]; then
        echo "file not found: $path"
        exit 1
    fi
    xxd -c 16 -i "$path" "$OUTPUT/$path.h"
} 

tohex favicon.ico

tohex index.html
tohex wsjpeg.min.js

tohex mjpeg.index.html
tohex mjpeg.min.js
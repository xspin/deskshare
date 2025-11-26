#!/bin/bash

# npm install terser -g
# npm install -g csso-cli

set -x

OUTPUT=output

rm -rvf include ${OUTPUT}

mkdir include
mkdir ${OUTPUT}

to_hex_header() {
    local path=$1
    if [ ! -f "$path" ]; then
        echo "file not found: $path"
        exit 1
    fi
    local fname=$(basename "$path")
    xxd -c 16 -i "$path" "include/$fname.h"
} 


to_hex_header favicon.ico
to_hex_header index.html

terser player.js -o ${OUTPUT}/player.js -m -c
to_hex_header ${OUTPUT}/player.js

csso player.css -o ${OUTPUT}/player.css
to_hex_header ${OUTPUT}/player.css

to_hex_header mjpeg.index.html

terser mjpeg.js -o ${OUTPUT}/mjpeg.js -m -c
to_hex_header ${OUTPUT}/mjpeg.js
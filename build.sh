#!/usr/bin/env bash

# export MallocStackLogging=0
# export MallocStackLoggingNoCompact=0

ulimit -c 2097152

ARGS=${@:1}

hasArg() {
    for arg in ${ARGS[@]}; do
        if [[ "$arg" == "$1" ]]; then
            return 0
        fi
    done
    return 1
}

hasArg clean
if [ $? -eq 0 ]; then
    echo "Clean ..."
    # cmake --build build --target clean
    rm -vrf build/*
    exit $? 
fi

BUILD_FLAGS=
TARGET=./build/bin/deskshare

hasArg nogui
if [ $? -eq 0 ]; then
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_GUI=OFF"
else
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_GUI=ON"
    TARGET=${TARGET}.app/Contents/MacOS/deskshare
fi

hasArg debug
if [ $? -eq 0 ]; then
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_DEBUG=ON"
else
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_DEBUG=OFF"
fi

hasArg asan
if [ $? -eq 0 ]; then
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_ASAN=ON"
else
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_ASAN=OFF"
fi

hasArg test
if [ $? -eq 0 ]; then
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_TEST=ON"
else
    BUILD_FLAGS="${BUILD_FLAGS} -DENABLE_TEST=OFF"
fi

echo "Build flags: ${BUILD_FLAGS}"

cmake . -B build ${BUILD_FLAGS} \
&& cmake --build build
if [ $? -ne 0 ]; then
    echo "\033[31m Build Failed! \033[0m"
    exit 1
fi

hasArg test
if [ $? -eq 0 ]; then
    ctest --test-dir build/test -V
fi

hasArg lldb
if [ $? -eq 0 ]; then
    lldb --batch -o "run" -o "thread backtrace all" --file $TARGET
    exit $?
fi

hasArg run
if [ $? -eq 0 ]; then
    echo "\nRun $TARGET\n"
    exec ${TARGET}
    exit $?
fi

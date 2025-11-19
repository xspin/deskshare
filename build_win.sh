#!/usr/bin/env bash

#-DCMAKE_C_COMPILER_FORCED=ON -DCMAKE_CXX_COMPILER_FORCED=ON 

md5sum `which mingw32-make.exe`
mingw32-make.exe --version
if [ $? -ne 0 ]; then
    echo "\033[31m mingw32-make.exe execute failed! \033[0m"
    exit 1
fi

echo

cmake -G "MinGW Makefiles" -B build \
&& cmake --build build

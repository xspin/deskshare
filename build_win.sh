#!/usr/bin/env bash

#-DCMAKE_C_COMPILER_FORCED=ON -DCMAKE_CXX_COMPILER_FORCED=ON 

cmake -G "MinGW Makefiles" -B build \
&& cmake --build build

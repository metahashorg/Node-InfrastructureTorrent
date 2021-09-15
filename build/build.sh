#!/bin/bash

cmake -DCMAKE_BUILD_TYPE=Release ..
#cmake -DCMAKE_BUILD_TYPE=Debug ..

make --jobs=`nproc`
#make VERBOSE=1

#!/bin/sh

mkdir build
cd build
cmake -DMEM_TEST=ON ../
cd ..

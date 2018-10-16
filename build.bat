@echo off

call cmake.bat

cd build
cmake --build . --config Release
cd ..

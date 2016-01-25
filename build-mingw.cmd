@echo off
rem Build Picol on Windows with MinGW.
set PATH=%PATH%:C:\mingw\bin
make
cd examples
make
cd ..

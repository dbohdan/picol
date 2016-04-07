@echo off
rem Build Picol on Windows with MinGW.
set PATH=%PATH%;C:\mingw\bin
make
make examples
pause

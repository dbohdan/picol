@echo off
rem Build Picol on Windows with MinGW.
set PATH=C:\MinGW\bin;C:\MinGW\msys\1.0\bin
set CC=gcc
make
make examples
pause

@echo off

rem Process the command line arguments.
set flag_batch=0
for %%a in (%*) do (
    if "%%a"=="/batch" set flag_batch=1
)

rem Build Picol with MinGW.
echo on
set PATH=C:\MinGW\bin;C:\MinGW\msys\1.0\bin
set CC=gcc
make picolsh
make examples
@if "%flag_batch%"=="0" pause

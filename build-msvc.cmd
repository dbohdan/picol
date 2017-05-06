@echo off
rem This build file should work with Visual Studio versions 12.0
rem (Visual Studio 2013) and 14.0 (Visual Studio 2015) when they are installed
rem in their default directory on 64-bit Windows. It does not work with
rem version 15.0 (Visual Studio 2017). Specify which version of VS to use
rem ("14.0" is the default) with the command line option "/version:".

setlocal ENABLEDELAYEDEXPANSION

rem Process the command line arguments.
set flag_batch=0
set msvs_version=14.0
for %%a in (%*) do (
    set flag=%%a
    if "!flag!"=="/batch" set flag_batch=1
    if "!flag:~0,9!"=="/version:" set msvs_version=!flag:~9!
)

rem Build Picol with MSVC.
echo on
call "C:\Program Files (x86)\Microsoft Visual Studio %msvs_version%\VC\vcvarsall.bat"
echo on
nmake /F Makefile.nmake
@if "%flag_batch%"=="0" pause

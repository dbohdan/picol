@echo off
rem This build file is for Visual Studio version 15.0 (2017).

setlocal ENABLEDELAYEDEXPANSION

rem Process the command line arguments.
set flag_batch=0
set msvs_edition=Community
for %%a in (%*) do (
    set flag=%%a
    if "!flag!"=="/batch" set flag_batch=1
    if "!flag:~0,9!"=="/edition:" set msvs_edition=!flag:~9!
)

rem Build Picol with MSVC.
echo on
call "c:\Program Files (x86)\Microsoft Visual Studio\2017\%msvs_edition%\VC\Auxiliary\Build\vcvarsall.bat" x86
echo on
nmake /F Makefile.nmake
@if "%flag_batch%"=="0" pause

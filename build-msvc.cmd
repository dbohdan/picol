@echo off

rem Process the command line arguments.
set flag_batch=0
for %%a in (%*) do (
    if "%%a"=="/batch" set flag_batch=1
)

rem Build Picol with MSVC.
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
echo on
nmake /F Makefile.nmake
if "%flag_batch%"=="0" pause

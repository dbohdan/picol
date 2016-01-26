@echo off
rem Build Picol on Windows with MSVC.
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
echo on
nmake /F Makefile.nmake
pause

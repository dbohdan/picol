set path=%path%;c:\tcl86\bin
call clean.cmd
cmd /c build-mingw.cmd /batch
picol.exe test.pcl
cmd /c build-msvc-2017.cmd /batch
picol.exe test.pcl
pause

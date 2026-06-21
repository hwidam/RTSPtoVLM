@echo off
echo Cleaning build artifacts...

rem -- Intermediate / build output files ---------------------------------------
del /s /q *.pdb
del /s /q *.sdf
del /s /q *.obj
del /s /q *.cod
del /s /q *.ilk
del /s /q *.ncb
del /s /q *.pch
del /s /q *.ipch
del /s /q *.idb
del /s /q *.dep
del /s /q *.sbr
del /s /q *.opt
del /s /q *.plg
del /s /q *.map
del /s /q *.exp
del /s /q *.recipe
del /s /q *.res
del /s /q *.iobj
del /s /q *.ipdb

rem -- Build tracking / log files ----------------------------------------------
del /s /q *.log
del /s /q *.tlog
del /s /q *.lastbuildstate
del /s /q *.FileListAbsolute.txt
del /s /q *.cachefile
del /s /q *.unsuccessfulbuild

rem -- IntelliSense / browse database ------------------------------------------
del /s /q *.db
del /s /q *.opendb
del /s /q *.id0
del /s /q *.id1
del /s /q *.nam
del /s /q *.til
del /s /q *.tih

rem -- VS solution / workspace local settings ----------------------------------
rem .suo is a hidden file - needs /a:h flag
del /s /q /a:h *.suo

rem -- Resource editor binary --------------------------------------------------
del /s /q *.aps

rem -- Misc --------------------------------------------------------------------
del /s /q *.bak
del /s /q *._xe
del /s /q *.mine
del /s /q *.dmp
del /s /q Thumbs.db

rem -- Directories (entire trees) ----------------------------------------------
rem VS local cache - IntelliSense, browse DB, launch settings
if exist .vs            rd /s /q .vs

rem Build output dirs - searched recursively so per-project folders (e.g. VA\x64\) are caught too
for /d /r . %%d in (out x64 x86 Debug Release) do (
    if exist "%%d" rd /s /q "%%d"
)

rem .tlog directories left behind inside project build folders (e.g. VA\x64\Debug\VA.tlog\)
for /d /r . %%d in (*.tlog) do (
    if exist "%%d" rd /s /q "%%d"
)

echo Done.
pause

@echo off

:: Check if the args are there
if "%~1"=="" goto :usage
if "%~2"=="" goto :usage

:: Command on the database
isam_db.exe -f %1 -k %2

exit /b
:usage
	echo Usage isam_db -f[file_name] -k[record key]
	exit /b

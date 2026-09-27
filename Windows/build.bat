@echo off
setlocal enabledelayedexpansion

:: Set the compiler and the final output executable name
set CC=gcc
set OUT=isam_db.exe
set dir=..\src\
set obj=..\obj\
:: List of all source files
set SOURCES=build.c journal.c common.c crud.c date.c debug.c endian.c file.c globals.c hash_tbl.c helper.c input.c  key.c lock.c main.c parse.c record.c sort.c string_utilities.c str_op.c

echo Starting compilation...

:: Compile each source file into an object file
for %%f in (%SOURCES%) do (
    echo Compiling %dir%%%f...
    gcc -g3 -c %dir%%%f -o %obj%%%~nf.o -I..\include
    
    :: Stop if any compilation fails
    if !ERRORLEVEL! neq 0 (
        echo.
        echo Build failed during compilation of %%f.
        goto :error
    )
)
echo.
echo Linking object files into %OUT%...
%CC% -g3 %obj%*.o -o %OUT%

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed during linking.
    goto :error
)

:: Make isam_db.exe accessable from everywhere
copy %OUT% C:\Users\loren\bin\

:: build crud library
echo.
echo Creating crud Library.
move ..\obj\main.o .
move ..\src\main.c .
gcc -g3 -shared -o libcrud.dll ..\obj\*.o

if %ERRORLEVEL% neq 0 (
    echo.
    echo Cannot create crud library.
    goto :error
)

move main.o ..\obj\
echo .
echo Creating crud object for development
mkdir obj_static
cd obj_static

gcc -c ..\..\src\*.c  -I..\..\include

if %ERRORLEVEL% neq 0 (
    echo.
    echo Cannot create database static library.
	move main.c ..\..\src\
    goto :error
)


:: Collect all .o files into a single string variable
set "OBJ_FILES="
for %%f in (*.o) do (
    set "OBJ_FILES=!OBJ_FILES! %%f"
)

:: Create the static library using the collected files
echo Creating static library libdb.a...
ar rcs libdb.a %OBJ_FILES%

move libdb.a ..\
cd ..\
del obj_static\*.*
move main.c ..\src\

:: Make libcrud.dll accessable from everywhere
copy libcrud.dll  C:\Users\loren\bin\

:: build the test suite 
echo.
echo Build tests...
cd ..\test\
gcc -g3 -o obj\test.o -c src\test.c -Iinclude -I../include 

if %ERRORLEVEL% neq 0 (
    echo.
    echo Cannot create test_suite.
    goto :error
)
gcc -g3 -o obj\main.o -c src\main.c -Iinclude -I../include 

if %ERRORLEVEL% neq 0 (
    echo.
    echo Cannot create test_suite.
    goto :error
)
gcc -g3 -o test_suite obj\*.o -L../ -lcrud


if %ERRORLEVEL% neq 0 (
    echo.
    echo Cannot create test_suite.
    goto :error
)
test_suite

echo.
echo Build successful: %OUT%
goto :EOF

:error
exit /b 1

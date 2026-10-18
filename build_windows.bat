@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   Campus Lost ^& Found System - Windows Build Script
echo =======================================================
echo.

where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GCC compiler was not found in your system PATH.
    echo.
    echo Please install MinGW-w64 ^(MSYS2 recommended^):
    echo   1. Install MSYS2 from https://www.msys2.org/
    echo   2. Run MSYS2 UCRT64 terminal and execute:
    echo        pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-make
    echo   3. Add "C:\msys64\ucrt64\bin" to your Windows PATH.
    echo.
    echo Or use portable w64devkit from:
    echo   https://github.com/skeeto/w64devkit/releases
    echo.
    pause
    exit /b 1
)

echo [1/3] Compiling lostfound_server.exe ...
gcc -Wall -Wextra -O2 -Iinclude -pthread src\main.c src\db.c src\matching.c src\validation.c src\handlers.c src\http.c src\cJSON.c -lsqlite3 -pthread -lws2_32 -lm -o lostfound_server.exe

if %ERRORLEVEL% neq 0 (
    echo.
    echo [BUILD ERROR] Failed to compile lostfound_server.exe.
    echo Ensure SQLite3 development headers and libraries are installed.
    echo For MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-sqlite3
    pause
    exit /b 1
)
echo [OK] lostfound_server.exe compiled successfully.

echo.
echo [2/3] Compiling test_matching.exe ...
gcc -Wall -Wextra -O2 -Iinclude -pthread tests\test_matching.c src\matching.c src\validation.c -pthread -lm -o test_matching.exe

if %ERRORLEVEL% neq 0 (
    echo.
    echo [BUILD ERROR] Failed to compile test_matching.exe.
    pause
    exit /b 1
)
echo [OK] test_matching.exe compiled successfully.

echo.
echo [3/3] Running unit tests to verify build ...
test_matching.exe
if %ERRORLEVEL% neq 0 (
    echo.
    echo [TEST ERROR] Unit tests failed!
    pause
    exit /b 1
)

echo.
echo =======================================================
echo   BUILD SUCCEEDED! All binaries and tests verified.
echo =======================================================
echo.
echo To run the application:
echo   - Double-click 'run_windows.bat'
echo   - OR run: lostfound_server.exe -p 8080 --seed
echo.
pause

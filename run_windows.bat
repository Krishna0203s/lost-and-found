@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   Campus Lost ^& Found System - Windows Native Launcher
echo =======================================================
echo.

set PORT=8080
if not "%~1"=="" set PORT=%~1

:: Check if server binary exists, build if missing
if not exist "lostfound_server.exe" (
    echo [INFO] 'lostfound_server.exe' not found. Compiling now...
    where gcc >nul 2>nul
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] GCC compiler was not found in your Windows PATH.
        echo.
        echo To run this natively on Windows, install MinGW-w64 / MSYS2:
        echo   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-make
        echo.
        echo Or run via WSL ^(Windows Subsystem for Linux^):
        echo   wsl
        echo   make run
        echo.
        pause
        exit /b 1
    )
    
    echo Compiling lostfound_server.exe...
    gcc -Wall -Wextra -O2 -Iinclude -pthread src\main.c src\db.c src\matching.c src\validation.c src\handlers.c src\http.c src\cJSON.c -lsqlite3 -pthread -lws2_32 -lm -o lostfound_server.exe
    if !ERRORLEVEL! neq 0 (
        echo.
        echo [ERROR] Compilation failed.
        echo Ensure 'sqlite3' headers and library are installed in your GCC environment.
        pause
        exit /b 1
    )
    echo [OK] Compilation complete.
    echo.
)

:: Seed database if it does not exist yet
set SEED_ARG=
if not exist "lostfound.db" (
    echo [INFO] First run detected: Seeding database with initial campus records...
    set SEED_ARG=--seed
)

echo Starting Campus Lost ^& Found server on port %PORT%...
echo Web Application URL: http://localhost:%PORT%/
echo.
echo Opening browser in 2 seconds...
start "" cmd /c "timeout /t 2 /nobreak >nul & start http://localhost:%PORT%/"

echo =======================================================
echo Server is running. Press Ctrl+C in this console to stop.
echo =======================================================
lostfound_server.exe -p %PORT% %SEED_ARG%

pause

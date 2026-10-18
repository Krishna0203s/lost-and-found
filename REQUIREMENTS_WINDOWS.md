# Windows Requirements — Campus Lost & Found System

This document outlines the system, compiler, library, and runtime requirements needed to build and run the **Campus Lost & Found System** natively on Microsoft Windows.

---

## 1. System Requirements

| Specification | Minimum | Recommended |
|---|---|---|
| **Operating System** | Windows 10 (64-bit, Version 1809+) | Windows 11 (64-bit) |
| **Architecture** | x86_64 (AMD64) | x86_64 (AMD64) |
| **RAM** | 64 MB | 256 MB+ |
| **Disk Space** | 25 MB free space | 100 MB free space |
| **Network** | 1 open TCP port (default `8080`) | Localhost loopback interface |
| **Permissions** | Standard User (No Admin rights required) | Standard User |

---

## 2. Toolchain & Compiler Requirements

The application is written in standard **C99** and compiles to a native Windows PE executable (`lostfound_server.exe`).

### Required Toolchain Components:
1. **C Compiler (C99 compliant)**:
   - **MinGW-w64 GCC** (GCC 9.x, 10.x, 11.x, 12.x, 13.x, or 14.x)
   - *Alternative*: Clang / LLVM for Windows with MinGW runtime
2. **SQLite3 Development Package**:
   - Header: `sqlite3.h` (version 3.25.0 or later recommended)
   - Library: `libsqlite3.a` (static) or `sqlite3.dll` (dynamic)
3. **Threading**:
   - `winpthreads` (`pthread.h` and `libpthread.a` / `libwinpthread-1.dll`), bundled with all MinGW-w64 distributions
4. **Networking**:
   - `ws2_32` (Windows Sockets 2 — built into Windows and standard MinGW-w64 library path)
5. **Math Library**:
   - standard C math library (`-lm`)

---

## 3. Recommended Toolchain Options for Windows

### Option A: MSYS2 UCRT64 (Recommended)
MSYS2 provides modern MinGW-w64 toolchains using the Microsoft Universal C Runtime (UCRT).

**Package Requirements**:
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
                   mingw-w64-ucrt-x86_64-sqlite3 \
                   mingw-w64-ucrt-x86_64-make
```

### Option B: w64devkit (Portable / Zero Installation)
[w64devkit](https://github.com/skeeto/w64devkit/releases) is an all-in-one portable C/C++ development environment for x64 Windows that unzips to a single folder and requires no installer or administrator privileges.

### Option C: WSL2 (Windows Subsystem for Linux)
If you prefer running in a Linux environment inside Windows:
- OS: Ubuntu 22.04 LTS / 24.04 LTS on WSL2
- Packages: `sudo apt update && sudo apt install -y build-essential libsqlite3-dev`

---

## 4. Port & Firewall Requirements

- **Port**: Default is `8080`. Can be customized via the `-p <port>` flag (e.g. `-p 8085` or `-p 3000`).
- **Windows Defender Firewall**:
  - Listening on `0.0.0.0` or `127.0.0.1` may trigger a standard Windows Defender prompt on the first run asking for network access permission.
  - Allowing access for **Private Networks** or accessing via `localhost` works out of the box without special network configuration.

---

## 5. Web Browser Compatibility

The frontend requires an evergreen browser supporting ES6, CSS Grid, and Fetch API:
- Google Chrome (Version 80+)
- Microsoft Edge (Version 80+)
- Mozilla Firefox (Version 78+)
- Brave / Opera / Vivaldi

---

## 6. Optional: Python E2E Testing Suite Requirements

To execute the automated end-to-end browser and WCAG accessibility tests:
- **Python**: Version 3.8, 3.9, 3.10, 3.11, or 3.12 (64-bit)
- **Packages**:
  - `playwright>=1.40.0`
  - `pytest>=7.4.0`
  - `requests>=2.31.0`
- **Installation**:
  ```cmd
  pip install -r requirements.txt
  playwright install chromium
  ```

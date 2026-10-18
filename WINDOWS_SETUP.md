# How to Run on Windows — Campus Lost & Found System

A step-by-step guide to building, running, and testing the **Campus Lost & Found System** natively on Windows 10 and Windows 11.

---

## ⚡ Quick Start (If GCC and SQLite3 are already installed)

1. Double-click **`build_windows.bat`** to compile the server and run unit tests.
2. Double-click **`run_windows.bat`** to start the server.
3. Your default browser will automatically open to [http://localhost:8080/](http://localhost:8080/).

---

## 🛠️ Step-by-Step Setup from Scratch

Choose whichever setup method you prefer:

### Method 1: MSYS2 UCRT64 (Recommended Native Method)

This is the standard and cleanest way to get a modern C compiler with SQLite on Windows.

1. **Download & Install MSYS2**:
   - Download the installer from [https://www.msys2.org/](https://www.msys2.org/).
   - Follow the default installation wizard (installs to `C:\msys64`).

2. **Install Compiler & SQLite Packages**:
   - Open the **MSYS2 UCRT64** terminal from the Start Menu.
   - Run the following command:
     ```bash
     pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-make
     ```

3. **Add MinGW to your Windows PATH**:
   - Press `Win + R`, type `sysdm.cpl`, and press Enter.
   - Go to the **Advanced** tab -> **Environment Variables**.
   - Under **System variables** (or **User variables**), select `Path` and click **Edit**.
   - Click **New** and add:
     ```text
     C:\msys64\ucrt64\bin
     ```
   - Click **OK** to save.
   - Open a new Command Prompt or PowerShell and verify:
     ```cmd
     gcc --version
     ```

---

### Method 2: Portable w64devkit (Zero Installation Required)

If you cannot install software or prefer a standalone folder:

1. Download the latest `w64devkit-x.x.x.zip` from [GitHub Releases](https://github.com/skeeto/w64devkit/releases).
2. Extract the ZIP to a folder (e.g. `C:\w64devkit`).
3. Add `C:\w64devkit\bin` to your Windows PATH, or launch `w64devkit.exe` to open a shell with GCC and Make pre-configured.

---

### Method 3: WSL (Windows Subsystem for Linux)

If you already use WSL:

1. Open PowerShell and launch your Linux distribution:
   ```cmd
   wsl
   ```
2. Navigate to your project folder:
   ```bash
   cd /mnt/c/path/to/college_lost_found_project
   ```
3. Install build tools and SQLite:
   ```bash
   sudo apt update && sudo apt install -y build-essential libsqlite3-dev
   ```
4. Build and run:
   ```bash
   make run
   ```
5. Open your Windows browser to `http://localhost:8080/`.

---

## 🔨 Building the Project on Windows

Open Command Prompt (`cmd.exe`) or PowerShell in the project directory:

### Option A: Using the Build Script
```cmd
build_windows.bat
```
*This compiles `lostfound_server.exe` and `test_matching.exe`, and verifies the build by running the unit tests.*

### Option B: Using Make
```cmd
mingw32-make
```
*Or `make` if inside an MSYS2 shell.*

### Option C: Direct GCC Compilation Command
```cmd
gcc -Wall -Wextra -O2 -Iinclude -pthread src\main.c src\db.c src\matching.c src\validation.c src\handlers.c src\http.c src\cJSON.c -lsqlite3 -pthread -lws2_32 -lm -o lostfound_server.exe
```

---

## 🚀 Running the Server

### 1. Simple 1-Click Launch
Double-click `run_windows.bat`, or in terminal:
```cmd
run_windows.bat
```
*This script will compile the project if needed, seed the database with initial sample records on first launch, open your browser, and start the server on port 8080.*

### 2. Manual Command-Line Launch
```cmd
lostfound_server.exe -p 8080 --seed
```

### Command-Line Arguments:
| Option | Description | Example |
|---|---|---|
| `-p <port>` | Server TCP port (default: `8080`) | `lostfound_server.exe -p 8085` |
| `-d <db_path>` | SQLite database file path (default: `lostfound.db`) | `lostfound_server.exe -d mycampus.db` |
| `-w <web_root>`| Web root assets directory (default: `web`) | `lostfound_server.exe -w web` |
| `--seed` | Populate database with realistic campus records | `lostfound_server.exe --seed` |
| `-h`, `--help` | Display command usage and options | `lostfound_server.exe -h` |

---

## 🌐 Accessing the Web Application

Once running, access the application in any web browser:
- **Main Portal**: [http://localhost:8080/](http://localhost:8080/)
- **Live Statistics API**: [http://localhost:8080/api/statistics](http://localhost:8080/api/statistics)
- **Items API**: [http://localhost:8080/api/items](http://localhost:8080/api/items)
- **Matches API**: [http://localhost:8080/api/matches](http://localhost:8080/api/matches)

To stop the server, press `Ctrl + C` in the console window. The server will gracefully flush SQLite transactions, close all sockets, and exit cleanly.

---

## 🧪 Running Tests on Windows

### 1. C Matching Engine Unit Tests
```cmd
test_matching.exe
```
or
```cmd
mingw32-make test
```
*Output validates scoring weights, multi-attribute similarity algorithms, item validation, and status transitions.*

### 2. Playwright End-to-End Tests (Optional)
If you have Python installed:
```cmd
pip install -r requirements.txt
playwright install chromium
```
Run tests against your running server:
```cmd
pytest tests/
```

---

## ❓ Troubleshooting & FAQs

### 1. `'gcc' is not recognized as an internal or external command`
- **Cause**: MinGW `bin` directory is not in your Windows `PATH` environment variable.
- **Solution**: Add `C:\msys64\ucrt64\bin` (or your MinGW installation directory) to your user or system PATH, then restart your Command Prompt or terminal.

### 2. `fatal error: sqlite3.h: No such file or directory` or `-lsqlite3 cannot be found`
- **Cause**: SQLite3 development package is not installed in your MinGW environment.
- **Solution**: In your MSYS2 UCRT64 terminal, run:
  ```bash
  pacman -S mingw-w64-ucrt-x86_64-sqlite3
  ```

### 3. `Address already in use` or Port Conflict
- **Cause**: Another application (or a previous server instance) is using port 8080.
- **Solution**:
  - Run the server on a different port:
    ```cmd
    lostfound_server.exe -p 8085
    ```
  - Or find and terminate the occupying process:
    ```cmd
    netstat -ano | findstr 8080
    taskkill /PID <PID_NUMBER> /F
    ```

### 4. Windows Defender / Firewall Prompt
- **Cause**: Windows detects a newly compiled executable binding to a network port.
- **Solution**: Click **"Allow access"** for Private networks. The server listens on `0.0.0.0` or localhost loopback for standard browser interaction.

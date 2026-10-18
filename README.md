# Campus Lost & Found System

A high-performance, lightweight web application built in native C99 with SQLite3 and a vanilla HTML5/CSS3/JavaScript frontend, designed for college campuses to report, match, track, and resolve lost and found items.

---

## 🌟 Key Features

- **Blazing Fast Native C Engine**: Multi-threaded HTTP 1.1 server with sub-millisecond response times and low memory footprint (< 10 MB RAM).
- **Intelligent Multi-Attribute Matching**: Weighted matching algorithm scoring categories (40%), location proximity (30%), date closeness (10%), and text token similarity (20%).
- **Full Life-Cycle Management**: Lost / Found reporting, claimed status workflow, resolved verification, and administrative item deletion.
- **SQLite3 Database**: ACID compliant, persistent storage with thread-safe mutex-protected query operations.
- **Modern Responsive Frontend**: Semantic HTML5, accessible WCAG 2.1 AA compliant, responsive CSS Grid / Flexbox layout, and live statistics dashboard.
- **Cross-Platform**: Compiles and runs out-of-the-box on both Linux and native Windows (MinGW-w64 / MSYS2 / WSL).

---

## 🚀 Quick Start on Windows

See [WINDOWS_SETUP.md](file:///run/media/krishna/AA103F86103F5891/Projects/Lost%20&%20found/college_lost_found_project/WINDOWS_SETUP.md) for full instructions and [REQUIREMENTS_WINDOWS.md](file:///run/media/krishna/AA103F86103F5891/Projects/Lost%20&%20found/college_lost_found_project/REQUIREMENTS_WINDOWS.md) for system requirements.

### 1-Click Launch
1. Run **`build_windows.bat`** to build the server and run unit tests.
2. Run **`run_windows.bat`** to launch the server and open [http://localhost:8080/](http://localhost:8080/).

---

## 🐧 Quick Start on Linux / macOS / WSL

### Prerequisites
- GCC / Clang
- SQLite3 development library (`libsqlite3-dev` on Debian/Ubuntu or `sqlite-devel` on Fedora/RHEL)
- GNU Make

### Build & Run
```bash
# Build the application
make

# Run matching engine unit tests
make test

# Start the server on port 8080
make run
```

Then visit [http://localhost:8080/](http://localhost:8080/) in your browser.

---

## 📁 Project Structure

```text
college_lost_found_project/
├── build_windows.bat          # Windows native build & verification script
├── run_windows.bat            # Windows 1-click launcher (auto-build & browser open)
├── requirements.txt           # Requirements spec & optional Python E2E dependencies
├── REQUIREMENTS_WINDOWS.md    # Detailed Windows hardware, OS & toolchain specs
├── WINDOWS_SETUP.md           # Step-by-step Windows setup and running guide
├── report_and_presentation/   # 10-12 Page Technical Report, 10-Slide PPTX & Screenshots
│   ├── report.md              # Detailed structural architecture & C mechanics report
│   ├── presentation.pptx      # Editable 10-slide PowerPoint presentation
│   ├── presentation.md        # Markdown companion slide deck
│   ├── generate_presentation.py # Script to rebuild PowerPoint presentation
│   └── screenshots/           # Live application screenshots (telemetry, matcher, etc.)
├── Makefile                   # Cross-platform GNU Makefile (Linux & Windows)
├── include/                   # C header files
│   ├── cJSON.h                # Embedded JSON parser
│   ├── db.h                   # SQLite database interface & schema
│   ├── handlers.h             # REST API endpoint handlers
│   ├── http.h                 # Multi-threaded HTTP server
│   ├── matching.h             # Weighted similarity engine
│   ├── models.h               # Data structures
│   └── validation.h           # Input sanitation & validation
├── src/                       # C source implementations
│   ├── cJSON.c
│   ├── db.c
│   ├── handlers.c
│   ├── http.c
│   ├── main.c
│   ├── matching.c
│   └── validation.c
├── tests/                     # Unit tests
│   └── test_matching.c        # Matching engine & validation test suite
└── web/                       # Frontend web assets
    ├── index.html             # Single-page web application
    ├── css/style.css          # Responsive design & CSS custom properties
    └── js/app.js              # Client-side UI & REST API controller
```

---

## ⚙️ Command-Line Arguments

```text
Usage: lostfound_server [options]
Options:
  -p <port>      Server port (default: 8080)
  -d <db_path>   Database file path (default: lostfound.db)
  -w <web_root>  Web root directory (default: web)
  --seed         Populate database with sample campus records
  -h, --help     Show this help message
```

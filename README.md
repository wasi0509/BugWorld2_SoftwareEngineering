# Bug World Simulator — Client Frontend

**Course:** Software Engineering Project — Constructor University, Spring 2026  
**Team:** Giorgi Pataridze & Wai Huen Sin
**Date:** 1 April 2026 to 13 April 2026
---

## Overview

Bug World is a dual-player simulation where two bug cohorts (Red and Black) compete in a
hexagonal grid world. Each cohort is controlled by a binary bug program and the simulation
advances in ticks. Bugs collect food, carry it back to their nest, and compete for resources.

This repository implements the **visual frontend client** for the Bug World simulator (`sim`).
The client connects to `sim` over named pipes, sends step commands, reads the world state each
frame, and renders it visually. Two frontends are provided:

- **Terminal client** (`client`) — renders the grid using ANSI color codes directly in the
  terminal. Useful for quick testing and remote SSH sessions without an X server.
- **Qt GUI client** (`bugworld`) — renders the grid in a proper graphical window with colored
  cells, real-time stats, and an interactive trace length control. Requires an X server.

Both clients support **bug trace visualization**: each bug's N previous positions are drawn
with a fading color trail, where N is configurable at runtime.

---

## Project Structure

```
project-08/
├── client.cpp          # Terminal client — ANSI rendering, pipe IPC, input thread
├── parser.h/.cpp       # Free function parseResponse() — parses raw simulator output
│                       # into a WorldState struct. Qt-free so it can be unit tested.
├── simulator.h/.cpp    # SimulatorWorker class — forks sim, manages pipes, emits frames
│                       # to the Qt UI via signals/slots on a background QThread
├── mainwindow.h/.cpp   # Qt main window — lays out the UI, connects signals to slots,
│                       # displays cycle count and live stats
├── gridwidget.h/.cpp   # Custom QWidget — paints the hex grid, bugs, food, and traces
│                       # using QPainter with per-cell color and alpha fading
├── main.cpp            # Qt application entry point — parses args, launches MainWindow
├── test_client.cpp     # Unit test binary — 19 tests covering core logic functions
├── bugworld.pro        # Qt project file for qmake
├── Makefile            # Builds terminal client, Qt app, and test binary
├── bin/
│   ├── sim             # Simulator binary (provided, not authored by this team)
│   └── asm             # Assembler binary — converts .bug source to binary format
├── Worlds/             # Sample world map files (.world)
└── Bug/                # Sample bug program files (.bug)
```

---

## Dependencies

| Dependency     | Version | Purpose                              |
|----------------|---------|--------------------------------------|
| g++            | 13.3.0+ | C++17 compiler                       |
| Qt6            | 6.4.2   | GUI framework for graphical client   |
| qt6-base-dev   | 6.4.2   | Qt6 development headers              |
| pthreads       | —       | Threading for terminal client        |

On the teaching VM all dependencies are already installed.

---

## Building

### Build the terminal client

```bash
make
```

Produces `./client`.

### Build the Qt GUI client

```bash
make qt
```

Produces `./bugworld`. This runs `qmake` and compiles all Qt source files into `build_qt/`.

### Build and run unit tests

```bash
make test
```

Compiles `test_client` and runs all 19 tests immediately.

### Clean all build artifacts

```bash
make clean
```

---

## Running

### Terminal client

```bash
./client <world> <bug1> <bug2> [ticks_per_frame] [fps]
```

**Example:**

```bash
./client "Worlds/cross.world" "Bug/beetle.bug" "Bug/beetle.bug"
./client "Worlds/cross.world" "Bug/beetle.bug" "Bug/beetle.bug" 20 5
```

| Argument        | Required | Default | Description                              |
|-----------------|----------|---------|------------------------------------------|
| world           | Yes      | —       | Path to a `.world` file                  |
| bug1            | Yes      | —       | Path to Red team `.bug` file             |
| bug2            | Yes      | —       | Path to Black team `.bug` file           |
| ticks_per_frame | No       | 50      | Simulation ticks advanced per frame      |
| fps             | No       | 10      | Display frames per second                |

**Controls:**
- Type a number and press Enter to change the trace length N
- Type `q` and press Enter to quit
- Press `Ctrl+C` to quit

### Qt GUI client

Requires an X server running on your local machine:

- **macOS:** Install [XQuartz](https://www.xquartz.org/) and restart your Mac. Connect with `ssh -Y`.
- **Windows:** Install [VcXsrv](https://sourceforge.net/projects/vcxsrv/) with "Disable access control" checked. In PowerShell set `$env:DISPLAY = "127.0.0.1:0.0"` then connect with `ssh -Y`.
- **Linux:** Works natively, connect with `ssh -Y`.

```bash
ssh -Y your_username@teaching.peter-baumann.org
./bugworld <world> <bug1> <bug2> [ticks_per_frame] [fps]
```

**Example:**

```bash
./bugworld "Worlds/cross.world" "Bug/beetle.bug" "Bug/beetle.bug"
```

**Controls:**
- Adjust the **Trace length (N)** spinbox in the bottom-right corner to change how many previous positions are shown per bug
- Close the window to quit

---

## Architecture

### Communication Protocol

The client forks `sim` as a child process and communicates via two named pipes created in a
temporary directory (`/tmp/bugworld_qt_<pid>/`):

```
Client                         Simulator (sim)
  |                                 |
  |-- STEP N\n ------------------> |
  |<-- CYCLE x                     |
  |<-- MAP rows cols               |
  |<-- ROW ...                     |
  |<-- STATS r_alive b_alive ...   |
  |<-- END                         |
  |                                 |
  |-- QUIT\n ------------------>   |
```

The command pipe is opened with `O_NONBLOCK` and a retry loop (up to 200 retries over 2
seconds) to avoid hanging if `sim` is slow to start. If `sim` exits immediately after being
forked, this is detected early via `waitpid` with `WNOHANG` and a clear error is reported.

### Qt Threading Model

The Qt app uses a `QThread` / worker pattern to keep the UI responsive:

```
Main Thread (Qt event loop)           Background QThread
        |                                     |
  MainWindow                          SimulatorWorker::run()
  GridWidget::paintEvent()   <-----   emit frameReady(WorldState)
  MainWindow::onFrameReady()
```

`SimulatorWorker` runs entirely on the background thread. It emits `frameReady(WorldState)`
each time a new frame is parsed. Qt's signal/slot mechanism delivers this to the main thread
safely without manual locking.

### Trace Visualization

Each bug's last N positions are stored in a `deque<pair<int,int>>`. The newest position is
always at index 0. When rendering, trace cells are colored with decreasing alpha (180 to 40)
based on their age rank, creating a smooth fade effect. N is configurable at runtime via the
spinbox in the Qt app or by typing a number in the terminal client.

### SharedState

The terminal client wraps all mutable shared state (`traceN`, `traceHistory`, `historyMutex`,
`running`) into a single `SharedState` struct that is passed by reference to all functions
that need it. This avoids global variables and makes the data flow explicit, which also makes
the functions easier to test in isolation.

---

## Unit Tests

Tests are in `test_client.cpp` and cover:

| #  | Function              | What is tested                                      |
|----|-----------------------|-----------------------------------------------------|
| 1  | `extractBugPositions` | Correct row/col for R and B                         |
| 2  | `updateHistory`       | Newest entry at index 0, capped at N                |
| 3  | `updateHistory`       | Lowering N trims history immediately                |
| 4  | `updateHistory`       | Stationary bug records every tick                   |
| 5  | `updateHistory`       | N=0 produces empty history without crash            |
| 6  | `updateHistory`       | Fewer ticks than N stores only available entries    |
| 7  | `isBugChar`           | Returns true for R, r, B, b                         |
| 8  | `isBugChar`           | Returns false for terrain and food characters       |
| 9  | `extractBugPositions` | Empty grid returns empty map                        |
| 10 | `extractBugPositions` | Grid with no bugs returns empty map                 |
| 11 | `extractBugPositions` | Food-carrying bugs r and b detected                 |
| 12 | `extractBugPositions` | Only first occurrence of duplicate stored           |
| 13 | `updateHistory`       | R and B histories tracked independently             |
| 14 | `updateHistory`       | Empty positions map does not corrupt history        |
| 15 | `updateHistory`       | Raising N after lowering allows growth              |
| 16 | `updateHistory`       | Very large N does not crash                         |
| 17 | `parseResponse`       | Full valid response parsed correctly                |
| 18 | `parseResponse`       | Missing CYCLE returns valid=false                   |
| 19 | `parseResponse`       | Empty string returns valid=false without crash      |

---

## Known Limitations

- **Single bug per character:** The simulator protocol represents bugs as characters (`R`, `r`,
  `B`, `b`). Since there are many bugs of each type, the client can only track one position per
  character — whichever appears first when scanning the grid top-to-bottom. This means only one
  Red and one Black bug will display a full trace at any time.
- **Terminal client display:** The terminal client requires a wide terminal to display large
  worlds without line wrapping. Resize your terminal window if the grid appears broken.
- **X server required for Qt client:** The Qt GUI requires a running X server on your local
  machine when connecting via SSH. The terminal client works without one.

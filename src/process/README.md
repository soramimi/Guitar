# ProcessExample

A multiplatform C++ library for spawning child processes and capturing their output, together with a sample application that exercises the library. The project started as an experiment with Windows ConPTY and is being reorganized as a reusable process-launching library for applications.

The sample program exercises the available backends and prints the captured output. On Windows it currently runs `git --version` through all six Windows implementations; on non-Windows builds `sampleapp/main.cpp` contains a simple fixed selector for the POSIX pipe and PTY variants.

## Features

| Class | Platform | Method | Interface |
|---|---|---|---|
| `BasicProcessWin` | Windows | Anonymous pipes + `CreateProcessW` (stdout/stderr merged) | `_AbstractBasicProcess` |
| `BasicProcessWinConPty` | Windows 10 1809+ | `CreatePseudoConsole` (ConPTY) | `_AbstractBasicProcess` |
| `ProcessWin` | Windows | Pipes + `CreateProcessW` (separate stdout/stderr queues) | `AbstractProcess` |
| `ProcessWinConPty` | Windows 10 1809+ | ConPTY behind the abstract PTY interface | `AbstractPtyProcess` |
| `ProcessWinPty` | Windows | [winpty](https://github.com/rprichard/winpty) (legacy/compat) | `AbstractPtyProcess` |
| `ProcessWinConPtyWithWorker` | Windows 10 1809+ | ConPTY hosted in a separate `conpty-worker.exe` | `AbstractPtyProcess` |
| `ProcessPosix` | Linux / macOS | `pipe()` + `fork()` + `execvp()` (separate stdout/stderr) | `AbstractProcess` |
| `ProcessPosixPty` | Linux / macOS | `posix_openpt()` pseudo-terminal | `AbstractPtyProcess` |

### Interfaces

- `AbstractProcess` — plain pipe-based process. Final output is read from `stdout_bytes()` / `stderr_bytes()` after `wait()`.
- `AbstractPtyProcess` — pseudo-terminal process. Adds an incremental `read_output()` queue, a completion callback, and a change-directory setting on top of the result buffers.
- `_AbstractBasicProcess` — low-level Windows interface with configurable output sinks (`Options::output_stdout` / `output_vector` / `output_queue`) and a `wait_for_output()` prompt watcher (`BasicProcessWin`).

### Why pseudo-terminals?

Plain pipes only capture stdout/stderr. Some programs (including Git in certain configurations) detect that their output is not a terminal and change their behavior — e.g., disabling color or progress output. Using a pseudo-terminal (PTY) makes the child process behave as if it is writing to a real terminal.

VT/ANSI escape sequences emitted on a PTY are removed by the stateful `VtStripper` class (`src/ProcessWinHelper.h`) on the ConPTY backend. It keeps its parsing state across `ReadFile` calls, so sequences split over chunk boundaries are handled correctly.

### ConPTY worker separation

`ProcessWinConPtyWithWorker` launches `conpty-worker.exe` (built from the same `sampleapp/main.cpp` with `CONPTY_WORKER` defined) through ordinary pipes, and the worker owns the ConPTY. This avoids interference between an outer pseudo-terminal (e.g. an IDE-embedded terminal) and the inner ConPTY. The command line is passed to the worker as a Base64-encoded argument and validated with strict Base64 decoding on the worker side.

## Building

qmake project files:

```sh
# library + sample app
qmake process-example.pro
make            # or nmake / jom with MSVC on Windows

# Windows only: ConPTY worker executable
qmake conpty-worker.pro
make
```

Outputs go to `_bin/`. On Windows, `ProcessWinConPtyWithWorker` requires `conpty-worker.exe` to be placed next to the main executable.

## Dependencies

- C++17
- Windows: MSVC or MinGW; Windows 10 version 1809+ for the ConPTY backends (`BasicProcessWinConPty::is_conpty_available()` checks at runtime)
- winpty (Windows, legacy backend): pre-built headers/libs bundled under `winpty/`
- Linux / macOS: nothing special (`posix_openpt`, `fork`, `execvp`)

## Project Structure

```
process-example.pro   — qmake project for the sample app
conpty-worker.pro     — qmake project for the Windows ConPTY worker
process.pri           — shared qmake fragment listing the library sources
src/                  — the process library
sampleapp/            — sample/experimental application (main.cpp and helpers)
winpty/               — bundled winpty library
_bin/                 — build output
```

See `AGENTS.md` for implementation details and `ConPTY.md` for the ConPTY experiment notes.

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

The abstract ConPTY backends preserve VT/ANSI sequences for terminal rendering. The low-level `BasicProcessWinConPty` optionally strips them with a stateful `VtStripper`.

### ConPTY worker separation

`ProcessWinConPtyWithWorker` launches a standalone `conpty-worker.exe` built from `src/ConPtyWorkerMain.cpp`. The worker owns the ConPTY. Its stdout carries raw UTF-8/VT bytes; stdin carries versioned requests (initial command/environment, terminal input, resize, input close, stop); stderr carries a framed final result. Protocol bytes never reach the terminal renderer. Each frame has a 16-byte little-endian magic/version/type/length header and a maximum 16 MiB payload. The host and worker must be deployed together; the old Base64 command-line protocol is no longer supported.

The parent creates a suspended worker, assigns it to a kill-on-close Job Object, then resumes it. All descendant clients are cleaned up on host termination or session completion. Failure to assign the job is reported rather than silently weakening cleanup. The worker process itself uses `CREATE_NO_WINDOW`; the inner ConPTY client does not.

- Call `resize_terminal()` before start for the initial dimensions and while running for later changes.
- `start(command, env, use_input)` accepts NUL-separated environment overrides and uses `set_change_dir()` without changing the host's current directory. `use_input=false` rejects user input while keeping ConPTY's input pipe alive for output-only commands.
- Output is retained after `wait()`/`stop()` until consumed or the next `start()`. The abstract queue limit and optional result-vector capture apply. `wait_for_output()` requires result-vector capture and does not consume the output queue.
- `wait(timeout)` returns a snapshot; a timeout leaves the session running. A final result distinguishes worker/protocol failures from the client's exit code. `started` during startup describes the launched worker; the final result describes the client.
- Completion fires once per start attempt, after final output is drained, including launch failures. Queue UI updates. The callback must not restart, stop, or destroy the object; it may call `wait(0)` or consume output.
- Serialize lifecycle calls (`start`, `stop`, destruction) and configure environment/directory/worker path before start. Input, resize, output reads and timed waits may run concurrently. Stop requests cooperative termination and kills the job after two seconds if needed.
- Default worker lookup is beside the host executable; `set_worker_path()` can override it for packaging/tests.


## Building

qmake project files:

```sh
# library + sample app
qmake process-example.pro
make            # or nmake / jom with MSVC on Windows

# Windows only: ConPTY worker executable
qmake conpty-worker.pro
make

# Windows integration tests (also builds the worker)
qmake WorkerProcessTest/WorkerProcessTest.pro
nmake
_bin/WorkerProcessTest.exe
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

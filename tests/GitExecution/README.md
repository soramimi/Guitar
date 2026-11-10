# Git execution integration tests (Linux)

Build Guitar in release mode, including its `_build` objects, then run:

```sh
mkdir -p _build-git-execution
cd _build-git-execution
qmake6 ../tests/GitExecution/GitExecution.pro CONFIG+=release CONFIG+=reuse_app_objects
make -j2
QT_QPA_PLATFORM=offscreen ./GitExecutionTest
```

Qt 6 and GoogleTest are required. `reuse_app_objects` links the already-built
release application objects to avoid compiling the GUI twice. Omit it to compile
the application sources independently inside the test build directory.

Tests call the real Git, GitBasicSession and process implementations in disposable
repositories. Global/system Git settings are isolated, and no push/fetch or
external SSH connections are made. A local SSH stub records the exact arguments
and fails immediately. Temporary files are cleaned up automatically.

Coverage includes normal/PTY argv preservation, empty values, embedded quotes,
Windows command-line serialization, literal pathspecs, leading hyphens, reset of
one file without changing others, stage/unstage, Git's quoted output, rename
arrows, file history and AI diff assembly. Windows serialization is checked by
portable unit cases; execution through CreateProcessW still needs native Windows
validation.

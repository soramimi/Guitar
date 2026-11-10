# Git execution integration tests (Linux)

Build Guitar in release mode, including its `_build` objects, then run:

```sh
mkdir -p _build-git-execution
cd _build-git-execution
qmake6 ../tests/GitExecution/GitExecution.pro CONFIG+=release CONFIG+=reuse_app_objects
make -j2
QT_QPA_PLATFORM=offscreen ./GitExecutionTest
python3 ../tests/GitExecution/tls_test.py ./GitExecutionTest
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

AI regressions use an in-process HTTP stub and a Python loopback TLS server.
They verify index-only diffs (including initial/root/amend commits), strict JSON
validation, size limits, cancellation/deadlines, response limits, redirect policy,
untrusted-certificate rejection, and omission of dummy credentials/diffs from
logs. No external AI service or real API key is used. The AI transport has a
120-second total deadline, a 4 MiB response limit and 25 ms cancellation polling;
commit diffs are limited to 200000 bytes and include text patches/binary summaries
from the index, without running external diff or textconv programs.

Model-settings regressions use the real modal dialogs with a disposable encrypted
Vault. They cover Custom/provider round trips, rendering and row ordering,
protocol preservation, pending key edits, nested/parent cancellation, failed and
cancelled model queries, presets, legacy-file loading, malformed inputs, and
failed encryption/metadata writes. Model files retain their legacy on-disk shape
for compatibility; the reader also accepts array records. Model and encrypted-key
files use atomic replacement. If committing metadata fails after key replacement,
the old encrypted keys are restored; a failed rollback attempts to retain an
`apikeys.bin.recovery` copy and reports failure instead of accepting the dialog.

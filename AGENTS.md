# Guitar: development notes

Guidance for contributors and coding agents working on this repository. More specific `AGENTS.md` files exist in some subdirectories (for example `src/process`, `src/texteditor`, and `src/localvault`); read and follow them before changing code in those subtrees.

## Build and validation

- Guitar is a C++20 / Qt 6 application built with qmake (`Guitar.pro`). Linux builds are known to work with GCC 14 and Qt 6.
- Make sure the Qt 6 tools (`qmake6` or `qmake`), a C++20 compiler, and the required libraries are on `PATH` before building.
- From a `_build` directory, run `qmake6 ../Guitar.pro CONFIG+=release`, then `make -j$(nproc)`. The application is produced at `_bin/Guitar`.
- The required FileTypePlugin and IncrementalSearchPlugin must be built and loadable under `lib/soramimi.jp`. Use the existing subproject build scripts, and check that they load after changing plugin initialization or interfaces. Do not silently proceed after an initialization failure.
- Linux integration tests are in `tests/GitExecution/GitExecution.pro`. After a matching release build of the application, from `_build-git-execution`, run `qmake6 ../tests/GitExecution/GitExecution.pro CONFIG+=release CONFIG+=reuse_app_objects`, then `make` and `QT_QPA_PLATFORM=offscreen ./GitExecutionTest`. This configuration reuses the application's object files, so rebuild the application first after source or header changes.
- Other regression projects are `tests/PluginRuntime`, `src/texteditor/PtyProcessTest`, and `src/texteditor/TerminalScreenTest`; use the actual `.pro` file in each directory. Run the suites relevant to the code you change.
- Embedded MeCab `Tokenizer::open` uses `CHECK_FALSE` early returns. Keep dictionary ownership in RAII holders until registration in `dic_` succeeds, including on user-dictionary validation failures. Run PluginRuntime with LeakSanitizer when changing this ownership.
- `prepare.rb` runs during application builds. Review tracked diffs afterwards and keep build artifacts and disposable repositories out of commits.
- Use temporary repositories and synthetic credentials in tests. Linux/offscreen checks do not establish Windows behavior or live behavior of external AI services and accounts.

## Git execution and file paths

Relevant code: `Git`, `GitRunner`, process argument handling, `GitTypes::gitTrimPath`.

- Preserve argument boundaries from command construction through process execution. Do not add a shell interpretation step, and do not assume shell quoting and the application's command parser follow the same rules.
- Keep literal filenames (spaces, quotes, backslashes, leading dashes, pathspec metacharacters) distinct from revisions and options. Use `--` and literal pathspec handling where the Git operation requires them.
- Git's quoted path output uses C escapes and octal bytes. Preserve non-ASCII bytes, and filenames containing tabs, newlines, or leading/trailing whitespace, through decoding.
- Validate changes against Git itself in isolated repositories, including unusual filenames; assertions on command strings alone are insufficient.

## Generative AI and commit messages

Relevant code: `src/ai`, `GenerateCommitMessageThread`, `GenerateCommitMessageDialog`, and the model selection/settings dialogs.

- Preserve HTTPS certificate verification and request timeouts. Do not forward credentials across redirects or write credentials to logs. Local tests should use synthetic keys and loopback servers.
- Commit-message generation uses staged (index) changes. Keep the model, endpoint, credentials, and diff data stable for the lifetime of a request; handle cancellation and completion without touching destroyed UI objects.
- For the Custom provider, keep the endpoint, model ID, API compatibility, credential method, and key symbol as independent values. Switching provider or list selection must retain draft edits.
- A failed model query must not erase the custom model or endpoint. Canceling a dialog must not persist its drafts, including credentials edited through a nested dialog.
- Distinguish malformed/unreadable settings from an empty configuration. Reset per-record parser state; stage metadata safely and propagate write failures before saving associated credentials.
- Regression coverage for model settings is in `tests/GitExecution/model_settings_tests.cpp`; AI/process integration coverage is in `git_execution_tests.cpp`.

## History loading, fetch, and commit refresh

- History loading uses `Git::readCommits` (`cat-file --batch`) with length-framed bodies and the existing raw commit parser. Keep the bounds checks, process-failure handling, and the independent-reader fallback. Do not replace it with delimiter-split subjects or eager signature verification.
- Fetch runs before refreshing history. Reuse unchanged history only when the repository, ref/HEAD/shallow state, log limit, and dirty-state presence all match. Always refresh worktree details, and reap the completed PTY worker even when skipping a reopen. Preserve selection by commit ID.
- After committing, wait for successful PTY completion, refresh history without reopening the repository, then explicitly invoke the HEAD-row selection handler after installing records. Signals are blocked during the table rebuild, so selecting the same row alone does not refresh the file list. Clear callbacks left by earlier PTY operations before a locally awaited commit.
- Linux GUI regression: `python3 tests/GitExecution/fetch_ui_smoke.py` after building the application (requires Xvfb and xdotool). It creates private local remotes and configuration, and checks actual command counts for unchanged/changed fetch, selection, dirty-state changes, and offline Update.

## Independent object and pack reading

Relevant code: `GitObjectManager`, `GitPack`, `GitPackIdxV2`, `MemoryReader`, and the fallback/cache connection.

- Treat pack, index, and loose-object bytes as untrusted. Check every variable-length integer, instruction operand, seek, read, and conversion before use.
- A delta must match its base and result sizes. Check copy ranges by subtraction after validating the offset, to avoid addition overflow. Distinguish failure from a valid empty result.
- Bound delta-chain traversal and detect cycles; reject missing bases. Propagate failure to the external `git cat-file` fallback instead of caching partial output as success.
- Decode index v2 high-bit offsets through the 64-bit table with bounds checks and endian-independent reads. A failed parse must leave no partially usable index.
- Use unsigned, sufficiently wide arithmetic with checked shifts for pack sizes. Enforce expansion bounds before appending, and require the exact pack entry size, including zero-byte entries. Loose-object expansion limits and exact packed sizes are separate contracts.
- Always release zlib state, including on error paths; handle negative reads and truncated streams explicitly. Verify entry CRCs for both delta and non-delta entries.
- Tree mode `160000` denotes a commit (gitlink); preserve names through the existing quoted-path decoder. Reject malformed binary trees rather than returning partial success.
- The independent reader caps expanded objects at 1,000,000,000 bytes and delta traversal at 128 entries; rejected objects fall back to external Git. These are application limits, not Git format limits.
- Regression coverage is in `tests/GitExecution/pack_tests.cpp`. Compare ordinary and delta-packed results with Git's output, and keep the malformed-input cases. Use sanitizers for bounds and overflow regressions where available.

## Areas without automated coverage

The optional OnePassword integration, AppImage packaging, Windows execution, and live external AI providers are not covered by the Linux test suites above. Verify them separately when changing them, and keep changes scoped to the requested task rather than expanding into unrelated code.

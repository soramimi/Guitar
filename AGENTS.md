# Guitar: confirmed development notes

These notes cover the application paths examined in this session. They are not a review of the entire repository. Read any more specific AGENTS.md before changing its subtree; existing instructions in `src/process`, `src/texteditor`, and `src/localvault` apply there.

## Build and validation

- Guitar is a C++20 / Qt application built with qmake (`Guitar.pro`). The cloud session has validated GCC 14 and Qt 6 on Linux.
- In this cloud instance, `. /workspace/toolchain/activate.sh` selects the installed tools and libraries. These absolute paths are session conveniences, not portable project requirements.
- From `_build`, run `qmake6 ../Guitar.pro CONFIG+=release`, then `make -j5`. The application is `_bin/Guitar`.
- Required FileTypePlugin and IncrementalSearchPlugin must be built and loadable under `lib/soramimi.jp`. Follow the existing subproject build scripts; check loading after changes to plugin initialization or interfaces. Do not silently proceed after initialization failure.
- Linux integration tests are in `tests/GitExecution/GitExecution.pro`. After a matching release application build, from `_build-git-execution`, run `qmake6 ../tests/GitExecution/GitExecution.pro CONFIG+=release CONFIG+=reuse_app_objects`, then `make -j2` and `QT_QPA_PLATFORM=offscreen ./GitExecutionTest`. This configuration reuses application object files, so rebuild the app first after source/header changes.
- Related regression projects are `tests/PluginRuntime`, `src/texteditor/PtyProcessTest`, and `src/texteditor/TerminalScreenTest`; use the actual `.pro` filenames in each directory. Run suites relevant to the affected code.
- `prepare.rb` runs during application builds. Review tracked diffs afterward and keep build artifacts and disposable repositories out of commits.
- Use temporary repositories and synthetic credentials for tests. Linux/offscreen checks do not establish Windows behavior or live external AI/account integration.

## Git execution and file paths

Examined paths include `Git`, `GitRunner`, process argument handling, and `GitTypes::gitTrimPath`.

- Preserve argument boundaries from command construction through process execution. Do not add a shell interpretation step or assume shell quoting and the application's command parser have identical rules.
- Keep literal filenames (spaces, quotes, backslashes, leading dashes, and pathspec metacharacters) distinct from revision/options. Use `--` and literal pathspec handling where the Git operation requires them.
- Git's quoted path output uses C escapes and octal bytes. Preserve non-ASCII bytes and filenames containing tabs/newlines or leading/trailing whitespace through decoding.
- Validate changes against Git itself in isolated repositories, including unusual filenames; command-string-only assertions are insufficient.

## Generative AI and commit messages

Examined paths include `src/ai`, `GenerateCommitMessageThread`, `GenerateCommitMessageDialog`, and the model selection/settings dialogs.

- Preserve HTTPS certificate verification and request timeouts. Do not forward credentials across redirects or print credentials in logs. Local tests should use synthetic keys and loopback servers.
- Commit-message generation uses staged/index changes. Keep model, endpoint, credentials, and diff data stable for the lifetime of a request; handle cancellation and completion without accessing destroyed UI objects.
- For the Custom provider, preserve the endpoint, model ID, API compatibility, credential method, and key symbol as independent values. Switching provider or list selection must retain draft edits.
- A failed model query must not erase the custom model or endpoint. Canceling a dialog must not persist its drafts, including credentials edited through a nested dialog.
- Distinguish malformed/unreadable settings from an empty configuration. Reset per-record parser state; stage metadata safely and propagate write failures before saving associated credentials.
- History loading uses `Git::readCommits` (`cat-file --batch`) with length-framed bodies and the existing raw commit parser; keep bounds, process failure handling and the independent-reader fallback. Do not replace it with delimiter-split subjects or eager signature verification.
- Fetch should run before refreshing history. Reuse unchanged history only when the repository, ref/HEAD/shallow state, log limit and dirty-state presence match. Always refresh worktree details, and reap the completed PTY worker even when skipping a reopen. Preserve selection by commit ID.
- Linux GUI regression: `python3 tests/GitExecution/fetch_ui_smoke.py` after building the app (requires Xvfb and xdotool). It creates private local remotes/configuration and verifies actual command counts for unchanged/changed fetch, selection, dirty-state changes and offline Update.
- Regression coverage for model settings is in `tests/GitExecution/model_settings_tests.cpp`; AI/process integration coverage is in `git_execution_tests.cpp`.

## Independent cat-file and pack reading

Examined paths are `GitObjectManager`, `GitPack`, `GitPackIdxV2`, `MemoryReader`, and the fallback/cache connection.

- Treat pack/index/loose-object bytes as untrusted. Check every variable-length integer, instruction operand, seek, read, and conversion before using it.
- A delta must match its base and result sizes. Check copy ranges with subtraction after validating the offset, avoiding addition overflow. Separate failure from a valid empty result.
- Bound delta traversal and detect cycles; reject missing bases. Preserve failure up to the external `git cat-file` fallback instead of caching partial output as success.
- Decode index v2 high-bit offsets through the 64-bit table with bounds checks and endian-independent reads. Failed parses must leave no partially usable index.
- Use unsigned, sufficiently wide pack size arithmetic with checked shifts. Enforce expansion bounds before appending, and require exact pack entry size including zero-byte entries. Loose-object expansion limits and exact packed sizes are separate contracts.
- Always release zlib state, including error paths; handle negative reads and truncated streams explicitly. Verify entry CRCs for delta and non-delta entries.
- Tree mode `160000` denotes a commit (gitlink); preserve names through the existing quoted-path decoder. Reject malformed binary trees rather than returning partial success.
- Regression coverage is in `tests/GitExecution/pack_tests.cpp`. Compare ordinary and delta-packed results with Git's output, and retain malformed-input cases. Use sanitizers for bounds and overflow regressions where available.
- The independent reader caps expanded objects at 1,000,000,000 bytes and delta traversal at 128 entries; rejected objects can use the external Git fallback. These limits do not describe Git's own format limits.

## Scope limits

Optional OnePassword integration, AppImage packaging, Windows execution, and live external AI providers were not validated in this session. Do not infer their behavior from these notes or expand a requested change into unrelated code solely to complete this document.

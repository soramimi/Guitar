# Plugin runtime regression tests (Linux)

Build the release application and its required plugins first. These tests load
`lib/soramimi.jp/libfiletypeplugin.so` and `libincrementalsearchplugin.so` and need
Qt 6, GoogleTest and the Zstd development library.

From the repository root:

```sh
mkdir -p _build-loaderfixture-release _build-plugin-runtime-release
cd _build-loaderfixture-release
qmake6 ../tests/PluginRuntime/fixtures/loaderfixture.pro CONFIG+=release
make -j2
cd ../_build-plugin-runtime-release
qmake6 ../tests/PluginRuntime/PluginRuntime.pro CONFIG+=release CONFIG+=sanitize
make -j2
ASAN_OPTIONS=detect_leaks=0 ./PluginRuntimeTest
```

The `sanitize` option instruments the test executable, shared Zstd adapter and
romaji converter with AddressSanitizer and UndefinedBehaviorSanitizer. The loaded
application plugins retain their own build configuration. Leak checking is
excluded because the Qt plugin loader retains plugin instances for process life.

Coverage includes compression/decompression (including empty and concatenated
frames), truncated/corrupt streams, input/output failures, strict output limits,
non-terminated string views, Japanese search, 2/4 GiB sparse files, corrupt magic
and search dictionaries, invalid matrix lengths, and plugin factory/init errors.
The sparse files consume little actual disk space and are removed automatically.
Dictionary fault injection changes only the test process's memory and restores
it before continuing; it never rewrites checked-in dictionary data.

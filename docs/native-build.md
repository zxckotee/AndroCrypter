# Native (VeraCrypt core) build on Linux

## Shell (fish vs bash)

Fixture and CI scripts are **bash**. From the repository root:

```bash
bash scripts/fixtures/make-oracle-matrix.sh /tmp/om quick
```

Or use the wrapper (matrix + cmake + interop gtests):

```bash
bash scripts/native/run-oracle-quick.sh /tmp/om
```

In **fish**, prefix with `bash` (or run `chmod +x` and `./scripts/...`). Plain `scripts/foo.sh` is not a fish command.

## Toolchain

- **C/C++**: GCC is the supported default (`CMAKE_C_COMPILER=gcc`, `CMAKE_CXX_COMPILER=g++`).
- **Clean rebuild** after pulling native changes: `rm -rf native/build-ci` then re-run cmake.

## Common compile errors

### `expected ';' before 'volumeSize'` / `unsigned __int64`

You need the current `native/compat/hcvault_preinclude.h` (defines `__int64`) and CMake must pass `-include .../hcvault_preinclude.h` and `-fms-extensions`. Update the branch and wipe the build directory.

### `__builtin_ia32_aesenc128' needs ... -maes`

On x86_64, CMake adds `-maes -mpclmul` automatically. If it still fails (unusual `CMAKE_SYSTEM_PROCESSOR`), configure with:

```bash
cmake -S native -B native/build-ci -DHCVAULT_FORCE_PORTABLE_CRYPTO=ON ...
```

### `fish: Unknown command: native/build-ci/hcvault_volume_tests`

Run the binary explicitly: `./native/build-ci/hcvault_volume_tests` (after a successful build), or use `bash scripts/native/run-oracle-quick.sh`.

## Interop tests only

```bash
cmake -S native -B native/build-ci -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build native/build-ci -j"$(nproc)"
HCVAULT_ORACLE_DIR=/tmp/om native/build-ci/hcvault_volume_tests --gtest_filter='Interop*'
```

Oracle volumes are **not** committed; generate under `/tmp` or set `HCVAULT_ORACLE_DIR`.
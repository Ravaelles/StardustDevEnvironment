# Plan: Fix StardustDevEnvironment Build & Test Suite

## Goal
Resolve build errors and ensure the project builds and runs cleanly on Linux with Clang/GCC.

## Tasks
1. **Fix Missing `<cstdint>` in `BWAPI/Game.h`**:
   - Add `#include <cstdint>` to `3rdparty/openbw/bwapi/bwapi/include/BWAPI/Game.h`.
   - Verify if any other BWAPI or OpenBW headers have undeclared fixed-width integer types.

2. **Verify Full Build**:
   - Run `cmake --build build -j$(nproc)` using Clang (test both Clang-14 and modern Clang-18 if possible).
   - Identify and resolve any downstream compilation or linking errors.

3. **Verify Tests**:
   - Check if unit/integration tests can build and execute (noting any requirements for MPQ files if applicable).

4. **Hygiene & Automation**:
   - Update `STATUS.md` with each milestone.
   - Commit changes cleanly.

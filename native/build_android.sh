#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-${ANDROID_NDK:-}}}"
if [[ -z "$NDK" ]]; then
  SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
  if [[ -n "$SDK" ]]; then NDK="$SDK/ndk/27.2.12479018"; fi
fi

if [[ -z "$NDK" || ! -f "$NDK/build/cmake/android.toolchain.cmake" ]]; then
  echo "Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_HOME." >&2
  exit 2
fi

cmake -S "$ROOT/native" -B "$ROOT/build/native" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build/native" --config Release --parallel

RES="$ROOT/patches/src/main/resources/pgo/arm64-v8a"
mkdir -p "$RES"
cp "$ROOT/build/native/libmain.so" "$RES/libmain.so"
cp "$ROOT/build/native/libpgo_hook.so" "$RES/libpgo_hook.so"
cp "$ROOT/build/native/libpgo_rpc_probe.so" "$RES/libpgo_rpc_probe.so"
echo "Native payloads installed into $RES"

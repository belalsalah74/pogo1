# Pokémon GO Runtime Patches for Morphe

Experimental Morphe patches for **Pokémon GO 0.431.0 (ARM64)**. This branch adds a separate, opt-in RPC Runtime Probe for gathering runtime metadata needed to investigate the local Shiny Atlas adapter.

## Available patches

The patch index below is generated from the Morphe patch metadata during release. It is kept between markers because the release workflow updates it automatically.

<!-- PATCHES_START EXPANDED -->
> **[v1.1.1](https://github.com/belalsalah74/pogo1/releases/tag/v1.1.1)**&nbsp;&nbsp;•&nbsp;&nbsp;`rpc-runtime-probe`&nbsp;&nbsp;•&nbsp;&nbsp;3 patches total
<details open>
<summary>📦 Pokémon GO&nbsp;&nbsp;•&nbsp;&nbsp;3 patches</summary>
<br>

**🎯 Supported versions:**

| 0.431.0 |
| :---: |

| 💊&nbsp;Patch | 📜&nbsp;Description | ⚙️&nbsp;Options |
|----------|----------------|-----------|
| [Enhanced throw](#enhanced-throw) | Experimental Pokémon GO 0.431.0 ARM64 catch-request hook; runtime behavior is not yet verified. |  |
| [Google sign-in callback](#google-sign-in-callback) | Routes the Google OAuth callback scheme to UnityMainActivity for Pokémon GO 0.431.0. |  |
| [RPC runtime probe](#rpc-runtime-probe) | Diagnostic-only Pokémon GO 0.431.0 ARM64 probe that logs IL2CPP RPC method signatures, code RVAs and related fields. |  |

</details>
<!-- PATCHES_END -->

The RPC probe is a diagnostic prototype, **not a working authenticated game-session adapter**. Runtime logs are needed before deciding how a supported adapter should be designed. Use a clean official 0.431.0 ARM64 APK as the patching source; do not patch an APK that already contains `libmain_orig.so`.

## Build on Fedora/Linux

Install Java 21, Android SDK/NDK, CMake, and the Android platform tools. Then configure the SDK/NDK locations and build the native payloads followed by the Morphe bundle:

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
./native/build_android.sh
./gradlew :patches:buildAndroid --no-daemon
```

The CI workflow compiles the native payloads and bundles them with the Morphe patch source. The resulting `.mpp` file is in `patches/build/libs/`.

## Test the RPC runtime probe

1. Patch an unmodified Pokémon GO 0.431.0 ARM64 APK, selecting **RPC runtime probe** only.
2. Install it and launch the game to the map screen.
3. On a computer with Android platform-tools and USB debugging enabled, run:

   ```sh
   adb logcat -c
   adb logcat -s PGORPCPROBE431:I PGOLOADER431:I '*:S'
   ```

4. Save the filtered lines from `PGORPCPROBE431`, especially `CLASS`, `METHOD`, `PARAM`, `FIELD`, `STATIC_REF`, `OWNER_FIELD`, and `RPC metadata probe complete`. Share only those lines; do not share account credentials or unrelated logs.

If the game crashes or never reaches the completion marker, report the first relevant error lines. Do not enable the experimental Enhanced Throw patch for this test.

## Patch source

Add the source in Morphe Manager: https://morphe.software/add-source?github=belalsalah74/pogo1

Native code is in `native/`; Morphe patch definitions are in `patches/src/main/kotlin/`. The RPC probe is loaded only when its patch adds `libpgo_rpc_probe.so`; the common `libmain.so` wrapper keeps native feature payloads optional.

## License

GPL-3.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

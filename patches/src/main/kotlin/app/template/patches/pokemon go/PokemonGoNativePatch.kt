package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.rawResourcePatch

private val COMPATIBILITY_POKEMON_GO = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(AppTarget(version = "0.431.0")),
)

/**
 * Shared, hidden native bootstrap. Feature patches depend on this so the original
 * libmain.so is backed up only once when one or more native features are selected.
 */
val pokemonGoNativeBasePatch = rawResourcePatch {
    compatibleWith(COMPATIBILITY_POKEMON_GO)

    execute {
        val dir = get("lib/arm64-v8a")
        val original = dir.resolve("libmain.so")
        val backup = dir.resolve("libmain_orig.so")

        require(original.exists()) { "lib/arm64-v8a/libmain.so not found" }
        require(!backup.exists()) {
            "libmain_orig.so already exists; use a clean official Pokémon GO 0.431.0 APK"
        }

        val patchClassLoader = object {}.javaClass.classLoader
            ?: error("Unable to resolve the Pokémon GO patch bundle class loader")
        val bootstrap = patchClassLoader
            .getResourceAsStream("pgo/arm64-v8a/libmain.so")
            ?.use { it.readBytes() }
            ?: error("Missing bundled pgo/arm64-v8a/libmain.so")

        original.copyTo(backup, overwrite = false)
        original.writeBytes(bootstrap)
    }
}

/** Experimental catch serializer hook, separated from the shared native loader. */
@Suppress("unused")
val pokemonGoNativeBootstrap = rawResourcePatch(
    name = "Enhanced throw",
    description = "Experimental Pokémon GO 0.431.0 ARM64 catch-request hook; runtime behavior is not yet verified.",
    default = false,
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO)
    dependsOn(pokemonGoNativeBasePatch)

    execute {
        val dir = get("lib/arm64-v8a")
        val patchClassLoader = object {}.javaClass.classLoader
            ?: error("Unable to resolve the Pokémon GO patch bundle class loader")
        val payload = patchClassLoader
            .getResourceAsStream("pgo/arm64-v8a/libpgo_hook.so")
            ?.use { it.readBytes() }
            ?: error("Missing bundled pgo/arm64-v8a/libpgo_hook.so")
        dir.resolve("libpgo_hook.so").writeBytes(payload)
    }
}

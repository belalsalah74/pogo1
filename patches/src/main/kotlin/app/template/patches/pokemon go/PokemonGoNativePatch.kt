package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.rawResourcePatch

private val COMPATIBILITY_POKEMON_GO = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(
        AppTarget(
            version = "0.431.0",
        ),
    ),
)

/**
 * Experimental ARM64 hook for the verified CatchPokemonProto.WriteTo entry point
 * in Pokémon GO 0.431.0. Runtime behavior still needs device confirmation.
 */
val pokemonGoNativeBootstrap = rawResourcePatch(
    name = "Enhanced throw",
    description = "Experimental Pokémon GO 0.431.0 ARM64 hook that adjusts outgoing catch-request fields for an Excellent hit and curveball; runtime behavior is not yet verified.",
    default = false,
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO)

    execute {
        val dir = get("lib/arm64-v8a")
        val original = dir.resolve("libmain.so")

        val patchClassLoader = object {}.javaClass.classLoader
            ?: error("Unable to resolve the Pokémon GO patch bundle class loader")

        val bootstrap = patchClassLoader
            .getResourceAsStream("pgo/arm64-v8a/libmain.so")
            ?.readBytes()
            ?: error("Missing bundled pgo/arm64-v8a/libmain.so")

        val payload = patchClassLoader
            .getResourceAsStream("pgo/arm64-v8a/libpgo_hook.so")
            ?.readBytes()
            ?: error("Missing bundled pgo/arm64-v8a/libpgo_hook.so")

        require(original.exists()) {
            "lib/arm64-v8a/libmain.so not found"
        }

        dir.resolve("libmain_orig.so").let {
            original.copyTo(it, overwrite = true)
        }

        original.writeBytes(bootstrap)
        dir.resolve("libpgo_hook.so").writeBytes(payload)
    }
}

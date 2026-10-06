package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.rawResourcePatch

private val COMPATIBILITY_POKEMON_GO = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(
        AppTarget(
            version = "0.429.1",
        ),
    ),
)

/**
 * Experimental native loader infrastructure for Pokémon GO 0.429.1 arm64-v8a.
 */
val pokemonGoNativeBootstrap = rawResourcePatch(
    name = "Pokémon GO native research bootstrap",
    description = "Installs the experimental native research loader for Pokémon GO 0.429.1 arm64-v8a.",
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO)

    execute {
        val dir = get("lib/arm64-v8a")
        val original = dir.resolve("libmain.so")
        val loader = Thread.currentThread().contextClassLoader

        val bootstrap = loader
            .getResourceAsStream("pgo/arm64-v8a/libmain.so")
            ?.readBytes()
            ?: error("Missing bundled pgo/arm64-v8a/libmain.so")

        val payload = loader
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

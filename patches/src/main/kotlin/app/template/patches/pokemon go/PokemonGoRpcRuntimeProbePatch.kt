package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.rawResourcePatch

private val COMPATIBILITY_POKEMON_GO_RPC_PROBE = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(AppTarget(version = "0.431.0")),
)

/**
 * Read-only diagnostic patch for the Pokémon GO 0.431.0 ARM64 runtime.
 * It logs selected IL2CPP RPC method signatures/RVAs and related fields.
 * It does not invoke managed methods or send game RPCs.
 */
@Suppress("unused")
val pokemonGoRpcRuntimeProbe = rawResourcePatch(
    name = "RPC runtime probe",
    description = "Diagnostic-only Pokémon GO 0.431.0 ARM64 probe that logs IL2CPP RPC method signatures, code RVAs and related fields.",
    default = false,
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_RPC_PROBE)
    dependsOn(pokemonGoNativeBasePatch)

    execute {
        val dir = get("lib/arm64-v8a")
        require(dir.resolve("libmain_orig.so").exists()) {
            "Native bootstrap did not preserve libmain_orig.so"
        }

        val patchClassLoader = object {}.javaClass.classLoader
            ?: error("Unable to resolve the Pokémon GO patch bundle class loader")
        val payload = patchClassLoader
            .getResourceAsStream("pgo/arm64-v8a/libpgo_rpc_probe.so")
            ?.use { it.readBytes() }
            ?: error("Missing bundled pgo/arm64-v8a/libpgo_rpc_probe.so; build native payloads first")
        dir.resolve("libpgo_rpc_probe.so").writeBytes(payload)
    }
}

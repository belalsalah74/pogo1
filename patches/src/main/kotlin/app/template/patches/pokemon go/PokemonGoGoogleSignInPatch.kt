package app.pokemongo

import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.resourcePatch
import org.w3c.dom.Element

private val COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(AppTarget(version = "0.431.0")),
)

/**
 * Registers the Google OAuth callback URI observed in the working PGSharp
 * 1.272.2 build for Pokémon GO 0.431.0.
 *
 * This only restores Android intent routing; it does not bypass OAuth or
 * alter authentication tokens. Successful sign-in still depends on the
 * existing Google/Niantic authentication configuration.
 */
@Suppress("unused")
val pokemonGoGoogleSignInCallback = resourcePatch(
    name = "Google sign-in callback",
    description = "Registers the Google OAuth callback scheme used by the working PGSharp 1.272.2 build.",
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN)

    execute {
        document("AndroidManifest.xml").use { document ->
            val callbackScheme =
                "com.googleusercontent.apps.848232511240-dmrj3gba506c9svge2p9gq35p1fg654p"
            val activities = document.getElementsByTagName("activity")
            var customTabActivity: Element? = null

            for (i in 0 until activities.length) {
                val activity = activities.item(i) as? Element ?: continue
                if (activity.getAttribute("android:name") == "com.facebook.CustomTabActivity") {
                    customTabActivity = activity
                    break
                }
            }

            val target = customTabActivity
                ?: error("com.facebook.CustomTabActivity not found in AndroidManifest.xml")

            // Be idempotent so applying the patch to a manifest that already
            // contains the callback does not create duplicate intent filters.
            val filters = target.getElementsByTagName("intent-filter")
            for (i in 0 until filters.length) {
                val filter = filters.item(i) as? Element ?: continue
                val dataNodes = filter.getElementsByTagName("data")
                for (j in 0 until dataNodes.length) {
                    val data = dataNodes.item(j) as? Element ?: continue
                    if (data.getAttribute("android:scheme") == callbackScheme) {
                        return@use
                    }
                }
            }

            val intentFilter = document.createElement("intent-filter")
            val action = document.createElement("action")
            action.setAttribute("android:name", "android.intent.action.VIEW")
            intentFilter.appendChild(action)

            val defaultCategory = document.createElement("category")
            defaultCategory.setAttribute("android:name", "android.intent.category.DEFAULT")
            intentFilter.appendChild(defaultCategory)

            val browsableCategory = document.createElement("category")
            browsableCategory.setAttribute("android:name", "android.intent.category.BROWSABLE")
            intentFilter.appendChild(browsableCategory)

            val data = document.createElement("data")
            data.setAttribute("android:scheme", callbackScheme)
            intentFilter.appendChild(data)

            target.appendChild(intentFilter)
        }
    }
}

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
 * Routes the Google OAuth browser callback to Pokémon GO's Unity activity.
 *
 * This only changes Android intent routing. It does not bypass Google OAuth
 * or fix a server-side client-ID/signing-certificate mismatch.
 */
@Suppress("unused")
val pokemonGoGoogleSignInCallback = resourcePatch(
    name = "Google sign-in callback",
    description = "Routes the Google OAuth browser callback to UnityMainActivity for Pokémon GO 0.431.0.",
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN)

    execute {
        document("AndroidManifest.xml").use { document ->
            val callbackScheme =
                "com.googleusercontent.apps.848232511240-dmrj3gba506c9svge2p9gq35p1fg654p"
            val callbackActivityName =
                "com.nianticproject.holoholo.libholoholo.unity.UnityMainActivity"

            val activities = document.getElementsByTagName("activity")
            var callbackActivity: Element? = null

            for (i in 0 until activities.length) {
                val activity = activities.item(i) as? Element ?: continue
                if (activity.getAttribute("android:name") == callbackActivityName) {
                    callbackActivity = activity
                    break
                }
            }

            val target = callbackActivity
                ?: error("$callbackActivityName not found in AndroidManifest.xml")

            // The callback scheme must not also be registered on another
            // activity, or Android may send the redirect to the wrong handler.
            for (i in 0 until activities.length) {
                val activity = activities.item(i) as? Element ?: continue
                if (activity === target) continue

                val filters = activity.getElementsByTagName("intent-filter")
                for (fi in filters.length - 1 downTo 0) {
                    val filter = filters.item(fi) as? Element ?: continue
                    val dataNodes = filter.getElementsByTagName("data")
                    var removedCallback = false

                    for (di in dataNodes.length - 1 downTo 0) {
                        val data = dataNodes.item(di) as? Element ?: continue
                        if (data.getAttribute("android:scheme") == callbackScheme) {
                            data.parentNode?.removeChild(data)
                            removedCallback = true
                        }
                    }

                    if (removedCallback && filter.getElementsByTagName("data").length == 0) {
                        filter.parentNode?.removeChild(filter)
                    }
                }
            }

            // Avoid duplicate intent filters when the callback is already present.
            val targetFilters = target.getElementsByTagName("intent-filter")
            for (i in 0 until targetFilters.length) {
                val filter = targetFilters.item(i) as? Element ?: continue
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

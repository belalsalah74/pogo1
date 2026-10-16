package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.resourcePatch
import org.w3c.dom.Element

private val COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(AppTarget(version = "0.431.0")),
)

private const val GOOGLE_CALLBACK_SCHEME =
    "com.googleusercontent.apps.848232511240-dmrj3gba506c9svge2p9gq35p1fg654p"
private const val UNITY_ACTIVITY_NAME =
    "com.nianticproject.holoholo.libholoholo.unity.UnityMainActivity"

/**
 * Routes the Google OAuth browser callback to UnityMainActivity.
 *
 * This intentionally changes only the manifest. The previous bytecode patch
 * assumed a particular UnityMainActivity.onActivityResult implementation
 * (including mGameObjectName), which is not present in every 0.431.0 APK.
 */
@Suppress("unused")
val pokemonGoGoogleSignInCallback = resourcePatch(
    name = "Google sign-in callback",
    description = "Routes the Google OAuth callback scheme to UnityMainActivity for Pokémon GO 0.431.0.",
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN)

    execute {
        document("AndroidManifest.xml").use { document ->
            val activities = document.getElementsByTagName("activity")
            var target: Element? = null

            for (i in 0 until activities.length) {
                val activity = activities.item(i) as? Element ?: continue
                if (activity.getAttribute("android:name") == UNITY_ACTIVITY_NAME) {
                    target = activity
                    break
                }
            }

            val callbackActivity = target
                ?: error("$UNITY_ACTIVITY_NAME not found in AndroidManifest.xml")

            // Ensure Android has exactly one owner for this callback scheme.
            // Keeping it registered on a second activity can route the browser
            // result to the wrong handler. Remove existing copies first so the
            // patch is safe to reapply to manifests that already contain it.
            for (i in 0 until activities.length) {
                val activity = activities.item(i) as? Element ?: continue
                val filters = activity.getElementsByTagName("intent-filter")

                for (fi in filters.length - 1 downTo 0) {
                    val filter = filters.item(fi) as? Element ?: continue
                    val dataNodes = filter.getElementsByTagName("data")
                    var removedCallback = false

                    for (di in dataNodes.length - 1 downTo 0) {
                        val data = dataNodes.item(di) as? Element ?: continue
                        if (data.getAttribute("android:scheme") == GOOGLE_CALLBACK_SCHEME) {
                            data.parentNode?.removeChild(data)
                            removedCallback = true
                        }
                    }

                    if (
                        removedCallback &&
                        filter.getElementsByTagName("data").length == 0
                    ) {
                        filter.parentNode?.removeChild(filter)
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
            data.setAttribute("android:scheme", GOOGLE_CALLBACK_SCHEME)
            intentFilter.appendChild(data)

            callbackActivity.appendChild(intentFilter)
        }
    }
}

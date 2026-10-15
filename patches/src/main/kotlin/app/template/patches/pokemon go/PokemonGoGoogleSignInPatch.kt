package app.pokemongo

import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.extensions.InstructionExtensions.addInstructionsWithLabels
import app.morphe.patcher.patch.bytecodePatch
import app.morphe.patcher.patch.resourcePatch
import com.android.tools.smali.dexlib2.iface.instruction.FiveRegisterInstruction
import com.android.tools.smali.dexlib2.iface.instruction.OneRegisterInstruction
import com.android.tools.smali.dexlib2.iface.instruction.ReferenceInstruction
import com.android.tools.smali.dexlib2.iface.instruction.RegisterRangeInstruction
import com.android.tools.smali.dexlib2.iface.instruction.ThreeRegisterInstruction
import com.android.tools.smali.dexlib2.iface.instruction.TwoRegisterInstruction
import com.android.tools.smali.dexlib2.iface.reference.FieldReference
import com.android.tools.smali.dexlib2.iface.reference.MethodReference
import org.w3c.dom.Element

private val COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(AppTarget(version = "0.431.0")),
)


private const val UNITY_ACTIVITY =
    "Lcom/nianticproject/holoholo/libholoholo/unity/UnityMainActivity;"
private const val UNITY_PLAYER = "Lcom/unity3d/player/UnityPlayer;"

/**
 * Hidden bytecode dependency for [pokemonGoGoogleSignInCallback].
 *
 * The Unity activity normally forwards only Android's numeric activity result.
 * For callback object HL.PL, Google/FB browser redirects must instead be sent
 * back into Unity as the intent URI string.
 */
@Suppress("unused")
val googleSignInActivityResultForwarding = bytecodePatch {
    compatibleWith(COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN)

    execute {
        val activityClass = mutableClassDefByOrNull(UNITY_ACTIVITY)
            ?: error("Google sign-in: UnityMainActivity was not found in this APK")

        val onActivityResult = activityClass.methods.singleOrNull { method ->
            method.name == "onActivityResult" &&
                method.returnType == "V" &&
                method.parameterTypes.map { it.toString() } ==
                    listOf("I", "I", "Landroid/content/Intent;") &&
                (method.accessFlags and 0x0008) == 0
        } ?: error(
            "Google sign-in: expected instance method " +
                "UnityMainActivity.onActivityResult(II, Intent) was not found uniquely"
        )

        val implementation = onActivityResult.implementation
            ?: error("Google sign-in: onActivityResult has no bytecode")
        val instructions = implementation.instructions

        fun fieldReads(fieldName: String): List<Pair<Int, TwoRegisterInstruction>> =
            instructions.mapIndexedNotNull { index, instruction ->
                val field = (instruction as? ReferenceInstruction)?.reference as? FieldReference
                    ?: return@mapIndexedNotNull null
                val registers = instruction as? TwoRegisterInstruction
                    ?: return@mapIndexedNotNull null

                if (
                    field.name == fieldName &&
                    field.type == "Ljava/lang/String;" &&
                    instruction.opcode.name.startsWith("IGET_OBJECT")
                ) {
                    index to registers
                } else {
                    null
                }
            }

        val gameObjectField = fieldReads("mGameObjectName").singleOrNull()
            ?: error(
                "Google sign-in: expected one mGameObjectName field read in onActivityResult"
            )
        val callbackMethodField = fieldReads("mMethodName").singleOrNull()
            ?: error(
                "Google sign-in: expected one mMethodName field read in onActivityResult"
            )

        val unityCalls = instructions.mapIndexedNotNull { index, instruction ->
            val methodRef = (instruction as? ReferenceInstruction)?.reference as? MethodReference
                ?: return@mapIndexedNotNull null

            if (
                methodRef.definingClass == UNITY_PLAYER &&
                methodRef.name == "UnitySendMessage" &&
                methodRef.returnType == "V" &&
                methodRef.parameterTypes.map { it.toString() } ==
                    listOf("Ljava/lang/String;", "Ljava/lang/String;", "Ljava/lang/String;")
            ) {
                index to instruction
            } else {
                null
            }
        }

        val (sendMessageIndex, sendMessageInstruction) = unityCalls.singleOrNull()
            ?: error(
                "Google sign-in: expected one UnityPlayer.UnitySendMessage call " +
                    "in onActivityResult"
            )

        val sendMessageRegisters = when (val instruction = sendMessageInstruction) {
            is FiveRegisterInstruction -> listOf(
                instruction.registerC,
                instruction.registerD,
                instruction.registerE,
                instruction.registerF,
                instruction.registerG,
            ).take(instruction.registerCount)

            is RegisterRangeInstruction ->
                (instruction.startRegister until instruction.startRegister + instruction.registerCount)
                    .toList()

            else -> error(
                "Google sign-in: unsupported UnitySendMessage invocation encoding " +
                    "($sendMessageInstruction.opcode)"
            )
        }

        require(sendMessageRegisters.size == 3) {
            "Google sign-in: UnitySendMessage did not have exactly three arguments"
        }

        val gameObjectRegister = sendMessageRegisters[0]
        val callbackMethodRegister = sendMessageRegisters[1]
        val messageRegister = sendMessageRegisters[2]
        val callbackInfoRegister = gameObjectField.second.registerB

        require(gameObjectField.second.registerA == gameObjectRegister) {
            "Google sign-in: mGameObjectName does not feed UnitySendMessage argument 1"
        }
        require(callbackMethodField.second.registerA == callbackMethodRegister) {
            "Google sign-in: mMethodName does not feed UnitySendMessage argument 2"
        }
        require(callbackInfoRegister == callbackMethodField.second.registerB) {
            "Google sign-in: callback fields were not read from the same UnityCallbackInfo"
        }

        val inputRegisterCount = onActivityResult.parameterTypes.size + 1
        val localRegisterCount = implementation.registerCount - inputRegisterCount

        require(callbackInfoRegister in 0 until localRegisterCount) {
            "Google sign-in: no safe local register was available for the callback guard"
        }
        require(callbackInfoRegister < 16 && gameObjectRegister < 16) {
            "Google sign-in: callback registers exceed the safe encoding range"
        }
        require(callbackInfoRegister !in sendMessageRegisters) {
            "Google sign-in: callback scratch register overlaps a UnitySendMessage argument"
        }
        require(messageRegister < 256) {
            "Google sign-in: message register exceeds the const-string encoding range"
        }

        // The UnityCallbackInfo object is dead after its fields are read. Confirm
        // the scratch register is not touched again before UnitySendMessage.
        for (index in callbackMethodField.first + 1 until sendMessageIndex) {
            val instruction = instructions[index]
            val touchesScratch = when (instruction) {
                is FiveRegisterInstruction -> listOf(
                    instruction.registerC,
                    instruction.registerD,
                    instruction.registerE,
                    instruction.registerF,
                    instruction.registerG,
                ).take(instruction.registerCount).contains(callbackInfoRegister)

                is RegisterRangeInstruction ->
                    callbackInfoRegister in
                        instruction.startRegister until
                        instruction.startRegister + instruction.registerCount

                is ThreeRegisterInstruction ->
                    instruction.registerA == callbackInfoRegister ||
                        instruction.registerB == callbackInfoRegister ||
                        instruction.registerC == callbackInfoRegister

                is TwoRegisterInstruction ->
                    instruction.registerA == callbackInfoRegister ||
                        instruction.registerB == callbackInfoRegister

                is OneRegisterInstruction -> instruction.registerA == callbackInfoRegister
                else -> false
            }

            require(!touchesScratch) {
                "Google sign-in: callback-info register is still in use before UnitySendMessage"
            }
        }

        /*
         * For HL.PL only, forward intent.data.toString() (or an empty string
         * when Android supplied no URI). Every other Unity callback continues
         * receiving the original numeric-result string, unchanged.
         */
        onActivityResult.addInstructionsWithLabels(
            sendMessageIndex,
            """
                const-string v$callbackInfoRegister, "HL.PL"
                invoke-virtual {v$gameObjectRegister, v$callbackInfoRegister}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
                move-result v$callbackInfoRegister
                if-eqz v$callbackInfoRegister, :pogo_google_oauth_done
                if-eqz p3, :pogo_google_oauth_empty
                invoke-virtual/range {p3 .. p3}, Landroid/content/Intent;->getData()Landroid/net/Uri;
                move-result-object v$callbackInfoRegister
                if-eqz v$callbackInfoRegister, :pogo_google_oauth_empty
                invoke-virtual/range {v$callbackInfoRegister .. v$callbackInfoRegister}, Landroid/net/Uri;->toString()Ljava/lang/String;
                move-result-object v$callbackInfoRegister
                move-object/16 v$messageRegister, v$callbackInfoRegister
                goto :pogo_google_oauth_done
                :pogo_google_oauth_empty
                const-string v$messageRegister, ""
                :pogo_google_oauth_done
                nop
            """
        )
    }
}

/**
 * Routes the Google OAuth browser callback to Unity and forwards the redirect
 * URI through UnityMainActivity.onActivityResult. This does not bypass OAuth
 * or change Google’s package/signing-certificate validation.
 */
@Suppress("unused")
val pokemonGoGoogleSignInCallback = resourcePatch(
    name = "Google sign-in callback",
    description = "Routes the Google OAuth callback to Unity and forwards its URI for Pokémon GO 0.431.0.",
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_GOOGLE_SIGN_IN)
    dependsOn(googleSignInActivityResultForwarding)

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

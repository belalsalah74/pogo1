group = "app.pokemongo"

patches {
    // TODO: Update this section with your project details.
    about {
        name = "Pokémon GO Runtime Patches"
        description = "Experimental Morphe patches for Pokémon GO 0.431.0 ARM64"
        source = "https://github.com/belalsalah74/pogo1"
        author = "Belal Salah"
        contact = "GitHub issues"
        website = "https://github.com/belalsalah74/pogo1"
        license = "GPLv3"
    }
}

// Separate configuration so gson is available at runtime for the
// generatePatchesList task but never bundled into the APK.
val patchListGeneratorClasspath = configurations.create("patchListGeneratorClasspath")

dependencies {
    compileOnly(libs.gson)
    patchListGeneratorClasspath(libs.gson)
}

tasks {
    register<JavaExec>("generatePatchesList") {
        description = "Build patch with patch list"

        dependsOn(build)

        classpath = sourceSets["main"].runtimeClasspath + patchListGeneratorClasspath
        mainClass.set("util.PatchListGeneratorKt")
    }

    // Used by gradle-semantic-release-plugin.
    publish {
        dependsOn("generatePatchesList")
    }
}

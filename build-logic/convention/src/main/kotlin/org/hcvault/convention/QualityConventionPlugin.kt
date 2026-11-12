package org.hcvault.convention

import io.gitlab.arturbosch.detekt.extensions.DetektExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.configure
import org.gradle.kotlin.dsl.dependencies

class QualityConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) = with(target) {
        pluginManager.apply("io.gitlab.arturbosch.detekt")
        extensions.configure<DetektExtension> {
            buildUponDefaultConfig = true
            allRules = false
            parallel = true
            config.setFrom(rootProject.files("config/detekt/detekt.yml"))
            source.setFrom(files("src/main/kotlin", "src/test/kotlin", "src/androidTest/kotlin"))
        }
        dependencies {
            add("detektPlugins", libs.findLibrary("detekt-formatting").get())
        }
    }
}

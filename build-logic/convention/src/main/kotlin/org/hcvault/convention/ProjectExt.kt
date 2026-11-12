package org.hcvault.convention

import com.android.build.api.dsl.CommonExtension
import org.gradle.api.JavaVersion
import org.gradle.api.Project
import org.gradle.api.artifacts.VersionCatalog
import org.gradle.api.artifacts.VersionCatalogsExtension
import org.gradle.kotlin.dsl.getByType
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.dsl.KotlinAndroidProjectExtension

internal val Project.libs: VersionCatalog
    get() = extensions.getByType<VersionCatalogsExtension>().named("libs")

internal fun VersionCatalog.version(alias: String): String = findVersion(alias).get().requiredVersion

internal fun Project.configureAndroidCommon(extension: CommonExtension<*, *, *, *, *, *>) {
    extension.apply {
        compileSdk = libs.version("compileSdk").toInt()
        ndkVersion = libs.version("ndk")
        defaultConfig {
            minSdk = libs.version("minSdk").toInt()
            testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        }
        compileOptions {
            sourceCompatibility = JavaVersion.VERSION_17
            targetCompatibility = JavaVersion.VERSION_17
        }
        lint {
            warningsAsErrors = true
            abortOnError = true
            checkDependencies = false
            lintConfig = rootProject.file("config/lint.xml")
        }
    }
    extensions.getByType<KotlinAndroidProjectExtension>().apply {
        compilerOptions {
            jvmTarget.set(JvmTarget.JVM_17)
            allWarningsAsErrors.set(true)
        }
    }
}

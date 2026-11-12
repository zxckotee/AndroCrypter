plugins {
    `kotlin-dsl`
}

group = "org.hcvault.buildlogic"

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    compileOnly(libs.android.gradlePlugin)
    compileOnly(libs.kotlin.gradlePlugin)
    compileOnly(libs.kotlin.composeGradlePlugin)
    compileOnly(libs.ksp.gradlePlugin)
    compileOnly(libs.hilt.gradlePlugin)
    compileOnly(libs.detekt.gradlePlugin)
}

gradlePlugin {
    plugins {
        register("androidApplication") {
            id = "hcvault.android.application"
            implementationClass = "org.hcvault.convention.AndroidApplicationConventionPlugin"
        }
        register("androidLibrary") {
            id = "hcvault.android.library"
            implementationClass = "org.hcvault.convention.AndroidLibraryConventionPlugin"
        }
        register("androidCompose") {
            id = "hcvault.android.compose"
            implementationClass = "org.hcvault.convention.AndroidComposeConventionPlugin"
        }
        register("androidHilt") {
            id = "hcvault.android.hilt"
            implementationClass = "org.hcvault.convention.AndroidHiltConventionPlugin"
        }
        register("quality") {
            id = "hcvault.quality"
            implementationClass = "org.hcvault.convention.QualityConventionPlugin"
        }
    }
}

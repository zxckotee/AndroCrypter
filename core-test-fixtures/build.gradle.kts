plugins {
    id("hcvault.android.library")
    alias(libs.plugins.kotlin.serialization)
}

android {
    namespace = "org.hcvault.core.fixtures"
}

dependencies {
    implementation(project(":core-common"))
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.kotlinx.serialization.json)
}

plugins {
    id("hcvault.android.library")
}

android {
    namespace = "org.hcvault.core.mount"
}

dependencies {
    implementation(project(":core-common"))
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.libsu.core)
    implementation(libs.libsu.service)
}

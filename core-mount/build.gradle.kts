plugins {
    id("hcvault.android.library")
    id("hcvault.android.hilt")
}

android {
    namespace = "org.hcvault.core.mount"

    buildFeatures {
        aidl = true
    }
}

dependencies {
    implementation(project(":core-common"))
    implementation(libs.kotlinx.coroutines.core)
    implementation(libs.libsu.core)
    implementation(libs.libsu.service)

    testImplementation(libs.junit)
    testImplementation(libs.kotlinx.coroutines.test)
    testImplementation(libs.turbine)
}

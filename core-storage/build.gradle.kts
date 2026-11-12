plugins {
    id("hcvault.android.library")
    id("hcvault.android.hilt")
}

android {
    namespace = "org.hcvault.core.storage"
}

dependencies {
    implementation(project(":core-common"))
    implementation(project(":core-crypto"))
    implementation(libs.androidx.core.ktx)
    implementation(libs.kotlinx.coroutines.core)
}

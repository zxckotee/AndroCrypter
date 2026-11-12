plugins {
    id("hcvault.android.library")
}

android {
    namespace = "org.hcvault.core.storage"
}

dependencies {
    implementation(project(":core-common"))
    implementation(libs.kotlinx.coroutines.core)
}

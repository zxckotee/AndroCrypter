plugins {
    id("hcvault.android.library")
}

android {
    namespace = "org.hcvault.core.fixtures"
}

dependencies {
    implementation(project(":core-common"))
    implementation(libs.kotlinx.coroutines.core)
}

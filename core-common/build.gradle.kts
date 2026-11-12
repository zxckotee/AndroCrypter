plugins {
    id("hcvault.android.library")
}

android {
    namespace = "org.hcvault.core.common"
}

dependencies {
    implementation(libs.kotlinx.coroutines.core)
    testImplementation(libs.junit)
    testImplementation(libs.kotlinx.coroutines.test)
}

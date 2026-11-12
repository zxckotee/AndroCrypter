plugins {
    id("hcvault.android.library")
    id("hcvault.android.hilt")
}

android {
    namespace = "org.hcvault.core.crypto"

    defaultConfig {
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments("-DANDROID_STL=c++_shared", "-DHCVAULT_BUILD_TESTS=OFF")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = rootProject.file("native/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    implementation(project(":core-common"))
    implementation(libs.kotlinx.coroutines.core)
}

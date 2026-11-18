# JNI bridge (hcvault_jni) — method names must survive R8 for native registration.
-keepclasseswithmembernames class org.hcvault.core.crypto.internal.NativeVolume {
    native <methods>;
}
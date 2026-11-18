# AIDL / Binder IPC consumed by :app when minify is enabled.
-keep interface org.hcvault.core.mount.IMountService { *; }
-keep class org.hcvault.core.mount.IMountService$Stub { *; }
-keep class org.hcvault.core.mount.IMountService$Stub$Proxy { *; }

# libsu RootService subclasses (mount daemon host).
-keep class * extends com.topjohnwu.superuser.ipc.RootService { <init>(); }
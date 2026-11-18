package org.hcvault.core.mount;

/** Root-side mount daemon IPC (skeleton until hcvault-mountd is wired). */
interface IMountService {
    /** Mirrors [MountPhase] ordinal in the app process. */
    int getMountPhase();

    String getActiveMountPoint();

    boolean requestUnmount();

    oneway void ping();
}
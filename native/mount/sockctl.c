#include "mount_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static volatile sig_atomic_t g_sock_serve_stop;

static void on_sigterm(int sig) {
    (void)sig;
    g_sock_serve_stop = 1;
}

struct hm_peer_cred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

static int peer_cred_allowed(int client_fd, uid_t allowed_uid) {
    struct hm_peer_cred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        return 0;
    }
    if (allowed_uid == (uid_t)-1) {
        return 1;
    }
    return cred.uid == allowed_uid || cred.uid == 0;
}

static void handle_client(int client_fd) {
    char buf[512];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        return;
    }
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }

    if (strcmp(buf, "PING") == 0) {
        (void)write(client_fd, "PONG\n", 5);
        return;
    }
    if (strncmp(buf, "CLEANUP", 7) == 0) {
        hm_status st = hm_registry_cleanup_all();
        dprintf(client_fd, st == HM_OK ? "OK\n" : "ERR %s\n", hm_status_str(st));
        return;
    }
    dprintf(client_fd, "ERR unknown\n");
}

hm_status hm_sockctl_serve(const char* socket_path, uid_t allowed_uid) {
    if (socket_path == NULL || socket_path[0] == '\0') {
        return HM_ERR_INVAL;
    }

    int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (srv < 0) {
        return HM_ERR_IO;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(socket_path) >= sizeof(addr.sun_path)) {
        close(srv);
        return HM_ERR_INVAL;
    }
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
    unlink(socket_path);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(srv);
        return HM_ERR_IO;
    }
    if (listen(srv, 8) != 0) {
        close(srv);
        return HM_ERR_IO;
    }

    signal(SIGTERM, on_sigterm);
    signal(SIGINT, on_sigterm);

    while (!g_sock_serve_stop) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        (void)fcntl(client, F_SETFD, FD_CLOEXEC);
        if (!peer_cred_allowed(client, allowed_uid)) {
            dprintf(client, "ERR perm\n");
            close(client);
            continue;
        }
        handle_client(client);
        close(client);
    }

    close(srv);
    unlink(socket_path);
    return HM_OK;
}
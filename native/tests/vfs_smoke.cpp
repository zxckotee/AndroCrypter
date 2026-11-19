#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "hcvault/vfs.h"
#include "hcvault/volume.h"

namespace {

TEST(VfsSmoke, ListRootAndReadSmallFile) {
    const char* password = std::getenv("TEST_VOLUME_PASSWORD");
    ASSERT_NE(password, nullptr);
    ASSERT_GT(std::strlen(password), 0u);

    const char* fixture = std::getenv("HCVAULT_FIXTURE");
    ASSERT_NE(fixture, nullptr);

    char copy_path[] = "/tmp/hcvault-vfs-smoke-XXXXXX.hc";
    int cfd = mkstemps(copy_path, 3);
    ASSERT_GE(cfd, 0);
    int src = open(fixture, O_RDONLY);
    ASSERT_GE(src, 0);
    struct stat stbuf;
    ASSERT_EQ(fstat(src, &stbuf), 0);
    std::vector<uint8_t> blob((size_t)stbuf.st_size);
    ASSERT_EQ(read(src, blob.data(), blob.size()), (ssize_t)blob.size());
    close(src);
    ASSERT_EQ(write(cfd, blob.data(), blob.size()), (ssize_t)blob.size());
    close(cfd);

    int fd = open(copy_path, O_RDWR);
    ASSERT_GE(fd, 0);

    vc_open_params params = {};
    params.password = password;
    params.password_len = std::strlen(password);
    params.pim = 0;
    vc_volume* vol = nullptr;
    ASSERT_EQ(vc_open(fd, &params, &vol), VC_OK);

    vfs_ctx* vfs = nullptr;
    ASSERT_EQ(vfs_open(vol, &vfs), VFS_OK);

    vfs_dirent names[64];
    size_t count = 0;
    ASSERT_EQ(vfs_list_dir(vfs, "/", names, 64, &count), VFS_OK);
    if (count == 0) {
        static const char payload[] = "hcvault-vfs-smoke";
        ASSERT_EQ(vfs_fat_put_root_file(vfs, "SMOKE.TXT", payload, sizeof(payload) - 1u), VFS_OK);
        ASSERT_EQ(vc_flush(vol), VC_OK);
        count = 0;
        ASSERT_EQ(vfs_list_dir(vfs, "/", names, 64, &count), VFS_OK);
    }
    EXPECT_GT(count, 0u);

    for (size_t i = 0; i < count; i++) {
        if (names[i].is_dir || names[i].size == 0) {
            continue;
        }
        char path[32];
        snprintf(path, sizeof(path), "/%s", names[i].name);
        std::vector<uint8_t> buf((size_t)names[i].size + 1u);
        size_t got = 0;
        ASSERT_EQ(vfs_read_file(vfs, path, buf.data(), buf.size(), &got), VFS_OK);
        EXPECT_EQ(got, names[i].size);
        break;
    }

    vfs_close(vfs);
    vc_close(vol);
    close(fd);
    unlink(copy_path);
}

} // namespace
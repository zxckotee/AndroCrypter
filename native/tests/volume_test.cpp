#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "hcvault/volume.h"

namespace {

TEST(VolumeTest, OpenAndReadBootSector) {
    const char* password = std::getenv("TEST_VOLUME_PASSWORD");
    ASSERT_NE(password, nullptr);
    ASSERT_GT(std::strlen(password), 0u);

    const char* fixture = std::getenv("HCVAULT_FIXTURE");
    ASSERT_NE(fixture, nullptr);
    ASSERT_GT(std::strlen(fixture), 0u);

    int fd = open(fixture, O_RDONLY);
    ASSERT_GE(fd, 0);

    vc_open_params params = {
        .password = password,
        .password_len = std::strlen(password),
        .pim = 0,
    };
    vc_volume* vol = nullptr;
    vc_status st = vc_open(fd, &params, &vol);
    ASSERT_EQ(st, VC_OK);
    ASSERT_NE(vol, nullptr);

    uint8_t boot[512];
    st = vc_read(vol, 0, boot, sizeof(boot));
    EXPECT_EQ(st, VC_OK);
    EXPECT_EQ(boot[510], 0x55);
    EXPECT_EQ(boot[511], 0xAA);

    vc_close(vol);
    close(fd);
}

} // namespace

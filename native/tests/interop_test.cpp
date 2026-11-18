// Interop tests: volumes created by the real VeraCrypt CLI must open with the native core.
// Matrix produced by scripts/fixtures/make-oracle-matrix.sh (HCVAULT_ORACLE_DIR).
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "hcvault/volume.h"
}

namespace {

struct Row {
    std::string name, type, cipher, hash;
    int pim = 0;
    std::vector<std::string> keyfiles;
    std::string password;
};

std::string oracleDir() {
    const char* d = std::getenv("HCVAULT_ORACLE_DIR");
    return d ? d : "";
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(s);
    while (std::getline(in, cur, sep)) {
        out.push_back(cur);
    }
    return out;
}

std::vector<Row> loadMatrix() {
    std::vector<Row> rows;
    std::ifstream in(oracleDir() + "/matrix.tsv");
    std::string line;
    while (std::getline(in, line)) {
        auto f = split(line, '\t');
        // keyfiles column can be empty, so std::getline drops it; re-parse by position.
        std::vector<std::string> cols;
        size_t start = 0;
        while (true) {
            size_t p = line.find('\t', start);
            if (p == std::string::npos) {
                cols.push_back(line.substr(start));
                break;
            }
            cols.push_back(line.substr(start, p - start));
            start = p + 1;
        }
        if (cols.size() < 7) {
            continue;
        }
        Row r;
        r.name = cols[0];
        r.type = cols[1];
        r.cipher = cols[2];
        r.hash = cols[3];
        r.pim = std::atoi(cols[4].c_str());
        if (!cols[5].empty()) {
            r.keyfiles = split(cols[5], ',');
        }
        r.password = cols[6];
        rows.push_back(std::move(r));
    }
    return rows;
}

std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

static int hashLabelToPrf(const std::string& hash) {
    const std::string h = lower(hash);
    if (h.find("sha512") != std::string::npos) return VC_PRF_SHA512;
    if (h.find("sha256") != std::string::npos) return VC_PRF_SHA256;
    if (h.find("whirlpool") != std::string::npos) return VC_PRF_WHIRLPOOL;
    if (h.find("blake2") != std::string::npos) return VC_PRF_BLAKE2S;
    if (h.find("streebog") != std::string::npos) return VC_PRF_STREEBOG;
    return VC_PRF_AUTO;
}

struct Opened {
    int fd = -1;
    vc_volume* vol = nullptr;
    ~Opened() {
        if (vol) vc_close(vol);
        if (fd >= 0) close(fd);
    }
};

class Params {
public:
    explicit Params(const Row& r) : password_(r.password) {
        for (const auto& k : r.keyfiles) {
            keyfileData_.push_back(slurp(oracleDir() + "/" + k));
        }
        for (const auto& d : keyfileData_) {
            kfPtrs_.push_back(d.data());
            kfLens_.push_back(d.size());
        }
        p.password = password_.c_str();
        p.password_len = password_.size();
        p.pim = r.pim;
        p.prf = hashLabelToPrf(r.hash);
        p.keyfiles = kfPtrs_.empty() ? nullptr : kfPtrs_.data();
        p.keyfile_lens = kfLens_.empty() ? nullptr : kfLens_.data();
        p.keyfile_count = kfPtrs_.size();
    }
    vc_open_params p{};

private:
    std::string password_;
    std::vector<std::vector<uint8_t>> keyfileData_;
    std::vector<const uint8_t*> kfPtrs_;
    std::vector<size_t> kfLens_;
};

vc_status openPath(const std::string& path, const vc_open_params& p, Opened& o, bool rw = false) {
    o.fd = open(path.c_str(), rw ? O_RDWR : O_RDONLY);
    if (o.fd < 0) return VC_ERR_IO;
    return vc_open(o.fd, &p, &o.vol);
}

std::string copyOf(const std::string& src, const std::string& tag) {
    std::string dst = "/tmp/hcvault_" + tag + "_" + std::to_string(getpid()) + ".hc";
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    out << in.rdbuf();
    return dst;
}

bool looksLikeFat(const std::vector<uint8_t>& s0) {
    return s0.size() >= 512 && s0[510] == 0x55 && s0[511] == 0xAA;
}

class Interop : public ::testing::Test {
protected:
    void SetUp() override {
        if (oracleDir().empty()) {
            GTEST_SKIP() << "SKIP(no-oracle): set HCVAULT_ORACLE_DIR (scripts/fixtures/make-oracle-matrix.sh)";
        }
        rows = loadMatrix();
        ASSERT_FALSE(rows.empty());
    }
    std::vector<Row> rows;
};

} // namespace

TEST_F(Interop, OpensEveryOracleVolumeWithAutodetect) {
    int opened = 0;
    for (const auto& r : rows) {
        Params prm(r);
        prm.p.hidden = r.type == "hidden" ? VC_HEADER_HIDDEN : VC_HEADER_AUTO;
        Opened o;
        vc_status st = openPath(oracleDir() + "/" + r.name + ".hc", prm.p, o);
        ASSERT_EQ(st, VC_OK) << r.name << " " << r.type;

        vc_info info;
        ASSERT_EQ(vc_get_info(o.vol, &info), VC_OK);
        EXPECT_EQ(info.hidden, r.type == "hidden" ? 1 : 0) << r.name;
        const std::string got = lower(info.cipher_name);
        const std::string want = lower(r.cipher);
        EXPECT_TRUE(got == want || got.find(want) != std::string::npos) << got << " vs " << want;
        EXPECT_GT(info.data_size, 0u);
        EXPECT_TRUE(info.readonly == 0);
        if (r.type == "hidden" || r.name.rfind("hidden", 0) != 0) {
            std::vector<uint8_t> s0(512);
            ASSERT_EQ(vc_read(o.vol, 0, s0.data(), s0.size()), VC_OK);
            EXPECT_TRUE(looksLikeFat(s0)) << r.name << " first sector is not a FAT boot sector";
        }
        opened++;
    }
    EXPECT_GE(opened, 15);
}

TEST_F(Interop, AutodetectFindsPrfWithoutHint) {
    for (const auto& r : rows) {
        if (r.type != "normal" || r.name.rfind("m-AES-", 0) != 0) continue;
        Params prm(r);
        prm.p.prf = VC_PRF_AUTO;
        Opened o;
        ASSERT_EQ(openPath(oracleDir() + "/" + r.name + ".hc", prm.p, o), VC_OK) << r.name;
        vc_info info;
        vc_get_info(o.vol, &info);
        EXPECT_EQ(info.prf, hashLabelToPrf(r.hash)) << info.prf_name << " vs " << r.hash;
    }
}

TEST_F(Interop, WrongPasswordPimAndKeyfilesFail) {
    int checked = 0;
    for (const auto& r : rows) {
        if (r.type != "normal") continue;
        if (checked++ >= 6) break;
        const std::string path = oracleDir() + "/" + r.name + ".hc";
        {
            Row bad = r;
            bad.password += "x";
            Params prm(bad);
            Opened o;
            EXPECT_EQ(openPath(path, prm.p, o), VC_ERR_AUTH) << r.name << " wrong password";
        }
        if (r.pim != 0) {
            Row bad = r;
            bad.pim = r.pim + 1;
            Params prm(bad);
            Opened o;
            EXPECT_EQ(openPath(path, prm.p, o), VC_ERR_AUTH) << r.name << " wrong pim";
            Row nopim = r;
            nopim.pim = 0;
            Params prm2(nopim);
            Opened o2;
            EXPECT_EQ(openPath(path, prm2.p, o2), VC_ERR_AUTH) << r.name << " missing pim";
        }
        if (!r.keyfiles.empty()) {
            Row bad = r;
            bad.keyfiles.clear();
            Params prm(bad);
            Opened o;
            EXPECT_EQ(openPath(path, prm.p, o), VC_ERR_AUTH) << r.name << " missing keyfiles";
            Row fewer = r;
            fewer.keyfiles.pop_back();
            if (!fewer.keyfiles.empty()) {
                Params prm2(fewer);
                Opened o2;
                EXPECT_EQ(openPath(path, prm2.p, o2), VC_ERR_AUTH) << r.name << " fewer keyfiles";
            }
        }
    }
}

TEST_F(Interop, HiddenAndOuterCredentialsAreDistinct) {
    const Row *outer = nullptr, *hidden = nullptr;
    for (const auto& r : rows) {
        if (r.name == "hidden-aes" && r.type == "normal") outer = &r;
        if (r.name == "hidden-aes" && r.type == "hidden") hidden = &r;
    }
    ASSERT_TRUE(outer && hidden);
    const std::string path = oracleDir() + "/hidden-aes.hc";

    {
        Params prm(*hidden);
        prm.p.hidden = VC_HEADER_NORMAL;
        Opened o;
        EXPECT_EQ(openPath(path, prm.p, o), VC_ERR_AUTH);
    }
    {
        Params prm(*outer);
        prm.p.hidden = VC_HEADER_HIDDEN;
        Opened o;
        EXPECT_EQ(openPath(path, prm.p, o), VC_ERR_AUTH);
    }
    {
        Params prm(*hidden);
        Opened o;
        ASSERT_EQ(openPath(path, prm.p, o), VC_OK);
        vc_info info;
        vc_get_info(o.vol, &info);
        EXPECT_EQ(info.hidden, 1);
    }
    {
        Params prm(*outer);
        Opened o;
        ASSERT_EQ(openPath(path, prm.p, o), VC_OK);
        vc_info info;
        vc_get_info(o.vol, &info);
        EXPECT_EQ(info.hidden, 0);
    }
}

TEST_F(Interop, WriteReadRoundTripUnalignedAndPersistent) {
    const Row* r = nullptr;
    for (const auto& x : rows) {
        if (x.name == "m-Serpent-sha512") r = &x;
    }
    ASSERT_TRUE(r);
    const std::string copy = copyOf(oracleDir() + "/" + r->name + ".hc", "rw");
    Params prm(*r);

    std::vector<uint8_t> payload(300000);
    for (size_t i = 0; i < payload.size(); i++) payload[i] = (uint8_t)((i * 31 + 7) ^ (i >> 8));
    {
        Opened o;
        ASSERT_EQ(openPath(copy, prm.p, o, true), VC_OK);
        std::vector<uint8_t> before(4096);
        ASSERT_EQ(vc_read(o.vol, 1000, before.data(), before.size()), VC_OK);
        ASSERT_EQ(vc_write(o.vol, 70001, payload.data(), payload.size()), VC_OK);
        ASSERT_EQ(vc_write(o.vol, 5, payload.data(), 3), VC_OK);
        ASSERT_EQ(vc_write(o.vol, 509, payload.data(), 6), VC_OK);
        ASSERT_EQ(vc_flush(o.vol), VC_OK);
        std::vector<uint8_t> after(4096);
        ASSERT_EQ(vc_read(o.vol, 1000, after.data(), after.size()), VC_OK);
        EXPECT_EQ(before, after) << "untouched bytes changed";
    }
    {
        Opened o;
        ASSERT_EQ(openPath(copy, prm.p, o), VC_OK);
        std::vector<uint8_t> got(payload.size());
        ASSERT_EQ(vc_read(o.vol, 70001, got.data(), got.size()), VC_OK);
        EXPECT_EQ(got, payload);
        uint8_t b[6];
        ASSERT_EQ(vc_read(o.vol, 509, b, 6), VC_OK);
        EXPECT_EQ(std::memcmp(b, payload.data(), 6), 0);
    }
    unlink(copy.c_str());
}

TEST_F(Interop, RangeAndReadOnlyEnforced) {
    const Row& r = rows.front();
    const std::string copy = copyOf(oracleDir() + "/" + r.name + ".hc", "ro");
    Params prm(r);
    prm.p.flags = VC_OPEN_READONLY;
    Opened o;
    ASSERT_EQ(openPath(copy, prm.p, o, true), VC_OK);
    uint8_t b[16] = {};
    EXPECT_EQ(vc_write(o.vol, 0, b, sizeof b), VC_ERR_READONLY);
    EXPECT_EQ(vc_is_readonly(o.vol), 1);
    uint64_t size = vc_data_size(o.vol);
    EXPECT_EQ(vc_read(o.vol, size - 8, b, 16), VC_ERR_RANGE);
    EXPECT_EQ(vc_read(o.vol, size, b, 1), VC_ERR_RANGE);
    EXPECT_EQ(vc_read(o.vol, size - 16, b, 16), VC_OK);
    unlink(copy.c_str());
}

TEST_F(Interop, BackupHeaderRecoversDamagedPrimary) {
    const Row& r = rows.front();
    const std::string copy = copyOf(oracleDir() + "/" + r.name + ".hc", "bk");
    {
        int fd = open(copy.c_str(), O_RDWR);
        ASSERT_GE(fd, 0);
        std::vector<uint8_t> zeros(131072, 0xAA);
        ASSERT_EQ(pwrite(fd, zeros.data(), zeros.size(), 0), (ssize_t)zeros.size());
        close(fd);
    }
    Params prm(r);
    {
        Opened o;
        EXPECT_EQ(openPath(copy, prm.p, o), VC_ERR_AUTH);
    }
    prm.p.flags = VC_OPEN_TRY_BACKUP;
    Opened o;
    ASSERT_EQ(openPath(copy, prm.p, o), VC_OK);
    vc_info info;
    vc_get_info(o.vol, &info);
    EXPECT_EQ(info.used_backup_header, 1);
    unlink(copy.c_str());
}

TEST_F(Interop, HiddenVolumeProtectionBlocksWritesIntoHiddenArea) {
    const Row *outer = nullptr, *hidden = nullptr;
    for (const auto& r : rows) {
        if (r.name == "hidden-aes" && r.type == "normal") outer = &r;
        if (r.name == "hidden-aes" && r.type == "hidden") hidden = &r;
    }
    ASSERT_TRUE(outer && hidden);
    const std::string copy = copyOf(oracleDir() + "/hidden-aes.hc", "prot");

    uint64_t hidden_start = 0, hidden_size = 0;
    {
        Params hp(*hidden);
        Opened ho;
        ASSERT_EQ(openPath(copy, hp.p, ho), VC_OK);
        vc_info hi;
        vc_get_info(ho.vol, &hi);
        hidden_start = hi.data_offset;
        hidden_size = hi.data_size;
    }

    Params op(*outer);
    Params pp(*hidden);
    pp.p.hidden = VC_HEADER_HIDDEN;
    op.p.protect_hidden = &pp.p;
    Opened o;
    ASSERT_EQ(openPath(copy, op.p, o, true), VC_OK);
    vc_info oi;
    vc_get_info(o.vol, &oi);
    EXPECT_EQ(oi.hidden_protected, 1);

    std::vector<uint8_t> buf(4096, 0x5A);
    ASSERT_EQ(vc_write(o.vol, 0, buf.data(), buf.size()), VC_OK);
    uint64_t rel = hidden_start - oi.data_offset + 1024;
    ASSERT_LT(rel + buf.size(), oi.data_size);
    EXPECT_EQ(vc_write(o.vol, rel, buf.data(), buf.size()), VC_ERR_PROTECTED);
    EXPECT_EQ(vc_write(o.vol, 0, buf.data(), buf.size()), VC_ERR_READONLY) << "protection trip must lock the volume";
    EXPECT_GT(hidden_size, 0u);

    {
        Params hp(*hidden);
        Opened ho;
        ASSERT_EQ(openPath(copy, hp.p, ho), VC_OK) << "hidden volume survived";
    }
    unlink(copy.c_str());
}

TEST_F(Interop, ConcurrentDisjointWritesAreConsistent) {
    const Row& r = rows.front();
    const std::string copy = copyOf(oracleDir() + "/" + r.name + ".hc", "mt");
    Params prm(r);
    Opened o;
    ASSERT_EQ(openPath(copy, prm.p, o, true), VC_OK);

    constexpr int kThreads = 6;
    constexpr size_t kSpan = 40000;
    std::vector<std::thread> ts;
    std::atomic<int> failures{0};
    for (int t = 0; t < kThreads; t++) {
        ts.emplace_back([&, t]() {
            std::vector<uint8_t> data(kSpan, (uint8_t)(0x10 + t));
            // Adjacent regions share 512-byte units at their boundaries (offset not aligned).
            uint64_t off = 100 + (uint64_t)t * kSpan;
            for (int i = 0; i < 20; i++) {
                if (vc_write(o.vol, off, data.data(), data.size()) != VC_OK) failures++;
            }
        });
    }
    for (auto& t : ts) t.join();
    EXPECT_EQ(failures.load(), 0);
    for (int t = 0; t < kThreads; t++) {
        std::vector<uint8_t> got(kSpan);
        ASSERT_EQ(vc_read(o.vol, 100 + (uint64_t)t * kSpan, got.data(), got.size()), VC_OK);
        EXPECT_TRUE(std::all_of(got.begin(), got.end(), [t](uint8_t b) { return b == 0x10 + t; })) << "region " << t;
    }
    unlink(copy.c_str());
}

TEST(KeyfilePool, PoolIsDeterministicAndOrderIndependent) {
    std::vector<uint8_t> a(1000), b(200);
    for (size_t i = 0; i < a.size(); i++) a[i] = (uint8_t)i;
    for (size_t i = 0; i < b.size(); i++) b[i] = (uint8_t)(255 - i);

    auto run = [&](bool swap) {
        uint8_t text[129] = {};
        std::memcpy(text, "abc", 3);
        size_t len = 3;
        const uint8_t* ptrs[2] = {swap ? b.data() : a.data(), swap ? a.data() : b.data()};
        size_t lens[2] = {swap ? b.size() : a.size(), swap ? a.size() : b.size()};
        EXPECT_EQ(vc_keyfiles_apply(text, &len, sizeof text, ptrs, lens, 2), VC_OK);
        EXPECT_EQ(len, 64u);
        return std::vector<uint8_t>(text, text + len);
    };
    EXPECT_EQ(run(false), run(true));
}

TEST(KeyfilePool, LongPasswordUsesLargePool) {
    uint8_t text[129] = {};
    std::memset(text, 'p', 70);
    size_t len = 70;
    std::vector<uint8_t> kf(32, 7);
    const uint8_t* ptrs[1] = {kf.data()};
    size_t lens[1] = {kf.size()};
    ASSERT_EQ(vc_keyfiles_apply(text, &len, sizeof text, ptrs, lens, 1), VC_OK);
    EXPECT_EQ(len, 128u);
}

TEST(KeyfilePool, EmptyKeyfileRejected) {
    uint8_t text[129] = {};
    size_t len = 0;
    const uint8_t* ptrs[1] = {text};
    size_t lens[1] = {0};
    EXPECT_EQ(vc_keyfiles_apply(text, &len, sizeof text, ptrs, lens, 1), VC_ERR_KEYFILE);
}

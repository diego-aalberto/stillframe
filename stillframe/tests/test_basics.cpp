#include "test.hpp"
#include "../core/json.hpp"
#include "../core/vfs.hpp"
#include "../core/inflate.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace sf;
namespace fs = std::filesystem;

static std::string tmpdir(const char* name) {
    auto p = fs::temp_directory_path() / (std::string("sf_test_") + name);
    fs::remove_all(p);
    fs::create_directories(p);
    return p.string();
}

TEST(json_roundtrip) {
    Json j = Json::parse(R"({"a":1,"b":[true,null,2.5,"x\n\u00e9\ud83d\ude00"],"c":{"d":-3}})");
    CHECK_EQ(j["a"].as_int(), 1);
    CHECK(j["b"].at(0).as_bool());
    CHECK(j["b"].at(1).is_null());
    CHECK_NEAR(j["b"].at(2).as_num(), 2.5, 1e-12);
    CHECK_EQ(j["b"].at(3).as_str(), std::string("x\n\xc3\xa9\xf0\x9f\x98\x80"));
    CHECK_EQ(j["c"]["d"].as_int(), -3);
    CHECK(j["zzz"].is_null());
    Json k = Json::parse(j.dump());
    CHECK(k == j);
    CHECK(Json::parse(j.dump(2)) == j);
    CHECK_EQ(Json(1.0).dump(), std::string("1.0"));
}

TEST(json_errors) {
    CHECK_THROWS(Json::parse("{"));
    CHECK_THROWS(Json::parse("[1,]"));
    CHECK_THROWS(Json::parse("{\"a\":1} x"));
    CHECK_THROWS(Json::parse("\"abc"));
    CHECK_THROWS(Json::parse("01"));
    CHECK_THROWS(Json::parse("\"\\ud800\""));
    CHECK_THROWS(Json::parse(std::string(500, '[')));
}

TEST(json_object_order_and_edit) {
    Json o = Json::object();
    o.set("z", 1); o.set("a", 2); o.set("z", 3);
    CHECK_EQ(o.dump(), std::string("{\"z\":3,\"a\":2}"));
    CHECK(o.erase("z"));
    CHECK_EQ(o.size(), size_t(1));
}

TEST(paths) {
    CHECK_EQ(normalize_path("a/./b//c"), std::string("a/b/c"));
    CHECK_EQ(normalize_path("a/b/../c"), std::string("a/c"));
    CHECK_EQ(normalize_path("a\\b"), std::string("a/b"));
    CHECK_THROWS(normalize_path("../x"));
    CHECK_THROWS(normalize_path("a/../../x"));
    CHECK_EQ(join_path("scripts", "../img/a.png"), std::string("img/a.png"));
    CHECK_EQ(join_path("scripts", "/img/a.png"), std::string("img/a.png"));
}

TEST(inflate_known_vectors) {
    // zlib-compressed "hello hello hello hello\n" (fixed Huffman with a back-reference), made with python zlib.
    const uint8_t z[] = {0x78,0x9c,0xcb,0x48,0xcd,0xc9,0xc9,0x57,0xc8,0x40,0x27,0xb9,0x00,0x70,0xbe,0x08,0xbb};
    Bytes out = zlib_decompress(z, sizeof z);
    CHECK_EQ(to_string(out), std::string("hello hello hello hello\n"));
    CHECK_EQ(crc32(reinterpret_cast<const uint8_t*>("123456789"), 9), 0xCBF43926u);
    CHECK_THROWS(zlib_decompress(z, 8));
}

TEST(zip_roundtrip_and_vfs_parity) {
    std::string d = tmpdir("zip");
    fs::create_directories(d + "/src/sub");
    std::ofstream(d + "/src/a.txt") << "alpha";
    std::ofstream(d + "/src/sub/b.bin", std::ios::binary) << std::string("\0\1\2\3", 4);
    std::ofstream(d + "/src/.hidden") << "x";
    size_t n = pack_directory(d + "/src", d + "/g.stillgame");
    CHECK_EQ(n, size_t(2));
    auto zip = Vfs::open(d + "/g.stillgame");
    auto dir = Vfs::open(d + "/src");
    CHECK(zip->is_archive());
    CHECK(!dir->is_archive());
    CHECK_EQ(zip->read_text("a.txt"), std::string("alpha"));
    CHECK_EQ(zip->read("sub/b.bin").size(), size_t(4));
    CHECK(zip->exists("sub/../a.txt"));
    CHECK(!zip->exists("nope"));
    CHECK_THROWS(zip->read("nope"));
    CHECK_THROWS(zip->exists("../etc/passwd"));
    CHECK_EQ(zip->list_dir("sub").size(), size_t(1));
    CHECK_EQ(dir->read_text("a.txt"), zip->read_text("a.txt"));
}

TEST(zip_reads_deflated_archives_from_other_tools) {
    std::string d = tmpdir("zip2");
    fs::create_directories(d + "/src");
    std::ofstream(d + "/src/big.txt") << std::string(5000, 'q') + "tail";
    std::ofstream(d + "/src/b.txt") << "short";
    int rc = std::system(("cd " + d + "/src && python3 -c \"import zipfile;z=zipfile.ZipFile('../ext.zip','w',zipfile.ZIP_DEFLATED);z.write('big.txt');z.write('b.txt');z.close()\"").c_str());
    if (rc != 0) { std::cerr << "  (python3 unavailable; skipping)\n"; return; }
    auto zip = Vfs::open(d + "/ext.zip");
    CHECK_EQ(zip->read_text("big.txt"), std::string(5000, 'q') + "tail");
    CHECK_EQ(zip->read_text("b.txt"), std::string("short"));
}

TEST(zip_rejects_corruption) {
    std::string d = tmpdir("zip3");
    fs::create_directories(d + "/src");
    std::ofstream(d + "/src/a.txt") << "alpha";
    pack_directory(d + "/src", d + "/g.zip");
    { std::fstream f(d + "/g.zip", std::ios::in | std::ios::out | std::ios::binary); f.seekp(35); f.put('X'); }
    auto zip = Vfs::open(d + "/g.zip");
    CHECK_THROWS(zip->read("a.txt")); // CRC mismatch
    std::ofstream(d + "/junk.zip") << "not a zip at all, definitely not";
    CHECK_THROWS(Vfs::open(d + "/junk.zip"));
}

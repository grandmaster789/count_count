#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>

#include "io/jpg.h"
#include "io/data_location.h"

namespace fs = std::filesystem;

static fs::path get_data_dir() {
    // Walk up from CWD looking for the data folder
    return cc::find_data_folder(fs::current_path());
}

TEST_CASE("load_jpg pixel values match expected BGR", "[pixel_verify]") {
    auto data_dir = get_data_dir();
    auto path = data_dir / "test_gear_001.jpg";

    if (!fs::exists(path)) {
        WARN("Test image not found: " << path.string());
        return;
    }

    auto img = cc::io::load_jpg(path);

    REQUIRE(!img.empty());
    REQUIRE(img.channels() == 3);

    std::cout << "Image: " << img.cols() << "x" << img.rows()
              << " channels=" << img.channels() << std::endl;

    // Dump pixel values at key positions for comparison with Python
    struct SamplePoint { const char* name; int y; int x; };
    SamplePoint points[] = {
        {"top-left",     0,              0},
        {"top-right",    0,              img.cols() - 1},
        {"center",       img.rows() / 2, img.cols() / 2},
        {"bottom-left",  img.rows() - 1, 0},
        {"bottom-right", img.rows() - 1, img.cols() - 1},
    };

    for (const auto& pt : points) {
        const uint8_t* pixel = img.at(pt.y, pt.x);
        std::cout << "  " << pt.name
                  << " (" << pt.x << "," << pt.y << "): "
                  << "BGR=(" << (int)pixel[0] << "," << (int)pixel[1] << "," << (int)pixel[2] << ")"
                  << std::endl;
    }

    // Write raw BGR data to a binary file for Python comparison
    auto dump_path = data_dir / "verify_cpp_raw_bgr.bin";
    {
        std::ofstream out(dump_path, std::ios::binary);
        REQUIRE(out.is_open());
        // Header: width (4 bytes LE), height (4 bytes LE), channels (4 bytes LE)
        int32_t w = img.cols(), h = img.rows(), c = img.channels();
        out.write(reinterpret_cast<const char*>(&w), sizeof(w));
        out.write(reinterpret_cast<const char*>(&h), sizeof(h));
        out.write(reinterpret_cast<const char*>(&c), sizeof(c));
        // Raw pixel data
        out.write(reinterpret_cast<const char*>(img.data()),
                  static_cast<std::streamsize>(img.total_bytes()));
    }

    std::cout << "  Wrote raw BGR dump to: " << dump_path << std::endl;
}

TEST_CASE("round-trip save/load preserves pixel values", "[pixel_verify]") {
    // Use a large image (32x32) so JPEG 8x8 DCT blocks have clean data
    constexpr int sz = 32;
    cc::Image img(sz * 4, sz, 3);

    // Block 0: pure red (BGR: 0, 0, 255)
    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 0; p[1] = 0; p[2] = 255;
        }
    // Block 1: pure green (BGR: 0, 255, 0)
    for (int y = sz; y < sz * 2; ++y)
        for (int x = 0; x < sz; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 0; p[1] = 255; p[2] = 0;
        }
    // Block 2: pure blue (BGR: 255, 0, 0)
    for (int y = sz * 2; y < sz * 3; ++y)
        for (int x = 0; x < sz; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 255; p[1] = 0; p[2] = 0;
        }
    // Block 3: white (BGR: 255, 255, 255)
    for (int y = sz * 3; y < sz * 4; ++y)
        for (int x = 0; x < sz; ++x) {
            uint8_t* p = img.at(y, x);
            p[0] = 255; p[1] = 255; p[2] = 255;
        }

    auto tmp = fs::temp_directory_path() / "cc_roundtrip_test.jpg";
    cc::io::save_jpg(img, tmp);
    auto reloaded = cc::io::load_jpg(tmp);
    fs::remove(tmp);

    REQUIRE(reloaded.rows() == sz * 4);
    REQUIRE(reloaded.cols() == sz);
    REQUIRE(reloaded.channels() == 3);

    // JPEG is lossy but with large uniform blocks, deviation should be small
    constexpr int tol = 10;

    std::cout << "Round-trip pixel check (32x128 image):" << std::endl;
    struct PixelCheck { const char* name; int y; int expected_b; int expected_g; int expected_r; };
    PixelCheck checks[] = {
        {"red",   sz / 2,       0,   0, 255},
        {"green", sz + sz / 2,  0, 255,   0},
        {"blue",  sz * 2 + sz / 2, 255,   0,   0},
        {"white", sz * 3 + sz / 2, 255, 255, 255},
    };

    for (const auto& chk : checks) {
        const uint8_t* p = reloaded.at(chk.y, sz / 2); // center of each block
        int db = std::abs((int)p[0] - chk.expected_b);
        int dg = std::abs((int)p[1] - chk.expected_g);
        int dr = std::abs((int)p[2] - chk.expected_r);

        std::cout << "  " << chk.name << ": BGR=("
                  << (int)p[0] << "," << (int)p[1] << "," << (int)p[2]
                  << ") expected=(" << chk.expected_b << "," << chk.expected_g << "," << chk.expected_r
                  << ") diff=(" << db << "," << dg << "," << dr << ")"
                  << std::endl;

        REQUIRE(db <= tol);
        REQUIRE(dg <= tol);
        REQUIRE(dr <= tol);
    }
}

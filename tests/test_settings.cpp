// tests/test_settings.cpp — Settings serialization, incl. optional camera fields.
#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "types/settings.h"
#include "types/resolution.h"

using namespace cc;

TEST_CASE("Settings - round-trips all fields including exposure/white-balance", "[settings]") {
    Settings out;
    out.m_SelectedCamera           = 1;
    out.m_SourceResolution         = Resolution{1280, 720};
    out.m_ForegroundColor          = {10, 20, 30};
    out.m_ForegroundColorTolerance = 45;
    out.m_FocusValue               = 321;
    out.m_ExposureValue            = -6;     // log-scale exposure can be negative
    out.m_WhiteBalanceValue        = 4200;

    std::stringstream ss;
    ss << out;

    Settings in;
    ss >> in;

    REQUIRE(in.m_SelectedCamera           == 1);
    REQUIRE(in.m_ForegroundColorTolerance == 45);
    REQUIRE(in.m_FocusValue               == 321);
    REQUIRE(in.m_ExposureValue            == -6);
    REQUIRE(in.m_WhiteBalanceValue        == 4200);
}

TEST_CASE("Settings - older config without exposure/WB still loads", "[settings]") {
    // Build a legacy config string: the original five fields, no exposure/WB lines.
    std::stringstream ss;
    ss << 0 << '\n'
       << Resolution{1920, 1080} << '\n'
       << 119 << ' ' << 130 << ' ' << 132 << '\n'
       << 24 << '\n'
       << 200 << '\n';

    Settings s;
    ss >> s;

    REQUIRE(s.m_FocusValue               == 200);
    REQUIRE(s.m_ForegroundColorTolerance == 24);
    // Missing optional fields fall back to the unset sentinel, not garbage.
    REQUIRE(s.m_ExposureValue     == k_CameraValueUnset);
    REQUIRE(s.m_WhiteBalanceValue == k_CameraValueUnset);
}

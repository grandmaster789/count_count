#ifndef CC_TYPES_SETTINGS_H
#define CC_TYPES_SETTINGS_H

#include <cstdint>
#include <iosfwd>
#include <limits>

#include "color.h"
#include "resolution.h"

namespace cc {
    enum e_CameraSelection: int {
        FIRST_CAMERA = 0,
    };

    // Sentinel for camera controls that have not been tuned/locked yet (the
    // converge-then-lock autotune writes the real value). Exposure can be negative
    // on a log scale, so use the smallest int rather than a value like -1.
    inline constexpr int k_CameraValueUnset = std::numeric_limits<int>::min();

    struct Settings {
        int        m_SelectedCamera           = e_CameraSelection::FIRST_CAMERA;
        Resolution m_SourceResolution         = Resolution { 1920, 1080 };
        Color3     m_ForegroundColor          = { 0, 0, 0 };
        int        m_ForegroundColorTolerance = 0;
        int        m_FocusValue               = 200;
        int        m_ExposureValue            = k_CameraValueUnset;
        int        m_WhiteBalanceValue        = k_CameraValueUnset;

        friend std::ostream& operator << (std::ostream& os, const Settings& settings);
        friend std::istream& operator >> (std::istream& is,       Settings& settings);
    };
}

#endif

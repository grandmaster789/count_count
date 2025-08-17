#ifndef CC_TYPES_SETTINGS_H
#define CC_TYPES_SETTINGS_H

#include <cstdint>
#include <iosfwd>

#include "resolution.h"
#include "rgb.h"

namespace cc {
    enum e_CameraSelection: int {
        FIRST_CAMERA = 0,
    };

    struct Settings {
        int        m_SelectedCamera           = e_CameraSelection::FIRST_CAMERA;
        Resolution m_SourceResolution         = Resolution { 1920, 1080 };
        RGB        m_ForegroundColor          = {};
        int        m_ForegroundColorTolerance = 0;

        friend std::ostream& operator << (std::ostream& os, const Settings& settings);
        friend std::istream& operator >> (std::istream& is,       Settings& settings);
    };
}

#endif

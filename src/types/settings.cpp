#include "settings.h"

#include <algorithm>
#include <ostream>
#include <istream>

namespace cc {
    std::ostream& operator << (std::ostream& os, const Settings& s) {
        os
            << s.m_SelectedCamera                       << '\n'
            << s.m_SourceResolution                     << '\n'
            << static_cast<int>(s.m_ForegroundColor[0]) << ' '
            << static_cast<int>(s.m_ForegroundColor[1]) << ' '
            << static_cast<int>(s.m_ForegroundColor[2]) << '\n'
            << s.m_ForegroundColorTolerance             << '\n'
            << s.m_FocusValue                           << '\n';

        return os;
    }

    std::istream& operator >> (std::istream& is, Settings& s) {
        int fg0, fg1, fg2;

        is >> s.m_SelectedCamera;
        is >> s.m_SourceResolution;
        is >> fg0 >> fg1 >> fg2;
        is >> s.m_ForegroundColorTolerance;
        is >> s.m_FocusValue;

        if (is.fail())
            return is;

        // Clamp values to valid ranges
        s.m_SelectedCamera = std::max(0, s.m_SelectedCamera);
        s.m_ForegroundColorTolerance = std::clamp(s.m_ForegroundColorTolerance, 0, 255);
        fg0 = std::clamp(fg0, 0, 255);
        fg1 = std::clamp(fg1, 0, 255);
        fg2 = std::clamp(fg2, 0, 255);

        s.m_ForegroundColor = {
            static_cast<double>(fg0),
            static_cast<double>(fg1),
            static_cast<double>(fg2)
        };

        return is;
    }
}

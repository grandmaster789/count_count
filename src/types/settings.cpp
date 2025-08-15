#include "settings.h"

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
            << s.m_ForegroundColorTolerance             << '\n';

        return os;
    }

    std::istream& operator >> (std::istream& is, Settings& s) {
        int fg0;
        int fg1;
        int fg2;

        is >> s.m_SelectedCamera;
        is >> s.m_SourceResolution;

        is >> fg0;
        is >> fg1;
        is >> fg2;

        is >> s.m_ForegroundColorTolerance;

        s.m_ForegroundColor = {
            static_cast<uint8_t>(fg0),
            static_cast<uint8_t>(fg1),
            static_cast<uint8_t>(fg2)
        };

        return is;
    }
}
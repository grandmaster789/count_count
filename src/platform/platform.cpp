#include "platform.h"
#include <ostream>

namespace cc {
    std::ostream& operator << (std::ostream& os, ePlatform x) {
        switch (x) {
            case ePlatform::windows: os << "Windows"; break;
            case ePlatform::linux:   os << "Linux";   break;

            default:
                os << "Unknown platform";
        }

        return os;
    }
}
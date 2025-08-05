#include "tooth_anomaly.h"
#include <ostream>

namespace cc {
    std::ostream& operator << (
        std::ostream&       os,
        const ToothAnomaly& ta
    ) {
        os << "ToothAnomaly: " <<
            (ta & ToothAnomaly::gap ? "gap " : "") <<
            (ta & ToothAnomaly::arc ? "arc " : "") <<
            (!ta                    ? "none" : "");

        return os;
    }

    std::vector<uint8_t> create_anomaly_mask(size_t num_elements) {
        return std::vector<uint8_t>(num_elements, ToothAnomaly::none);
    }
}
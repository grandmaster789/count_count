#ifndef TOOTH_ANOMALY_H
#define TOOTH_ANOMALY_H

#include <cstdint>
#include <iosfwd>
#include <vector>

namespace cc {
    enum ToothAnomaly:
        uint8_t
    {
        none = 0x0,
        gap  = 0x1 << 1,
        arc  = 0x1 << 2
    };

    std::vector<uint8_t> create_anomaly_mask(size_t num_elements);

    std::ostream& operator << (std::ostream& os, const ToothAnomaly& ta);
}

#endif
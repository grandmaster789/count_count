#ifndef CC_TYPES_TOOTH_MEASUREMENT_H
#define CC_TYPES_TOOTH_MEASUREMENT_H

#include <limits>
#include <iosfwd>

namespace cc {
    struct ToothMeasurement {
        double m_MinDistance =  std::numeric_limits<double>::max();
        double m_MaxDistance = -std::numeric_limits<double>::max();
        double m_StartingAngle;
        double m_EndingAngle;

        size_t m_LowHighTransitionIdx;
        size_t m_HighLowTransitionIdx;

        size_t m_ToothIdx;

        friend std::ostream& operator << (std::ostream& os, const ToothMeasurement& tm);
    };
}

#endif
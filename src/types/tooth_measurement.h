#ifndef CC_TYPES_TOOTH_MEASUREMENT_H
#define CC_TYPES_TOOTH_MEASUREMENT_H

#include <limits>
#include <iosfwd>

namespace cc {
    struct ToothMeasurement {
        double m_MinDistance    =  std::numeric_limits<double>::max();
        double m_MaxDistance    = -std::numeric_limits<double>::max();
        double m_StartingAngle = 0.0;
        double m_EndingAngle   = 0.0;

        size_t m_LowHighTransitionIdx = 0;
        size_t m_HighLowTransitionIdx = 0;

        size_t m_ToothIdx = 0;

        friend std::ostream& operator << (std::ostream& os, const ToothMeasurement& tm);
    };
}

#endif
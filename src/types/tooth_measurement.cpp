#include "tooth_measurement.h"
#include <ostream>

namespace cc {
    std::ostream& operator << (std::ostream& os, const ToothMeasurement& tm) {
        os
            << "ToothMeasurement:"
            << "\n\tMin distance:    " << tm.m_MinDistance
            << "\n\tMax distance:    " << tm.m_MaxDistance
            << "\n\tStarting angle:  " << tm.m_StartingAngle
            << "\n\tEnding angle:    " << tm.m_EndingAngle
            << "\n\tLow -> HighIdx:  " << tm.m_LowHighTransitionIdx
            << "\n\tHigh -> Low Idx: " << tm.m_HighLowTransitionIdx
            << "\n\tTooth index:     " << tm.m_ToothIdx;

        return os;
    }
}
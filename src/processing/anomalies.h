#ifndef CC_PROCESSING_ANOMALIES_H
#define CC_PROCESSING_ANOMALIES_H

#include <vector>
#include <cstdint>

#include "types/tooth_measurement.h"

namespace cc::processing {
    std::vector<uint8_t> find_anomalies(const std::vector<ToothMeasurement>& teeth);
}

#endif
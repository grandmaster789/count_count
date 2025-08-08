#ifndef APP_CAMERA_MANAGER_H
#define APP_CAMERA_MANAGER_H

#include <opencv2/opencv.hpp>
#include "types/resolution.h"
#include "util/flat_map.h"

namespace cc::app {
    class CameraManager {
    public:
        bool initialize(int device_id = 0);

        [[nodiscard]] cv::Mat    grab_frame();

        [[nodiscard]] Resolution get_resolution() const;
        void set_resolution(const Resolution& res); // NOTE this might not actually set the resolution as expected

    private:
        FlatMap<int, Resolution> m_Cameras;
    };
}

#endif

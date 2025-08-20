#ifndef APP_CAMERA_MANAGER_H
#define APP_CAMERA_MANAGER_H

#include <opencv2/opencv.hpp>
#include "types/resolution.h"
#include "util/flat_map.h"

namespace cc::app {
    class CameraManager {
    public:
        bool initialize(int device_id = 0);

        [[nodiscard]] bool is_initialized() const;

        [[nodiscard]] cv::Mat    grab_frame();

        [[nodiscard]] Resolution get_resolution() const;
        [[nodiscard]] bool       set_resolution(const Resolution& res); // NOTE this might not set the resolution as expected; returns false if it didn't work out

    private:
        static bool check_resolution(int camera_id, const Resolution& res);

        int              m_DeviceId = 0;
        cv::VideoCapture m_CaptureSource;
        Resolution       m_Resolution;
    };
}

#endif

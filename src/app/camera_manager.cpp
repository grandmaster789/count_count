#include "camera_manager.h"

namespace cc::app {
    bool CameraManager::initialize(int device_id) {
        (void)device_id;
        return false;
    }

    cv::Mat CameraManager::grab_frame() {
        return {};
    }

    Resolution CameraManager::get_resolution() const {
        return {};
    }

    bool CameraManager::set_resolution(const Resolution& res) {
        (void)res;

        return false;
    }

    bool CameraManager::check_resolution(int camera_id, const Resolution& res) const {
        (void)camera_id;
        (void)res;

        return false;
    }
}
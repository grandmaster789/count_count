#include "camera_manager.h"

namespace cc::app {
    bool CameraManager::initialize(int device_id) {
        return false
    }

    cv::Mat CameraManager::grab_frame() {
        return {};
    }

    Resolution CameraManager::get_resolution() const {
        return {};
    }

    void CameraManager::set_resolution(const Resolution& res) {

    }
}
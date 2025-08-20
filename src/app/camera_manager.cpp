#include "camera_manager.h"

#include "util/logger.h"

namespace cc::app {
    bool CameraManager::initialize(int device_id) {
        m_DeviceId = device_id;

        if (m_CaptureSource.isOpened())
            m_CaptureSource.release();

        m_CaptureSource.open(device_id);

        if (m_CaptureSource.isOpened()) {
            if (m_Resolution.m_Width == 0 || m_Resolution.m_Height == 0) {
                // attempt a couple of default resolutions
                std::vector<cc::Resolution> resolutions = {
                    cc::Resolution { 3840, 2160 }, // 4k
                    cc::Resolution { 1920, 1080},  // 1080p
                    cc::Resolution { 1280, 720 },  // 720p
                    cc::Resolution { 640,  480 }   // 480p
                };

                size_t selected_resolution = 0;

                for (; selected_resolution < resolutions.size(); ++selected_resolution) {
                    if (check_resolution(device_id, resolutions[selected_resolution]))
                        break;
                }

                m_Resolution = resolutions[selected_resolution];
            }

            m_CaptureSource.set(cv::CAP_PROP_FRAME_WIDTH,  m_Resolution.m_Width);
            m_CaptureSource.set(cv::CAP_PROP_FRAME_HEIGHT, m_Resolution.m_Height);

            return true;
        }
        else {
            LOG_ERROR("Failed to open camera");
            return false;
        }
    }

    bool CameraManager::is_initialized() const {
        return m_CaptureSource.isOpened();
    }

    cv::Mat CameraManager::grab_frame() {
        if (!m_CaptureSource.isOpened())
            return {};

        cv::Mat frame;
        m_CaptureSource >> frame;

        return frame;
    }

    Resolution CameraManager::get_resolution() const {
        return m_Resolution;
    }

    bool CameraManager::set_resolution(const Resolution& res) {
        m_Resolution = res;

        if (m_CaptureSource.isOpened())
            return check_resolution(m_DeviceId, res);

        return true;
    }

    bool CameraManager::check_resolution(int camera_id, const Resolution& res) {
        cv::VideoCapture cap(camera_id);
        cap.set(cv::CAP_PROP_FRAME_WIDTH, res.m_Width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.m_Height);

        int actual_width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int actual_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

        return
            (actual_width  == res.m_Width) &&
            (actual_height == res.m_Height);
    }
}
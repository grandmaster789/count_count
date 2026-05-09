#ifndef CC_APP_CAMERA_MANAGER_H
#define CC_APP_CAMERA_MANAGER_H

#include "types/image.h"
#include "types/resolution.h"

#include "platform/platform.h"

#include <mfreadwrite.h>
#include <wrl/client.h>

namespace cc::app {
    class CameraManager {
    public:
        CameraManager();
        ~CameraManager();

        bool initialize(int device_id = 0);

        [[nodiscard]] bool is_initialized() const;

        bool grab_frame(cc::Image& output);

        [[nodiscard]] Resolution get_resolution() const;
        [[nodiscard]] bool       set_resolution(const Resolution& res);

    private:
        bool negotiate_media_type(const Resolution& desired);
        void query_stride();

        int                                          m_DeviceId       = 0;
        bool                                         m_MFInitialized  = false;
        bool                                         m_Initialized    = false;
        Resolution                                   m_Resolution     = { 0, 0 };
        int                                          m_Stride         = 0;
        Microsoft::WRL::ComPtr<IMFSourceReader>      m_SourceReader;
        GUID                                         m_SubType        = {};
    };
}

#endif

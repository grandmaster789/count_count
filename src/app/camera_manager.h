#ifndef CC_APP_CAMERA_MANAGER_H
#define CC_APP_CAMERA_MANAGER_H

#include "types/image.h"
#include "types/resolution.h"

#include "platform/platform.h"

#include <mfreadwrite.h>
#include <strmif.h> // IAMCameraControl, CameraControl_Focus
#include <wrl/client.h>

namespace cc::app {
    class CameraManager {
    public:
        CameraManager();
        ~CameraManager();

        bool initialize(int device_id = 0, int initial_focus = -1);

        [[nodiscard]] bool is_initialized() const;

        bool grab_frame(cc::Image& output);

        [[nodiscard]] Resolution get_resolution() const;
        [[nodiscard]] bool       set_resolution(const Resolution& res);

        bool get_focus_range(long& min, long& max, long& step) const;
        bool get_focus(long& value) const;
        bool set_focus(long value);

    private:
        bool negotiate_media_type(const Resolution& desired);
        void query_stride();

        int                                          m_DeviceId       = 0;
        bool                                         m_MFInitialized  = false;
        bool                                         m_Initialized    = false;
        Resolution                                   m_Resolution     = { 0, 0 };
        int                                          m_Stride         = 0;
        Microsoft::WRL::ComPtr<IMFSourceReader>      m_SourceReader;
        Microsoft::WRL::ComPtr<IAMCameraControl>     m_CameraControl;
        GUID                                         m_SubType        = {};
    };
}

#endif

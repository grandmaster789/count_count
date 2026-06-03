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
        bool set_focus(long value) const;

        // Lock exposure / white-balance / gain to manual for the duration of a focus
        // sweep so the sharpness metric isn't confounded by auto-exposure drift, then
        // restore the prior settings. No-op for controls the camera doesn't expose.
        void begin_focus_sweep();
        void end_focus_sweep();

        // Grab frames, discarding until two consecutive frames are near-identical over
        // the (clamped) ROI bbox — i.e. the lens has settled and the buffered/stale
        // MediaFoundation frames have drained — or max_discard is reached. The latest
        // frame is returned in `output`. Returns false only if a grab fails.
        bool grab_stable_frame(cc::Image& output,
                               int x0, int y0, int x1, int y1,
                               int max_discard = 10, double eps = 3.0);

        // Converge-then-lock exposure: engage the camera's own auto-exposure, settle,
        // read the converged value and lock it to manual (+ gain). Returns the locked
        // value. (White balance does NOT use this: reading WhiteBalance while in Auto
        // returns a stale value on this hardware, so WB uses a metric sweep instead —
        // see Application::auto_white_balance + processing/white_balance.)
        bool autotune_exposure(long& locked_value, int settle_max = 40);

        // White-balance is a scalar (Kelvin) control; these drive a metric sweep.
        bool get_white_balance_range(long& min, long& max, long& step) const;
        bool get_white_balance(long& value) const;
        bool set_white_balance(long value) const;

    private:
        bool negotiate_media_type(const Resolution& desired);
        void query_stride();
        void discard_frames(int n);

        int                                          m_DeviceId       = 0;
        bool                                         m_MFInitialized  = false;
        bool                                         m_Initialized    = false;
        Resolution                                   m_Resolution     = { 0, 0 };
        int                                          m_Stride         = 0;
        Microsoft::WRL::ComPtr<IMFSourceReader>      m_SourceReader;
        Microsoft::WRL::ComPtr<IAMCameraControl>     m_CameraControl;
        Microsoft::WRL::ComPtr<IAMVideoProcAmp>      m_VideoProcAmp;
        GUID                                         m_SubType        = {};

        // Saved auto/manual state restored by end_focus_sweep().
        long m_SavedExposure = 0, m_SavedExposureFlags = 0;
        long m_SavedWB       = 0, m_SavedWBFlags       = 0;
        long m_SavedGain     = 0, m_SavedGainFlags     = 0;
        bool m_HaveSavedExposure = false, m_HaveSavedWB = false, m_HaveSavedGain = false;
    };
}

#endif

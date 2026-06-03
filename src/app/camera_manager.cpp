#include "camera_manager.h"
#include "util/logger.h"

#include <algorithm>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
}

namespace cc::app {

    // MF_SOURCE_READER_FIRST_VIDEO_STREAM is a signed enum value; cast to DWORD for API calls
    static constexpr DWORD k_FirstVideoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

    // Helper: clamp YUV to RGB
    static uint8_t clamp_byte(int v) {
        return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
    }

    // Helper: convert a single YUV pixel to BGR and write to output
    static void yuv_to_bgr_pixel(
        const int y,
        const int u,
        const int v,
        uint8_t* bgr
    ) {
        bgr[0] = clamp_byte(y + ((u * 454) >> 8));
        bgr[1] = clamp_byte(y - ((u * 88 + v * 183) >> 8));
        bgr[2] = clamp_byte(y + ((v * 359) >> 8));
    }

    // Convert NV12 to BGR, writing into an existing output buffer
    static void nv12_to_bgr(
        const uint8_t* data,
        const int width,
        const int height,
        const int stride,
        Image& out
    ) {
        const uint8_t* y_plane  = data;
        const uint8_t* uv_plane = data + stride * height;

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const int y_val = y_plane[row * stride + col];
                const int uv_idx = (row / 2) * stride + (col & ~1);
                const int u_val = uv_plane[uv_idx + 0] - 128;
                const int v_val = uv_plane[uv_idx + 1] - 128;

                yuv_to_bgr_pixel(y_val, u_val, v_val, out.at(row, col));
            }
        }
    }

    // Convert YUY2 to BGR, writing into an existing output buffer
    // YUY2 encodes pixel pairs; width must be even
    static void yuy2_to_bgr(
        const uint8_t* data,
        const int width,
        const int height,
        const int stride,
        Image& out
    ) {
        const int even_width = width & ~1; // round down to even

        for (int row = 0; row < height; ++row) {
            const uint8_t* src_row = data + row * stride;

            for (int col = 0; col < even_width; col += 2) {
                const int idx = col * 2;
                const int y0 = src_row[idx + 0];
                const int u  = src_row[idx + 1] - 128;
                const int y1 = src_row[idx + 2];
                const int v  = src_row[idx + 3] - 128;

                yuv_to_bgr_pixel(y0, u, v, out.at(row, col));
                yuv_to_bgr_pixel(y1, u, v, out.at(row, col + 1));
            }
        }
    }

    // Convert MJPG/RGB24 to BGR, writing into an existing buffer.
    // Positive stride = bottom-up BGR, negative stride = top-down BGR
    static void rgb24_to_bgr(
        const uint8_t* data,
        const int width,
        const int height,
        const int stride,
        Image& out
    ) {
        const int abs_stride = stride < 0 ? -stride : stride;
        const bool bottom_up = stride > 0;

        for (int row = 0; row < height; ++row) {
            const int      src_row_idx = bottom_up ? (height - 1 - row) : row;
            const uint8_t* src_row     = data + static_cast<size_t>(src_row_idx) * abs_stride;
            uint8_t*       dst_row     = out.ptr(row);

            std::memcpy(dst_row, src_row, static_cast<size_t>(width) * 3);
        }
    }

    CameraManager::CameraManager() {
        if (const HRESULT hr = MFStartup(MF_VERSION); FAILED(hr)) {
            LOG_ERROR("MFStartup failed: 0x{:08x}", static_cast<unsigned>(hr));
            return;
        }

        m_MFInitialized = true;
    }

    CameraManager::~CameraManager() {
        m_SourceReader.Reset();
        if (m_MFInitialized) {
            if (const HRESULT hr = MFShutdown(); FAILED(hr)) {
                LOG_ERROR("MFShutdown failed: 0x{:08x}", static_cast<unsigned>(hr));
            }
        }
    }

    void CameraManager::query_stride() {
        if (!m_SourceReader) {
            m_Stride = 0;
            return;
        }

        ComPtr<IMFMediaType> type;
        HRESULT hr = m_SourceReader->GetCurrentMediaType(k_FirstVideoStream, &type);
        if (FAILED(hr)) {
            m_Stride = 0;
            return;
        }

        // Try to get stride from media type attribute
        UINT32 val = 0;
        hr = type->GetUINT32(MF_MT_DEFAULT_STRIDE, &val);
        if (SUCCEEDED(hr)) {
            m_Stride = static_cast<int>(val);
            LOG_INFO("Camera stride (from media type): {}", m_Stride);
            return;
        }

        // Fallback: calculate default stride based on format
        const int w = m_Resolution.m_Width;

        if (m_SubType == MFVideoFormat_NV12)       m_Stride = w;
        else if (m_SubType == MFVideoFormat_YUY2)  m_Stride = w * 2;
        else if (m_SubType == MFVideoFormat_RGB24) m_Stride = w * 3;
        else                                       m_Stride = w;

        LOG_INFO("Camera stride (calculated): {}", m_Stride);
    }

    bool CameraManager::initialize(int device_id, int initial_focus) {
        if (!m_MFInitialized) {
            LOG_ERROR("Cannot initialize camera: MediaFoundation not available");
            return false;
        }

        m_DeviceId = device_id;
        m_SourceReader.Reset();
        m_CameraControl.Reset();
        m_Initialized = false;

        // Enumerate video capture devices
        ComPtr<IMFAttributes> attr;
        HRESULT hr = MFCreateAttributes(&attr, 1);
        if (FAILED(hr)) return false;

        hr = attr->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        );
        if (FAILED(hr)) return false;

        IMFActivate** devices = nullptr;
        UINT32 device_count = 0;
        hr = MFEnumDeviceSources(attr.Get(), &devices, &device_count);

        if (FAILED(hr) || device_count == 0) {
            LOG_ERROR("No video capture devices found");
            return false;
        }

        if (static_cast<UINT32>(device_id) >= device_count) {
            LOG_ERROR("Device ID {} out of range (have {} devices)", device_id, device_count);

            for (UINT32 i = 0; i < device_count; ++i)
                devices[i]->Release();

            CoTaskMemFree(devices);
            return false;
        }

        // Create media source
        ComPtr<IMFMediaSource> media_source;
        hr = devices[device_id]->ActivateObject(
            IID_PPV_ARGS(&media_source)
        );

        for (UINT32 i = 0; i < device_count; ++i)
            devices[i]->Release();
        CoTaskMemFree(devices);

        if (FAILED(hr)) {
            LOG_ERROR("Failed to activate camera device");
            return false;
        }

        // Create source reader
        hr = MFCreateSourceReaderFromMediaSource(
            media_source.Get(),
            nullptr,
            &m_SourceReader
        );
        if (FAILED(hr)) {
            LOG_ERROR("Failed to create source reader");
            return false;
        }

        // Try to set resolution
        if (m_Resolution.m_Width == 0 || m_Resolution.m_Height == 0) {
            const std::vector<Resolution> resolutions = {
                { 3840, 2160 }, // 4k
                { 1920, 1080 }, // 1080p
                { 1280, 720  }, // 720p
                { 640,  480  }  // 480p
            };

            bool found = false;
            for (const auto& res : resolutions) {
                if (negotiate_media_type(res)) {
                    m_Resolution = res;
                    found = true;
                    break;
                }
            }

            if (!found) {
                LOG_WARNING("Could not set preferred resolution, using camera default");
                // Accept whatever the camera gives us
                ComPtr<IMFMediaType> current_type;
                hr = m_SourceReader->GetCurrentMediaType(
                    k_FirstVideoStream,
                    &current_type
                );

                if (SUCCEEDED(hr)) {
                    UINT32 w = 0, h = 0;
                    hr = MFGetAttributeSize(current_type.Get(), MF_MT_FRAME_SIZE, &w, &h);

                    if (SUCCEEDED(hr)) {
                        m_Resolution = {
                            static_cast<int>(w),
                            static_cast<int>(h)
                        };

                        hr = current_type->GetGUID(MF_MT_SUBTYPE, &m_SubType);

                        if (FAILED(hr)) {
                            LOG_ERROR("Failed to get video format subtype");
                            return false;
                        }
                    }
                }
            }
        }
        else {
            if (!negotiate_media_type(m_Resolution)) {
                LOG_WARNING("Could not set resolution {}x{}", m_Resolution.m_Width, m_Resolution.m_Height);
            }
        }

        query_stride();
        m_Initialized = true;
        LOG_INFO("Camera initialized at {}x{}, stride={}", m_Resolution.m_Width, m_Resolution.m_Height, m_Stride);

        // Set focus to manual mode
        // (https://learn.microsoft.com/en-us/windows/win32/api/strmif/nn-strmif-iamcameracontrol)
        hr = media_source->QueryInterface(IID_PPV_ARGS(&m_CameraControl));
        if (SUCCEEDED(hr)) {
            // query the focus range
            long min_focus, max_focus, step, default_value, capabilities;
            hr = m_CameraControl->GetRange(
                CameraControl_Focus,
                &min_focus,
                &max_focus,
                &step,
                &default_value,
                &capabilities
            );

            if (SUCCEEDED(hr)) {
                LOG_INFO("Focus range: min={}, max={}, step={}, default={}, capabilities=0x{:08x}",
                         min_focus, max_focus, step, default_value, capabilities);
            }

            // set manual focus at a specific distance
            // for a Razer Kiyo Pro the range is 0 to 600
            int focus_to_set = (initial_focus >= 0) ? initial_focus : 200;
            hr = m_CameraControl->Set(
                CameraControl_Focus,
                focus_to_set,
                CameraControl_Flags_Manual
            );

            if (SUCCEEDED(hr)) {
                LOG_INFO("Set manual focus to {}", focus_to_set);
            }
        }

        // VideoProcAmp exposes white-balance and gain (exposure lives on
        // IAMCameraControl above). Used to lock auto-adjustments during a focus sweep.
        hr = media_source->QueryInterface(IID_PPV_ARGS(&m_VideoProcAmp));
        if (FAILED(hr))
            LOG_WARNING("IAMVideoProcAmp unavailable; white-balance/gain won't be locked during autofocus");

        return true;
    }

    void CameraManager::begin_focus_sweep() {
        if (m_CameraControl) {
            long val, flags;
            if (SUCCEEDED(m_CameraControl->Get(CameraControl_Exposure, &val, &flags))) {
                m_SavedExposure = val;
                m_SavedExposureFlags = flags;
                m_HaveSavedExposure = true;

                m_CameraControl->Set(CameraControl_Exposure, val, CameraControl_Flags_Manual);
            }
        }

        if (m_VideoProcAmp) {
            long val, flags;

            if (SUCCEEDED(m_VideoProcAmp->Get(VideoProcAmp_WhiteBalance, &val, &flags))) {
                m_SavedWB = val;
                m_SavedWBFlags = flags;
                m_HaveSavedWB = true;

                m_VideoProcAmp->Set(VideoProcAmp_WhiteBalance, val, VideoProcAmp_Flags_Manual);
            }

            if (SUCCEEDED(m_VideoProcAmp->Get(VideoProcAmp_Gain, &val, &flags))) {
                m_SavedGain = val;
                m_SavedGainFlags = flags;
                m_HaveSavedGain = true;

                m_VideoProcAmp->Set(VideoProcAmp_Gain, val, VideoProcAmp_Flags_Manual);
            }
        }
        LOG_INFO("autofocus: locked exposure/WB/gain for the sweep");
    }

    void CameraManager::end_focus_sweep() {
        if (m_CameraControl && m_HaveSavedExposure)
            m_CameraControl->Set(CameraControl_Exposure, m_SavedExposure, m_SavedExposureFlags);
        if (m_VideoProcAmp && m_HaveSavedWB)
            m_VideoProcAmp->Set(VideoProcAmp_WhiteBalance, m_SavedWB, m_SavedWBFlags);
        if (m_VideoProcAmp && m_HaveSavedGain)
            m_VideoProcAmp->Set(VideoProcAmp_Gain, m_SavedGain, m_SavedGainFlags);

        m_HaveSavedExposure = m_HaveSavedWB = m_HaveSavedGain = false;

        LOG_INFO("autofocus: restored exposure/WB/gain");
    }

    bool CameraManager::grab_stable_frame(
        Image& output,
        int x0,
        int y0,
        int x1,
        int y1,
        const int max_discard,
        const double eps
    ) {
        Image prev;

        if (!grab_frame(prev))
            return false;

        // Clamp ROI to the frame.
        auto clamp_roi = [&](const Image& img) {
            x0 = std::max(0, std::min(x0, img.cols() - 1));
            x1 = std::max(0, std::min(x1, img.cols() - 1));
            y0 = std::max(0, std::min(y0, img.rows() - 1));
            y1 = std::max(0, std::min(y1, img.rows() - 1));

            if (x1 < x0)
                std::swap(x0, x1);

            if (y1 < y0)
                std::swap(y0, y1);
        };
        clamp_roi(prev);

        for (int i = 0; i < max_discard; ++i) {
            if (!grab_frame(output)) return false;
            if (output.channels() != 3 || prev.channels() != 3 ||
                output.rows() != prev.rows() || output.cols() != prev.cols()) {
                prev = output.clone();
                continue;
            }

            double sum = 0.0; long n = 0;
            for (int y = y0; y <= y1; ++y) {
                const uint8_t* a = prev.ptr(y);
                const uint8_t* b = output.ptr(y);

                for (int x = x0; x <= x1; ++x) {
                    int idx = x * 3;
                    sum += std::abs(static_cast<int>(a[idx + 0]) - static_cast<int>(b[idx + 0]))
                         + std::abs(static_cast<int>(a[idx + 1]) - static_cast<int>(b[idx + 1]))
                         + std::abs(static_cast<int>(a[idx + 2]) - static_cast<int>(b[idx + 2]));
                    ++n;
                }
            }

            if (const double mean_diff = (n > 0) ? sum / (3.0 * n) : 0.0; mean_diff < eps)
                return true;   // settled

            prev = output.clone();
        }
        return true;  // hit the discard cap; return the freshest frame anyway
    }

    void CameraManager::discard_frames(int n) {
        Image tmp;
        for (int i = 0; i < n; ++i)
            if (!grab_frame(tmp)) return;
    }

    bool CameraManager::autotune_exposure(long& locked_value, int settle_max) {
        if (!m_CameraControl) {
            LOG_WARNING("autotune exposure: camera control unavailable");
            return false;
        }

        long cur, flags;
        if (FAILED(m_CameraControl->Get(CameraControl_Exposure, &cur, &flags))) {
            LOG_WARNING("autotune exposure: cannot read exposure");
            return false;
        }

        // Engage the camera's own auto-exposure. If Auto is unsupported, just lock the
        // current value (still deterministic).
        if (FAILED(m_CameraControl->Set(CameraControl_Exposure, cur, CameraControl_Flags_Auto))) {
            LOG_WARNING("autotune exposure: Auto mode unavailable; locking current value {}", cur);
            m_CameraControl->Set(CameraControl_Exposure, cur, CameraControl_Flags_Manual);
            locked_value = cur;
            return true;
        }
        // The AE algorithm trades off exposure time AND gain, so let gain auto-adjust
        // during convergence too — otherwise we lock exposure but gain keeps drifting
        // brightness frame-to-frame ("exposure locked" must mean "brightness locked").
        long gain_cur = 0, gain_flags = 0;
        bool have_gain = m_VideoProcAmp &&
                         SUCCEEDED(m_VideoProcAmp->Get(VideoProcAmp_Gain, &gain_cur, &gain_flags));
        if (have_gain)
            m_VideoProcAmp->Set(VideoProcAmp_Gain, gain_cur, VideoProcAmp_Flags_Auto);

        // Let auto-exposure engage, then wait for the frame to stop changing.
        discard_frames(5);
        Image tmp;
        grab_stable_frame(tmp, 0, 0, m_Resolution.m_Width - 1, m_Resolution.m_Height - 1,
                          settle_max, 2.5);

        long val, f2;
        if (FAILED(m_CameraControl->Get(CameraControl_Exposure, &val, &f2))) {
            LOG_WARNING("autotune exposure: cannot read converged value");
            return false;
        }
        m_CameraControl->Set(CameraControl_Exposure, val, CameraControl_Flags_Manual);
        locked_value = val;

        // Pin the converged gain too.
        if (have_gain) {
            long g, gf;
            if (SUCCEEDED(m_VideoProcAmp->Get(VideoProcAmp_Gain, &g, &gf))) {
                m_VideoProcAmp->Set(VideoProcAmp_Gain, g, VideoProcAmp_Flags_Manual);
                LOG_INFO("autotune exposure: locked gain = {} (was {})", g, gain_cur);
            }
        }

        // Log pre-Auto vs converged: if these are identical across different lighting,
        // the driver returns a stale value while in Auto and converge-then-lock can't
        // work on this camera (would need a metric sweep instead).
        LOG_INFO("autotune exposure: converged and locked at {} (was {})", val, cur);
        return true;
    }

    void CameraManager::get_white_balance_range(long& min, long& max, long& step) const {
        if (!m_VideoProcAmp) {
            LOG_WARNING("get_white_balance_range: video proc amp unavailable");
            return;
        }

        long def, caps;

        if (FAILED(m_VideoProcAmp->GetRange(VideoProcAmp_WhiteBalance, &min, &max, &step, &def, &caps)))
            LOG_WARNING("get_white_balance_range: failed to read white balance range");
    }

    void CameraManager::get_white_balance(long& value) const {
        if (!m_VideoProcAmp) {
            LOG_WARNING("get_white_balance: video proc amp unavailable");
            return;
        }

        long flags;

        if (FAILED(m_VideoProcAmp->Get(VideoProcAmp_WhiteBalance, &value, &flags)))
            LOG_WARNING("get_white_balance: failed to read white balance value");
    }

    void CameraManager::set_white_balance(long value) const {
        if (!m_VideoProcAmp) {
            LOG_WARNING("set_white_balance: video proc amp unavailable");
            return;
        }

        if (FAILED(m_VideoProcAmp->Set(VideoProcAmp_WhiteBalance, value, VideoProcAmp_Flags_Manual)))
            LOG_WARNING("set_white_balance: failed to set white balance value");
    }

    void CameraManager::get_focus_range(long& min, long& max, long& step) const {
        if (!m_CameraControl) {
            LOG_WARNING("get_focus_range: camera control unavailable");
            return;
        }

        long default_val, caps;

        if (FAILED(m_CameraControl->GetRange(CameraControl_Focus, &min, &max, &step, &default_val, &caps)))
            LOG_WARNING("get_focus_range: failed to read focus range");
    }

    void CameraManager::get_focus(long& value) const {
        if (!m_CameraControl) {
            LOG_WARNING("get_focus: camera control unavailable");
            return;
        }

        long flags = 0;

        if (FAILED(m_CameraControl->Get(CameraControl_Focus, &value, &flags)))
            LOG_WARNING("get_focus: failed to read focus value");
    }

    void CameraManager::set_focus(long value) const {
        if (!m_CameraControl) {
            LOG_WARNING("set_focus: camera control unavailable");
            return;
        }

        if (FAILED(m_CameraControl->Set(CameraControl_Focus, value, CameraControl_Flags_Manual)))
            LOG_WARNING("set_focus: failed to set focus value");
    }

    bool CameraManager::negotiate_media_type(const Resolution& desired) {
        if (!m_SourceReader) return false;

        // Iterate through available media types to find a matching resolution
        for (DWORD i = 0; ; ++i) {
            ComPtr<IMFMediaType> media_type;
            HRESULT hr = m_SourceReader->GetNativeMediaType(
                k_FirstVideoStream,
                i,
                &media_type
            );

            if (FAILED(hr))
                break;

            UINT32 w = 0, h = 0;
            MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &w, &h);

            if (static_cast<int>(w) == desired.m_Width &&
                static_cast<int>(h) == desired.m_Height)
            {
                hr = m_SourceReader->SetCurrentMediaType(
                    k_FirstVideoStream,
                    nullptr,
                    media_type.Get()
                );

                if (SUCCEEDED(hr)) {
                    return SUCCEEDED(media_type->GetGUID(MF_MT_SUBTYPE, &m_SubType));
                }
            }
        }

        return false;
    }

    bool CameraManager::is_initialized() const {
        return m_Initialized;
    }

    bool CameraManager::grab_frame(Image& output) {
        if (!m_Initialized || !m_SourceReader) {
            LOG_WARNING("grab_frame: camera not initialized (initialized={}, reader={})",
                        m_Initialized, m_SourceReader ? "valid" : "null");
            return false;
        }

        ComPtr<IMFSample> sample;
        DWORD stream_index = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;

        // MediaFoundation often returns a null sample with a status flag (STREAMTICK,
        // format change, etc.) on the first few calls. Retry a bounded number of times
        // before giving up so a transient notification doesn't abort the capture loop.
        constexpr int k_MaxReadSampleRetries = 16;
        HRESULT hr = S_OK;

        for (int attempt = 0; attempt < k_MaxReadSampleRetries; ++attempt) {
            sample.Reset();
            flags = 0;
            stream_index = 0;
            timestamp = 0;

            hr = m_SourceReader->ReadSample(
                k_FirstVideoStream,
                0,
                &stream_index,
                &flags,
                &timestamp,
                &sample
            );

            if (attempt > 0)
            LOG_INFO("grab_frame: ReadSample attempt {}: hr=0x{:08x}, flags=0x{:08x}, sample={}",
                     attempt, static_cast<unsigned>(hr), static_cast<unsigned>(flags),
                     sample ? "valid" : "null");

            if (FAILED(hr)) {
                LOG_ERROR("grab_frame: ReadSample failed: hr=0x{:08x}", static_cast<unsigned>(hr));
                return false;
            }

            if (flags & MF_SOURCE_READERF_ERROR) {
                LOG_ERROR("grab_frame: MF_SOURCE_READERF_ERROR set");
                return false;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                LOG_WARNING("grab_frame: end of stream reached");
                return false;
            }
            if (flags & MF_SOURCE_READERF_NEWSTREAM)
                LOG_INFO("grab_frame: new stream started");
            if (flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
                LOG_INFO("grab_frame: native media type changed");
            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
                LOG_INFO("grab_frame: current media type changed — re-querying stride");
                query_stride();
            }
            if (flags & MF_SOURCE_READERF_STREAMTICK)
                LOG_INFO("grab_frame: stream tick (no sample, retrying)");

            if (sample)
                break;
            // null sample with no error/EOS: transient, retry
        }

        if (!sample) {
            LOG_ERROR("grab_frame: no sample after {} retries (last flags=0x{:08x})",
                      k_MaxReadSampleRetries, static_cast<unsigned>(flags));
            return false;
        }

        ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) {
            LOG_ERROR("grab_frame: ConvertToContiguousBuffer failed: hr=0x{:08x}",
                      static_cast<unsigned>(hr));
            return false;
        }

        BYTE* raw_data = nullptr;
        DWORD max_len = 0;
        DWORD cur_len = 0;
        hr = buffer->Lock(&raw_data, &max_len, &cur_len);
        if (FAILED(hr)) {
            LOG_ERROR("grab_frame: buffer Lock failed: hr=0x{:08x}",
                      static_cast<unsigned>(hr));
            return false;
        }

        // RAII guard ensures Unlock even if conversion throws
        struct BufferUnlocker {
            IMFMediaBuffer* buf;
            ~BufferUnlocker() { if (buf) buf->Unlock(); }
        } unlock_guard{buffer.Get()};

        int w = m_Resolution.m_Width;
        int h = m_Resolution.m_Height;

        // Ensure output buffer is allocated at the correct size
        if (output.empty() || output.rows() != h || output.cols() != w)
            output.create(h, w, 3);

        int abs_stride = m_Stride < 0 ? -m_Stride : m_Stride;

        if (m_SubType == MFVideoFormat_NV12) {
            auto expected = static_cast<DWORD>(static_cast<size_t>(abs_stride) * h * 3 / 2);

            if (cur_len < expected) {
                LOG_WARNING("grab_frame: NV12 buffer too small: cur_len={}, expected={}, stride={}, {}x{}",
                            static_cast<unsigned>(cur_len), static_cast<unsigned>(expected),
                            abs_stride, w, h);
                return false;
            }

            nv12_to_bgr(raw_data, w, h, abs_stride, output);
            return true;
        }

        if (m_SubType == MFVideoFormat_YUY2) {
            auto expected = static_cast<DWORD>(static_cast<size_t>(abs_stride) * h);

            if (cur_len < expected) {
                LOG_WARNING("grab_frame: YUY2 buffer too small: cur_len={}, expected={}, stride={}, {}x{}",
                            static_cast<unsigned>(cur_len), static_cast<unsigned>(expected),
                            abs_stride, w, h);
                return false;
            }

            yuy2_to_bgr(raw_data, w, h, abs_stride, output);

            return true;
        }

        if (m_SubType == MFVideoFormat_RGB24) {
            auto expected = static_cast<DWORD>(static_cast<size_t>(abs_stride) * h);

            if (cur_len < expected) {
                LOG_WARNING("grab_frame: RGB24 buffer too small: cur_len={}, expected={}, stride={}, {}x{}",
                            static_cast<unsigned>(cur_len), static_cast<unsigned>(expected),
                            abs_stride, w, h);
                return false;
            }

            rgb24_to_bgr(raw_data, w, h, m_Stride, output);

            return true;
        }

        LOG_WARNING("grab_frame: unsupported pixel format, skipping frame (subtype Data1=0x{:08x})",
                    static_cast<unsigned>(m_SubType.Data1));
        return false;
    }

    Resolution CameraManager::get_resolution() const {
        return m_Resolution;
    }

    bool CameraManager::set_resolution(const Resolution& res) {
        m_Resolution = res;
        return true;
    }
}

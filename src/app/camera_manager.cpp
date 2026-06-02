#include "camera_manager.h"
#include "util/logger.h"

#include <cstring>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <strmif.h> // IAMCameraControl, CameraControl_Focus

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
    static void yuv_to_bgr_pixel(int y, int u, int v, uint8_t* bgr) {
        bgr[0] = clamp_byte(y + ((u * 454) >> 8));
        bgr[1] = clamp_byte(y - ((u * 88 + v * 183) >> 8));
        bgr[2] = clamp_byte(y + ((v * 359) >> 8));
    }

    // Convert NV12 to BGR, writing into an existing output buffer
    static void nv12_to_bgr(const uint8_t* data, int width, int height, int stride, cc::Image& out) {
        const uint8_t* y_plane  = data;
        const uint8_t* uv_plane = data + stride * height;

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                int y_val = y_plane[row * stride + col];
                int uv_idx = (row / 2) * stride + (col & ~1);
                int u_val = uv_plane[uv_idx + 0] - 128;
                int v_val = uv_plane[uv_idx + 1] - 128;

                yuv_to_bgr_pixel(y_val, u_val, v_val, out.at(row, col));
            }
        }
    }

    // Convert YUY2 to BGR, writing into an existing output buffer
    // YUY2 encodes pixel pairs; width must be even
    static void yuy2_to_bgr(const uint8_t* data, int width, int height, int stride, cc::Image& out) {
        int even_width = width & ~1; // round down to even

        for (int row = 0; row < height; ++row) {
            const uint8_t* src_row = data + row * stride;

            for (int col = 0; col < even_width; col += 2) {
                int idx = col * 2;
                int y0 = src_row[idx + 0];
                int u  = src_row[idx + 1] - 128;
                int y1 = src_row[idx + 2];
                int v  = src_row[idx + 3] - 128;

                yuv_to_bgr_pixel(y0, u, v, out.at(row, col));
                yuv_to_bgr_pixel(y1, u, v, out.at(row, col + 1));
            }
        }
    }

    // Convert MJPG/RGB24 to BGR, writing into an existing output buffer
    // Positive stride = bottom-up BGR, negative stride = top-down BGR
    static void rgb24_to_bgr(const uint8_t* data, int width, int height, int stride, cc::Image& out) {
        int abs_stride = stride < 0 ? -stride : stride;
        bool bottom_up = stride > 0;

        for (int row = 0; row < height; ++row) {
            int src_row_idx = bottom_up ? (height - 1 - row) : row;
            const uint8_t* src_row = data + static_cast<size_t>(src_row_idx) * abs_stride;
            uint8_t*       dst_row = out.ptr(row);
            std::memcpy(dst_row, src_row, static_cast<size_t>(width) * 3);
        }
    }

    CameraManager::CameraManager() {
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            LOG_ERROR("MFStartup failed: 0x{:08x}", static_cast<unsigned>(hr));
            return;
        }
        m_MFInitialized = true;
    }

    CameraManager::~CameraManager() {
        m_SourceReader.Reset();
        if (m_MFInitialized)
            MFShutdown();
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
        int w = m_Resolution.m_Width;
        if (m_SubType == MFVideoFormat_NV12)       m_Stride = w;
        else if (m_SubType == MFVideoFormat_YUY2)  m_Stride = w * 2;
        else if (m_SubType == MFVideoFormat_RGB24) m_Stride = w * 3;
        else                                       m_Stride = w;

        LOG_INFO("Camera stride (calculated): {}", m_Stride);
    }

    bool CameraManager::initialize(int device_id) {
        if (!m_MFInitialized) {
            LOG_ERROR("Cannot initialize camera: MediaFoundation not available");
            return false;
        }

        m_DeviceId = device_id;
        m_SourceReader.Reset();
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
            std::vector<cc::Resolution> resolutions = {
                { 3840, 2160 },
                { 1920, 1080 },
                { 1280, 720  },
                { 640,  480  }
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
                    MFGetAttributeSize(current_type.Get(), MF_MT_FRAME_SIZE, &w, &h);
                    m_Resolution = { static_cast<int>(w), static_cast<int>(h) };

                    current_type->GetGUID(MF_MT_SUBTYPE, &m_SubType);
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
        ComPtr<IAMCameraControl> camera_control;
        hr = media_source->QueryInterface(IID_PPV_ARGS(&camera_control));
        if (SUCCEEDED(hr)) {
            // query the focus range
            long min_focus, max_focus, step, default_value, capabilities;
            hr = camera_control->GetRange(
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
            hr = camera_control->Set(
                CameraControl_Focus,
                200,
                CameraControl_Flags_Manual
            );
        }

        return true;
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
                    media_type->GetGUID(MF_MT_SUBTYPE, &m_SubType);
                    return true;
                }
            }
        }

        return false;
    }

    bool CameraManager::is_initialized() const {
        return m_Initialized;
    }

    bool CameraManager::grab_frame(cc::Image& output) {
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

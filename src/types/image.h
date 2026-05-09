#ifndef CC_TYPES_IMAGE_H
#define CC_TYPES_IMAGE_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cassert>

namespace cc {
    class Image {
    public:
        Image() = default;

        Image(int rows, int cols, int channels):
            m_Rows(rows), m_Cols(cols), m_Channels(channels)
        {
            assert(rows >= 0 && cols >= 0 && channels >= 0);
            m_Data.assign(static_cast<size_t>(rows) * cols * channels, 0);
        }

        Image(int rows, int cols, int channels, const uint8_t* data):
            m_Rows(rows), m_Cols(cols), m_Channels(channels)
        {
            assert(rows >= 0 && cols >= 0 && channels >= 0 && data != nullptr);
            auto size = static_cast<size_t>(rows) * cols * channels;
            m_Data.assign(data, data + size);
        }

        [[nodiscard]] bool empty()    const { return m_Data.empty(); }
        [[nodiscard]] int  rows()     const { return m_Rows; }
        [[nodiscard]] int  cols()     const { return m_Cols; }
        [[nodiscard]] int  channels() const { return m_Channels; }

        [[nodiscard]] size_t stride() const { return static_cast<size_t>(m_Cols) * m_Channels; }
        [[nodiscard]] size_t total_bytes() const { return m_Data.size(); }

        [[nodiscard]]       uint8_t* data()       { return m_Data.data(); }
        [[nodiscard]] const uint8_t* data() const { return m_Data.data(); }

        [[nodiscard]]       uint8_t* ptr(int row)       { assert(row >= 0 && row < m_Rows); return m_Data.data() + static_cast<size_t>(row) * stride(); }
        [[nodiscard]] const uint8_t* ptr(int row) const { assert(row >= 0 && row < m_Rows); return m_Data.data() + static_cast<size_t>(row) * stride(); }

        // Access pixel at (y, x) — returns pointer to the first channel of that pixel
        [[nodiscard]]       uint8_t* at(int y, int x)       { assert(y >= 0 && y < m_Rows && x >= 0 && x < m_Cols); return ptr(y) + static_cast<size_t>(x) * m_Channels; }
        [[nodiscard]] const uint8_t* at(int y, int x) const { assert(y >= 0 && y < m_Rows && x >= 0 && x < m_Cols); return ptr(y) + static_cast<size_t>(x) * m_Channels; }

        [[nodiscard]] Image clone() const {
            Image copy;
            copy.m_Rows     = m_Rows;
            copy.m_Cols     = m_Cols;
            copy.m_Channels = m_Channels;
            copy.m_Data     = m_Data;
            return copy;
        }

        void create(int rows, int cols, int channels) {
            assert(rows >= 0 && cols >= 0 && channels >= 0);
            m_Rows     = rows;
            m_Cols     = cols;
            m_Channels = channels;
            m_Data.assign(static_cast<size_t>(rows) * cols * channels, 0);
        }

        void ensure_size(int rows, int cols, int channels) {
            if (empty() || m_Rows != rows || m_Cols != cols || m_Channels != channels)
                create(rows, cols, channels);
        }

        [[nodiscard]] static Image zeros(int rows, int cols, int channels) {
            return Image(rows, cols, channels);
        }

    private:
        int m_Rows     = 0;
        int m_Cols     = 0;
        int m_Channels = 0;
        std::vector<uint8_t> m_Data;
    };
}

#endif

// This was suggested by Claude 4, and is very broken. Still, it's a decent starting point.

#include <emscripten.h>
#include <emscripten/bind.h>

#include <opencv2/opencv.hpp>

#include "processing/foreground.h"
#include "processing/anomalies.h"
#include "processing/contours.h"

#include "gui/visualization.h"

namespace cc::wasm {
    struct ImageProcessingResult {
        int tooth_count;
        int anomaly_count;
        std::string result_message;
        std::vector<uint8_t> output_image_data;
        int image_width;
        int image_height;
    };

    class WasmImageProcessor {
    private:
        cv::Mat m_Foreground;
        cv::Mat m_ForegroundMask;

        // Default settings - could be exposed to JavaScript
        cv::Scalar m_ForegroundColor = cv::Scalar(120, 120, 120);
        int m_ForegroundColorTolerance = 30;

        void initialize_buffers(const cv::Size& size) {
            if (m_Foreground.empty() || m_Foreground.size() != size)
                m_Foreground.create(size, CV_8UC3);

            if (m_ForegroundMask.empty() || m_ForegroundMask.size() != size)
                m_ForegroundMask.create(size, CV_8UC1);
        }

    public:
        ImageProcessingResult process_image(
            const std::vector<uint8_t>& image_data,
            int width,
            int height
        ) {
            ImageProcessingResult result;
            result.tooth_count = 0;
            result.anomaly_count = 0;
            result.result_message = "Processing failed";
            result.image_width = width;
            result.image_height = height;

            try {
                // Convert input data to OpenCV Mat
                cv::Mat input_image(height, width, CV_8UC3, (void*)image_data.data());
                cv::Mat output_image = input_image.clone();

                initialize_buffers(input_image.size());

                // Process the image using your existing algorithms
                cc::processing::determine_foreground(
                    m_ForegroundColor,
                    m_ForegroundColorTolerance,
                    input_image,
                    m_ForegroundMask,
                    m_Foreground
                );

                std::vector<std::vector<cv::Point>> contours;
                std::vector<cv::Vec4i> hierarchy;

                cv::findContours(
                    m_ForegroundMask,
                    contours,
                    hierarchy,
                    cv::RETR_CCOMP,
                    cv::CHAIN_APPROX_SIMPLE
                );

                if (!contours.empty()) {
                    auto maybe_contour_result = cc::processing::process_contours(
                        contours,
                        hierarchy,
                        output_image
                    );

                    if (maybe_contour_result) {
                        auto [teeth, centroid_i] = *maybe_contour_result;

                        if (teeth.size() >= 8) { // k_MinimumToothCount
                            auto tooth_anomaly_mask = cc::processing::find_anomalies(teeth);

                            // Count anomalies
                            for (auto anomaly : tooth_anomaly_mask) {
                                if (anomaly & (cc::ToothAnomaly::gap | cc::ToothAnomaly::arc)) {
                                    result.anomaly_count++;
                                }
                            }

                            // Display results on image
                            cc::display_results(
                                centroid_i,
                                teeth,
                                tooth_anomaly_mask,
                                output_image
                            );

                            result.tooth_count = teeth.size();
                            result.result_message = "Success: Found " + std::to_string(teeth.size()) + " teeth";
                        } else {
                            result.result_message = "Insufficient teeth detected: " + std::to_string(teeth.size());
                        }
                    }
                }

                // Convert output image to vector for JavaScript
                result.output_image_data.resize(output_image.total() * output_image.channels());
                std::memcpy(result.output_image_data.data(), output_image.data, result.output_image_data.size());

            } catch (const std::exception& e) {
                result.result_message = "Error: " + std::string(e.what());
            }

            return result;
        }

        void set_foreground_color(int r, int g, int b) {
            m_ForegroundColor = cv::Scalar(b, g, r); // OpenCV uses BGR
        }

        void set_foreground_tolerance(int tolerance) {
            m_ForegroundColorTolerance = tolerance;
        }
    };
}

// Emscripten bindings to expose C++ functions to JavaScript
using namespace emscripten;

EMSCRIPTEN_BINDINGS(CountVonCount) {
    register_vector<uint8_t>("VectorUint8");

    value_object<cc::wasm::ImageProcessingResult>("ImageProcessingResult")
    .field("tooth_count", &cc::wasm::ImageProcessingResult::tooth_count)
    .field("anomaly_count", &cc::wasm::ImageProcessingResult::anomaly_count)
    .field("result_message", &cc::wasm::ImageProcessingResult::result_message)
    .field("output_image_data", &cc::wasm::ImageProcessingResult::output_image_data)
    .field("image_width", &cc::wasm::ImageProcessingResult::image_width)
    .field("image_height", &cc::wasm::ImageProcessingResult::image_height);

    class_<cc::wasm::WasmImageProcessor>("WasmImageProcessor")
    .constructor<>()
    .function("process_image", &cc::wasm::WasmImageProcessor::process_image)
    .function("set_foreground_color", &cc::wasm::WasmImageProcessor::set_foreground_color)
    .function("set_foreground_tolerance", &cc::wasm::WasmImageProcessor::set_foreground_tolerance);
}

// Traditional C-style export for compatibility
extern "C" {
EMSCRIPTEN_KEEPALIVE
int process_image_data(uint8_t* data, int width, int height) {
    // Simple C interface if needed
    return 0;
}
}
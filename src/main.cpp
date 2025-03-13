#include <iostream>
#include "platform/platform.h"
#include "platform/build_date.h"

#include <opencv2/opencv.hpp>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported (webcam API)"
#endif
/*

  ## Python sketch, should be a fair starting point

import cv2
import numpy as np

# Load the gear image
image = cv2.imread('gear.png')
gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

# Threshold the image
_, thresh = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY_INV | cv2.THRESH_OTSU)

# Find contours
contours, _ = cv2.findContours(thresh, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

# Detect the largest contour (assumed to be the gear)
largest_contour = max(contours, key=cv2.contourArea)

# Approximate the contour for teeth detection
epsilon = 0.01 * cv2.arcLength(largest_contour, True)
approx = cv2.approxPolyDP(largest_contour, epsilon, True)

# Count the number of vertices in approximated contour
teeth_count = len(approx)

print(f"Number of teeth: {teeth_count}")

# Visualize the result
cv2.drawContours(image, [largest_contour], -1, (0, 255, 0), 2)
cv2.imshow('Gear', image)
cv2.waitKey(0)
cv2.destroyAllWindows()
 */
int main() {
    std::cout << "Starting Counting...\n";
    std::cout << "Running on: " << cvc::ePlatform::current << '\n';
    std::cout << "Built " << cvc::get_days_since_build() << " days ago\n";

    {
        cv::VideoCapture webcam(0); // assume we have a webcam...
        if (!webcam.isOpened()) {
            std::cerr << "Cannot open webcam\n";
            return -1;
        }

        double fps = webcam.get(cv::CAP_PROP_FPS);
        std::cout << "fps: " << fps << '\n';

        cv::String main_window = "Webcam Video Stream";
        cv::namedWindow(main_window);

        // press 'q' to terminate the loop
        while (true) {
            cv::Mat webcam_image;

            webcam.read(webcam_image); // returns a flag?

            cv::imshow(main_window, webcam_image);

            if (cv::waitKey(1) == 'q')
                break;
        }
    }

    std::cout << "Completed\n";

    return 0;
}

## Dependencies

The project uses the following dependencies:

- **Stb**: Image loading/saving (stb_image, stb_image_write)
- **Catch2 3.4+**: Unit testing framework
- **Standard Template Library (STL)**: C++ standard library components
- **vcpkg**: Package manager for C++ dependencies

**No OpenCV dependency** — all image processing, drawing, contour detection, and camera capture are implemented from scratch using Win32 APIs and standard C++.

Dependencies are managed using [vcpkg](https://github.com/microsoft/vcpkg).

### System Requirements

- **Platform**: Windows (uses MediaFoundation for camera, Win32 for GUI)
- **Compiler**: C++23 compatible compiler (MSVC recommended)
- **CMake**: 3.15 or higher for building

## Building the Project

1. **Clone the repository**:
   ```bash
   git clone https://github.com/grandmaster789/count_count.git
   cd CountVonCount
   ```

2. **Initialize vcpkg submodule**:
   ```bash
   git submodule update --init --recursive
   ```

3. **Build with CMake**:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

## Usage

1. **Run the application**: Execute the built binary
2. **Select a foreground color**: Either click on the gear surface to pick a color manually, or press **A** to auto-detect the optimal color and tolerance
3. **Adjust the sensitivity**: Use the slider to fine-tune the foreground color detection sensitivity
4. **Review results**: The application will display the tooth count and any detected anomalies

### Keyboard Controls

| Key       | Action                                              |
|-----------|-----------------------------------------------------|
| **A**     | Auto-detect foreground color and tolerance           |
| **L**     | Toggle between live video and static image           |
| **G**     | Save current frame as a timestamped JPG              |
| **Enter** | Cycle between processed image and foreground mask    |
| **Q/Esc** | Quit                                                 |

## Configuration

The application behavior can be customized through the configuration file (`data/count_count.cfg`). Key parameters include:

- Previously selected foreground color
- Foreground detection sensitivity
- Selected webcam identifier
- Webcam resolution

## Algorithm explanation

We start with the assumption that we're looking at a gear which can be distinguished from the background by its color.
This approach is sensitive to lighting conditions, so we provide both manual controls and automatic detection.

### Auto-Sensitivity Detection (press A)

The auto-detection finds the optimal foreground color and tolerance without any manual input:

1. **Dominant color detection** — A 32x32x32 color histogram is built from subsampled pixels. The highest bin is identified as the background and suppressed (±2 bins zeroed). The next highest bin is the gear/foreground color.
2. **Optimal tolerance (Otsu's method)** — The Chebyshev distance from the detected gear color is computed for every subsampled pixel, producing a 256-bin distance histogram. Otsu's method finds the threshold that maximally separates "gear" pixels from "non-gear" pixels.
3. The detected color and tolerance are applied to the existing settings, so the user can further refine them with the slider if needed.

### Processing Pipeline

Processing occurs in several stages:
- First we grab an image from the webcam (or load a static image)
- The image is filtered against the selected foreground color, and a binary mask is generated
- The mask is blurred to reduce the effect of noise in the source image
- We use Moore boundary tracing to find contours in the mask
- The largest contour (by shoelace area) is assumed to be the gear
- We traverse the contour to compute the centroid
- Next we traverse the contour again, collecting distances to the centroid
- The smallest distance to the gear center should be a gap between gear teeth, while the largest would be at the distance of a tooth
- We use the midpoint of the 25th and 75th percentile distances as a threshold (robust to outliers compared to simple min/max midpoint)
- The sequence of distances is compared against the threshold, converting from distances to (binary) teeth and gaps
- To count the number of teeth, we count the number of transitions from 0 (gap) to 1 (tooth), which is a rising edge
- Next we apply statistical anomaly detection to figure out if any teeth are missing
- The assumption here is that most distances between teeth should be incredibly regular
- We determine the mean and standard deviation of the distance between teeth (and the tooth width itself) and use a liberal threshold of 3 standard deviations to identify irregularities
- This will not work if there are too many irregularities, but it should work well enough for most cases
- Finally, we display the gears' contour, the actual tooth count and the assumed total tooth count
- If there are any anomalies detected, the counts will be shown in red, otherwise they will be shown in green

## Screenshots

![screenshot.PNG](screenshot.PNG)

## Testing

The project includes test images for validation:

- `test_gear_001.jpg`, `test_gear_002.jpg`, `test_gear_003.jpg`: Normal gears for testing basic counting functionality
- `test_broken_tooth_001.jpg`, `test_broken_tooth_002.jpg`: Gears with defects for testing anomaly detection

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## References

- [C++ Reference](https://en.cppreference.com/w/)

## License

See the [LICENSE](LICENSE) file for details.

## Dependencies

The project uses the following dependencies:

- **OpenCV 4.10**: Core computer vision library for image processing
- **Stbi**: Image loading library
- **Catch2**: Unit testing framework
- **Standard Template Library (STL)**: C++ standard library components
- **vcpkg**: Package manager for C++ dependencies

Dependencies are managed using [vcpkg](https://github.com/microsoft/vcpkg).

### System Requirements

- **Platform**: Windows (primary), with limited cross-platform support
- **Compiler**: C++17 compatible compiler
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
2. **Click on the gear**: Click on the gear to select a foreground color
3. **Adjust the sensitivity**: Use the slider to adjust the sensitivity of the foreground color detection
4. **Review results**: The application will output tooth count and any detected anomalies

## Configuration

The application behavior can be customized through the configuration file (`data/count_count.cfg`). Key parameters include:

- Previously selected foreground color
- Foreground detection sensitivity
- Selected webcam identifier
- Webcam resolution

## Algorithm explanation

We start with the assumption that we're looking at a gear which can be distinguished from the background by its color.
This approach is sensitive to lighting conditions, so we have added a manual tolerance slider to adjust the sensitivity.

Processing occurs in several stages:
- First we grab an image from the webcam
- The image is filtered against the previously selected foreground color, and a binary mask is generated
- The mask is blurred to reduce the effect of noise in the source image
- We use contour detection to find the gear contour
- Again we have an assumption here that the largest contour is the gear
- We traverse the contour to try and figure out what the center would be
- Next we traverse the contour again, collecting distances to the center
- The smallest distance to the gear center should be a gap between gear teeth, while the largest would be at the distance of a tooth.
- We use the middle between these extremes to determine a threshold
- The sequence of distances is compared against the threshold, converting from distances to (binary) teeth and gaps
- To count the number of teeth, we count the number of transitions from 0 (gap) to 1 (tooth), which is a rising edge
- Next we apply statistical anomaly detection to figure out if any teeth are missing
- The assumption here is that most distances between teeth should be incredibly regular
- We determine the mean and standard deviation of the distance between teeth (and the tooth width itself) and use a liberal threshold of 3 standard deviations to identify irregularities.
- This will not work if there are too many irregularities, but it should work well enough for most cases.
- Finally, we display the gears' contour, the actual tooth count and the assumed total tooth count.
- If there are any anomalies detected, the counts will be shown in red, otherwise they will be shown in green.

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

- [OpenCV Documentation](https://docs.opencv.org/4.x/index.html)
- [C++ Reference](https://en.cppreference.com/w/)

## License

See the [LICENSE](LICENSE) file for details.
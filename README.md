
## Dependencies

The project uses the following major dependencies:

- **OpenCV 4.x**: Core computer vision library for image processing
- **Standard Template Library (STL)**: C++ standard library components
- **vcpkg**: Package manager for C++ dependencies

### System Requirements

- **Platform**: Windows (primary), with limited cross-platform support
- **Compiler**: C++17 compatible compiler
- **CMake**: 3.15 or higher for building

## Building the Project

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
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

1. **Prepare your gear images**: Place gear images in the `data/` directory
2. **Configure parameters**: Adjust settings in `data/count_count.cfg`
3. **Run the application**: Execute the built binary
4. **Review results**: The application will output tooth count and any detected anomalies

## Configuration

The application behavior can be customized through the configuration file (`data/count_count.cfg`). Key parameters include:

- Image processing thresholds
- Contour detection sensitivity
- Tooth counting parameters
- Anomaly detection criteria

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
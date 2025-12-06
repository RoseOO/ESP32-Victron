# Contributing to ESP32-Victron

Thank you for your interest in contributing to ESP32-Victron! This document provides guidelines for contributing to the project.

## How to Contribute

### Reporting Issues

If you find a bug or have a feature request:

1. Check if the issue already exists in [GitHub Issues](https://github.com/RoseOO/ESP32-Victron/issues)
2. If not, create a new issue with:
   - Clear, descriptive title
   - Detailed description of the problem or feature
   - Steps to reproduce (for bugs)
   - Expected vs actual behavior
   - Hardware and software versions
   - Serial monitor output (if relevant)

### Suggesting Enhancements

We welcome suggestions for new features:

- **Display improvements**: Better layouts, additional data fields
- **New device support**: Other Victron devices with BLE
- **Additional features**: Data logging, WiFi connectivity, alarms
- **Performance optimizations**: Battery life, scan efficiency
- **Documentation**: Improvements to guides and examples

## Development Setup

### Prerequisites

- PlatformIO or Arduino IDE
- M5StickC PLUS for testing
- At least one Victron device for testing
- Git for version control

### Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/ESP32-Victron.git
   cd ESP32-Victron
   ```
3. Create a branch for your feature:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Code Style Guidelines

### C++ Code

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Opening brace on same line
- **Naming**:
  - Variables: `camelCase`
  - Constants: `UPPER_CASE` or `kCamelCase`
  - Functions: `camelCase`
  - Classes: `PascalCase`
- **Comments**: Use clear, concise comments for complex logic

Example:
```cpp
// Good
const unsigned long SCAN_INTERVAL = 30000;

void updateDeviceList() {
    // Clear existing addresses
    deviceAddresses.clear();
    
    // Add all device addresses from map
    for (auto& pair : devices) {
        deviceAddresses.push_back(pair.first);
    }
}
```

### Documentation

- Use clear, simple language
- Include code examples where helpful
- Add screenshots for visual features
- Keep README.md up to date

## Testing

Before submitting a pull request:

1. **Compile successfully**:
   ```bash
   pio run
   ```

2. **Test on hardware**:
   - Upload to M5StickC PLUS
   - Verify with at least one Victron device
   - Test all modified functionality

3. **Check for regressions**:
   - Ensure existing features still work
   - Test button functionality
   - Verify display updates correctly

4. **Test edge cases**:
   - No devices found
   - Multiple devices
   - Poor signal strength
   - Device out of range

## Pull Request Process

1. **Update documentation**:
   - README.md if behavior changes
   - Code comments for complex logic
   - CHANGELOG.md with your changes

2. **Commit messages**:
   - Use clear, descriptive messages
   - Start with a verb: "Add", "Fix", "Update", "Remove"
   - Reference issue numbers: "Fix #123: Description"

   Example:
   ```
   Add support for SmartSolar 150/35 MPPT controller
   
   - Parse additional record types for MPPT
   - Update device identification logic
   - Add MPPT-specific display fields
   
   Closes #45
   ```

3. **Create pull request**:
   - Provide clear description of changes
   - Reference related issues
   - Include screenshots for visual changes
   - List testing performed

4. **Code review**:
   - Respond to feedback promptly
   - Make requested changes
   - Keep discussion focused and professional

## Areas for Contribution

### High Priority

- [ ] Support for additional Victron devices (Inverters, etc.)
- [ ] Battery life optimizations
- [ ] Better error handling and user feedback
- [ ] WiFi/MQTT data publishing

### Medium Priority

- [ ] SD card data logging
- [ ] Configuration menu on device
- [ ] Historical data graphs
- [ ] Multiple language support

### Low Priority

- [ ] Custom display themes
- [ ] OTA (Over-The-Air) updates
- [ ] Web interface for configuration

## Victron Protocol

When adding support for new devices or record types:

1. Refer to [docs/VICTRON_BLE_PROTOCOL.md](docs/VICTRON_BLE_PROTOCOL.md)
2. Document any new record types discovered
3. Provide sample BLE advertisement data
4. Test with actual hardware

### Adding New Record Types

```cpp
// In VictronBLE.h
enum VictronRecordType {
    // ... existing types ...
    NEW_RECORD_TYPE = 0x11,  // Document what this is
};

// In VictronBLE.cpp parseVictronAdvertisement()
case NEW_RECORD_TYPE:
    device.newField = decodeValue(recordData, recordLen, scale);
    break;
```

## Display Modifications

When modifying the display:

1. Consider the 240x135 pixel screen size
2. Test with long device names
3. Ensure readability (font size, contrast)
4. Update display layout diagram in README

## Code Review Checklist

Before submitting, verify:

- [ ] Code compiles without warnings
- [ ] Tested on actual hardware
- [ ] No memory leaks
- [ ] Appropriate bounds checking
- [ ] Documentation updated
- [ ] Follows code style guidelines
- [ ] Commit messages are clear
- [ ] No debug code left in

## Security Considerations

- Don't commit sensitive data (WiFi passwords, etc.)
- Validate all external input (BLE data)
- Use safe string operations
- Implement proper bounds checking
- Review dependencies for vulnerabilities

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

## Getting Help

- 💬 **Discussions**: Ask questions in GitHub Discussions
- 📧 **Contact**: Open an issue for specific problems
- 📖 **Docs**: Read existing documentation thoroughly

## Recognition

Contributors will be recognized in:
- README.md contributors section
- Release notes
- Commit history

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Welcome newcomers and help them learn
- Accept constructive criticism gracefully
- Focus on what's best for the project

### Unacceptable Behavior

- Harassment or discrimination
- Trolling or insulting comments
- Publishing others' private information
- Other unprofessional conduct

## Questions?

Don't hesitate to ask! Open an issue or discussion if you're unsure about anything.

Thank you for contributing to ESP32-Victron! 🙏

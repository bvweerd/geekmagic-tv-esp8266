# Contributing to SmartClock

## Development Setup

### Requirements
- PlatformIO Core or IDE
- ESP8266 hardware
- ST7789V display

### Getting Started
```bash
git clone <repo>
cd smartclock-arduino
pio run
```

## Code Style

### C/C++
- Indentation: 4 spaces
- Braces: K&R style
- Naming:
  - Functions: camelCase
  - Variables: camelCase
  - Constants: UPPER_CASE
  - Structs: PascalCase

### Comments
- Single line: `//`
- Function docs: Brief description above function
- Complex logic: Inline comments

### Example
```cpp
// Set display brightness
void displaySetBrightness(int brightness) {
    brightness = constrain(brightness, 0, 100);
    int pwmValue = map(brightness, 0, 100, 0, 1023);
    analogWrite(PIN_BACKLIGHT, 1023 - pwmValue);  // Inverted
}
```

## File Organization

### Headers (.h)
- Include guards
- Forward declarations
- Extern declarations
- Function prototypes

### Implementation (.cpp)
- Include header first
- Static helpers if needed
- Public functions

## Adding Features

### New Display Mode
1. Add state to `DisplayState` struct in `display.h`
2. Implement render function in `display.cpp`
3. Add control endpoint in `webserver.cpp`
4. Update API.md documentation

### New API Endpoint
1. Add handler in `webserver.cpp`
2. Update `webserverInit()`
3. Document in API.md
4. Add test in test-api.sh

### New Setting
1. Add field to `Settings` struct in `settings.h`
2. Update `settingsLoad()` defaults
3. Update `settingsSave()`
4. Add persistence in webserver handlers

## Testing

### Before Commit
```bash
# Build
pio run

# Upload to test device
pio run -t upload

# Monitor output
pio device monitor

# Test API
./test-api.sh smartclock.local
```

### Testing Checklist
- [ ] Compiles without warnings
- [ ] Uploads successfully
- [ ] Display shows correctly
- [ ] WiFi connects
- [ ] All API endpoints work
- [ ] Settings persist after reboot
- [ ] OTA update works
- [ ] No memory leaks (check free heap)

## Pull Requests

### Process
1. Fork repository
2. Create feature branch: `git checkout -b feature/my-feature`
3. Make changes
4. Test thoroughly
5. Commit: `git commit -m "feat: add feature"`
6. Push: `git push origin feature/my-feature`
7. Open PR

### PR Description
- What: Brief summary
- Why: Motivation
- How: Implementation notes
- Testing: What you tested

### Commit Messages
Format: `type: description`

Types:
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `refactor:` Code refactoring
- `perf:` Performance improvement
- `test:` Add tests
- `chore:` Maintenance

Examples:
```
feat: add MQTT support
fix: display flicker on image load
docs: update API.md with new endpoint
refactor: extract scroll logic to function
```

## Bug Reports

### Template
```
**Description:**
Brief description of the bug

**Steps to Reproduce:**
1. Step 1
2. Step 2
3. ...

**Expected Behavior:**
What should happen

**Actual Behavior:**
What actually happens

**Environment:**
- Hardware: ESP8266 NodeMCU v2
- Display: ST7789V 240x240
- Firmware version: 1.0.0
- PlatformIO version: X.Y.Z

**Logs:**
```
Serial output or error messages
```

**Additional Context:**
Any other relevant information
```

## Feature Requests

### Template
```
**Feature:**
Brief description

**Use Case:**
Why is this useful?

**Proposed Solution:**
How could it work?

**Alternatives:**
Other approaches considered

**Additional Context:**
Mockups, examples, etc.
```

## Documentation

### When to Update
- API changes → API.md
- New features → README.md
- Architecture changes → ARCHITECTURE.md
- Setup changes → INSTALL.md

### Style
- Clear, concise language
- Code examples for complex topics
- Tables for reference data
- Diagrams for architecture

## Memory Constraints

ESP8266 has limited RAM (~80KB). When adding features:

### Guidelines
- Avoid large global arrays
- Use `const` for read-only data (stored in flash)
- Prefer streaming over buffering
- Use `F()` macro for string literals
- Check free heap: `ESP.getFreeHeap()`

### Example
```cpp
// Bad: wastes 1KB RAM
char buffer[1024];

// Good: uses stack or flash
const char* msg PROGMEM = "Long message...";
```

## Performance

### Display Updates
- Keep render functions fast (<50ms)
- Batch TFT operations
- Avoid unnecessary clears

### Network
- Use async operations
- Handle errors gracefully
- Add timeouts

## Dependencies

### Adding Library
1. Add to `lib_deps` in platformio.ini
2. Document version and purpose
3. Update ARCHITECTURE.md

### Updating Library
1. Test thoroughly
2. Document breaking changes
3. Update CHANGELOG.md

## Release Process

### Versioning
Semantic versioning: MAJOR.MINOR.PATCH

- MAJOR: Breaking changes
- MINOR: New features (backward compatible)
- PATCH: Bug fixes

### Steps
1. Update version in code
2. Update CHANGELOG.md
3. Test on hardware
4. Tag release: `git tag v1.0.0`
5. Push: `git push --tags`
6. Create GitHub release

## Questions?

- Check existing issues
- Read documentation
- Ask in discussions

## Code of Conduct

- Be respectful
- Be constructive
- Help others
- Follow guidelines

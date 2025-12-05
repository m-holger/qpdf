# Copilot Coding Agent Instructions for qpdf

## Repository Summary
qpdf is a command-line tool and C++ library that performs content-preserving transformations on PDF files. It supports linearization, encryption, page splitting/merging, and PDF file inspection. Version: 12.3.0.

**Project Type:** C++ library and CLI tool (C++20 standard)  
**Build System:** CMake 3.16+ with Ninja generator  
**External Dependencies:** zlib, libjpeg, OpenSSL, GnuTLS (crypto providers)

## Build Instructions

### Quick Build (Recommended)
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake ninja-build zlib1g-dev libjpeg-dev libgnutls28-dev libssl-dev

# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure
```

### Using CMake Presets (Maintainer Mode)
```bash
cmake --preset maintainer          # Configure
cmake --build --preset maintainer  # Build
ctest --preset maintainer          # Test
```

Available presets: `maintainer`, `maintainer-debug`, `maintainer-coverage`, `maintainer-profile`, `debug`, `release`, `sanitizers`, `msvc`, `msvc-release`. Use `cmake --list-presets` to see all options.

### Build Notes
- **Always build out-of-source** in a subdirectory (e.g., `build/`). In-source builds are explicitly blocked.
- Build time: approximately 2-3 minutes on typical CI runners.
- Test suite time: approximately 1 minute for all 7 test groups.
- The `MAINTAINER_MODE` cmake option enables stricter checks and auto-generation of job files.

## Running Tests
```bash
cd build

# Run all tests
ctest --output-on-failure

# Run specific test groups
ctest -R qpdf        # Main qpdf CLI tests (~43 seconds)
ctest -R libtests    # Library unit tests (~8 seconds)
ctest -R examples    # Example code tests
ctest -R fuzz        # Fuzzer tests

# Run with verbose output
ctest --verbose
```

**Test Framework:** Tests use `qtest` (a Perl-based test framework). Tests are invoked via `ctest` and compare outputs against expected files. Test coverage uses `QTC::TC` macros.

## Code Formatting
```bash
./format-code   # Formats all C/C++ files with clang-format
```
- Requires **clang-format version 20 or higher**.
- Configuration: `.clang-format` in the repository root.
- Always run before committing changes to C/C++ files.

## Project Layout

### Key Directories
| Directory | Purpose |
|-----------|---------|
| `libqpdf/` | Core library implementation (*.cc files) |
| `include/qpdf/` | Public headers (QPDF.hh, QPDFObjectHandle.hh, QPDFWriter.hh) |
| `qpdf/` | CLI executable and main test driver |
| `libtests/` | Library unit tests |
| `examples/` | Example programs demonstrating API usage |
| `fuzz/` | Fuzzer test programs for oss-fuzz |
| `manual/` | Documentation (reStructuredText for Sphinx) |
| `build-scripts/` | CI and build automation scripts |

### Important Files
| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Main build configuration |
| `CMakePresets.json` | Predefined build configurations |
| `job.yml` | Command-line argument definitions (auto-generates code) |
| `generate_auto_job` | Python script that generates argument parsing code |
| `.clang-format` | Code formatting rules |
| `README-maintainer.md` | Detailed maintainer and coding guidelines |

### Auto-Generated Files
When modifying `job.yml` or CLI options, regenerate with:
```bash
./generate_auto_job --generate
# Or build with: cmake -DGENERATE_AUTO_JOB=ON
```

## CI Workflows (.github/workflows/)

### main.yml (Primary CI)
- **Prebuild**: Documentation and external libs preparation
- **Linux**: Full build and test with image comparison
- **Windows**: MSVC and MinGW builds (32/64-bit)
- **macOS**: macOS build
- **AppImage**: Linux AppImage generation
- **Sanitizers**: AddressSanitizer and UndefinedBehaviorSanitizer tests
- **CodeCov**: Coverage reporting
- **pikepdf**: Compatibility testing with pikepdf Python library

## Coding Conventions

### Must Follow
1. **Assertions**: Test code should include `qpdf/assert_test.h` first. Debug code should include `qpdf/assert_debug.h` and use `qpdf_assert_debug` instead of `assert`. Use `qpdf_expect`, `qpdf_ensures`, `qpdf_invariant` for pre/post-conditions. Never use raw `assert()`. The `check-assert` test enforces this.
2. **Use `QIntC` for type conversions** - Required for safe integer casting.
3. **Avoid `operator[]`** - Use `.at()` for std::string and std::vector (see README-hardening.md).
4. **Include order**: Include the class's own header first, then a blank line, then other includes.
5. **Use `std::to_string`** instead of QUtil::int_to_string.

### New Code Style (See `libqpdf/qpdf/AcroForm.hh` FormNode class for examples)
1. **PIMPL Pattern**: New public classes should use the PIMPL (Pointer to Implementation) pattern with a full implementation class. See `QPDFAcroFormDocumentHelper::Members` as an example.
2. **Avoid `this->`**: Do not use `this->` and remove it when updating existing code.
3. **QTC::TC Calls**: Remove simple `QTC::TC` calls (those with 2 parameters) unless they are the only executable statement in a branch.
   - When removing a `QTC::TC` call:
      - Use the first parameter to find the corresponding `.testcov` file.
      - Remove the line in the `.testcov` (or related coverage file) that includes the second parameter.
4. **Doxygen Comments**: Use `///` style comments with appropriate tags (`@brief`, `@param`, `@return`, `@tparam`, `@since`).
   ```cpp
   /// @brief Retrieves the field value.
   ///
   /// @param inherit If true, traverse parent hierarchy.
   /// @return The field value or empty string if not found.
   std::string value() const;
   ```
5. **Member Variables**: Use trailing underscores for member variables (e.g., `cache_valid_`, `fields_`).
6. **Naming Conventions**:
   - Use `snake_case` for new function and variable names (e.g., `fully_qualified_name()`, `root_field()`).
   - **Exception**: PDF dictionary entry accessors and variables use the exact capitalization from the PDF spec (e.g., `FT()`, `TU()`, `DV()` for `/FT`, `/TU`, `/DV`).
7. **Getters/Setters**: Simple getters/setters use the attribute name without "get" or "set" prefixes:
   ```cpp
   String TU() const { return {get("/TU")}; }
   ```
   Note: Names like `setFieldAttribute()` are legacy naming; new code should use `snake_case` (e.g., `set_field_attribute()`).

### Style
- Column limit: 100 characters
- Braces on their own lines for classes/functions
- Use `// line-break` comment to prevent clang-format from joining lines
- Use `// clang-format off/on` for blocks that shouldn't be formatted

## Adding Command-Line Arguments
1. Add option to `job.yml` (top half for CLI, bottom half for JSON schema)
2. Add documentation in `manual/cli.rst` with `.. qpdf:option::` directive
3. Implement the Config method in `libqpdf/QPDFJob_config.cc`
4. Build with `-DGENERATE_AUTO_JOB=1` or run `./generate_auto_job --generate`

## Validation Checklist
Before submitting changes:
- [ ] `cmake --build build` succeeds without warnings (WERROR is ON in maintainer mode)
- [ ] `ctest --output-on-failure` - all tests pass
- [ ] `./format-code` - code is properly formatted
- [ ] `./spell-check` - no spelling errors (requires cspell: `npm install -g cspell`)

## Troubleshooting

### Common Issues
1. **"clang-format version >= 20 is required"**: The `format-code` script automatically tries `clang-format-20` if available. Install clang-format 20 or newer via your package manager.
2. **Build fails in source directory**: Always use out-of-source builds (`cmake -B build`).
3. **Tests fail with file comparison errors**: May be due to zlib version differences. Use `qpdf-test-compare` for comparisons.
4. **generate_auto_job errors**: Ensure Python 3 and PyYAML are installed.

### Environment Variables for Extended Tests
- `QPDF_TEST_COMPARE_IMAGES=1`: Enable image comparison tests
- `QPDF_LARGE_FILE_TEST_PATH=/path`: Enable large file tests (needs 11GB free)

## Trust These Instructions
These instructions have been validated against the actual repository. Only search for additional information if:
- Instructions appear outdated or incomplete
- Build commands fail unexpectedly
- Test patterns don't match current code structure

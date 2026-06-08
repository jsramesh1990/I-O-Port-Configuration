
### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(jetson-orin-nano-cpp VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Check for required libraries
pkg_check_modules(GPIOD REQUIRED libgpiod)
pkg_check_modules(I2CDEV REQUIRED i2cdev)

# Library sources
set(LIBRARY_SOURCES
    src/gpio.cpp
    src/uart.cpp
    src/rs485.cpp
    src/spi.cpp
    src/i2c.cpp
    src/pwm.cpp
    src/ethernet.cpp
    src/usb.cpp
    src/pcie.cpp
    src/can.cpp
    src/camera.cpp
    src/m2.cpp
)

# Header files
set(LIBRARY_HEADERS
    include/gpio.hpp
    include/uart.hpp
    include/rs485.hpp
    include/spi.hpp
    include/i2c.hpp
    include/pwm.hpp
    include/ethernet.hpp
    include/usb.hpp
    include/pcie.hpp
    include/can.hpp
    include/camera.hpp
    include/m2.hpp
)

# Create library
add_library(jetson_peripheral ${LIBRARY_SOURCES})
target_include_directories(jetson_peripheral PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(jetson_peripheral
    ${GPIOD_LIBRARIES}
    ${I2CDEV_LIBRARIES}
    pthread
    rt
)

# Compiler flags
target_compile_options(jetson_peripheral PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O2
)

# Installation
install(TARGETS jetson_peripheral
    EXPORT jetson_peripheralTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LIBRARY_HEADERS} DESTINATION include/jetson_peripheral)

# Export package
install(EXPORT jetson_peripheralTargets
    FILE jetson_peripheralTargets.cmake
    NAMESPACE jetson_peripheral::
    DESTINATION lib/cmake/jetson_peripheral
)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation
find_program(DOXYGEN_EXECUTABLE doxygen)
if(DOXYGEN_EXECUTABLE)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation"
    )
endif()

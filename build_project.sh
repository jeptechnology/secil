#!/bin/bash

# This script builds the project using CMake and then runs the tests.
# Ensure the build directory is clean
echo "Building core library and tests..."
rm -rf build
cmake -G "Ninja" -B build -S .
cmake --build build 
cmake --install build

# Run the tests
echo "Running tests..."
./build/loopback_test
if [ $? -ne 0 ]; then
    echo "Loopback test failed."
    exit 1
fi

# Now check that our installed library can be built from source
echo "Building installation from source..."
cmake -G "Ninja" -B build/test -S build/install
cmake --build build/test
if [ $? -ne 0 ]; then
    echo "Installation build failed."
    exit 1
fi

# Create a zip of the install directory
echo "Creating zip of the install directory..."
cd build/install
zip -r secil_install.zip *
if [ $? -ne 0 ]; then
    echo "Failed to create zip of the install directory."
    exit 1
fi


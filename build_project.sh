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

# Create a tarball of the install directory
echo "Creating tarball of the install directory..."
tar -czf libsecil.tgz build/install
echo "Build and tests completed successfully. Tarball created: libsecil.tgz"

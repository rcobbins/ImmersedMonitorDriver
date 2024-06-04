# Immersed Monitor Driver

## Overview

The Immersed Monitor Driver is designed to disable and interfere with secondary physical monitors when the Immersed agent is running. It provides a mechanism to control the behavior of secondary monitors based on the state of the Immersed agent.

## Prerequisites

Before working with the Immersed Monitor Driver, ensure that you have the following prerequisites set up:

1. **Windows Driver Kit (WDK):**
   - Download and install the latest version of the [Windows Driver Kit](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).
   
2. **Development Environment:**
   - Set up a development environment with the necessary tools for building and testing Windows drivers. This may include Visual Studio with the WDK integrated or command-line tools provided by the WDK.

## Setting up Development Environment

To set up your development environment for building the driver:

1. **Install WDK:**
   - Download and install the Windows Driver Kit from the provided link.

2. **Verify WDK Installation:**
   - Ensure that the WDK tools such as `build.exe`, `devcon.exe`, and `inf2cat.exe` are accessible from your command-line environment.

3. **Configure Visual Studio (if applicable):**
   - If you're using Visual Studio, configure it to recognize the installed WDK. Set up paths for include files, libraries, and tools within Visual Studio's project settings.

4. **Set Up Build Configurations:**
   - Configure build configurations to target the correct platform and architecture (e.g., x86, x64, ARM) and select the appropriate build configuration (e.g., Debug, Release).

5. **Configure Driver Signing:**
   - Configure driver signing options to allow test-signed or self-signed drivers to be loaded. Obtain test signing certificates and configure driver signing policies as needed.

6. **Verify Dependencies:**
   - Ensure that all necessary dependencies, such as required libraries and header files, are properly set up and accessible from your build environment.

7. **Test Build Process:**
   - Test the build process to ensure that the driver project can be compiled without errors or warnings. Resolve any build issues promptly.

## Running Test-Signed Drivers

To install and run test-signed drivers:

1. **Prepare Driver Package:**
   - Build the driver project to generate the driver package (.sys file).

2. **Sign Driver Package:**
   - Sign the driver package using tools like `inf2cat.exe` and `signtool.exe` to generate a test-signed driver package.

3. **Install Driver:**
   - Install the test-signed driver package on your test machine using tools like `devcon.exe` or by manually installing through Device Manager.

4. **Verify Installation:**
   - Verify that the driver is installed correctly and functioning as expected.

## Usage

Once the driver is successfully built, signed, and installed, it will automatically intercept and control secondary monitor behavior based on the state of the Immersed agent.

## Contributing

Contributions to the Immersed Monitor Driver project are welcome. Feel free to submit issues, feature requests, or pull requests through the project's [GitHub repository](https://github.com/example/project).

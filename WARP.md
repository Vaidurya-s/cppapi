# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

This is the **C++ implementation for STTP (Streaming Telemetry Transport Protocol)**, which implements IEEE 2664-2024. STTP is a protocol designed for high-performance streaming of telemetry data, commonly used in power grid and industrial applications.

The codebase supports both **subscriber** (data consumer) and **publisher** (data producer) functionality.

## Development Commands

### Build System
The project uses **CMake** with Make as the build system.

**Initial Configuration:**
```bash
# Configure from source directory
cd /home/vaidurya/sttp/cppapi/src
cmake .

# Or configure from separate build directory
mkdir build && cd build
cmake path/to/source
```

**Debug Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wno-unknown-pragmas"
```

**Build Commands:**
```bash
# Build main library
make -j6

# Build all samples (includes library dependency)
make -j6 samples

# Build individual sample applications
make SimpleSubscribe
make SimplePublish
make AdvancedSubscribe
make AdvancedPublish
make FilterExpressionTests
make InteropTest
make LatencyTest
make ReverseSubscribe
make ReversePublish
make InstanceSubscribe
make InstancePublish
make DynamicMetadataPublish
make AverageFrequencyCalculator
```

**Installation:**
```bash
make install
```

### Testing
The project includes several test/sample applications that serve as both examples and functional tests:

- **FilterExpressionTests**: Tests filter expression parsing and evaluation
- **InteropTest**: Tests interoperability between different STTP implementations
- **LatencyTest**: Performance testing for measuring latency

## Architecture Overview

### Core Transport Classes
The main STTP functionality is in the `src/lib/transport/` directory:

- **DataPublisher**: Core class for publishing telemetry data streams
- **SubscriberConnection**: Manages individual subscriber connections on publisher side
- **SubscriberConnector**: Client-side connector for subscribing to data streams
- **PublisherInstance**: Manages publisher instance lifecycle

### Key Data Structures
- **CompactMeasurement**: Efficient measurement data structure
- **SignalIndexCache**: Optimized signal lookup and caching
- **MetadataSchema**: Handles metadata definitions and schemas
- **RoutingTables**: Manages data routing between publishers and subscribers

### Compression and Encoding
- **TSSC (Time Series Special Compression)**: Located in `src/lib/transport/tssc/`
  - **TSSCEncoder**: Compresses time-series data for transmission
  - **TSSCDecoder**: Decompresses received time-series data

### Filter Expressions
- **ExpressionTree**: Located in `src/lib/filterexpressions/`
- **FilterExpressionParser**: Parses and evaluates filter expressions for selective data subscription

### Utility Components
- **ThreadPool**: Asynchronous task execution
- **ThreadSafeQueue**: Thread-safe data structures
- **Timer**: High-precision timing functionality
- **DataSet/DataTable/DataRow**: ADO.NET-style data containers

## Dependencies

### Required System Libraries
- **CMake v2.8+**
- **GCC v10.2+** (for C++20 support)
- **GNU Make**
- **zlib1g-dev**: `sudo apt install zlib1g-dev`
- **libbz2-dev**: `sudo apt install libbz2-dev`

### Boost Libraries
The project requires **Boost v1.80.0** with these specific modules:
- Boost.Asio (networking)
- Boost.Bind (function binding)
- Boost.Iostreams (with zlib support)
- Boost.System (system utilities)
- Boost.Thread (threading)
- Boost.Uuid (UUID generation)

**Boost Installation Commands:**
```bash
# Install Boost dependencies
sudo apt install zlib1g-dev libbz2-dev

# Download and build Boost (adjust paths as needed)
cd /usr/local/
sudo wget https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.bz2
sudo tar -xvjf boost_1_80_0.tar.bz2
cd boost_1_80_0
sudo ./bootstrap.sh
sudo ./b2 install
sudo ldconfig /usr/local/lib
```

## Sample Applications

### Basic Examples
- **SimpleSubscribe**: Basic data subscription example
- **SimplePublish**: Basic data publishing example

### Advanced Examples  
- **AdvancedSubscribe**: Advanced subscription with filtering and metadata
- **ReverseSubscribe**: Reverse connection subscription pattern
- **DynamicMetadataPublish**: Publishing with dynamic metadata updates

### Specialized Examples
- **AverageFrequencyCalculator**: Calculates average frequency from incoming measurements
- **LatencyTest**: Measures end-to-end latency between publisher and subscriber

## Working with the Codebase

### Code Organization
- **src/lib/**: Core STTP library implementation
- **src/samples/**: Example applications and tests  
- **doc/**: Documentation (including FilterExpressions.md)

### Key Namespaces
- `sttp::transport`: Core transport functionality
- `sttp::filterexpressions`: Filter expression parsing
- `sttp::data`: Data container classes

### Publisher/Subscriber Pattern
The codebase follows a publisher/subscriber architecture where:
- **Publishers** stream telemetry data to multiple subscribers
- **Subscribers** can filter data using expression trees
- **Connections** are managed asynchronously with Boost.Asio
- **Compression** is handled transparently via TSSC encoding

### Threading Model
- Uses Boost.Thread for multi-threading
- ThreadPool class manages asynchronous operations
- Thread-safe queues handle data passing between threads
- Subscribers and publishers operate on separate I/O contexts

## Platform Support

### Windows (Visual Studio)
- Requires Visual Studio 2022
- Boost libraries should be placed in parallel `../boost` directory
- Solution file: `src/sttp.cpp.sln`

### Linux/Unix
- Uses CMake build system as documented above
- Tested with GCC 10.2+ for C++20 support
- Requires manual Boost compilation with zlib support
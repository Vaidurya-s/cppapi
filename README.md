# STTP C++ API

[![CodeQL](https://github.com/sttp/cppapi/actions/workflows/codeql.yml/badge.svg)](https://github.com/sttp/cppapi/actions/workflows/codeql.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![IEEE 2664-2024](https://img.shields.io/badge/IEEE-2664--2024-blue.svg)](https://standards.ieee.org/project/2664.html)

**C++ implementation of the Streaming Telemetry Transport Protocol (STTP)** -- a high-performance, publish-subscribe protocol for real-time streaming telemetry data, standardized as [IEEE 2664-2024](https://standards.ieee.org/project/2664.html).

STTP is optimized for synchrophasor (PMU), SCADA, and IoT data streams where low latency, high throughput, and bandwidth efficiency are critical.

---

## Architecture Overview

```mermaid
graph TB
    subgraph Publisher["DataPublisher"]
        Sources["Data Sources<br/>(PMU, SCADA, IoT)"]
        --> Meta["Metadata Store<br/>(DataSet)"]
        Sources --> RT["RoutingTables"]
        RT --> SC1["SubscriberConnection 1<br/>(TSSC Encoder)"]
        RT --> SC2["SubscriberConnection 2<br/>(TSSC Encoder)"]
        RT --> SCN["SubscriberConnection N<br/>(TSSC Encoder)"]
    end

    SC1 -->|"TCP or UDP"| S1["Subscriber A<br/>(Voltage Phasors)"]
    SC2 -->|"TCP or UDP"| S2["Subscriber B<br/>(Frequency Only)"]
    SCN -->|"TCP or UDP"| SN["Subscriber C<br/>(All Signals)"]

    S1 & S2 & SN -->|"TCP Command Channel"| Publisher

    style Publisher fill:#2d6a4f,color:#fff
    style S1 fill:#1b4332,color:#fff
    style S2 fill:#1b4332,color:#fff
    style SN fill:#1b4332,color:#fff
```

Each subscriber specifies a **filter expression** to select exactly which signals to receive. The publisher evaluates the filter, builds a signal index cache, and streams only the requested measurements using TSSC compression.

---

## Key Features

| Feature | Description | Learn More |
|---------|-------------|------------|
| **Publisher-Subscriber Model** | Push-based one-to-many data distribution with per-subscriber signal filtering | [Architecture](doc/Architecture.md) |
| **TSSC Compression** | Time-Series Special Compression achieves 10:1+ compression with delta encoding and adaptive coding | [TSSC Deep-Dive](doc/TSSC.md) |
| **Dual TCP/UDP Channels** | TCP for reliable commands, optional UDP for low-latency data streaming | [Protocol Reference](doc/Protocol.md) |
| **Filter Expressions** | SQL-like WHERE clauses for server-side signal selection | [Filter Expressions](doc/FilterExpressions.md) |
| **Reverse Subscribe** | Subscriber listens, publisher connects -- firewall-friendly topology | [Connection Modes](doc/Architecture.md#connection-modes) |
| **Temporal Subscriptions** | Replay historical data with time-based queries and playback control | [Temporal Subscriptions](doc/Protocol.md#temporal-subscriptions) |
| **Cipher Key Rotation** | Dual-key encryption with seamless rotation, no data interruption | [Security Model](doc/Architecture.md#security-model) |
| **Auto-Reconnect** | Exponential backoff reconnection with configurable retry limits | [Auto-Reconnect](doc/Architecture.md#connection-modes) |
| **Rich Metadata** | XML-serialized DataSet with device, measurement, and phasor metadata | [Metadata Exchange](doc/Protocol.md#metadata-exchange) |
| **Quality Flags** | 32-bit measurement state flags for data quality, time quality, and alarms | [Quality Flags](doc/Architecture.md#measurement-quality-flags) |
| **Multi-Threaded** | Dedicated threads for command, data, routing, and callbacks | [Threading Model](doc/Architecture.md#threading-model) |
| **Cross-Platform** | Windows (Visual Studio 2022) and Linux/Unix (CMake + gcc 10.2+) | [Build Instructions](src/README.md) |

---

## STTP vs Traditional Protocols

```mermaid
graph LR
    subgraph "Traditional SCADA (Poll-Based)"
        Master["SCADA Master"]
        -->|"1. Poll request"| RTU1["RTU / Device"]
        RTU1 -->|"2. Response"| Master
        Master -->|"3. Poll request"| RTU2["RTU / Device"]
        RTU2 -->|"4. Response"| Master
    end

    subgraph "STTP (Push-Based)"
        Pub["Publisher"]
        -->|"Continuous stream"| SubA["Subscriber A"]
        Pub -->|"Continuous stream"| SubB["Subscriber B"]
        Pub -->|"Continuous stream"| SubC["Subscriber C"]
    end

    style Master fill:#e76f51,color:#fff
    style Pub fill:#2a9d8f,color:#fff
```

| Aspect | Traditional (Poll) | STTP (Push) |
|--------|-------------------|-------------|
| **Latency** | High (poll interval) | Low (immediate push) |
| **Bandwidth** | Inefficient (full payload each poll) | Efficient (TSSC delta compression) |
| **Scalability** | O(N) polls per device | O(1) publish, fan-out to N subscribers |
| **Data Selection** | All-or-nothing | Per-subscriber filter expressions |
| **Historical Replay** | Separate historian query | Built-in temporal subscriptions |
| **Connection Resilience** | Manual reconnect | Auto-reconnect with backoff |
| **Compression** | Typically none | TSSC: 10:1+ for time-series data |
| **Security** | Varies | TLS + rotating cipher keys |

---

## Module Architecture

```mermaid
graph TD
    App["Application Code"]
    App --> PI["PublisherInstance /<br/>SubscriberInstance"]
    PI --> DP["DataPublisher /<br/>DataSubscriber"]
    DP --> Transport["Transport Layer<br/>(TCP/UDP, Boost.Asio)"]
    DP --> TSSC["TSSC Codec<br/>(Encoder / Decoder)"]
    DP --> SIC["SignalIndexCache<br/>(ID mapping)"]
    DP --> Data["DataSet<br/>(Metadata)"]
    Data --> FE["FilterExpressionParser<br/>(ANTLR4)"]
    Data --> XML["pugixml<br/>(XML serialization)"]
    Transport --> Boost["Boost Libraries<br/>(Asio, Thread, System,<br/>Iostreams, Uuid)"]

    style App fill:#264653,color:#fff
    style PI fill:#2a9d8f,color:#fff
    style DP fill:#e9c46a,color:#000
    style Transport fill:#f4a261,color:#000
    style TSSC fill:#e76f51,color:#fff
    style Boost fill:#e76f51,color:#fff
```

---

## Repository Structure

```mermaid
graph TD
    Root["cppapi/"]
    Root --> Doc["doc/<br/>Architecture, Protocol,<br/>TSSC, FilterExpressions,<br/>Samples"]
    Root --> Src["src/"]

    Src --> Inc["Include/sttp/lib/<br/>(public headers)"]
    Src --> Lib["lib/<br/>(implementation)"]
    Src --> Samp["samples/<br/>(13 sample apps)"]

    Inc --> IncT["transport/<br/>DataPublisher, DataSubscriber,<br/>SubscriberConnection, Constants"]
    Inc --> IncTSSC["transport/tssc/<br/>TSSCEncoder, TSSCDecoder"]
    Inc --> IncD["data/<br/>DataSet, DataTable"]
    Inc --> IncF["filterexpressions/<br/>FilterExpressionParser"]

    Lib --> LibT["transport/"]
    Lib --> LibTSSC["transport/tssc/"]
    Lib --> LibD["data/"]
    Lib --> LibF["filterexpressions/<br/>(ANTLR4 runtime)"]

    style Root fill:#264653,color:#fff
    style Doc fill:#2a9d8f,color:#fff
    style Inc fill:#e9c46a,color:#000
    style Lib fill:#f4a261,color:#000
    style Samp fill:#e76f51,color:#fff
```

---

## Codebase Composition

```mermaid
pie title "Source Code Distribution"
    "Transport & Protocol" : 45
    "Filter Expressions (ANTLR4)" : 25
    "Data (DataSet)" : 10
    "TSSC Compression" : 8
    "Utilities & Threading" : 7
    "Samples" : 5
```

---

## Quick Start

### Minimal Subscriber (High-Level API)

```cpp
#include "sttp/transport/SubscriberInstance.h"

class MySubscriber : public sttp::transport::SubscriberInstance
{
protected:
    void ReceivedNewMeasurements(const std::vector<sttp::transport::MeasurementPtr>& measurements) override
    {
        for (const auto& m : measurements)
            std::cout << m->SignalID << " = " << m->Value << std::endl;
    }

    void StatusMessage(const std::string& message) override
    {
        std::cout << message << std::endl;
    }

    void ErrorMessage(const std::string& message) override
    {
        std::cerr << message << std::endl;
    }
};

int main()
{
    MySubscriber subscriber;
    subscriber.Connect("localhost", 7165);
    subscriber.SetFilterExpression("FILTER ActiveMeasurements WHERE SignalType = 'FREQ'");
    subscriber.Subscribe();

    // Run until interrupted...
    std::string line;
    std::getline(std::cin, line);

    subscriber.Disconnect();
    return 0;
}
```

---

## Sample Applications

| Sample | Role | Description |
|--------|------|-------------|
| [SimplePublish](src/samples/SimplePublish) | Publisher | Basic measurement publishing with XML metadata |
| [SimpleSubscribe](src/samples/SimpleSubscribe) | Subscriber | Basic subscription and data reception |
| [InstancePublish](src/samples/InstancePublish) | Publisher | High-level API with virtual method overrides |
| [InstanceSubscribe](src/samples/InstanceSubscribe) | Subscriber | High-level API with virtual method overrides |
| [AdvancedPublish](src/samples/AdvancedPublish) | Publisher | Dynamic metadata, temporal subscription support |
| [AdvancedSubscribe](src/samples/AdvancedSubscribe) | Subscriber | Auto-reconnect, metadata parsing, signal display |
| [ReversePublish](src/samples/ReversePublish) | Publisher | Publisher connects to subscriber (reverse mode) |
| [ReverseSubscribe](src/samples/ReverseSubscribe) | Subscriber | Subscriber listens for publisher connections |
| [DynamicMetadataPublish](src/samples/DynamicMetadataPublish) | Publisher | Runtime metadata creation and updates |
| [LatencyTest](src/samples/LatencyTest) | Subscriber | End-to-end latency measurement |
| [AverageFrequencyCalculator](src/samples/AverageFrequencyCalculator) | Subscriber | Rolling frequency average calculation |
| [FilterExpressionTests](src/samples/FilterExpressionTests) | Utility | Filter expression parser test harness |
| [InteropTest](src/samples/InteropTest) | Subscriber | Cross-platform interoperability testing |

See the [Sample Applications Guide](doc/Samples.md) for detailed walkthroughs.

---

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](doc/Architecture.md) | System architecture, class hierarchy, threading model, connection modes |
| [TSSC Compression](doc/TSSC.md) | Deep-dive into the Time-Series Special Compression algorithm |
| [Protocol Reference](doc/Protocol.md) | Wire protocol, commands, responses, packet formats |
| [Filter Expressions](doc/FilterExpressions.md) | SQL-like filter expression language reference |
| [Sample Applications](doc/Samples.md) | Guide to all 13 sample applications |
| [Data Sets](src/lib/data/README.md) | DataSet, DataTable, DataColumn, DataRow documentation |
| [Build Instructions](src/README.md) | Windows and Linux build guides |
| [Full STTP Documentation](https://sttp.info/) | Complete STTP specification and reference |

---

## Building

Requires [Boost](https://www.boost.org/) C++ Libraries (v1.80.0 recommended) and a C++20 compiler.

**Linux (Quick Start):**
```bash
cmake .
make -j6 samples
```

**Windows:** Open the Visual Studio 2022 solution in `src/`.

See [Build Instructions](src/README.md) for detailed setup including Boost installation.

---

## License

[MIT License](LICENSE) -- Copyright (c) 2020 Streaming Telemetry Transport Protocol

---

## Security

See [SECURITY.md](SECURITY.md) for vulnerability reporting procedures.

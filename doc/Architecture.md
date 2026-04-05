# STTP C++ API Architecture

> System architecture reference for the Streaming Telemetry Transport Protocol (STTP) C++ implementation, IEEE 2664-2024.

---

## Table of Contents

- [System Overview](#system-overview)
- [Publisher-Subscriber Model](#publisher-subscriber-model)
- [Class Hierarchy](#class-hierarchy)
- [Connection Lifecycle](#connection-lifecycle)
- [Connection Modes](#connection-modes)
- [Signal Routing](#signal-routing)
- [Signal Index Cache](#signal-index-cache)
- [Threading Model](#threading-model)
- [Security Model](#security-model)
- [Measurement Quality Flags](#measurement-quality-flags)
- [Design Patterns](#design-patterns)

---

## System Overview

The STTP C++ API implements a high-performance **publisher-subscriber** messaging system optimized for real-time streaming telemetry. The architecture separates **control** (TCP) from **data** (TCP or UDP) using a dual-channel design.

```mermaid
block-beta
  columns 3

  block:pub["DataPublisher"]:3
    columns 3
    RoutingTables MetadataStore["Metadata (DataSet)"] ThreadPoolP["ThreadPool"]
    SubConn1["SubscriberConnection 1"] SubConn2["SubscriberConnection 2"] SubConnN["SubscriberConnection N"]
  end

  space:1
  block:channels["Network Channels"]:1
    TCP["TCP Command Channel"]
    UDP["UDP Data Channel (optional)"]
  end
  space:1

  block:sub["DataSubscriber"]:3
    columns 3
    SIC["SignalIndexCache [2]"] Connector["SubscriberConnector"] SubInfo["SubscriptionInfo"]
    Decoder["TSSC Decoder"] CallbackQ["Callback Queue"] MRE["ManualResetEvent"]
  end

  pub --> channels
  channels --> sub

  style pub fill:#2d6a4f,color:#fff
  style sub fill:#1b4332,color:#fff
  style channels fill:#40916c,color:#fff
```

### Module Dependency Graph

```mermaid
graph TD
    App["Application Code"] --> PI["PublisherInstance / SubscriberInstance"]
    PI --> DP["DataPublisher"]
    PI --> DS["DataSubscriber"]
    DP --> RT["RoutingTables"]
    DP --> SC["SubscriberConnection"]
    DS --> SIC["SignalIndexCache"]
    DS --> SCon["SubscriberConnector"]
    SC --> TSSC["TSSCEncoder"]
    DS --> TSSCD["TSSCDecoder"]
    SC --> CM["CompactMeasurement"]
    DS --> CM
    DP --> DataSet["DataSet"]
    DS --> DataSet
    DataSet --> FEP["FilterExpressionParser"]
    FEP --> ANTLR["ANTLR4 Runtime"]
    DP --> Boost["Boost (Asio, Thread, System)"]
    DS --> Boost
    DataSet --> pugi["pugixml"]

    style App fill:#264653,color:#fff
    style PI fill:#2a9d8f,color:#fff
    style DP fill:#e9c46a,color:#000
    style DS fill:#e9c46a,color:#000
    style Boost fill:#e76f51,color:#fff
```

---

## Publisher-Subscriber Model

STTP uses a **push-based** model where the publisher actively streams measurements to all subscribed clients. Each subscriber specifies a **filter expression** to select which signals it wants to receive.

```mermaid
graph LR
    subgraph Publisher
        M["Measurements"] --> RT["RoutingTables"]
        RT --> SC1["SubscriberConnection 1"]
        RT --> SC2["SubscriberConnection 2"]
        RT --> SC3["SubscriberConnection N"]
    end

    SC1 -->|"TSSC / Compact"| S1["Subscriber A<br/>(Voltage Phasors)"]
    SC2 -->|"TSSC / Compact"| S2["Subscriber B<br/>(Frequency Data)"]
    SC3 -->|"TSSC / Compact"| S3["Subscriber C<br/>(All Signals)"]

    style Publisher fill:#2d6a4f,color:#fff
    style S1 fill:#1b4332,color:#fff
    style S2 fill:#1b4332,color:#fff
    style S3 fill:#1b4332,color:#fff
```

### Data Flow Pipeline

```mermaid
graph LR
    Raw["Raw Measurement<br/>(SignalID, Value,<br/>Timestamp, Quality)"]
    --> Adj["Apply Adder /<br/>Multiplier"]
    --> Route["RoutingTables<br/>Lookup"]
    --> Encode["TSSC Encode<br/>or Compact"]
    --> Pack["Pack into<br/>DataPacket"]
    --> Send["Send via<br/>TCP or UDP"]
    --> Recv["Receive<br/>DataPacket"]
    --> Decode["TSSC Decode<br/>or Compact"]
    --> CB["Invoke<br/>NewMeasurements<br/>Callback"]

    style Raw fill:#264653,color:#fff
    style Encode fill:#2a9d8f,color:#fff
    style Decode fill:#2a9d8f,color:#fff
    style CB fill:#e76f51,color:#fff
```

---

## Class Hierarchy

```mermaid
classDiagram
    class DataPublisher {
        +Start(endpoints)
        +Stop()
        +PublishMeasurements(measurements)
        +DefineMetadata(metadata)
        +FilterMetadata(expression)
        -m_subscriberConnections
        -m_routingTables
        -m_metadata : DataSet
    }

    class DataSubscriber {
        +Connect(hostname, port)
        +Disconnect()
        +Subscribe(info : SubscriptionInfo)
        +Unsubscribe()
        -m_signalIndexCache[2]
        -m_connector : SubscriberConnector
        -m_commandChannelSocket : TcpSocket
        -m_dataChannelSocket : UdpSocket
    }

    class PublisherInstance {
        #StatusMessage(message)*
        #ErrorMessage(message)*
        #ClientConnected(connection)*
        #ClientDisconnected(connection)*
        +Start(port)
        +Stop()
    }

    class SubscriberInstance {
        #StatusMessage(message)*
        #ErrorMessage(message)*
        #ReceivedNewMeasurements(measurements)*
        #ConnectionEstablished()*
        #ConnectionTerminated()*
        +Connect(hostname, port)
        +SetFilterExpression(expression)
    }

    class SubscriberConnection {
        +PublishMeasurements(measurements)
        +CancelTemporalSubscription()
        -m_signalIndexCache : SignalIndexCache
        -m_tsscEncoder : TSSCEncoder
        -m_keys[2], m_ivs[2]
    }

    class SubscriberConnector {
        +Connect(subscriber)
        +Cancel()
        -m_maxRetries : int32
        -m_retryInterval : int32
        -m_maxRetryInterval : int32
        -m_autoReconnect : bool
    }

    class SubscriptionInfo {
        +FilterExpression : string
        +Throttled : bool
        +PublishInterval : float64
        +UdpDataChannel : bool
        +StartTime : string
        +StopTime : string
        +ProcessingInterval : int32
    }

    PublisherInstance --> DataPublisher : wraps
    SubscriberInstance --> DataSubscriber : wraps
    DataPublisher o-- SubscriberConnection : manages many
    DataSubscriber --> SubscriberConnector : uses
    DataSubscriber --> SubscriptionInfo : configures with
    SubscriberConnection --> SignalIndexCache : maintains
```

---

## Connection Lifecycle

```mermaid
sequenceDiagram
    participant S as DataSubscriber
    participant P as DataPublisher

    Note over S,P: TCP Command Channel
    S->>P: TCP Connect
    S->>P: DefineOperationalModes (compression, encoding flags)
    P->>S: Succeeded

    S->>P: MetadataRefresh
    P->>S: Succeeded + GZip'd XML Metadata (DataSet)

    S->>P: Subscribe (FilterExpression, SubscriptionInfo)
    P->>S: UpdateSignalIndexCache
    S->>P: ConfirmUpdateSignalIndexCache
    P->>S: UpdateBaseTimes
    S->>P: ConfirmUpdateBaseTimes
    P->>S: Succeeded

    Note over S,P: Data Streaming Begins
    loop Every measurement batch
        P-->>S: DataPacket (TSSC compressed)
    end

    loop Keepalive
        P-->>S: NoOP (0xFF)
    end

    S->>P: Unsubscribe
    P->>S: Succeeded
```

---

## Connection Modes

STTP supports three connection patterns to handle different network topologies and firewall configurations.

### Normal Mode (Subscriber Connects to Publisher)

```mermaid
graph LR
    S["DataSubscriber<br/>Connect(host, port)"]
    -->|"TCP SYN"| P["DataPublisher<br/>Listening on port"]

    style S fill:#1b4332,color:#fff
    style P fill:#2d6a4f,color:#fff
```

The standard mode where the publisher listens and the subscriber initiates the connection. Used when the publisher is accessible from the subscriber's network.

### Reverse Mode (Publisher Connects to Subscriber)

```mermaid
graph RL
    P["DataPublisher<br/>ReverseConnect(host, port)"]
    -->|"TCP SYN"| S["DataSubscriber<br/>Listen(port)"]

    style S fill:#1b4332,color:#fff
    style P fill:#2d6a4f,color:#fff
```

In reverse mode, the **subscriber listens** on a port and the **publisher initiates** the TCP connection. This is critical for scenarios where:
- The subscriber is behind a firewall that blocks inbound connections
- The publisher is in a DMZ or external network
- Network policy requires data to be "pushed" from a trusted zone

The protocol flow after connection is identical -- only the TCP initiation direction is reversed.

### Auto-Reconnect State Machine

```mermaid
stateDiagram-v2
    [*] --> Disconnected
    Disconnected --> Connecting : Connect() / autoReconnect
    Connecting --> Connected : TCP handshake success
    Connecting --> WaitRetry : TCP handshake failed
    WaitRetry --> Connecting : retryInterval elapsed<br/>(exponential backoff)
    WaitRetry --> Disconnected : maxRetries exceeded<br/>or Cancel()
    Connected --> Subscribed : Subscribe succeeds
    Subscribed --> Connected : Unsubscribe
    Connected --> Disconnected : Connection lost
    Subscribed --> Disconnected : Connection lost
    Disconnected --> Connecting : autoReconnect = true

    note right of WaitRetry
        Backoff: min(retryInterval * 2^attempt, maxRetryInterval)
        Default: 1000ms initial, 30000ms max
        Retries: -1 = infinite
    end note
```

**SubscriberConnector** manages reconnection with:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `MaxRetries` | `-1` (infinite) | Total retry attempts before giving up |
| `RetryInterval` | `1000` ms | Initial delay between retry attempts |
| `MaxRetryInterval` | `30000` ms | Maximum backoff cap |
| `AutoReconnect` | `true` | Enable automatic reconnection |

---

## Signal Routing

The `RoutingTables` class manages signal-to-subscriber mappings, enabling efficient fan-out of measurements.

```mermaid
graph TD
    M["Incoming Measurements<br/>(SignalID, Value, Timestamp, Quality)"]
    --> RT["RoutingTables"]
    RT -->|"Lookup SignalID → Connections"| Map["Route Map<br/>(HashMap: Guid → Connection Set)"]
    Map --> SC1["SubscriberConnection 1<br/>(subscribed to Guid A, B, C)"]
    Map --> SC2["SubscriberConnection 2<br/>(subscribed to Guid B, D)"]
    Map --> SC3["SubscriberConnection 3<br/>(subscribed to Guid A, C, D, E)"]

    SC1 --> Enc1["TSSC Encode → Send"]
    SC2 --> Enc2["TSSC Encode → Send"]
    SC3 --> Enc3["TSSC Encode → Send"]

    style RT fill:#e9c46a,color:#000
    style Map fill:#f4a261,color:#000
```

Routing operations are **queued** via `ThreadSafeQueue<RoutingTableOperation>` to ensure thread safety. Routes are updated when subscribers change their filter expressions or disconnect.

---

## Signal Index Cache

The `SignalIndexCache` provides compact wire representation of measurements by mapping 128-bit GUIDs to small 16-bit runtime indices.

```mermaid
graph LR
    subgraph Publisher Side
        G["Guid (128-bit)<br/>e.g. {3F2504E0-...}"]
        --> F["Apply Filter<br/>Expression"]
        --> A["Assign Runtime<br/>Index (int32)"]
        --> SER["Serialize Cache"]
    end

    SER -->|"UpdateSignalIndexCache<br/>(compressed)"| DES["Deserialize Cache"]

    subgraph Subscriber Side
        DES --> LUT["Lookup Table<br/>int32 → Guid, Source, ID"]
        LUT --> D["Decode DataPacket<br/>using runtime index"]
    end

    style G fill:#264653,color:#fff
    style LUT fill:#2a9d8f,color:#fff
```

### Double-Buffered Cache

The signal index cache uses **double-buffering** (`m_signalIndexCache[2]`) with the `CacheIndex` flag in `DataPacketFlags` to indicate which cache is active. This enables zero-downtime cache updates:

1. Publisher builds new cache in the inactive slot
2. Sends `UpdateSignalIndexCache` to subscriber
3. Subscriber confirms with `ConfirmUpdateSignalIndexCache`
4. Publisher toggles `CacheIndex` flag in subsequent `DataPacket` headers
5. Both sides now use the new cache while the old one can be recycled

---

## Threading Model

The STTP implementation uses a multi-threaded architecture to maximize throughput and responsiveness.

```mermaid
graph TB
    subgraph "DataPublisher Threads"
        AT["Acceptor Thread<br/>(TCP listener)"]
        PT["Per-Connection Threads<br/>(SubscriberConnection I/O)"]
        RTT["RoutingTables Thread<br/>(route update queue)"]
        CBP["Callback Thread<br/>(publisher events)"]
    end

    subgraph "DataSubscriber Threads"
        CT["Command Channel Thread<br/>(TCP read/write)"]
        DT["Data Channel Thread<br/>(UDP read, if enabled)"]
        RCT["Reconnect Thread<br/>(SubscriberConnector)"]
        CBS["Callback Thread<br/>(subscriber events)"]
    end

    subgraph "Shared Primitives"
        TSQ["ThreadSafeQueue<br/>(MPSC queue)"]
        TP["ThreadPool<br/>(delayed task execution)"]
        MRE["ManualResetEvent<br/>(cross-thread signaling)"]
        STR["Boost.Asio Strand<br/>(serialized socket ops)"]
    end

    CBP --> TSQ
    CBS --> TSQ
    RTT --> TSQ
    PT --> STR
    CT --> STR
    RCT --> MRE

    style TSQ fill:#e9c46a,color:#000
    style TP fill:#e9c46a,color:#000
    style MRE fill:#e9c46a,color:#000
    style STR fill:#e9c46a,color:#000
```

### Concurrency Primitives

| Primitive | Type | Purpose |
|-----------|------|---------|
| `ThreadPool` | Task scheduler | Delayed task execution using timers |
| `ThreadSafeQueue<T>` | MPSC queue | Multiple-producer, single-consumer callback dispatch |
| `ManualResetEvent` | Event signal | Cross-thread wait/notify synchronization |
| `Mutex` / `SharedMutex` | Lock | Exclusive / reader-writer locking |
| `ScopeLock` / `WriterLock` / `ReaderLock` | RAII guard | Scoped lock acquisition |
| `Strand` | Boost.Asio | Serialized async operations on sockets |
| `atomic<bool>` | Atomic flag | Lock-free cancellation / state flags |

### Callback Dispatch Pattern

All user callbacks (status messages, new measurements, connection events) are dispatched through a `ThreadSafeQueue<CallbackDispatcher>` to ensure:
- Single-threaded callback execution (no concurrent callback invocations)
- Decoupling of I/O threads from application logic
- Predictable ordering of events

---

## Security Model

```mermaid
graph TD
    SEC{"SecurityMode?"}
    SEC -->|"Off"| Plain["Unencrypted<br/>TCP/UDP"]
    SEC -->|"TLS"| TLS["TLS Handshake<br/>on TCP Channel"]

    TLS --> ENC["Encrypted Command Channel"]
    TLS --> CKR["Cipher Key Rotation<br/>for Data Channel"]

    CKR --> DualKey["Dual Key Buffer<br/>m_keys[0], m_keys[1]<br/>m_ivs[0], m_ivs[1]"]
    DualKey --> Toggle["CipherIndex flag<br/>toggles even ↔ odd"]

    style SEC fill:#264653,color:#fff
    style TLS fill:#2a9d8f,color:#fff
    style DualKey fill:#e9c46a,color:#000
```

### Cipher Key Rotation Sequence

```mermaid
sequenceDiagram
    participant P as Publisher
    participant S as Subscriber

    Note over P: Timer fires (rotation period)
    P->>P: Generate new key pair<br/>for index (1 - currentIndex)
    P->>S: UpdateCipherKeys (new keys + IVs)
    S->>S: Store keys in m_keys[newIndex]
    S->>P: ConfirmUpdateCipherKeys
    Note over P: Toggle CipherIndex bit<br/>in subsequent DataPackets
    P-->>S: DataPacket (CipherIndex = newIndex)
```

---

## Measurement Quality Flags

Each measurement carries a 32-bit `MeasurementStateFlags` bitmask that provides detailed quality information.

| Bit | Hex | Flag | Category |
|-----|-----|------|----------|
| 0 | `0x0001` | `BadData` | Data Quality |
| 1 | `0x0002` | `SuspectData` | Data Quality |
| 2 | `0x0004` | `OverRangeError` | Data Quality |
| 3 | `0x0008` | `UnderRangeError` | Data Quality |
| 4 | `0x0010` | `AlarmHigh` | Alarm |
| 5 | `0x0020` | `AlarmLow` | Alarm |
| 6 | `0x0040` | `WarningHigh` | Alarm |
| 7 | `0x0080` | `WarningLow` | Alarm |
| 8 | `0x0100` | `FlatlineAlarm` | Alarm |
| 9 | `0x0200` | `ComparisonAlarm` | Alarm |
| 10 | `0x0400` | `ROCAlarm` | Alarm |
| 11 | `0x0800` | `ReceivedAsBad` | Data Quality |
| 12 | `0x1000` | `CalculatedValue` | Calculation |
| 13 | `0x2000` | `CalculationError` | Calculation |
| 14 | `0x4000` | `CalculationWarning` | Calculation |
| 16 | `0x10000` | `BadTime` | Time Quality |
| 17 | `0x20000` | `SuspectTime` | Time Quality |
| 18 | `0x40000` | `LateTimeAlarm` | Time Quality |
| 19 | `0x80000` | `FutureTimeAlarm` | Time Quality |
| 20 | `0x100000` | `UpSampled` | Sampling |
| 21 | `0x200000` | `DownSampled` | Sampling |
| 22 | `0x400000` | `DiscardedValue` | Sampling |
| 24-28 | `0x1000000`-`0x10000000` | `UserDefinedFlag1-5` | User Defined |
| 29 | `0x20000000` | `SystemError` | System |
| 30 | `0x40000000` | `SystemWarning` | System |
| 31 | `0x80000000` | `MeasurementError` | System |

A value of `0x0` means `Normal` -- the measurement is good with no flags set.

---

## Design Patterns

| Pattern | Where Used | Purpose |
|---------|-----------|---------|
| **Observer** | Callback system (`MessageCallback`, `NewMeasurementsCallback`) | Async event notification to application code |
| **Double Buffering** | `SignalIndexCache[2]`, cipher keys `m_keys[2]` | Zero-downtime updates to shared state |
| **Producer-Consumer** | `ThreadSafeQueue<CallbackDispatcher>` | Decouple I/O threads from callback execution |
| **Strategy** | Compression mode (TSSC / Compact / None) | Switchable encoding algorithms |
| **Command** | `ServerCommand` / `ServerResponse` protocol codes | Extensible request-response protocol |
| **Template Method** | `PublisherInstance` / `SubscriberInstance` virtual methods | Simplified API via subclassing |
| **RAII** | `ScopeLock`, `WriterLock`, `ReaderLock` | Automatic resource cleanup |
| **Adapter** | `CompactMeasurement` encoder/decoder | Translates between measurement types and wire format |
| **Factory** | `NewSharedPtr<T>()` helper | Consistent shared pointer creation |

---

*See also: [Protocol Reference](Protocol.md) | [TSSC Compression](TSSC.md) | [Filter Expressions](FilterExpressions.md) | [Sample Applications](Samples.md)*

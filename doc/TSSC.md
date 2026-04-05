# Time-Series Special Compression (TSSC)

> Deep-dive into the TSSC algorithm used by STTP for efficient streaming telemetry compression.

---

## Table of Contents

- [Overview](#overview)
- [Why TSSC?](#why-tssc)
- [Measurement Encoding Pipeline](#measurement-encoding-pipeline)
- [TSSC Code Words](#tssc-code-words)
- [Delta Encoding Strategy](#delta-encoding-strategy)
- [Adaptive Prefix Coding](#adaptive-prefix-coding)
- [Per-Point Metadata State](#per-point-metadata-state)
- [Bit Stream Management](#bit-stream-management)
- [Encoder / Decoder Lifecycle](#encoder--decoder-lifecycle)
- [Compression Performance](#compression-performance)
- [Class Reference](#class-reference)

---

## Overview

**TSSC** (Time-Series Special Compression) is a delta-encoding algorithm with adaptive Huffman-like prefix codes, purpose-built for streaming telemetry data. It is the primary compression method in STTP for reducing bandwidth on the data channel.

Each measurement in STTP consists of four components:

```mermaid
graph LR
    M["Measurement"]
    M --> ID["PointID<br/>(int32)"]
    M --> TS["Timestamp<br/>(int64, ticks)"]
    M --> Q["Quality<br/>(uint32, flags)"]
    M --> V["Value<br/>(float32)"]

    style M fill:#264653,color:#fff
    style ID fill:#2a9d8f,color:#fff
    style TS fill:#e9c46a,color:#000
    style Q fill:#f4a261,color:#000
    style V fill:#e76f51,color:#fff
```

TSSC exploits **temporal locality** in telemetry data: consecutive readings from the same sensor tend to have similar values, incrementing timestamps, and unchanged quality flags. By encoding only the *differences* from previous values, TSSC achieves significant compression ratios.

---

## Why TSSC?

| Feature | TSSC | GZip | Raw |
|---------|------|------|-----|
| **Designed for** | Streaming time-series | General-purpose | N/A |
| **Compression approach** | Per-field delta + adaptive coding | Dictionary + Huffman | None |
| **Latency** | Per-measurement (no buffering needed) | Block-based (needs buffer) | Zero |
| **State tracking** | Per-point history | Sliding window | None |
| **Typical ratio** | 10:1 to 20:1 | 3:1 to 5:1 | 1:1 |
| **CPU overhead** | Very low | Moderate | None |
| **Best for** | Steady-state telemetry streams | Metadata, bulk transfers | Testing |

TSSC outperforms generic compression because it understands the **structure** of time-series data:
- **Timestamps** are nearly monotonic with regular intervals
- **Values** change slowly (e.g., voltage = 119.98, 119.99, 120.01...)
- **Quality flags** rarely change during normal operation
- **Point IDs** follow predictable sequences in a data frame

---

## Measurement Encoding Pipeline

For each measurement, TSSC encodes the four fields in sequence, using delta-encoding against previous values:

```mermaid
flowchart TD
    Input["Input: (id, timestamp, quality, value)"]
    --> Step1{"PointID changed<br/>from previous?"}

    Step1 -->|"Same"| Skip1["No PointID encoding<br/>(implicit)"]
    Step1 -->|"Different"| EncID["Encode PointID XOR delta<br/>4 / 8 / 12 / 16 / 20 / 24 / 32 bits"]

    Skip1 --> Step2
    EncID --> Step2

    Step2{"Timestamp changed<br/>from previous?"}
    Step2 -->|"Same delta<br/>as slot 1-4"| EncTD["Emit TimeDelta<br/>Forward/Reverse code"]
    Step2 -->|"Same as<br/>prevTimestamp2"| EncT2["Emit Timestamp2 code"]
    Step2 -->|"Different"| EncTX["Encode Timestamp XOR<br/>7-bit variable"]

    EncTD --> Step3
    EncT2 --> Step3
    EncTX --> Step3

    Step3{"Quality changed?"}
    Step3 -->|"Same as<br/>PrevQuality1"| Skip3["No Quality encoding<br/>(implicit)"]
    Step3 -->|"Same as<br/>PrevQuality2"| EncQ2["Emit Quality2 code"]
    Step3 -->|"Different"| EncQ7["Encode Quality<br/>7Bit32 variable"]

    Skip3 --> Step4
    EncQ2 --> Step4
    EncQ7 --> Step4

    Step4{"Value changed?"}
    Step4 -->|"Zero"| EncVZ["Emit ValueZero"]
    Step4 -->|"Same as<br/>PrevValue 1/2/3"| EncVP["Emit Value1 / Value2 / Value3"]
    Step4 -->|"Different"| EncVX["Encode Value XOR delta<br/>4 / 8 / 12 / 16 / 20 / 24 / 28 / 32 bits"]

    EncVZ --> Out["Write to BitStream"]
    EncVP --> Out
    EncVX --> Out

    style Input fill:#264653,color:#fff
    style Out fill:#e76f51,color:#fff
```

---

## TSSC Code Words

TSSC defines **32 code words** (0-31), each representing a specific encoding scenario. The encoder selects the code word that requires the fewest bits for the current delta.

### Code Word Reference

| Code | Name | Field | Description |
|------|------|-------|-------------|
| 0 | `EndOfStream` | Control | Marks end of current TSSC block |
| 1 | `PointIDXOR4` | PointID | XOR delta fits in 4 bits |
| 2 | `PointIDXOR8` | PointID | XOR delta fits in 8 bits |
| 3 | `PointIDXOR12` | PointID | XOR delta fits in 12 bits |
| 4 | `PointIDXOR16` | PointID | XOR delta fits in 16 bits |
| 5 | `PointIDXOR20` | PointID | XOR delta fits in 20 bits |
| 6 | `PointIDXOR24` | PointID | XOR delta fits in 24 bits |
| 7 | `PointIDXOR32` | PointID | Full 32-bit XOR delta |
| 8 | `TimeDelta1Forward` | Timestamp | Same delta as slot 1, forward |
| 9 | `TimeDelta2Forward` | Timestamp | Same delta as slot 2, forward |
| 10 | `TimeDelta3Forward` | Timestamp | Same delta as slot 3, forward |
| 11 | `TimeDelta4Forward` | Timestamp | Same delta as slot 4, forward |
| 12 | `TimeDelta1Reverse` | Timestamp | Same delta as slot 1, reversed |
| 13 | `TimeDelta2Reverse` | Timestamp | Same delta as slot 2, reversed |
| 14 | `TimeDelta3Reverse` | Timestamp | Same delta as slot 3, reversed |
| 15 | `TimeDelta4Reverse` | Timestamp | Same delta as slot 4, reversed |
| 16 | `Timestamp2` | Timestamp | Same as previous timestamp 2 |
| 17 | `TimeXOR7Bit` | Timestamp | 7-bit variable-length XOR delta |
| 18 | `Quality2` | Quality | Same as previous quality 2 |
| 19 | `Quality7Bit32` | Quality | 7-bit variable-length encoding |
| 20 | `Value1` | Value | Same as previous value 1 |
| 21 | `Value2` | Value | Same as previous value 2 |
| 22 | `Value3` | Value | Same as previous value 3 |
| 23 | `ValueZero` | Value | Value is zero (0x00000000) |
| 24 | `ValueXOR4` | Value | XOR delta fits in 4 bits |
| 25 | `ValueXOR8` | Value | XOR delta fits in 8 bits |
| 26 | `ValueXOR12` | Value | XOR delta fits in 12 bits |
| 27 | `ValueXOR16` | Value | XOR delta fits in 16 bits |
| 28 | `ValueXOR20` | Value | XOR delta fits in 20 bits |
| 29 | `ValueXOR24` | Value | XOR delta fits in 24 bits |
| 30 | `ValueXOR28` | Value | XOR delta fits in 28 bits |
| 31 | `ValueXOR32` | Value | Full 32-bit XOR delta |

### Code Word Distribution by Field

```mermaid
pie title "TSSC Code Words by Field"
    "PointID (7 codes)" : 7
    "Timestamp (10 codes)" : 10
    "Quality (2 codes)" : 2
    "Value (12 codes)" : 12
    "Control (1 code)" : 1
```

---

## Delta Encoding Strategy

### PointID Delta

The point ID is XOR'd with the previous point ID. The result is encoded using the smallest bit-width that can represent it:

```mermaid
flowchart LR
    XOR["XOR = currentID ^ prevID"]
    --> C4{"fits 4 bits?"}
    C4 -->|Yes| E4["PointIDXOR4<br/>(4 extra bits)"]
    C4 -->|No| C8{"fits 8 bits?"}
    C8 -->|Yes| E8["PointIDXOR8<br/>(8 extra bits)"]
    C8 -->|No| C12{"fits 12?"}
    C12 -->|Yes| E12["PointIDXOR12"]
    C12 -->|No| More["... up to<br/>PointIDXOR32"]

    style XOR fill:#264653,color:#fff
```

### Timestamp Delta

TSSC maintains a **4-slot history** of timestamp deltas (`PrevTimeDelta1..4`). If the current delta matches any slot, a compact 0-bit code is emitted:

```mermaid
flowchart TD
    Delta["delta = timestamp - prevTimestamp1"]
    --> M1{"delta == PrevTimeDelta1?"}
    M1 -->|Yes| TD1F["TimeDelta1Forward ✓"]
    M1 -->|No| M2{"delta == PrevTimeDelta2?"}
    M2 -->|Yes| TD2F["TimeDelta2Forward ✓"]
    M2 -->|No| M3{"delta == PrevTimeDelta3?"}
    M3 -->|Yes| TD3F["TimeDelta3Forward ✓"]
    M3 -->|No| M4{"delta == PrevTimeDelta4?"}
    M4 -->|Yes| TD4F["TimeDelta4Forward ✓"]
    M4 -->|No| NM1{"-delta == PrevTimeDelta1?"}
    NM1 -->|Yes| TD1R["TimeDelta1Reverse ✓"]
    NM1 -->|No| CheckTS2{"timestamp == prevTimestamp2?"}
    CheckTS2 -->|Yes| TS2["Timestamp2 ✓"]
    CheckTS2 -->|No| TX7["TimeXOR7Bit<br/>(variable-length encoding)"]

    style Delta fill:#e9c46a,color:#000
    style TD1F fill:#2a9d8f,color:#fff
    style TX7 fill:#e76f51,color:#fff
```

For telemetry at a fixed sample rate (e.g., 30 samples/second), the timestamp delta is constant across all points in a frame, so `TimeDelta1Forward` is used almost exclusively -- requiring **zero extra bits**.

### Value Delta

Values are compared as raw 32-bit IEEE 754 floats using XOR. The bit-width of the XOR result determines the code word:

```
prevValue = 0x42F00000  (120.0)
currValue = 0x42F00148  (120.01)
XOR       = 0x00000148  → fits in 12 bits → ValueXOR12
```

When a measurement holds steady (common for status signals), the value is identical and no extra bits are needed (`Value1`).

---

## Adaptive Prefix Coding

TSSC uses **adaptive prefix codes** to assign shorter bit patterns to more frequent code words. This is similar in concept to Huffman coding but operates with a fixed set of 4 encoding modes.

### Encoding Modes

Each of the 32 code words is assigned one of 4 modes, determining its prefix bit pattern:

| Mode | Prefix | Total Bits | Description |
|------|--------|-----------|-------------|
| Mode 1 | *(none)* | 5 bits | Most frequent code -- no prefix, just 5-bit code |
| Mode 2 | `1` | 1 + 5 bits | Second most frequent -- 1-bit prefix |
| Mode 3 | `01` | 2 + 5 bits | Third tier -- 2-bit prefix |
| Mode 4 | `001` | 3 + 5 bits | Least frequent -- 3-bit prefix |

### Adaptive Mode Switching

```mermaid
stateDiagram-v2
    [*] --> Startup : Initial fixed assignment
    Startup --> Tracking : Begin counting code usage

    Tracking --> Adapting : commandsSentSinceLastChange<br/>exceeds threshold
    Adapting --> Tracking : Reassign modes based<br/>on m_commandStats[32]

    state Adapting {
        [*] --> SortByFreq : Sort codes by usage count
        SortByFreq --> AssignModes : Most frequent → Mode 1<br/>Next → Mode 2<br/>Next → Mode 3<br/>Rest → Mode 4
        AssignModes --> ResetStats : Clear counters
    }

    note right of Tracking
        m_commandStats[32] tracks
        usage count per code word.
        Per-point adaptation ensures
        optimal coding for each signal.
    end note
```

The adaptation is **per-point**: each measurement point (`TSSCPointMetadata`) maintains its own command statistics and mode assignments. A voltage phasor that changes slowly will adapt differently from a rapidly varying frequency measurement.

### Mode Assignment Example

For a steady-state voltage measurement at 30 Hz:

| Code Word | Typical Frequency | Assigned Mode |
|-----------|-------------------|---------------|
| `TimeDelta1Forward` | ~100% of timestamps | Mode 1 (0 prefix bits) |
| `Value1` (same value) | ~80% of values | Mode 2 (1 prefix bit) |
| `ValueXOR4` | ~15% of values | Mode 3 (2 prefix bits) |
| `Quality7Bit32` | ~0.1% of quality | Mode 4 (3 prefix bits) |

---

## Per-Point Metadata State

Each unique point ID gets a `TSSCPointMetadata` instance that tracks encoding history:

```mermaid
classDiagram
    class TSSCPointMetadata {
        +PrevNextPointID1 : int32
        +PrevQuality1 : uint32
        +PrevQuality2 : uint32
        +PrevValue1 : uint32
        +PrevValue2 : uint32
        +PrevValue3 : uint32
        -m_commandStats[32] : uint8
        -m_commandsSentSinceLastChange : int32
        -m_mode : uint8
        -m_mode21 : uint8
        -m_mode31, m_mode301 : uint8
        -m_mode41, m_mode401, m_mode4001 : uint8
        -m_startupMode : int32
        +WriteCode(code) void
        +ReadCode() int32
        -UpdatedCodeStatistics(code) void
        -AdaptCommands() void
    }

    class TSSCEncoder {
        -m_prevTimestamp1, m_prevTimestamp2 : int64
        -m_prevTimeDelta1..4 : int64
        -m_points : vector~TSSCPointMetadataPtr~
        -m_lastPoint : TSSCPointMetadataPtr
        -m_bitStreamCache : int32
        -m_bitStreamCacheBitCount : int32
        -m_data : uint8*
        -m_position : uint32
        +Reset() void
        +SetBuffer(data, offset, length) void
        +FinishBlock() uint32
        +TryAddMeasurement(id, timestamp, quality, value) bool
    }

    class TSSCDecoder {
        -m_prevTimestamp1, m_prevTimestamp2 : int64
        -m_prevTimeDelta1..4 : int64
        -m_points : vector~TSSCPointMetadataPtr~
        -m_lastPoint : TSSCPointMetadataPtr
        -m_data : uint8*
        -m_position : uint32
        +Reset() void
        +SetBuffer(data, offset, length) void
        +TryGetMeasurement(id, timestamp, quality, value) bool
    }

    TSSCEncoder "1" *-- "many" TSSCPointMetadata : per point
    TSSCDecoder "1" *-- "many" TSSCPointMetadata : per point
```

**State tracking** enables the encoder to exploit patterns unique to each signal. For example:
- A frequency measurement at 60.000 Hz will often have `Value1` (unchanged)
- A voltage phasor angle cycling through 0-360 will use `ValueXOR8` or `ValueXOR12`
- Quality flags in normal operation will always match `PrevQuality1`

---

## Bit Stream Management

TSSC packs variable-length codes into a byte stream using a bit cache:

```mermaid
flowchart LR
    subgraph "Write Path"
        WB["WriteBits(code, length)"]
        --> Cache["m_bitStreamCache<br/>(accumulate bits)"]
        --> Full{"8 bits<br/>accumulated?"}
        Full -->|Yes| Flush["Flush to<br/>m_data[bufferIndex]"]
        Full -->|No| Wait["Wait for<br/>more bits"]
        Flush --> Cache
    end

    subgraph "Read Path"
        RB["ReadBit() / ReadBits5()"]
        --> RCache["m_bitStreamCache<br/>(consume bits)"]
        --> Empty{"Cache<br/>empty?"}
        Empty -->|Yes| Load["Load next byte<br/>from m_data"]
        Empty -->|No| Return["Return bits"]
        Load --> RCache
    end

    style WB fill:#264653,color:#fff
    style RB fill:#264653,color:#fff
    style Flush fill:#2a9d8f,color:#fff
    style Load fill:#2a9d8f,color:#fff
```

The bit stream is flushed at the end of each TSSC block via `BitStreamEnd()`, ensuring the final byte is properly padded.

---

## Encoder / Decoder Lifecycle

```mermaid
sequenceDiagram
    participant App as Application
    participant Enc as TSSCEncoder
    participant Net as Network
    participant Dec as TSSCDecoder
    participant Sub as Subscriber App

    Note over Enc: Publisher Side
    App->>Enc: Reset()
    App->>Enc: SetBuffer(data, 0, MaxPacketSize)

    loop For each measurement in batch
        App->>Enc: TryAddMeasurement(id, ts, quality, value)
        Enc-->>App: true (added) / false (buffer full)
    end

    App->>Enc: FinishBlock()
    Enc-->>App: position (bytes written)
    App->>Net: Send DataPacket (Compressed flag set)

    Note over Dec: Subscriber Side
    Net->>Dec: Receive DataPacket
    Sub->>Dec: Reset()
    Sub->>Dec: SetBuffer(receivedData, offset, length)

    loop Until no more measurements
        Sub->>Dec: TryGetMeasurement(&id, &ts, &quality, &value)
        Dec-->>Sub: true (measurement decoded) / false (end of block)
    end
```

### Reset Behavior

Both encoder and decoder must be **reset** when:
- A new connection is established
- The signal index cache changes (`UpdateSignalIndexCache`)
- A TSSC synchronization error is detected

After reset, all per-point metadata is cleared, and delta encoding starts fresh.

---

## Compression Performance

### Typical Telemetry Characteristics

Synchrophasor data (PMU) at 30 samples/second exhibits strong patterns that TSSC exploits:

| Component | Typical Pattern | TSSC Efficiency |
|-----------|----------------|-----------------|
| **Timestamp** | Fixed 33.33ms interval | 0 extra bits (TimeDelta1Forward) |
| **Quality** | Always Normal (0x0) | 0 extra bits (implicit) |
| **Value (Frequency)** | ~60.000 Hz, changes < 0.01 | 4-8 extra bits (ValueXOR4/8) |
| **Value (Voltage)** | ~120.0V, steady | 0 extra bits (Value1) |
| **Value (Angle)** | Cycling 0-360 | 8-12 extra bits (ValueXOR8/12) |
| **PointID** | Sequential in frame | 4 extra bits (PointIDXOR4) |

### Compression Comparison

```mermaid
---
config:
    xyChart:
        width: 600
        height: 300
---
xychart-beta
    title "Bytes per Measurement (Lower is Better)"
    x-axis ["Raw (uncompressed)", "GZip (block)", "Compact Format", "TSSC"]
    y-axis "Bytes per measurement" 0 --> 30
    bar [26, 8, 10, 2.5]
```

For a typical PMU data stream:
- **Raw**: ~26 bytes/measurement (16-byte GUID + 8-byte value + 8-byte timestamp + 4-byte quality)
- **Compact format**: ~10 bytes/measurement (4-byte index + 8-byte value + optional timestamp)
- **GZip on compact**: ~6-8 bytes/measurement (block compression)
- **TSSC**: **~2-3 bytes/measurement** (delta encoding + adaptive coding)

This translates to **10:1 or better** compression ratios for steady-state telemetry, dramatically reducing bandwidth requirements for large-scale deployments with thousands of measurements.

---

## Class Reference

### Source Files

| File | Path | Purpose |
|------|------|---------|
| `TSSCEncoder.h/cpp` | `src/lib/transport/tssc/` | Encodes measurements into TSSC format |
| `TSSCDecoder.h/cpp` | `src/lib/transport/tssc/` | Decodes TSSC-compressed measurements |
| `TSSCPointMetadata.h/cpp` | `src/lib/transport/tssc/` | Per-point state and adaptive coding logic |

### Key Methods

| Class | Method | Description |
|-------|--------|-------------|
| `TSSCEncoder` | `Reset()` | Clears all state for a fresh encoding session |
| `TSSCEncoder` | `SetBuffer(data, offset, length)` | Sets the output buffer for encoded data |
| `TSSCEncoder` | `TryAddMeasurement(id, ts, quality, value)` | Encodes one measurement; returns `false` if buffer is full |
| `TSSCEncoder` | `FinishBlock()` | Finalizes the current block; returns bytes written |
| `TSSCDecoder` | `Reset()` | Clears all state for a fresh decoding session |
| `TSSCDecoder` | `SetBuffer(data, offset, length)` | Sets the input buffer to decode from |
| `TSSCDecoder` | `TryGetMeasurement(&id, &ts, &quality, &value)` | Decodes one measurement; returns `false` at end of block |

---

*See also: [Architecture](Architecture.md) | [Protocol Reference](Protocol.md) | [Filter Expressions](FilterExpressions.md)*

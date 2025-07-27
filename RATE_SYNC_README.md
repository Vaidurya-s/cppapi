# Dynamic Rate Synchronization Feature

## Overview

The Advanced Publisher and Subscriber applications now include a sophisticated dynamic rate synchronization mechanism that automatically adjusts the publishing rate based on real-time feedback from subscribers about their processing capacity.

## How It Works

### Subscriber Side (`AdvancedSubscribe.cpp`)

1. **Queue Monitoring**: Tracks measurement processing load over a rolling 30-second window
2. **Feedback Generation**: Every 2 seconds, calculates current metrics:
   - `queue_size`: Number of measurements processed in last 30 seconds
   - `lag`: Processing lag in milliseconds  
   - `recommendation`: Rate recommendation based on load
   - `total_processed`: Total measurements since connection

3. **Rate Recommendations**:
   - **READY**: Queue size < 150 (low load, can handle faster rate)
   - **NORMAL**: Queue size 150-600 (medium load, current rate OK)
   - **SLOW**: Queue size > 600 (high load, needs slower rate)

4. **Feedback Protocol**: Uses UserCommand01 with extensible message format:
   ```
   RATE_FEEDBACK:queue_size=250,lag=75.5,recommendation=NORMAL,total_processed=1500
   ```

### Publisher Side (`AdvancedPublish.cpp`)

1. **Feedback Processing**: Listens for UserCommand01 messages from subscribers
2. **Aggregation**: Maintains feedback from all active subscribers (10-second timeout)
3. **Rate Adjustment**: Updates Timer interval based on majority recommendation:
   - **SLOW majority**: Increase interval by 20% (max 500ms)
   - **READY majority**: Decrease interval by 15% (min 10ms)  
   - **NORMAL/mixed**: Gradually return to base 33ms interval

4. **Logging**: Comprehensive logging of all rate changes with subscriber statistics

## Build Instructions

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 2.8 or higher
- Boost libraries (system, filesystem, uuid, iostreams)

### Building the Applications

**Note**: Due to boost UUID hash compatibility issues with newer GCC versions, manual compilation may be needed.

#### Option 1: Individual Compilation (Recommended for Testing)
```bash
# Compile subscriber
g++ -I src/ -I src/lib -I src/lib/transport -I src/Include -std=c++17 \
    -DBOOST_UUID_COMPAT \
    src/samples/AdvancedSubscribe/AdvancedSubscribe.cpp \
    -lboost_system -lboost_filesystem -lboost_thread -lpthread \
    -o AdvancedSubscribe

# Compile publisher  
g++ -I src/ -I src/lib -I src/lib/transport -I src/Include -std=c++17 \
    -DBOOST_UUID_COMPAT \
    src/samples/AdvancedPublish/AdvancedPublish.cpp \
    src/samples/AdvancedPublish/GenHistory.cpp \
    src/samples/AdvancedPublish/TemporalSubscriber.cpp \
    -lboost_system -lboost_filesystem -lboost_thread -lpthread \
    -o AdvancedPublish
```

#### Option 2: Full CMake Build
```bash
mkdir build && cd build
cmake ..
make AdvancedPublish AdvancedSubscribe
```

## Usage Instructions

### Running the Applications

1. **Start the Publisher**:
   ```bash
   ./AdvancedPublish 7165
   ```
   
2. **Connect Subscribers**:
   ```bash
   ./AdvancedSubscribe localhost 7165
   ```

3. **Monitor Rate Synchronization**:
   - Publisher console shows rate adjustments with detailed statistics
   - Subscriber console shows feedback messages being sent
   - Connect multiple subscribers to see aggregated behavior

### Expected Behavior

- **Initial state**: Publisher starts at 33ms interval (30 FPS)
- **Light load**: If subscribers report READY, interval decreases (faster publishing)
- **Heavy load**: If subscribers report SLOW, interval increases (slower publishing)  
- **Mixed load**: System maintains balance or gradually returns to baseline
- **Connection changes**: Automatic cleanup when subscribers disconnect

### Example Output

**Publisher Console**:
```
>> RATE ADJUSTMENT: Publishing interval changed from 33ms to 39ms 
   (Active subscribers: 2, SLOW: 1, NORMAL: 0, READY: 1)

Processed feedback from client "127.0.0.1:12345": queue=150, lag=45.2ms, recommendation=NORMAL
```

**Subscriber Console**:  
```
Dynamic rate synchronization feedback started (2-second interval)
Sent feedback to publisher: RATE_FEEDBACK:queue_size=145,lag=38.50,recommendation=READY,total_processed=725
```

## Testing

Validation tests are provided in `/tmp/` directory:

```bash
# Test feedback parsing
g++ -std=c++17 /tmp/test_feedback_parsing.cpp -o test_feedback && ./test_feedback

# Test rate adjustment logic  
g++ -std=c++17 /tmp/test_rate_adjustment.cpp -o test_rate && ./test_rate

# Test full system simulation
g++ -std=c++17 /tmp/test_full_system.cpp -o test_system && ./test_system
```

## Architecture Notes

### Design Principles
- **Minimal invasive changes**: Uses existing UserCommand infrastructure
- **Extensible protocol**: JSON-like format allows adding new metrics
- **Robust aggregation**: Majority-based decisions prevent oscillation
- **Graceful degradation**: System works with partial feedback

### Key Components Modified
- `Timer::SetInterval()`: Used for dynamic rate adjustment
- `DataPublisher::RegisterUserCommandCallback()`: Handles feedback messages
- `DataSubscriber::SendServerCommand()`: Sends feedback to publisher

### Future Enhancements
The protocol is designed to be extensible. Additional metrics can be added:
- Network latency measurements
- Memory usage statistics  
- CPU utilization data
- Custom application-specific metrics

## Troubleshooting

### Common Issues

1. **Compilation Errors**: 
   - Ensure boost-dev packages are installed
   - Use individual compilation if CMake build fails
   
2. **No Rate Changes**:
   - Verify feedback messages are being sent (check subscriber console)
   - Ensure publisher is receiving UserCommand01 messages
   
3. **Rapid Oscillation**:
   - Check feedback thresholds (150/600 queue size)
   - Verify 5ms minimum change threshold is working

### Debug Mode
Add `-DDEBUG_RATE_SYNC` to compilation flags for verbose debugging output.
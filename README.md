# Parallel Firewall

A high-performance parallel firewall implementation using POSIX threads and a thread-safe ring buffer for processing network packets concurrently.

## Overview

This project implements a parallel firewall that processes network packets using a producer-consumer pattern. Instead of processing packets sequentially, the firewall distributes packet processing across multiple consumer threads, significantly improving throughput for high-volume packet streams.

The system simulates a real firewall scenario where:
- **Producer thread**: Generates packets from an input file and inserts them into a shared ring buffer
- **Consumer threads**: Extract packets from the ring buffer, apply firewall filtering logic, and write results to a log file
- **Ring buffer**: A thread-safe circular buffer that enables efficient packet distribution between producer and consumers

## Architecture

### Ring Buffer Implementation

The core of the parallel implementation is a thread-safe ring buffer (`ring_buffer.c`) that provides:

- **Synchronization primitives**: Uses `pthread_mutex_t` for mutual exclusion and `pthread_cond_t` condition variables for efficient thread signaling
- **FIFO semantics**: Maintains packet order using sequence numbers
- **Non-busy waiting**: Threads block on condition variables instead of polling, ensuring efficient CPU usage
- **Graceful shutdown**: Supports stopping the buffer and notifying all waiting threads

Key functions:
- `ring_buffer_init()`: Initializes the buffer with specified capacity and synchronization primitives
- `ring_buffer_enqueue()`: Thread-safe packet insertion (used by producer)
- `ring_buffer_dequeue()`: Thread-safe packet extraction (used by consumers)
- `ring_buffer_stop()`: Signals all threads that no more packets will be produced
- `ring_buffer_destroy()`: Cleans up allocated resources

### Consumer Threads

The consumer implementation (`consumer.c`) features:

- **Multi-threaded processing**: Supports configurable number of consumer threads (1-32)
- **Ordered logging**: Ensures log entries are written in timestamp order despite parallel processing
- **Sequence-based synchronization**: Uses sequence numbers to coordinate log writes across threads
- **Condition variable signaling**: Consumers wait on condition variables when it's not their turn to write

Each consumer thread:
1. Dequeues packets from the ring buffer
2. Processes packets through the firewall filter (`process_packet()`)
3. Calculates packet hash (`packet_hash()`)
4. Waits for its sequence number before writing to the log file
5. Writes the result (PASS/DROP, hash, timestamp) in the correct order

### Log File Ordering

A critical feature is maintaining log file entries in ascending timestamp order during parallel processing. This is achieved through:

- **Sequence numbers**: Each dequeued packet receives a unique sequence number
- **Synchronized writing**: A mutex-protected condition variable ensures only the thread with the next expected sequence number writes
- **Broadcast signaling**: After writing, threads broadcast to wake up the next waiting thread

This ensures logs are written in order **during processing**, not sorted afterwards.

## Project Structure

```
.
├── src/                    # Source code
│   ├── firewall.c         # Main program entry point
│   ├── ring_buffer.c     # Ring buffer implementation
│   ├── ring_buffer.h     # Ring buffer interface
│   ├── consumer.c        # Consumer thread implementation
│   ├── consumer.h        # Consumer interface
│   ├── producer.c        # Producer thread implementation
│   ├── producer.h        # Producer interface
│   ├── packet.c          # Packet processing logic
│   ├── packet.h          # Packet structures
│   ├── serial.c          # Serial reference implementation
│   └── Makefile          # Build configuration
├── utils/                 # Utility modules
│   └── log/              # Logging utilities
├── tests/                # Test suite
│   ├── checker.py        # Automated test checker
│   ├── grade.sh          # Grading script
│   ├── in/               # Test input files
│   └── ref/              # Reference output files
├── checker/              # Checker utilities
├── Dockerfile            # Docker environment setup
└── README.md             # This file
```

## Building

To build both the serial and parallel versions:

```bash
cd src/
make
```

This creates two executables:
- `serial`: Reference serial implementation
- `firewall`: Parallel implementation

## Usage

Run the parallel firewall:

```bash
./firewall <input-file> <output-file> <num-consumers>
```

Where:
- `input-file`: Path to file containing packets to process
- `output-file`: Path where the log file will be written
- `num-consumers`: Number of consumer threads (1-32)

Example:

```bash
./firewall ../tests/in/test_1000.in output.log 4
```

## Testing

The project includes an automated test suite. To run tests:

```bash
cd tests/
./grade.sh
```

Tests validate:
- Correctness with various packet counts (10, 100, 1,000, 10,000, 20,000)
- Multi-threaded performance (2, 4, 8 threads)
- Log file ordering correctness
- Thread synchronization (no busy waiting, proper blocking)

## Implementation Details

### Synchronization Strategy

The implementation uses POSIX threading primitives:

- **Mutexes**: Protect shared data structures (ring buffer state, log file access)
- **Condition variables**: Enable efficient blocking and signaling between threads
- **No busy waiting**: All waiting is done through condition variable waits

### Thread Safety

- Ring buffer operations are fully thread-safe
- Log file writes are synchronized to maintain order
- Sequence numbers prevent race conditions in log writing

### Performance Characteristics

- **Scalability**: Performance improves with multiple consumer threads
- **Memory efficiency**: Ring buffer uses fixed-size circular allocation
- **CPU efficiency**: No busy waiting, threads block when idle

## Requirements

- POSIX-compliant system (Linux, macOS, etc.)
- pthread library
- GCC compiler
- Make

## License

BSD-3-Clause

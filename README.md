# System Architecture

## Overview

The Lightweight Event-Driven Platform Core is a C++17 embedded-style event-driven system that simulates RTOS (Real-Time Operating System) capabilities with focus on deterministic scheduling, priority-based task execution, and performance instrumentation. The platform provides a modular architecture for task registration, event routing, and timer-driven triggers with starvation prevention mechanisms.

**Key Design Goals:**
- Lightweight and resource-constrained deployment
- Deterministic task execution with priority scheduling
- Interrupt handling simulation using Linux eventfd/timerfd
- Predictable latency and starvation prevention through aging
- Comprehensive performance metrics and monitoring

---

## Core Components

### 1. **EventLoop** (`EventLoop.h`, `EventLoop.cpp`)

The central event dispatcher and I/O multiplexing component built on Linux `epoll`.

**Responsibilities:**
- Manages file descriptor-based event sources (timerfd, eventfd)
- Routes incoming events to registered task handlers
- Executes the main event loop with I/O multiplexing
- Coordinates with the Scheduler for task prioritization

**Key Methods:**
- `add_timerfd(interval_ms, EventType)` - Create timer-based event sources
- `add_eventfd(EventType)` - Create software interrupt event sources
- `register_task(EventType, Task)` - Register task handlers for events
- `signal_eventfd(efd, value)` - Trigger software interrupts
- `run()` - Start the event processing loop
- `stop()` - Gracefully shutdown the loop

**Internal Data Structures:**
```cpp
int epfd_;                                          // epoll file descriptor
std::unordered_map<int, EventType> fd_to_event_type_;             // FD → Event type mapping
std::unordered_map<EventType, std::vector<Task>> event_routes_;   // Event → Task handlers
Scheduler* scheduler_;                              // Reference to scheduler
```

---

### 2. **Scheduler** (`Scheduler.h`, `Scheduler.cpp`)

Priority-based FIFO scheduler with starvation prevention through aging.

**Design Features:**
- **Priority Levels:** 0-32 (configurable via `kMaxPriority`)
- **Priority Queues:** Separate FIFO queue per priority level
- **Aging Mechanism:** Automatically increases effective priority of waiting tasks to prevent starvation
  - Threshold: 0.2ms (200,000 nanoseconds)
  - Tasks waiting longer than threshold get priority boost
- **Thread-Safe:** Protected by mutex for concurrent access

**Key Methods:**
- `enqueue(Task, Event)` - Add task to appropriate queue
- `pick_next()` - Retrieve highest-priority ready task
- `size()` - Get total queued tasks
- `apply_aging_locked(now_ns)` - Age-based priority adjustment

**Data Structure:**
```cpp
std::vector<std::deque<ScheduledItem>> queues_; // Priority level → task queue
int highest_priority_;                           // Cache highest priority level
std::mutex mu_;                                  // Thread safety
```

---

### 3. **Task** (`Task.h`)

Represents a schedulable unit of work with event handler.

**Structure:**
```cpp
struct Task {
    int priority;                              // Priority level (higher = more important)
    std::string name;                          // Task identifier
    std::function<void(const Event&)> handler; // Event handler callback
};
```

---

### 4. **Events** (`Events.h`)

Event type definitions and event payload structure.

**Event Types:**
- `TimerTick` - Periodic timer-based events (from timerfd)
- `Interrupt` - Software or hardware interrupt simulation (from eventfd)

**Event Structure:**
```cpp
struct Event {
    EventType type;        // Type of event
    uint64_t ts_ns;        // Timestamp in nanoseconds
    uint64_t payload;      // Event count/data
};
```

---

### 5. **Metrics** (`Metrics.h`, `Metrics.cpp`)

Performance instrumentation and monitoring system.

**Capabilities:**
- Records task response latencies
- Tracks queue depth over time
- Computes statistical measures:
  - Average latency
  - P95 (95th percentile) latency
  - Worst-case latency
- Supports configurable stress workloads

**Key Methods:**
- `start_timing()` - Begin measurement window
- `record_latency_ns(ns)` - Record single latency sample
- `record_queue_depth(depth)` - Record queue occupancy
- `report()` - Print aggregated metrics

---

## System Architecture Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                      EventLoop (epoll)                       │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐                          │
│  │  timerfd     │  │  eventfd     │                          │
│  │ (TimerTick)  │  │ (Interrupt)  │                          │
│  └──────┬───────┘  └──────┬───────┘                          │
│         │                 │                                  │
│         └─────────┬───────┘                                  │
│                   │                                          │
│          ┌────────▼────────┐                                 │
│          │  Event Router   │                                 │
│          │ fd→EventType    │                                 │
│          └────────┬────────┘                                 │
│                   │                                          │
│          ┌────────▼──────────────────┐                       │
│          │  Task Handler Dispatch    │                       │
│          │  EventType→Tasks[]        │                       │
│          └────────┬──────────────────┘                       │
│                   │                                          │
└───────────────────┼──────────────────────────────────────────┘
                    │
         ┌──────────▼──────────┐
         │   Scheduler Queue   │
         │                     │
         │  Priority Levels    │
         │  ┌────────┐         │
         │  │ P=32   │ FIFO    │
         │  ├────────┤         │
         │  │ P=31   │ FIFO    │
         │  ├────────┤         │
         │  │  ...   │         │
         │  ├────────┤         │
         │  │ P=0    │ FIFO    │
         │  └────────┘         │
         │                     │
         │ ┌──────────────┐    │
         │ │ Aging        │    │
         │ │ thread age   │    │
         │ │ > 200µs      │    │
         │ └──────────────┘    │
         └─────────────────────┘
         
         ▼
    ┌─────────────┐
    │ Task Handler│
    │ Execution   │
    └─────────────┘
         ▼
    ┌─────────────────┐
    │   Metrics       │
    │  - Latency      │
    │  - Queue Depth  │
    └─────────────────┘
```

---

## Data Flow

### 1. **Event Reception & Routing**
```
External Event (timer/interrupt)
    ↓
epoll_wait() detects I/O
    ↓
EventLoop reads FD
    ↓
Map FD → EventType
    ↓
Retrieve registered Tasks for EventType
    ↓
Enqueue Tasks to Scheduler
```

### 2. **Task Scheduling & Execution**
```
Scheduler.enqueue(Task, Event)
    ↓
Select priority queue (P = task.priority)
    ↓
Append to queue[P]
    ↓
Scheduler.pick_next()
    ↓
Select highest priority non-empty queue
    ↓
Apply aging if task waiting > 200µs
    ↓
Return ScheduledItem (Task + Event + timestamp)
    ↓
Execute task.handler(event)
    ↓
Record latency metrics
```

### 3. **Starvation Prevention (Aging)**
```
Task enqueued at time T1
    ↓
Waiting... (T2 - T1) >= 200µs?
    ↓
YES: Increase effective_priority toward kMaxPriority
    ↓
Task moves to higher priority queue on next selection
    ↓
Prevents indefinite waiting of lower-priority tasks
```

---

## Executables & Build Artifacts

### Build System
- **CMake 3.16+** with C++17 standard
- Compilation flags: `-O2 -Wall -Wextra -Wpedantic`

### Targets

#### 1. **core** - Main Application
```
Executable: ./core
Built from:
  - src/main.cpp
  - src/EventLoop.cpp
  - src/Scheduler.cpp
  - src/Metrics.cpp
Purpose: Runs the event-driven platform with default configuration
```

#### 2. **test_scheduler** - Scheduler Unit Tests
```
Executable: ./test_scheduler
Built from:
  - tests/test_scheduler.cpp
  - src/Scheduler.cpp
Purpose: Direct stress testing of Scheduler component
Measurements: Priority queue behavior, aging mechanism, latency
```

#### 3. **stress_injector** - Full System Stress Test
```
Executable: ./stress_injector
Built from:
  - tools/stress_injector.cpp
  - src/EventLoop.cpp
  - src/Scheduler.cpp
  - src/Metrics.cpp
Purpose: Comprehensive system testing with configurable workloads
Measures: End-to-end latency, queue depth, P95/P99 response times
```

---

## Key Architectural Decisions

### 1. **Priority-Based Scheduling**
- Tasks with higher priority values execute before lower priority tasks
- Enables predictable behavior for critical operations
- Prevents resource starvation through aging mechanism

### 2. **Epoll-Based I/O Multiplexing**
- Scale-efficient event source management
- Linux-native for minimal overhead
- Supports mixed synchronous and asynchronous event sources

### 3. **Separation of Concerns**
- **EventLoop**: Event source management and routing
- **Scheduler**: Task prioritization and dispatch
- **Metrics**: Observability and performance analysis
- **Task/Event**: Data structure definitions

### 4. **Thread-Safe Scheduler**
- Mutex protection for concurrent enqueue/dequeue
- Enables integration with multi-threaded event sources
- Lock-only required for queue operations (minimal contention)

### 5. **Aging-Based Starvation Prevention**
- Hard threshold at 200µs prevents indefinite waiting
- Preserves priority for truly high-priority work
- Balances fairness with predictability

---

## Performance Characteristics

### Latency Metrics
- **Measured Quantity:** Time from event generation to task handler completion
- **Statistics:** Average, P95, P99, P999, Maximum
- **Queue Depth Tracking:** Peak and average utilization

### Stress Testing Scenarios
- Configurable event rates (throughput)
- Configurable priority distributions
- Variable task handler runtime
- Simultaneous timer and interrupt events

### Optimization Profile
- Priority queue selection: O(1) amortized (cached highest priority)
- Task enqueue: O(1)
- Task dequeue: O(1)
- Aging check: O(n) worst-case per item (linear scan, but typically low queue depth)

---

## Directory Structure

```
project/
├── CMakeLists.txt              # Build configuration
├── README.md                   # Project overview
├── ARCHITECTURE.md             # This file
├── include/core/
│   ├── EventLoop.h            # Main dispatcher
│   ├── Scheduler.h            # Priority scheduler
│   ├── Task.h                 # Task structure
│   ├── Events.h               # Event definitions
│   └── Metrics.h              # Performance monitoring
├── src/
│   ├── main.cpp               # Main entry point
│   ├── EventLoop.cpp          # EventLoop implementation
│   ├── Scheduler.cpp          # Scheduler implementation
│   └── Metrics.cpp            # Metrics implementation
├── tests/
│   └── test_scheduler.cpp     # Scheduler stress tests
├── tools/
│   └── stress_injector.cpp    # Full system stress testing
└── build/                      # CMake build output
```

---

## Deployment Considerations

### Resource Requirements
- **Memory:** Minimal - single main process, configurable queue sizes
- **CPU:** Single-threaded event loop with optional background threads
- **Kernel Features:** Linux epoll, timerfd, eventfd support

### Real-Time Characteristics
- **Deterministic:** Priority-based dispatch ensures predictable execution order
- **Bounded Latency:** Aging prevents starvation; metrics track worst-case
- **Interrupt Handling:** Supports mixed interrupt types via eventfd/timerfd

### Scaling Limitations
- Event loop latency scales with event source count
- Queue depth affects aging overhead
- Typical deployments: 10-100 concurrent tasks, 1-10 event sources


![Metrics](images/metrics.png)

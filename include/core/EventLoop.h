#pragma once
#include <unordered_map>
#include <vector>
#include <functional>
#include "core/Events.h"
#include "core/Task.h"
#include "core/Scheduler.h"

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void set_scheduler(Scheduler& sched) { scheduler_ = &sched; }

    int add_timerfd(int interval_ms, EventType type);
    int add_eventfd(EventType type);

    void register_task(EventType type, Task task);

    void signal_eventfd(int efd, uint64_t value = 1);

    void run();
    void stop();

private:
    int epfd_{-1};
    bool running_{false};
    Scheduler* scheduler_{nullptr};

    std::unordered_map<int, EventType> fd_to_event_type_;
    std::unordered_map<EventType, std::vector<Task>> event_routes_;
};
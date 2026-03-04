#pragma once
#include <cstdint>
#include <vector>
#include <optional>

class Metrics {
public:
    void start_timing();
    void record_latency_ns(uint64_t ns);
    void record_queue_depth(size_t depth);
    void report() const;

private:
    uint64_t start_time_ns_{0};
    std::vector<uint64_t> latency_samples_;
    std::vector<size_t> queue_depths_;
};
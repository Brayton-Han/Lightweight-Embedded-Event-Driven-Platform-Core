#include "core/Metrics.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <chrono>

static uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void Metrics::start_timing() {
    start_time_ns_ = now_ns();
}

void Metrics::record_latency_ns(uint64_t ns) {
    latency_samples_.push_back(ns);
}

void Metrics::record_queue_depth(size_t depth) {
    queue_depths_.push_back(depth);
}

void Metrics::report() const {
    if (latency_samples_.empty()) {
        std::cout << "No metrics recorded.\n";
        return;
    }

    // Latency statistics
    auto lat = latency_samples_;
    std::sort(lat.begin(), lat.end());

    auto lat_sum = std::accumulate(lat.begin(), lat.end(), uint64_t(0));
    auto lat_avg = static_cast<double>(lat_sum) / lat.size();
    auto lat_min = lat[0];
    auto lat_p50 = lat[lat.size() / 2];
    auto lat_p95 = lat[static_cast<size_t>(lat.size() * 0.95)];
    auto lat_worst = lat.back();

    // Queue depth statistics
    double avg_queue_depth = 0;
    size_t max_queue_depth = 0;
    if (!queue_depths_.empty()) {
        auto depth_sum = std::accumulate(queue_depths_.begin(), queue_depths_.end(), size_t(0));
        avg_queue_depth = static_cast<double>(depth_sum) / queue_depths_.size();
        max_queue_depth = *std::max_element(queue_depths_.begin(), queue_depths_.end());
    }

    // Throughput
    uint64_t elapsed_ns = 0;
    if (start_time_ns_ > 0) {
        elapsed_ns = now_ns() - start_time_ns_;
    }
    double throughput = 0;
    if (elapsed_ns > 0) {
        throughput = static_cast<double>(latency_samples_.size()) / (elapsed_ns / 1e9);
    }

    std::cout << "\n=== Metrics Report ===\n";
    std::cout << "Throughput: " << static_cast<uint64_t>(throughput) << " tasks/sec\n";
    std::cout << "\nLatency (ns):\n";
    std::cout << "  Min:      " << lat_min << "\n";
    std::cout << "  Avg:      " << static_cast<uint64_t>(lat_avg) << "\n";
    std::cout << "  P50:      " << lat_p50 << "\n";
    std::cout << "  P95:      " << lat_p95 << "\n";
    std::cout << "  Max:      " << lat_worst << "\n";
    std::cout << "  Samples:  " << lat.size() << "\n";
    std::cout << "\nQueue Depth:\n";
    std::cout << "  Max:      " << max_queue_depth << "\n";
    std::cout << "  Avg:      " << static_cast<uint64_t>(avg_queue_depth) << "\n";
    std::cout << "  Samples:  " << queue_depths_.size() << "\n";
}
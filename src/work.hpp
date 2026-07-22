#pragma once

#include "search_types.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

namespace affine {

struct WorkItem {
    std::vector<BranchMove> path;
};

struct WorkQueueStats {
    std::vector<std::uint64_t> pushed_by_depth;
    std::vector<std::uint64_t> popped_by_depth;
};

class WorkQueue {
public:
    static constexpr std::size_t no_active_depth =
        std::numeric_limits<std::size_t>::max();

    void push(WorkItem item)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const std::size_t depth = item.path.size();
            ensure_depth(depth);
            items_by_depth_[depth].push_back(std::move(item));
            ++queued_size_;
            ++pushed_by_depth_[depth];
        }
        work_available_.notify_one();
    }

    [[nodiscard]] bool try_pop(WorkItem& item)
    {
        return try_pop(item, nullptr);
    }

    [[nodiscard]] bool try_pop(
        WorkItem& item,
        std::atomic<std::uint32_t>* active_workers)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pop_locked(item, active_workers);
    }

    [[nodiscard]] bool wait_pop(
        WorkItem& item,
        const std::atomic<bool>& stop_requested)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        work_available_.wait(lock, [&] {
            return stop_requested.load(std::memory_order_acquire)
                || queued_size_ != 0
                || active_size_ == 0;
        });

        if (stop_requested.load(std::memory_order_acquire)
            || queued_size_ == 0) {
            return false;
        }
        return pop_locked(item, nullptr);
    }

    void request_stop(std::atomic<bool>& stop_requested)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested.store(true, std::memory_order_release);
        }
        work_available_.notify_all();
    }

    void complete(const WorkItem& item)
    {
        bool exhausted = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const std::size_t depth = item.path.size();
            if (depth < active_by_depth_.size() && active_by_depth_[depth] != 0) {
                --active_by_depth_[depth];
                --active_size_;
            }
            exhausted = queued_size_ == 0 && active_size_ == 0;
        }

        if (exhausted) {
            work_available_.notify_all();
        }
    }

    [[nodiscard]] std::size_t approximate_size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queued_size_;
    }

    [[nodiscard]] bool has_unfinished_above(
        std::size_t depth,
        std::size_t excluded_active_depth = no_active_depth) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t limit = std::min(depth, items_by_depth_.size());
        for (std::size_t current_depth = 0; current_depth < limit; ++current_depth) {
            std::size_t unfinished = items_by_depth_[current_depth].size();
            if (current_depth < active_by_depth_.size()) {
                unfinished += active_by_depth_[current_depth];
                if (current_depth == excluded_active_depth && unfinished != 0) {
                    --unfinished;
                }
            }
            if (unfinished != 0) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] WorkQueueStats stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return WorkQueueStats {
            .pushed_by_depth = pushed_by_depth_,
            .popped_by_depth = popped_by_depth_,
        };
    }

private:
    [[nodiscard]] bool pop_locked(
        WorkItem& item,
        std::atomic<std::uint32_t>* active_workers)
    {
        for (std::size_t depth = 0; depth < items_by_depth_.size(); ++depth) {
            std::deque<WorkItem>& bucket = items_by_depth_[depth];
            if (bucket.empty()) {
                continue;
            }

            item = std::move(bucket.front());
            bucket.pop_front();
            --queued_size_;
            ++active_size_;
            ++active_by_depth_[depth];
            ++popped_by_depth_[depth];
            if (active_workers != nullptr) {
                active_workers->fetch_add(1, std::memory_order_acq_rel);
            }
            return true;
        }

        return false;
    }
    void ensure_depth(std::size_t depth)
    {
        if (depth >= items_by_depth_.size()) {
            items_by_depth_.resize(depth + 1u);
            active_by_depth_.resize(depth + 1u);
            pushed_by_depth_.resize(depth + 1u);
            popped_by_depth_.resize(depth + 1u);
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable work_available_;
    std::vector<std::deque<WorkItem>> items_by_depth_;
    std::vector<std::size_t> active_by_depth_;
    std::vector<std::uint64_t> pushed_by_depth_;
    std::vector<std::uint64_t> popped_by_depth_;
    std::size_t queued_size_ = 0;
    std::size_t active_size_ = 0;
};

[[nodiscard]] inline bool path_has_prefix(
    std::span<const BranchMove> path,
    std::span<const BranchMove> prefix)
{
    if (prefix.size() > path.size()) {
        return false;
    }

    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (path[index] != prefix[index]) {
            return false;
        }
    }

    return true;
}

class PruningStore {
public:
    [[nodiscard]] bool is_dead(std::span<const BranchMove> path) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const std::vector<BranchMove>& prefix : dead_prefixes_) {
            if (path_has_prefix(path, prefix)) {
                return true;
            }
        }
        return false;
    }

    void add_dead_prefix(std::vector<BranchMove> prefix)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dead_prefixes_.push_back(std::move(prefix));
        version_.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] std::uint64_t version() const
    {
        return version_.load(std::memory_order_acquire);
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::vector<BranchMove>> dead_prefixes_;
    std::atomic<std::uint64_t> version_ = 0;
};

} // namespace affine

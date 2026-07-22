#pragma once

#include "profile.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace affine {

class Partition {
public:
    using ObjectId = std::uint32_t;
    using Index = std::uint32_t;
    using CellId = std::uint32_t;
    using Snapshot = std::size_t;
    using Signature = std::uint64_t;

    static constexpr CellId npos = std::numeric_limits<CellId>::max();

    struct Cell {
        Index begin = 0;
        Index end = 0;
        CellId prev = npos;
        CellId next = npos;

        [[nodiscard]] Index size() const
        {
            return end - begin;
        }
    };

    struct RefinementScratch {
        std::vector<std::vector<ObjectId>> buckets;
        std::vector<ObjectId> ordered_objects;
        std::vector<ObjectId> radix_buffer;
        std::vector<std::size_t> histogram;
        std::vector<Index> run_sizes;
        std::vector<Signature> run_signatures;
    };

    Partition() = default;

    explicit Partition(ObjectId object_count)
    {
        reset(object_count);
    }

    void reset(ObjectId object_count)
    {
        elements_.resize(object_count);
        std::iota(elements_.begin(), elements_.end(), ObjectId { 0 });

        position_.resize(object_count);
        std::iota(position_.begin(), position_.end(), Index { 0 });

        object_cell_.assign(object_count, npos);
        cells_.clear();
        trail_.clear();
        saved_element_ranges_.clear();

        if (object_count == 0) {
            first_cell_ = npos;
            last_cell_ = npos;
            return;
        }

        cells_.push_back(Cell {
            .begin = 0,
            .end = static_cast<Index>(object_count),
            .prev = npos,
            .next = npos,
        });
        std::fill(object_cell_.begin(), object_cell_.end(), CellId { 0 });
        first_cell_ = 0;
        last_cell_ = 0;
    }

    [[nodiscard]] Snapshot snapshot() const
    {
        return trail_.size();
    }

    void rollback(Snapshot snapshot)
    {
        while (trail_.size() > snapshot) {
            UndoRecord undo = trail_.back();
            trail_.pop_back();

            switch (undo.kind) {
            case UndoKind::Swap:
                swap_positions_raw(undo.pos_a, undo.pos_b);
                break;
            case UndoKind::Cell:
                cells_[undo.cell_id] = undo.cell;
                break;
            case UndoKind::ObjectCell:
                object_cell_[undo.object] = undo.cell_id;
                break;
            case UndoKind::ObjectCellRange:
                for (Index pos = undo.pos_a; pos < undo.pos_b; ++pos) {
                    object_cell_[elements_[pos]] = undo.cell_id;
                }
                break;
            case UndoKind::ElementRange:
                restore_element_range(undo.cell_count, undo.pos_a);
                break;
            case UndoKind::CellCount:
                cells_.resize(undo.cell_count);
                break;
            case UndoKind::LastCell:
                last_cell_ = undo.cell_id;
                break;
            }
        }
    }

    [[nodiscard]] std::size_t object_count() const
    {
        return elements_.size();
    }

    [[nodiscard]] CellId first_cell() const
    {
        return first_cell_;
    }

    [[nodiscard]] CellId last_cell() const
    {
        return last_cell_;
    }

    [[nodiscard]] CellId next_cell(CellId cell_id) const
    {
        return cells_[cell_id].next;
    }

    [[nodiscard]] CellId previous_cell(CellId cell_id) const
    {
        return cells_[cell_id].prev;
    }

    [[nodiscard]] const Cell& cell(CellId cell_id) const
    {
        return cells_[cell_id];
    }

    [[nodiscard]] CellId cell_of(ObjectId object) const
    {
        return object_cell_[object];
    }

    [[nodiscard]] Index position_of(ObjectId object) const
    {
        return position_[object];
    }

    [[nodiscard]] std::span<const ObjectId> objects(CellId cell_id) const
    {
        const Cell& c = cells_[cell_id];
        return std::span<const ObjectId>(
            elements_.data() + c.begin,
            static_cast<std::size_t>(c.size()));
    }

    [[nodiscard]] bool is_singleton(CellId cell_id) const
    {
        return cells_[cell_id].size() == 1;
    }

    void individualize(ObjectId object)
    {
        const CellId cell_id = object_cell_[object];
        const Cell c = cells_[cell_id];
        if (c.size() <= 1) {
            return;
        }

        std::vector<ObjectId> rest;
        rest.reserve(c.size() - 1);
        for (Index pos = c.begin; pos < c.end; ++pos) {
            const ObjectId current = elements_[pos];
            if (current != object) {
                rest.push_back(current);
            }
        }

        split_cell(cell_id, { { object }, std::move(rest) });
    }

    void split_cell(
        CellId cell_id,
        const std::vector<std::vector<ObjectId>>& buckets)
    {
        assert(cell_id != npos);
        assert(!buckets.empty());

        const Cell old_cell = cells_[cell_id];
        [[maybe_unused]] const Index old_size = old_cell.size();
        assert(old_size > 0);

        [[maybe_unused]] Index total_size = 0;
        for (const auto& bucket : buckets) {
            assert(!bucket.empty());
            total_size += static_cast<Index>(bucket.size());
            for ([[maybe_unused]] const ObjectId object : bucket) {
                assert(object_cell_[object] == cell_id);
            }
        }
        assert(total_size == old_size);

        Index write = old_cell.begin;
        std::vector<Index> run_sizes;
        run_sizes.reserve(buckets.size());
        for (const auto& bucket : buckets) {
            run_sizes.push_back(static_cast<Index>(bucket.size()));
            for (const ObjectId object : bucket) {
                const Index current = position_[object];
                if (current != write) {
                    swap_positions(write, current);
                }
                ++write;
            }
        }

        if (buckets.size() == 1) {
            return;
        }

        split_cell_after_reorder(cell_id, run_sizes);
    }

    bool refine_cell_by_signature(
        CellId cell_id,
        std::span<const Signature> signatures,
        RefinementScratch& scratch)
    {
        if (!build_refinement_plan(cell_id, signatures, scratch)) {
            return false;
        }

        split_cell_by_ordered_objects(cell_id, scratch.ordered_objects, scratch.run_sizes);
        return true;
    }

    bool build_refinement_plan(
        CellId cell_id,
        std::span<const Signature> signatures,
        RefinementScratch& scratch) const
    {
        const bool profile_sample = profile::sample_hot_path();
        profile::ScopedTimer plan_timer(
            profile::counters.partition_plan_ns,
            profile_sample);
        profile::count_if(profile_sample, profile::counters.partition_plan_calls);

        assert(signatures.size() >= object_count());
        assert(cell_id != npos);

        const Cell c = cells_[cell_id];
        profile::count_if(
            profile_sample,
            profile::counters.partition_plan_objects,
            c.size());
        scratch.ordered_objects.clear();
        scratch.run_sizes.clear();
        scratch.run_signatures.clear();

        if (c.size() == 1) {
            const Signature signature = signatures[elements_[c.begin]];
            scratch.run_signatures.push_back(signature);
            scratch.run_sizes.push_back(1);
            return false;
        }

        const Signature first_signature = signatures[elements_[c.begin]];
        bool constant_signature = true;
        {
            profile::ScopedTimer scan_timer(
                profile::counters.partition_constant_scan_ns,
                profile_sample);
            for (Index pos = c.begin + 1; pos < c.end; ++pos) {
                if (signatures[elements_[pos]] != first_signature) {
                    constant_signature = false;
                    break;
                }
            }
        }

        if (constant_signature) {
            scratch.run_signatures.push_back(first_signature);
            scratch.run_sizes.push_back(c.size());
            return false;
        }

        {
            profile::ScopedTimer copy_timer(
                profile::counters.partition_copy_ns,
                profile_sample);
            scratch.ordered_objects.reserve(c.size());
            for (Index pos = c.begin; pos < c.end; ++pos) {
                scratch.ordered_objects.push_back(elements_[pos]);
            }
        }

        profile::count_if(
            profile_sample,
            profile::counters.partition_sort_objects,
            c.size());
        if (c.size() >= radix_sort_threshold) {
            profile::ScopedTimer sort_timer(
                profile::counters.partition_sort_radix_ns,
                profile_sample);
            profile::count_if(
                profile_sample,
                profile::counters.partition_sort_radix_calls);
            radix_sort_by_signature(
                scratch.ordered_objects,
                scratch.radix_buffer,
                scratch.histogram,
                signatures);
        } else {
            profile::ScopedTimer sort_timer(
                profile::counters.partition_sort_std_ns,
                profile_sample);
            profile::count_if(
                profile_sample,
                profile::counters.partition_sort_std_calls);
            std::sort(
                scratch.ordered_objects.begin(),
                scratch.ordered_objects.end(),
                [&](ObjectId lhs, ObjectId rhs) {
                    const Signature left_signature = signatures[lhs];
                    const Signature right_signature = signatures[rhs];
                    if (left_signature != right_signature) {
                        return left_signature < right_signature;
                    }
                    return lhs < rhs;
                });
        }

        {
            profile::ScopedTimer run_timer(
                profile::counters.partition_run_build_ns,
                profile_sample);
            Signature previous_signature = signatures[scratch.ordered_objects.front()];
            scratch.run_signatures.push_back(previous_signature);
            scratch.run_sizes.push_back(0);
            for (const ObjectId object : scratch.ordered_objects) {
                const Signature signature = signatures[object];
                if (signature != previous_signature) {
                    scratch.run_signatures.push_back(signature);
                    scratch.run_sizes.push_back(0);
                    previous_signature = signature;
                }
                ++scratch.run_sizes.back();
            }
        }

        return scratch.run_sizes.size() > 1;
    }

    void split_cell_by_ordered_objects(
        CellId cell_id,
        std::span<const ObjectId> ordered_objects,
        std::span<const Index> run_sizes)
    {
        if (run_sizes.size() <= 1) {
            return;
        }

        const bool profile_sample = profile::sample_hot_path();
        profile::ScopedTimer split_timer(
            profile::counters.partition_split_ns,
            profile_sample);
        profile::count_if(profile_sample, profile::counters.partition_split_calls);
        profile::count_if(
            profile_sample,
            profile::counters.partition_split_objects,
            static_cast<std::uint64_t>(ordered_objects.size()));

        rewrite_cell(cell_id, ordered_objects, profile_sample);
        split_cell_after_reorder(cell_id, run_sizes, profile_sample);
    }

    [[nodiscard]] bool check_invariants() const
    {
        if (elements_.size() != position_.size()
            || elements_.size() != object_cell_.size()) {
            return false;
        }

        std::vector<bool> seen(elements_.size(), false);
        for (Index pos = 0; pos < elements_.size(); ++pos) {
            const ObjectId object = elements_[pos];
            if (object >= elements_.size() || seen[object]) {
                return false;
            }
            seen[object] = true;
            if (position_[object] != pos) {
                return false;
            }
        }

        CellId previous = npos;
        for (CellId cell_id = first_cell_; cell_id != npos; cell_id = cells_[cell_id].next) {
            const Cell& c = cells_[cell_id];
            if (c.prev != previous || c.begin >= c.end || c.end > elements_.size()) {
                return false;
            }
            for (Index pos = c.begin; pos < c.end; ++pos) {
                if (object_cell_[elements_[pos]] != cell_id) {
                    return false;
                }
            }
            previous = cell_id;
        }

        return previous == last_cell_;
    }

private:
    static constexpr Index radix_sort_threshold = 4096;

    enum class UndoKind : std::uint8_t {
        Swap,
        Cell,
        ObjectCell,
        ObjectCellRange,
        ElementRange,
        CellCount,
        LastCell,
    };

    static void radix_sort_by_signature(
        std::vector<ObjectId>& objects,
        std::vector<ObjectId>& buffer,
        std::vector<std::size_t>& histogram,
        std::span<const Signature> signatures)
    {
        constexpr std::size_t radix_bits = 16;
        constexpr std::size_t radix_size = std::size_t { 1 } << radix_bits;
        constexpr Signature radix_mask = static_cast<Signature>(radix_size - 1u);

        buffer.resize(objects.size());
        histogram.resize(radix_size);

        for (std::uint32_t pass = 0; pass < 4; ++pass) {
            std::fill(histogram.begin(), histogram.end(), std::size_t { 0 });
            const std::uint32_t shift = pass * radix_bits;

            for (const ObjectId object : objects) {
                ++histogram[(signatures[object] >> shift) & radix_mask];
            }

            std::size_t offset = 0;
            for (std::size_t& count : histogram) {
                const std::size_t next = offset + count;
                count = offset;
                offset = next;
            }

            for (const ObjectId object : objects) {
                const Signature bucket = (signatures[object] >> shift) & radix_mask;
                buffer[histogram[bucket]++] = object;
            }

            objects.swap(buffer);
        }
    }

    struct UndoRecord {
        UndoKind kind = UndoKind::Swap;
        Index pos_a = 0;
        Index pos_b = 0;
        CellId cell_id = npos;
        Cell cell {};
        ObjectId object = 0;
        std::size_t cell_count = 0;
    };

    struct SavedElementRange {
        std::vector<ObjectId> objects;
    };

    void swap_positions(Index pos_a, Index pos_b)
    {
        trail_.push_back(UndoRecord {
            .kind = UndoKind::Swap,
            .pos_a = pos_a,
            .pos_b = pos_b,
        });
        swap_positions_raw(pos_a, pos_b);
    }

    void swap_positions_raw(Index pos_a, Index pos_b)
    {
        if (pos_a == pos_b) {
            return;
        }

        std::swap(elements_[pos_a], elements_[pos_b]);
        position_[elements_[pos_a]] = pos_a;
        position_[elements_[pos_b]] = pos_b;
    }

    void trail_cell(CellId cell_id)
    {
        trail_.push_back(UndoRecord {
            .kind = UndoKind::Cell,
            .cell_id = cell_id,
            .cell = cells_[cell_id],
        });
    }

    void trail_cell_count()
    {
        trail_.push_back(UndoRecord {
            .kind = UndoKind::CellCount,
            .cell_count = cells_.size(),
        });
    }

    void trail_last_cell()
    {
        trail_.push_back(UndoRecord {
            .kind = UndoKind::LastCell,
            .cell_id = last_cell_,
        });
    }

    void set_object_cell(ObjectId object, CellId cell_id)
    {
        if (object_cell_[object] == cell_id) {
            return;
        }

        trail_.push_back(UndoRecord {
            .kind = UndoKind::ObjectCell,
            .cell_id = object_cell_[object],
            .object = object,
        });
        object_cell_[object] = cell_id;
    }

    void rewrite_cell(
        CellId cell_id,
        std::span<const ObjectId> ordered_objects,
        bool profile_sample = false)
    {
        profile::ScopedTimer rewrite_timer(
            profile::counters.partition_rewrite_ns,
            profile_sample);

        const Cell c = cells_[cell_id];
        assert(ordered_objects.size() == c.size());

        const std::size_t range_index = saved_element_ranges_.size();
        SavedElementRange& saved = saved_element_ranges_.emplace_back();
        saved.objects.assign(elements_.begin() + c.begin, elements_.begin() + c.end);

        trail_.push_back(UndoRecord {
            .kind = UndoKind::ElementRange,
            .pos_a = c.begin,
            .cell_count = range_index,
        });

        Index write = c.begin;
        for (const ObjectId object : ordered_objects) {
            elements_[write] = object;
            position_[object] = write;
            ++write;
        }
    }

    void restore_element_range(std::size_t range_index, Index begin)
    {
        const SavedElementRange& saved = saved_element_ranges_[range_index];
        for (Index offset = 0; offset < saved.objects.size(); ++offset) {
            const ObjectId object = saved.objects[offset];
            const Index pos = begin + offset;
            elements_[pos] = object;
            position_[object] = pos;
        }
        saved_element_ranges_.resize(range_index);
    }

    void split_cell_after_reorder(
        CellId cell_id,
        std::span<const Index> run_sizes,
        bool profile_sample = false)
    {
        profile::ScopedTimer reorder_timer(
            profile::counters.partition_split_after_reorder_ns,
            profile_sample);

        assert(cell_id != npos);
        assert(run_sizes.size() > 1);

        const Cell old_cell = cells_[cell_id];
        const CellId old_next = old_cell.next;

        [[maybe_unused]] Index total_size = 0;
        for (const Index run_size : run_sizes) {
            assert(run_size > 0);
            total_size += run_size;
        }
        assert(total_size == old_cell.size());

        trail_cell(cell_id);
        if (old_next != npos) {
            trail_cell(old_next);
        }
        trail_last_cell();
        trail_cell_count();
        trail_.push_back(UndoRecord {
            .kind = UndoKind::ObjectCellRange,
            .pos_a = old_cell.begin,
            .pos_b = old_cell.end,
            .cell_id = cell_id,
        });

        Index begin = old_cell.begin;
        const Index first_end = begin + run_sizes.front();
        cells_[cell_id].begin = begin;
        cells_[cell_id].end = first_end;

        CellId previous = cell_id;
        begin = first_end;

        for (std::size_t run_index = 1; run_index < run_sizes.size(); ++run_index) {
            const Index end = begin + run_sizes[run_index];
            const CellId new_cell_id = static_cast<CellId>(cells_.size());
            cells_.push_back(Cell {
                .begin = begin,
                .end = end,
                .prev = previous,
                .next = npos,
            });

            cells_[previous].next = new_cell_id;
            previous = new_cell_id;

            for (Index pos = begin; pos < end; ++pos) {
                object_cell_[elements_[pos]] = new_cell_id;
            }

            begin = end;
        }

        cells_[previous].next = old_next;
        if (old_next != npos) {
            cells_[old_next].prev = previous;
        } else {
            last_cell_ = previous;
        }
    }

    std::vector<ObjectId> elements_;
    std::vector<Index> position_;
    std::vector<CellId> object_cell_;
    std::vector<Cell> cells_;
    CellId first_cell_ = npos;
    CellId last_cell_ = npos;
    std::vector<UndoRecord> trail_;
    std::vector<SavedElementRange> saved_element_ranges_;
};

} // namespace affine

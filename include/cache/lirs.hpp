/*******************************************************************************
 * @file lirs.hpp
 * @brief Declaration and implementation of the LIRS cache.
 ******************************************************************************/

#pragma once

#include <cstdio>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "access_result.hpp"

namespace nhwc_cache {

/**
 * @class LIRS
 * @brief Low Inter-reference Recency Set cache with LIR and HIR entries.
 * @tparam KeyT Hashable, equality-comparable key type.
 * @tparam DataT Default-constructible cached data type.
 * @note Copying and moving are disabled.
 */
template <typename KeyT, typename DataT> class LIRS {
public:
    /**
     * @brief Creates a cache with the given resident capacity.
     * @param capacity Maximum number of resident entries.
     * @throws std::invalid_argument If capacity <= 1 or capacity > SIZE_MAX / 2.
     */
    explicit LIRS(std::size_t capacity)
        : cache_capacity_(capacity), max_stack_capacity_(2 * capacity),
          hir_capacity_(std::max<std::size_t>(1, capacity / 100)),
          lir_capacity_(capacity - hir_capacity_), hir_size_(0), lir_size_(0)
    {
        if (capacity <= 1)
            throw std::invalid_argument("LIRS cache size must be greater "
                                        "than one");

        if (capacity > std::numeric_limits<std::size_t>::max() / 2)
            throw std::invalid_argument("LIRS cache capacity must be less");
    }

    LIRS(const LIRS&) = delete;
    LIRS& operator=(const LIRS&) = delete;

    LIRS(LIRS&&) = delete;
    LIRS& operator=(LIRS&&) = delete;

    using key_t = KeyT;
    using data_t = DataT;

    using LoaderT = std::function<void(const KeyT&, DataT&)>;

    /**
     * @brief Looks up a key and updates the replacement state.
     * @param key Key of the requested entry.
     * @param loader  LoaderT (std::function<void(const KeyT&, DataT&)>)
     * @return Cached data by reference and HIT or MISS; see AccessResult.
     */
    AccessResult<DataT> lookup_update(const KeyT& key, const LoaderT& loader)
    {
        EntryIterT entry_it = entries_.find(key);

        if (entry_it == entries_.end()) { //there is no entry in entries
            if (lir_size_ < lir_capacity_)
                return lirs_filling(key, loader);
            else
                return miss(key, loader);
        } else { // there is entry in entries
            Entry& entry = entry_it->second;

            if (entry.status_ == Status::LIR)
                return lir_hit(entry);

            else { // Status::HIR
                if (entry.is_resident_) {
                    if (entry.stack_it_.has_value())
                        return stack_hir_hit(entry);
                    else
                        return non_stack_hir_hit(entry);
                } else // Non resident
                    return ghost_hit(entry, key, loader);
            }
        }
    }

private:
    const std::size_t cache_capacity_;
    const std::size_t max_stack_capacity_;
    const std::size_t hir_capacity_;
    const std::size_t lir_capacity_;

    std::size_t hir_size_;
    std::size_t lir_size_;

    enum Status { LIR, HIR, UNINIT };

    struct Entry;

    using PoolT = typename std::list<Entry*>;
    using EntriesT = typename std::unordered_map<KeyT, Entry>;

    using PoolIterT = typename PoolT::iterator;
    using EntryIterT = typename EntriesT::iterator;

    /** @brief LIR or HIR entry with optional stack and queue positions. */
    struct Entry {
        std::unique_ptr<DataT> data_ = nullptr;

        std::optional<PoolIterT> stack_it_ = std::nullopt;
        std::optional<PoolIterT> queue_it_ = std::nullopt;

        const KeyT* key_ = nullptr;

        Status status_ = Status::UNINIT;
        bool is_resident_ = false;

        /** @brief Allocates data for a new nonresident, unclassified entry. */
        Entry() : data_(std::make_unique<DataT>()) {}

        /** @brief Copy construction is disabled. */
        Entry(const Entry&) = delete;
        /** @brief Copy assignment is disabled. */
        Entry& operator=(const Entry&) = delete;

        /** @brief Move construction is defaulted. */
        Entry(Entry&&) = default;
        /** @brief Move assignment is defaulted. */
        Entry& operator=(Entry&&) = default;
    };

    EntriesT entries_{};
    PoolT recency_stack_{};
    PoolT hir_queue_{};

    /** @brief Moves an LIR entry to the stack head and prunes the stack if needed. */
    AccessResult<DataT> lir_hit(Entry& entry)
    {
        // Move x to head of recency stack
        const PoolIterT stack_it = entry.stack_it_.value();
        const bool was_in_bottom = in_stack_bottom(stack_it);

        move_to_pool_head(recency_stack_, entry.stack_it_.value());

        if (was_in_bottom)
            stack_prune();

        return {*entry.data_, HitStatus::HIT};
    }

    /** @brief Promotes a resident stack HIR entry to LIR and demotes the bottom LIR entry. */
    AccessResult<DataT> stack_hir_hit(Entry& entry)
    {
        move_to_pool_head(recency_stack_, entry.stack_it_.value());
        remove_from_queue(entry);

        move_bottom_stack_to_queue();

        stack_prune();

        return {*entry.data_, HitStatus::HIT};
    };

    /** @brief Refreshes a resident HIR entry in the queue and adds it to the stack. */
    AccessResult<DataT> non_stack_hir_hit(Entry& entry)
    {
        move_to_pool_head(hir_queue_, entry.queue_it_.value());

        // Add entry in recency stack
        add_to_stack_head(entry);

        return {*entry.data_, HitStatus::HIT};
    }

    /** @brief Loads an LIR entry while the initial LIR quota is not filled. */
    AccessResult<DataT> lirs_filling(const KeyT& key, const LoaderT& loader)
    {

        Entry& entry = add_new_entry(key);

        loader(key, *entry.data_);
        entry.is_resident_ = true;

        entry.status_ = Status::LIR;
        ++lir_size_;

        add_to_stack_head(entry);

        return {*entry.data_, HitStatus::MISS};
    }

    /** @brief Loads a new HIR entry, evicting a resident HIR entry if full. */
    AccessResult<DataT> miss(const KeyT& key, const LoaderT& loader)
    {
        if (cache_is_full())
            evict_hir(); // Evict bottom of HIR queue

        Entry& entry = add_new_entry(key);

        // load data in cache
        loader(key, *(entry.data_));
        entry.is_resident_ = true;

        add_to_stack_head(entry);
        entry.status_ = Status::HIR;
        add_to_queue_head(entry);

        return {*entry.data_, HitStatus::MISS};
    }

    /** @brief Reloads a ghost entry as LIR and demotes the bottom LIR entry. */
    AccessResult<DataT> ghost_hit(Entry& entry, const KeyT& key, const LoaderT& loader)
    {
        if (cache_is_full())
            evict_hir(); // Evict bottom of HIR queue

        // load data in cache
        entry.data_ = std::make_unique<DataT>();
        loader(key, *(entry.data_));
        entry.is_resident_ = true;

        move_to_pool_head(recency_stack_, entry.stack_it_.value());
        entry.status_ = Status::LIR;

        // move stack bottom element to HIR queue head
        move_bottom_stack_to_queue();

        stack_prune();

        return {*entry.data_, HitStatus::MISS};
    }

    /** @brief Returns whether the iterator is at the bottom of the nonempty stack. */
    bool in_stack_bottom(PoolIterT stack_it) { return stack_it == std::prev(recency_stack_.end()); }

    /** @brief Moves an entry to the head of its pool without invalidating its iterator. */
    void move_to_pool_head(PoolT& pool, PoolIterT stack_it)
    {
        pool.splice(pool.begin(), pool, stack_it);
    }

    /** @brief Removes bottom HIR entries until the stack bottom is LIR or the stack is empty. */
    void stack_prune()
    {
        while (!recency_stack_.empty()) {
            Entry& entry = *recency_stack_.back();
            if (entry.status_ == Status::LIR)
                break;

            // delete entry from recency stack if x status is HIR
            recency_stack_.pop_back();
            entry.stack_it_ = std::nullopt;

            // delete entry from entries if x is non resident
            if (entry.queue_it_ == std::nullopt)
                entries_.erase(*entry.key_);
        }
    }

    /** @brief Removes a queued HIR entry and marks it as LIR. */
    void remove_from_queue(Entry& entry)
    {
        hir_queue_.erase(entry.queue_it_.value());
        entry.queue_it_ = std::nullopt;
        entry.status_ = Status::LIR;
        --hir_size_;
    }

    /** @brief Demotes the bottom stack entry to HIR and adds it to the queue. */
    void move_bottom_stack_to_queue()
    {
        Entry& stack_entry = *recency_stack_.back();
        add_to_queue_head(stack_entry);
        stack_entry.status_ = Status::HIR;
    }

    /** @brief Returns whether the resident entry count equals capacity. */
    bool cache_is_full() { return lir_size_ + hir_size_ == cache_capacity_; }

    /** @brief Creates an entry and stores a pointer to its key in the entry table. */
    Entry& add_new_entry(const KeyT& key)
    {
        auto [entry_it, inserted] = entries_.emplace(key, Entry{});

        if (!inserted)
            throw std::runtime_error("Not enough memory to add new entry");

        entry_it->second.key_ = std::addressof(entry_it->first);

        return entry_it->second;
    }

    /** @brief Evicts the tail of the nonempty HIR queue, retaining stack history if present. */
    void evict_hir()
    {
        Entry& queue_entry = *hir_queue_.back();
        hir_queue_.pop_back();
        --hir_size_;

        queue_entry.data_.reset();
        queue_entry.is_resident_ = false;
        queue_entry.queue_it_ = std::nullopt;

        if (queue_entry.stack_it_ == std::nullopt)
            entries_.erase(*queue_entry.key_);
    }

    /** @brief Adds an entry to the stack head, trimming ghost history at the stack limit. */
    void add_to_stack_head(Entry& entry)
    {
        if (recency_stack_.size() == max_stack_capacity_)
            remove_ghost_from_stack();

        recency_stack_.emplace_front(std::addressof(entry));
        entry.stack_it_ = recency_stack_.begin();
    }

    /** @brief Adds an entry to the HIR queue head and increments the resident HIR count. */
    void add_to_queue_head(Entry& entry)
    {
        hir_queue_.emplace_front(std::addressof(entry));
        entry.queue_it_ = hir_queue_.begin();
        ++hir_size_;
    }

    /** @brief Removes the oldest nonresident stack entry, if present. */
    void remove_ghost_from_stack()
    {
        for (auto it = recency_stack_.rbegin(), end = recency_stack_.rend(); it != end; ++it) {

            Entry& entry = **it;
            if (!entry.is_resident_) {
                recency_stack_.erase(std::prev(it.base()));
                entries_.erase(*entry.key_);
                return;
            }
        }
    }

}; // class LIRS

} // namespace nhwc_cache

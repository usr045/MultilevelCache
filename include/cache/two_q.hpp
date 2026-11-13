/*******************************************************************************
 * @file two_q.hpp
 * @brief Declaration and implementation of the TwoQ cache.
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "access_result.hpp"
#include "cache_interface.hpp"

namespace nhwc_cache {

/**
 * @class TwoQ
 * @brief 2Q cache with an admission queue, a ghost queue and an LRU queue.
 * @tparam KeyT Hashable, equality-comparable key type.
 * @tparam DataT Default-constructible cached data type.
 * @note Copying and moving are disabled.
 */
template <typename KeyT, typename DataT> class TwoQ final : public Cache<KeyT, DataT> {
public:
    /**
     * @brief Creates a cache with the given resident capacity.
     * @param capacity Maximum number of resident entries.
     * @throws std::invalid_argument if capacity <= 1.
     */
    explicit TwoQ(std::size_t capacity)
        : K_in_(std::max<std::size_t>(1, capacity / 4)),
          K_out_(std::max<std::size_t>(1, capacity / 2)), free_slots_(capacity)
    {
        if (capacity <= 1)
            throw std::invalid_argument(""); // FIXME
    }

    TwoQ(const TwoQ&) = delete;
    TwoQ& operator=(const TwoQ&) = delete;

    TwoQ(TwoQ&&) = delete;
    TwoQ& operator=(TwoQ&&) = delete;

    using key_t = KeyT;
    using data_t = DataT;

    using LoaderT = std::function<void(const KeyT&, DataT&)>;

    /**
     * @brief Looks up a key and updates the replacement state.
     * @param key Key of the requested entry.
     * @param loader  LoaderT (std::function<void(const KeyT&, DataT&)>)
     * @return Cached data by reference and HIT or MISS; see AccessResult.
     */
    AccessResult<DataT> lookup_update(const KeyT& key, const LoaderT& loader) override
    {
        auto entry = entries_table_.find(key);

        if (entry == entries_table_.end())
            return case_nowhere(key, loader);

        else if (entry->second.pool_ == Pools::Am)
            return case_in_am(entry->second);

        else if (entry->second.pool_ == Pools::A1_in)
            return {*entry->second.data_, HitStatus::HIT};

        return case_in_a1_out(entry->second, key, loader);
    }

private:
    enum Pools { A1_in, A1_out, Am };

    using PoolT = typename std::list<KeyT>;
    using IterT = typename PoolT::iterator;
    using DataptrT = typename std::unique_ptr<DataT>;

    /** @brief Resident or ghost entry with its pool position. */
    struct Entry {
        std::unique_ptr<DataT> data_;
        IterT it_;
        Pools pool_;

        /** @brief Creates an entry with its data, list position and pool. */
        Entry(DataptrT data, IterT it, Pools pool) : data_(std::move(data)), it_(it), pool_(pool) {}
    };

    std::list<KeyT> A1_in_{}, A1_out_{}, Am_{};
    std::unordered_map<KeyT, Entry> entries_table_{};

    std::size_t K_in_, K_out_, free_slots_;

    /** @brief Moves a resident Am entry to the MRU position. */
    AccessResult<DataT> case_in_am(const Entry& entry)
    {
        Am_.splice(Am_.begin(), Am_, entry.it_);

        return {*entry.data_, HitStatus::HIT};
    }

    /** @brief Makes room and reloads an A1_out ghost entry into Am. */
    AccessResult<DataT> case_in_a1_out(Entry& entry, const KeyT& key, const LoaderT& loader)
    {
        reclaimfor(key);
        return_from_ghost(entry, key, loader);

        return {*entry.data_, HitStatus::MISS};
    }

    /** @brief Makes room and loads a previously unseen key into A1_in. */
    AccessResult<DataT> case_nowhere(const KeyT& key, const LoaderT& loader)
    {
        reclaimfor(key);
        auto entry_it = add_new(key, loader);

        return {*entry_it->second.data_, HitStatus::MISS};
    }

    /** @brief Frees a resident slot and trims ghost history when needed. */
    void reclaimfor(const KeyT& key)
    {
        if (free_slots_ == 0) {
            if (A1_in_.size() > K_in_) {
                evict_tail(A1_in_, A1_out_, Pools::A1_out);
                ++free_slots_;

                if (A1_out_.size() > K_out_) {
                    if (A1_out_.back() != key)
                        evict_tail(A1_out_);
                }

            } else {
                evict_tail(Am_);
                ++free_slots_;
            }
        }
    }

    /** @brief Creates and loads an entry at the head of A1_in. */
    auto add_new(const KeyT& key, const LoaderT& loader)
    {
        A1_in_.emplace_front(key);

        auto [entry_it, inserted] = entries_table_.try_emplace(key, std::make_unique<DataT>(),
                                                               A1_in_.begin(), Pools::A1_in);

        if (!inserted)
            throw std::runtime_error("Not enough memory to add new entry");

        --free_slots_;
        loader(key, *entry_it->second.data_);

        return entry_it;
    }

    /** @brief Reloads a ghost entry into Am and consumes a free slot. */
    void return_from_ghost(Entry& entry, const KeyT& key, const LoaderT& loader)
    {
        Am_.splice(Am_.begin(), A1_out_, entry.it_);
        entry.pool_ = Pools::Am;
        entry.data_ = std::make_unique<DataT>();

        --free_slots_;
        loader(key, *entry.data_);
    }

    /** @brief Removes the tail entry from a nonempty pool and the entry table. */
    void evict_tail(PoolT& src)
    {
        entries_table_.erase(src.back());
        src.pop_back();
    }

    /** @brief Moves the tail of a nonempty pool to a ghost pool and releases its data. */
    void evict_tail(PoolT& src, PoolT& dest, const Pools poolname)
    {
        dest.splice(dest.begin(), src, std::prev(src.end()));

        auto& entry = entries_table_.at(dest.front());
        entry.pool_ = poolname;
        entry.data_.reset();
    }

}; // class TwoQ

} // namespace nhwc_cache

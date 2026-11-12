/*******************************************************************************
 * @file lfu.hpp
 * @brief Declaration and implementation of the LFU cache.
 ******************************************************************************/

#pragma once

#include <functional>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "access_result.hpp"

namespace cache {

/**
 * @class LFU
 * @brief Least Frequently Used cache with LRU tie-breaking.
 * @tparam KeyT Hashable, equality-comparable key type.
 * @tparam DataT Default-constructible cached data type.
 * @note Copying and moving are disabled.
 */
template <typename KeyT, typename DataT> class LFU {
public:
    /**
     * @brief Creates a cache with the given resident capacity.
     * @param capacity Maximum number of resident entries.
     * @throws std::invalid_argument If capacity <= 1.
     */
    explicit LFU(std::size_t capacity) : freq_list_(), capacity_(capacity)
    {
        if (capacity <= 1)
            throw std::invalid_argument("LFU cache size must be greater "
                                        "than one");
        entries_.reserve(capacity);
    }

    LFU(const LFU&) = delete;
    LFU& operator=(const LFU&) = delete;

    LFU(LFU&&) = delete;
    LFU& operator=(LFU&&) = delete;

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
        auto entry_it = entries_.find(key);

        if (entry_it == entries_.end())
            return request(key, loader);

        return access_cached(entry_it);
    }

private:
    struct ElemInfo;

    /** @brief Access-frequency bucket ordered from MRU to LRU. */
    struct FreqBucket {
        std::list<ElemInfo> elem_list{};
        std::size_t counter;

        /** @brief Creates an empty bucket for the given access frequency. */
        FreqBucket(std::size_t cnt) : counter(cnt) {}
    };

    using FreqBucketIt = typename std::list<FreqBucket>::iterator;

    using DataTptr = std::unique_ptr<DataT>;
    /** @brief Resident entry with its key and parent frequency bucket. */
    struct ElemInfo {
        DataTptr data;
        KeyT key;
        FreqBucketIt parent;

        /** @brief Creates an entry with its data, key and frequency bucket. */
        ElemInfo(DataTptr d, const KeyT& k, FreqBucketIt p) : data(std::move(d)), key(k), parent(p)
        {
        }
    };

    using ElemInfoIt = typename std::list<ElemInfo>::iterator;
    using EntryIt = typename std::unordered_map<KeyT, ElemInfoIt>::iterator;

    std::list<FreqBucket> freq_list_{};
    std::unordered_map<KeyT, ElemInfoIt> entries_{};

    std::size_t capacity_;

    /** @brief Returns whether the resident entry count equals capacity. */
    bool is_full() const noexcept { return capacity_ == entries_.size(); }

    /** @brief Loads a missing key, evicting the LRU entry of the lowest-frequency bucket if full. */
    AccessResult<DataT> request(const KeyT& key, const LoaderT& loader)
    {
        if (is_full()) {
            auto& lfu_elem_list = freq_list_.front().elem_list;
            entries_.erase(lfu_elem_list.back().key); // delete entry from entries
            lfu_elem_list.pop_back();                 // delete elem from list

            // delete frequency bucket if list has not elems
            if (freq_list_.front().elem_list.empty())
                freq_list_.erase(freq_list_.begin());
        }

        if (freq_list_.empty() || freq_list_.front().counter != 1)
            freq_list_.emplace_front(1);

        // add to FreqBucket
        auto& elem_list = freq_list_.begin()->elem_list;

        elem_list.emplace_front(std::make_unique<DataT>(), key, freq_list_.begin());

        // load data
        auto& data = *elem_list.begin()->data;
        loader(key, data);

        // add to entries_table
        entries_.emplace(key, elem_list.begin());

        return {data, HitStatus::MISS};
    }

    /** @brief Increments access frequency and moves the entry to its new bucket. */
    AccessResult<DataT> access_cached(EntryIt entry_it)
    {
        // create new frequency bucket if it not created
        auto freq_it = entry_it->second->parent;
        auto next_freq_it = std::next(freq_it);
        auto counter = freq_it->counter;

        if (next_freq_it == freq_list_.end() || next_freq_it->counter != counter + 1) {
            next_freq_it = freq_list_.emplace(next_freq_it, counter + 1);
        }

        // move elem in next freq bucket
        auto& next_elem_list = next_freq_it->elem_list;
        auto& current_elem_list = freq_it->elem_list;

        next_elem_list.splice(next_elem_list.begin(), current_elem_list, entry_it->second);
        entry_it->second->parent = next_freq_it;

        if (freq_it->elem_list.empty())
            freq_list_.erase(freq_it);

        return {*entry_it->second->data, HitStatus::HIT};
    }

}; // class LFU

} // namespace cache

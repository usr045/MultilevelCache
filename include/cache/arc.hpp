/*******************************************************************************
 * @file arc.hpp
 * @brief Declaration and implementation of the ARC cache.
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "access_result.hpp"
#include "cache_interface.hpp"

namespace nhwc_cache {

/**
 * @class ARC
 * @brief Adaptive Replacement Cache balancing recency and frequency.
 * @tparam KeyT Hashable, equality-comparable key type.
 * @tparam DataT Default-constructible cached data type.
 * @note Copying and moving are disabled.
 * @see https://www.usenix.org/conference/fast-03/arc-self-tuning-low-overhead-replacement-cache
 */
template <typename KeyT, typename DataT> class ARC final : public Cache<KeyT, DataT> {
public:
    /**
     * @brief Creates a cache with the given resident capacity.
     * @param size Maximum number of resident entries.
     * @throws std::invalid_argument If size <= 1.
     */
    explicit ARC(std::size_t capacity) : capacity_(capacity)
    {
        if (capacity <= 1)
            throw std::invalid_argument("ARC cache capacity must be greater "
                                        "than one");
    }

    ARC(const ARC&) = delete;
    ARC& operator=(const ARC&) = delete;

    ARC(ARC&&) = delete;
    ARC& operator=(ARC&&) = delete;

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
        auto entry_it = entries_table_.find(key);

        if (entry_it == entries_table_.end())
            return case_IV(key, loader);

        else if (entry_it->second.pool_ == Pools::B2)
            return case_III(entry_it->second, key, loader);

        else if (entry_it->second.pool_ == Pools::B1)
            return case_II(entry_it->second, key, loader);

        return case_I(entry_it->second);
    }

private:
    using PoolT = std::list<KeyT>;
    using IterT = PoolT::iterator;
    using DataptrT = std::unique_ptr<DataT>;

    enum Pools { T1, T2, B1, B2 };

    /** @brief Resident or ghost entry with its pool position. */
    struct Entry {
        std::unique_ptr<DataT> data_;
        IterT it_;
        Pools pool_;

        /** @brief Creates an entry with its data, pool and list position. */
        Entry(DataptrT data, Pools pool, IterT it) : data_(std::move(data)), it_(it), pool_(pool) {}
    };

    /**
     * @brief ARC pools are ordered from MRU to LRU:
     *        begin() is the most recently used element,
     *        back() is the least recently used element.
     */
    PoolT T1_{}, T2_{}, B1_{}, B2_{};

    /** @brief set of entries. */
    std::unordered_map<KeyT, Entry> entries_table_{};

    /** @brief Maximum number of resident entries in T1 and T2 in total. */
    std::size_t capacity_ = 0;

    /**
     * @brief Adaptive coefficient controlling the target size of T1.
     * @note p_coeff_ is not the actual size of T1;
     *       it represents the target size of T1.
     */
    std::size_t p_coeff_ = 0;

    /** @brief Promotes a resident entry to the MRU position of T2. */
    AccessResult<DataT> case_I(Entry& entry)
    {
        auto& src = entry.pool_ == Pools::T1 ? T1_ : T2_;

        // Move x to T2 head
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;

        return {*entry.data_, HitStatus::HIT};
    }

    /** @brief Handles a B1 ghost hit and increases the target size of T1. */
    AccessResult<DataT> case_II(Entry& entry, const KeyT& key, const LoaderT& loader)
    {
        const std::size_t delta1 = B1_.size() >= B2_.size() ? 1 : B2_.size() / B1_.size();

        p_coeff_ += std::min(delta1, capacity_ - p_coeff_);

        replace(false);

        return_from_ghost_pool(entry, key, loader, B1_);

        return {*entry.data_, HitStatus::MISS};
    }

    /** @brief Handles a B2 ghost hit and decreases the target size of T1. */
    AccessResult<DataT> case_III(Entry& entry, const KeyT& key, const LoaderT& loader)
    {
        const std::size_t delta2 = B2_.size() >= B1_.size() ? 1 : B1_.size() / B2_.size();

        p_coeff_ -= std::min(p_coeff_, delta2);

        replace(true);

        return_from_ghost_pool(entry, key, loader, B2_);

        return {*entry.data_, HitStatus::MISS};
    }

    /** @brief Makes room and loads a previously unseen key into T1. */
    AccessResult<DataT> case_IV(const KeyT& key, const LoaderT& loader)
    {
        if ((T1_.size() + B1_.size()) == capacity_)
            case_IV_A();
        else
            case_IV_B();

        auto entry_it = add_entry(key, loader);

        return {*entry_it->second.data_, HitStatus::MISS};
    }

    /** @brief Makes room when T1 and B1 together reach cache capacity. */
    void case_IV_A()
    {
        if (T1_.size() < capacity_) {
            remove_lru_from_cache(B1_);

            replace(false);
        }

        else
            remove_lru_from_cache(T1_);
    }

    /** @brief Trims history and replaces a resident entry when needed. */
    void case_IV_B()
    {
        assert((T1_.size() + B1_.size()) < capacity_);

        const std::size_t sum = T1_.size() + T2_.size() + B1_.size() + B2_.size();

        if (sum >= capacity_) {
            if (sum == 2 * capacity_)
                remove_lru_from_cache(B2_);

            replace(false);
        }
    }

    /** @brief Evicts a resident entry to B1 or B2 using the adaptive target. */
    void replace(bool incoming_from_b2)
    {
        if (!T1_.empty() && (T1_.size() > p_coeff_ || (incoming_from_b2 && T1_.size() == p_coeff_)))

            evict_to_ghost_pool(T1_, B1_, Pools::B1);

        else
            evict_to_ghost_pool(T2_, B2_, Pools::B2);
    }

    /** @brief Creates and loads an entry at the MRU position of T1. */
    auto add_entry(const KeyT& key, const LoaderT& loader)
    {
        // Put x in MRU T1 and save info in entries table
        T1_.emplace_front(key);

        auto [entry_it, inserted] =
            entries_table_.try_emplace(key, std::make_unique<DataT>(), Pools::T1, T1_.begin());

        if (!inserted)
            throw std::runtime_error("Not enough memory to add new entry");

        loader(key, *(entry_it->second.data_));

        return entry_it;
    }

    /** @brief Reloads a ghost entry and moves it to the MRU position of T2. */
    void return_from_ghost_pool(Entry& entry, const KeyT& key, const LoaderT& loader, PoolT& src)
    {
        // Move x from B1/B2 to the MRU of T2
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;

        entry.data_ = std::make_unique<DataT>();
        loader(key, *entry.data_);
    }

    /** @brief Removes the LRU entry from a nonempty pool and the entry table. */
    void remove_lru_from_cache(PoolT& src)
    {
        assert(!src.empty());

        auto src_it = std::prev(src.end());
        entries_table_.erase(*src_it);
        src.pop_back();
    }

    /** @brief Moves the LRU entry of a nonempty pool to a ghost pool and releases its data. */
    void evict_to_ghost_pool(PoolT& src, PoolT& dest, const Pools dest_name)
    {
        assert(!src.empty());

        // Move from src to dest(ghost pool)
        dest.splice(dest.begin(), src, std::prev(src.end()));

        // Erase data from cache and change entry in entries table
        auto& entry_victim = entries_table_.at(*dest.begin());

        entry_victim.pool_ = dest_name;
        entry_victim.data_.reset();
    }

}; // class ARC

} // namespace nhwc_cache

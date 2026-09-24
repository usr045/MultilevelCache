/*******************************************************************************
 * @file arc.hpp
 * @brief Declaration and implementation of the ARC cache class.
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace cache {

/**
 * @class ARC
 * @brief Adaptive Replacement Cache (ARC) implementation.
 *
 * @tparam KeyT  Type used to identify cached entries.
 * @tparam DataT Type of data stored in the cache.
 * 
 * The cache dynamically balances between recently accessed and frequently
 * accessed entries using four LRU-ordered lists.
 *
 * @note Notation:  x - current element;
 *                  p - adaptive coefficient;
 *                 T1 - recently accessed entries stored in the cache;
 *                 T2 - frequently accessed entries stored in the cache;
 *                 B1 - ghost entries recently removed from T1;
 *                 B2 - ghost entries recently removed from T2.
 *
 * The algorithm was completely taken from the original article about the ARC cache:
 * https://www.usenix.org/conference/fast-03/arc-self-tuning-low-overhead-replacement-cache
 */
template <typename KeyT, typename DataT>
class ARC {
public:
    struct LookupResult {
        LookupResult(const DataT& data, bool hit) : data_(data), hit_(hit) {}

        const DataT& data_; 
        bool hit_;
    };

    explicit ARC(std::size_t size) : cache_size_(size)
    {
        if(size <= 1)
            throw std::invalid_argument("ARC cache size must be greater "
                                        "than one");
    }

    ARC(const ARC&) = delete;
    ARC& operator=(const ARC&) = delete;

    ARC(ARC&&) = delete;
    ARC& operator=(ARC&&) = delete;

    using key_t = KeyT;

    /**
     * @brief Looks up a key in the ARC cache and updates the cache state.
     * 
     * @tparam FuncT Type of the data-loading callable.
     *
     * @param key Key of the requested cache entry.
     * @param loader Callable used to load data on a cache miss.
     *
     * @return true if the requested data was already present in T1 or T2;
     *         false otherwise.   
    */
    template <typename FuncT>
    LookupResult lookup_update(const KeyT& key, FuncT& loader) {
        auto entry_it = entries_table_.find(key);

        if(entry_it == entries_table_.end())
            return case_IV(key, loader);

        else if(entry_it->second.pool_ == Pools::B2)
            return case_III(entry_it->second, key, loader);

        else if(entry_it->second.pool_ == Pools::B1)
            return case_II(entry_it->second, key, loader);

        return case_I(entry_it->second);
    }

private:
    using PoolT = std::list<KeyT>;
    using IterT = PoolT::iterator;
    using DataptrT = std::unique_ptr<DataT>;

    enum Pools { T1, T2, B1, B2 };

    struct Entry {
        std::unique_ptr<DataT> data_;
        IterT it_;
        Pools pool_;

        Entry(DataptrT data, Pools pool, IterT it) : 
            data_(std::move(data)), it_(it), pool_(pool)
        {}
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
    std::size_t cache_size_ = 0;

    /**
     * @brief Adaptive coefficient controlling the target size of T1.
     * @note p_coeff_ is not the actual size of T1;
     *       it represents the target size of T1.
     */
    std::size_t p_coeff_ = 0;

    LookupResult case_I(Entry& entry)
    { 
        auto& src = entry.pool_ == Pools::T1 ? T1_ : T2_;
        
        // Move x to T2 head
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;

        return {*entry.data_, true};
    }

    template <typename FuncT>
    LookupResult case_II(Entry& entry, const KeyT& key, FuncT& loader)
    {
        const std::size_t delta1 = B1_.size() >= B2_.size() ?
            1 : B2_.size() / B1_.size();
        
        p_coeff_ += std::min(delta1, cache_size_ - p_coeff_);
        
        replace(false);

        return_from_ghost_pool(entry, key, loader, B1_);

        return {*entry.data_, false};
    }

    template <typename FuncT>
    LookupResult case_III(Entry& entry, const KeyT& key, FuncT& loader)
    {
        const std::size_t delta2 = B2_.size() >= B1_.size() ? 
            1 : B1_.size() / B2_.size();
    
        p_coeff_ -= std::min(p_coeff_, delta2);

        replace(true);

        return_from_ghost_pool(entry, key, loader, B2_);

        return {*entry.data_, false};
    }

    template <typename FuncT>
    LookupResult case_IV(const KeyT& key, FuncT& loader)
    {
        if((T1_.size() + B1_.size()) == cache_size_)
            case_IV_A();
        else
            case_IV_B();

        auto entry_it = add_entry(key, loader);

        return {*entry_it->second.data_, false};
    }

    void case_IV_A()
    {
        if(T1_.size() < cache_size_) {
            remove_lru_from_cache(B1_);
            
            replace(false);
        }

        else 
            remove_lru_from_cache(T1_);
        
    }

    void case_IV_B()
    {
        assert((T1_.size() + B1_.size()) < cache_size_);
            
        const std::size_t sum =
            T1_.size() + T2_.size() + B1_.size() + B2_.size(); 
        
        if(sum >= cache_size_) {
            if(sum == 2 * cache_size_)
                remove_lru_from_cache(B2_);
            
            replace(false);
        }
    }

    void replace(bool incoming_from_b2)
    {
        if(!T1_.empty() && (T1_.size() > p_coeff_ ||
            (incoming_from_b2 && T1_.size() == p_coeff_)))
            
            evict_to_ghost_pool(T1_, B1_, Pools::B1);
        
        else 
            evict_to_ghost_pool(T2_, B2_, Pools::B2);
    }

    template <typename FuncT>
    auto add_entry(const KeyT& key, FuncT& loader)
    {
        // Put x in MRU T1 and save info in entries table
        T1_.emplace_front(key);

        auto [entry_it, inserted] =
            entries_table_.try_emplace(
                key,
                std::make_unique<DataT>(),
                Pools::T1, T1_.begin()
            );
        
        if(!inserted) throw std::runtime_error("Not enough memory to add new entry");
        
        loader(key, *(entry_it->second.data_));

        return entry_it;
    }

    template <typename FuncT>
    void return_from_ghost_pool(Entry& entry, const KeyT& key,
        FuncT& loader, PoolT& src)
    {
        // Move x from B1/B2 to the MRU of T2
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;

        entry.data_ = std::make_unique<DataT>();
        loader(key, *entry.data_);
    }

    void remove_lru_from_cache(PoolT& src)
    {
        assert(!src.empty());

        auto src_it = std::prev(src.end());
        entries_table_.erase(*src_it);
        src.pop_back();
    }

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

} // namespace cache

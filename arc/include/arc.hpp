/*******************************************************************************
 * @file arc.hpp
 * @brief Declaration and implementation of the ARC cache class.
 *
 * @author usr045
 * @date 2026
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace cache {

/**
 * @class ArcCache
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
class ArcCache {
public:
    using PoolT = typename std::list<KeyT>;
    using IterT = typename PoolT::iterator;
    
    enum Pools { T1, T2, B1, B2 };

    struct Entry {
        Pools pool_;
        IterT it_;
        std::optional<DataT> data_;

        Entry(Pools pool, IterT it) : 
            pool_(pool), it_(it), data_(std::nullopt) 
        {}
    };

    explicit ArcCache(std::size_t size) : cache_size_(size)
    {
        if(size == 0)
            throw std::invalid_argument("ARC cache size must be greater "
                                        "than zero");
    }

    ~ArcCache() = default;

    ArcCache(const ArcCache&) = delete;
    ArcCache& operator=(const ArcCache&) = delete;

    ArcCache(ArcCache&&) = delete;
    ArcCache& operator=(ArcCache&&) = delete;

    /**
     * @brief Looks up a key in the ARC cache and updates the cache state.
     * 
     * @tparam FuncT Type of the data-loading callable.
     *
     * @param key Key of the requested cache entry.
     * @param slow_get_page Callable used to load data on a cache miss.
     *
     * @return true if the requested data was already present in T1 or T2;
     *         false otherwise.   
    */
    template <typename FuncT>
    bool lookup_update(const KeyT& key, FuncT& slow_get_page) {
        auto table_it = table_.find(key);

        if(table_it == table_.end())
            case_IV(key, slow_get_page);

        else if(table_it->second.pool_ == Pools::B2)
            case_III(table_it->second, key, slow_get_page);

        else if(table_it->second.pool_ == Pools::B1)
            case_II(table_it->second, key, slow_get_page);

        else {
            case_I(table_it->second);
            return true;
        }

        return false;
    }
 
private:
    /**
     * @brief ARC pools are ordered from MRU to LRU:
     *        begin() is the most recently used element,
     *        back() is the least recently used element.
     */
    std::list<KeyT> T1_{}, T2_{}, B1_{}, B2_{};
    
    /** @brief set of entries. */
    std::unordered_map<KeyT, Entry> table_{};

    /** @brief Maximum number of resident entries in T1 and T2 in total. */
    std::size_t cache_size_ = 0;

    /**
     * @brief Adaptive coefficient controlling the target size of T1.
     * @note p_coeff_ is not the actual size of T1;
     *       it represents the target size of T1.
     */
    std::size_t p_coeff_ = 0;

    void case_I(Entry& entry)
    { 
        auto& src = entry.pool_ == Pools::T1 ? T1_ : T2_;
        
        // Move x to T2 head
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;
    }

    template <typename FuncT>
    void case_II(Entry& entry, const KeyT& key, FuncT& slow_get_page)
    {
        const std::size_t delta1 = B1_.size() >= B2_.size() ?
            1 : B2_.size() / B1_.size();
        
        p_coeff_ += std::min(delta1, cache_size_ - p_coeff_);
        
        replace(false);

        return_from_ghost_pool(entry, key, slow_get_page, B1_);
    }

    template <typename FuncT>
    void case_III(Entry& entry, const KeyT& key, FuncT& slow_get_page)
    {
        const std::size_t delta2 = B2_.size() >= B1_.size() ? 
            1 : B1_.size() / B2_.size();
    
        p_coeff_ -= std::min(p_coeff_, delta2);

        replace(true);

        return_from_ghost_pool(entry, key, slow_get_page, B2_);
    }

    template <typename FuncT>
    void case_IV(const KeyT& key, FuncT& slow_get_page)
    {
        if((T1_.size() + B1_.size()) == cache_size_)
            case_IV_A();
        else
            case_IV_B();

        add_entry(key, slow_get_page);
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
    void add_entry(const KeyT& key, FuncT& slow_get_page)
    {
        // Put x in MRU T1 and save info in table
        T1_.emplace_front(key);

        auto [table_it, inserted] =
            table_.try_emplace(key, Pools::T1, T1_.begin());
        assert(inserted);
        
        slow_get_page(key, table_it->second.data_.emplace());
    }

    template <typename FuncT>
    void return_from_ghost_pool(Entry& entry, const KeyT& key,
        FuncT& slow_get_page, PoolT& src)
    {
        // Move x from B1/B2 to the MRU of T2
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;

        slow_get_page(key, entry.data_.emplace());
    }

    void remove_lru_from_cache(PoolT& src)
    {
        assert(!src.empty());

        auto src_it = std::prev(src.end());
        table_.erase(*src_it);
        src.pop_back();
    }

    void evict_to_ghost_pool(PoolT& src, PoolT& dest, Pools dest_name)
    {
        assert(!src.empty());

        // Move from src to dest(ghost pool)
        dest.splice(dest.begin(), src, std::prev(src.end()));
        
        // Erase data from cache and change entry in table
        auto& table_victim = table_.at(*dest.begin());

        table_victim.pool_ = dest_name;
        table_victim.data_.reset();
    }

}; // class ArcCache

} // namespace cache

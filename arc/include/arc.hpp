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
#include <list>
#include <optional>
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
 * accessed entries using four LRU-ordered lists:
 *
 * T1 — recently accessed entries stored in the cache;
 * T2 — frequently accessed entries stored in the cache;
 * B1 — ghost entries recently removed from T1;
 * B2 — ghost entries recently removed from T2.
 *
 * An unordered map is used for fast lookup of entries by key. Each map entry
 * stores the current pool of the element, an iterator to its position in the
 * corresponding list, and the cached data when the element belongs to T1 or T2.
 *
 * The adaptive coefficient p controls the target size of T1 and therefore
 * adjusts the balance between recency and frequency. Hits in B1 increase p,
 * giving more space to recently used entries, while hits in B2 decrease p,
 * giving more space to frequently used entries.
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

    ArcCache(std::size_t size) : cache_size_(size) {}
    ~ArcCache() = default;

    ArcCache(const ArcCache&) = delete;

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
    bool lookup_update(KeyT& key, FuncT slow_get_page) {
        auto table_it = table_.find(key);

        if(table_it == table_.end())
            case_IV(table_it->second, key, slow_get_page);

        else if(table_it->second.pool_ == Pools::B2)
            case_III(table_it->second, key, slow_get_page);

        else if(table_it->second.pool_ == Pools::B1)
            case_II(table_it->second, key, slow_get_page);

        else {
            case_I(table_it->second, key);
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
    
    /** @brief set of entries */
    std::unordered_map<KeyT, Entry> table_{};

    /** Maximum number of resident entries in T1 and T2 in total */
    std::size_t cache_size_ = 0;

    /**
     * @brief Adaptive coefficient controlling the target size of T1.
     * @note p_coeff_ is not the actual size of T1;
     *       it represents the target size of T1.
     */
    double p_coeff_ = 0.0;

    /**
     * @brief Handles ARC case I: the requested key is found in T1 or T2.
     *
     * @param entry Metadata of the requested cache entry.
     * @param key Key of the requested cache entry.
     */
    void case_I(auto& entry, KeyT key)
    { 
        auto& src = entry.pool_ == Pools::T1 ? T1_ : T2_;
        
        // Move x to T2 head
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;
    }

    /**
     * @brief Handles ARC case II: the requested key is found in B1.
     *
     * @tparam FuncT Type of the data-loading callable.
     *
     * @param entry Metadata of the requested cache entry.
     * @param key Key of the requested cache entry.
     * @param slow_get_page Callable used to load data into the cache.
     */
    template <typename FuncT>
    void case_II(auto& entry, KeyT key, FuncT slow_get_page)
    {
        double delta1 = B1_.size() >= B2_.size() ? 1 : B2_.size()/B1_.size();
        p_coeff_ = std::min(p_coeff_ + delta1, static_cast<double>(cache_size_));
        
        replace(entry);

        // Move x from B1 to T2 head
        T2_.splice(T2_.begin(), B1_, entry.it_);
        entry.pool_ = Pools::T2;

        slow_get_page(key, entry.data_.emplace());
    }

    /**
     * @brief Handles ARC case III: the requested key is found in B2.
     *
     * @tparam FuncT Type of the data-loading callable.
     *
     * @param entry Metadata of the requested cache entry.
     * @param key Key of the requested cache entry.
     * @param slow_get_page Callable used to load data into the cache.
     */
    template <typename FuncT>
    void case_III(auto& entry, KeyT key, FuncT slow_get_page)
    {
        double delta2 = B2_.size() >= B1_.size() ? 1 : B1_.size()/B2_.size();
        p_coeff_ = std::max(p_coeff_ - delta2, 0.0);

        replace(entry);

        // Move x from B1 to T2 head
        T2_.splice(T2_.begin(), B2_, entry.it_);
        entry.pool_ = Pools::T2;

        slow_get_page(key, entry.data_.emplace());
    }

    /**
    * @brief Handles ARC case IV: the requested key is not tracked by the cache.

    * @tparam FuncT Type of the data-loading callable.
    *
    * @param entry Metadata used by the replacement procedure.
    * @param key Key of the new cache entry.
    * @param slow_get_page Callable used to load data into the cache.
    */
    template <typename FuncT>
    void case_IV(auto& entry, KeyT key, FuncT slow_get_page)
    {
        // case A:
        if((T1_.size() + B1_.size()) == cache_size_)
           case_IV_A(entry);
        
        // case B:
        else 
            case_IV_B(entry, key, slow_get_page);
    }

    /**
     * @brief Handles case IV-A when the combined size of T1 and B1 equals
     *        the cache capacity.
     *
     * @param entry Metadata used by the replacement procedure.
     */
    void case_IV_A(auto& entry)
    {
        if(T1_.size() < cache_size_) {
                assert(!B1_.empty());
                
                // Remove B1 LRU from all cache
                auto b1_lru_it = std::prev(B1_.end());
                table_.erase(*b1_lru_it);
                B1_.pop_back();
                
                replace(entry);
            }
        else  { // B1 should be empty 
            assert(!T1_.empty());
            
            // Remove T1 LRU from all cache
            auto t1_lru_it = std::prev(T1_.end());
            table_.erase(*t1_lru_it);
            T1_.pop_back();
        }
    }

    /**
     * @brief Handles case IV-B when the combined size of T1 and B1 is smaller
     *        than the cache capacity and inserts a new entry into T1.

     * @tparam FuncT Type of the data-loading callable.
     *
     * @param entry Metadata used by the replacement procedure.
     * @param key Key of the new cache entry.
     * @param slow_get_page Callable used to load data into the cache.
     */
    template <typename FuncT>
    void case_IV_B(auto& entry, KeyT key, FuncT slow_get_page)
    {
        assert((T1_.size() + B1_.size()) < cache_size_);
            
            std::size_t sum = T1_.size() + T2_.size() + B1_.size() + B2_.size(); 
            if(sum >= cache_size_) {
                if(sum == 2*cache_size_) {
                    
                    // Remove B2 LRU from all cache
                    auto b2_lru_it = std::prev(B2_.end());
                    table_.erase(*b2_lru_it);
                    B2_.pop_back();
                }

                replace(entry);
            }
            
            // Put x in MRU T1 and save info in table_
            T1_.emplace_front(key);

            auto [table_it, inserted] = table_.try_emplace(key, Pools::T1, T1_.begin());
            assert(inserted);
            
            slow_get_page(key, table_it->second.data_.emplace());
    }

    /**
     * @brief Performs the ARC replacement procedure.
     *
     * @param entry Metadata of the entry that triggered the replacement.
     */
    void replace(auto& entry)
    {
        if(!T1_.empty() &&
            (T1_.size() > p_coeff_ ||
            (entry.pool_ == Pools::B2 && T1_.size() == p_coeff_))) {
            
            // Move LRU T1 to head B1 and erase x's data from cache 
            B1_.splice(B1_.begin(), T1_, entry.it_);
            entry.data_.reset();
        }

        else {
            // Move LRU T2 to head B2 and erase x's data from cache
            B2_.splice(B2_.begin(), T2_, entry.it_);
        }
    }
};

} // namespace cache

#pragma once

#include <algorithm>
#include <cassert>
#include <list>
#include <optional>
#include <unordered_map>

namespace cache {

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

    /*
        TODO: add description about each class, method and func
              add text header
              add checking for slow_get_page
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
    // NOTE: for all pools: pool.begin() = MRU, pool.end()-1 = LRU

    std::list<KeyT> T1_{}, T2_{}, B1_{}, B2_{};
    std::unordered_map<KeyT, Entry> table_{};

    std::size_t cache_size_ = 0;
    double p_coeff_ = 0.0;

    void case_I(auto& entry, KeyT key)
    { 
        auto& src = entry.pool_ == Pools::T1 ? T1_ : T2_;
        
        // Move x to T2 head
        T2_.splice(T2_.begin(), src, entry.it_);
        entry.pool_ = Pools::T2;
    }

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

    template <typename FuncT>
    void case_IV(auto& entry, KeyT key, FuncT slow_get_page)
    {
        // case A:
        if((T1_.size() + B1_.size()) == cache_size_) {
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

        // case B:
        else {
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
    }

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

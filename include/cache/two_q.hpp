/*******************************************************************************
 * @file two_q.hpp
 * @brief Declaration and implementation of the 2Q cache class.
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace cache {

template <typename KeyT, typename DataT>
class TwoQCache {
public:
    explicit TwoQCache(std::size_t cache_size) :
        cache_size_(cache_size),
        K_in_(std::max<std::size_t>(1, cache_size / 4)),
        K_out_(std::max<std::size_t>(1, cache_size / 2)),
        free_slots_(cache_size)
    {
        if(cache_size <= 1)
            throw std::invalid_argument("");
    }

    ~TwoQCache() = default;

    TwoQCache(const TwoQCache&) = delete;
    TwoQCache& operator=(TwoQCache) = delete;
    // TODO

    using key_t = KeyT;
    using data_t = DataT;


    struct LookupResult {
        LookupResult(const DataT& data, bool hit) : data_(data), hit_(hit) {}

        const DataT& data_;
        bool hit_;
    };

    template <typename FuncT>
    LookupResult lookup_update(const KeyT& key, FuncT& loader)
    {
        auto entry = entries_table_.find(key);

        if(entry == entries_table_.end())
            return case_nowhere(key, loader);
        
        else if(entry->second.pool_ == Pools::Am)
            return case_in_am(entry->second);

        else if(entry->second.pool_ == Pools::A1_in)
            return {*entry->second.data_, true};

        return case_in_a1_out(entry->second, key, loader);
    }

private:
    enum Pools { A1_in, A1_out, Am };

    using PoolT = typename std::list<KeyT>;
    using IterT = typename PoolT::iterator;
    using DataptrT = typename std::unique_ptr<DataT>;

    struct Entry {
        std::unique_ptr<DataT> data_;
        IterT it_;
        Pools pool_;

        Entry(DataptrT data, IterT it, Pools pool) :
            data_(std::move(data)), it_(it), pool_(pool)
        {}
    };

    std::list<KeyT> A1_in_{}, A1_out_{}, Am_{};
    std::unordered_map<KeyT, Entry> entries_table_{};
    std::size_t cache_size_;

    std::size_t K_in_, K_out_, free_slots_;

    LookupResult case_in_am(const Entry& entry)
    {
        Am_.splice(Am_.begin(), Am_, entry.it_);

        return {*entry.data_, true};
    }

    template <typename FuncT>
    LookupResult case_in_a1_out(Entry& entry, const KeyT& key, FuncT& loader)
    {
        reclaimfor(key);
        return_from_ghost(entry, key, loader);
        
        return {*entry.data_, false};
    }

    template <typename FuncT>
    LookupResult case_nowhere(const KeyT& key, FuncT& loader)
    {
        reclaimfor(key);
        auto entry_it = add_new(key, loader);

        return {*entry_it->second.data_, false};
    }

    void reclaimfor(const KeyT& key)
    {
        if(free_slots_ == 0) {
            if(A1_in_.size() > K_in_) {
                evict_tail(A1_in_, A1_out_, Pools::A1_out);
                ++free_slots_;
                
                if(A1_out_.size() > K_out_) {
                    if(A1_out_.back() != key)
                        evict_tail(A1_out_);
                }

            }
            else {
                evict_tail(Am_);
                ++free_slots_;
            }
        }
    }

    template <typename FuncT>
    auto& add_new(const KeyT& key, FuncT& loader)
    {
        A1_in_.emplace_front(key);

        auto [entry_it,  inserted] = 
            entries_table_.try_emplace(
                key, 
                std::make_unique<DataT>(),
                A1_in_.begin(),
                Pools::A1_in
            );

        if(!inserted) throw std::runtime_error("Not enough memory to add new entry");

        --free_slots_;
        loader(key, *entry_it->second.data_);

        return entry_it;
    }

    template <typename FuncT>
    void return_from_ghost(Entry& entry, const KeyT& key, FuncT& loader)
    {
        Am_.splice(Am_.begin(), A1_out_, entry.it_);
        entry.pool_ = Pools::Am;
        entry.data_ = std::make_unique<DataT>();

        --free_slots_;
        loader(key, *entry.data_);
    }

    void evict_tail(PoolT& src)
    {
        entries_table_.at(src.back()).data_.reset();
        entries_table_.erase(src.back());
        src.pop_back();
    }

    void evict_tail(PoolT& src, PoolT& dest, const Pools poolname)
    {
        dest.splice(dest.begin(), src, std::prev(src.end()));
        
        auto& entry = entries_table_.at(dest.front());
        entry.pool_ = poolname;
        entry.data_.reset();
    }

}; // class TwoQCache

} // namespace cache

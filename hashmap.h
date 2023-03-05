#pragma once

#include <unordered_map>
#include <list>
#include <utility>
#include <vector>
#include <cstdint>
#include <stdexcept>

template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
private:
    using kv_list = std::list<std::pair<const KeyType, ValueType>>;

public:
    HashMap(Hash hash = Hash()) : table_(INITIAL_SIZE_, elements_.end()), probe_seq_lens_(INITIAL_SIZE_),
                                  hasher_(hash) {}

    template<class Iter>
    HashMap(Iter begin,
            Iter end, Hash hash = Hash())
            : HashMap(hash) {
        for (auto current_iterator = begin; current_iterator != end; ++current_iterator) {
            this->insert(*current_iterator);
        }
    }

    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> init_list, Hash hash = Hash())
            : HashMap(init_list.begin(), init_list.end(), hash) {}

    HashMap(const HashMap &oth) : HashMap(oth.begin(), oth.end(), oth.hasher_) {}

    HashMap &operator=(const HashMap &oth) {
        elements_.clear();
        for (auto &x: oth.elements_) {
            elements_.template emplace_back(x);
        }
        table_ = oth.table_;
        probe_seq_lens_ = oth.probe_seq_lens_;
        hasher_ = oth.hasher_;
        return *this;
    }

    Hash hash_function() const {
        return hasher_;
    }

    std::size_t size() const {
        return elements_.size();
    }

    bool empty() const {
        return elements_.empty();
    }

    void insert(const std::pair<KeyType, ValueType> &new_mapping) {
        if ((this->size() + 1) > LOAD_FACTOR_ * table_.size()) {
            rehash();
        }
        if (find(new_mapping.first) != end()) {
            return;
        }
        elements_.template emplace_back(new_mapping);
        auto current_mapping_it = --elements_.end();
        insert_already_existed(current_mapping_it);
    }

    void erase(const KeyType &to_erase) {
        std::size_t current_pos = hasher_(to_erase) % table_.size();
        while (table_[current_pos] != elements_.end() && !(table_[current_pos]->first == to_erase)) {
            ++current_pos;
            if (current_pos == table_.size()) {
                current_pos = 0;
            }
        }
        if (table_[current_pos] == elements_.end()) {
            return;
        }
        elements_.erase(table_[current_pos]);
        table_[current_pos] = elements_.end();
        probe_seq_lens_[current_pos] = 0;
        // начинаем сдвиг назад всех элементов
        auto prev_pos = current_pos;
        ++current_pos;
        if (current_pos == table_.size()) {
            current_pos = 0;
        }
        while (table_[current_pos] != elements_.end() && probe_seq_lens_[current_pos] > 0) {
            --probe_seq_lens_[current_pos];
            std::swap(probe_seq_lens_[prev_pos], probe_seq_lens_[current_pos]);
            std::swap(table_[prev_pos], table_[current_pos]);
            prev_pos = current_pos;
            ++current_pos;
            if (current_pos == table_.size()) {
                current_pos = 0;
            }
        }
    }

    class iterator {
    public:
        iterator() = default;

        iterator(const iterator &oth) : iter_(oth.iter_) {}

        explicit iterator(const typename kv_list::iterator &iter) : iter_(iter) {}

        iterator &operator++() {
            ++iter_;
            return *this;
        }

        iterator operator++(int) {
            auto to_ret = *this;
            ++iter_;
            return to_ret;
        }

        std::pair<const KeyType, ValueType> &operator*() {
            return *iter_;
        }

        std::pair<const KeyType, ValueType> *operator->() {
            return iter_.operator->();
        }

        bool operator==(const iterator &oth) const {
            return iter_ == oth.iter_;
        }

        bool operator!=(const iterator &oth) const {
            return iter_ != oth.iter_;
        }

    private:
        typename kv_list::iterator iter_;
    };

    class const_iterator {
    public:
        friend class iterator;

        const_iterator() = default;

        const_iterator(const const_iterator &oth) : iter_(oth.iter_) {}

        explicit const_iterator(const typename kv_list::const_iterator &iter) : iter_(iter) {}

        explicit const_iterator(const iterator &oth) : iter_(oth.iter_) {}

        const_iterator &operator++() {
            ++iter_;
            return *this;
        }

        const_iterator operator++(int) {
            auto to_ret = *this;
            ++iter_;
            return to_ret;
        }

        const std::pair<const KeyType, ValueType> &operator*() {
            return *iter_;
        }

        const std::pair<const KeyType, ValueType> *operator->() {
            return iter_.operator->();
        }

        bool operator==(const const_iterator &oth) const {
            return iter_ == oth.iter_;
        }

        bool operator!=(const const_iterator &oth) const {
            return iter_ != oth.iter_;
        }

    private:
        typename kv_list::const_iterator iter_;
    };

    iterator begin() {
        return iterator(elements_.begin());
    }

    const_iterator begin() const {
        return const_iterator(elements_.begin());
    }

    iterator end() {
        return iterator(elements_.end());
    }

    const_iterator end() const {
        return const_iterator(elements_.end());
    }

    iterator find(const KeyType &key) {
        std::size_t current_pos = hasher_(key) % table_.size();
        while (table_[current_pos] != elements_.end() && !(table_[current_pos]->first == key)) {
            ++current_pos;
            if (current_pos == table_.size()) {
                current_pos = 0;
            }
        }
        if (table_[current_pos] == elements_.end()) {
            return end();
        }
        return iterator(table_[current_pos]);
    }

    const_iterator find(const KeyType &key) const {
        std::size_t current_pos = hasher_(key) % table_.size();
        while (table_[current_pos] != elements_.end() && !(table_[current_pos]->first == key)) {
            ++current_pos;
            if (current_pos == table_.size()) {
                current_pos = 0;
            }
        }
        if (table_[current_pos] == elements_.end()) {
            return end();
        }
        return const_iterator(table_[current_pos]);
    }

    ValueType &operator[](const KeyType &key) {
        iterator it = find(key);
        if (it == end()) {
            insert(std::pair{key, ValueType()});
            it = find(key);
        }
        return it->second;
    }

    const ValueType &at(const KeyType &key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("Couldn't find an element in HashTable in call of .at()");
        }
        return it->second;
    }

    void clear() {
        table_.assign(table_.size(), elements_.end());
        probe_seq_lens_.assign(probe_seq_lens_.size(), 0);
        elements_.clear();
    }

private:
    void insert_already_existed(typename kv_list::iterator current_mapping_it) {
        std::size_t current_pos = hasher_(current_mapping_it->first) % table_.size();
        std::size_t current_psl = 0;
        while (table_[current_pos] != elements_.end()) {
            if (probe_seq_lens_[current_pos] < current_psl) {
                std::swap(current_mapping_it, table_[current_pos]);
                std::swap(current_psl, probe_seq_lens_[current_pos]);
            }
            ++current_pos;
            if (current_pos == table_.size()) {
                current_pos = 0;
            }
            ++current_psl;
        }
        table_[current_pos] = current_mapping_it;
        probe_seq_lens_[current_pos] = current_psl;
    }

    void rehash() {
        table_.assign(table_.size() * 2, elements_.end());
        probe_seq_lens_.assign(probe_seq_lens_.size() * 2, 0);
        for (auto current_it = elements_.begin(); current_it != elements_.end(); ++current_it) {
            insert_already_existed(current_it);
        }
    }

    kv_list elements_;
    std::vector<typename kv_list::iterator> table_;
    std::vector<std::size_t> probe_seq_lens_;
    Hash hasher_ = Hash();
    constexpr static const float LOAD_FACTOR_ = 0.75;
    constexpr static std::size_t INITIAL_SIZE_ = 8;
};


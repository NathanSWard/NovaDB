#ifndef COLLECTION_H
#define COLLECTION_H

#include "bson.hpp"
#include "../debug.hpp"
#include "document.hpp"
#include "index.hpp"
#include "map_results.hpp"
#include "non_null_ptr.hpp"

#include <tuple>
#include <unordered_map>

namespace nova {

class collection;

 class collection_iterator {
        decltype(std::declval<std::vector<document*>>().begin()) it_;
        using iter_type = decltype(it_);
    public:
        using value_type = document;
        using reference = document&;
        using iterator_category = iter_type::iterator_category;
        using difference_type = iter_type::difference_type;
        
        constexpr collection_iterator(iter_type it) noexcept : it_(it) {}
        
        auto operator++() { ++it_; return *this; }
        reference operator*() { return **it_; }
        bool operator==(collection_iterator const& rhs) const noexcept { return it_ == rhs.it_; }
        bool operator!=(collection_iterator const& rhs) const noexcept { return it_ != rhs.it_; }
    };

    class collection_const_iterator {
        decltype(std::declval<std::vector<document*>>().cbegin()) it_;
        using iter_type = decltype(it_);
    public:
        using value_type = document;
        using reference = document const&;
        using iterator_category = iter_type::iterator_category;
        using difference_type = iter_type::difference_type;
        
        constexpr collection_const_iterator(iter_type it) noexcept : it_(it) { }
        
        auto operator++() { ++it_; return *this; }
        reference operator*() { return **it_; }
        bool operator==(collection_const_iterator const& rhs) const noexcept { return it_ == rhs.it_; }
        bool operator!=(collection_const_iterator const& rhs) const noexcept { return it_ != rhs.it_; }
    };

class query_result {
    std::vector<document*> docs_{};
    friend class collection;
public:
    [[nodiscard]] collection_iterator begin() noexcept { return docs_.begin(); }
    [[nodiscard]] collection_const_iterator begin() const noexcept { return docs_.begin(); }
    [[nodiscard]] collection_iterator end() noexcept { return docs_.end(); }
    [[nodiscard]] collection_const_iterator end() const noexcept { return docs_.end(); }
};

class collection {
    std::vector<document*> docs_{};
    unordered_unique_index id_index_{};
    //std::unordered_map<std::string, index> indices_{};

public:
    collection() = default;

    ~collection() {
        for (auto&& ptr : docs_)
            delete ptr;
    }

/*
    [[nodiscard]] bool has_index(std::string const& field) const {
        return indices_.find(field) != indices_.end();
    }

    insert_result<std::string, index const> create_index(std::string const& field) {
        auto const [iter, b] = indices_.try_emplace(field);
        return {iter->first, iter->second, b};
    }
*/

    template<template<class, class> class... Tpls, class... Fields, class... Ops>
    [[nodiscard]] auto scan(Tpls<Fields, Ops>... queries) {

        static_assert(std::conjunction_v<detail::is_string_comparable<Fields>...>);
        static_assert(std::conjunction_v<std::is_invocable_r<bool, Ops, bson const&>...>);

        std::vector<document*> result;

        auto check_query = [] (auto&& query, auto&& doc) {
            if (auto const lookup = doc.values().get(std::get<0>(query)); lookup) {
                auto const [field, val] = *lookup;
                if (std::get<1>(query)(val))
                    return true;
            }
            return false;
        };

        for (auto&& doc : docs_) {
            if ((check_query(queries, *doc) && ...))
                result.push_back(doc);
        }
        return result;
    }

    template<class ID>
    optional<document&> insert(ID&& id) {
        auto const [it, inserted] = id_index_.try_emplace(id, nullptr);
        if (inserted) {
            // TODO INSERT INTO APPROPRIATE INDICES
            auto const& doc = docs_.emplace_back(new document(std::forward<ID>(id)));
            it->second = doc;
            return {*doc};
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<document> remove(doc_id const& id) {
        if (auto const it = id_index_.extract(id); it) {
            docs_.erase(std::find(docs_.begin(), docs_.end(), it.mapped()));
            // TODO ERASE ALL OCCURANCES FROM INDICES
            return std::unique_ptr<document>{it.mapped()};
        }
        return nullptr;
    }

    bool erase(doc_id const& id) {
        if (auto const it = id_index_.find(id); it != id_index_.end()) {
            DEBUG_ASSERT(it->second);
            docs_.erase(std::find(docs_.begin(), docs_.end(), it->second));
            delete it->second;
            id_index_.erase(id);
            return true;
        }
        return false;
    }

    [[nodiscard]] optional<document&> lookup(doc_id const& id) {
        if (auto const it = id_index_.find(id); it != id_index_.end())
            return optional<document&>{*(it->second)};
        return {};
    }

    [[nodiscard]] optional<document const&> lookup(doc_id const& id) const {
        if (auto const it = id_index_.find(id); it != id_index_.end())
            return optional<document const&>{*(it->second)};
        return {};
    }

    [[nodiscard]] auto operator[](doc_id const& id) {
        return lookup(id);
    }

    [[nodiscard]] auto operator[](doc_id const& id) const {
        return lookup(id);
    }

    [[nodiscard]] collection_iterator begin() noexcept { return docs_.begin(); }
    [[nodiscard]] collection_const_iterator begin() const noexcept { return docs_.cbegin(); }
    [[nodiscard]] collection_iterator end() noexcept { return docs_.end(); }
    [[nodiscard]] collection_const_iterator end() const noexcept { return docs_.cend(); }
};

} // namespace nova

#endif // COLLECTION_H
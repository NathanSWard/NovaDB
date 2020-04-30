#ifndef NOVA_COLLECTION_HPP
#define NOVA_COLLECTION_HPP

#include <absl/container/flat_hash_map.h>

#include "bson.hpp"
#include "../debug.hpp"
#include "document.hpp"
#include "index.hpp"
#include "index_manager.hpp"
#include "util/map_results.hpp"
#include "util/non_null_ptr.hpp"

#include <tuple>


namespace nova {

class collection;

class collection_iterator {
    decltype(std::declval<std::vector<non_null_ptr<document>>>().begin()) it_;
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
    decltype(std::declval<std::vector<non_null_ptr<document>>>().cbegin()) it_;
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
    std::vector<non_null_ptr<document>> docs_{};
    friend class collection;
public:
    [[nodiscard]] collection_iterator begin() noexcept { return docs_.begin(); }
    [[nodiscard]] collection_const_iterator begin() const noexcept { return docs_.begin(); }
    [[nodiscard]] collection_iterator end() noexcept { return docs_.end(); }
    [[nodiscard]] collection_const_iterator end() const noexcept { return docs_.end(); }
};

class collection {
    std::vector<non_null_ptr<document>> docs_{};
    absl::flat_hash_map<bson, non_null_ptr<document>> id_index_{}; // btree to enforce sorting?
    index_manager index_manager_;

public:
    collection() = default;

    ~collection() {
        for (auto&& ptr : docs_)
            delete ptr;
    }

    template<bool Unique, class Filter = detail::no_filter, class... Fields>
    bool create_index(Fields&&... fields) {
        if (auto result = index_manager_.template create_index<Unique, Filter>(std::forward<Fields>(fields)...); result) {
            // if index was successfully created, insert all documents into the index
            auto& field = result.key();
            auto& index = result.value();
            
            if constexpr (sizeof...(Fields) == 1) {
                for (auto&& doc : docs_)
                    if (auto const found = doc->values().lookup(field); found)
                        index.insert(found.value(), doc);
            }
            else { // multi
                std::vector<non_null_ptr<bson const>> vals;
                for (auto&& doc : docs_) {
                    if (doc->values().contains(field.begin(), field.end())) {
                        vals.reserve(index.field_count());
                        for (auto&& f : field)
                            vals.push_back(std::addressof(doc->values().lookup(f).value()));
                        index.insert(vals, doc);
                        vals.clear();
                    }
                }
            }
            return true;
        }
        return false;
    }

    void print_indices() const noexcept {
        index_manager_.print_indices();
    }

    template<template<class, class> class... Tpls, class... Fields, class... Ops>
    [[nodiscard]] const_cursor scan(Tpls<Fields, Ops>... queries) const {

        static_assert(std::conjunction_v<detail::is_string_comparable<Fields>...>);
        static_assert(std::conjunction_v<std::is_invocable_r<bool, Ops, bson const&>...>);

        std::vector<non_null_ptr<document const>> result;

        auto check_query = [] (auto&& query, auto&& doc) {
            if (auto const lookup = doc.values().lookup(std::get<0>(query)); lookup) {
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
        return multiple_index_lookup_vec{std::move(result)};
    }

    template<class ID>
    optional<document&> insert(ID&& id) {
        static constexpr int _not_a_document{1};
        static constexpr auto _fake_document_pointer = const_cast<document*>(reinterpret_cast<document const*>(&_not_a_document)); 
        if (auto const [it, inserted] = id_index_.try_emplace(id, _fake_document_pointer); inserted) {
            auto const& doc = docs_.emplace_back(new document(std::forward<ID>(id)));
            it->second = doc;
            index_manager_.register_document(doc);
            return {*doc};
        }
        return {};
    }

    optional<document const&> insert(document&& new_doc) {
        static constexpr int _not_a_document{1};
        static constexpr auto _fake_document_pointer = const_cast<document*>(reinterpret_cast<document const*>(&_not_a_document)); 
        if (auto const [it, inserted] = id_index_.try_emplace(new_doc.id(), _fake_document_pointer); inserted) {
            auto const& doc = docs_.emplace_back(new document(std::move(new_doc)));
            it->second = doc;
            index_manager_.register_document(doc);
            return {*doc};
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<document> remove(doc_id const& id) {
        if (auto const it = id_index_.extract(id); it) {
            docs_.erase(std::find(docs_.begin(), docs_.end(), it.mapped()));
            index_manager_.remove_document(it.mapped());
            return std::unique_ptr<document>{it.mapped()};
        }
        return nullptr;
    }

    bool erase(doc_id const& id) {
        if (auto const it = id_index_.find(id); it != id_index_.end()) {
            DEBUG_ASSERT(it->second);
            docs_.erase(std::find(docs_.begin(), docs_.end(), it->second));
            index_manager_.remove_document(it->second);
            delete it->second;
            id_index_.erase(id);
            return true;
        }
        return false;
    }

    [[nodiscard]] optional<document const&> lookup(doc_id const& id) {
        if (auto const it = id_index_.find(id); it != id_index_.end())
            return {*(it->second)};
        return {};
    }

    [[nodiscard]] optional<document const&> lookup(doc_id const& id) const {
        if (auto const it = id_index_.find(id); it != id_index_.end())
            return {*(it->second)};
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

#endif // NOVA_COLLECTION_HPP
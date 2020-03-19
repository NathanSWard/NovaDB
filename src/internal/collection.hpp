#ifndef COLLECTION_H
#define COLLECTION_H

#include "bson.hpp"
#include "../debug.hpp"
#include "document.hpp"
#include "map_results.hpp"
#include "non_null_ptr.hpp"

#include <tuple>
#include <unordered_map>

namespace nova {

class collection {
    std::unordered_map<doc_id, doc_values> docs_{};

public:
    collection() = default;

    template<class... Fields>
    auto scan(Fields&&... fields) {
        std::vector<valid_lookup<doc_id, doc_values>> docs;
        for (auto&& [id, doc] : docs_)
            if ((doc.contains(fields) && ...))
                docs.emplace_back(id, doc);
        return docs;
    }

    template<class ID, class Vals>
    insert_result<doc_id, doc_values> insert(ID&& id, Vals&& vals) {
        auto const [iter, b] = docs_.try_emplace(std::forward<ID>(id), std::forward<Vals>(vals));
        return {iter->first, iter->second, b};
    }

    insert_result<doc_id, doc_values> insert(document&& doc) {
        auto const [iter, b] = docs_.try_emplace(std::move(doc.id()), std::move(doc.values()));
        return {iter->first, iter->second, b};
    }

    insert_result<doc_id, doc_values> insert(document const& doc) {
        auto const [iter, b] = docs_.try_emplace(doc.id(), doc.values());
        return {iter->first, iter->second, b};
    }

    remove_result<doc_id, doc_values> remove(bson const& id) {
        if (auto doc = docs_.extract(id); doc)
            return {std::move(doc.key()), std::move(doc.mapped())};
        return {};
    }

    bool erase(doc_id const& id) {
        return docs_.erase(id) > 0;
    }

    [[nodiscard]] doc_lookup lookup(doc_id const& id) {
        if (auto const doc = docs_.find(id); doc != docs_.end())
            return {doc->first, doc->second};
        return {};
    }

    [[nodiscard]] const_doc_lookup lookup(doc_id const& id) const {
        if (auto const doc = docs_.find(id); doc != docs_.end())
            return {doc->first, doc->second};
        return {};
    }

    [[nodiscard]] auto operator[](doc_id const& id) {
        return lookup(id);
    }

    [[nodiscard]] auto operator[](doc_id const& id) const {
        return lookup(id);
    }

    auto begin() noexcept { return docs_.begin(); }
    auto begin() const noexcept { return docs_.begin(); }
    auto end() noexcept { return docs_.end(); }
    auto end() const noexcept { return docs_.end(); }
};

} // namespace nova

#endif // COLLECTION_H
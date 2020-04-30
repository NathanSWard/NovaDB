#ifndef NOVA_DATABASE_HPP
#define NOVA_DATABASE_HPP

#include <absl/container/flat_hash_map.h>

#include "collection.hpp"
#include "util/map_results.hpp"

#include <memory>
#include <vector>

namespace nova {

class database {
    absl::flat_hash_map<std::string, std::unique_ptr<collection> colls_{};

public:
    database() noexcept = default;

    database(database&&) = default;
    database& operator=(database&&) = default;
    
    database(database const&) = delete;
    database& operator=(database const&) = delete;

    template<class Str>
    [[nodiscard]] bool contains(Str const& str) const {
        return colls_.find(str) != colls_.end();
    }

    template<class Name, class... Args>
    insert_result<std::string, collection> insert(Name&& name, Args&&... args) {
        auto const [iter, b] = colls_.try_emplace(std::forward<Name>(name), nullptr);
        if (b)
            iter->second = std::make_unique<collection>(std::forward<Args>(args)...);
        return {iter->first, iter->second, b};
    }

    template<class Name>
    [[nodiscard]] lookup_result<std::string, collection> lookup(Name const& name) {
        if (auto const coll = colls_.find(name); coll != colls_.end())
            return {coll->first, *(coll->second)};
        return {};
    }

    template<class Name>
    [[nodiscard]] lookup_result<std::string, collection const> lookup(Name const& name) const {
        if (auto const coll = colls_.find(name); coll != colls_.end())
            return {coll->first, *(coll->second)};
        return {};
    }

    template<class Name>
    [[nodiscard]] auto operator[](Name const& name) noexcept {
        lookup(name);
    }

    template<class Name>
    [[nodiscard]] auto operator[](Name const& name) const noexcept {
        lookup(name);
    }
};

} // namespace nova

#endif // NOVA_DATABASE_HPP
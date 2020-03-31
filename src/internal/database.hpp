#ifndef DATABASE_H
#define DATABASE_H

#include "collection.hpp"
#include "map_results.hpp"

#include <memory>
#include <vector>

namespace nova {

class database {
    std::unordered_map<std::string, collection> colls_{};

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
        auto const [iter, b] = colls_.try_emplace(std::string{std::forward<Name>(name)}, std::forward<Args>(args)...);
        return {iter->first, iter->second, b};
    }

    template<class Name>
    [[nodiscard]] lookup_result<std::string, collection> get(Name&& name) {
        if (auto const coll = colls_.find(name); coll != colls_.end())
            return {coll->first, coll->second};
        return {};
    }

    template<class Name>
    [[nodiscard]] lookup_result<std::string, collection const> get(Name&& name) const {
        if (auto const coll = colls_.find(name); coll != colls_.end())
            return {coll->first, coll->second};
        return {};
    }

    template<class Name>
    [[nodiscard]] auto operator[](Name const& name) noexcept {
        get(name);
    }

    template<class Name>
    [[nodiscard]] auto operator[](Name const& name) const noexcept {
        get(name);
    }
};

} // namespace nova

#endif // DATABASE_H
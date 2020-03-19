#ifndef DATABASE_H
#define DATABASE_H

#include "collection.hpp"
#include "map_results.hpp"
#include "optional_ref.hpp"

#include <memory>
#include <vector>

namespace nova {

class database {
    std::unordered_map<std::string, std::unique_ptr<collection>> colls_{};
    std::string name_{};

public:
    database() noexcept = default;

    template<class Name, class... Args, std::enable_if_t<!std::is_same_v<std::decay_t<Name>, database>, int> = 0>
    database(Name&& name, Args&&... args)
        : cols_(std::forward<Args>(args)...)
        , name_(std::forward<Name>(name))
    {}

    database(database&&) = default;
    database& operator=(database&&) = default;
    
    database(database const&) = delete;
    database& operator=(database const&) = delete;

    std::string_view name() const noexcept { return name_; }

    template<class Name, class... Args>
    insert_result<std::string, collection> insert(Name&& name, Args&&... args) {
        auto [iter, b] = colls_.try_emplace(std::string{std::forward<Name>(name)}, std::forward<Args>(args)...);
        return {iter->first, iter->second, b};
    }

    template<class Name>
    optional_ref<collection> get(Name&& name) noexcept {
        if (auto coll = colls_.find(name); coll != colls_.end())
            return {*coll};
        return {};
    }

    template<class Name>
    optional_ref<collection const> get(Name&& name) const noexcept {
        if (auto coll = colls_.find(name); coll != colls_.end())
            return {*coll};
        return {};
    }

    template<class Name>
    auto operator[](Name const& name) noexcept {
        get(name);
    }

    template<class Name>
    auto operator[](Name const& name) const noexcept {
        get(name);
    }
};

} // namespace nova

#endif // DATABASE_H
#ifndef NOVA_INDEX_MANAGER_HPP
#define NOVA_INDEX_MANAGER_HPP

#include "index.hpp"
#include "util/multi_string.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>

namespace nova {

template<class Index>
using single_field_index_map = std::unordered_map<std::string, std::unique_ptr<Index>>;

struct compound_index_map_compare {
    using is_transparent = void;

    bool operator()(std::string_view const sv, multi_string const& ms) const noexcept {
        DEBUG_ASSERT(ms.size() > 0);
        return sv < ms[0];
    }

    bool operator()(multi_string const& ms, std::string_view const sv) const noexcept {
        DEBUG_ASSERT(ms.size() > 0);
        return ms[0] < sv;
    }

    bool operator()(multi_string const& lhs, multi_string const& rhs) const noexcept {
        auto const min = std::min(lhs.size(), rhs.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (lhs[i] == rhs[i])
                continue;
            return lhs[i] < rhs[i];
        }
        return false;
    }
};

template<class Index>
using compound_index_map = std::map<multi_string, std::unique_ptr<Index>, compound_index_map_compare>;

enum class index_type : std::uint8_t {
    single_field_unique,
    single_field_multi,
    compound_unique,
    compound_multi,
};

class index_manager {
    single_field_index_map<single_field_unique_index_interface> sfu_;
    single_field_index_map<single_field_multi_index_interface> sfm_;
    compound_index_map<compound_unique_index_interface> cu_;
    compound_index_map<compound_multi_index_interface> cm_;
public:
};

} // namespace nova

#endif // NOVA_INDEX_MANAGER_HPP
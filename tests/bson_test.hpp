#ifndef NOVA_BSON_TEST_HPP
#define NOVA_BSON_TEST_HPP

#include "../src/internal/unique_id.hpp"
#include "../src/internal/bson.hpp"
#include <cassert>

using namespace nova;

template<class As>
void bson_test_impl(bson const& b, As const& val, bson::types const type) {
    assert(b.type() == type);
    assert(b.template as<As>());
    assert(b.equals_strong(val));
    assert(b.equals_weak(val));
    assert((b == bson{bson_type<As>, val}));
}

void test_bson() {
    bson bson_null(bson_type<bson::null_t>);
    bson_test_impl<bson::null_t>(bson_null, bson::null_t{}, bson::types::Null);

    bson bson_bool(bson_type<bool>, false);
    bson_test_impl<bool>(bson_bool, false, bson::types::Bool);

    bson bson_i32(bson_type<std::int32_t>, -42);
    bson_test_impl<std::int32_t>(bson_i32, -42, bson::types::Int32);

    bson bson_i64(bson_type<std::int64_t>, -42);
    bson_test_impl<std::int64_t>(bson_i64, -42, bson::types::Int64);

    bson bson_u32(bson_type<std::uint32_t>, 42);
    bson_test_impl<std::uint32_t>(bson_u32, 42, bson::types::uInt32);

    bson bson_u64(bson_type<std::uint64_t>, 42);
    bson_test_impl<std::uint64_t>(bson_u64, 42, bson::types::uInt64);

    bson bson_flt(bson_type<float>, 3.14f);
    bson_test_impl<float>(bson_flt, 3.14f, bson::types::Float);

    bson bson_dbl(bson_type<double>, 3.14);
    bson_test_impl<double>(bson_dbl, 3.14, bson::types::Double);

    bson bson_str(bson_type<std::string>, "hello");
    bson_test_impl<std::string>(bson_str, "hello", bson::types::String);

    auto const id = unique_id::generate();
    bson bson_uid(bson_type<unique_id>, id);
    bson_test_impl<unique_id>(bson_uid, id, bson::types::UniqueID);
}

#endif // NOVA_BSON_TEST_HPP
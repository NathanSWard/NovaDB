#pragma once

#include "../src/internal/index_manager.hpp"

using namespace nova;

void test_index_manager() {
    index_manager manager;

    std::array<document*, 0> no_documents;

    // single-field-unique
    assert(manager.create_index<true>("firstname"));
    assert(!manager.create_index<true>("firstname"));

    auto cursor = manager.lookup("firstname");
    assert(cursor);

    for (auto&& [val, doc] : *cursor) {

    }

    // single-field-multi
    manager.create_index<false>("lastname");

    // compound-unique


    // compound-multi
}
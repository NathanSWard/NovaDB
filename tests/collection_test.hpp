#include "../src/internal/collection.hpp"

using namespace nova;

void test_collection() {
    constexpr auto id_a = 1;
    constexpr auto id_b = 2;

    collection c;

    assert(c.insert(id_a));
    assert(!c.insert(id_a));

    auto doc_a = c.lookup(id_a);
    assert(doc_a);
    assert(doc_a->id() == id_a);

    assert(!c.lookup(id_b));

    assert(c.erase(id_a));
    assert(!c.lookup(id_a));
    
    assert(!c.erase(id_b));
}
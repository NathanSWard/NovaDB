#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include "include_test.hpp"

using namespace nova;

int main() {
    test_bson();
    test_document();
    test_collection();    
    test_index_manager();
}


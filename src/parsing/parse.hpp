#ifndef PARSE_H
#define PARSE_H

#include <string_view>

enum class obj_type {
    bson, doc, collection,
};

class parse_command {
    obj_type type;
    std::string_view name;
};

#endif // PARSE_H
#define FMT_HEADER_ONLY
#include "../src/internal/collection.hpp"
#include "../src/internal/query_util.hpp"

using namespace nova;

void calculate_avg_gpa(collection const& c, std::string_view const house) {
    double avg = 0.;
    auto result = c.scan(is_equal_query("house", house));
    if (result.size() > 0) {
        for (auto&& doc : result)
            avg += doc.values().lookup("gpa").value().template as<double>().value();
        avg /= (double)result.size();
    }
    std::cout << fmt::format("average gpa for {}: {}\n", house, avg);
}

int main() {

    document harry_potter(unique_id::generate());
    harry_potter.values().insert("name", "Harry Potter");
    harry_potter.values().insert("house", "Gryffindor");
    harry_potter.values().insert("gpa", 2.9);
    harry_potter.values().insert("classes", std::vector<bson>({"Transfiguration", "Herbology"}));

    document ron_weasley(unique_id::generate());
    ron_weasley.values().insert("name", "Ron Weasley");
    ron_weasley.values().insert("house", "Gryffindor");
    ron_weasley.values().insert("gpa", 2.56);
    ron_weasley.values().insert("classes", std::vector<bson>({"Potions"}));

    document hermonie_granger(unique_id::generate());
    hermonie_granger.values().insert("name", "Hermonie Granger");
    hermonie_granger.values().insert("house", "Gryffindor");
    hermonie_granger.values().insert("gpa", 4.0);
    hermonie_granger.values().insert("classes", std::vector<bson>({"Charms", "Divination", "Potions", "Transfiguration"}));

    document luna_lovegood(unique_id::generate());
    luna_lovegood.values().insert("name", "Luna Lovegood");
    luna_lovegood.values().insert("house", "Ravenclaw");
    luna_lovegood.values().insert("gpa", 3.5);
    luna_lovegood.values().insert("classes", std::vector<bson>({"Divination", "Charms"}));

    document draco_malfoy(unique_id::generate());
    draco_malfoy.values().insert("name", "Draco Malfoy");
    draco_malfoy.values().insert("house", "Slytherine");
    draco_malfoy.values().insert("gpa", 3.12);
    draco_malfoy.values().insert("classes", std::vector<bson>({"Charms", "Transfiguration"}));

    document cho_chang(unique_id::generate());
    cho_chang.values().insert("name", "Cho Chang");
    cho_chang.values().insert("house", "Ravenclaw");
    cho_chang.values().insert("gpa", 3.56);
    cho_chang.values().insert("classes", std::vector<bson>({"Charms", "Divination", "Herbology"}));

    collection hogwart_students;
    hogwart_students.template create_index<false>("house");
    hogwart_students.template create_index<false>("name", "gpa");
    hogwart_students.insert(std::move(harry_potter));
    hogwart_students.insert(std::move(ron_weasley));
    hogwart_students.insert(std::move(hermonie_granger));
    hogwart_students.insert(std::move(luna_lovegood));
    hogwart_students.insert(std::move(draco_malfoy));
    hogwart_students.insert(std::move(cho_chang));

    std::cout << "------------------------------------------\n";
    std::cout << "all indices\n";
    std::cout << "------------------------------------------\n";
    hogwart_students.print_indices();

    std::cout << "------------------------------------------\n";
    std::cout << "avg gpa for each house\n";
    std::cout << "------------------------------------------\n";
    calculate_avg_gpa(hogwart_students, "Gryffindor");
    calculate_avg_gpa(hogwart_students, "Ravenclaw");
    calculate_avg_gpa(hogwart_students, "Slytherine");
    calculate_avg_gpa(hogwart_students, "Hufflepuff");

    std::cout << "------------------------------------------\n";
    std::cout << "all gpa >= 3.0:\n";
    std::cout << "------------------------------------------\n";
    auto gpa_result = hogwart_students.scan(is_greater_eq_query("gpa", 3.));
    for (auto&& doc : gpa_result)
        std::cout << fmt::format("{}\n", doc);

    std::cout << "------------------------------------------\n";
    std::cout << "all students taking Transfiguration:\n";
    std::cout << "------------------------------------------\n";
    auto is_taking_transfiguration = [](auto&& vec) {
        auto const& v = vec.template as<bson::array_t>().value();
        return std::find(v.begin(), v.end(), "Transfiguration") != v.end();
    };

    auto trans_result = hogwart_students.scan(std::make_tuple("classes", is_taking_transfiguration));
    for (auto&& doc : trans_result)
        std::cout << fmt::format("{}\n", doc);

    std::cout << "------------------------------------------\n";
    std::cout << "all students NOT in Griffindor AND taking Transfiguration:\n";
    std::cout << "------------------------------------------\n";
    auto combined_results = hogwart_students.scan(is_not_equal_query("house", "Gryffindor"), 
                                                  std::make_tuple("classes", is_taking_transfiguration));
    for (auto&& doc : combined_results)
        std::cout << fmt::format("{}\n", doc);
}
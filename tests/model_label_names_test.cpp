#include <iostream>
#include <string>
#include <vector>

#include "label_names.h"

namespace
{
bool Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "model_label_names_test: FAIL: " << message << '\n';
        return false;
    }
    return true;
}
} // namespace

int main()
{
    const std::vector<std::string> labels = {"face", "person"};
    std::vector<detection::Object> objects(2);
    objects[0].label = 1;
    objects[1].label = 0;
    std::string error;

    if (!Check(models::AssignLabelNames(labels, objects, error),
               "valid labels should succeed") ||
        !Check(objects[0].label_name == "person",
               "label 1 should map to person") ||
        !Check(objects[1].label_name == "face",
               "label 0 should map to face"))
    {
        return 1;
    }

    std::vector<detection::Object> invalid_objects(2);
    invalid_objects[0].label = 0;
    invalid_objects[0].label_name = "unchanged";
    invalid_objects[1].label = 2;
    if (!Check(!models::AssignLabelNames(labels, invalid_objects, error),
               "out-of-range label should fail") ||
        !Check(error.find("out of range") != std::string::npos,
               "out-of-range error should explain the failure") ||
        !Check(invalid_objects[0].label_name == "unchanged",
               "failed assignment should not partially update objects"))
    {
        return 1;
    }

    std::cout << "model_label_names_test: PASS\n";
    return 0;
}

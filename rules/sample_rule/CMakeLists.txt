# rules/sample_rule/CMakeLists.txt
add_library(sample_rule SHARED sample_rule.cpp)
target_include_directories(sample_rule PRIVATE ../..)
set_target_properties(sample_rule PROPERTIES
    OUTPUT_NAME "libsample_rule"
)
target_link_libraries(sample_rule PUBLIC spdlog dl)

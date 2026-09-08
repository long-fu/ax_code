function(add_ax_test target)
  add_executable(${target} ${ARGN})
  set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_ax_test(model_label_names_test
    model_label_names_test.cpp
    ${CMAKE_SOURCE_DIR}/models/label_names.cpp)
target_link_libraries(model_label_names_test PRIVATE opencv_core)

add_ax_test(scene_config_test
    scene_config_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin/scene_config.cpp)
target_link_libraries(scene_config_test PRIVATE yaml-cpp)

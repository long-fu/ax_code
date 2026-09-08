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

function(add_postprocessor_fake target source)
  add_library(${target} SHARED ${source})
  set_target_properties(${target} PROPERTIES
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/fakes")
endfunction()

add_postprocessor_fake(postprocessor_append_fake
    fakes/postprocessor_append.cpp)
add_postprocessor_fake(postprocessor_fail_fake
    fakes/postprocessor_fail.cpp)
add_postprocessor_fake(postprocessor_bad_version_fake
    fakes/postprocessor_bad_version.cpp)
add_postprocessor_fake(postprocessor_no_factory_fake
    fakes/no_factory.cpp)

add_ax_test(postprocessor_chain_test
    postprocessor_chain_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin/postprocessor_chain.cpp)
add_dependencies(postprocessor_chain_test
    postprocessor_append_fake
    postprocessor_fail_fake
    postprocessor_bad_version_fake
    postprocessor_no_factory_fake)
target_compile_definitions(postprocessor_chain_test PRIVATE
    POSTPROCESSOR_APPEND_PATH="$<TARGET_FILE:postprocessor_append_fake>"
    POSTPROCESSOR_FAIL_PATH="$<TARGET_FILE:postprocessor_fail_fake>"
    POSTPROCESSOR_BAD_VERSION_PATH="$<TARGET_FILE:postprocessor_bad_version_fake>"
    POSTPROCESSOR_NO_FACTORY_PATH="$<TARGET_FILE:postprocessor_no_factory_fake>")
target_link_libraries(postprocessor_chain_test PRIVATE dl opencv_core)

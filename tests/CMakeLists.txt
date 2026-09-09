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
add_postprocessor_fake(postprocessor_null_factory_fake
    fakes/postprocessor_null_factory.cpp)

add_postprocessor_fake(business_plugin_ok_fake
    fakes/business_plugin_ok.cpp)
add_postprocessor_fake(business_plugin_bad_version_fake
    fakes/business_plugin_bad_version.cpp)
add_postprocessor_fake(business_plugin_null_factory_fake
    fakes/business_plugin_null_factory.cpp)

add_ax_test(business_plugin_loader_test
    business_plugin_loader_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin/plugin_loader.cpp)
add_dependencies(business_plugin_loader_test
    business_plugin_ok_fake
    business_plugin_bad_version_fake
    business_plugin_null_factory_fake
    postprocessor_no_factory_fake)
target_compile_definitions(business_plugin_loader_test PRIVATE
    BUSINESS_PLUGIN_OK_PATH="$<TARGET_FILE:business_plugin_ok_fake>"
    BUSINESS_PLUGIN_BAD_VERSION_PATH="$<TARGET_FILE:business_plugin_bad_version_fake>"
    BUSINESS_PLUGIN_NULL_FACTORY_PATH="$<TARGET_FILE:business_plugin_null_factory_fake>"
    BUSINESS_PLUGIN_NO_FACTORY_PATH="$<TARGET_FILE:postprocessor_no_factory_fake>")
target_link_libraries(business_plugin_loader_test PRIVATE ax_runtime dl)

add_ax_test(postprocessor_chain_test
    postprocessor_chain_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin/postprocessor_chain.cpp)
add_dependencies(postprocessor_chain_test
    postprocessor_append_fake
    postprocessor_fail_fake
    postprocessor_bad_version_fake
    postprocessor_no_factory_fake
    postprocessor_null_factory_fake)
target_compile_definitions(postprocessor_chain_test PRIVATE
    POSTPROCESSOR_APPEND_PATH="$<TARGET_FILE:postprocessor_append_fake>"
    POSTPROCESSOR_FAIL_PATH="$<TARGET_FILE:postprocessor_fail_fake>"
    POSTPROCESSOR_BAD_VERSION_PATH="$<TARGET_FILE:postprocessor_bad_version_fake>"
    POSTPROCESSOR_NO_FACTORY_PATH="$<TARGET_FILE:postprocessor_no_factory_fake>"
    POSTPROCESSOR_NULL_FACTORY_PATH="$<TARGET_FILE:postprocessor_null_factory_fake>")
target_link_libraries(postprocessor_chain_test PRIVATE dl opencv_core)

add_ax_test(tracker_processor_test
    tracker_processor_test.cpp
    ${CMAKE_SOURCE_DIR}/plugin/postprocessor_chain.cpp)
set_source_files_properties(tracker_processor_test.cpp
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
add_dependencies(tracker_processor_test tracker_postprocessor)
target_compile_definitions(tracker_processor_test PRIVATE
    TRACKER_POSTPROCESSOR_PATH="$<TARGET_FILE:tracker_postprocessor>")
target_link_libraries(tracker_processor_test PRIVATE dl opencv_core)

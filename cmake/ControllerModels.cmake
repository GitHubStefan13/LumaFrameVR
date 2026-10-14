find_package(Threads REQUIRED)
add_library(controller_model_data STATIC cpp/application/model_data.cpp)
set_target_properties(controller_model_data PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(controller_model_data PUBLIC cpp/application)
add_library(controller_model_runtime STATIC
  cpp/application/model_runtime.cpp cpp/application/model_session.cpp cpp/application/model_transform.cpp
  cpp/application/model_components.cpp cpp/application/model_loading.cpp cpp/application/model_hands.cpp)
target_link_libraries(controller_model_runtime PUBLIC controller_model_data Threads::Threads ${CMAKE_DL_LIBS})
target_include_directories(controller_model_runtime SYSTEM PRIVATE cpp/application/model_vendor)
foreach(target controller_model_data controller_model_runtime)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()

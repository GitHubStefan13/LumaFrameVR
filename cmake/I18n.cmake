add_library(lumaframevr_i18n cpp/i18n/catalogue.cpp)
target_include_directories(lumaframevr_i18n PUBLIC cpp/i18n)
target_compile_options(lumaframevr_i18n PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(lumaframevr_i18n PRIVATE LUMAFRAMEVR_LANG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cpp/i18n/lang")

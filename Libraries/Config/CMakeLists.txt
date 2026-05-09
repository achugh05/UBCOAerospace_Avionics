add_library(config STATIC src/configLoader.cpp)

target_link_libraries(config PRIVATE logging)

target_include_directories(config PUBLIC include)
target_compile_features(config PUBLIC cxx_std_17)
add_library(logging STATIC src/Logger.cpp)

target_include_directories(logging PUBLIC include)
target_compile_features(logging PUBLIC cxx_std_17)
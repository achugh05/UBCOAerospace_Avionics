add_library(logging STATIC src/Logger.cpp)

target_include_directories(communication PUBLIC include)
target_compile_features(communication PUBLIC cxx_std_17)
add_library(serial STATIC
        src/Serial.cpp)
target_include_directories(serial PUBLIC include)
target_compile_features(serial PUBLIC cxx_std_17)


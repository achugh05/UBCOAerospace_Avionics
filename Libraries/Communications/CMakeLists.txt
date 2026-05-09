add_library(communication STATIC
        src/Serial.cpp
        src/NetworkLink.cpp
        src/CommunicationFrameworks.cpp)

target_link_libraries(communication PRIVATE logging)

target_include_directories(communication PUBLIC include)
target_compile_features(communication PUBLIC cxx_std_17)


add_library(video STATIC src/Camera.cpp
        src/VideoLink.cpp)

target_link_libraries(video PRIVATE logging)
target_link_libraries(video PRIVATE config)

target_include_directories(video PUBLIC include)
target_compile_features(video PUBLIC cxx_std_17)
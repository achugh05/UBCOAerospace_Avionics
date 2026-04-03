add_library(systemBase STATIC src/Scheduler.cpp)

target_link_libraries(systemBase PRIVATE communication)
target_link_libraries(systemBase PRIVATE logging)
target_link_libraries(systemBase PRIVATE config)

target_include_directories(systemBase PUBLIC include)
target_compile_features(systemBase PUBLIC cxx_std_17)
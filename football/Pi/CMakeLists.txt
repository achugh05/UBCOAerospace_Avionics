set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(PI main.cpp
        src/DataStructures/PiConfig.cpp
        include/DataStructures/PiConfig.h)

target_link_libraries(PI PRIVATE communication)
target_link_libraries(PI PRIVATE logging)
target_link_libraries(PI PRIVATE config)
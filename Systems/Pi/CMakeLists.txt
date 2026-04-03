set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(PI main.cpp
        src/DataStructures/PiConfig.cpp
        include/DataStructures/PiConfig.h
        src/Scheduler_PI.cpp
        include/Scheduler_PI.h)

target_link_libraries(PI PRIVATE communication)
target_link_libraries(PI PRIVATE logging)
target_link_libraries(PI PRIVATE config)
target_link_libraries(PI PRIVATE systemBase)
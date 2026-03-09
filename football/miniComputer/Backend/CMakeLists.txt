set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(FOOTBALL_PI main.cpp
        Classes/Scheduler.cpp
        Classes/Scheduler.h)

target_link_libraries(FOOTBALL_PI PRIVATE communication)
target_link_libraries(FOOTBALL_PI PRIVATE logging)
target_link_libraries(FOOTBALL_PI PRIVATE config)

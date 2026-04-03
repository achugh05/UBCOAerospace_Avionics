set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(FOOTBALL main.cpp
        src/Scheduler_miniPC.cpp
        include/Scheduler_miniPC.h
        src/DataStructures/Subsystem.cpp
        include/DataStuctures/Subsystem.h
        src/DataStructures/FootballConfig.cpp
        include/DataStuctures/FootballConfig.h)

target_link_libraries(FOOTBALL PRIVATE systemBase)
target_link_libraries(FOOTBALL PRIVATE communication)
target_link_libraries(FOOTBALL PRIVATE logging)
target_link_libraries(FOOTBALL PRIVATE config)

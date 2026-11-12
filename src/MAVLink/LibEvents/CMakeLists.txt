target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        EventHandler.cc
        EventHandler.h
        HealthAndArmingCheckReport.cc
        HealthAndArmingCheckReport.h
        logging.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

#===========================================================================#

CPMAddPackage(
    NAME libevents
    GITHUB_REPOSITORY mavlink/libevents
    GIT_TAG 840a88ea226d4eb0fd4c391ce860317422756435 # main as of 2026-04-07; pinned so builds are reproducible
    SOURCE_SUBDIR libs/cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libevents)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${libevents_SOURCE_DIR}/libs/cpp)

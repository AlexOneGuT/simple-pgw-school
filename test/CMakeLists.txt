enable_testing()

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
option(INSTALL_GMOCK "Install GMock" OFF)
option(INSTALL_GTEST "Install GTest" OFF)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set(TEST "${CMAKE_PROJECT_NAME}_tests")
set(OBJ_LIB "${CMAKE_PROJECT_NAME}_lib")
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
add_executable(${TEST} ${TEST_SOURCES})
target_include_directories(${TEST} PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_link_libraries(${TEST} PRIVATE ${OBJ_LIB} gtest_main gmock)
gtest_discover_tests(${TEST})

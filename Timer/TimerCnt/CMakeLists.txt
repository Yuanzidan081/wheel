file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
add_executable(
  TimerCnt
  ${SOURCES}
)
target_link_libraries(
    TimerCnt
  GTest::gtest_main
)

gtest_discover_tests(TimerCnt)
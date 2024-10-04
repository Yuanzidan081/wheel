file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
add_executable(
  TimerScheduler
  ${SOURCES}
)
target_link_libraries(
  TimerScheduler
  GTest::gtest_main
)

gtest_discover_tests(TimerScheduler)
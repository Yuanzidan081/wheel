add_executable(
  TimerCb
  TimerCb_UnitTest.cpp
  Timercb.cpp
  TimerCb.h
)
target_link_libraries(
  TimerCb
  GTest::gtest_main
)

gtest_discover_tests(TimerCb)
# 单元测试：哈希纯逻辑 + 方案 JSON 往返。仅依赖 das_lib（无 COM 依赖）。

find_package(Qt6 REQUIRED COMPONENTS Test)

set(_test_includes ${CMAKE_SOURCE_DIR}/src)

qt_add_executable(tst_userchoicehash tst_userchoicehash.cpp)
target_include_directories(tst_userchoicehash PRIVATE ${_test_includes})
target_link_libraries(tst_userchoicehash PRIVATE das_lib Qt6::Test)
add_test(NAME tst_userchoicehash COMMAND tst_userchoicehash)

qt_add_executable(tst_profileservice tst_profileservice.cpp)
target_include_directories(tst_profileservice PRIVATE ${_test_includes})
target_link_libraries(tst_profileservice PRIVATE das_lib Qt6::Test)
add_test(NAME tst_profileservice COMMAND tst_profileservice)

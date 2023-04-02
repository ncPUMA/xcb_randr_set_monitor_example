cmake_minimum_required(VERSION 3.5)

project(xcb_randr_set_monitor_example LANGUAGES C)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

find_package(XCB COMPONENTS XCB RANDR REQUIRED)

add_executable(xcb_randr_set_monitor_example main.c)

target_link_libraries(xcb_randr_set_monitor_example ${XCB_LIBRARIES})

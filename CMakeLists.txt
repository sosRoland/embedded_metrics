cmake_minimum_required(VERSION 3.21)
project(embedded_metrics)

set(CMAKE_CXX_STANDARD 20)

find_package(AWSSDK REQUIRED COMPONENTS monitoring)


add_library(aws_emf
        include/json.h
        include/named.h
        include/cw_emf.h)

target_link_libraries(aws_emf PUBLIC ${AWSSDK_LINK_LIBRARIES})
set_target_properties(aws_emf PROPERTIES LINKER_LANGUAGE CXX)


add_executable(${PROJECT_NAME}_test
        tests/catch2.h
        tests/bootstrap.cpp
        tests/emf_tests.cpp)

target_include_directories(${PROJECT_NAME}_test PRIVATE include)

target_link_libraries(${PROJECT_NAME}_test PUBLIC  aws_emf ${AWSSDK_LINK_LIBRARIES})

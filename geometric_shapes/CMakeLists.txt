cmake_minimum_required(VERSION 2.4.6)
include($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)

# Set the build type.  Options are:
#  Coverage       : w/ debug symbols, w/o optimization, w/ code-coverage
#  Debug          : w/ debug symbols, w/o optimization
#  Release        : w/o debug symbols, w/ optimization
#  RelWithDebInfo : w/ debug symbols, w/ optimization
#  MinSizeRel     : w/o debug symbols, w/ optimization, stripped binaries
#set(ROS_BUILD_TYPE RelWithDebInfo)

rosbuild_init()

set(ROS_BUILD_TYPE Debug)

find_package(PkgConfig REQUIRED)

find_package(ASSIMP QUIET)
if (NOT ASSIMP_FOUND)
  pkg_check_modules(ASSIMP assimp)
endif()
if (ASSIMP_FOUND)
  if( ${ASSIMP_VERSION} STRGREATER "2.0.0" )
    set(IS_ASSIMP3 1)
    add_definitions(-DIS_ASSIMP3)
    message(STATUS "Found assimp v3")
  else()
    message(STATUS "Found assimp v2")
  endif()
  include_directories(${ASSIMP_INCLUDE_DIRS})
  link_directories(${ASSIMP_LIBRARY_DIRS})
else()
  message(STATUS "could not find assimp (perhaps available thorugh ROS package?), so assimping assimp v2")
  set(ASSIMP_LIBRARIES assimp)
  set(ASSIMP_LIBRARY_DIRS)
  set(ASSIMP_CXX_FLAGS)
  set(ASSIMP_CFLAGS_OTHER)
  set(ASSIMP_LINK_FLAGS)
  set(ASSIMP_INCLUDE_DIRS)
  set(IS_ASSIMP3 0) # most likely not
endif()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${PROJECT_SOURCE_DIR}/cmake/" ${CMAKE_MODULE_PATH})

find_package(Qhull REQUIRED)
if (HAVE_QHULL_2011)
  add_definitions(-DGEOMETRIC_SHAPES_HAVE_QHULL_2011)
endif()

#set the default path for built libraries to the "lib" directory
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)


rosbuild_add_library(${PROJECT_NAME} src/shape_operations.cpp
				     src/bodies.cpp
				     src/body_operations.cpp)
target_link_libraries(${PROJECT_NAME} assimp ${QHULL_LIBRARIES})


# Unit tests
rosbuild_add_gtest(test_point_inclusion test/test_point_inclusion.cpp)
target_link_libraries(test_point_inclusion ${PROJECT_NAME})


#Install GLFW with cmake .. -DCMAKE_INSTALL_PREFIX=~/ -DBUILD_SHARED_LIBS=1

################################################################
# INITIALIZATION
################################################################
cmake_minimum_required(     VERSION 2.8)
set( CMAKE_BUILD_TYPE       "Release")
set( CMAKE_MACOSX_RPATH     1)
set( CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
project( ogl )
set( CMAKE_CXX_FLAGS        "-std=c++0x -g3 -O2 -w ")
include_directories(        "~/include/" "include/")

################################################################
#Dependencies
################################################################
find_library( common NAMES Commons HINTS "$ENV{HOME}/lib")
if(common)
  message(STATUS "Found Commons: ${common}")
endif()

find_library( glew   NAMES GLEW    HINTS "$ENV{HOME}/lib")
if(glew)
  message(STATUS "Found GLEW: ${glew}")
endif()

find_library( glfw   NAMES glfw    HINTS "$ENV{HOME}/lib" REQUIRED)
if(glfw)
  message(STATUS "Found GLFW: ${glfw}")
endif()

find_package( OpenGL REQUIRED)
find_package( X11    REQUIRED)
find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})

set(CORELIBS ${common} ${glfw} ${OPENGL_LIBRARY} ${X11_LIBRARIES} ${glew} ${FREETYPE_LIBRARIES})

#find_package( GLEW REQUIRED)
#set(CORELIBS ${common} ${glfw} ${OPENGL_LIBRARY} ${X11_LIBRARIES} ${GLEW_LIBRARIES})
#include_directories(${GLEW_INCLUDE_DIRS})

################################################################
#Library
################################################################
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/commons.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/commons.h)
#file(GLOB_RECURSE source_files sources/* include/*)
#add_library(OGL SHARED ${source_files})
#target_link_libraries(OGL ${CORELIBS})

################################################################
#Executable
################################################################
add_executable(        cube main.cpp)
target_link_libraries( cube ${CORELIBS})
install(TARGETS cube RUNTIME DESTINATION "$ENV{HOME}/bin")

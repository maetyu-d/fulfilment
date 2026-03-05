cmake_minimum_required(VERSION 3.18)
project(SkypodSwarm3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)
add_subdirectory(JUCE)

add_executable(fulfilment
  src/main.cpp
)

target_link_libraries(fulfilment PRIVATE raylib)
target_link_libraries(fulfilment PRIVATE
  juce::juce_audio_basics
  juce::juce_dsp
  juce::juce_core
)
target_compile_definitions(fulfilment PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

if (APPLE)
  target_link_libraries(fulfilment PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif (UNIX)
  target_link_libraries(fulfilment PRIVATE m pthread dl rt X11)
endif()

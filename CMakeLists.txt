cmake_minimum_required(VERSION 3.10)
project(min_sud LANGUAGES CXX)

# 1. Locate OpenCV
# Note: "geometry" is not a standard OpenCV component; features are usually split 
# between core, imgproc, highgui, and imgcodecs.
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs)

# 2. Add your executable target
add_executable(${PROJECT_NAME} main.cpp)

# 3. Modern target-based include and link configuration
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

# Optional: Keep your debug status prints
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

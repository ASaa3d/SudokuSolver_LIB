cmake_minimum_required(VERSION 3.10)
project(min_sud LANGUAGES CXX)

set(ONNX_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/onnxruntime")
file(GLOB SOURCES "src/*.cpp")
# 1. Locate OpenCV
# Note: "geometry" is not a standard OpenCV component; features are usually split 
# between core, imgproc, highgui, and imgcodecs.
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs)


# 2. Add your executable target
add_executable(${PROJECT_NAME} ${SOURCES})

# 3. Modern target-based include and link configuration
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS} "${ONNX_RUNTIME_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 4. link libraries
# 4.1 find onnx lib
find_library(ONNX_RUNTIME_LIB 
    NAMES onnxruntime
    PATHS "${ONNX_RUNTIME_DIR}/lib"
    REQUIRED
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} ${ONNX_RUNTIME_LIB})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNX_RUNTIME_DIR}/lib/libonnxruntime.so"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

# Optional: Keep your debug status prints
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
# cmake_minimum_required(VERSION 3.18)
# project(min_sud LANGUAGES CXX)

# option(MIN_SUD_BUILD_CLI "Build the min_sud command-line executable (non-Android only)" ON)

# find_package(OpenCV REQUIRED COMPONENTS core imgproc OPTIONAL_COMPONENTS imgcodecs)
# if(NOT TARGET opencv_imgcodecs AND NOT "imgcodecs" IN_LIST OpenCV_LIB_COMPONENTS)
#     message(WARNING "OpenCV imgcodecs not available in this build (opencv-mobile is minimal): "
#                     "cv::imread/imdecode/imencode will not link.")
# endif()

# find_library(ONNX_RUNTIME_LIB
#     NAMES libonnxruntime.so
#     PATHS "${ONNX_RUNTIME_DIR}/lib/"
#     REQUIRED
# )

# if(ANDROID)
#     add_library(${PROJECT_NAME} SHARED src/sudoku_pipeline.cpp src/jni_bridge.cpp)
#     set(PIPELINE_TARGET ${PROJECT_NAME})
#     target_link_libraries(${PROJECT_NAME} PRIVATE jnigraphics log)
# else()
#     add_library(min_sud_core SHARED src/sudoku_pipeline.cpp)
#     set(PIPELINE_TARGET min_sud_core)
#     if(MIN_SUD_BUILD_CLI)
#         add_executable(${PROJECT_NAME} src/main.cpp)
#         target_link_libraries(${PROJECT_NAME} PRIVATE ${PIPELINE_TARGET})
#     endif()
# endif()

# target_include_directories(${PIPELINE_TARGET} PUBLIC
#     "${CMAKE_CURRENT_SOURCE_DIR}/include"
#     ${OpenCV_INCLUDE_DIRS}
#     PRIVATE "${ONNX_RUNTIME_DIR}/include"
# )
# target_compile_features(${PIPELINE_TARGET} PUBLIC cxx_std_17)
# target_link_libraries(${PIPELINE_TARGET} PUBLIC ${OpenCV_LIBS} PRIVATE ${ONNX_RUNTIME_LIB})

# message(STATUS "OpenCV version: ${OpenCV_VERSION}")

# if(NOT ANDROID)
#     # Linux: position-independent code, and make the .so find any sibling
#     # shared libs (libonnxruntime.so*, libopencv_*.so*) in its own folder.
#     set_target_properties(${PIPELINE_TARGET} PROPERTIES
#         POSITION_INDEPENDENT_CODE ON
#         BUILD_WITH_INSTALL_RPATH ON
#         INSTALL_RPATH "$ORIGIN"
#     )
#     if(MIN_SUD_BUILD_CLI)
#         set_target_properties(${PROJECT_NAME} PROPERTIES
#             BUILD_WITH_INSTALL_RPATH ON
#             INSTALL_RPATH "$ORIGIN"
#         )
#     endif()
# endif()


cmake_minimum_required(VERSION 3.18)

project(min_sud LANGUAGES CXX)

option(MIN_SUD_BUILD_CLI
    "Build the min_sud command-line executable (non-Android only)"
    ON
)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

find_package(OpenCV REQUIRED COMPONENTS core imgproc OPTIONAL_COMPONENTS imgcodecs)

if(NOT TARGET opencv_imgcodecs AND NOT "imgcodecs" IN_LIST OpenCV_LIB_COMPONENTS)
    message(WARNING
        "OpenCV imgcodecs not available in this build (opencv-mobile is minimal): "
        "cv::imread/imdecode/imencode will not link."
    )
endif()

find_library(ONNX_RUNTIME_LIB
    NAMES onnxruntime
    PATHS "${ONNX_RUNTIME_DIR}/lib"
    REQUIRED
)

# --------------------------------------------------
# Platform targets
# --------------------------------------------------

if(ANDROID)

    # Android shared library with JNI
    add_library(${PROJECT_NAME} SHARED
        src/sudoku_pipeline.cpp
        src/jni_bridge.cpp
    )

    set(PIPELINE_TARGET ${PROJECT_NAME})

    target_link_libraries(${PROJECT_NAME} PRIVATE
        jnigraphics
        log
        ${ONNX_RUNTIME_LIB}
    )

else()

    # Linux reusable shared library
    add_library(min_sud_core SHARED
        src/sudoku_pipeline.cpp
    )

    set(PIPELINE_TARGET min_sud_core)

    # Optional Linux CLI
    if(MIN_SUD_BUILD_CLI)
        add_executable(${PROJECT_NAME}
            src/main.cpp
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${PIPELINE_TARGET}
        )
    endif()

endif()

# --------------------------------------------------
# Include directories and compile features
# --------------------------------------------------

target_include_directories(${PIPELINE_TARGET}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${OpenCV_INCLUDE_DIRS}
    PRIVATE
        "${ONNX_RUNTIME_DIR}/include"
)

target_compile_features(${PIPELINE_TARGET}
    PUBLIC cxx_std_17
)

# --------------------------------------------------
# Linking
# --------------------------------------------------

if(ANDROID)

    target_link_libraries(${PIPELINE_TARGET} PRIVATE
        ${OpenCV_LIBS}
    )

else()

    # PUBLIC ensures ONNX Runtime is propagated to consumers
    target_link_libraries(${PIPELINE_TARGET} PUBLIC
        ${OpenCV_LIBS}
        ${ONNX_RUNTIME_LIB}
    )

endif()

# --------------------------------------------------
# Linux runtime paths
# --------------------------------------------------

if(NOT ANDROID)

    set_target_properties(${PIPELINE_TARGET} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "${ONNX_RUNTIME_DIR}/lib"
        INSTALL_RPATH "$ORIGIN"
    )

    if(MIN_SUD_BUILD_CLI)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "${ONNX_RUNTIME_DIR}/lib"
            INSTALL_RPATH "$ORIGIN"
        )
    endif()

endif()

# --------------------------------------------------
# Build information
# --------------------------------------------------

message(STATUS "----------------------------------------")
message(STATUS "Project:          ${PROJECT_NAME}")
message(STATUS "Platform:         ${ANDROID}")
message(STATUS "OpenCV version:   ${OpenCV_VERSION}")
message(STATUS "OpenCV directory: ${OpenCV_DIR}")
message(STATUS "ONNX Runtime:     ${ONNX_RUNTIME_LIB}")

if(NOT ANDROID)
    message(STATUS "Build CLI:        ${MIN_SUD_BUILD_CLI}")
endif()

message(STATUS "----------------------------------------")
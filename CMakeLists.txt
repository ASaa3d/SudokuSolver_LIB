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

# CHANGE 1: new option. When ON, src/jni_bridge.cpp is compiled into libmin_sud_core.so
# so the JVM (Spring Boot) can find Java_com_example_SudokuSolver_MinSud_* (needs JAVA_HOME).
option(MIN_SUD_BUILD_JNI
    "Compile src/jni_bridge.cpp into the Linux library for Java / Spring Boot"
    OFF
)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

find_package(OpenCV REQUIRED COMPONENTS core imgproc OPTIONAL_COMPONENTS imgcodecs)

# CHANGE 2: instead of only warning, remember whether imgcodecs exists. The JNI bridge
# uses this to enable/disable cv::imread (file-path input).
if(TARGET opencv_imgcodecs OR "imgcodecs" IN_LIST OpenCV_LIB_COMPONENTS)
    set(MIN_SUD_HAVE_IMGCODECS ON)
else()
    set(MIN_SUD_HAVE_IMGCODECS OFF)
    message(STATUS "OpenCV imgcodecs not available (minimal opencv-mobile): "
                   "file-path input is disabled, use the byte[] input.")
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

    # CHANGE 3: compile the JNI bridge into the Linux library.
    if(MIN_SUD_BUILD_JNI)
        find_package(JNI REQUIRED)
        target_sources(min_sud_core PRIVATE src/jni_bridge.cpp)
        target_include_directories(min_sud_core PRIVATE ${JNI_INCLUDE_DIRS})
    endif()

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

# CHANGE 4: tell jni_bridge.cpp whether cv::imread is available.
if(MIN_SUD_HAVE_IMGCODECS)
    target_compile_definitions(${PIPELINE_TARGET} PRIVATE MIN_SUD_HAVE_IMGCODECS)
endif()

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

    # CHANGE 5: add $ORIGIN to BUILD_RPATH too. The .so you copy into dist/ (and into the
    # Spring Boot resources) is the build-tree file, so it needs to find libonnxruntime.so
    # next to itself, not only in your /mnt/data/... folder.
    set_target_properties(${PIPELINE_TARGET} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        BUILD_RPATH "$ORIGIN;${ONNX_RUNTIME_DIR}/lib"
        INSTALL_RPATH "$ORIGIN"
    )

    if(MIN_SUD_BUILD_CLI)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "$ORIGIN;${ONNX_RUNTIME_DIR}/lib"
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
message(STATUS "imgcodecs:        ${MIN_SUD_HAVE_IMGCODECS}")

if(NOT ANDROID)
    message(STATUS "Build CLI:        ${MIN_SUD_BUILD_CLI}")
    message(STATUS "Build JNI:        ${MIN_SUD_BUILD_JNI}")
endif()

message(STATUS "----------------------------------------")
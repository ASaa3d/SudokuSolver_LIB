#!/bin/bash

export ANDROID_SDK_ROOT="/home/sa3d/Android/Sdk"
export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/30.0.16248370"
export OPENCV_ANDROID_SDK="/mnt/data/main/Utilities/android/opencv"
export ORT_ANDROID_ROOT="/mnt/data/main/Utilities/android/onnxruntime_android_arm64_1_31_0"

cmake -S . -B build/android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_shared \
  -DOpenCV_DIR="$OPENCV_ANDROID_SDK/sdk/native/jni" \
  -DONNX_RUNTIME_DIR="$ORT_ANDROID_ROOT" \
  -DONNX_RUNTIME_LIB="$ORT_ANDROID_ROOT/lib/libonnxruntime.a" \
  -DCMAKE_BUILD_TYPE=Release
  

cmake --build build/android-arm64 --config Release

"$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" \
  --strip-unneeded build/android-arm64/libmin_sud.so
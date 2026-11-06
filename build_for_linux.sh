#!/bin/bash
# Builds libmin_sud_core.so for Linux x64 (with the JNI bridge for Spring Boot) and
# bundles it with its dependencies (ONNX Runtime + OpenCV) into dist/linux-x64/.
set -euo pipefail

OPENCV_ROOT="/mnt/data/main/Utilities/linux/opencv-mobile-4.14.0-ubuntu-2404"        # folder containing lib/ (or lib64/) and include/
ORT_ROOT="/mnt/data/main/Utilities/linux/onnxruntime-linux-x64-glibc2_17-Release-1.31.0"  # folder containing lib/ and include/

# NEW: JDK folder, needed for jni.h (used when compiling src/jni_bridge.cpp)
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk}"

BUILD_DIR="build/linux-x64"
DIST_DIR="dist/linux-x64"

# Locate OpenCVConfig.cmake automatically
OPENCV_CONFIG="$(find "$OPENCV_ROOT" -name OpenCVConfig.cmake -print -quit)"
if [ -z "$OPENCV_CONFIG" ]; then
    echo "OpenCVConfig.cmake not found under $OPENCV_ROOT" >&2
    exit 1
fi
OPENCV_DIR="$(dirname "$OPENCV_CONFIG")"

if [ ! -d "$ORT_ROOT/lib" ] || [ ! -d "$ORT_ROOT/include" ]; then
    echo "ONNX Runtime lib/ or include/ not found under $ORT_ROOT" >&2
    exit 1
fi

# NEW: make sure the JDK headers exist
if [ ! -f "$JAVA_HOME/include/jni.h" ]; then
    echo "jni.h not found in $JAVA_HOME/include - set JAVA_HOME to a full JDK (not a JRE)" >&2
    exit 1
fi

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOpenCV_DIR="$OPENCV_DIR" \
    -DONNX_RUNTIME_DIR="$ORT_ROOT" \
    -DMIN_SUD_BUILD_CLI=OFF \
    -DMIN_SUD_BUILD_JNI=ON

cmake --build "$BUILD_DIR" --config Release -j"$(nproc)"

# Collect everything into one folder
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
cp -a "$BUILD_DIR/libmin_sud_core.so" "$DIST_DIR/"
strip --strip-unneeded "$DIST_DIR/libmin_sud_core.so"

# ONNX Runtime shared libs (keep symlinks: the SONAME is the versioned name)
shopt -s nullglob
ort_libs=("$ORT_ROOT"/lib/libonnxruntime*.so*)
if [ ${#ort_libs[@]} -gt 0 ]; then
    cp -a "${ort_libs[@]}" "$DIST_DIR/"
else
    echo "No libonnxruntime .so found (static build?) - nothing to copy for ONNX Runtime."
fi

# OpenCV (and any other lib living under OPENCV_ROOT / ORT_ROOT) that the .so needs
copy_deps() {
    ldd "$1" | awk '/=> \//{print $3}' | while read -r dep; do
        case "$dep" in
            "$OPENCV_ROOT"/*|"$ORT_ROOT"/*)
                base="$(basename "$dep")"
                if [ ! -e "$DIST_DIR/$base" ]; then
                    cp -L "$dep" "$DIST_DIR/$base"
                    copy_deps "$DIST_DIR/$base"
                fi
                ;;
        esac
    done
}
LD_LIBRARY_PATH="$OPENCV_ROOT/lib:$OPENCV_ROOT/lib64:$ORT_ROOT/lib:${LD_LIBRARY_PATH:-}" \
    copy_deps "$DIST_DIR/libmin_sud_core.so"

echo
echo "Done. Output folder: $DIST_DIR"
ls -l "$DIST_DIR"
echo
echo "Check for missing deps:"
LD_LIBRARY_PATH="$DIST_DIR" ldd "$DIST_DIR/libmin_sud_core.so" | grep "not found" || echo "  none missing"

# NEW: confirm the JNI functions are in the library
echo
echo "JNI functions exported (expect 4):"
nm -D --defined-only "$DIST_DIR/libmin_sud_core.so" | grep "Java_" || { echo "  NONE - the JNI bridge was not compiled in" >&2; exit 1; }
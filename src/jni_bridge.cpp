// src/jni_bridge.cpp
// JNI layer over the C API in sudoku_pipeline.h.
// Kotlin class: com.example.myapp.MinSud  (rename the package/class in the
// function names below if yours is different).

#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>

#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "sudoku_pipeline.h"

#define LOG_TAG "min_sud"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr size_t kMaxCells = 9 * 9;  // pipeline produces 9x9 or 6x6

void throw_java(JNIEnv* env, const char* cls, const std::string& msg) {
    jclass c = env->FindClass(cls);
    if (c != nullptr) {
        env->ThrowNew(c, msg.c_str());
        env->DeleteLocalRef(c);
    }
}

const char* error_text(int code) {
    switch (code) {
        case -1: return "invalid arguments / unsupported pixel format";
        case -3: return "output buffer too small";
        case -4: return "inference failed";
        default: return "unknown error";
    }
}

// Runs the pipeline and converts the result to Array<IntArray> (size x size).
// Returns nullptr (no exception) when no grid is found, throws on real errors.
jobjectArray run_pipeline(JNIEnv* env, jlong handle, const uint8_t* pixels,
                          int width, int height, MinSudPixelFormat format,
                          size_t stride) {
    auto* pipeline = reinterpret_cast<MinSudPipeline*>(handle);
    if (pipeline == nullptr) {
        throw_java(env, "java/lang/IllegalStateException",
                   "MinSud pipeline is not initialised or was destroyed");
        return nullptr;
    }

    int cells[kMaxCells] = {0};
    int size = 0;
    int rc = min_sud_process(pipeline, pixels, width, height, format, stride,
                             cells, kMaxCells, &size);
    if (rc == -2) return nullptr;  // no sudoku grid in the image
    if (rc != 0) {
        throw_java(env, "java/lang/RuntimeException", error_text(rc));
        return nullptr;
    }

    jclass int_array_cls = env->FindClass("[I");
    jobjectArray out = env->NewObjectArray(size, int_array_cls, nullptr);
    env->DeleteLocalRef(int_array_cls);
    if (out == nullptr) return nullptr;

    for (int r = 0; r < size; ++r) {
        jintArray row = env->NewIntArray(size);
        if (row == nullptr) return nullptr;
        env->SetIntArrayRegion(row, 0, size, cells + r * size);
        env->SetObjectArrayElement(out, r, row);
        env->DeleteLocalRef(row);
    }
    return out;
}

}  // namespace

extern "C" {

// ---- lifecycle -------------------------------------------------------------

JNIEXPORT jlong JNICALL
Java_com_example_sudokusolver_MinSud_nativeCreate(JNIEnv* env, jobject /*thiz*/,
                                           jstring digit_model_path,
                                           jstring yolo_model_path) {
    const char* digit = env->GetStringUTFChars(digit_model_path, nullptr);
    const char* yolo = env->GetStringUTFChars(yolo_model_path, nullptr);

    MinSudPipeline* pipeline = min_sud_create(digit, yolo);

    env->ReleaseStringUTFChars(digit_model_path, digit);
    env->ReleaseStringUTFChars(yolo_model_path, yolo);

    if (pipeline == nullptr) {
        LOGE("min_sud_create failed (check model paths)");
    }
    return reinterpret_cast<jlong>(pipeline);
}

JNIEXPORT void JNICALL
Java_com_example_sudokusolver_MinSud_nativeDestroy(JNIEnv* /*env*/, jobject /*thiz*/,
                                            jlong handle) {
    min_sud_destroy(reinterpret_cast<MinSudPipeline*>(handle));
}

// ---- input: Android Bitmap (ARGB_8888 / RGBA in memory) --------------------

JNIEXPORT jobjectArray JNICALL
Java_com_example_sudokusolver_MinSud_nativeProcessBitmap(JNIEnv* env, jobject /*thiz*/,
                                                  jlong handle, jobject bitmap) {
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
        throw_java(env, "java/lang/IllegalArgumentException", "cannot read bitmap info");
        return nullptr;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        throw_java(env, "java/lang/IllegalArgumentException",
                   "bitmap must be Bitmap.Config.ARGB_8888");
        return nullptr;
    }

    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS ||
        pixels == nullptr) {
        throw_java(env, "java/lang/IllegalArgumentException",
                   "cannot lock bitmap pixels (hardware bitmap?)");
        return nullptr;
    }

    jobjectArray result =
        run_pipeline(env, handle, static_cast<const uint8_t*>(pixels),
                     static_cast<int>(info.width), static_cast<int>(info.height),
                     MIN_SUD_PIXEL_RGBA8, info.stride);

    AndroidBitmap_unlockPixels(env, bitmap);
    return result;
}

// ---- input: raw byte array -------------------------------------------------
// pixel_format uses the MinSudPixelFormat values:
// 1 = GRAY8, 2 = BGR8, 3 = RGB8, 4 = RGBA8, 5 = BGRA8
// row_stride = bytes per row (pass 0 for tightly packed).

JNIEXPORT jobjectArray JNICALL
Java_com_example_sudokusolver_MinSud_nativeProcessBytes(JNIEnv* env, jobject /*thiz*/,
                                                 jlong handle, jbyteArray data,
                                                 jint width, jint height,
                                                 jint pixel_format, jint row_stride) {
    int channels;
    switch (pixel_format) {
        case MIN_SUD_PIXEL_GRAY8: channels = 1; break;
        case MIN_SUD_PIXEL_BGR8:
        case MIN_SUD_PIXEL_RGB8: channels = 3; break;
        case MIN_SUD_PIXEL_RGBA8:
        case MIN_SUD_PIXEL_BGRA8: channels = 4; break;
        default:
            throw_java(env, "java/lang/IllegalArgumentException", "unknown pixel format");
            return nullptr;
    }

    size_t stride = row_stride > 0 ? static_cast<size_t>(row_stride)
                                   : static_cast<size_t>(width) * channels;
    size_t needed = stride * static_cast<size_t>(height);
    if (width <= 0 || height <= 0 ||
        static_cast<size_t>(env->GetArrayLength(data)) < needed) {
        throw_java(env, "java/lang/IllegalArgumentException",
                   "byte array is smaller than width/height/stride imply");
        return nullptr;
    }

    jbyte* buf = env->GetByteArrayElements(data, nullptr);
    if (buf == nullptr) return nullptr;

    jobjectArray result =
        run_pipeline(env, handle, reinterpret_cast<const uint8_t*>(buf), width,
                     height, static_cast<MinSudPixelFormat>(pixel_format), stride);

    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);  // read-only: no copy back
    return result;
}

// ---- input: image file path ------------------------------------------------

JNIEXPORT jobjectArray JNICALL
Java_com_example_sudokusolver_MinSud_nativeProcessFile(JNIEnv* env, jobject /*thiz*/,
                                                jlong handle, jstring image_path) {
    const char* path = env->GetStringUTFChars(image_path, nullptr);
    cv::Mat bgr = cv::imread(path, cv::IMREAD_COLOR);  // honours EXIF rotation
    env->ReleaseStringUTFChars(image_path, path);

    if (bgr.empty()) {
        throw_java(env, "java/io/IOException", "cannot read image file");
        return nullptr;
    }
    return run_pipeline(env, handle, bgr.data, bgr.cols, bgr.rows,
                        MIN_SUD_PIXEL_BGR8, bgr.step);
}

}  // extern "C"

# Sudoku Grid Recognition

A C++/OpenCV/ONNX Runtime pipeline that detects a Sudoku grid in an image, extracts its cells, and recognizes the printed digits. The machine-learning models are trained/exported in Python notebooks and then run by the native C++ pipeline.

> **Scope:** The current C++ pipeline extracts the puzzle into a 6x6 or 9x9 integer grid. It does not solve the Sudoku.

## Demo Video

### End-to-end Sudoku recognition demo

https://github.com/user-attachments/assets/aa1282c7-8ece-4dba-9d6d-c98fcb4c32e2



## Pipeline

1. **Find the grid:** The YOLOv8 model runs first on the input image. C++ resizes and normalizes the image for the ONNX model, then selects the first Sudoku detection whose confidence is above `0.90`.
2. **Rectify the grid:** The detected bounding box is mapped back to the source image and perspective-warped to a 3000x3000 image. If YOLO does not find a grid, the pipeline falls back to OpenCV contour detection and perspective correction.
3. **Preprocess the grid:** The rectified image is converted to grayscale and inverted when needed. Gaussian blur and adaptive thresholding binarize it; horizontal and vertical morphology removes grid lines. Contours are then used to locate candidate cells.
4. **Select the board size:** The pipeline chooses 9x9 when the number of detected cells is closer to 81; otherwise it uses 6x6.
5. **Recognize digits:** Each cell is cropped with a small inset and resized to 28x28 for the CNN. Pixels are normalized to `[0, 1]`. Crops treated as empty and predictions below the `0.85` confidence threshold remain `0`. The CNN has nine output classes, mapped to digits `1` through `9`.
6. **Return the board:** The native API returns the row-major cell values and board width. Empty or unrecognized cells are represented by `0`.

The pipeline keeps both ONNX sessions alive for reuse while processing images.

## Models and training

### Sudoku-grid detector

[`suduko_yolo.ipynb`](./suduko_yolo.ipynb) trains a YOLOv8n model using the dataset configuration at `db/sudoku/data.yaml`:

```python
model = YOLO("yolov8n.pt")
model.train(data="db/sudoku/data.yaml", epochs=10, imgsz=300)
model.export(format="onnx")
```

The detector is exported to ONNX and used by C++ as `models/yolo_sud.onnx`. The C++ inference path consumes the model's detection bounding box to locate the grid; it does not use an instance-segmentation mask.

### Digit recognizer

[`num_cnn.ipynb`](./num_cnn.ipynb) trains the CNN on the digit classes `1`–`9`. Images are converted to grayscale, resized to 28x28, thresholded, and augmented with zoom-in/zoom-out variants. The notebook expects the digit images under `db/data/training_data/<digit>` and `db/data/testing_data/<digit>`.

The CNN architecture is:

| Stage | Layers |
|---|---|
| Input | 28x28x1 grayscale image |
| Feature block 1 | Conv2D(32, 3x3, same) → BatchNorm → ReLU, repeated twice → 2x2 max pool → Dropout(0.25) |
| Feature block 2 | Conv2D(64, 3x3, same) → BatchNorm → ReLU, repeated twice → 2x2 max pool → Dropout(0.25) |
| Classifier | Flatten → Dense(128, ReLU) → BatchNorm → Dropout(0.5) → Dense(9, softmax) |

It is trained with Adam and sparse categorical cross-entropy for 15 epochs. The notebook exports the trained Keras model to TensorFlow Lite. Convert that `.tflite` model to ONNX with `tf2onnx` as a separate step:

```bash
python -m tf2onnx.convert \
  --opset 13 \
  --tflite models/model3.tflite \
  --output models/cnn_dig.onnx
```

The C++ pipeline loads the converted model as `models/cnn_dig.onnx`. The TFLite file is an intermediate export; inference in the C++ application uses ONNX Runtime.

### Exported model files

The default paths used by the C++ CLI are:

- `models/yolo_sud.onnx` — YOLOv8 Sudoku-grid detector
- `models/cnn_dig.onnx` — digit CNN

Keep the model files at those paths when running the CLI, or pass different model file paths to `min_sud_create()`.

### Datasets

- **Grid detector:** [YOLOv8 instance segmentation dataset on Roboflow](https://universe.roboflow.com/brecht/yolov8-instance-segmentation-final)
- **Digit recognizer:** [Standard Fonts Digit Dataset (0–9) on Kaggle](https://www.kaggle.com/datasets/preatcher/standard-fonts-digit-dataset-09). The notebook currently trains output classes `1`–`9`.

**Dataset metadata note:** The checked-in `db/sudoku/data.yaml` currently points to a different Roboflow project (`j-w-kydp4/sudoku-lq9gj`). Check that the local annotations and YAML correspond to the intended Roboflow dataset before retraining.

## C++ API and usage

The public C-compatible API is declared in [`include/sudoku_pipeline.h`](./include/sudoku_pipeline.h):

- `min_sud_create(digit_model_path, yolo_model_path)` loads both ONNX models.
- `min_sud_process(...)` accepts image pixels, dimensions, pixel format, and row stride, and fills a row-major board buffer.
- `min_sud_destroy(...)` releases the pipeline.

Supported pixel formats are grayscale, BGR, RGB, RGBA, and BGRA. The API returns `0` on success; negative status codes indicate invalid input, no grid found, insufficient output capacity, or inference failure.

The command-line example in [`src/main.cpp`](./src/main.cpp) loads `models/cnn_dig.onnx` and `models/yolo_sud.onnx`, reads an image, and prints the recognized board.

## Build shared libraries

CMake builds platform-specific shared libraries. OpenCV and ONNX Runtime must be installed for the target platform and ABI; the Android build cannot use Linux libraries or vice versa.

### Linux x86-64

[`build_for_linux.sh`](./build_for_linux.sh) configures CMake in Release mode, builds the JNI-enabled `libmin_sud_core.so`, strips it, and collects it with its required runtime libraries:

```bash
./build_for_linux.sh
```

Output: `dist/linux-x64/`. The script expects Linux OpenCV and ONNX Runtime 1.31.0 package directories under `/mnt/data/main/Utilities/linux/`; update `OPENCV_ROOT` and `ORT_ROOT` in the script if your dependency locations differ. A JDK is required for the JNI build.

### Android arm64-v8a

[`build_for_android.sh`](./build_for_android.sh) uses the Android NDK toolchain and builds for `arm64-v8a` with Android API level 24:

```bash
./build_for_android.sh
```

Output: `build/android-arm64/libmin_sud.so`. The script expects the Android NDK, OpenCV Android SDK, and ONNX Runtime Android package at the paths configured near the top of the script. It links the ONNX Runtime static archive and strips unnecessary symbols after building.

For an Android app, place the library under `app/src/main/jniLibs/arm64-v8a/` and package the ONNX models as app assets or files. Copy model assets to an accessible filesystem path before passing their paths to `min_sud_create()`. If the library has additional shared-library dependencies for your selected OpenCV package or C++ runtime, package the matching Android `.so` files for the same ABI.

## Repository layout

```text
include/                 Public C API and ONNX/OpenCV helpers
src/                     C++ pipeline, CLI, and JNI bridge
models/                  TFLite intermediate and ONNX model files
db/sudoku/               YOLO dataset configuration and metadata
num_cnn.ipynb            Digit CNN preparation, training, and TFLite export
suduko_yolo.ipynb        YOLO training, evaluation, and ONNX export
build_for_linux.sh        Linux x86-64 CMake build and runtime packaging
build_for_android.sh     Android arm64-v8a CMake build
```

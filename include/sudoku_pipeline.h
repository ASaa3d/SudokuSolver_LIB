#ifndef SUDOKU_PIPELINE_H
#define SUDOKU_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MinSudPipeline MinSudPipeline;

typedef enum MinSudPixelFormat {
    MIN_SUD_PIXEL_GRAY8 = 1,
    MIN_SUD_PIXEL_BGR8 = 2,
    MIN_SUD_PIXEL_RGB8 = 3,
    MIN_SUD_PIXEL_RGBA8 = 4,
    MIN_SUD_PIXEL_BGRA8 = 5
} MinSudPixelFormat;

MinSudPipeline* min_sud_create(const char* digit_model_path, const char* yolo_model_path);

// Returns 0 on success, -1 for invalid arguments, -2 if no grid is found,
// -3 if the output buffer is too small, and -4 if inference fails.
// On success, output_size is the board width and output_cells contains
// output_size * output_size row-major cell values.
int min_sud_process(
    MinSudPipeline* pipeline,
    const uint8_t* pixels,
    int width,
    int height,
    MinSudPixelFormat pixel_format,
    size_t row_stride,
    int* output_cells,
    size_t output_capacity,
    int* output_size);

void min_sud_destroy(MinSudPipeline* pipeline);

#ifdef __cplusplus
}
#endif

#endif
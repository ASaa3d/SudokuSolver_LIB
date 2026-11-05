#include "sudoku_pipeline.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "helpers.h"

struct MinSudPipeline {
    onnx_session digit_session;
    onnx_session yolo_session;

    MinSudPipeline(const char* digit_model_path, const char* yolo_model_path)
        : digit_session(digit_model_path, {1, 28, 28, 1}),
          yolo_session(yolo_model_path, {1, 3, 320, 320}) {}
};

namespace {

bool convert_to_bgr(const uint8_t* pixels, int width, int height,
                    MinSudPixelFormat pixel_format, size_t row_stride,
                    cv::Mat& image) {
    int channels;
    int conversion;
    switch (pixel_format) {
        case MIN_SUD_PIXEL_GRAY8:
            channels = 1;
            conversion = cv::COLOR_GRAY2BGR;
            break;
        case MIN_SUD_PIXEL_BGR8:
            channels = 3;
            conversion = -1;
            break;
        case MIN_SUD_PIXEL_RGB8:
            channels = 3;
            conversion = cv::COLOR_RGB2BGR;
            break;
        case MIN_SUD_PIXEL_RGBA8:
            channels = 4;
            conversion = cv::COLOR_RGBA2BGR;
            break;
        case MIN_SUD_PIXEL_BGRA8:
            channels = 4;
            conversion = cv::COLOR_BGRA2BGR;
            break;
        default:
            return false;
    }

    if (row_stride < static_cast<size_t>(width) * channels) {
        return false;
    }

    int type = CV_MAKETYPE(CV_8U, channels);
    cv::Mat source(height, width, type, const_cast<uint8_t*>(pixels), row_stride);
    if (conversion < 0) {
        image = source.clone();
    } else {
        cv::cvtColor(source, image, conversion);
    }
    return true;
}

bool process_image(MinSudPipeline& pipeline, const cv::Mat& input_image,
                   std::vector<std::vector<int>>& board) {
    cv::Mat sudoku_grid = cv::Mat::zeros(cv::Size(3000, 3000), CV_8UC3);
    cv::Mat image = input_image;

    if (!get_grid_yolo(&pipeline.yolo_session, &image, sudoku_grid) &&
        !get_grid_opencv(&image, sudoku_grid)) {
        return false;
    }

    cv::Mat grid_grey;
    cv::cvtColor(sudoku_grid, grid_grey, cv::COLOR_BGR2GRAY);

    cv::Mat flat = grid_grey.reshape(1, 1).clone();
    std::nth_element(flat.begin<uchar>(), flat.begin<uchar>() + flat.total() / 2,
                     flat.end<uchar>());
    uchar median = flat.at<uchar>(flat.total() / 2);
    if (median < 128) {
        cv::bitwise_not(grid_grey, grid_grey);
    }

    cv::Mat blurred;
    cv::GaussianBlur(grid_grey, blurred, cv::Size(5, 5), 0);

    int block = std::max(11, (grid_grey.cols / 15) | 1);
    cv::Mat grid_thresh;
    cv::adaptiveThreshold(blurred, grid_thresh, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                         cv::THRESH_BINARY, block, 5);

    int len = std::max(15, grid_thresh.cols / 4);
    cv::Mat hk = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(len, 1));
    cv::Mat vk = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, len));
    cv::Mat hlines, vlines, grid_lines;
    cv::morphologyEx(grid_thresh, hlines, cv::MORPH_OPEN, hk);
    cv::morphologyEx(grid_thresh, vlines, cv::MORPH_OPEN, vk);
    cv::bitwise_or(hlines, vlines, grid_lines);
    cv::dilate(grid_lines, grid_lines,
               cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
    cv::subtract(grid_thresh, grid_lines, grid_thresh);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(grid_thresh, contours, hierarchy, cv::RETR_TREE,
                     cv::CHAIN_APPROX_SIMPLE);

    std::vector<cv::Point> child_approx;
    std::vector<cv::Rect> cells;
    double grid_area = grid_thresh.cols * grid_thresh.rows;
    for (int index = 1; index != -1; index = hierarchy[index][0]) {
        double child_area = cv::contourArea(contours[index]);
        if (child_area < grid_area * 0.006 || child_area > grid_area * 0.05) {
            continue;
        }

        double perimeter = cv::arcLength(contours[index], true);
        cv::approxPolyDP(contours[index], child_approx, 0.03 * perimeter, true);
        if (child_approx.size() != 4 || !cv::isContourConvex(child_approx)) {
            continue;
        }

        cv::Rect cell = cv::boundingRect(child_approx);
        float aspect_ratio = static_cast<float>(cell.width) / cell.height;
        if (aspect_ratio >= 0.7f && aspect_ratio <= 1.4f) {
            cells.push_back(cell);
        }
    }

    int size = std::abs(static_cast<int>(cells.size()) - 81) <
                       std::abs(static_cast<int>(cells.size()) - 36)
                   ? 9
                   : 6;
    float cell_width = static_cast<float>(grid_thresh.cols) / size;
    float cell_height = static_cast<float>(grid_thresh.rows) / size;
    board.assign(size, std::vector<int>(size, 0));

    for (const cv::Rect& cell : cells) {
        int col = std::clamp(static_cast<int>((cell.x + cell.width / 2) / cell_width),
                             0, size - 1);
        int row = std::clamp(static_cast<int>((cell.y + cell.height / 2) / cell_height),
                             0, size - 1);
        int margin_x = static_cast<int>(cell.width * 0.1);
        int margin_y = static_cast<int>(cell.height * 0.1);
        cv::Rect inner(cell.x + margin_x, cell.y + margin_y,
                       cell.width - 2 * margin_x, cell.height - 2 * margin_y);
        cv::Mat child_crop = grid_thresh(inner);
        float fill = static_cast<float>(cv::countNonZero(child_crop)) /
                     static_cast<float>(child_crop.total());
        if (fill >= 0.98f) {
            continue;
        }

        std::pair<int, float> result = get_digit(&pipeline.digit_session, &child_crop);
        if (result.second > 0.85f) {
            board[row][col] = result.first;
        }
    }

    return true;
}

}  // namespace

MinSudPipeline* min_sud_create(const char* digit_model_path,
                               const char* yolo_model_path) {
    if (digit_model_path == nullptr || yolo_model_path == nullptr) {
        return nullptr;
    }
    try {
        return new MinSudPipeline(digit_model_path, yolo_model_path);
    } catch (...) {
        return nullptr;
    }
}

int min_sud_process(MinSudPipeline* pipeline, const uint8_t* pixels, int width,
                    int height, MinSudPixelFormat pixel_format,
                    size_t row_stride, int* output_cells,
                    size_t output_capacity, int* output_size) {
    if (pipeline == nullptr || pixels == nullptr || width <= 0 || height <= 0 ||
        output_cells == nullptr || output_size == nullptr) {
        return -1;
    }

    try {
        cv::Mat image;
        if (!convert_to_bgr(pixels, width, height, pixel_format, row_stride, image)) {
            return -1;
        }

        std::vector<std::vector<int>> board;
        if (!process_image(*pipeline, image, board)) {
            return -2;
        }

        size_t required_capacity = board.size() * board.size();
        if (output_capacity < required_capacity) {
            return -3;
        }

        *output_size = static_cast<int>(board.size());
        for (size_t row = 0; row < board.size(); ++row) {
            std::copy(board[row].begin(), board[row].end(),
                      output_cells + row * board.size());
        }
        return 0;
    } catch (...) {
        return -4;
    }
}

void min_sud_destroy(MinSudPipeline* pipeline) {
    delete pipeline;
}
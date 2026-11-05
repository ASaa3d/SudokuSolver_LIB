#include <iostream>
#include <opencv2/imgcodecs.hpp>

#include "sudoku_pipeline.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: ./program <image_path>" << std::endl;
        return -1;
    }
    cv::Mat image = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (image.empty())
    {
        std::cerr << "Could not open image" << std::endl;
        return -1;
    }
    MinSudPipeline* pipeline = min_sud_create("models/cnn_dig.onnx", "models/yolo_sud.onnx");
    if (pipeline == nullptr)
    {
        std::cerr << "Could not initialize ONNX models" << std::endl;
        return -1;
    }

    int board[81] = {};
    int board_size = 0;
    int status = min_sud_process(pipeline, image.data, image.cols, image.rows,
                                 MIN_SUD_PIXEL_BGR8, image.step, board, 81,
                                 &board_size);
    min_sud_destroy(pipeline);

    if (status != 0)
    {
        std::cerr << "Sudoku processing failed with status " << status << std::endl;
        return -1;
    }

    for (int row = 0; row < board_size; ++row)
    {
        for (int col = 0; col < board_size; ++col)
        {
            std::cout << board[row * board_size + col]
                      << (col + 1 == board_size ? '\n' : ' ');
        }
    }
    return 0;
}

#include <iostream>
#include <cstdio>

#include <set>
#include "onnx_session.h"
#include "helpers.h"
#include <algorithm>
#include <iterator>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: ./program <image_path>" << std::endl;
        return -1;
    }
    std::string dig_path = "models/model3.onnx";
    std::vector<int64_t> dig_in_shape = std::vector<int64_t>({1, 28, 28, 1});
    onnx_session dig_session(dig_path,dig_in_shape);

    std::string yolo_path = "models/yolo_sud.onnx";
    std::vector<int64_t> yolo_in_shape = std::vector<int64_t>({1, 3, 320, 320});
    onnx_session yolo_session(yolo_path,yolo_in_shape);
    // Read the image file (adjust path as necessary)
    // IMREAD_COLOR loads the image in BGR 8-bit format
    cv::Mat image = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (image.empty())
    {
        // std::cout << "Could not open or find the image!" << std::endl;
        return -1;
    }
    cv::Mat sudoku_grid = cv::Mat::zeros(cv::Size(3000, 3000), CV_8UC3);

    if(get_grid_yolo(&yolo_session,&image,sudoku_grid))
    {
        // std::cout << "yolo found grid" << std::endl;
    }    
    else if (get_grid_opencv(&image, sudoku_grid))
    {
        // std::cout << "opencv found grid" << std::endl;
        
    }
    else
    {
        // std::cout<< "no grid found"<<std::endl;
        return -1;
    }
    

    // 1. Grayscale
    cv::Mat grid_grey;
    cv::cvtColor(sudoku_grid, grid_grey, cv::COLOR_BGR2GRAY);

    cv::Mat flat = grid_grey.reshape(1, 1).clone();
    std::nth_element(flat.begin<uchar>(), flat.begin<uchar>() + flat.total() / 2, flat.end<uchar>());
    uchar median = flat.at<uchar>(flat.total() / 2);

    if (median < 128)
        cv::bitwise_not(grid_grey, grid_grey);   // now dark ink on light background

    // 2. Blur (3x3 keeps thin digits; use 5x5 if the image is noisy)
    cv::Mat blurred;
    cv::GaussianBlur(grid_grey, blurred, cv::Size(5, 5), 0);

    // 3. Adaptive threshold (block size scales with image width, must be odd)
    int block = std::max(11, (grid_grey.cols / 15) | 1);
    cv::Mat grid_thresh;
    cv::adaptiveThreshold(blurred, grid_thresh, 255,
                        cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY,
                        block, 5);


    // 4. Keep only long horizontal/vertical strokes = the grid lines
    int len = std::max(15, grid_thresh.cols / 4); // longer than any cell, so digits drop out
    cv::Mat hk = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(len, 1));
    cv::Mat vk = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, len));
    cv::Mat hlines, vlines, grid_lines;
    cv::morphologyEx(grid_thresh, hlines, cv::MORPH_OPEN, hk);
    cv::morphologyEx(grid_thresh, vlines, cv::MORPH_OPEN, vk);
    cv::bitwise_or(hlines, vlines, grid_lines);

    // 5. Remove the lines from the threshold, leaving digits only
    cv::dilate(grid_lines, grid_lines,
    cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
    cv::subtract(grid_thresh, grid_lines, grid_thresh);

    
    // find_contours
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(grid_thresh, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    std::vector<cv::Point> childApprox;
    double child_peri, child_area, fill;
    int size, col, row, mx, my;
    double grid_area = grid_thresh.cols * grid_thresh.rows;
    float ar, cw, ch;
    std::unique_ptr<std::vector<std::vector<int>>> board;
    std::vector<cv::Rect> cells;
    cv::Rect r;
    std::pair<int,float> result;



    for (int idx = 1; idx != -1; idx = hierarchy[idx][0])
    {
        child_area = cv::contourArea(contours[idx]);
        if (child_area < grid_area * 0.006 || child_area > grid_area * 0.05) continue;   // covers 9x9 (1.2%) and 6x6 (2.8%)

        child_peri = cv::arcLength(contours[idx], true);
        cv::approxPolyDP(contours[idx], childApprox, 0.03 * child_peri, true);
        if (childApprox.size() != 4 || !cv::isContourConvex(childApprox)) continue;

        r = cv::boundingRect(childApprox);
        ar = (float)r.width / r.height;
        if (ar < 0.7f || ar > 1.4f) continue;                           
        // cells are roughly square
        
        cells.push_back(r);
    }

    // Size from cell count (midpoint of 36 and 81 is 58.5)
    size = (std::abs((int)cells.size() - 81) < std::abs((int)cells.size() - 36)) ? 9 : 6;
    cw = (float)grid_thresh.cols / size;
    ch = (float)grid_thresh.rows / size;
    board = std::make_unique<std::vector<std::vector<int>>>(size, std::vector<int>(size, 0));

    // Place each cell by its centre, not by iteration order

    for (const cv::Rect& r : cells)
    {
        // cv::rectangle(sudoku_grid,r,cv::Scalar(0, 255, 0), 8);

        col = std::clamp((int)((r.x + r.width  / 2) / cw), 0, size - 1);
        row = std::clamp((int)((r.y + r.height / 2) / ch), 0, size - 1);
        // r is now the cell at (row, col): crop the digit from it here
        mx = r.width  * 0.1;
        my = r.height * 0.1;
        cv::Rect inner(r.x + mx, r.y + my, r.width - 2*mx, r.height - 2*my);
        cv::Mat child_crop = grid_thresh(inner);
        fill = (double)cv::countNonZero(child_crop) / child_crop.total();   // 0..1
        if (fill >= 0.98)
            continue;

    
        result = get_digit(&dig_session,&child_crop);

        if (result.second > 0.85)
            (*board)[row][col] = result.first;
    }
    
    return 0;
}

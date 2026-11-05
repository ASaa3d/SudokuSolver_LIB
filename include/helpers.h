#ifndef HELPERS_H
#define HELPERS_H
#include <vector>
#include <opencv2/geometry/2d.hpp>
// #include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include "onnx_session.h"


void warp_contour(cv::Mat* input_image, std::vector<cv::Point2f> *pts_src, cv::Mat &warped_image)
{
    // 4. Define your destination coordinates
    float width = warped_image.cols;
    float height = warped_image.rows;
    std::vector<cv::Point2f> pts_dst = {
        cv::Point2f(0, 0),
        cv::Point2f(width, 0),
        cv::Point2f(0, height),
        cv::Point2f(width, height)
    };

    // 5. Generate matrix and warp (ensure point sorting matches!)
    cv::Mat matrix = cv::getPerspectiveTransform(*pts_src, pts_dst);
    cv::warpPerspective(*input_image, warped_image, matrix, cv::Size(width, height));

}



std::vector<Ort::Value> get_out_tensor(onnx_session* dig_onnx_session,cv::Mat* in_tensor)
{

   
    // 3. Extract dimensions matching Keras [Batch, Height, Width, Channels] layout
    int channels      = static_cast<int>(dig_onnx_session->get_input_shape()->at(0)); // 1
    int target_height = static_cast<int>(dig_onnx_session->get_input_shape()->at(1)); // 28
    int target_width  = static_cast<int>(dig_onnx_session->get_input_shape()->at(2)); // 28

    // 4. Resize the already grayscaled/thresholded image to 28x28
    cv::Mat resized_img;
    cv::resize(*in_tensor, resized_img, cv::Size(target_width, target_height));

    // cv::imshow("asd",resized_img);
    // 5. Convert to 32-bit Float and scale [0.0, 1.0] (Matches: array / 255.0)
    cv::Mat float_img;
    resized_img.convertTo(float_img, CV_32FC1, 1.0f / 255.0f);

    // 6. Flatten the contiguous continuous single-channel block buffer
    std::vector<float> input_tensor_values;
    if (float_img.isContinuous()) {
        input_tensor_values.assign((float*)float_img.datastart, (float*)float_img.dataend);
    } else {
        for (int i = 0; i < float_img.rows; ++i) {
            input_tensor_values.insert(input_tensor_values.end(), 
                                       float_img.ptr<float>(i), 
                                       float_img.ptr<float>(i) + float_img.cols);
        }
    }

    // 7. Create Memory Info and Wrap Input Data into an Ort::Value
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor_values.data(),
        input_tensor_values.size(),
        dig_onnx_session->get_input_shape()->data(),
        dig_onnx_session->get_input_shape()->size()
    );

    // 8. Specify Node Names
    const char* input_names[] = {dig_onnx_session->get_in_name()};
    const char* output_names[] = {dig_onnx_session->get_out_name()};

    // 9. Run Inference
    std::vector<Ort::Value> output_tensors = dig_onnx_session->get_session()->Run(
        Ort::RunOptions{nullptr}, 
        input_names, 
        &input_tensor, 
        1, 
        output_names, 
        1  
    );
    return output_tensors;
}


// Pass the session BY REFERENCE instead of recreating it every frame!
std::vector<Ort::Value> get_yolo_out_tensor(onnx_session* yolo_onnx_session, cv::Mat* in_tensor)
{
   
    // YOLO v8 expects: [Batch=1, Channels=3, Height=640, Width=640]
    int channels      = static_cast<int>(yolo_onnx_session->get_input_shape()->at(1)); // Index 1 is channels (3)
    int target_height = static_cast<int>(yolo_onnx_session->get_input_shape()->at(2)); // Index 2 is height (640)
    int target_width  = static_cast<int>(yolo_onnx_session->get_input_shape()->at(3)); // Index 3 is width (640)

    // 1. Convert source image to RGB (YOLO was trained on RGB, OpenCV defaults to BGR)
    cv::Mat rgb_img;
    cv::cvtColor(*in_tensor, rgb_img, cv::COLOR_BGR2RGB);

    // 2. Resize to YOLO standard dimension
    cv::Mat resized_img;
    cv::resize(rgb_img, resized_img, cv::Size(target_width, target_height));

    // 3. Convert to 32-bit Float and normalize to [0.0, 1.0]
    cv::Mat float_img;
    resized_img.convertTo(float_img, CV_32FC3, 1.0f / 255.0f);

    // 4. Force NCHW format (Separate continuous channel spaces)
    // Allocates memory for entire input tensor size (e.g., 1 * 3 * 640 * 640)
    std::vector<float> input_tensor_values(1 * channels * target_height * target_width);
    
    // Map individual vector positions to distinct cv::Mat channel headers
    std::vector<cv::Mat> chw_channels;
    for (int i = 0; i < channels; ++i) {
        chw_channels.emplace_back(
            target_height, 
            target_width, 
            CV_32FC1, 
            &(input_tensor_values[i * target_height * target_width])
        );
    }
    
    // Split the continuous interleaved HWC image into the sequential NCHW vector
    cv::split(float_img, chw_channels);

    // 5. Wrap memory into an ONNX tensor wrapper
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, 
        input_tensor_values.data(),     
        input_tensor_values.size(),     
        yolo_onnx_session->get_input_shape()->data(),
        yolo_onnx_session->get_input_shape()->size()
    );

    // 6. Run Inference
    const char* input_names[] = {yolo_onnx_session->get_in_name()};
    const char* output_names[] = {yolo_onnx_session->get_out_name()};

    return yolo_onnx_session->get_session()->Run(
        Ort::RunOptions{nullptr}, 
        input_names, 
        &input_tensor, 
        1, 
        output_names, 
        1  
    );
}

std::pair<int,float> get_digit(onnx_session* dig_onnx_session ,cv::Mat* in_tensor)
{
   std::vector<Ort::Value> output_tensors = get_out_tensor(dig_onnx_session,in_tensor);

    // 10. Extract Softmax probabilities as 32-bit floats
    size_t output_element_count = output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount();
    auto* raw_data = output_tensors[0].GetTensorMutableData<float>();
    
    std::vector<float> out_tensor(raw_data, raw_data + output_element_count);
    auto max_it = std::max_element(out_tensor.begin(), out_tensor.end());
    int argmax = std::distance(out_tensor.begin(), max_it);
    // printf("(%d,%d), number = %d, confidence = %.2f%\n", cR, cC, argmax + 1, out_tensor[argmax] * 100);
    return std::pair<int,float>(argmax+1,out_tensor[argmax]);
}


bool get_yolo_bounding_rect(onnx_session* yolo_onnx_session, cv::Mat* in_tensor, std::vector<cv::Point2f> &result )
{
    std::vector<Ort::Value> output_tensors = get_yolo_out_tensor(yolo_onnx_session,in_tensor);

    // Parse the Output Tensor
    float* raw_output = output_tensors[0].GetTensorMutableData<float>();
    auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    
    // For YOLOv8 single-class, output_shape is typically [1, 5, 8400]
    int64_t rows = output_shape[1]; // 5 (x, y, w, h, score)
    int64_t cols = output_shape[2]; // 8400 (anchor boxes)
    // printf("number of cols found: %d\n",cols);
    float conf_threshold = 0.90;

    for (int c = 0; c < cols; ++c) {
        // The 5th element (index 4) contains the single-class accuracy/confidence
        // Array mapping: row * cols + col
        float accuracy = raw_output[4 * cols + c];
        if (accuracy > conf_threshold)
        {
            float x_center = raw_output[0 * cols + c];
            float y_center = raw_output[1 * cols + c];
            float width    = raw_output[2 * cols + c];
            float height   = raw_output[3 * cols + c];
            result.push_back(cv::Point2f(x_center-width/2.0,y_center-height/2.0));
            result.push_back(cv::Point2f(x_center+width/2.0,y_center-height/2.0));
            result.push_back(cv::Point2f(x_center-width/2.0,y_center+height/2.0));
            result.push_back(cv::Point2f(x_center+width/2.0,y_center+height/2.0));
            return true;
        }
 
    }
    return false;
}



bool get_grid_opencv(cv::Mat* input_image , cv::Mat &warped_image)
{
    double imageArea = input_image->cols * input_image->rows;
    // grayscale
    cv::Mat img_grey;
    cv::cvtColor(*input_image,img_grey,cv::COLOR_BGR2GRAY);

    // normalize grayscale
    cv::Mat norm_img_grey;
    cv::normalize(img_grey,norm_img_grey, 0, 255, cv::NORM_MINMAX,CV_8U);

    // threshholding
    cv::Mat img_thresh;
    cv::threshold(norm_img_grey,img_thresh,220,255,cv::THRESH_BINARY);

    // find_contours
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(img_thresh,contours,hierarchy,cv::RETR_TREE,cv::CHAIN_APPROX_SIMPLE);
    double candidateArea,perimeter;
    std::vector<cv::Point> approx;

    for (size_t i=1;i<contours.size();i++)
    {
        candidateArea = cv::contourArea(contours[i]);
        if (candidateArea < imageArea * 0.1 || candidateArea > imageArea * 0.9) continue;
        perimeter = cv::arcLength(contours[i],true);
        cv::approxPolyDP(contours[i],approx,0.04*perimeter,true);

        if (approx.size()==4)
        {
            //Convert std::vector<cv::Point> to std::vector<cv::Point2f>
            std::vector<cv::Point2f> pts_src; 
            pts_src.push_back(cv::Point2f(approx[0].x, approx[0].y));
            pts_src.push_back(cv::Point2f(approx[1].x, approx[1].y));
            pts_src.push_back(cv::Point2f(approx[3].x, approx[3].y));
            pts_src.push_back(cv::Point2f(approx[2].x, approx[2].y));

            warp_contour(input_image,&pts_src,warped_image);
            return true;
        }
    }
    return false;

}


bool get_grid_yolo(onnx_session* yolo_onnx_session,cv::Mat* in_tensor, cv::Mat &warped_image)
{
    std::vector<cv::Point2f> bounding_rect;
    if(get_yolo_bounding_rect(yolo_onnx_session,in_tensor,bounding_rect))
    {
        for(int i=0;i<4;i++)
        {
            bounding_rect[i] = cv::Point2f(bounding_rect[i].x * in_tensor->cols/yolo_onnx_session->get_input_shape()->at(2),bounding_rect[i].y * in_tensor->rows/yolo_onnx_session->get_input_shape()->at(3));
        }
        warp_contour(in_tensor,&bounding_rect,warped_image);
        return true;
    }
    return false;
}





#endif
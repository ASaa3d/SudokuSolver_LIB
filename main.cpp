#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdio>
#include <opencv2/geometry/2d.hpp>
#include <set>
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: ./program <image_path>" << std::endl;
        return -1;
    }
    cv::Mat template_img = cv::imread("media/template2.png");
    // cv::Mat dst;
    cv::Mat result_img;
    double min_val; 
    double max_val;
    cv::Point min_loc; 
    cv::Point max_loc;
    cv::Point match_loc;
    // // Quarter the image size (scale factor of 0.5 for width and height)
    // cv::resize(src, dst, cv::Size(), 0.25, 0.25, cv::INTER_AREA);

    // // Save or display the result
    // cv::imwrite("template2.png", dst);


    // Read the image file (adjust path as necessary)
    // IMREAD_COLOR loads the image in BGR 8-bit format
    cv::Mat image = cv::imread(argv[1], cv::IMREAD_COLOR);
    double imageArea = image.cols * image.rows;
    // std::printf("imageArea = %d\n",imageArea);
    // Check if the image opened successfully
    if (image.empty()) {
        std::cout << "Could not open or find the image!" << std::endl;
        return -1;
    }
    // grayscale
    cv::Mat img_grey;
    cv::cvtColor(image,img_grey,cv::COLOR_BGR2GRAY);

    // normalize grayscale
    cv::Mat norm_img_grey;
    cv::normalize(img_grey,norm_img_grey, 0, 255, cv::NORM_MINMAX,CV_8U);
    // cv::GaussianBlur(img_grey, norm_img_grey, cv::Size(15, 15), 0);

    // threshholding
    cv::Mat img_thresh;
    cv::threshold(norm_img_grey,img_thresh,220,255,cv::THRESH_BINARY);

    // find_contours
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(img_thresh,contours,hierarchy,cv::RETR_TREE,cv::CHAIN_APPROX_SIMPLE);
    
    // draw contours
    cv::Mat image_clone = image.clone();
    //cv::drawContours(image_clone, contours, -1, cv::Scalar(0, 255, 0), 2);
    std::vector<cv::Point> approx;
    std::vector<std::vector<cv::Point>> approxss;
    std::vector<cv::Point> childApprox;
    std::set<int> mySet={};
    int small_sud_count = 0;
    int big_sud_count = 0;
    double peri;
    double child_peri;
    int rect_child_count;
    int childIdx;
    int xMin;
    int xMax;
    int yMin;
    int yMax;
    double cX;
    double cY;
    int cR;
    int cC;
    int size = 0;
    int nextChildIdx;
    double childArea;
    // window init
    cv::namedWindow("asdasd",cv::WINDOW_NORMAL);
    int inner_rect_counts=0;
    for (size_t i=1;i<contours.size();i++)
    {
        double parentArea= cv::contourArea(contours[i]);
        if (parentArea < imageArea * 0.1 || parentArea > imageArea * 0.9) continue;
        // std::printf("contourArea = %d\n",parentArea);
        peri = cv::arcLength(contours[i],true);
        cv::approxPolyDP(contours[i],approx,0.165*peri,true);
        auto it = mySet.find(i);
        // std::printf("approx size = %d and convex = %d \n",approx.size() , cv::isContourConvex(contours[i]));
        if (approx.size()==4  && it == mySet.end())
        {
            xMin = std::min({approx[0].x, approx[1].x, approx[2].x, approx[3].x});
            xMax = std::max({approx[0].x, approx[1].x, approx[2].x, approx[3].x});
            yMin = std::min({approx[0].y, approx[1].y, approx[2].y, approx[3].y});
            yMax = std::max({approx[0].y, approx[1].y, approx[2].y, approx[3].y});
            
            cv::drawContours(image_clone,contours,i,cv::Scalar(0,255,0),2);
            childIdx = hierarchy[i][2];
            rect_child_count=0;
            while (childIdx != -1)
            {
                // Capture the next sibling index
                nextChildIdx = hierarchy[childIdx][0]; 

                // 1. Filter out tiny noise contours
                childArea = cv::contourArea(contours[childIdx]);
                
                if ( childArea < parentArea * 0.009 || childArea > parentArea * 0.2 ) {
                    // printf("skipped due to small area\n");
                    childIdx = nextChildIdx; // Move to next sibling
                    continue;
                }
                else if (size == 0) 
                {
                    if (childArea > parentArea * (0.8/100) && childArea < parentArea * (1.5/100))
                    {
                        size = 9;
                        
                    }
                    else if (childArea >= parentArea * (1.5/100) && childArea < parentArea * (5.0/100))
                    {
                        size = 6;
                    }
                }
                // printf("child/parent ratio = %f\n" , childArea/parentArea);
                rect_child_count++;

                // 2. Shape Approximation (Fixed epsilon from 0.6 to 0.04)
                child_peri = cv::arcLength(contours[childIdx], true);
                cv::approxPolyDP(contours[childIdx], childApprox, 0.04 * child_peri, true);
                
                // 3. Convexity check on 'childApprox', not the raw contour
                if (childApprox.size() == 4 && cv::isContourConvex(childApprox))
                {
                    cX = (childApprox[0].x + childApprox[1].x + childApprox[2].x + childApprox[3].x) / 4.0;
                    cY = (childApprox[0].y + childApprox[1].y + childApprox[2].y + childApprox[3].y) / 4.0;
                    cC = ((cX-xMin)*size )/(xMax-xMin);
                    cR = ((cY-yMin)*size )/(yMax-yMin);

                    std::printf("(%d, %d)\n",cR,cC);
                    // for(cv::Point point : childApprox)
                    // {
                    //     std::printf("x= %d,y= %d\n",point.x,point.y);
                    // }
                    cv::drawContours(image_clone, contours, childIdx, cv::Scalar(0, 0, 255), 2);
                    
                    // std::printf("parentIdx:%d, childIdx:%d\n", i, childIdx);
                    mySet.insert(childIdx);
                    
                    // cv::matchTemplate(source_img, template_img, result_img, match_method);
                    // cv::minMaxLoc(result_img, &min_val, &max_val, &min_loc, &max_loc, cv::Mat());
                    // match_loc = max_loc;
                    // std::cout << "Match Confidence Score: " << (max_val) << std::endl;
                    
                    // // 7. Draw a rectangle highlighting the detected region on the source image
                    // cv::Point bottom_right(match_loc.x + template_img.cols, match_loc.y + template_img.rows);
                    // cv::rectangle(template_img, match_loc, bottom_right, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
                    cv::imshow("asdasd",image_clone);
                    // Wait for a keystroke in the window (0 means wait indefinitely)
                    cv::waitKey(0);
                    
                    
                    // // 8. Display the results
                    // cv::imshow("Detected Template Location", template_img);
                    inner_rect_counts++;

                }
                
                // Crucial: Seamlessly advance to the next sibling regardless of match outcome
                childIdx = nextChildIdx; 
            }
        }
    }

    std::printf("%d\n",inner_rect_counts);
    cv::imshow("asdasd",image_clone);

    // Wait for a keystroke in the window (0 means wait indefinitely)
    cv::waitKey(0);
    return 0;
}

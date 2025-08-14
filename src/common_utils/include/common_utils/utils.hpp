#pragma once
#include <functional>
#include <chrono>
#include <future>
#include <cstdio>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>


// 角度弧度转换
inline float rad2deg(float rad) { return rad * 180 / M_PI;}
inline float deg2rad(float deg) { return deg * M_PI / 180;}

// 判断旋转矩阵或旋转向量
inline int rotationType(cv::Mat& R){
    if (R.rows == 3 && R.cols == 3)
        return 9;
    else if (R.rows == 3 && R.cols == 1)
        return 3;
    else
        return 0;
}

// 将数字转换为指定宽度的字符串，不足的部分用 0 填充，用于文件读取和保存的命名
inline std::string int2StringZeros(int value, int width = 6) {
    char buffer[width+2];
    std::snprintf(buffer, sizeof(buffer), "%0*d", width, value);
    return std::string(buffer);
}
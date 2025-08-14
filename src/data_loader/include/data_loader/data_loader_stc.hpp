#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <csignal>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm> // 如果使用标准库算法，如std::sort或std::find
#include <opencv2/opencv.hpp>
#include <boost/filesystem.hpp>

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <sensor_msgs/Image.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>

#include <common_utils/utils.hpp>

// 数据加载类
class DataLoaderSTC
{
private:
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    ros::Timer timer_;
    image_transport::Publisher insta360_pano_pub_;
    image_transport::Publisher ladybug_pano_pub_;

    // 配置参数
    int sceneNums, playScene;
    int startInsta360Idx;
    double pubHz, offsetTime;
    std::string datasetPath;
    std::string insta360Path, ladybugPath;
    std::string insta360Csv, ladybugCsv;

    std::vector<std::map<int, double>> insta360Timestamps;  // 每个场景的帧序号与时间戳
    std::vector<std::map<double, int>> ladybugTimestamps;  // 每个场景的时间戳与帧序号

    // 当前访问的场景
    int Scene_ = -1;
    int totalInsta360Frames_ = -1;
    int totalLadybugFrames_ = -1;
    int currentInsta360Frame_ = -1;
    int currentLadybugFrame_ = -1;


public:
    // 构造函数
    DataLoaderSTC();

public:
    bool readCameraCsv();
    bool setScene(int sceneNum);
    bool getInsta360Frame(int idx, cv::Mat& frame);
    bool getLadybugFrame(int idx, cv::Mat& frame);
    bool readScene(int idx, double& frameTimestamp, cv::Mat& insta360Frame, cv::Mat& ladybugFrame);
    bool releaseScene();
    void run(const ros::TimerEvent& event);
};
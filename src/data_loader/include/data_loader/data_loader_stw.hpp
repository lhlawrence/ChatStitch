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
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/TransformStamped.h>

#include <common_utils/utils.hpp>



// 组合导航数据结构
class NavigationData 
{
public:
    double timestamp;  // 时间戳
    double longitude;  // 经度
    double latitude;   // 纬度
    double altitude;   // 高度
    double eightVel;  // 东向速度
    double northVel;  // 北向速度
    double upVel;     // 天向速度
    double roll;      // 横滚角
    double pitch;     // 俯仰角
    double yaw;       // 偏航角

    // 默认构造函数和带参构造函数
    NavigationData(double ts, double lat, double lon, double alt, double eVel, double nVel, double uVel, double r, double p, double y)
        : timestamp(ts), latitude(lat), longitude(lon), altitude(alt), eightVel(eVel), northVel(nVel), upVel(uVel), roll(r), pitch(p), yaw(y) 
        {}

    // 格式化输出
    std::string print() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << "Timestamp: " << timestamp 
            << ", Latitude: " << latitude 
            << ", Longitude: " << longitude 
            << ", Altitude: " << altitude 
            << ", EightVel: " << eightVel
            << ", NorthVel: " << northVel
            << ", UpVel: " << upVel
            << ", Roll: " << roll
            << ", Pitch: " << pitch
            << ", Yaw: " << yaw;

        return oss.str();
    }

    std::string print(int idx) const {
        std::ostringstream oss;
        oss << "Index: " << idx << ", " << print();
        
        return oss.str();
    }
};

// 数据加载类
class DataLoaderSTW
{
private:
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    ros::Timer timer_;
    image_transport::Publisher pano_pub_;
    tf2_ros::TransformBroadcaster car_br_;

    // 配置参数
    int sceneNums, carNums, playScene;
    double pubHz, offsetTime;
    std::string datasetPath;
    std::string panoramaPath;
    std::string localizationCsvPath_A, localizationCsvPath_B;

    std::vector<std::map<int, double>> sceneTimestamps;  // 场景序号与时间戳
    std::vector<std::map<double, NavigationData>> carsGpsDatas;  // 两辆车的GPS时间戳与定位数据

    // 当前访问的场景
    int Scene_ = -1;
    int totalFrames_ = -1;
    int currentFrame_ = -1;
    double init_x_ = 0, init_y_ = 0, init_z_ = 0;  // 起始时刻第一辆车的位置


public:
    // 构造函数
    DataLoaderSTW();

public:
    bool readCameraCsv();
    bool readLocalizationCsv();
    bool setScene(int sceneNum);
    bool getVideoFrame(int idx, cv::Mat& frame);
    bool readScene(int idx, double& frameTimestamp, cv::Mat& frame, std::vector<NavigationData>& frameNavDatas);
    bool releaseScene();
    bool gpsToMercator(double lon, double lat, double& x, double& y);
    bool gpsToMercator(double lon, double lat, double alt, double& x, double& y, double& z);
    bool generateTfMsgs(const std::vector<NavigationData>& frameNavDatas, std::vector<geometry_msgs::TransformStamped>& levelMsgs, std::vector<geometry_msgs::TransformStamped>& bodyMsgs, std::vector<geometry_msgs::TransformStamped>& cameraMsgs);
    void run(const ros::TimerEvent& event);
};
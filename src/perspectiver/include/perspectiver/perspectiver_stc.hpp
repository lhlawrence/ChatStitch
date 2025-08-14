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
#include <geometry_msgs/TransformStamped.h>
#include <tf2/LinearMath/Vector3.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

#include <common_utils/utils.hpp>
#include <common_utils/NerfRender.h>

class PerspectiverSTC
{
private:
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    ros::Timer timer_;
    image_transport::Subscriber pano_sub_;  // 订阅 data_loader 节点发布的全景图
    image_transport::Publisher perspective_pub_;  // 发布透视结果图
    image_transport::Publisher browse3d_pub_;  // 发布基于 OpenCV 映射的全景浏览结果图
    tf2_ros::Buffer tf_buffer_;
    tf2_ros::TransformListener tf_listener_;
    ros::ServiceClient ngp_client_;  // 请求 ngp 渲染服务的客户端

    // 配置ROS参数
    int browse_cam_yaw, browse_cam_pitch;  // 全景浏览时虚拟相机的偏航角与俯仰角
    int browse_fov_h, browse_fov_v;  // 全景浏览时虚拟相机的水平与垂直视场角
    int browse_img_width, browse_img_height;  // 全景浏览时图像的分辨率
    double target_car_bbox_front, target_car_bbox_back, target_car_bbox_left, target_car_bbox_right, target_car_bbox_up, target_car_bbox_down;  // 目标车辆 BBox 的前后左右上下距离


public:
    PerspectiverSTC();
    ~PerspectiverSTC() {}

public:
    void panoCallback(const sensor_msgs::ImageConstPtr& msg);
    bool perspective(cv::Mat& panorama, ros::Time img_timestamp);
};
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


class BBox
{
public:
    std::vector<tf2::Vector3> bbox_vertices_3d;
    std::vector<cv::Point2d> bbox_vertices_2d;

    // 假设载体坐标系原点位于车辆上顶面中心
    BBox(int length, int width, int height) {
        bbox_vertices_3d.clear();
        bbox_vertices_3d.push_back(tf2::Vector3(0, 0, 0));                          // 0. 中心
        bbox_vertices_3d.push_back(tf2::Vector3(-width/2, -length/2, -height));   // 1. 后左下
        bbox_vertices_3d.push_back(tf2::Vector3(width/2, -length/2, -height));    // 2. 后右下
        bbox_vertices_3d.push_back(tf2::Vector3(-width/2, length/2, -height));    // 3. 前左下
        bbox_vertices_3d.push_back(tf2::Vector3(width/2, length/2, -height));     // 4. 前右下
        bbox_vertices_3d.push_back(tf2::Vector3(-width/2, -length/2, 0));    // 5. 后左上
        bbox_vertices_3d.push_back(tf2::Vector3(width/2, -length/2, 0));     // 6. 后右上
        bbox_vertices_3d.push_back(tf2::Vector3(-width/2, length/2, 0));     // 7. 前左上
        bbox_vertices_3d.push_back(tf2::Vector3(width/2, length/2, 0));      // 8. 前右上
        bbox_vertices_2d.resize(bbox_vertices_3d.size());
    }

    // 载体坐标系原点到车辆前后左右上下的距离
    BBox(double front, double back, double left, double right, double up, double down) {
        bbox_vertices_3d.clear();
        bbox_vertices_3d.push_back(tf2::Vector3(0, 0, 0));                // 0. 中心
        bbox_vertices_3d.push_back(tf2::Vector3(-left, -back, -down));    // 1. 左后下
        bbox_vertices_3d.push_back(tf2::Vector3(right, -back, -down));    // 2. 右后下
        bbox_vertices_3d.push_back(tf2::Vector3(-left, front, -down));    // 3. 左前下
        bbox_vertices_3d.push_back(tf2::Vector3(right, front, -down));    // 4. 右前下
        bbox_vertices_3d.push_back(tf2::Vector3(-left, -back, up));       // 5. 左后上
        bbox_vertices_3d.push_back(tf2::Vector3(right, -back, up));       // 6. 右后上
        bbox_vertices_3d.push_back(tf2::Vector3(-left, front, up));       // 7. 左前上
        bbox_vertices_3d.push_back(tf2::Vector3(right, front, up));       // 8. 右前上
        bbox_vertices_2d.resize(bbox_vertices_3d.size());
    }

    bool draw3DBBox(cv::Mat& image) {
        if (bbox_vertices_2d.size() != bbox_vertices_3d.size()) {
            ROS_ERROR("bbox_vertices_2d.size() != bbox_vertices_3d.size()");
            return false;
        }

        // 绘制顶点
        for (int i = 0; i < bbox_vertices_2d.size(); ++i) {
            cv::circle(image, bbox_vertices_2d[i], 4, cv::Scalar(0, 0, 255), -1);
        }

        // 绘制底面
        cv::line(image, bbox_vertices_2d[1], bbox_vertices_2d[2], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[2], bbox_vertices_2d[4], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[4], bbox_vertices_2d[3], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[3], bbox_vertices_2d[1], cv::Scalar(255, 0, 0), 2);

        // 绘制顶面
        cv::line(image, bbox_vertices_2d[5], bbox_vertices_2d[6], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[6], bbox_vertices_2d[8], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[8], bbox_vertices_2d[7], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[7], bbox_vertices_2d[5], cv::Scalar(255, 0, 0), 2);

        // 绘制侧面
        cv::line(image, bbox_vertices_2d[1], bbox_vertices_2d[5], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[2], bbox_vertices_2d[6], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[3], bbox_vertices_2d[7], cv::Scalar(255, 0, 0), 2);
        cv::line(image, bbox_vertices_2d[4], bbox_vertices_2d[8], cv::Scalar(255, 0, 0), 2);

        return true;
    }

    void print() const {
        std::cout << "BBox3D: " << std::endl;
        for (int i = 0; i < bbox_vertices_3d.size(); ++i) {
            std::cout << bbox_vertices_3d[i] << std::endl;
        }
    }
};

class PerspectiverSTW
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
    PerspectiverSTW();
    ~PerspectiverSTW() {}

public:
    void panoCallback(const sensor_msgs::ImageConstPtr& msg);
    bool perspective(cv::Mat& panorama, ros::Time img_timestamp);
    bool callRenderService(const geometry_msgs::TransformStamped& T_tb_cam, const geometry_msgs::TransformStamped& T_tb_nerf, cv::Mat& image);
    bool targetBodyToPanorama(const cv::Mat& panorama, const geometry_msgs::TransformStamped& T_tb_cam, const tf2::Vector3& point3D_tb, cv::Point2d& point2D);
    double computeBlendScale(const cv::Mat& panorama, const cv::Mat& ngp_img, const std::vector<cv::Point2d>& bbox_vertices_2d);
    bool pinholeNerfToPanorama(const cv::Mat& panorama, const cv::Mat& ngp_img, cv::Mat& pano_ngp_img, cv::Mat& pano_ngp_mask, cv::Point2d& target_center, double scale = 1.0f);
    bool blendImages(const cv::Mat& panorama, const cv::Mat& pano_ngp_img, const cv::Mat& pano_ngp_mask, cv::Mat& blended_result, double alpha_1 = 0.5f, double alpha_2 = 0.5f);
    bool browse3D(const cv::Mat& panorama);
};
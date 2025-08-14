#include "perspectiver/perspectiver_stc.hpp"

PerspectiverSTC::PerspectiverSTC(): nh_("~"), it_(nh_), tf_listener_(tf_buffer_)
{
    // 将私有成员变量 nh_ 初始化为一个 ROS 节点句柄对象
    // 该节点句柄的命名空间是当前节点的私有命名空间

    // ------------------------- 配置ROS参数 -------------------------
    nh_.param("browse_cam_yaw", browse_cam_yaw, 0);  // 朝前为0，右转180，左转-180
    nh_.param("browse_cam_pitch", browse_cam_pitch, 0);  // 水平为0，向上-90，向下90
    nh_.param("browse_fov_h", browse_fov_h, 90);
    nh_.param("browse_fov_v", browse_fov_v, 60);
    nh_.param("browse_img_width", browse_img_width, 900);
    nh_.param("browse_img_height", browse_img_height, 600);
    nh_.param("target_car_bbox_front", target_car_bbox_front, 2.0);
    nh_.param("target_car_bbox_back", target_car_bbox_back, 2.0);
    nh_.param("target_car_bbox_left", target_car_bbox_left, 1.0);
    nh_.param("target_car_bbox_right", target_car_bbox_right, 1.0);
    nh_.param("target_car_bbox_up", target_car_bbox_up, 0.0);
    nh_.param("target_car_bbox_down", target_car_bbox_down, 2.0);

    // ------------------------- 初始化ROS对象 -------------------------
    pano_sub_ = it_.subscribe("/panorama", 1, &PerspectiverSTC::panoCallback, this);
    perspective_pub_ = it_.advertise("/perspective", 1);
    browse3d_pub_ = it_.advertise("/browse3d", 1);
    ngp_client_ = nh_.serviceClient<common_utils::NerfRender>("/ngp_render");
}

// 全景图像订阅者回调函数
void PerspectiverSTC::panoCallback(const sensor_msgs::ImageConstPtr& msg)
{
    // 获取图像
    cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    cv::Mat image = cv_ptr->image;

    perspective(image, msg->header.stamp);
}

// 透明感知主函数
bool PerspectiverSTC::perspective(cv::Mat& panorama, ros::Time img_timestamp)
{


    return true;
}
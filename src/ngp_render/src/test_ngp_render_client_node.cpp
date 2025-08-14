#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/Image.h>
#include <tf2/LinearMath/Vector3.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

#include <common_utils/NerfRender.h>

// 在图像中央绘制十字线
void drawCross(cv::Mat &img, const cv::Scalar &color = cv::Scalar(0, 0, 255), int size = 20)
{
    int center_x = img.cols / 2;
    int center_y = img.rows / 2;
    cv::line(img, cv::Point(center_x, center_y - size), cv::Point(center_x, center_y + size), color, 3);
    cv::line(img, cv::Point(center_x - size, center_y), cv::Point(center_x + size, center_y), color, 3);
}


int main(int argc, char **argv) {
    // 初始化 ROS
    ros::init(argc, argv, "test_ngp_render_client_node");
    ros::NodeHandle nh;
    ros::ServiceClient client = nh.serviceClient<common_utils::NerfRender>("/ngp_render");
    tf2_ros::Buffer tf_buffer;
    tf2_ros::TransformListener tf_listener(tf_buffer);

    // 循环渲染测试
    bool finish = false;
    while(!finish) {
        for (int i=0; i<4; i++) {
            // 载体坐标系下的相机视线朝向向量
            tf2::Vector3 body_cam_view_dir;
            switch(i) {  // 设两车朝向一致，改变相对位置
                case 0: body_cam_view_dir = tf2::Vector3(0.0f, 1.0f, 0.0f); break;  // 目标车辆在前方
                case 1: body_cam_view_dir = tf2::Vector3(-1.0f, 0.0f, 0.0f); break;  // 目标车辆在左方
                case 2: body_cam_view_dir = tf2::Vector3(0.0f, -1.0f, 0.0f); break;  // 目标车辆在后方
                case 3: body_cam_view_dir = tf2::Vector3(1.0f, 0.0f, 0.0f); break;  // 目标车辆在右方
            }

            // 变换到 nerf 坐标系下的视线朝向向量
            geometry_msgs::TransformStamped T_Body1_Nerf1;
            T_Body1_Nerf1 = tf_buffer.lookupTransform("nerf_1", "body_1", ros::Time(0), ros::Duration(1.0));
            tf2::Transform transform;
            tf2::fromMsg(T_Body1_Nerf1.transform, transform);  // 将 geometry_msgs::Transform 转换为 tf2::Transform
            tf2::Vector3 nerf_cam_view_dir = transform.getBasis() * body_cam_view_dir;
            tf2::Vector3 nerf_cam_pos_offset = transform.getOrigin();

            // 创建请求
            common_utils::NerfRender srv;
            srv.request.cam_view_dir = {(float)nerf_cam_view_dir.x(), (float)nerf_cam_view_dir.y(), (float)nerf_cam_view_dir.z()};
            srv.request.cam_pos_offset = {(float)nerf_cam_pos_offset.x(), (float)nerf_cam_pos_offset.y(), (float)nerf_cam_pos_offset.z()};
            srv.request.scale = 2.0f;

            // 请求响应
            if (client.call(srv)) {
                cv::Mat image = cv_bridge::toCvCopy(srv.response.image, sensor_msgs::image_encodings::BGR8)->image;
                ROS_INFO("Loop %d: body_cam_view_dir: (%.3f, %.3f, %.3f)", i, body_cam_view_dir.x(), body_cam_view_dir.y(), body_cam_view_dir.z());
                ROS_WARN("Loop %d: nerf_cam_view_dir: (%.3f, %.3f, %.3f)", i, nerf_cam_view_dir.x(), nerf_cam_view_dir.y(), nerf_cam_view_dir.z());
                drawCross(image);
                cv::imshow("render_image", image);
                int key = cv::waitKey(0);
                if (key == 'q' || key == 'Q') {
                    finish = true;
                    break;
                }
            } else {
                ROS_ERROR("Failed to call service nerf_render");
                return 1;
            }
        }
    }
    
    
    return 0;
}

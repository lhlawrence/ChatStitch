#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/Image.h>
#include <std_msgs/String.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <tf2_ros/transform_broadcaster.h>
#include <geometry_msgs/TransformStamped.h>
#include <tf2/LinearMath/Quaternion.h>
#include <common_utils/utils.hpp>
#include <common_utils/NerfRender.h>

#include <neural-graphics-primitives/testbed.h>
#include <tiny-cuda-nn/common.h>
#include <filesystem/path.h>

using namespace ngp;

class NgpRender
{
private:
	ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    ros::Timer timer_;
	ros::ServiceServer ngp_server_;
    image_transport::Publisher render_result_pub_;
    tf2_ros::TransformBroadcaster transform_br_;

	// 配置参数
	std::string ingp_file_path;
	int window_width, window_height;
	bool window_hidden;
	double body_nerf_rotation_x, body_nerf_rotation_y, body_nerf_rotation_z;
	double nerf_body_translation_x, nerf_body_translation_y, nerf_body_translation_z;

	std::shared_ptr<Testbed> testbed;

public:
	NgpRender(): nh_("~"), it_(nh_) 
	{
		// 读取配置参数
		nh_.param("ingp_file_path", ingp_file_path, std::string("base.ingp"));
		nh_.param("window_width", window_width, 1920);
		nh_.param("window_height", window_height, 1080);
		nh_.param("window_hidden", window_hidden, true);
		nh_.param("body_nerf_rotation_x", body_nerf_rotation_x, 0.0);
		nh_.param("body_nerf_rotation_y", body_nerf_rotation_y, 0.0);
		nh_.param("body_nerf_rotation_z", body_nerf_rotation_z, 0.0);
		nh_.param("nerf_body_translation_x", nerf_body_translation_x, 0.0);
		nh_.param("nerf_body_translation_y", nerf_body_translation_y, 0.0);
		nh_.param("nerf_body_translation_z", nerf_body_translation_z, 0.0);

		// 初始化 ROS
		timer_ = nh_.createTimer(ros::Duration(0.1), &NgpRender::timer_transform_broadcast, this);
		ngp_server_ = nh_.advertiseService("/ngp_render", &NgpRender::process_render_request, this);
		render_result_pub_ = it_.advertise("/ngp_render_result", 1);
		
		// 初始化 ngp
		testbed = std::make_shared<Testbed>();
		testbed->load_file(ingp_file_path);
		testbed->m_train = false;
		testbed->m_use_cv = true;
		testbed->init_window(window_width, window_height, window_hidden);
		for (int i=0; i<36; i++) {  // 初始化渲染
			testbed->set_camera_to_user_view(i, 3.0);
			testbed->frame();
		}
		ROS_INFO("Ready to process num array to image.");

	}

private:
	// 定时广播载体坐标系与 Nerf 坐标系的变换
	void timer_transform_broadcast(const ros::TimerEvent& event)
	{
		// 更新配置参数
		nh_.param("body_nerf_rotation_x", body_nerf_rotation_x, 0.0);
		nh_.param("body_nerf_rotation_y", body_nerf_rotation_y, 0.0);
		nh_.param("body_nerf_rotation_z", body_nerf_rotation_z, 0.0);
		nh_.param("nerf_body_translation_x", nerf_body_translation_x, 0.0);
		nh_.param("nerf_body_translation_y", nerf_body_translation_y, 0.0);
		nh_.param("nerf_body_translation_z", nerf_body_translation_z, 0.0);

		// 创建绕 Y 轴的旋转
		tf2::Quaternion rotationY;
		rotationY.setRPY(0, deg2rad(body_nerf_rotation_y), 0); // RPY指Roll(绕X轴), Pitch(绕Y轴), Yaw(绕Z轴)

		// 创建绕新坐标系的 X 轴的旋转
		tf2::Quaternion rotationX;
		rotationX.setRPY(deg2rad(body_nerf_rotation_x), 0, 0); // RPY指Roll(绕X轴), Pitch(绕Y轴), Yaw(绕Z轴)

		// 创建绕新坐标系的 Z 轴的旋转
		tf2::Quaternion rotationZ;
		rotationZ.setRPY(0, 0, deg2rad(body_nerf_rotation_z)); // RPY指Roll(绕X轴), Pitch(绕Y轴), Yaw(绕Z轴)

		// 组合两个旋转
		tf2::Quaternion body_nerf_rotation = rotationY * rotationX * rotationZ;
		tf2::Quaternion nerf_body_rotation = body_nerf_rotation.inverse();

		// 广播 Nerf 坐标系到车辆载体坐标系的变换
        geometry_msgs::TransformStamped nerfMsg;
        nerfMsg.header.frame_id = "body_1";  // 目标车辆载体坐标系
        nerfMsg.child_frame_id = "nerf_1";  // 目标车辆 Nerf 坐标系
        nerfMsg.transform.translation.x = nerf_body_translation_x;
        nerfMsg.transform.translation.y = nerf_body_translation_y;
        nerfMsg.transform.translation.z = nerf_body_translation_z;
        nerfMsg.transform.rotation.x = nerf_body_rotation.x();
        nerfMsg.transform.rotation.y = nerf_body_rotation.y();
        nerfMsg.transform.rotation.z = nerf_body_rotation.z();
        nerfMsg.transform.rotation.w = nerf_body_rotation.w();
		nerfMsg.header.stamp = ros::Time::now();
		transform_br_.sendTransform(nerfMsg);
	}

	// 处理 ngp 渲染请求
	bool process_render_request(common_utils::NerfRender::Request &req, common_utils::NerfRender::Response &res) 
	{
		Timer ngp_timer;
		ROS_DEBUG("Received a request.");

		// 渲染图像
		vec3 view_dir{req.cam_view_dir[0], req.cam_view_dir[1], req.cam_view_dir[2]};
		vec3 pos_offset{req.cam_pos_offset[0], req.cam_pos_offset[1], req.cam_pos_offset[2]};
		testbed->set_camera_to_user_view(view_dir, pos_offset, req.scale);
		testbed->frame();
		ngp_timer.update("render_frame");
		
		// 生成响应，并将结果作为话题发布
		cv::Mat cv_img;
		testbed->return_cv_img(cv_img);
		sensor_msgs::ImagePtr img_msg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", cv_img).toImageMsg();
		res.image = *img_msg;
		render_result_pub_.publish(img_msg);
		ngp_timer.update("response_image");

		ngp_timer.print("process_request");
		return true;
	}
};


int main(int argc, char* argv[]) {
	// 初始化 ROS
	ros::init(argc, argv, "ngp_render_server_node");
	
	NgpRender ngp_render;

	ros::spin();

	return 0;
}

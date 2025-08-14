#include "perspectiver/perspectiver_stw.hpp"

PerspectiverSTW::PerspectiverSTW(): nh_("~"), it_(nh_), tf_listener_(tf_buffer_)
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
    pano_sub_ = it_.subscribe("/panorama", 1, &PerspectiverSTW::panoCallback, this);
    perspective_pub_ = it_.advertise("/perspective", 1);
    browse3d_pub_ = it_.advertise("/browse3d", 1);
    ngp_client_ = nh_.serviceClient<common_utils::NerfRender>("/ngp_render");
}

// 全景图像订阅者回调函数
void PerspectiverSTW::panoCallback(const sensor_msgs::ImageConstPtr& msg)
{
    // 获取图像
    cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    cv::Mat image = cv_ptr->image;

    perspective(image, msg->header.stamp);
}

// 透明感知主函数
bool PerspectiverSTW::perspective(cv::Mat& panorama, ros::Time img_timestamp)
{
    // 获取 tf 坐标变换
    geometry_msgs::TransformStamped T_Body1_Camera0, T_Body1_Body0, T_Body1_Nerf1;
    try {
        T_Body1_Camera0 = tf_buffer_.lookupTransform("camera_0", "body_1", img_timestamp, ros::Duration(1.0));
        T_Body1_Body0 = tf_buffer_.lookupTransform("body_0", "body_1", img_timestamp, ros::Duration(1.0));
        T_Body1_Nerf1 = tf_buffer_.lookupTransform("nerf_1", "body_1", img_timestamp, ros::Duration(1.0));
    }
    catch (tf2::TransformException &ex) {
        ROS_ERROR("%s", ex.what());
        return false;
    }

    // 两车距离与姿态角
    double distance = sqrt(
        pow(T_Body1_Body0.transform.translation.x, 2) +
        pow(T_Body1_Body0.transform.translation.y, 2) +
        pow(T_Body1_Body0.transform.translation.z, 2));
    tf2::Quaternion q;
    tf2::fromMsg(T_Body1_Body0.transform.rotation, q);
    tf2::Matrix3x3 m(q);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);
    yaw = rad2deg(yaw);  // 角度表示
    ROS_INFO("distance: %f m, yaw: %f degree", distance, yaw);

    // 坐标映射，将目标载体坐标系的3D点映射到全景图像上
    BBox bbox(target_car_bbox_front, target_car_bbox_back, target_car_bbox_left, target_car_bbox_right, target_car_bbox_up, target_car_bbox_down);
    for (int i=0; i<bbox.bbox_vertices_3d.size(); i++) {
        targetBodyToPanorama(panorama, T_Body1_Camera0, bbox.bbox_vertices_3d[i], bbox.bbox_vertices_2d[i]);
    }

    // 获取 Nerf 渲染图像
    cv::Mat ngp_img;
    callRenderService(T_Body1_Camera0, T_Body1_Nerf1, ngp_img);

    // 将针孔图像的 Nerf 渲染结果映射到全景图像上
    cv::Mat pano_ngp_img, pano_ngp_mask;
    double blend_scale = computeBlendScale(panorama, ngp_img, bbox.bbox_vertices_2d);
    pinholeNerfToPanorama(panorama, ngp_img, pano_ngp_img, pano_ngp_mask, bbox.bbox_vertices_2d[0], blend_scale);

    // 融合全景图像与 Nerf 渲染图像
    cv::Mat blended_result;
    std::cout << "000\n";
    blendImages(panorama, pano_ngp_img, pano_ngp_mask, blended_result, 0.3f, 0.7f);

    // 在融合结果图上绘制 3D BBox 显示
    bbox.draw3DBBox(blended_result);
    perspective_pub_.publish(cv_bridge::CvImage(std_msgs::Header(), "bgr8", blended_result).toImageMsg());
    ROS_INFO("Center point of 3D BBox: (%f, %f)", bbox.bbox_vertices_2d[0].x, bbox.bbox_vertices_2d[0].y);

    // 生成 3D 全景浏览效果
    browse3D(blended_result);

    return true;
}


/** @brief 请求渲染服务
 *  @param T_tb_cam 目标载体坐标系到全景相机坐标系的变换
 *  @param T_tb_nerf 目标载体坐标系到目标 Nerf 坐标系的变换
 *  @param image 获取的渲染图像
*/
bool PerspectiverSTW::callRenderService(const geometry_msgs::TransformStamped& T_tb_cam, const geometry_msgs::TransformStamped& T_tb_nerf, cv::Mat& image)
{
    // 载体坐标系下的相机视线朝向向量
    tf2::Transform transform_tb_cam;
    tf2::fromMsg(T_tb_cam.transform, transform_tb_cam);  // 将 geometry_msgs::Transform 转换为 tf2::Transform
    tf2::Vector3 body_cam_view_dir = - transform_tb_cam.inverse().getOrigin();  // 全景相机坐标系的原点在目标载体坐标系下的坐标取负
    ROS_INFO("body_cam_view_dir: (%.3f, %.3f, %.3f)", body_cam_view_dir.y(), body_cam_view_dir.z(), body_cam_view_dir.x());

    // 变换到 nerf 坐标系下的视线朝向向量
    tf2::Transform transform_tb_nerf;
    tf2::fromMsg(T_tb_nerf.transform, transform_tb_nerf);  // 将 geometry_msgs::Transform 转换为 tf2::Transform
    tf2::Vector3 nerf_cam_view_dir = transform_tb_nerf.getBasis() * body_cam_view_dir;
    tf2::Vector3 nerf_cam_pos_offset = transform_tb_nerf.getOrigin();

    // 创建请求
    common_utils::NerfRender srv;
    srv.request.cam_view_dir = {(float)nerf_cam_view_dir.x(), (float)nerf_cam_view_dir.y(), (float)nerf_cam_view_dir.z()};
    srv.request.cam_pos_offset = {(float)nerf_cam_pos_offset.x(), (float)nerf_cam_pos_offset.y(), (float)nerf_cam_pos_offset.z()};
    srv.request.scale = 1.5f;

    // 请求响应
    if (ngp_client_.call(srv)) {
        image = cv_bridge::toCvCopy(srv.response.image, sensor_msgs::image_encodings::BGR8)->image;
        // cv::imwrite("/home/nvidia/ssd/Projects/CrossView_ws/data/ngp.png", image);
        return true;
    } else {
        ROS_ERROR("Failed to call service nerf_render");
        return false;
    }
}


/** @brief 将目标载体坐标系的3D点映射到全景图像上
 *  @param panorama 全景图像
 *  @param T_tb_cam 目标载体坐标系到全景相机坐标系的变换
 *  @param point3D_tb 目标坐标系下的点
 *  @param point2D 全景图像上的点
*/
bool PerspectiverSTW::targetBodyToPanorama(const cv::Mat& panorama, const geometry_msgs::TransformStamped& T_tb_cam, const tf2::Vector3& point3D_tb, cv::Point2d& point2D)
{
    // 目标载体坐标系的3D点变换到全景相机坐标系下
    tf2::Transform transform;
    tf2::fromMsg(T_tb_cam.transform, transform);  // 将 geometry_msgs::Transform 转换为 tf2::Transform
    tf2::Vector3 point3D_cam = transform * point3D_tb;

    // 从 3D 坐标到球面坐标
    double theta = atan2(point3D_cam.x(), point3D_cam.y());  // 方位角，相对于 y 轴的角度，范围 [-pi, pi]
    double phi = atan2(sqrt(pow(point3D_cam.x(), 2) + pow(point3D_cam.y(), 2)), point3D_cam.z());  // 俯仰角，从 z 轴向下的角度，范围 [0, pi]

    // 球面坐标到全景图像坐标
    int width = panorama.cols;
    int height = panorama.rows;
    int u = (int)(width * (theta + M_PI) / (2 * M_PI));
    int v = (int)(height * phi / M_PI);
    point2D.x = u;
    point2D.y = v;

    return true;
}


/** @brief 计算融合时针孔图像的 Nerf 渲染结果的缩放比例
 *  @param panorama 全景图像
 *  @param ngp_img Nerf 渲染结果，针孔图像
 *  @param bbox_vertices_2d 目标车辆 BBox 顶点在全景图像中的像素坐标
*/
double PerspectiverSTW::computeBlendScale(const cv::Mat& panorama, const cv::Mat& ngp_img, const std::vector<cv::Point2d>& bbox_vertices_2d)
{
    // 计算全景图中 BBox 的左右宽度
    auto min_max_X = std::minmax_element(bbox_vertices_2d.begin(), bbox_vertices_2d.end(),  // 使用 lambda 函数找到 x 坐标最小和最大的点
        [](const cv::Point2d& a, const cv::Point2d& b) {
            return a.x < b.x;
        });  // min_max_X 是一个 pair，包含最小和最大元素的迭代器
    double minX = min_max_X.first->x;  
    double maxX = min_max_X.second->x;
    double pano_dist = maxX - minX;  // 全景图中 BBox 的左右宽度

    // 计算针孔图像的 Nerf 渲染结果的掩码，并进行形态学操作去除噪点
    cv::Mat ngp_img_mask;
    cv::cvtColor(ngp_img, ngp_img_mask, cv::COLOR_BGR2GRAY); // 转换为灰度图
    cv::threshold(ngp_img_mask, ngp_img_mask, 1, 255, cv::THRESH_BINARY); // 生成二值掩码
    cv::morphologyEx(ngp_img_mask, ngp_img_mask, cv::MORPH_OPEN, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3)));  // 开运算，先腐蚀后膨胀

    // 计算针孔图像的 Nerf 渲染结果的左右宽度
    std::vector<cv::Point> nonZeroLocations;
    cv::findNonZero(ngp_img_mask, nonZeroLocations);
    auto xCompare = [](const cv::Point& a, const cv::Point& b) {
        return a.x < b.x;
    };
    auto minXIt = std::min_element(nonZeroLocations.begin(), nonZeroLocations.end(), xCompare);
    auto maxXIt = std::max_element(nonZeroLocations.begin(), nonZeroLocations.end(), xCompare);
    double ngp_dist = maxXIt->x - minXIt->x;
    
    // 计算尺度因子
    double scale = pano_dist / ngp_dist;
    ROS_INFO("pano_dist: %f, ngp_dist: %f, scale: %f", pano_dist, ngp_dist, scale);
    return scale;
}


/** @brief 将针孔图像的 Nerf 渲染结果映射到全景图像上，生成与全景图具有相同尺寸的 Nerf 渲染图象和掩码
 *  @param panorama 全景图像
 *  @param ngp_img Nerf 渲染结果，针孔图像
 *  @param pano_ngp_img 全景图上的 Nerf 渲染结果
 *  @param pano_ngp_mask 全景图上的 Nerf 渲染结果的掩码
 *  @param target_center 目标车辆中心在全景图像上的中心点
 *  @param scale 缩放比例
*/
bool PerspectiverSTW::pinholeNerfToPanorama(const cv::Mat& panorama, const cv::Mat& ngp_img, cv::Mat& pano_ngp_img, cv::Mat& pano_ngp_mask, cv::Point2d& target_center, double scale)
{
    // 创建一个与全景图相同大小的空白图像
    pano_ngp_img = cv::Mat::zeros(panorama.size(), panorama.type());

    //! TODO: 对针孔图像进行投影变形，保证几何一致性

    // 缩放针孔图像的 Nerf 渲染结果
    cv::Mat resized_ngp_img;
    cv::resize(ngp_img, resized_ngp_img, cv::Size(), scale, scale);

    // 计算渲染结果在全景图中应当放置的位置
    int width = resized_ngp_img.cols;
    int height = resized_ngp_img.rows;
    int topLeftX = std::max((int)(target_center.x - width / 2), 0);  // 左上角的 x 坐标
    int topLeftY = std::max(int(target_center.y - height / 2), 0);  // 左上角的 y 坐标

    // 确保放置的位置不超过全景图的边界
    width = std::min(width, panorama.cols - topLeftX);
    height = std::min(height, panorama.rows - topLeftY);

    // 在全景图上放置缩放后的渲染结果
    cv::Mat roi = pano_ngp_img(cv::Rect(topLeftX, topLeftY, width, height));
    resized_ngp_img(cv::Rect(0, 0, width, height)).copyTo(roi);

    // 创建掩码
    cv::cvtColor(pano_ngp_img, pano_ngp_mask, cv::COLOR_BGR2GRAY); // 转换为灰度图
    cv::threshold(pano_ngp_mask, pano_ngp_mask, 1, 255, cv::THRESH_BINARY); // 生成二值掩码
    
    return true;
}


/** @brief 将全景图像与 Nerf 渲染图像进行融合
 *  @param panorama 全景图像
 *  @param pano_ngp_img 全景图上的 Nerf 渲染结果
 *  @param pano_ngp_mask 全景图上的 Nerf 渲染结果的掩码
 *  @param alpha_1 掩码有效区域里，全景图像的权重
 *  @param alpha_2 掩码有效区域里，Nerf 渲染图像的权重
*/
bool PerspectiverSTW::blendImages(const cv::Mat& panorama, const cv::Mat& pano_ngp_img, const cv::Mat& pano_ngp_mask, cv::Mat& blended_result, double alpha_1, double alpha_2)
{
    // 按指定权重混合两个图像：dst = src1 * alpha_1 + src2 * alpha_2 + gamma
    cv::Mat blended;
    cv::addWeighted(panorama, alpha_1, pano_ngp_img, alpha_2, 0.0, blended, panorama.type());

    // 创建一个与全景图像大小相同的空图像，用于存储最终结果
    blended_result = cv::Mat::zeros(panorama.size(), panorama.type());

    // 将加权融合的结果复制到掩码的白色区域
    blended.copyTo(blended_result, pano_ngp_mask);

    // 将全景图像复制到掩码的黑色区域
    panorama.copyTo(blended_result, ~pano_ngp_mask);

    return true;
}


// 生成3D浏览效果
bool PerspectiverSTW::browse3D(const cv::Mat& panorama)
{
    cv::Mat browse_img(browse_img_height, browse_img_width, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat map_x(browse_img_height, browse_img_width, CV_32FC1);
    cv::Mat map_y(browse_img_height, browse_img_width, CV_32FC1);

    // 更新相机姿态
    nh_.param("browse_cam_yaw", browse_cam_yaw, 0);  // 朝前为0，右转180，左转-180
    nh_.param("browse_cam_pitch", browse_cam_pitch, 0);  // 水平为0，向上-90，向下90

    // 计算反向映射，目标图像中的像素在全景图中对应的位置
    for (int i=0; i<browse_img_width; i++) {
        for (int j=0; j<browse_img_height; j++) {
            double theta = deg2rad(browse_cam_yaw + (i / double(browse_img_width) - 0.5) * browse_fov_h);  // 单位：弧度
            double phi = deg2rad(browse_cam_pitch + (j / double(browse_img_height) - 0.5) * browse_fov_v);  // 单位：弧度
            double u = panorama.cols * (theta + CV_PI) / (2 * CV_PI);
            double v = panorama.rows * (phi + CV_PI / 2) / CV_PI;
            map_x.at<float>(j, i) = u;
            map_y.at<float>(j, i) = v;
        }
    }

    // 应用映射变换
    cv::remap(panorama, browse_img, map_x, map_y, cv::INTER_LINEAR);
    browse3d_pub_.publish(cv_bridge::CvImage(std_msgs::Header(), "bgr8", browse_img).toImageMsg());
    
    return true;
}
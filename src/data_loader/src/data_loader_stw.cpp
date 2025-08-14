#include "data_loader/data_loader_stw.hpp"

DataLoaderSTW::DataLoaderSTW(): nh_("~"), it_(nh_) 
{
    // 将私有成员变量 nh_ 初始化为一个 ROS 节点句柄对象
    // 该节点句柄的命名空间是当前节点的私有命名空间

    // ------------------------- 配置ROS参数 -------------------------
    nh_.param("sceneNums", sceneNums, 4);
    nh_.param("carNums", carNums, 2);
    nh_.param("playScene", playScene, 1);
    nh_.param("pubHz", pubHz, 10.0);
    nh_.param("offsetTime", offsetTime, 0.0);
    nh_.param("datasetPath", datasetPath, std::string("/home/dzp62442/Projects/CrossView_ws/data/"));
    nh_.param("panoramaPath", panoramaPath, std::string("SurroundView/VID_20231020_172334/"));
    nh_.param("localizationCsvPath_A", localizationCsvPath_A, std::string("Localization/Car-A/"));
    nh_.param("localizationCsvPath_B", localizationCsvPath_B, std::string("Localization/Car-B/"));
    if (ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Info))  // 设置日志级别
        ros::console::notifyLoggerLevelsChanged();

    // ------------------------- 初始化ROS对象 -------------------------
    double period = 1.0 / pubHz;
    timer_ = nh_.createTimer(ros::Duration(period), &DataLoaderSTW::run, this);
    pano_pub_ = it_.advertise("/panorama", 1);


    // ------------------------- 读取CSV文件 -------------------------
    if (!readCameraCsv()) {
        ROS_ERROR("[ DataLoaderSTW::DataLoaderSTW() ] Error readCameraCsv !");
        return;
    }
    if (!readLocalizationCsv()) {
        ROS_ERROR("[ DataLoaderSTW::DataLoaderSTW() ] Error readLocalizationCsv !");
        return;
    }

    // ------------------------- 设置场景视频 -------------------------
    setScene(playScene);
    
}

// 读取相机的 CSV 文件，包含多个场景
bool DataLoaderSTW::readCameraCsv()
{
    sceneTimestamps.clear();

    for (int i=1; i<=sceneNums; i++) {  // 遍历所有场景视频文件
        std::string scene = "scene" + std::to_string(i);
        std::string filename = datasetPath + panoramaPath + scene + "/" + scene + ".csv";
        ROS_DEBUG("[ DataLoaderSTW::readCameraCsv() ] Csv file: %s", filename.c_str());

        std::ifstream file(filename);
        if (!file.is_open()) {
            ROS_ERROR("[ DataLoaderSTW::readCameraCsv() ] Error open: %s", filename.c_str());
            return false;
        }

        std::string line;
        std::map<int, double> frameTimestamps;  // 帧序号与时间戳
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(iss, token, ',')) {  // 分割每行并存储到向量中
                tokens.push_back(token);
            }
            int frameNumber = std::stoi(tokens[0]);
            double timestamp = std::stod(tokens[1]);
            frameTimestamps[frameNumber] = timestamp;
        }

        sceneTimestamps.push_back(frameTimestamps);
        file.close();
    }
    ROS_INFO("[ DataLoaderSTW::readCameraCsv() ] Read all %d camera csv successfully!", sceneNums);

    return true;
}

// 读取定位导航的 CSV 文件，包含多辆车
bool DataLoaderSTW::readLocalizationCsv()
{
    carsGpsDatas.clear();

    for (int i=0; i<carNums; i++) {  // 遍历所有车辆定位导航文件
        std::string filename;
        switch (i) {
            case 0: filename = datasetPath + localizationCsvPath_A + "A.csv"; break;
            case 1: filename = datasetPath + localizationCsvPath_B + "B.csv"; break;
            default: ROS_ERROR("[ DataLoaderSTW::readLocalizationCsv() ] Error carNums: %d", carNums); return false;
        }
        ROS_DEBUG("[ DataLoaderSTW::readLocalizationCsv() ] Csv file: %s", filename.c_str());

        std::ifstream file(filename);
        if (!file.is_open()) {
            ROS_ERROR("[ DataLoaderSTW::readLocalizationCsv() ] Error open: %s", filename.c_str());
            return false;
        }

        std::string line;
        std::map<double, NavigationData> gpsData;  // 时间戳与定位数据
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(iss, token, ',')) {  // 分割每行并存储到向量中
                tokens.push_back(token);
            }
            double timestamp = std::stod(tokens[0]);
            double latitude = std::stod(tokens[1]);
            double longitude = std::stod(tokens[2]);
            double altitude = std::stod(tokens[3]);
            double eightVel = std::stod(tokens[4]);
            double northVel = std::stod(tokens[5]);
            double upVel = std::stod(tokens[6]);
            double roll = std::stod(tokens[7]);
            double pitch = std::stod(tokens[8]);
            double yaw = std::stod(tokens[9]);
            NavigationData navData(timestamp, latitude, longitude, altitude, eightVel, northVel, upVel, roll, pitch, yaw);
            gpsData.emplace(timestamp, navData);
        }

        carsGpsDatas.push_back(gpsData);
        file.close();
    }
    ROS_INFO("[ DataLoaderSTW::readLocalizationCsv() ] Read all %d localization csv successfully!", carNums);

    return true;
}

// 设置正在读取的场景
bool DataLoaderSTW::setScene(int sceneNum)
{
    // 设置序列参数
    if (sceneNum < 0 || sceneNum > sceneNums) {
        ROS_ERROR("[ DataLoaderSTW::setScene() ] Error input sceneNum: %d", sceneNum);
        return false;
    }
    Scene_ = sceneNum;

    boost::filesystem::path imgsPath = datasetPath + panoramaPath + "scene" + std::to_string(Scene_) + "/imgs";
    if (!boost::filesystem::exists(imgsPath) || !boost::filesystem::is_directory(imgsPath)) {
        ROS_ERROR("[ DataLoaderSTW::setScene() ] Error imgsPath: %s", imgsPath.c_str());
        return false;
    }

    totalFrames_ = std::distance(boost::filesystem::directory_iterator(imgsPath), boost::filesystem::directory_iterator{});
    currentFrame_ = 0;
    
    // 读取初始时刻第一辆车的位置
    double frameTimestamp;
    cv::Mat frame;
    std::vector<NavigationData> frameNavDatas;
    ROS_INFO("[ DataLoaderSTW::run() ] Current frame: %d", currentFrame_);
    readScene(0, frameTimestamp, frame, frameNavDatas);  // 初始时刻
    gpsToMercator(frameNavDatas[0].longitude, frameNavDatas[0].latitude, frameNavDatas[0].altitude, init_x_, init_y_, init_z_);  // 第一辆车
    ROS_INFO(" [ DataLoaderSTW::setScene() ] init_x_: %f, init_y_: %f, init_z_: %f", init_x_, init_y_, init_z_);

    return true;
}

// 初始化场景后，读取场景视频中的指定帧
bool DataLoaderSTW::getVideoFrame(int idx, cv::Mat& frame)
{
    if (Scene_ < 0) {
        ROS_ERROR("[ DataLoaderSTW::getVideoFrame() ] Scene not set !");
        return false;
    }
    
    if (idx < 0 || idx > totalFrames_) {
        ROS_ERROR("[ DataLoaderSTW::getVideoFrame() ] Error input idx: %d", idx);
        return false;
    }

    std::string imgPath = datasetPath + panoramaPath + "scene" + std::to_string(Scene_) + "/imgs/" + std::to_string(idx) + ".jpg";
    cv::imread(imgPath).copyTo(frame);
    if (frame.empty()) {
        ROS_ERROR("[ DataLoaderSTW::getVideoFrame() ] Error read empty frame: %d", idx);
        return false;
    }

    return true;
}

// 读取场景中的数据
bool DataLoaderSTW::readScene(int idx, double& frameTimestamp, cv::Mat& frame, std::vector<NavigationData>& frameNavDatas)
{
    if (Scene_ < 0) {
        ROS_ERROR("[ DataLoaderSTW::readScene() ] Scene not set !");
        return false;
    }

    // 读取场景时间戳
    frameTimestamp = sceneTimestamps[Scene_-1][idx] - offsetTime;  // 校正时间对齐

    // 读取视频帧
    if (!getVideoFrame(idx, frame)) {
        ROS_ERROR("[ DataLoaderSTW::readScene() ] Error getVideoFrame: %d", idx);
        return false;
    }

    // 配准导航数据
    frameNavDatas.clear();  // 该场景视频帧对应的每辆车的导航数据
    for (int carIdx=0; carIdx<carNums; carIdx++) {
        auto navIt = carsGpsDatas[carIdx].lower_bound(frameTimestamp);  // 查找第一个不小于 frameTimestamp 的导航数据条目
        if (navIt == carsGpsDatas[carIdx].end()) {
            ROS_ERROR("[ DataLoaderSTW::readScene() ] Error: No navigation data found for timestamp %f", frameTimestamp);
            return false;
        }
        NavigationData navData = navIt->second;
        frameNavDatas.push_back(navData);
        ROS_DEBUG("%s", navData.print(carIdx).c_str());  // 打印导航数据
    }
    
    return true;
}

// 释放正在读取的场景
bool DataLoaderSTW::releaseScene()
{
    Scene_ = -1;
    totalFrames_ = -1;
    currentFrame_ = -1;
    init_x_ = 0;
    init_y_ = 0;
    init_z_ = 0;
    return true;
}

// GPS 坐标转平面墨卡托坐标
bool DataLoaderSTW::gpsToMercator(double lon, double lat, double alt, double& x, double& y, double& z)
{
    const double R = 6378137; // WGS-84半径
    x = R * lon * M_PI / 180.0;
    y = R * log(tan(M_PI / 4.0 + lat * (M_PI / 180.0) / 2.0));
    z = alt;

    ROS_DEBUG("[ DataLoaderSTW::gpsToMercator() ] lon: %f, lat: %f, alt: %f, x: %f, y: %f, z: %f", lon, lat, alt, x, y, z);
    return true;

}

/*
当一个Transform消息被发布时，它定义了从child_frame_id（子坐标系）到frame_id（父坐标系）的空间关系
对于子坐标系中的点、向量或其它几何对象，可以使用这个 Transform 将其转换到父坐标系中的相应位置和方向
P_parent = R * P_child + T
*/

/*! @brief 根据导航数据生成 tf 坐标变换消息，不含时间戳
 *  @param frameNavDatas 该场景视频帧对应的每辆车的导航数据
 *  @param levelMsgs 世界坐标系到车辆水平坐标系
 *  @param bodyMsgs 车辆水平坐标系到车辆载体坐标系
 *  @param cameraMsgs 车辆载体坐标系到全景相机坐标系
*/
bool DataLoaderSTW::generateTfMsgs(const std::vector<NavigationData>& frameNavDatas, std::vector<geometry_msgs::TransformStamped>& levelMsgs, std::vector<geometry_msgs::TransformStamped>& bodyMsgs, std::vector<geometry_msgs::TransformStamped>& cameraMsgs)
{
    bodyMsgs.clear();
    levelMsgs.clear();
    for (int carIdx=0; carIdx<carNums; carIdx++) {
        // GPS 坐标转平面墨卡托坐标
        double x, y, z;
        gpsToMercator(frameNavDatas[carIdx].longitude, frameNavDatas[carIdx].latitude, frameNavDatas[carIdx].altitude, x, y, z);
        // 车辆水平坐标系到世界坐标系
        geometry_msgs::TransformStamped levelMsg;
        levelMsg.header.frame_id = "world";  // 全局坐标系
        levelMsg.child_frame_id = "level_" + std::to_string(carIdx);  // 车辆水平坐标系
        levelMsg.transform.translation.x = x - init_x_;
        levelMsg.transform.translation.y = y - init_y_;
        levelMsg.transform.translation.z = z - init_z_;
        levelMsg.transform.rotation.x = 0.0;
        levelMsg.transform.rotation.y = 0.0;
        levelMsg.transform.rotation.z = 0.0;
        levelMsg.transform.rotation.w = 1.0;
        levelMsgs.push_back(levelMsg);
        // 车辆载体坐标系到车辆水平坐标系
        geometry_msgs::TransformStamped bodyMsg;
        bodyMsg.header.frame_id = "level_" + std::to_string(carIdx);  // 车辆水平坐标系
        bodyMsg.child_frame_id = "body_" + std::to_string(carIdx);  // 车辆载体坐标系
        bodyMsg.transform.translation.x = 0.0;
        bodyMsg.transform.translation.y = 0.0;
        bodyMsg.transform.translation.z = 0.0;
        tf2::Quaternion q;
        q.setRPY(0.0, 0.0, deg2rad(-frameNavDatas[carIdx].yaw));  // 导航系统输出航向角为车辆朝向与北向的逆时针夹角，而水平坐标系Z轴指天，顺时针为正
        bodyMsg.transform.rotation.x = q.x();
        bodyMsg.transform.rotation.y = q.y();
        bodyMsg.transform.rotation.z = q.z();
        bodyMsg.transform.rotation.w = q.w();
        bodyMsgs.push_back(bodyMsg);
        // 全景相机坐标系到车辆载体坐标系
        if (carIdx == 0) {  // 只有第一辆车上装有全景相机
            geometry_msgs::TransformStamped cameraMsg;
            cameraMsg.header.frame_id = "body_" + std::to_string(carIdx);  // 车辆载体坐标系
            cameraMsg.child_frame_id = "camera_" + std::to_string(carIdx);  // 全景相机坐标系
            cameraMsg.transform.translation.x = 0.0;
            cameraMsg.transform.translation.y = 0.5;
            cameraMsg.transform.translation.z = 0.1;
            cameraMsg.transform.rotation.x = 0.0;
            cameraMsg.transform.rotation.y = 0.0;
            cameraMsg.transform.rotation.z = 0.0;
            cameraMsg.transform.rotation.w = 1.0;
            cameraMsgs.push_back(cameraMsg);
        }
    }
    return true;
}

// 定时器回调函数
void DataLoaderSTW::run(const ros::TimerEvent& event)
{
    // 读取场景中的数据
    double frameTimestamp;
    cv::Mat frame;
    std::vector<NavigationData> frameNavDatas;
    ROS_INFO("[ DataLoaderSTW::run() ] Play scene: %d, Current frame: %d", Scene_, currentFrame_);
    readScene(currentFrame_, frameTimestamp, frame, frameNavDatas);
    currentFrame_++;
    if (currentFrame_ >= totalFrames_)
        currentFrame_ = 0;

    // 转换全景图像
    sensor_msgs::ImagePtr panoMsg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", frame).toImageMsg();
    
    // 转换定位导航数据
    std::vector<geometry_msgs::TransformStamped> bodyMsgs, levelMsgs, cameraMsgs;
    generateTfMsgs(frameNavDatas, bodyMsgs, levelMsgs, cameraMsgs);

    // 设置时间戳并发布
    for (int i=0; i<carNums; i++) {
        levelMsgs[i].header.stamp = ros::Time::now();
        car_br_.sendTransform(levelMsgs[i]);
        bodyMsgs[i].header.stamp = ros::Time::now();
        car_br_.sendTransform(bodyMsgs[i]);
        if (i == 0) {  // 只有第一辆车上装有全景相机
            cameraMsgs[i].header.stamp = ros::Time::now();
            car_br_.sendTransform(cameraMsgs[i]);
        }
    }
    panoMsg->header.stamp = ros::Time::now();
    pano_pub_.publish(panoMsg);
    
}
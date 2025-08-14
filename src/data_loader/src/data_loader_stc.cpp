#include "data_loader/data_loader_stc.hpp"
#include "common_utils/Timer.hpp"

DataLoaderSTC::DataLoaderSTC(): nh_("~"), it_(nh_) 
{
    // 将私有成员变量 nh_ 初始化为一个 ROS 节点句柄对象
    // 该节点句柄的命名空间是当前节点的私有命名空间

    // ------------------------- 配置ROS参数 -------------------------
    nh_.param("sceneNums", sceneNums, 1);
    nh_.param("playScene", playScene, 1);
    nh_.param("startInsta360Idx", startInsta360Idx, 0);
    nh_.param("pubHz", pubHz, 30.0);
    nh_.param("offsetTime", offsetTime, 0.0);
    nh_.param("datasetPath", datasetPath, std::string("/home/dzp62442/Projects/CrossView_ws/data/"));
    nh_.param("insta360Path", insta360Path, std::string("Insta360/"));
    nh_.param("insta360Csv", insta360Csv, std::string("pano.csv"));
    nh_.param("ladybugPath", ladybugPath, std::string("Ladybug/"));
    nh_.param("ladybugCsv", ladybugCsv, std::string("pano.csv"));
    if (ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Debug))  // 设置日志级别
        ros::console::notifyLoggerLevelsChanged();

    // ------------------------- 初始化ROS对象 -------------------------
    double period = 1.0 / pubHz;
    timer_ = nh_.createTimer(ros::Duration(period), &DataLoaderSTC::run, this);
    insta360_pano_pub_ = it_.advertise("/Insta360_pano", 1);
    ladybug_pano_pub_ = it_.advertise("/ladybug_pano", 1);

    // ------------------------- 读取CSV文件 -------------------------
    if (!readCameraCsv()) {
        ROS_ERROR("[ DataLoaderSTC::DataLoaderSTC() ] Error readCameraCsv !");
        return;
    }

    // ------------------------- 设置场景视频 -------------------------
    setScene(playScene);
    
}

// 读取相机的 CSV 文件，包含多个场景
bool DataLoaderSTC::readCameraCsv()
{
    insta360Timestamps.clear();
    ladybugTimestamps.clear();

    for (int i=1; i<=sceneNums; i++) {  // 遍历所有场景视频文件
        std::string scene = "scene" + std::to_string(i);
        std::string insta360_file = datasetPath + insta360Path + insta360Csv;
        std::string ladybug_file = datasetPath + ladybugPath + ladybugCsv;

        // 读取 Insta360 全景相机的 CSV 文件
        ROS_DEBUG("[ DataLoaderSTC::readCameraCsv() ] Insta360 csv file: %s", insta360_file.c_str());
        std::ifstream file1(insta360_file);
        if (!file1.is_open()) {
            ROS_ERROR("[ DataLoaderSTC::readCameraCsv() ] Error open: %s", insta360_file.c_str());
            return false;
        }
        std::string line1;
        std::map<int, double> frameTimestamps1;  // 帧序号与时间戳
        while (std::getline(file1, line1)) {
            std::istringstream iss(line1);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(iss, token, ',')) {  // 分割每行并存储到向量中
                tokens.push_back(token);
            }
            int frameNumber = std::stoi(tokens[0]);
            double timestamp = std::stod(tokens[1]);
            frameTimestamps1[frameNumber] = timestamp;
        }
        insta360Timestamps.push_back(frameTimestamps1);
        file1.close();

        // 读取 Ladybug 全景相机的 CSV 文件
        ROS_DEBUG("[ DataLoaderSTC::readCameraCsv() ] Ladybug csv file: %s", ladybug_file.c_str());
        std::ifstream file2(ladybug_file);
        if (!file2.is_open()) {
            ROS_ERROR("[ DataLoaderSTC::readCameraCsv() ] Error open: %s", ladybug_file.c_str());
            return false;
        }
        std::string line2;
        std::map<double, int> frameTimestamps2;  // 时间戳与帧序号
        while (std::getline(file2, line2)) {
            std::istringstream iss(line2);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(iss, token, ',')) {  // 分割每行并存储到向量中
                tokens.push_back(token);
            }
            int frameNumber = std::stoi(tokens[0]);
            double timestamp = std::stod(tokens[1]);
            frameTimestamps2[timestamp] = frameNumber;
        }
        ladybugTimestamps.push_back(frameTimestamps2);
        file2.close();
    }
    ROS_INFO("[ DataLoaderSTC::readCameraCsv() ] Read all %d camera csv successfully!", sceneNums);

    return true;
}

// 设置正在读取的场景
bool DataLoaderSTC::setScene(int sceneNum)
{
    // 设置序列参数
    if (sceneNum < 0 || sceneNum > sceneNums) {
        ROS_ERROR("[ DataLoaderSTC::setScene() ] Error input sceneNum: %d", sceneNum);
        return false;
    }
    Scene_ = sceneNum;

    // 初始化 Insta360 图像序列
    boost::filesystem::path imgsPath1 = datasetPath + insta360Path + "imgs/";
    if (!boost::filesystem::exists(imgsPath1) || !boost::filesystem::is_directory(imgsPath1)) {
        ROS_ERROR("[ DataLoaderSTC::setScene() ] Error imgsPath: %s", imgsPath1.c_str());
        return false;
    }
    totalInsta360Frames_ = std::distance(boost::filesystem::directory_iterator(imgsPath1), boost::filesystem::directory_iterator{});
    currentInsta360Frame_ = startInsta360Idx;

    // 初始化 Ladybug 图像序列
    boost::filesystem::path imgsPath2 = datasetPath + ladybugPath + "imgs/";
    if (!boost::filesystem::exists(imgsPath2) || !boost::filesystem::is_directory(imgsPath2)) {
        ROS_ERROR("[ DataLoaderSTC::setScene() ] Error imgsPath: %s", imgsPath2.c_str());
        return false;
    }
    totalLadybugFrames_ = std::distance(boost::filesystem::directory_iterator(imgsPath2), boost::filesystem::directory_iterator{});
    currentLadybugFrame_ = 0;

    return true;
}

// 初始化场景后，读取场景视频中的指定帧
bool DataLoaderSTC::getInsta360Frame(int idx, cv::Mat& frame)
{
    if (Scene_ < 0) {
        ROS_ERROR("[ DataLoaderSTC::getInsta360Frame() ] Scene not set !");
        return false;
    }
    
    if (idx < 0 || idx > totalInsta360Frames_) {
        ROS_ERROR("[ DataLoaderSTC::getInsta360Frame() ] Error input idx: %d", idx);
        return false;
    }

    std::string imgPath = datasetPath + insta360Path + "imgs/" + std::to_string(idx) + ".jpg";
    cv::imread(imgPath).copyTo(frame);
    if (frame.empty()) {
        ROS_ERROR("[ DataLoaderSTC::getInsta360Frame() ] Error read empty frame: %d", idx);
        return false;
    }

    return true;
}

// 初始化场景后，读取场景视频中的指定帧
bool DataLoaderSTC::getLadybugFrame(int idx, cv::Mat& frame)
{
    if (Scene_ < 0) {
        ROS_ERROR("[ DataLoaderSTC::getLadybugFrame() ] Scene not set !");
        return false;
    }
    
    if (idx < 0 || idx > totalLadybugFrames_) {
        ROS_ERROR("[ DataLoaderSTC::getLadybugFrame() ] Error input idx: %d", idx);
        return false;
    }

    std::string imgPath = datasetPath + ladybugPath + "imgs/" + std::to_string(idx) + ".jpg";
    cv::imread(imgPath).copyTo(frame);
    if (frame.empty()) {
        ROS_ERROR("[ DataLoaderSTC::getLadybugFrame() ] Error read empty frame: %d", idx);
        return false;
    }

    return true;
}

// 读取场景中的数据，以 Insta360 为基准，寻找最接近的 Ladybug 数据
bool DataLoaderSTC::readScene(int idx, double& frameTimestamp, cv::Mat& insta360Frame, cv::Mat& ladybugFrame)
{
    if (Scene_ < 0) {
        ROS_ERROR("[ DataLoaderSTC::readScene() ] Scene not set !");
        return false;
    }

    Timer timer;

    // 读取 Insta360 时间戳，配准 Ladybug 视频帧
    frameTimestamp = insta360Timestamps[Scene_-1][idx] - offsetTime;  // 校正时间对齐
    auto ladybugIt = ladybugTimestamps[Scene_-1].lower_bound(frameTimestamp);  //! 查找第一个不小于 frameTimestamp 的 Ladybug 数据
    int ladybug_idx = ladybugIt->second;
    ROS_DEBUG("[ DataLoaderSTC::readScene() ] Insta360 frame: %d, Timestamp: %f", idx, frameTimestamp);
    ROS_DEBUG("[ DataLoaderSTC::readScene() ] Ladybug frame: %d, Timestamp: %f", ladybug_idx, ladybugIt->first);
    timer.update("readTimestamp");

    // omp 并行读取 Insta360 和 Ladybug 视频帧
    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task  // 读取 Insta360 视频帧
            {
                if (!getInsta360Frame(idx, insta360Frame)) {
                    ROS_FATAL("[ DataLoaderSTC::readScene() ] Error getInsta360Frame: %d", idx);
                }
            }
            #pragma omp task  // 读取 Ladybug 视频帧
            {
                if (!getLadybugFrame(ladybug_idx, ladybugFrame)) {
                    ROS_FATAL("[ DataLoaderSTC::readScene() ] Error getLadybugFrame: %d", idx);
                }
            }
        }
    }
    timer.update("readFrame");

    timer.print("readScene");
    return true;
}

// 释放正在读取的场景
bool DataLoaderSTC::releaseScene()
{
    Scene_ = -1;
    totalInsta360Frames_ = -1;
    totalLadybugFrames_ = -1;
    currentInsta360Frame_ = -1;
    currentLadybugFrame_ = -1;

    return true;
}

// 定时器回调函数
void DataLoaderSTC::run(const ros::TimerEvent& event)
{
    Timer timer;

    // 以 Insta360 为基准，读取场景中的数据
    double frameTimestamp;
    cv::Mat insta360Frame, ladybugFrame;
    ROS_INFO("[ DataLoaderSTC::run() ] Play scene: %d, Current insta360 frame: %d", Scene_, currentInsta360Frame_);
    readScene(currentInsta360Frame_, frameTimestamp, insta360Frame, ladybugFrame);
    currentInsta360Frame_++;
    if (currentInsta360Frame_ >= totalInsta360Frames_)
        currentInsta360Frame_ = startInsta360Idx;
    timer.update("readScene");

    // 转换全景图像
    sensor_msgs::ImagePtr panoInsta360Msg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", insta360Frame).toImageMsg();
    sensor_msgs::ImagePtr panoLadybugMsg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", ladybugFrame).toImageMsg();

    // 设置时间戳并发布 
    panoInsta360Msg->header.stamp = ros::Time::now();
    panoLadybugMsg->header.stamp = ros::Time::now();
    insta360_pano_pub_.publish(panoInsta360Msg);
    ladybug_pano_pub_.publish(panoLadybugMsg);
    timer.update("publish");

    timer.print("run");
}
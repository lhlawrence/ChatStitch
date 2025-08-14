#include "data_loader/data_loader_stw.hpp"
#include "data_loader/data_loader_stc.hpp"

void sigintHandler(int sig)
{
    ROS_INFO("--------------------------------------");
    ROS_INFO("data_loader_node: shutting down...");
    ROS_INFO("--------------------------------------");
    ros::shutdown();
}

int main(int argc, char * argv[])
{
    ros::init(argc, argv, "data_loader_node", ros::init_options::NoSigintHandler);
    ros::NodeHandle nh("~");
    signal(SIGINT, sigintHandler);  // 设置SIGINT信号的处理函数

    std::string task;
    nh.param("task", task, std::string("none"));
    if (task == "STW") {
        DataLoaderSTW data_loader_stw;
        ros::spin();
    }
    else if (task == "STC") {
        DataLoaderSTC data_loader_stc;
        ros::spin();
    }
    else
        ROS_FATAL("[ data_loader_node ] task {%s} error !", task.c_str());
    
    return 0;
}
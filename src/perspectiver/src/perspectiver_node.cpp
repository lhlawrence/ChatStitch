#include "perspectiver/perspectiver_stw.hpp"
#include "perspectiver/perspectiver_stc.hpp"

void sigintHandler(int sig)
{
    ROS_INFO("--------------------------------------");
    ROS_INFO("perspectiver_node: shutting down...");
    ROS_INFO("--------------------------------------");
    ros::shutdown();
}

int main(int argc, char * argv[])
{
    ros::init(argc, argv, "perspectiver_node", ros::init_options::NoSigintHandler);
    ros::NodeHandle nh("~");
    signal(SIGINT, sigintHandler);  // 设置SIGINT信号的处理函数    

    std::string task;
    nh.param("task", task, std::string("none"));
    if (task == "STW") {
        PerspectiverSTW perspectiver_stw;
        ros::spin();
    }
    else if (task == "STC") {
        PerspectiverSTC perspectiver_stc;
        ros::spin();
    }
    else
        ROS_FATAL("[ Perspective Node Fail ] task {%s} error !", task.c_str());

    return 0;
}
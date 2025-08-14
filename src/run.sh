#!/bin/bash

# Activate the conda environment
# conda init
conda activate chatstitch
source ../devel/setup.bash

# Set environment variables
export PYTHONPATH= #Your PYTHONPATH
export OPENAI_API_KEY= #Your OPENAI API Key
export OPENAI_BASE_URL= #OPENAI Base URL

# Run the first ROS node in the background
roslaunch chatstitch chat.launch &
sleep 5
 roslaunch ngp_render ngp_render.launch &
 sleep 5
 roslaunch data_loader data_loader_stw.launch &
 sleep 5
 roslaunch perspectiver perspectiver_stw.launch

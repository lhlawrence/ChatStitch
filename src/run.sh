#!/bin/bash

# Activate the conda environment
# conda init
conda activate chatstitch
source ../devel/setup.bash

# Set environment variables
export PYTHONPATH= #Your PYTHONPATH
export OPENAI_API_KEY= #Your OPENAI API Key
export OPENAI_BASE_URL= #OPENAI Base URL

# export OPENAI_BASE_URL="https://www.gptapi.us/v1"
# export OPENAI_API_KEY=sk-YuzZ2NMCE63opFYn22Ae6d3b015c42D49a0d4008B6780fD4

# Run the first ROS node in the background
# roslaunch chatstitch chat.launch &
sleep 5
 roslaunch ngp_render ngp_render.launch &
 sleep 5
 roslaunch data_loader data_loader_stw.launch &
 sleep 5
 roslaunch perspectiver perspectiver_stw.launch

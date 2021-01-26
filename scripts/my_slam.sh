#!/bin/sh
xterm  -e  " source /opt/ros/kinetic/setup.bash; source ~/catkin_ws/devel/setup.bash; roslaunch  rse_nd_home_service_robot world.launch" &
sleep 5
xterm  -e  " source /opt/ros/kinetic/setup.bash; source ~/catkin_ws/devel/setup.bash; rosrun teleop_twist_keyboard teleop_twist_keyboard.py" & 
sleep 3
xterm  -e  " source /opt/ros/kinetic/setup.bash; source ~/catkin_ws/devel/setup.bash; roslaunch gmapping slam_gmapping_pr2.launch " & 
sleep 3
xterm  -e  " rosrun rviz rviz -d /home/robond/catkin_ws/src/rse-nd-home-service-robot/rvizConfig/mapping.rviz" 
# rosrun map_server map_saver -f myMap
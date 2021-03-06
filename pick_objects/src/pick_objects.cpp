#include <ros/ros.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include "add_markers/JobRequest.h"
#include <tf/tf.h>
#include <math.h>
#include <nav_msgs/Odometry.h>

// Define a client for to send goal requests to the move_base server through a SimpleActionClient
typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

// Distance on x axis from base_chassis to sup_chassis
float xRobotContainerOffset = 0.25;

// Service for request to add markers
ros::ServiceClient jobRequestClient;

// Get current robot's real position from odom topic and calculate the right place to put the marker to it be placed on the center of the sup_chassis robot link.
geometry_msgs::Pose getRobotContainerPose()
{
  geometry_msgs::Pose containerPose;
  boost::shared_ptr<nav_msgs::Odometry const> sharedOdom;
  nav_msgs::Odometry odom;

  // Get one message from odom topic
  sharedOdom = ros::topic::waitForMessage<nav_msgs::Odometry>("/odom", ros::Duration(5));
  if (sharedOdom != NULL)
  {
    odom = *sharedOdom;
    ROS_INFO("Got robot odometry, calculate the right robot container center");

    tf::Quaternion q(
        odom.pose.pose.orientation.x,
        odom.pose.pose.orientation.y,
        odom.pose.pose.orientation.z,
        odom.pose.pose.orientation.w);
    tf::Matrix3x3 m(q);

    // Convert quartenion to RPY
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);

    // Translate odom pose to container pose by the yaw angle and distance between robot_chassis and sup_chassis
    containerPose.position.x = odom.pose.pose.position.x - xRobotContainerOffset * cos(yaw);
    containerPose.position.y = odom.pose.pose.position.y - xRobotContainerOffset * sin(yaw);

    // Considering the size of the marker of 0.2 in z axis and odom position z is near and above sup_chassis surface
    containerPose.position.z = odom.pose.pose.position.z + 0.1;

    containerPose.orientation.x = odom.pose.pose.orientation.x;
    containerPose.orientation.y = odom.pose.pose.orientation.y;
    containerPose.orientation.z = odom.pose.pose.orientation.z;
    containerPose.orientation.w = odom.pose.pose.orientation.w;

    // Debug
    // ROS_INFO("containerPose >>  x:%1.2f, y:%1.2f, z:%1.2f, roll:%1.2f, pitch:%1.2f, yaw:%1.2f", containerPose.position.x, containerPose.position.y, containerPose.position.z, roll, pitch, yaw);
  }
  return containerPose;
}

// Move robot to goal and request for add_marker handler the job
void travelToGoal(MoveBaseClient *ac, const move_base_msgs::MoveBaseGoal goal, const char *goalName)
{
  ROS_INFO("Robot is moving to %s at x:%1.2f,y:%1.2f,yaw:%1.2f ", goalName, goal.target_pose.pose.position.x, goal.target_pose.pose.position.y, goal.target_pose.pose.orientation.w);

  // point the robot goal pose
  ac->sendGoal(goal);

  // Wait an infinite time for the results
  ac->waitForResult();

  // Check if the robot reached its goal
  if (ac->getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
  {
    ROS_INFO("Successfully arrived at %s zone, now starting job_request", goalName);

    // Request to add_marker service to handle marker at current arrived goal
    add_markers::JobRequest srv;
    srv.request.job = goalName;
    // Set request pose with the current robot's container position
    srv.request.pose = getRobotContainerPose(); //or use goal.target_pose.pose to put marker on the goal position
    if (!jobRequestClient.call(srv))
      ROS_ERROR("Failed to call service job_request, head to the next goal");
  }
  else
    ROS_INFO("Failed to arrive at %s zone, head to the next goal", goalName);
}

// Build goal variable based on roll, pitch and yaw
move_base_msgs::MoveBaseGoal setupGoal(float x, float y, float z, float roll, float pitch, float yaw)
{
  move_base_msgs::MoveBaseGoal goal;
  goal.target_pose.header.frame_id = "map";
  goal.target_pose.header.stamp = ros::Time::now();
  goal.target_pose.pose.position.x = x;
  goal.target_pose.pose.position.y = y;
  goal.target_pose.pose.position.z = z;
  tf::Quaternion q;
  q.setRPY(roll, pitch, yaw);
  q.normalize();
  goal.target_pose.pose.orientation.x = q[0];
  goal.target_pose.pose.orientation.y = q[1];
  goal.target_pose.pose.orientation.z = q[2];
  goal.target_pose.pose.orientation.w = q[3];
  return goal;
}
void startupMarker(const move_base_msgs::MoveBaseGoal goal)
{
  add_markers::JobRequest srv;
  srv.request.job = "Startup";
  // Set marker on the goal pose
  srv.request.pose = goal.target_pose.pose;
  if (!jobRequestClient.call(srv))
    ROS_ERROR("Failed to call service job_request");
}

int main(int argc, char **argv)
{
  // Initialize the pick_objects node
  ros::init(argc, argv, "pick_objects");
  ros::NodeHandle n;

  // Connect to the JobRequest service
  jobRequestClient = n.serviceClient<add_markers::JobRequest>("job_request");

  // tell the action client that we want to spin a thread by default
  MoveBaseClient ac("move_base", true);

  // Wait 5 sec for move_base action server to start
  while (!ac.waitForServer(ros::Duration(5.0)))
  {
    ROS_INFO("Waiting for the move_base action server to start");
  }

  // Initialize goalPickup
  move_base_msgs::MoveBaseGoal goalPickup;
  goalPickup = setupGoal(9.0, -2.0, 0.0, 0.0, 0.0, 1.15707);

  // Initialize goalDropOff
  move_base_msgs::MoveBaseGoal goalDropOff;
  goalDropOff = setupGoal(-0.3, -0.5, 0.0, 0.0, 0.0, -1.15707);

  // Start robot duties
  // Initialize marker at the Pickup position 0.2 higher in z
  startupMarker(setupGoal(9.0, -2.0, 0.2, 0.0, 0.0, 1.15707));

  // Go to the goalPickup
  travelToGoal(&ac, goalPickup, "Pickup");

  // wait 5 seconds
  ros::Duration(5).sleep();

  // Go to the goalDropOff
  travelToGoal(&ac, goalDropOff, "DropOff");

  return 0;
}
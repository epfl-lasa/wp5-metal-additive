/**
 * @file Subtask.cpp
 * @brief Declaration of the Subtask class
 * @author [Elise Jeandupeux]
 * @author [Louis Munier] - lmunier@protonmail.com
 * @date 2024-10-08
 */

#include "Subtask.h"

using namespace std;

Subtask::Subtask(ros::NodeHandle& nh) : nh_(nh) {
  subROI_ = nh_.subscribe("/ur5/roi_topic", 1000, &Subtask::cbkROI_, this);

  // debug waypoint
  pubWaypoint1_ = nh_.advertise<geometry_msgs::PointStamped>("debug_waypoint_1", 10);
  pubWaypoint2_ = nh_.advertise<geometry_msgs::PointStamped>("debug_waypoint_2", 10);
  pubRobotBase_ = nh_.advertise<geometry_msgs::PointStamped>("debug_robot_base", 10);
  pubComputedQuat_ = nh_.advertise<geometry_msgs::PoseStamped>("debug_computedQuat", 10);
}

void Subtask::clearROI() { dequeROI_.clear(); }

const bool Subtask::empty() const { return dequeROI_.empty(); }

const Subtask::ROI Subtask::getROI() {
  if (!dequeROI_.empty()) {
    ROI roi = dequeROI_.front();
    dequeROI_.pop_front();
    return roi;
  } else {
    return ROI();
  }
}

void Subtask::parseROI_(const string& str) {
  const size_t MSG_SIZE = 7;

  string waypointID = "";
  vector<double> waypointsPos{};
  splitString_(str, ',', waypointID, waypointsPos);

  if (waypointsPos.size() != MSG_SIZE) {
    ROS_ERROR_STREAM("[Subtask] - Waypoint ROS message " << str.c_str() << " doesn't have the correct size, should be "
                                                         << MSG_SIZE << " instead of " << waypointsPos.size());
  }

  // Store ROI only if it is not already done
  if (!isIdStored_(waypointID)) {
    ROI roi;
    roi.id = waypointID;
    roi.posStart = Eigen::Vector3d(waypointsPos[0], waypointsPos[1], waypointsPos[2]);
    roi.posEnd = Eigen::Vector3d(waypointsPos[3], waypointsPos[4], waypointsPos[5]);
    roi.quat = rotateVectorInPlan_({roi.posStart, roi.posEnd, robotPos_}, refVector_);
    dequeROI_.push_back(roi);

    // Visualisation debug
    publishWaypoint_(roi.posStart, pubWaypoint1_);
    publishWaypoint_(roi.posEnd, pubWaypoint2_);
    publishWaypoint_(robotPos_, pubRobotBase_);
    publishPose_(roi.posStart, roi.quat, pubComputedQuat_);

    ROS_INFO_STREAM("[Subtask] - Waypoint registered, key : " << waypointID);
  } else {
    ROS_INFO_STREAM("[Subtask] - Waypoint received previously, already registered, key : " << waypointID);
  }
}

const bool Subtask::isIdStored_(const string& id) const {
  for (const auto& roi : dequeROI_) {
    if (roi.id == id) {
      return true;
    }
  }
  return false;
}

void Subtask::splitString_(const string& str, const char delimiter, string& waypointID, vector<double>& waypointsPos) {
  bool idTaken = false;
  string token = "";
  stringstream ss(str);

  while (getline(ss, token, delimiter)) {
    if (!idTaken) {
      waypointID = token;
      idTaken = true;
    } else {
      waypointsPos.push_back(stod(token));
    }
  }
}

const Eigen::Quaterniond Subtask::rotateVectorInPlan_(const array<Eigen::Vector3d, 3>& pointsArray,
                                                      const Eigen::Vector3d refVector,
                                                      const double theta) {

  // Compute vector normal to plan define by the 3 points
  const Eigen::Vector3d planVector1 = pointsArray[0] - pointsArray[1];
  const Eigen::Vector3d planVector2 = pointsArray[0] - pointsArray[2];
  const Eigen::Vector3d normalVector = planVector1.cross(planVector2).normalized();

  // Rotate vector defined by plan, forming by points : pointsArray, by theta
  const Eigen::Quaterniond quatRotation(cos(theta / 2),
                                        normalVector.x() * sin(theta / 2),
                                        normalVector.y() * sin(theta / 2),
                                        normalVector.z() * sin(theta / 2));
  const Eigen::Vector3d rotatedVectPlan = quatRotation * planVector1;

  // Compute rotation in world frame
  const Eigen::Vector3d axisFinalRot = refVector.cross(rotatedVectPlan).normalized();
  const double angleFinalRot = acos(refVector.dot(rotatedVectPlan) / (refVector.norm() * rotatedVectPlan.norm()));

  return Eigen::Quaterniond(cos(angleFinalRot / 2),
                            axisFinalRot.x() * sin(angleFinalRot / 2),
                            axisFinalRot.y() * sin(angleFinalRot / 2),
                            axisFinalRot.z() * sin(angleFinalRot / 2));
}

void Subtask::publishPose_(const Eigen::Vector3d& pos, const Eigen::Quaterniond quat, ros::Publisher pub) {
  float TIME_WAIT = 0.2;
  size_t NB_PUBLISH = 3;

  geometry_msgs::PoseStamped poseStamped;
  poseStamped.header.stamp = ros::Time::now();
  poseStamped.header.frame_id = "base_link";
  poseStamped.pose.position.x = pos.x();
  poseStamped.pose.position.y = pos.y();
  poseStamped.pose.position.z = pos.z();

  poseStamped.pose.orientation.w = quat.w();
  poseStamped.pose.orientation.x = quat.x();
  poseStamped.pose.orientation.y = quat.y();
  poseStamped.pose.orientation.z = quat.z();

  for (size_t i = 0; i < NB_PUBLISH; ++i) {
    pub.publish(poseStamped);
    ros::Duration(TIME_WAIT).sleep();
  }
}

void Subtask::publishWaypoint_(const Eigen::Vector3d& pos, ros::Publisher pub) {
  float TIME_WAIT = 0.05;
  size_t NB_PUBLISH = 3;
  const string frameID = "base_link";

  geometry_msgs::PointStamped pointStamped;
  pointStamped.header.stamp = ros::Time::now();
  pointStamped.header.frame_id = frameID;
  pointStamped.point.x = pos.x();
  pointStamped.point.y = pos.y();
  pointStamped.point.z = pos.z();

  for (size_t i = 0; i < NB_PUBLISH; ++i) {
    pub.publish(pointStamped);
    ros::Duration(TIME_WAIT).sleep();
  }
}

void Subtask::cbkROI_(const std_msgs::String::ConstPtr& msg) { parseROI_(msg->data); }
/**
 * @file test_robotic_arm_ur5.cpp
 * @author Louis Munier (lmunier@protonmail.com)
 * @brief
 * @version 0.2
 * @date 2024-10-01
 *
 * @copyright Copyright (c) 2024 - EPFL
 *
 */
#include <gtest/gtest.h>
#include <ros/ros.h>

#include <random>
#include <variant>
#include <vector>

#include "IRosInterfaceBase.h"
#include "RoboticArmFactory.h"
#include "math_tools.h"

using namespace std;

// Static member initialization
const double TOLERANCE = 2e-4;
static const int NB_TESTS = 50;

std::unique_ptr<IRoboticArmBase> roboticArm = nullptr;
static mt19937 gen(random_device{}());
static uniform_real_distribution<> dis(-0.5, 0.5);
static uniform_real_distribution<> disJoint(-2 * M_PI, 2 * M_PI);
static vector<vector<double>> jointPositions;
static vector<pair<Eigen::Quaterniond, Eigen::Vector3d>> waypoints;

class RoboticArmUrTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    ros::NodeHandle nh;
    RoboticArmFactory armFactory = RoboticArmFactory();

    std::string robotName = "";
    std::string rosVersion = "";
    nh.getParam("robotName", robotName);
    nh.getParam("rosVersion", rosVersion);
    roboticArm = armFactory.createRoboticArm(robotName, IRosInterfaceBase::rosVersionsMap.at(rosVersion));

    generateWaypoints();
    generateJointPositions();
  }

  static void TearDownTestSuite() {}

  // Function to generate a random quaternion
  static Eigen::Quaterniond generateRandomQuaternion() {
    Eigen::Quaterniond q(dis(gen), dis(gen), dis(gen), dis(gen));
    q.normalize();
    return q;
  }

  // Function to generate a random position vector
  static Eigen::Vector3d generateRandomPosition() { return Eigen::Vector3d(dis(gen), dis(gen), dis(gen)); }

  // Function to generate a random vector of size n
  static vector<double> generateRandomVector(int n) {
    vector<double> vec(n);
    for (int i = 0; i < n; ++i) {
      vec[i] = dis(gen);
    }
    return vec;
  }

  // Function to generate a reachable random waypoint
  static pair<Eigen::Quaterniond, Eigen::Vector3d> generateReachableWaypoint() {
    uint tries = 0;
    const int MAX_TRIES = 100;

    while (true) {
      tries++;
      Eigen::Quaterniond quaternion = generateRandomQuaternion();
      Eigen::Vector3d position = generateRandomPosition();
      vector<double> jointPos{};

      bool isValid = roboticArm->getIKTrac(quaternion, position, jointPos);

      // Check if the IK solver found a valid solution
      if (isValid) {
        return make_pair(quaternion, position);
      } else if (tries > MAX_TRIES) {
        throw runtime_error("Could not find a valid waypoint after " + to_string(MAX_TRIES) + " tries.");
      }
    }
  }

  // Function to generate waypoints
  static void generateWaypoints() {
    for (int i = 0; i < NB_TESTS; ++i) {
      waypoints.push_back(generateReachableWaypoint());
    }
  }

  // Function to generate random joint positions between the joint limits
  static void generateJointPositions() {
    jointPositions.clear();

    for (size_t i = 0; i < NB_TESTS; ++i) {
      std::vector<double> jointPos;
      for (size_t j = 0; j < roboticArm->getNbJoints(); ++j) {
        jointPos.push_back(disJoint(gen));
      }
      jointPositions.push_back(jointPos);
    }
  }

  static void quaternionToAxisAngle(const Eigen::Quaterniond& q, Eigen::Vector3d& axis, double& angle) {
    // Ensure the quaternion is normalized
    Eigen::Quaterniond normalized_q = q.normalized();

    // Extract the components of the quaternion
    double w = normalized_q.w();
    double x = normalized_q.x();
    double y = normalized_q.y();
    double z = normalized_q.z();

    // Compute the angle
    angle = 2.0 * std::acos(w);

    // Compute the axis
    double sin_half_angle = std::sqrt(1.0 - w * w);
    if (sin_half_angle < 1e-6) {
      // If the angle is very small, the axis is not well-defined
      axis = Eigen::Vector3d(1.0, 0.0, 0.0); // Default axis
    } else {
      axis = Eigen::Vector3d(x, y, z) / sin_half_angle;
    }
  }

  // Function to compute angle difference between two quaternions
  static double calculateRotationDifference(const Eigen::Quaterniond& q1, const Eigen::Quaterniond& q2) {
    // Normalize the quaternions to ensure they are unit quaternions
    Eigen::Quaterniond q1_normalized = q1.normalized();
    Eigen::Quaterniond q2_normalized = q2.normalized();

    // Compute the relative quaternion q_rel = q1.inverse() * q2
    Eigen::Quaterniond q_rel = q1_normalized.conjugate() * q2_normalized;

    // Extract the scalar part of the relative quaternion
    double w_rel = q_rel.w();

    // Calculate the rotation angle in radians
    double theta = 2 * std::acos(w_rel);

    return theta;
  }

  static void calculateAxisAngleDifference(const Eigen::Quaterniond& q1,
                                           const Eigen::Quaterniond& q2,
                                           Eigen::Vector3d& axis,
                                           double& angle) {
    // Normalize the quaternions to ensure they are unit quaternions
    Eigen::Quaterniond q1_normalized = q1.normalized();
    Eigen::Quaterniond q2_normalized = q2.normalized();

    // Compute the relative quaternion q_rel = q1.inverse() * q2
    Eigen::Quaterniond q_rel = q1_normalized.conjugate() * q2_normalized;

    // Convert the relative quaternion to axis-angle representation
    quaternionToAxisAngle(q_rel, axis, angle);
  }
};

// Create a test to check the swapJoints_ function of the UR5 robotic arm
TEST_F(RoboticArmUrTest, TestSwapJoints) {
  // Generate fake input data to check swapJoints_ function
  int nbJoints = roboticArm->getNbJoints();

  vector<double> jointPosIn = generateRandomVector(nbJoints);
  vector<double> jointVelIn = generateRandomVector(nbJoints);
  vector<double> jointTorqueIn = generateRandomVector(nbJoints);
  tuple<vector<double>, vector<double>, vector<double>> state{jointPosIn, jointVelIn, jointTorqueIn};

  // Generate fake output data to check swapJoints_ function
  vector<double> jointPosOut = jointPosIn;
  vector<double> jointVelOut = jointVelIn;
  vector<double> jointTorqueOut = jointTorqueIn;

  // Swap the data 0 to 2 for both fake input and output data using different methods
  swap(jointPosOut[0], jointPosOut[2]);
  swap(jointVelOut[0], jointVelOut[2]);
  swap(jointTorqueOut[0], jointTorqueOut[2]);

  roboticArm->swapJoints_(state);

  // Check if the joint positions are valid
  EXPECT_EQ(jointPosOut, get<0>(state));
  EXPECT_EQ(jointVelOut, get<1>(state));
  EXPECT_EQ(jointTorqueOut, get<2>(state));
}

// Create a test to check the reference configuration of the UR5 robotic arm to fit the trac-ik one
TEST_F(RoboticArmUrTest, TestForwardComparison) {
  for (auto& jointPos : jointPositions) {
    pair<Eigen::Quaterniond, Eigen::Vector3d> fkTracResult = roboticArm->getFKTrac(jointPos);
    pair<Eigen::Quaterniond, Eigen::Vector3d> fkGeoResult = roboticArm->getFKGeo(jointPos);

    EXPECT_TRUE(MathTools::areQuatEquivalent(fkTracResult.first, fkGeoResult.first, TOLERANCE));
    EXPECT_TRUE(MathTools::arePosEquivalent(fkTracResult.second, fkGeoResult.second, TOLERANCE));
  }
}

// Create a test to check the reference configuration of the UR5 robotic arm to fit the trac-ik one
TEST_F(RoboticArmUrTest, TestInverseComparison) {
  for (auto& [quaternion, position] : waypoints) {
    vector<double> tracJointPos{};
    roboticArm->getIKTrac(quaternion, position, tracJointPos);

    vector<vector<double>> ikSolutions;
    roboticArm->getIKGeo(quaternion, position, ikSolutions);

    vector<double> geoJointPos = ikSolutions[0];
    pair<Eigen::Quaterniond, Eigen::Vector3d> tracFKResult = roboticArm->getFKTrac(tracJointPos);
    pair<Eigen::Quaterniond, Eigen::Vector3d> geoFKResult = roboticArm->getFKTrac(geoJointPos);

    EXPECT_TRUE(MathTools::areQuatEquivalent(tracFKResult.first, geoFKResult.first, TOLERANCE));
    EXPECT_TRUE(MathTools::arePosEquivalent(tracFKResult.second, geoFKResult.second, TOLERANCE));

    tracFKResult = roboticArm->getFKGeo(tracJointPos);
    geoFKResult = roboticArm->getFKGeo(geoJointPos);

    EXPECT_TRUE(MathTools::areQuatEquivalent(tracFKResult.first, geoFKResult.first, TOLERANCE));
    EXPECT_TRUE(MathTools::arePosEquivalent(tracFKResult.second, geoFKResult.second, TOLERANCE));
  }
}

// Create a test to check coherency of the UR5 robotic arm trac-ik solver
TEST_F(RoboticArmUrTest, TestTracIkSolver) {
  for (auto& [quaternion, position] : waypoints) {
    vector<double> jointPos{};
    roboticArm->getIKTrac(quaternion, position, jointPos);

    // Compute forward kinematics
    pair<Eigen::Quaterniond, Eigen::Vector3d> tracFKResult = roboticArm->getFKTrac(jointPos);

    EXPECT_TRUE(MathTools::areQuatEquivalent(tracFKResult.first, quaternion, TOLERANCE));
    EXPECT_TRUE(MathTools::arePosEquivalent(tracFKResult.second, position, TOLERANCE));
  }
}

// Create a test to check coherency of the UR5 robotic arm geometric solver
TEST_F(RoboticArmUrTest, TestIkGeoSolver) {
  for (auto& [quaternion, position] : waypoints) {
    vector<vector<double>> ikSolutions;
    roboticArm->getIKGeo(quaternion, position, ikSolutions);

    // Compute forward kinematics
    for (const auto& sol : ikSolutions) {
      pair<Eigen::Quaterniond, Eigen::Vector3d> geoFKResult = roboticArm->getFKGeo(sol);

      EXPECT_TRUE(MathTools::areQuatEquivalent(geoFKResult.first, quaternion, TOLERANCE));
      EXPECT_TRUE(MathTools::arePosEquivalent(geoFKResult.second, position, TOLERANCE));
    }
  }
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "test_robotic_arm_ur5");
  ::testing::InitGoogleTest(&argc, argv);

  // Set the filter to run only the specific test
  // ::testing::GTEST_FLAG(filter) = "RoboticArmUrTest.TestReferenceConfiguration";

  return RUN_ALL_TESTS();
}
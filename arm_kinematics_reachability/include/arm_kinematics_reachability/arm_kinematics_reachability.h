/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2011, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of Willow Garage, Inc. nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*
* Author: Sachin Chitta
*********************************************************************/

#ifndef ARM_KINEMATICS_REACHABILITY_H_
#define ARM_KINEMATICS_REACHABILITY_H_

// System
#include <ros/ros.h>
#include <boost/shared_ptr.hpp>
#include <arm_kinematics_constraint_aware/arm_kinematics_constraint_aware.h>

// ROS msgs
#include <kinematics_msgs/GetConstraintAwarePositionIK.h>
#include <kinematics_msgs/WorkspacePoints.h>
#include <kinematics_msgs/WorkspacePoint.h>
#include <visualization_msgs/MarkerArray.h>

namespace arm_kinematics_reachability
{
class ArmKinematicsReachability
{
public:

  /** @class
   *  @brief Compute and visualize reachable workspace for an arm
   *  @author Sachin Chitta <sachinc@willowgarage.com>
   *
   */
  ArmKinematicsReachability();

  virtual ~ArmKinematicsReachability()
  {
  };

  /**
   * @brief This method computes and returns a discretized reachable workspace for an arm
   */
  bool computeWorkspace(kinematics_msgs::WorkspacePoints &workspace);
  bool computeWorkspace(kinematics_msgs::WorkspacePoints &workspace, const geometry_msgs::Pose &tool_frame_offset);

  /**
   * @brief This method visualizes a workspace region for a given arm
   */
  void visualize(const kinematics_msgs::WorkspacePoints &workspace,
                 const std::string &marker_namespace);
    
  bool getOnlyReachableWorkspace(kinematics_msgs::WorkspacePoints &workspace, 
                                 const geometry_msgs::Pose &tool_frame_offset);

  void publishWorkspace(const kinematics_msgs::WorkspacePoints &workspace);

  void visualize(const kinematics_msgs::WorkspacePoints &workspace,
                 const std::string &marker_namespace,
                 const geometry_msgs::Quaternion &orientation);

  void visualize(const kinematics_msgs::WorkspacePoints &workspace,
                 const std::string &marker_namespace,
                 const std::vector<geometry_msgs::Quaternion> &orientations);

  void visualizeWithArrows(const kinematics_msgs::WorkspacePoints &workspace,
                           const std::string &marker_namespace);

  bool isActive();


private:

  void setToolFrameOffset(const geometry_msgs::Pose &pose);

  void findIKSolutions(kinematics_msgs::WorkspacePoints &workspace);

  void getDefaultIKRequest(kinematics_msgs::GetConstraintAwarePositionIK::Request &req);

  void removeUnreachableWorkspace(kinematics_msgs::WorkspacePoints &workspace);

  std::vector<const kinematics_msgs::WorkspacePoint*> getPointsAtOrientation(const kinematics_msgs::WorkspacePoints &workspace,
                                                                                const geometry_msgs::Quaternion &orientation);

  std::vector<const kinematics_msgs::WorkspacePoint*> getPointsWithinRange(const kinematics_msgs::WorkspacePoints &workspace,
                                                                              const double min_radius,
                                                                              const double max_radius);

  void getNumPoints(const kinematics_msgs::WorkspacePoints &workspace,
                    unsigned int &x_num_points,
                    unsigned int &y_num_points,
                    unsigned int &z_num_points);
  
  bool sampleUniform(kinematics_msgs::WorkspacePoints &workspace);

  arm_kinematics_constraint_aware::ArmKinematicsConstraintAware kinematics_solver_;

  bool isEqual(const geometry_msgs::Quaternion &orientation_1, 
               const geometry_msgs::Quaternion &orientation_2);

  void getPositionIndexedMarkers(const kinematics_msgs::WorkspacePoints &workspace,
                                 const std::string &marker_namespace,
                                 visualization_msgs::MarkerArray &marker_array);

  void getPositionIndexedArrowMarkers(const kinematics_msgs::WorkspacePoints &workspace,
                                      const std::string &marker_namespace,
                                      visualization_msgs::MarkerArray &marker_array);

  void getMarkers(const kinematics_msgs::WorkspacePoints &workspace,
                  const std::string &marker_namespace,
                  std::vector<const kinematics_msgs::WorkspacePoint*> points,
                  visualization_msgs::MarkerArray &marker_array);

  ros::Publisher visualization_publisher_, workspace_publisher_;

protected:

  ros::NodeHandle node_handle_;
  tf::Pose tool_offset_, tool_offset_inverse_;
};

}
#endif

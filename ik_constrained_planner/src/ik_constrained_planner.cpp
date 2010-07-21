/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
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
*   * Neither the name of the Willow Garage nor the names of its
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
*********************************************************************/

/** \author Sachin Chitta, Ioan Sucan */


#include <ros/console.h>
#include <sstream>
#include <cstdlib>
#include <ik_constrained_planner/ik_constrained_planner.h>

namespace ik_constrained_planner
{
bool IKConstrainedPlanner::isRequestValid(motion_planning_msgs::GetMotionPlan::Request &req)
{   

  bool found = false;

  for(unsigned int i=0; i < group_names_.size(); i++)
  {
    if(group_names_[i] == req.motion_plan_request.group_name)
    {
      found = true;
      break;
    }
  }
  if(!found)
  {
    ROS_ERROR("Model %s does not exist",req.motion_plan_request.group_name.c_str());
    return false;
  }
    
  /* if the user did not specify a planner, use the first available one */
  if (req.motion_plan_request.planner_id.empty())
    req.motion_plan_request.planner_id = default_planner_id_;

  if(planner_map_.find(req.motion_plan_request.planner_id+"["+req.motion_plan_request.group_name+"]") == planner_map_.end())
  {
    ROS_ERROR("Could not find motion planner %s",(req.motion_plan_request.planner_id+"["+req.motion_plan_request.group_name+"]").c_str());
    return false;
  }

  /*  std::map<std::string, ompl::base::Planner*>::iterator planner_iterator = planner_map_.end();
  
  for (std::map<std::string, ompl::base::Planner*>::iterator it = planner_map_.begin() ; it != planner_map_.end() ; ++it)
  {
    if (it->first.find(req.motion_plan_request.planner_id+"["+req.motion_plan_request.group_name+"]") != std::string::npos)
    {
      req.motion_plan_request.planner_id = it->first;
      planner_iterator = it;
      break;
    }
  }      
  */
  if(req.motion_plan_request.goal_constraints.position_constraints.size() != 1 || 
     req.motion_plan_request.goal_constraints.orientation_constraints.size() != 1)
  {
    ROS_ERROR("Request for this planner must have one position and one orientation constraint");
    return false; 
  }
  // check headers
  for (unsigned int i = 0 ; i < req.motion_plan_request.goal_constraints.position_constraints.size() ; ++i)
    if (!planning_monitor_->getTransformListener()->frameExists(req.motion_plan_request.goal_constraints.position_constraints[i].header.frame_id))
    {
      ROS_ERROR("Frame '%s' is not defined for goal position constraint message %u", req.motion_plan_request.goal_constraints.position_constraints[i].header.frame_id.c_str(), i);
      return false;
    }
  
  for (unsigned int i = 0 ; i < req.motion_plan_request.goal_constraints.orientation_constraints.size() ; ++i)
    if (!planning_monitor_->getTransformListener()->frameExists(req.motion_plan_request.goal_constraints.orientation_constraints[i].header.frame_id))
    {
      ROS_ERROR("Frame '%s' is not defined for goal orientation constraint message %u", req.motion_plan_request.goal_constraints.orientation_constraints[i].header.frame_id.c_str(), i);
      return false;
    }
  for (unsigned int i = 0 ; i < req.motion_plan_request.path_constraints.position_constraints.size() ; ++i)
    if (!planning_monitor_->getTransformListener()->frameExists(req.motion_plan_request.path_constraints.position_constraints[i].header.frame_id))
    {
      ROS_ERROR("Frame '%s' is not defined for path position constraint message %u", req.motion_plan_request.path_constraints.position_constraints[i].header.frame_id.c_str(), i);
      return false;
    }
  
  for (unsigned int i = 0 ; i < req.motion_plan_request.path_constraints.orientation_constraints.size() ; ++i)
    if (!planning_monitor_->getTransformListener()->frameExists(req.motion_plan_request.path_constraints.orientation_constraints[i].header.frame_id))
    {
      ROS_ERROR("Frame '%s' is not defined for path orientation constraint message %u", req.motion_plan_request.path_constraints.orientation_constraints[i].header.frame_id.c_str(), i);
      return false;
    }
  
  return true;
}

void IKConstrainedPlanner::setWorkspaceBounds(motion_planning_msgs::WorkspaceParameters &workspace_parameters, 
                                              ompl::base::SpaceInformation *space_information)
{
  // set the workspace volume for planning
  if (planning_monitor_->getTransformListener()->frameExists(workspace_parameters.workspace_region_pose.header.frame_id))
  {
    bool err = false;
    try
    {
      planning_monitor_->getTransformListener()->transformPose(planning_monitor_->getFrameId(), workspace_parameters.workspace_region_pose, workspace_parameters.workspace_region_pose);
    }
    catch(...)
    {
      err = true;
    }
    if (err)
      ROS_ERROR("Unable to transform workspace bounds to planning frame");
    else
    {	    
      //TODO - this currently only works when the workspace is setup as a box
      if(workspace_parameters.workspace_region_shape.type == geometric_shapes_msgs::Shape::BOX && workspace_parameters.workspace_region_shape.dimensions.size() == 3)
      {
        /*        if (ompl_ros::ROSSpaceInformationKinematic *s = dynamic_cast<ompl_ros::ROSSpaceInformationKinematic*>(space_information))
        {
          double min_x = workspace_parameters.workspace_region_pose.pose.position.x-workspace_parameters.workspace_region_shape.dimensions[0]/2.0;
          double min_y = workspace_parameters.workspace_region_pose.pose.position.y-workspace_parameters.workspace_region_shape.dimensions[1]/2.0;
          double min_z = workspace_parameters.workspace_region_pose.pose.position.z-workspace_parameters.workspace_region_shape.dimensions[2]/2.0;
          
          double max_x = workspace_parameters.workspace_region_pose.pose.position.x+workspace_parameters.workspace_region_shape.dimensions[0]/2.0;
          double max_y = workspace_parameters.workspace_region_pose.pose.position.y+workspace_parameters.workspace_region_shape.dimensions[1]/2.0;
          double max_z = workspace_parameters.workspace_region_pose.pose.position.z+workspace_parameters.workspace_region_shape.dimensions[2]/2.0;
          s->setPlanningArea(min_x,min_y,max_x,max_y);
          s->setPlanningVolume(min_x,min_y,min_z,max_x,max_y,max_z);
        }
        */
      }
    }
  }
  else
    ROS_DEBUG("No workspace bounding volume was set");
}

void IKConstrainedPlanner::configureOnRequest(motion_planning_msgs::GetMotionPlan::Request &req, 
                                              const planning_models::KinematicState *start_state, 
                                              ompl::base::SpaceInformation *space_information)
{
  motion_planning_msgs::ArmNavigationErrorCodes error_code;
  /* clear memory */
  //  space_information->clearGoal();           // goal definitions
  //  space_information->clearStartStates();    // start states
  // Clear allowed contact regions
  planning_monitor_->clearAllowedContacts();
  planning_monitor_->revertAllowedCollisionToDefault();
//   std::vector<std::vector<bool> > matrix;
//   std::map<std::string, unsigned int> ind;  
//   planning_monitor_->getEnvironmentModel()->getCurrentAllowedCollisionMatrix(matrix, ind);
//   planning_monitor_->printAllowedCollisionMatrix(matrix, ind);
  planning_monitor_->revertCollisionSpacePaddingToDefault();
  planning_monitor_->clearAllowedContacts();
  planning_monitor_->clearConstraints();
  
  resetStateComponents();

  /* before configuring, we may need to update bounds on the state space + other path constraints */
  planning_monitor_->transformConstraintsToFrame(req.motion_plan_request.path_constraints, planning_monitor_->getFrameId(),error_code);
  planning_monitor_->setPathConstraints(req.motion_plan_request.path_constraints,error_code);
  setWorkspaceBounds(req.motion_plan_request.workspace_parameters, space_information);
  updateStateComponents(req.motion_plan_request.path_constraints);

  // TODO
  // space_information->setStateDistanceEvaluator(planner_setup->ompl_model->sde[distance_metric]);
    
  /* set the starting state */
  const unsigned int dim = space_information->getStateDimension();
  ompl::base::State *start = new ompl::base::State(dim);
    
  /* set the pose of the whole robot */
  planning_monitor_->getKinematicModel()->computeTransforms(start_state->getParams());
  planning_monitor_->getEnvironmentModel()->updateRobotModel();
    
  /* extract the components needed for the start state of the desired group */
  std::vector<std::string> joints = planning_monitor_->getKinematicModel()->getGroup(req.motion_plan_request.group_name)->jointNames;
  planning_models::KinematicModel::Link* link = planning_monitor_->getKinematicModel()->getJoint(joints.back())->after;
  btTransform end_effector_pose = link->globalTrans;
  double roll, pitch, yaw;
  end_effector_pose.getBasis().getRPY(roll,pitch,yaw);

  start->values[0] = end_effector_pose.getOrigin().x();
  start->values[1] = end_effector_pose.getOrigin().y();
  start->values[2] = end_effector_pose.getOrigin().z();

  start->values[3] = roll;
  start->values[4] = pitch;
  start->values[5] = yaw;

  start->values[6] = start_state->getParamsJoint(redundant_joint_map_[req.motion_plan_request.group_name])[0];
  ROS_INFO("Start state:");
  ROS_INFO("Position   :");

  ROS_INFO(" x         : %f",start->values[0]);
  ROS_INFO(" y         : %f",start->values[1]);
  ROS_INFO(" z         : %f",start->values[2]);

  ROS_INFO(" roll      : %f",start->values[3]);
  ROS_INFO(" pitch     : %f",start->values[4]);
  ROS_INFO(" yaw       : %f",start->values[5]);

  ROS_INFO(" redundancy: %f",start->values[6]);

  //  start_state->copyParamsGroup(start->values,req.motion_plan_request.group_name);    
  space_information->addStartState(start);
    
  /* add allowed contacts */
  motion_planning_msgs::OrderedCollisionOperations operations;
  std::vector<std::string> child_links;
  //  sensor_msgs::JointState joint_state = motion_planning_msgs::jointConstraintsToJointState(req.motion_plan_request.goal_constraints.joint_constraints);

  planning_monitor_->setCollisionSpace();
  planning_monitor_->setAllowedContacts(req.motion_plan_request.allowed_contacts);    
  planning_monitor_->getChildLinks(planning_monitor_->getKinematicModel()->getGroup(req.motion_plan_request.group_name)->jointNames,child_links);

  ROS_DEBUG("Moving links are");
  for(unsigned int i=0; i < child_links.size(); i++)
  {
    ROS_DEBUG("%s",child_links[i].c_str());
  }

  planning_monitor_->getOrderedCollisionOperationsForOnlyCollideLinks(child_links,req.motion_plan_request.ordered_collision_operations,operations);
  ROS_DEBUG("Ordered collision operations");
  for(unsigned int i=0; i < operations.collision_operations.size(); i++)
  {
    ROS_DEBUG("%s :: %s :: %d",operations.collision_operations[i].object1.c_str(),operations.collision_operations[i].object2.c_str(),operations.collision_operations[i].operation);
  }
  planning_monitor_->applyLinkPaddingToCollisionSpace(req.motion_plan_request.link_padding);
  planning_monitor_->applyOrderedCollisionOperationsToCollisionSpace(operations);

//   std::vector<std::vector<bool> > matrix2;
//   std::map<std::string, unsigned int> ind2;  
//   planning_monitor_->getEnvironmentModel()->getCurrentAllowedCollisionMatrix(matrix2, ind2);
//   planning_monitor_->printAllowedCollisionMatrix(matrix2, ind2);


  /* add goal state */
  planning_monitor_->transformConstraintsToFrame(req.motion_plan_request.goal_constraints, planning_monitor_->getFrameId(),error_code);
  planning_monitor_->setGoalConstraints(req.motion_plan_request.goal_constraints,error_code);
  ROS_INFO("Setting goal for space information");
  ompl::base::Goal *goal = computeGoalFromConstraints(space_information,req.motion_plan_request.goal_constraints,req.motion_plan_request.group_name);
  space_information->setGoal(goal);
  
  printSettings(space_information);
}

void IKConstrainedPlanner::printSettings(ompl::base::SpaceInformation *si)
{
  std::stringstream ss;
  si->printSettings(ss);
  ROS_DEBUG("%s", ss.str().c_str());
}

void IKConstrainedPlanner::updateStateComponents(const motion_planning_msgs::Constraints &constraints)
{
  // Setup the state first
  std::vector<ompl::base::StateComponent> state_specification = original_state_specification_;

  // For now assume that the position constraints are constraints on the end-effector link. 
  // TODO: will need to check this properly eventually
  if(constraints.position_constraints.size() == 1)
  {
    if(constraints.position_constraints[0].constraint_region_shape.type == geometric_shapes_msgs::Shape::BOX)
    {
          double min_x = constraints.position_constraints[0].position.x-constraints.position_constraints[0].constraint_region_shape.dimensions[0]/2.0;
          double min_y = constraints.position_constraints[0].position.y-constraints.position_constraints[0].constraint_region_shape.dimensions[1]/2.0;
          double min_z = constraints.position_constraints[0].position.z-constraints.position_constraints[0].constraint_region_shape.dimensions[2]/2.0;
          
          double max_x = constraints.position_constraints[0].position.x+constraints.position_constraints[0].constraint_region_shape.dimensions[0]/2.0;
          double max_y = constraints.position_constraints[0].position.y+constraints.position_constraints[0].constraint_region_shape.dimensions[1]/2.0;
          double max_z = constraints.position_constraints[0].position.z+constraints.position_constraints[0].constraint_region_shape.dimensions[2]/2.0;

          state_specification[0].minValue = min_x;
          state_specification[0].maxValue = max_x;

          state_specification[1].minValue = min_y;
          state_specification[1].maxValue = max_y;

          state_specification[2].minValue = min_z;
          state_specification[2].maxValue = max_z;
    }
  }

  if(constraints.orientation_constraints.size() == 1)
  {
    btQuaternion orientation;
    double roll,pitch,yaw;
    tf::quaternionMsgToTF(constraints.orientation_constraints[0].orientation,orientation);
    btMatrix3x3 rotation(orientation);
    rotation.getRPY(roll,pitch,yaw);

    double min_roll = roll - constraints.orientation_constraints[0].absolute_roll_tolerance;
    double max_roll = roll + constraints.orientation_constraints[0].absolute_roll_tolerance;

    double min_pitch = pitch - constraints.orientation_constraints[0].absolute_pitch_tolerance;
    double max_pitch = pitch + constraints.orientation_constraints[0].absolute_pitch_tolerance;

    double min_yaw = yaw - constraints.orientation_constraints[0].absolute_yaw_tolerance;
    double max_yaw = yaw + constraints.orientation_constraints[0].absolute_yaw_tolerance;

    state_specification[3].minValue = min_pitch;
    state_specification[3].maxValue = max_pitch;
    
    state_specification[4].minValue = min_roll;
    state_specification[4].maxValue = max_roll;
    
    state_specification[5].minValue = min_yaw;
    state_specification[5].maxValue = max_yaw;
  }
  space_information_->setStateComponents(state_specification);
}

void IKConstrainedPlanner::resetStateComponents()
{
  space_information_->setStateComponents(original_state_specification_);  
}

planning_models::KinematicState* IKConstrainedPlanner::fillStartState(const motion_planning_msgs::RobotState &robot_state)
{
  motion_planning_msgs::ArmNavigationErrorCodes error_code;
  planning_models::KinematicState *kinematic_state = new planning_models::KinematicState(planning_monitor_->getKinematicModel());
  if (!planning_monitor_->getTransformListener()->frameExists(robot_state.joint_state.header.frame_id))
  {
    ROS_ERROR("Frame '%s' in starting state is unknown.", robot_state.joint_state.header.frame_id.c_str());
  }
  std::vector<double> tmp;
  tmp.resize(1);
  
  for(unsigned int i=0; i < robot_state.joint_state.name.size(); i++)
  {
    double tmp_state = robot_state.joint_state.position[i];
    std::string tmp_name = robot_state.joint_state.name[i];
    roslib::Header tmp_header = robot_state.joint_state.header;
    std::string tmp_frame = planning_monitor_->getFrameId();
    
    if (planning_monitor_->transformJointToFrame(tmp_state, tmp_name, tmp_header, tmp_frame, error_code))
    {
      tmp[0] = tmp_state;
      kinematic_state->setParamsJoint(tmp, robot_state.joint_state.name[i]);
    }
  }
  if (kinematic_state->seenAll())
    return kinematic_state;
  else
  {
    if (planning_monitor_->haveState())
    {
      ROS_INFO("Using the current state to fill in the starting state for the motion plan");
      std::vector<const planning_models::KinematicModel::Joint*> joints;
      planning_monitor_->getKinematicModel()->getJoints(joints);
      for (unsigned int i = 0 ; i < joints.size() ; ++i)
        if (!kinematic_state->seenJoint(joints[i]->name))
          kinematic_state->setParamsJoint(planning_monitor_->getRobotState()->getParamsJoint(joints[i]->name), joints[i]->name);
      return kinematic_state;
    }
  }
  delete kinematic_state;
  return NULL;
}

bool IKConstrainedPlanner::computePlan(motion_planning_msgs::GetMotionPlan::Request &req, 
                                       motion_planning_msgs::GetMotionPlan::Response &res)
{

  planning_models::KinematicState *start_state = fillStartState(req.motion_plan_request.start_state);
  if (start_state)
  {
    std::stringstream ss;
    start_state->print(ss);
    ROS_DEBUG("Complete starting state:\n%s", ss.str().c_str());
  }
  else
  {
    ROS_ERROR("Starting robot state is unknown. Cannot start plan.");
    return false;
  }

  if (!isRequestValid(req))
    return false;
    
  // get the planner setup
  ompl::base::Planner *planner;
  planner = planner_map_[req.motion_plan_request.planner_id+"["+req.motion_plan_request.group_name+"]"];  

  ROS_INFO("Planner %x",planner);

  // choose the kinematics solver
  if(kinematics_solver_map_.find(req.motion_plan_request.group_name) != kinematics_solver_map_.end())
    kinematics_solver_ = kinematics_solver_map_[req.motion_plan_request.group_name];
  else
  {
    ROS_INFO("Could not find kinematics solver for group %s",req.motion_plan_request.group_name.c_str());
    return false;
  }
  
  geometry_msgs::PoseStamped tmp_frame;
  tmp_frame.pose.orientation.w = 1.0;
  tmp_frame.header.stamp = ros::Time(0.0);
  tmp_frame.header.frame_id = planning_monitor_->getFrameId();
  ROS_INFO("Trying to transform from planner: %s to kinematics: %s frames",planning_monitor_->getFrameId().c_str(),(kinematics_solver_->getBaseFrame()).c_str());
  tf_listener_.transformPose(kinematics_solver_->getBaseFrame(), 
                             tmp_frame, 
                             kinematics_planner_frame_);

  ik_constrained_planner::IKStateValidator *validity_checker;
  validity_checker = dynamic_cast<ik_constrained_planner::IKStateValidator*> (validity_checker_);
  validity_checker->configure(req.motion_plan_request.group_name,
                              redundant_joint_map_[req.motion_plan_request.group_name],
                              kinematics_planner_frame_.pose,
                              kinematics_solver_);
  
  planning_monitor_->getEnvironmentModel()->lock();
  planning_monitor_->getKinematicModel()->lock();
  
  // configure the planner
  configureOnRequest(req,start_state,space_information_);
    
  /* compute actual motion plan */
  Solution sol;
  sol.path = NULL;
  sol.difference = 0.0;
  sol.approximate = false;
  callPlanner(planner,space_information_,req,sol);
    
  planning_monitor_->getEnvironmentModel()->unlock();
  planning_monitor_->getKinematicModel()->unlock();
  
  space_information_->clearGoal();
  space_information_->clearStartStates();
  
  /* copy the solution to the result */
  if (sol.path)
  {
    fillResult(req,res,sol);
    delete sol.path;
  }  
  delete start_state;
  return true;
}

void IKConstrainedPlanner::fillResult(motion_planning_msgs::GetMotionPlan::Request &req, 
                                               motion_planning_msgs::GetMotionPlan::Response &res, 
                                               const Solution &sol)
{   
  std::vector<const planning_models::KinematicModel::Joint*> joints;
  planning_monitor_->getKinematicModel()->getJoints(joints);
  double state_delay = req.motion_plan_request.expected_path_dt.toSec();
  ompl::kinematic::PathKinematic *kpath = dynamic_cast<ompl::kinematic::PathKinematic*>(sol.path);
  if (kpath)
  {
    res.trajectory.joint_trajectory.points.resize(kpath->states.size());
    res.trajectory.joint_trajectory.joint_names = planning_monitor_->getKinematicModel()->getGroup(req.motion_plan_request.group_name)->jointNames;
    
    const unsigned int dim = space_information_->getStateDimension();
    for (unsigned int i = 0 ; i < kpath->states.size() ; ++i)
    {
      res.trajectory.joint_trajectory.points[i].time_from_start = ros::Duration(i * state_delay);
      res.trajectory.joint_trajectory.points[i].positions.resize(dim);
      
      btVector3 tmp_pos(kpath->states[i]->values[0],kpath->states[i]->values[1],kpath->states[i]->values[2]);
      btQuaternion tmp_rot;
      tmp_rot.setRPY(kpath->states[i]->values[3],kpath->states[i]->values[4],kpath->states[i]->values[5]);
      btTransform tmp_transform(tmp_rot,tmp_pos), kinematics_planner_tf;
      tf::poseMsgToTF(kinematics_planner_frame_.pose,kinematics_planner_tf);
      btTransform result = kinematics_planner_tf*tmp_transform;
      geometry_msgs::Pose pose;
      tf::poseTFToMsg(result,pose);      
      std::vector<double> solution;
      std::vector<double> seed;
      seed.resize(dim,0.0);
      seed[2] = kpath->states[i]->values[6];      
      if(!kinematics_solver_->getPositionIK(pose,seed,solution))
      {
        ROS_WARN("IK invalid");
        continue;
      }
      else
      {
        ROS_DEBUG("%d: %f %f %f %f %f %f %f",i,solution[0],solution[1],solution[2],solution[3],solution[4],solution[5],solution[6]);
        for (unsigned int j = 0 ; j < dim ; ++j)
        {
          res.trajectory.joint_trajectory.points[i].positions[j] = solution[j];
        }
      }
    }
  }
    
  /*  ompl::dynamic::PathDynamic *dpath = dynamic_cast<ompl::dynamic::PathDynamic*>(sol.path);
  if (dpath)
  {
    res.trajectory.joint_trajectory.points.resize(dpath->states.size());
    res.trajectory.joint_trajectory.joint_names = planning_monitor_->getKinematicModel()->getGroup(req.motion_plan_request.group_name)->jointNames;
    
    const unsigned int dim = space_information_->getStateDimension();
    for (unsigned int i = 0 ; i < dpath->states.size() ; ++i)
    {
      res.trajectory.joint_trajectory.points[i].time_from_start = ros::Duration(i * state_delay);
      res.trajectory.joint_trajectory.points[i].positions.resize(dim);
      for (unsigned int j = 0 ; j < dim ; ++j)
        res.trajectory.joint_trajectory.points[i].positions[j] = kpath->states[i]->values[j];
    }
    }*/
  assert(kpath);    
}

bool IKConstrainedPlanner::callPlanner(ompl::base::Planner *planner,
                                       ompl::base::SpaceInformation *space_information,
                                       motion_planning_msgs::GetMotionPlan::Request &req, 
                                       Solution &sol)
{
  int times = req.motion_plan_request.num_planning_attempts;
  double allowed_time = req.motion_plan_request.allowed_planning_time.toSec();
  if (times <= 0)
  {
    ROS_ERROR("Motion plan cannot be computed %d times", times);
    return false;
  }
    
  if (dynamic_cast<ompl::base::GoalRegion*>(space_information->getGoal()))
    ROS_DEBUG("Goal threshold is %g", dynamic_cast<ompl::base::GoalRegion*>(space_information->getGoal())->threshold);
    
  unsigned int t_index = 0;
  double t_distance = 0.0;
  bool result = planner->isTrivial(&t_index, &t_distance);
    
  if (result)
  {
    ROS_INFO("Solution already achieved");
    sol.difference = t_distance;
    sol.approximate = false;
	
    /* we want to maintain the invariant that a path will
       at least consist of start & goal states, so we copy
       the start state twice */
    ompl::kinematic::PathKinematic *kpath = new ompl::kinematic::PathKinematic(space_information);
    ompl::base::State *s0 = new ompl::base::State(space_information->getStateDimension());
    ompl::base::State *s1 = new ompl::base::State(space_information->getStateDimension());
    space_information->copyState(s0, space_information->getStartState(t_index));
    space_information->copyState(s1, space_information->getStartState(t_index));
    kpath->states.push_back(s0);
    kpath->states.push_back(s1);
    sol.path = kpath;
  }
  else
  {		
    /* do the planning */
    sol.path = NULL;
    sol.difference = 0.0;
    double totalTime = 0.0;
    ompl::base::Goal *goal = space_information->getGoal();
    
    for (int i = 0 ; i < times ; ++i)
    {
      ros::WallTime startTime = ros::WallTime::now();
      bool ok = planner->solve(allowed_time); 
      double tsolve = (ros::WallTime::now() - startTime).toSec();	
      ROS_INFO("%s Motion planner spent %g seconds", (ok ? "[Success]" : "[Failure]"), tsolve);
      totalTime += tsolve;
      
      bool all_ok = ok;
      
      if (ok)
      {
        ompl::kinematic::PathKinematic *path = dynamic_cast<ompl::kinematic::PathKinematic*>(goal->getSolutionPath());
        
        ROS_INFO_STREAM("Path out of planner consists of " << path->length() << " states");
                
        /* do path smoothing, if we are doing kinematic planning */
        /*        if (smoother_)
        {
          ompl::kinematic::PathKinematic *path = dynamic_cast<ompl::kinematic::PathKinematic*>(goal->getSolutionPath());
          if (path)
          {
            ros::WallTime startTime = ros::WallTime::now();
            smoother_->smoothMax(path);
            double tsmooth = (ros::WallTime::now() - startTime).toSec();
            ROS_INFO("          Smoother spent %g seconds (%g seconds in total)", tsmooth, tsmooth + tsolve);
            //TODO            dynamic_cast<ompl_ros::ROSSpaceInformationKinematic*>(space_information)->interpolatePath(path, 1.0);
          }
          }*/
        
        dynamic_cast<ompl::kinematic::SpaceInformationKinematic*>(space_information)->interpolatePath(path, 1.0);

        if (sol.path == NULL || sol.difference > goal->getDifference() || 
            (sol.path && sol.difference == goal->getDifference() && sol.path->length() > goal->getSolutionPath()->length()))
        {
          if (sol.path)
            delete sol.path;
          sol.path = goal->getSolutionPath();
          sol.difference = goal->getDifference();
          sol.approximate = goal->isApproximate();
          goal->forgetSolutionPath();
          ROS_DEBUG("          Obtained better solution: distance is %f", sol.difference);
        }
      } else {
        ROS_INFO("Something not ok in ompl");
      }
      
      if(all_ok) {
        ROS_INFO("Ompl reports ok");
      } else {
        ROS_INFO("Ompl not ok");
      }
      
      //      if (onFinishPlan_)
      //        onFinishPlan_(planner_setup);
	    
      planner->clear();	    
    }
    
    ROS_DEBUG("Total planning time: %g; Average planning time: %g", totalTime, (totalTime / (double)times));
  }
  return result;
}

ompl::base::Goal* IKConstrainedPlanner::computeGoalFromConstraints(ompl::base::SpaceInformation *space_information, 
                                                                   motion_planning_msgs::Constraints &constraints,
                                                                   const std::string &group_name)
{
  double redundancy;
  if(!computeRedundancyFromConstraints(constraints,
                                       kinematics_planner_frame_,
                                       kinematics_solver_,
                                       redundant_joint_map_[group_name],
                                       redundancy))
  {
    ROS_INFO("Could not find solution for goal");
    return NULL;
  }

  return new IKConstrainedGoal(space_information,
                               constraints,
                               redundancy);
}

ompl::base::ProjectionEvaluator* IKConstrainedPlanner::getProjectionEvaluator(boost::shared_ptr<ompl_planning::PlannerConfig> &options, 
                                                                              ompl::base::SpaceInformation * space_information) const
{
  ompl::base::ProjectionEvaluator *pe = NULL;  
  if (options->hasParam("projection") && options->hasParam("celldim"))
  {
    std::string proj = options->getParamString("projection");
    std::string celldim = options->getParamString("celldim");
    
    if (proj.substr(0, 4) == "link")
    {
	    std::string linkName = proj.substr(4);
	    boost::trim(linkName);
	    pe = new ik_constrained_planner::IKProjectionEvaluator(space_information, planning_monitor_, linkName);
    }
    else
    {
	    std::vector<unsigned int> projection;
	    std::stringstream ss(proj);
	    while (ss.good())
	    {
        unsigned int comp;
        ss >> comp;
        projection.push_back(comp);
	    }
	    pe = new ompl::base::OrthogonalProjectionEvaluator(space_information, projection);
    }
    
    std::vector<double> cdim;
    std::stringstream ss(celldim);
    while (ss.good())
    {
	    double comp;
	    ss >> comp;
	    cdim.push_back(comp);
    }
    
    pe->setCellDimensions(cdim);
    ROS_DEBUG("Projection is set to %s", proj.c_str());
    ROS_DEBUG("Cell dimensions set to %s", celldim.c_str());
  }  
  return pe;
}

bool IKConstrainedPlanner::initializeSpaceInformation(planning_environment::PlanningMonitor *planning_monitor_,
                                                      const std::string &param_server_prefix)
{
  space_information_ = new ompl::kinematic::SpaceInformationKinematic();    

  // Setup the state first
  std::vector<ompl::base::StateComponent> state_specification;
  state_specification.resize(IK_CONSTRAINED_DIMENSION);  
  for(unsigned int i=0; i < IK_CONSTRAINED_DIMENSION; i++)
  {
    getStateSpecs(param_server_prefix,state_names_[i],state_specification[i]);
  }  
  original_state_specification_ = state_specification;
  space_information_->setStateComponents(state_specification);

  //Start and goal states will be setup on request but they need to be allocated before hand


  validity_checker_ = new IKStateValidator(space_information_,
                                           planning_monitor_);
  space_information_->setStateValidityChecker(validity_checker_);
  space_information_->setup();

  return true;
}

bool IKConstrainedPlanner::getStateSpecs(const std::string &param_server_prefix, 
                                         const std::string &state_name,
                                         ompl::base::StateComponent &state_specification)
{
  if(state_name == "x" || state_name == "y" || state_name == "z")
    state_specification.type = ompl::base::StateComponent::LINEAR;
  else if(state_name == "roll" || state_name == "pitch" || state_name == "yaw" || state_name == "redundancy")
    state_specification.type = ompl::base::StateComponent::WRAPPING_ANGLE;
  else
  {
    ROS_INFO("Could not recognize state_name: %s",state_name.c_str());
    return false;
  }
  if(!node_handle_.getParam(param_server_prefix+"/"+state_name+"/min",state_specification.minValue))
  {
    if(state_specification.type == ompl::base::StateComponent::LINEAR)
      state_specification.minValue = IK_CONSTRAINED_MIN_LINEAR_STATE;
    if(state_specification.type == ompl::base::StateComponent::WRAPPING_ANGLE)
      state_specification.minValue = -M_PI;
  }
  if(!node_handle_.getParam(param_server_prefix+"/"+state_name+"/max",state_specification.maxValue))
  {
    if(state_specification.type == ompl::base::StateComponent::LINEAR)
      state_specification.maxValue = IK_CONSTRAINED_MAX_LINEAR_STATE;
    if(state_specification.type == ompl::base::StateComponent::WRAPPING_ANGLE)
      state_specification.maxValue = M_PI;
  }
  if(!node_handle_.getParam(param_server_prefix+"/"+state_name+"/resolution",state_specification.resolution))
  {
    if(state_specification.type == ompl::base::StateComponent::LINEAR)
      state_specification.resolution = IK_CONSTRAINED_RESOLUTION_LINEAR_STATE;
    if(state_specification.type == ompl::base::StateComponent::WRAPPING_ANGLE)
      state_specification.resolution = IK_CONSTRAINED_RESOLUTION_WRAPPING_ANGLE;
  }

  ROS_INFO("Setting up state %s",state_name.c_str());
  ROS_INFO("Min            : %f",state_specification.minValue);
  ROS_INFO("Max            : %f",state_specification.maxValue);
  ROS_INFO("Resolution     : %f\n",state_specification.resolution);

  return true;
}

bool IKConstrainedPlanner::initializeKinematics(const std::string &param_server_prefix,
                                                const std::vector<std::string> &group_names)
{
  for(unsigned int i=0; i < group_names.size(); i++)
  {
    std::string group_name = group_names[i];
    std::string kinematics_solver_name, redundant_joint_name;
    if(!node_handle_.getParam(param_server_prefix+"/"+group_name+"/kinematics_solver",kinematics_solver_name))
    {
      ROS_ERROR("Could not find parameter %s on param server",(param_server_prefix+"/"+group_name+"/kinematics_solver").c_str());
      return false;    
    }
    else
    {
      ROS_INFO("Kinematics solver name is %s",kinematics_solver_name.c_str());
    }
    if(!kinematics_loader_.isClassAvailable(kinematics_solver_name))
    {
      ROS_ERROR("pluginlib does not have the class %s",kinematics_solver_name.c_str());
      return false;
    }

    try
    {
      kinematics_solver_map_[group_name] = kinematics_loader_.createClassInstance(kinematics_solver_name);
    }
    catch(pluginlib::PluginlibException& ex)    //handle the class failing to load
    {
      ROS_ERROR("The plugin failed to load. Error: %s", ex.what());
      return false;
    }
    if(!kinematics_solver_map_[group_name]->initialize(group_name))
    {
      ROS_ERROR("Could not initialize kinematics solver for group %s",group_name.c_str());
      return false;    
    }

    ROS_INFO("Base frame: %s",kinematics_solver_map_[group_name]->getBaseFrame().c_str());
    // Find all the redundant joint name
    ompl::base::StateComponent state_specification;
    if(!node_handle_.getParam(param_server_prefix+"/"+group_name+"/redundancy/name",redundant_joint_name))
    {
      ROS_ERROR("Could not find parameter %s on parameter server",(param_server_prefix+"/"+group_name+"/redundancy/name").c_str());
      return false;    
    }
    planning_models::KinematicModel::Joint *joint = planning_monitor_->getKinematicModel()->getJoint(redundant_joint_name);
    if(joint == NULL)
    {
      ROS_ERROR("Could not find joint %s in kinematic model",redundant_joint_name.c_str());
      return false;
    }
    if(static_cast<planning_models::KinematicModel::RevoluteJoint*>(joint))
    {
      planning_models::KinematicModel::RevoluteJoint* revolute_joint = static_cast<planning_models::KinematicModel::RevoluteJoint*>(joint);
      state_specification.minValue = revolute_joint->lowLimit;
      state_specification.maxValue = revolute_joint->hiLimit;
      if(revolute_joint->continuous)
        state_specification.type = ompl::base::StateComponent::WRAPPING_ANGLE;
      else
        state_specification.type = ompl::base::StateComponent::LINEAR;
    }
    else
    {
      ROS_ERROR("This planner cannot deal with a non-revolute joint (for now).");
      return false;
    }
    node_handle_.getParam(param_server_prefix+"/"+group_name+"/redundancy/min",state_specification.minValue);
    node_handle_.getParam(param_server_prefix+"/"+group_name+"/redundancy/max",state_specification.maxValue); 

    if(!(node_handle_.getParam(param_server_prefix+"/"+group_name+"/redundancy/resolution",state_specification.resolution)))
      state_specification.resolution = IK_CONSTRAINED_RESOLUTION_WRAPPING_ANGLE;

    redundancy_map_[group_name] = state_specification;
    redundant_joint_map_[group_name] = redundant_joint_name;
  }                                               
  return true;
}

bool IKConstrainedPlanner::getGroupNames(const std::string &param_server_prefix, 
                                         std::vector<std::string> &group_names)
{
  XmlRpc::XmlRpcValue group_list;
  if(!node_handle_.getParam(param_server_prefix+"/groups", group_list))
  {
    ROS_ERROR("Could not find parameter %s on param server",(param_server_prefix+"/groups").c_str());
    return false;
  }
  if(group_list.getType() != XmlRpc::XmlRpcValue::TypeArray)
  {
    ROS_ERROR("Group list should be of XmlRpc Array type");
    return false;
  }
  for (int32_t i = 0; i < group_list.size(); ++i) 
  {
    if(group_list[i].getType() != XmlRpc::XmlRpcValue::TypeString)
    {
      ROS_ERROR("Group names should be strings");
      return false;
    }
    group_names.push_back(static_cast<std::string>(group_list[i]));
    ROS_INFO("Adding group: %s",group_names.back().c_str());
  }
  return true;
}

bool IKConstrainedPlanner::addPlanner(const std::string &param_server_prefix,
                                      const std::string &planner_config_name,
                                      const std::string &model_name,
                                      ompl::base::SpaceInformation* space_information)
{	
  boost::shared_ptr<ompl_planning::PlannerConfig> cfg;
  cfg.reset(new ompl_planning::PlannerConfig(param_server_prefix,planner_config_name));
  std::string type = cfg->getParamString("type");

  if (type == "kinematic::SBL")
  {
    if(!addPlanner(cfg,model_name,space_information))
    {
      ROS_ERROR("Could not add planner of type: %s",type.c_str());
      return false;
    }
  }
  else if (type == "kinematic::LBKPIECE")
  {
    if(!addPlanner(cfg,model_name,space_information))
    {
      ROS_ERROR("Could not add planner of type: %s",type.c_str());
      return false;
    }
  }
  else
  {
    ROS_WARN("Unknown planner type: %s", type.c_str());
    return false;
  }
  return true;
}

bool IKConstrainedPlanner::addPlanner(boost::shared_ptr<ompl_planning::PlannerConfig> &cfg, 
                                      const std::string &model_name,
                                      ompl::base::SpaceInformation* space_information)
{
  std::string type = cfg->getParamString("type");
  ompl::base::Planner *p;    
  if (type == "kinematic::SBL")
  {
    ompl::kinematic::SBL *planner = new ompl::kinematic::SBL(dynamic_cast<ompl::kinematic::SpaceInformationKinematic*>(space_information));
    if (cfg->hasParam("range"))
    {
      planner->setRange(cfg->getParamDouble("range", planner->getRange()));
      ROS_DEBUG("Range is set to %g", planner->getRange());
    }
    /*    if (cfg->hasParam("thread_count"))
    {
      planner->setThreadCount(cfg->getParamInt("thread_count", planner->getThreadCount()));
      ROS_DEBUG("Thread count is set to %u", planner->getThreadCount());
      }*/
    planner->setProjectionEvaluator(getProjectionEvaluator(cfg,space_information));    
    if (planner->getProjectionEvaluator() == NULL)
    {
      ROS_WARN("Adding %s failed: need to set both 'projection' and 'celldim' for %s", type.c_str(), model_name.c_str());
      return false;
    }
    p = planner;
  }
  else if (type == "kinematic::LBKPIECE")
  {
    ompl::kinematic::LBKPIECE1 *planner = new ompl::kinematic::LBKPIECE1(dynamic_cast<ompl::kinematic::SpaceInformationKinematic*>(space_information));
    if (cfg->hasParam("range"))
    {
      planner->setRange(cfg->getParamDouble("range", planner->getRange()));
      ROS_DEBUG("Range is set to %g", planner->getRange());
    }
    planner->setProjectionEvaluator(getProjectionEvaluator(cfg,space_information));    
    if (planner->getProjectionEvaluator() == NULL)
    {
      ROS_WARN("Adding %s failed: need to set both 'projection' and 'celldim' for %s", type.c_str(), model_name.c_str());
      return false;
    }
    p = planner;
  }
  else
  {
   ROS_WARN("Unknown planner type: %s", type.c_str());
   return false;
  }

  p->setup();
  std::string location = type + "[" + model_name + "]";
  if (planner_map_.find(location) != planner_map_.end())
  {
    ROS_WARN("Re-definition of '%s'", location.c_str());
    delete planner_map_[location];
  }
  planner_map_[location] = p;
  return true;
}

bool IKConstrainedPlanner::initializePlanners(planning_environment::PlanningMonitor *planning_monitor,
                                              const std::string &param_server_prefix,
                                              const std::vector<std::string> &group_names)
{
  for (unsigned int i = 0; i < group_names.size(); ++i) 
  {
    XmlRpc::XmlRpcValue planner_list;
    if(!node_handle_.getParam(param_server_prefix+"/"+group_names[i]+"/planner_configs", planner_list))
    {
      ROS_ERROR("Could not find parameter %s on param server",(param_server_prefix+"/"+group_names[i]+"/planner_configs").c_str());
      return false;
    }
    ROS_ASSERT(planner_list.getType() == XmlRpc::XmlRpcValue::TypeArray);    
    for (int32_t j = 0; j < planner_list.size(); ++j) 
    {
      if(planner_list[j].getType() != XmlRpc::XmlRpcValue::TypeString)
      {
        ROS_ERROR("Planner names must be of type string");
        return false;
      }
      std::string planner_config = static_cast<std::string>(planner_list[j]);
      if(!addPlanner(param_server_prefix,planner_config,group_names[i],space_information_))
      {
        ROS_ERROR("Could not add planner for group %s and planner_config %s",group_names[i].c_str(),planner_config.c_str());
        return false;
      }
    }
  }
  return true;
}

bool IKConstrainedPlanner::initialize(planning_environment::PlanningMonitor *planning_monitor,
                                      const std::string &param_server_prefix)
{  
  state_names_.push_back("x");
  state_names_.push_back("y");
  state_names_.push_back("z");

  state_names_.push_back("roll");
  state_names_.push_back("pitch");
  state_names_.push_back("yaw");
  state_names_.push_back("redundancy");
  default_planner_id_ = "kinematic::SBL";

  planning_monitor_ = planning_monitor;

  getGroupNames(param_server_prefix,group_names_);

  solver_initialized_ = false;
  if(!initializeKinematics(param_server_prefix,group_names_))
  {
    ROS_ERROR("Failed to initialize kinematics solvers");
    return false;
  }
  // std::string base_frame = kinematics_solver_->getBaseFrame();
  ROS_INFO("Base frame for kinematics: %s",kinematics_solver_map_["right_arm"]->getBaseFrame().c_str());
  if(!initializeSpaceInformation(planning_monitor_,
                                 param_server_prefix))
  {
    ROS_ERROR("Failed to initialize space information");
    return false;
  }

  if(!initializePlanners(planning_monitor_,
                         param_server_prefix,
                         group_names_))
  {
    ROS_ERROR("Failed to initialize planners");
    return false;
  }

  return true;
} 



}

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

/** \author Ioan Sucan */

#include <planning_models/kinematic_state.h>
#include <ros/console.h>
#include <ros/time.h>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <climits>

planning_models::KinematicState::KinematicState(const KinematicModelConstPtr &model) : owner_(model)
{
    randSeed_ = ros::WallTime::now().toNSec() % UINT_MAX;
    params_ = model->getDimension() > 0 ? new double[model->getDimension()] : NULL;
    updated_.resize(model->getDimension(), false);
    defaultParams();
    reset();
}

planning_models::KinematicState::KinematicState(const KinematicState &sp) : owner_(sp.owner_), params_(NULL)
{   
    randSeed_ = ros::WallTime::now().toNSec() % UINT_MAX;
    copyFrom(sp);
}

planning_models::KinematicState::~KinematicState(void)
{
    if (params_)
	delete[] params_;
}

planning_models::KinematicState& planning_models::KinematicState::operator=(const KinematicState &rhs) 
{
    if (this != &rhs)
	copyFrom(rhs);
    return *this;
}

bool planning_models::KinematicState::operator==(const KinematicState &rhs) const
{
    const unsigned int dim = owner_->getDimension();
    if (dim != rhs.owner_->getDimension())
	return false;
    for (unsigned int i = 0 ; i < dim ; ++i)
	if (fabs(params_[i] - rhs.params_[i]) > DBL_MIN)
	    return false;
    return true;
}

bool planning_models::KinematicState::operator!=(const KinematicState &rhs) const
{
    return !(*this == rhs);
}

void planning_models::KinematicState::copyFrom(const KinematicState &sp)
{
    owner_ = sp.owner_;
    if (params_)
	delete[] params_;
    const unsigned int dim = owner_->getDimension();
    params_ = dim > 0 ? new double[dim] : NULL;
    if (params_)
	for (unsigned int i = 0 ; i < dim ; ++i)
	    params_[i] = sp.params_[i];
    updated_ = sp.updated_;
}

unsigned int planning_models::KinematicState::getDimension(void) const
{
    return owner_->getDimension();
}

void planning_models::KinematicState::defaultParams(void)
{
    const unsigned int dim = owner_->getDimension();
    const std::vector<double> &bounds = owner_->getStateBounds();
    
    for (unsigned int i = 0 ; i < dim ; ++i)
    {
	const unsigned int i2 = i << 1;
	if (bounds[i2] <= 0.0 && bounds[i2 + 1] >= 0.0)
	    params_[i] = 0.0;
	else
	    params_[i] = (bounds[i2] + bounds[i2 + 1]) / 2.0;
	updated_[i] = true;
    }
}

void planning_models::KinematicState::defaultParamsGroup(const std::string &group)
{
    defaultParamsGroup(owner_->getGroup(group));
}

void planning_models::KinematicState::defaultParamsGroup(const KinematicModel::JointGroup *group)
{
    const std::vector<double> &bounds = owner_->getStateBounds();
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	const unsigned int j  = group->state_index[i];
	const unsigned int j2 = j << 1;

	if (bounds[j2] <= 0.0 && bounds[j2 + 1] >= 0.0)
	    params_[j] = 0.0;
	else
	    params_[j] = (bounds[j2] + bounds[j2 + 1]) / 2.0;
    }
}

void planning_models::KinematicState::randomParamsGroup(const std::string &group)
{
    randomParamsGroup(owner_->getGroup(group));
}

void planning_models::KinematicState::randomParamsGroup(const KinematicModel::JointGroup *group)
{
    const std::vector<double> &bounds = owner_->getStateBounds();
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	const unsigned int j  = group->state_index[i];
	const unsigned int j2 = j << 1;
	params_[j] = (bounds[j2 + 1] - bounds[j2]) * ((double)rand_r(&randSeed_) / (RAND_MAX + 1.0)) +  bounds[j2];
	updated_[j] = true;
    }
}

void planning_models::KinematicState::randomParams(void)
{   
    const std::vector<double> &bounds = owner_->getStateBounds();
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
    {
	const unsigned int i2 = i << 1;
	params_[i] = (bounds[i2 + 1] - bounds[i2]) * ((double)rand_r(&randSeed_) / (RAND_MAX + 1.0)) + bounds[i2];
	updated_[i] = true;
    }
}

void planning_models::KinematicState::perturbParamsGroup(double factor, const std::string &group)    
{   
    perturbParamsGroup(factor, owner_->getGroup(group));
}

void planning_models::KinematicState::perturbParamsGroup(double factor, const KinematicModel::JointGroup *group)
{   
    const std::vector<double> &bounds = owner_->getStateBounds();
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	const unsigned int j  = group->state_index[i];
	const unsigned int j2 = j << 1;
	params_[j] += factor * (bounds[j2 + 1] - bounds[j2]) * (2.0 * ((double)rand_r(&randSeed_) / (RAND_MAX + 1.0)) - 1.0);
    }
    enforceBoundsGroup(group);
}

void planning_models::KinematicState::perturbParams(double factor)
{   
    const std::vector<double> &bounds = owner_->getStateBounds();
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
    {
	const unsigned int i2 = i << 1;
	params_[i] += factor * (bounds[i2 + 1] - bounds[i2]) * (2.0 * ((double)rand_r(&randSeed_) / (RAND_MAX + 1.0)) - 1.0);
    }
    enforceBounds();
}

void planning_models::KinematicState::enforceBoundsGroup(const std::string &group)
{
    enforceBoundsGroup(owner_->getGroup(group));
}

void planning_models::KinematicState::enforceBoundsGroup(const KinematicModel::JointGroup *group)
{
    const std::vector<double> &bounds = owner_->getStateBounds();
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	const unsigned int j  = group->state_index[i];
	const unsigned int j2 = j << 1;
	if (params_[j] > bounds[j2 + 1])
	    params_[j] = bounds[j2 + 1];
	else
	    if (params_[j] < bounds[j2])
		params_[j] = bounds[j2];
    }
}

void planning_models::KinematicState::enforceBounds(void)
{  
    const std::vector<double> &bounds = owner_->getStateBounds();
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
    {
	const unsigned int i2 = i << 1;
	if (params_[i] > bounds[i2 + 1])
	    params_[i] = bounds[i2 + 1];
	else
	    if (params_[i] < bounds[i2])
		params_[i] = bounds[i2];
    }
}

bool planning_models::KinematicState::checkBoundsGroup(const std::string &group) const
{
    return checkBoundsGroup(owner_->getGroup(group));
}

bool planning_models::KinematicState::checkBoundsGroup(const KinematicModel::JointGroup *group) const
{
    const std::vector<double> &bounds = owner_->getStateBounds();
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	const unsigned int j  = group->state_index[i];
	const unsigned int j2 = j << 1;
	if (params_[j] > bounds[j2 + 1])
	    return false;
	if (params_[j] < bounds[j2])
	    return false;
    }
    return true;
}

bool planning_models::KinematicState::checkBounds(void) const
{
    const std::vector<double> &bounds = owner_->getStateBounds();
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
    {
	const unsigned int i2 = i << 1;
	if (params_[i] > bounds[i2 + 1])
	    return false;
	if (params_[i] < bounds[i2])
	    return false;
    }
    return true;
}

bool planning_models::KinematicState::checkBoundsJoint(const std::string &name) const
{
    const std::vector<double> &bounds = owner_->getStateBounds();
    const KinematicModel::Joint *joint = owner_->getJoint(name);
    const unsigned int p = joint->state_index + joint->used_params;
    for (unsigned int i = joint->state_index; i < p ; ++i)
    {
	const unsigned int i2 = i << 1;
	if (params_[i] > bounds[i2 + 1])
	    return false;
	if (params_[i] < bounds[i2])
	    return false;
    }
    return true;
}

bool planning_models::KinematicState::checkBoundsJoints(const std::vector<std::string> &names) const
{    
    for (unsigned int i = 0 ; i < names.size() ; ++i)
	if (!checkBoundsJoint(names[i]))
	    return false;
    return true;
}

void planning_models::KinematicState::reset(void)
{
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
	updated_[i] = false;
}

void planning_models::KinematicState::resetGroup(const std::string &group)
{
    resetGroup(owner_->getGroup(group));
}

void planning_models::KinematicState::resetGroup(const KinematicModel::JointGroup *group)
{
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
	updated_[group->state_index[i]] = false;
}

bool planning_models::KinematicState::seenAll(void) const
{
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
        if (!updated_[i])
	    return false;
    return true;
}

bool planning_models::KinematicState::seenAllGroup(const std::string &group) const
{
    return seenAllGroup(owner_->getGroup(group));
}

bool planning_models::KinematicState::seenAllGroup(const KinematicModel::JointGroup *group) const
{    
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
	if (!updated_[group->state_index[i]])
	    return false;
    return true;
}

bool planning_models::KinematicState::seenJoint(const std::string &name) const
{
    const KinematicModel::Joint *joint = owner_->getJoint(name);
    for (unsigned int i = 0 ; i < joint->used_params ; ++i)
	if (!updated_[joint->state_index + i])
	    return false;
    return true;
}

void planning_models::KinematicState::missing(std::ostream &out)
{
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
	if (!updated_[i])
	    out << i << " ";
}

const double* planning_models::KinematicState::getParamsJoint(const std::string &name) const
{
    const KinematicModel::Joint *joint = owner_->getJoint(name);
    return params_ + joint->state_index;
}

bool planning_models::KinematicState::setParamsJoints(const double *params, const std::vector<std::string> &names)
{
    bool change = false;
    int u = 0;
    for (unsigned int i = 0 ; i < names.size() ; ++i)
    {
	const KinematicModel::Joint *joint = owner_->getJoint(names[i]);
	bool ch = setParamsJoint(params + u, names[i]);
	u += joint->used_params;
	change = change || ch;
    }
    
    return change;
}

bool planning_models::KinematicState::setParamsJoints(const std::vector<double> &params, const std::vector<std::string> &names)
{
    bool change = false;
    int u = 0;
    for (unsigned int i = 0 ; i < names.size() ; ++i)
    {
	const KinematicModel::Joint *joint = owner_->getJoint(names[i]);
	int u2 = u + joint->used_params;
	ROS_ASSERT(u2 <= (int)params.size());
	bool ch = setParamsJoint(&params[0] + u, names[i]);
	u = u2;
	change = change || ch;
    }
    
    return change;
}

bool planning_models::KinematicState::setParamsJoint(const std::vector<double> &params, const std::string &name)
{
    ROS_ASSERT(params.size() == owner_->getJoint(name)->used_params);
    return setParamsJoint(&params[0], name);
}

bool planning_models::KinematicState::setParamsJoint(const double *params, const std::string &name)
{
    bool result = false;
    const KinematicModel::Joint *joint = owner_->getJoint(name);
    
    for (unsigned int i = 0 ; i < joint->used_params ; ++i)
    {
	unsigned int pos_i = joint->state_index + i;
	if (params_[pos_i] != params[i] || !updated_[pos_i])
	{
	    params_[pos_i] = params[i];
	    updated_[pos_i] = true;
	    result = true;
	}
    }
    
    return result;
}

bool planning_models::KinematicState::setParams(const std::vector<double> &params)
{
    ROS_ASSERT(params.size() == owner_->getDimension());
    return setParams(&params[0]);
}

bool planning_models::KinematicState::setParams(const double *params)
{  
    const unsigned int dim = owner_->getDimension();
    bool result = false;
    for (unsigned int i = 0 ; i < dim ; ++i)
	if (params_[i] != params[i] || !updated_[i])
	{
	    params_[i] = params[i];
	    updated_[i] = true;
	    result = true;
	}
    return result;
}

bool planning_models::KinematicState::setParamsGroup(const std::vector<double> &params, const std::string &group)
{
    const KinematicModel::JointGroup *jg = owner_->getGroup(group);
    ROS_ASSERT(params.size() == jg->dimension);
    return setParamsGroup(&params[0], jg);
}

bool planning_models::KinematicState::setParamsGroup(const std::vector<double> &params, const KinematicModel::JointGroup *group)
{
    ROS_ASSERT(params.size() == group->dimension);
    return setParamsGroup(&params[0], group);
}

bool planning_models::KinematicState::setParamsGroup(const double *params, const std::string &group)
{
    return setParamsGroup(params, owner_->getGroup(group));
}

bool planning_models::KinematicState::setParamsGroup(const double *params, const KinematicModel::JointGroup *group)
{
    bool result = false;
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	unsigned int j = group->state_index[i];
	if (params_[j] != params[i] || !updated_[j])
	{
	    params_[j] = params[i];
	    updated_[j] = true;
	    result = true;
	}
    }
    return result;
}

void planning_models::KinematicState::setAllInGroup(const double value, const std::string &group)
{
    setAllInGroup(value, owner_->getGroup(group));
}

void planning_models::KinematicState::setAllInGroup(const double value, const KinematicModel::JointGroup *group)
{
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
    {
	unsigned int j = group->state_index[i];
	params_[j] = value;
	updated_[j] = true;
    }	
}

void planning_models::KinematicState::setAll(const double value)
{
    const unsigned int dim = owner_->getDimension();
    for (unsigned int i = 0 ; i < dim ; ++i)
    {
	params_[i] = value;
	updated_[i] = true;
    }
}

const double* planning_models::KinematicState::getParams(void) const
{
    return params_;
}

void planning_models::KinematicState::copyParamsJoint(double *params, const std::string &name) const
{
    const KinematicModel::Joint *joint = owner_->getJoint(name);
    std::copy(params_ + joint->state_index, params_ + joint->state_index + joint->used_params, params);
}

void planning_models::KinematicState::copyParamsJoint(std::vector<double> &params, const std::string &name) const
{
    const KinematicModel::Joint *joint = owner_->getJoint(name);
    params.resize(joint->used_params);
    std::copy(params_ + joint->state_index, params_ + joint->state_index + joint->used_params, params.begin());
}

void planning_models::KinematicState::copyParams(double *params) const
{
    std::copy(params_, params_ + owner_->getDimension(), params);
}

void planning_models::KinematicState::copyParams(std::vector<double> &params) const
{
    params.resize(owner_->getDimension());
    std::copy(params_, params_ + owner_->getDimension(), params.begin());
}

void planning_models::KinematicState::copyParamsJoints(double *params, const std::vector<std::string> &names) const
{
    double *dest = params;
    for (unsigned int j = 0 ; j < names.size() ; ++j)
    {
	const KinematicModel::Joint *joint = owner_->getJoint(names[j]);
	std::copy(params_ + joint->state_index, params_ + joint->state_index + joint->used_params, dest);
	dest += joint->used_params;
    }
}

void planning_models::KinematicState::copyParamsJoints(std::vector<double> &params, const std::vector<std::string> &names) const
{
    params.clear();
    for (unsigned int j = 0 ; j < names.size() ; ++j)
    {
	std::vector<double> p;
	copyParamsJoint(p, names[j]);
	params.insert(params.end(), p.begin(), p.end());
    }
}

void planning_models::KinematicState::copyParamsGroup(double *params, const std::string &group) const
{
    copyParamsGroup(params, owner_->getGroup(group));
}

void planning_models::KinematicState::copyParamsGroup(std::vector<double> &params, const std::string &group) const
{
    const KinematicModel::JointGroup *g = owner_->getGroup(group);
    params.resize(g->dimension);
    copyParamsGroup(&params[0], g);
}

void planning_models::KinematicState::copyParamsGroup(std::vector<double> &params, const KinematicModel::JointGroup *group) const
{ 
    params.resize(group->dimension);
    copyParamsGroup(&params[0], group);
}

void planning_models::KinematicState::copyParamsGroup(double *params, const KinematicModel::JointGroup *group) const
{   
    for (unsigned int i = 0 ; i < group->dimension ; ++i)
	params[i] = params_[group->state_index[i]];
}

void planning_models::KinematicState::print(std::ostream &out) const
{
    out << std::endl;
    std::vector<const KinematicModel::Joint*> joints;
    owner_->getJoints(joints);
    
    for (unsigned int i = 0 ; i < joints.size() ; ++i)
    {
	out << joints[i]->name;
	if (!seenJoint(joints[i]->name))
	    out << "[ *** UNSEEN *** ]";
	out << ": ";
	for (unsigned int j = 0 ; j < joints[i]->used_params ; ++j)
	    out << params_[joints[i]->state_index + j] << std::endl;
    }
    out << std::endl;
    for (unsigned int i = 0; i < owner_->getDimension() ; ++i)
	out << params_[i] << " ";
    out << std::endl;
}

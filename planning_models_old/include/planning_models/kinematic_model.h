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

#ifndef PLANNING_MODELS_KINEMATIC_MODEL_
#define PLANNING_MODELS_KINEMATIC_MODEL_

#include <geometric_shapes/shapes.h>

#include <urdf/model.h>
#include <LinearMath/btTransform.h>
#include <boost/thread/mutex.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <map>


/** \brief Main namespace */
namespace planning_models
{
 
    /** \brief Definition of a kinematic model. This class is not thread
	safe, however multiple instances can be created */
    class KinematicModel
    {
    public:	
	/** \brief Forward definition of a joint */
	class Joint;
	
	/** \brief Forward definition of a link */
	class Link;

	/** \brief Forward definition of an attached body */
	class AttachedBody;
	
	/** \brief Forward definition of a group of joints */
	class JointGroup;
	
	
	/** \brief A joint from the robot. Contains the transform applied by the joint type */
	class Joint
	{
	public:
	    Joint(KinematicModel *model);
	    virtual ~Joint(void);

	    /** \brief Name of the joint */
	    std::string       name;
	    
	    /** \brief The model that owns this joint */
	    KinematicModel   *owner;
	    
	    /** \brief The range of indices in the parameter vector that
		needed to access information about the position of this
		joint */
	    unsigned int      usedParams;

	    /** \brief The index where this joint starts readin params in the global state vector */
	    unsigned int      stateIndex;
	    
	    /** \brief the links that this joint connects */	    
	    Link             *before;
	    Link             *after;

	    /** \brief the local transform (computed by forward kinematics) */
	    btTransform       varTrans;

	    /** \brief Update the value of varTrans using the information from params */
	    virtual void updateVariableTransform(const double *params) = 0;

	};

	/** \brief A fixed joint */
	class FixedJoint : public Joint
	{
	public:
	    
	    FixedJoint(KinematicModel *owner) : Joint(owner)
	    {
	    }
	    
	    /** \brief Update the value of varTrans using the information from params */
	    virtual void updateVariableTransform(const double *params);
	};

	/** \brief A planar joint */
	class PlanarJoint : public Joint
	{
	public:
	    
	    PlanarJoint(KinematicModel *owner) : Joint(owner)
	    {
	        usedParams = 3; // (x, y, theta)
	    }
	    
	    /** \brief Update the value of varTrans using the information from params */
	    virtual void updateVariableTransform(const double *params);
	};

	/** \brief A floating joint */
	class FloatingJoint : public Joint
	{
	public:
	    
	    FloatingJoint(KinematicModel *owner) : Joint(owner)
	    {
	        usedParams = 7; // vector: (x, y, z)  quaternion: (x, y, z, w)
	    }

	    /** \brief Update the value of varTrans using the information from params */
	    virtual void updateVariableTransform(const double *params);
	};

	/** \brief A prismatic joint */
	class PrismaticJoint : public Joint
	{
	public:
	    
	    PrismaticJoint(KinematicModel *owner) : Joint(owner), axis(0.0, 0.0, 0.0), lowLimit(0.0), hiLimit(0.0)
	    {
		usedParams = 1;
	    }
	    
	    btVector3 axis;
	    double    lowLimit;
	    double    hiLimit;
	    
	    /** \brief Update the value of varTrans using the information from params */
	    virtual void updateVariableTransform(const double *params);
	    
	};
	
	/** \brief A revolute joint */
	class RevoluteJoint : public Joint
	{
	public:
	    
	    RevoluteJoint(KinematicModel *owner) : Joint(owner), axis(0.0, 0.0, 0.0),
						   lowLimit(0.0), hiLimit(0.0), continuous(false)
	    {
		usedParams = 1;
	    }
	    	    
	    btVector3 axis;
	    double    lowLimit;
	    double    hiLimit;
	    bool      continuous;

	    /** \brief Update the value of varTrans using the information from params */
	    virtual void updateVariableTransform(const double *params);

	};
	
	/** \brief A link from the robot. Contains the constant transform applied to the link and its geometry */
	class Link
	{
	public:

	    Link(KinematicModel *model);	    
	    ~Link(void);

	    /** \brief Name of the link */
	    std::string                name;

	    /** \brief The model that owns this link */
	    KinematicModel            *owner;

	    /** \brief Joint that connects this link to the parent link */
	    Joint                     *before;
	    
	    /** \brief List of descending joints (each connects to a child link) */
	    std::vector<Joint*>        after;
	    
	    /** \brief The constant transform applied to the link (local) */
	    btTransform                constTrans;
	    
	    /** \brief The constant transform applied to the collision geometry of the link (local) */
	    btTransform                constGeomTrans;
	    
	    /** \brief The geometry of the link */
	    shapes::Shape             *shape;
	    
	    /** \brief Attached bodies */
	    std::vector<AttachedBody*> attachedBodies;	    
	    
	    /** \brief The global transform this link forwards (computed by forward kinematics) */
	    btTransform                globalTransFwd;

	    /** \brief The global transform for this link (computed by forward kinematics) */
	    btTransform                globalTrans;

	    /** \brief Recompute globalTrans and globalTransFwd */
	    void computeTransform(void);

      void setTransform(const btTransform &bt);	    

      void updateTransformsRecursive(void);
    
    void getAllChildLinksRecursive(std::vector<std::string> &link_names);

	};


	
	/** \brief Class defining bodies that can be attached to robot
	    links. This is useful when handling objects picked up by
	    the robot. */
	class AttachedBody
	{
	public:
	    
          AttachedBody(Link *link, std::string id ="");
          ~AttachedBody(void);
	  
          /** \brief The link that owns this attached body */
          Link                     *owner;
          
          /** \brief The geometry of the attached body */
          std::vector<shapes::Shape*>            shapes;
          
          /** \brief The constant transform applied to the link (needs to be specified by user) */
          std::vector<btTransform>               attachTrans;
          
          /** \brief The global transform for this link (computed by forward kinematics) */
          std::vector<btTransform>               globalTrans;
	    
          /** \brief The set of links this body is allowed to touch */
          std::vector<std::string>  touchLinks;
          
          /** string id for reference */
          std::string id;

          /** \brief Recompute globalTrans */
          void computeTransform(void);
	};


	class JointGroup
	{
	public:
	    
	    JointGroup(KinematicModel *model, const std::string& groupName, const std::vector<Joint*> &groupJoints);
	    ~JointGroup(void);
	    
	    /** \brief The kinematic model that owns the group */
	    KinematicModel                     *owner;	    

	    /** \brief Name of group */
	    std::string                         name;

	    /** \brief Names of joints in the order they appear in the group state */
	    std::vector<std::string>            jointNames;

	    /** \brief Joint instances in the order they appear in the group state */
	    std::vector<Joint*>                 joints;

	    /** \brief Index where each joint starts within the group state */
	    std::vector<unsigned int>           jointIndex;

	    /** \brief Easy way of finding the position of a joint in the list of joints contained in the group */
	    std::map<std::string, unsigned int> jointMap;

	    /** \brief The dimension of the group */
	    unsigned int                        dimension;

	    /** \brief The bounds for the state corresponding to the group */
	    std::vector<double>                 stateBounds;
	    
	    /** \brief An array containing the index in the global state for each dimension of the state of the group */
	    std::vector<unsigned int>           stateIndex;
	    
	    /** \brief The list of joints that are roots in this group */
	    std::vector<Joint*>                 jointRoots;

	    /** \brief The list of links that are updated when computeTransforms() is called, in the order they are updated */
	    std::vector<Link*>                  updatedLinks;
	    
	    /** \brief Perform forward kinematics starting at the roots
		within a group. Links that are not in the group are also
		updated, but transforms for joints that are not in the
		group are not recomputed.  */
	    void computeTransforms(const double *params);	

	    /** \brief Check if a joint is part of this group */
	    bool hasJoint(const std::string &joint) const;
	    
	    /** \brief Get the position of a joint inside this group */
	    int  getJointPosition(const std::string &joint) const;
	    
	    /** \brief Bring the group to a default state. All joints are
		at 0. If 0 is not within the bounds of the joint, the
		middle of the bounds is used. */
	    void defaultState(void);
	    
	    /** \brief Check if this group contains the joints from another group */
	    bool containsGroup(const JointGroup *group) const;

	    /** \brief Construct a group that consists of the union of joints of this group and the argument group */
	    JointGroup* addGroup(const JointGroup *group) const;

	    /** \brief Construct a group that consists of the joints of this group that are not joints in the argument group */
	    JointGroup* removeGroup(const JointGroup *group) const;

	};
	
	/** \brief Construct a kinematic model from another one */
	KinematicModel(const KinematicModel &source);

	/** \brief Construct a kinematic model from a parsed description and a list of planning groups */
	KinematicModel(const urdf::Model &model, const std::map< std::string, std::vector<std::string> > &groups);
	
	/** \brief Destructor. Clear all memory. */
	~KinematicModel(void);

	/** \brief Bring the robot to a default state */
	void defaultState(void);
	
	/** \brief General the model name **/
	const std::string& getName(void) const;
	
	/** \brief Get a group by its name */
	const JointGroup* getGroup(const std::string &name) const;

	/** \brief Get a group by its name */
	JointGroup* getGroup(const std::string &name);
	
	/** \brief Check if a group exists */
	bool hasGroup(const std::string &name) const;
	
	/** \brief Get the array of planning groups */
	void getGroups(std::vector<const JointGroup*> &groups) const;
	
	/** \brief Get the group names, in no particular order */
	void getGroupNames(std::vector<std::string> &groups) const;

	/** \brief Get a link by its name */
	const Link* getLink(const std::string &link) const;

	/** \brief Get a link by its name */
	Link* getLink(const std::string &link);

	/** \brief Check if a link exists */
	bool hasLink(const std::string &name) const;

	/** \brief Get the array of links, in no particular order */
	void getLinks(std::vector<const Link*> &links) const;

	/** \brief Get the link names, in no particular order */
	void getLinkNames(std::vector<std::string> &links) const;

	/** \brief Get a joint by its name */
	const Joint* getJoint(const std::string &joint) const;

	/** \brief Get a joint by its name */
	Joint* getJoint(const std::string &joint);

	/** \brief Check if a joint exists */
	bool hasJoint(const std::string &name) const;
	
	/** \brief Get the array of joints, in the order they appear
	    in the robot state. */
	void getJoints(std::vector<const Joint*> &joints) const;
	
	/** \brief Get the array of joint names, in the order they
	    appear in the robot state. */
	void getJointNames(std::vector<std::string> &joints) const;
	
	/** \brief Perform forward kinematics for the entire robot */
	void computeTransforms(const double *params);
	
	/** \brief Get the global transform applied to the entire tree of links */
	const btTransform& getRootTransform(void) const;
	
	/** \brief Set the global transform applied to the entire tree of links */
	void setRootTransform(const btTransform &transform);

	/** \brief Get the root joint */
	const Joint* getRoot(void) const;

	/** \brief Get the root joint */
	Joint* getRoot(void);
	
	/** \brief Get the dimension of the entire model */
	unsigned int getDimension(void) const;
	
	/** \brief Get the state bounds constructed for this
	    model. Component i of the state space has bounds (min,
	    max) at index positions (2*i, 2*i+1)*/
	const std::vector<double> &getStateBounds(void) const;
	
	/** \brief Return a list of names of joints that are planar */
	const std::vector<std::string> &getPlanarJoints(void) const;

	/** \brief Return a list of names of joints that are floating */
	const std::vector<std::string> &getFloatingJoints(void) const;

	/** \brief Return a list of names of joints that are floating */
	const std::vector<std::string> &getFixedJoints(void) const;
	
	/** \brief Provide interface to a lock. Use carefully! */
	void lock(void);
	
	/** \brief Provide interface to a lock. Use carefully! */
	void unlock(void);

	/** \brief Print information about the constructed model */
	void printModelInfo(std::ostream &out = std::cout) const;

	/** \brief Print the pose of every link */
	void printTransforms(std::ostream &out = std::cout) const;
	
    private:
	
	/** \brief The name of the model */
	std::string                                       modelName_;	

	/** \brief A map from group names to their instances */
	std::map<std::string, JointGroup*>                groupMap_;	

	/** \brief A map from link names to their instances */
	std::map<std::string, Link*>                      linkMap_;

	/** \brief A map from joint names to their instances */
	std::map<std::string, Joint*>                     jointMap_;

	/** \brief The list of joints in the model, in the order they appear in the state vector */
	std::vector<Joint*>                               jointList_;
	
	/** \brief The index at which a joint starts reading values in the state vector */
	std::vector<unsigned int>                         jointIndex_;
	
	/** \brief The list of links that are updated when computeTransforms() is called, in the order they are updated */
	std::vector<Link*>                                updatedLinks_;	
	
	/** \brief The root joint */
	Joint                                            *root_;
	
	/** \brief List of floating joints, maintained for convenience */
	std::vector<std::string>                          floatingJoints_;

	/** \brief List of planar joints, maintained for convenience */
	std::vector<std::string>                          planarJoints_;

      std::vector<std::string>                          fixedJoints_;
	
	/** \brief The bounds in the form (min, max) for every component of the state */
	std::vector<double>                               stateBounds_;
	
	/** \brief The dimension of the model */
	unsigned int                                      dimension_;
	
	/** \brief Additional transform to be applied to the tree of links */
	btTransform                                       rootTransform_;
	
	boost::mutex                                      lock_;


	void buildConvenientDatastructures(void);	
	void buildGroups(const std::map< std::string, std::vector<std::string> > &groups);
	Joint* buildRecursive(Link *parent, const urdf::Link *link);
	Joint* constructJoint(const urdf::Joint *urdfJoint, std::vector<double> &bounds);
	Link* constructLink(const urdf::Link *urdfLink);
	shapes::Shape* constructShape(const urdf::Geometry *geom);

	Joint* copyJoint(const Joint *joint);
	Link* copyLink(const Link *link);
	Joint* copyRecursive(Link *parent, const Link *link);

	void printTransform(const std::string &st, const btTransform &t, std::ostream &out = std::cout) const;
	
    };

}

#endif

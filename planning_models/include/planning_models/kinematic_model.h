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
#include <boost/thread/shared_mutex.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <boost/bimap.hpp>

/** \brief Main namespace */
namespace planning_models
{
 
/** \brief Definition of a kinematic model. This class is not thread
    safe, however multiple instances can be created */
class KinematicModel
{
public:	
  /** \brief Forward definition of a joint */
  class JointModel;
	
  /** \brief Forward definition of a link */
  class LinkModel;

  /** \brief Forward definition of an attached body */
  class AttachedBodyModel;
	
  struct MultiDofConfig
  {
    MultiDofConfig(std::string n) : name(n) {
    }
    
    ~MultiDofConfig() {
    }
    
    /** \brief The name of the joint */
    std::string name;
    
    /** \brief The type of multi-dof joint */
    std::string type;
    
    /** \brief The parent frame in which the joint state will be supplied */
    std::string parent_frame_id;

    /** \brief The child frame into which to convert the supplied transform */
    std::string child_frame_id;

    /** \brief The mapping between internally defined DOF names and externally defined DOF names */
    std::map<std::string, std::string> name_equivalents;
  };
  
  /** \brief A joint from the robot. Contains the transform applied by the joint type */
  class JointModel
  {
    friend class KinematicModel;
  public:
    JointModel(const std::string& name);

    JointModel(const JointModel* joint);

    void initialize(const std::vector<std::string>& local_names,
                    const MultiDofConfig* multi_dof_config = NULL);

    virtual ~JointModel(void);

    typedef boost::bimap< std::string, std::string > js_type;

    const std::string& getName() const 
    {
      return name_;
    }

    const LinkModel* getParentLinkModel() const 
    {
      return parent_link_model_;
    }
    
    const LinkModel* getChildLinkModel() const
    {
      return child_link_model_;
    }
    
    const std::string& getParentFrameId() const
    {
      return parent_frame_id_;
    }

    const std::string& getChildFrameId() const
    {
      return child_frame_id_;
    }

    const js_type& getJointStateEquivalents() const
    {
      return joint_state_equivalents_;
    }

    const std::map<unsigned int, std::string>& getComputatationOrderMapIndex() const
    {
      return computation_order_map_index_;
    }
    /** \brief Gets the joint state equivalent for given name */  
    std::string getEquiv(const std::string name) const;

    /** \brief Gets the lower and upper bounds for a variable */
    std::pair<double, double> getVariableBounds(std::string variable) const;
   
    /** \brief Sets the lower and upper bounds for a variable */
    void setVariableBounds(std::string variable, double low, double high);

    const std::map<std::string, std::pair<double, double> >& getAllVariableBounds() const {
      return joint_state_bounds_;
    }

    bool hasVariable(const std::string var) const
    {
      return(joint_state_equivalents_.right.find(var) != joint_state_equivalents_.right.end());
    }
   
    virtual btTransform computeTransform(const std::vector<double>& joint_values) const = 0;
    
    virtual std::vector<double> computeJointStateValues(const btTransform& transform) const = 0;

  private:

    /** \brief Name of the joint */
    std::string name_;
    
    /** \brief The link before this joint */
    LinkModel *parent_link_model_;
    
    /** \brief The link after this joint */
    LinkModel *child_link_model_;

    //local names on the left, config names on the right
    js_type joint_state_equivalents_;

    //map for high and low bounds
    std::map<std::string, std::pair<double, double> > joint_state_bounds_;
    
    //correspondance between index into computation array and external name
    std::map<unsigned int, std::string> computation_order_map_index_;
   
    /** The parent frame id for this joint.  May be empty unless specified as multi-dof*/
    std::string parent_frame_id_;
    
    /** The child frame id for this joint.  May be empty unless specified as multi-dof*/
    std::string child_frame_id_;
  };

  /** \brief A fixed joint */
  class FixedJointModel : public JointModel
  {
  public:
	    
    FixedJointModel(const std::string name, const MultiDofConfig* multi_dof_config) :
      JointModel(name)
    {
    }
	    
    FixedJointModel(const FixedJointModel* joint): JointModel(joint)
    {
    }

    virtual btTransform computeTransform(const std::vector<double>& joint_values) const {
      btTransform ident;
      ident.setIdentity();
      return ident;
    }
    
    virtual std::vector<double> computeJointStateValues(const btTransform& transform) const {
      std::vector<double> ret;
      return ret;
    }

  };

  /** \brief A planar joint */
  class PlanarJointModel : public JointModel
  {
  public:
	    
    PlanarJointModel(const std::string& name, const MultiDofConfig* multi_dof_config);
	    
    PlanarJointModel(const PlanarJointModel* joint): JointModel(joint)
    {
    }

    virtual btTransform computeTransform(const std::vector<double>& joint_values) const;
    
    virtual std::vector<double> computeJointStateValues(const btTransform& transform) const;

  };

  /** \brief A floating joint */
  class FloatingJointModel : public JointModel
  {
  public:
	    
    FloatingJointModel(const std::string& name, const MultiDofConfig* multi_dof_config);

    FloatingJointModel(const FloatingJointModel* joint) : JointModel(joint)
    {
    }

    virtual btTransform computeTransform(const std::vector<double>& joint_values) const;
    
    virtual std::vector<double> computeJointStateValues(const btTransform& transform) const;

  };

  /** \brief A prismatic joint */
  class PrismaticJointModel : public JointModel
  {
  public:
	    
    PrismaticJointModel(const std::string& name, const MultiDofConfig* multi_dof_config);
    
    PrismaticJointModel(const PrismaticJointModel* joint) : JointModel(joint){
      axis_ = joint->axis_;
    }
	    
    btVector3 axis_;
    
    virtual btTransform computeTransform(const std::vector<double>& joint_values) const;
    
    virtual std::vector<double> computeJointStateValues(const btTransform& transform) const;
    
  };
	
  /** \brief A revolute joint */
  class RevoluteJointModel : public JointModel
  {
  public:
	    
    RevoluteJointModel(const std::string& name, const MultiDofConfig* multi_dof_config);

    RevoluteJointModel(const RevoluteJointModel* joint) : JointModel(joint){
      axis_ = joint->axis_;
      continuous_ = joint->continuous_;
    }
	    	    
    btVector3 axis_;
    bool      continuous_;

    virtual btTransform computeTransform(const std::vector<double>& joint_values) const;
    
    virtual std::vector<double> computeJointStateValues(const btTransform& transform) const;
  };
	
  /** \brief A link from the robot. Contains the constant transform applied to the link and its geometry */
  class LinkModel
  {
    friend class KinematicModel;
  public:
    
    LinkModel(const KinematicModel* kinematic_model);
    
    LinkModel(const LinkModel* link_model);

    ~LinkModel(void);

    const std::string& getName() const {
      return name_;
    }
    
    const JointModel* getParentJointModel() const {
      return parent_joint_model_;
    }

    const std::vector<JointModel*>& getChildJointModels() const {
      return child_joint_models_;
    }

    const btTransform& getJointOriginTransform() const {
      return joint_origin_transform_;
    }

    const btTransform& getCollisionOriginTransform() const {
      return collision_origin_transform_;
    }

    const shapes::Shape* getLinkShape() const {
      return shape_;
    }
    
    const std::vector<AttachedBodyModel*>& getAttachedBodyModels() const {
      return attached_body_models_;
    }
    
    /** \brief Removes all attached body models from this link, requiring an exclusive lock */
    void clearAttachedBodyModels();
    
    /** \brief Removes all attached body models from this link, and replaces them with the supplied vector,
        requiring an exclusive lock
     */
    void replaceAttachedBodyModels(std::vector<AttachedBodyModel*>& attached_body_vector);

  private:
        
    /** \brief Name of the link */
    std::string name_;
    
    /** \brief KinematicState point for accessing locks */
    const KinematicModel* kinematic_model_;

    /** \brief JointModel that connects this link to the parent link */
    JointModel *parent_joint_model_;
    
    /** \brief List of descending joints (each connects to a child link) */
    std::vector<JointModel*> child_joint_models_;
    
    /** \brief The constant transform applied to the link (local) */
    btTransform joint_origin_transform_;
    
    /** \brief The constant transform applied to the collision geometry of the link (local) */
    btTransform  collision_origin_transform_;
    
    /** \brief The geometry of the link */
    shapes::Shape *shape_;
    
    /** \brief Attached bodies */
    std::vector<AttachedBodyModel*> attached_body_models_;	        
  };
  
  /** \brief Class defining bodies that can be attached to robot
      links. This is useful when handling objects picked up by
      the robot. */
  class AttachedBodyModel
  {
  public:
    
    AttachedBodyModel(const LinkModel *link, 
                      const std::string& id,
                      const std::vector<btTransform>& attach_trans,
                      const std::vector<std::string>& touch_links,
                      std::vector<shapes::Shape*>& shapes);

    ~AttachedBodyModel(void);
    
    const std::string& getName() const 
    {
      return id_;
    }
    
    const LinkModel* getAttachedLinkModel() const 
    {
      return attached_link_model_;
    }

    const std::vector<shapes::Shape*>& getShapes() const 
    {
      return shapes_;
    }

    const std::vector<btTransform>& getAttachedBodyFixedTransforms() const
    {
      return attach_trans_;
    }

    const std::vector<std::string>& getTouchLinks() const
    {
      return touch_links_;
    }
    
  private:

    /** \brief The link that owns this attached body */
    const LinkModel *attached_link_model_;
    
    /** \brief The geometries of the attached body */
    std::vector<shapes::Shape*> shapes_;
    
    /** \brief The constant transforms applied to the link (need to be specified by user) */
    std::vector<btTransform> attach_trans_;
    
    /** \brief The set of links this body is allowed to touch */
    std::vector<std::string> touch_links_;
    
    /** string id for reference */
    std::string id_;
  };

  class JointModelGroup
  {
    friend class KinematicModel;
  public:
	    
    JointModelGroup(
                    const std::string& groupName, 
                    const std::vector<const JointModel*> &groupJoints);
    ~JointModelGroup(void);

    const std::string& getName() const
    {
      return name_;
    }
	    
    /** \brief Check if a joint is part of this group */
    bool hasJointModel(const std::string &joint) const;

    /** \brief Get a joint by its name */
    const JointModel* getJointModel(const std::string &joint);

    const std::vector<const JointModel*>& getJointModels() const
    {
      return joint_model_vector_;
    }
    
    const std::vector<std::string>& getJointModelNames() const
    {
      return joint_model_name_vector_;
    }

    const std::vector<const LinkModel*>& getUpdatedLinkModels() const
    {
      return updated_link_model_vector_;
    }

    const std::vector<const JointModel*>& getJointRoots() const
    {
      return joint_roots_;
    }
    
  private:

    /** \brief Name of group */
    std::string name_;

    /** \brief Names of joints in the order they appear in the group state */
    std::vector<std::string> joint_model_name_vector_;

    /** \brief Joint instances in the order they appear in the group state */
    std::vector<const JointModel*> joint_model_vector_;

    /** \brief A map from joint names to their instances */
    std::map<std::string, const JointModel*> joint_model_map_;

     /** \brief The list of joint models that are roots in this group */
    std::vector<const JointModel*> joint_roots_;

    /** \brief The list of child link_models in the order they should be updated */
    std::vector<const LinkModel*> updated_link_model_vector_;
	    
  };

  /** \brief Construct a kinematic model from another one */
  KinematicModel(const KinematicModel &source);

  /** \brief Construct a kinematic model from a parsed description and a list of planning groups */
  KinematicModel(const urdf::Model &model, 
                 const std::map< std::string, std::vector<std::string> > &groups, 
                 const std::vector<MultiDofConfig>& multi_dof_configs);
	
  /** \brief Destructor. Clear all memory. */
  ~KinematicModel(void);

  void copyFrom(const KinematicModel &source);

  void defaultState(void);
	
  /** \brief General the model name **/
  const std::string& getName(void) const;

  /** \brief Get a link by its name */
  const LinkModel* getLinkModel(const std::string &link) const;

  /** \brief Check if a link exists */
  bool hasLinkModel(const std::string &name) const;

  /** \brief Get the link names in the order they should be updated */
  void getLinkModelNames(std::vector<std::string> &links) const;

  /** \brief Get the set of links that follow a parent link in the kinematic chain */
  void getChildLinkModels(const LinkModel* parent, std::vector<const LinkModel*> &links) const;
  
  /** \brief Get a joint by its name */
  const JointModel* getJointModel(const std::string &joint) const;

  /** \brief Check if a joint exists */
  bool hasJointModel(const std::string &name) const;
	
  /** \brief Get the array of joints, in the order they appear
      in the robot state. */
  const std::vector<JointModel*>& getJointModels() const
  {
    return joint_model_vector_;
  }
 /** \brief Get the array of joints, in the order they should be
     updated*/
  const std::vector<LinkModel*>& getLinkModels() const
  {
    return link_model_vector_;
  }
	
  /** \brief Get the array of joint names, in the order they
      appear in the robot state. */
  void getJointModelNames(std::vector<std::string> &joints) const;

  /** \brief Get the root joint */
  const JointModel* getRoot(void) const;
	
  /** \brief Provide interface to get an exclusive lock to change the model. Use carefully! */
  void exclusiveLock(void) const;
	
  /** \brief Provide interface to release an exclusive lock. Use carefully! */
  void exclusiveUnlock(void) const;

  /** \brief Provide interface to get a shared lock for reading model data */
  void sharedLock(void) const;
  
  /** \brief Provide interface to release a shared lock */
  void sharedUnlock(void) const;

  /** \brief Print information about the constructed model */
  void printModelInfo(std::ostream &out = std::cout) const;

  bool hasModelGroup(const std::string& group) const;

  const JointModelGroup* getModelGroup(const std::string& name) const
  {
    if(!hasModelGroup(name)) return NULL;
    return joint_model_group_map_.find(name)->second;
  }

  const std::map<std::string, JointModelGroup*>& getJointModelGroupMap() const
  {
    return joint_model_group_map_;
  }

  void getModelGroupNames(std::vector<std::string>& getModelGroupNames) const;
	
private:
	
  /** \brief Shared lock for changing models */ 
  mutable boost::shared_mutex lock_;

  /** \brief The name of the model */
  std::string model_name_;	

  /** \brief A map from link names to their instances */
  std::map<std::string, LinkModel*> link_model_map_;

  /** \brief A map from joint names to their instances */
  std::map<std::string, JointModel*> joint_model_map_;

  /** \brief The vector of joints in the model, in the order they appear in the state vector */
  std::vector<JointModel*> joint_model_vector_;
	
  /** \brief The vector of links that are updated when computeTransforms() is called, in the order they are updated */
  std::vector<LinkModel*> link_model_vector_;	
	
  /** \brief The root joint */
  JointModel *root_;
	
  std::map<std::string, JointModelGroup*> joint_model_group_map_;

  void buildGroups(const std::map< std::string, std::vector<std::string> > &groups);
  JointModel* buildRecursive(LinkModel *parent, const urdf::Link *link, 
                             const std::vector<MultiDofConfig>& multi_dof_configs);
  JointModel* constructJointModel(const urdf::Joint *urdfJointModel,  const urdf::Link *child_link,
                        const std::vector<MultiDofConfig>& multi_dof_configs);
  LinkModel* constructLinkModel(const urdf::Link *urdfLink);
  shapes::Shape* constructShape(const urdf::Geometry *geom);

  JointModel* copyJointModel(const JointModel *joint);
  JointModel* copyRecursive(LinkModel *parent, const LinkModel *link);
	
};

}

#endif

// Copyright (c) 2016-2017, Philipp Sebastian Ruppel
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the Universität Hamburg nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <fcl/math/geometry.h>                                // for Vec3f
#include <moveit/collision_detection_fcl/collision_common.h>  // for FCLGeom...
#include <moveit/collision_detection_fcl/fcl_compat.h>        // for FCL_VER...
#include <moveit/robot_model/robot_model.h>                   // for RobotMo...
#include <stddef.h>                                           // for size_t
#include <sys/types.h>                                        // for ssize_t

#include <bio_ik/goal.hpp>        // for GoalCon...
#include <bio_ik/robot_info.hpp>  // for RobotInfo
#include <bio_ik/utils.hpp>       // for IKParams
#include <memory>                 // for shared_ptr
#include <vector>                 // for vector

#include "bio_ik/frame.hpp"  // for Vector3
namespace moveit {
namespace core {
class JointModelGroup;
}
}  // namespace moveit
namespace moveit {
namespace core {
class LinkModel;
}
}  // namespace moveit

namespace bio_ik {

class Problem {
 private:
  bool ros_params_initrd_;
  std::vector<int> joint_usage_;
  std::vector<ssize_t> link_tip_indices_;
  std::vector<double> minimal_displacement_factors_;
  std::vector<double> joint_transmission_goal_temp_,
      joint_transmission_goal_temp2_;
  moveit::core::RobotModelConstPtr robot_model_;
  const moveit::core::JointModelGroup* joint_model_group_;
  IKParams params_;
  RobotInfo modelInfo_;
  double dpos_, drot_, dtwist_;
#if (MOVEIT_FCL_VERSION < FCL_VERSION_CHECK(0, 6, 0))
  struct CollisionShape {
    std::vector<Vector3> vertices;
    std::vector<fcl::Vec3f> points;
    std::vector<int> polygons;
    std::vector<fcl::Vec3f> plane_normals;
    std::vector<double> plane_dis;
    collision_detection::FCLGeometryConstPtr geometry;
    Frame frame;
    std::vector<std::vector<size_t>> edges;
  };
  struct CollisionLink {
    bool initialized;
    std::vector<std::shared_ptr<CollisionShape>> shapes;
    CollisionLink() : initialized(false) {}
  };
  std::vector<CollisionLink> collision_links_;
#endif
  size_t addTipLink(const moveit::core::LinkModel* link_model);

 public:
  /*enum class GoalType;
  struct BalanceGoalInfo
  {
      ssize_t tip_index;
      double mass;
      Vector3 center;
  };
  struct GoalInfo
  {
      const Goal* goal;
      GoalType goal_type;
      size_t tip_index;
      double weight;
      double weight_sq;
      double rotation_scale;
      double rotation_scale_sq;
      Frame frame;
      tf2::Vector3 target;
      tf2::Vector3 direction;
      tf2::Vector3 axis;
      double distance;
      ssize_t active_variable_index;
      double variable_position;
      std::vector<ssize_t> variable_indices;
      mutable size_t last_collision_vertex;
      std::vector<BalanceGoalInfo> balance_goal_infos;
  };*/
  enum class GoalType;
  // std::vector<const Frame*> temp_frames;
  // std::vector<double> temp_variables;
  struct GoalInfo {
    const Goal* goal;
    double weight_sq;
    double weight;
    GoalType goal_type;
    size_t tip_index;
    Frame frame;
    GoalContext goal_context;
  };
  double timeout;
  std::vector<double> initial_guess;
  std::vector<size_t> active_variables;
  std::vector<size_t> tip_link_indices;
  std::vector<GoalInfo> goals;
  std::vector<GoalInfo> secondary_goals;
  Problem();
  void initialize(moveit::core::RobotModelConstPtr robot_model,
                  const moveit::core::JointModelGroup* joint_model_group,
                  const IKParams& params,
                  const std::vector<const Goal*>& goals2,
                  const BioIKKinematicsQueryOptions* options);
  void initialize2();
  double computeGoalFitness(GoalInfo& goal_info, const Frame* tip_frames,
                            const double* active_variable_positions);
  double computeGoalFitness(std::vector<GoalInfo>& goals,
                            const Frame* tip_frames,
                            const double* active_variable_positions);
  bool checkSolutionActiveVariables(const std::vector<Frame>& tip_frames,
                                    const double* active_variable_positions);
};
}  // namespace bio_ik

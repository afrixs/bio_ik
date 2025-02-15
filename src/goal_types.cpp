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

#include <eigen_stl_containers/eigen_stl_vector_container.h>  // for vector_...
#include <fcl/narrowphase/distance_request.h>                 // for Distanc...
#include <fcl/narrowphase/distance_result.h>                  // for Distanc...
#include <fcl/narrowphase/distance.h>                         // for distance
#include <fcl/geometry/shape/convex.h>                        // for Convex:...
#include <float.h>                                            // for DBL_MAX
#include <geometric_shapes/bodies.h>                          // for ConvexMesh
#include <geometric_shapes/shapes.h>                          // for Mesh
#include <moveit/collision_detection_fcl/collision_common.h>  // for FCLGeom...
#include <moveit/collision_detection_fcl/fcl_compat.h>        // for FCL_VER...
#include <moveit/robot_model/link_model.h>                    // for LinkModel
#include <moveit/robot_model/robot_model.h>                   // for RobotModel
#include <stddef.h>                                           // for size_t
#include <tf2/LinearMath/Vector3.h>                           // for Vector3
#include <urdf/urdfdom_compatibility.h>                       // for ModelIn...
#include <urdf_model/link.h>                                  // for Link
#include <urdf_model/model.h>                                 // for ModelIn...
#include <urdf_model/pose.h>                                  // for Vector3
#include <urdf_model/types.h>                                 // for Inertia...

#include <Eigen/Core>             // for Matri
#include <bio_ik/goal_types.hpp>  // for TouchGo...
#include <ext/alloc_traits.h>     // for __alloc...
#include <map>                    // for map
#include <memory>                 // for shared_ptr
#include <mutex>                  // for mutex
#include <new>                    // for operato...
#include <unordered_set>          // for unorder...
#include <vector>                 // for vector

#include "bio_ik/frame.hpp"  // for Vector3
#include "bio_ik/goal.hpp"   // for GoalCon...

namespace bio_ik {

#if (MOVEIT_FCL_VERSION < FCL_VERSION_CHECK(0, 6, 0))
void TouchGoal::describe(GoalContext& context) const {
  LinkGoalBase::describe(context);
  auto* robot_model = &context.getRobotModel();
  {
    static std::map<const moveit::core::RobotModel*, CollisionModel*>
        collision_cache;
    if (collision_cache.find(robot_model) == collision_cache.end())
      collision_cache[&context.getRobotModel()] = new CollisionModel();
    collision_model = collision_cache[robot_model];
    collision_model->collision_links.resize(robot_model->getLinkModelCount());
  }
  link_model = robot_model->getLinkModel(this->getLinkName());
  size_t link_index = link_model->getLinkIndex();
  // auto touch_goal_normal = normal_.normalized();
  // auto fbrot = fb.rot.normalized();
  auto& collision_link = collision_model->collision_links[link_index];
  if (!collision_link.initialized) {
    collision_link.initialized = true;
    collision_link.shapes.resize(link_model->getShapes().size());
    for (size_t shape_index = 0; shape_index < link_model->getShapes().size();
         shape_index++) {
      collision_link.shapes[shape_index] = std::make_shared<CollisionShape>();
      auto& s = *collision_link.shapes[shape_index];
      s.frame = Frame(link_model->getCollisionOriginTransforms()[shape_index]);
      auto* shape = link_model->getShapes()[shape_index].get();
      // LOG(link_model->getName(), shape_index, link_model->getShapes().size(),
      // typeid(*shape).name());
      if (auto* mesh = dynamic_cast<const shapes::Mesh*>(shape)) {
        struct : bodies::ConvexMesh {
          std::vector<fcl::Vec3f> points;
          std::vector<int> polygons;
          std::vector<fcl::Vec3f> plane_normals;
          std::vector<double> plane_dis;
          void init(const shapes::Shape* shape) {
            type_ = shapes::MESH;
            scaled_vertices_ = nullptr;
            {
              static std::mutex mutex;
              std::lock_guard<std::mutex> lock(mutex);
              setDimensions(shape);
            }
            for (const auto& v : getVertices())
              points.emplace_back(v.x(), v.y(), v.z());

            const auto& triangles = getTriangles();
            for (size_t triangle_index = 0;
                 triangle_index < triangles.size() / 3; triangle_index++) {
              polygons.push_back(3);
              polygons.push_back(
                  static_cast<int>(triangles[triangle_index * 3 + 0]));
              polygons.push_back(
                  static_cast<int>(triangles[triangle_index * 3 + 1]));
              polygons.push_back(
                  static_cast<int>(triangles[triangle_index * 3 + 2]));
            }
            // planes are given in the same order as the triangles, though
            // redundant ones will appear only once.
            for (const auto& plane : getPlanes()) {
              // planes stored as Eigen::Vector4d(nx, ny, nz, d)
              plane_normals.emplace_back(plane.x(), plane.y(), plane.z());
              plane_dis.push_back(plane.w());
            }
          }
        } convex;
        convex.init(mesh);
        s.points = convex.points;
        s.polygons = convex.polygons;
        s.plane_normals = convex.plane_normals;
        s.plane_dis = convex.plane_dis;

        // auto* fcl = new fcl::Convex(s.plane_normals.data(),
        // s.plane_dis.data(), s.plane_normals.size(), s.points.data(),
        // s.points.size(), s.polygons.data());

        // workaround for fcl::Convex initialization bug
        auto fcl = [&s]() -> fcl::Convex* {
          auto buffer = operator new(sizeof(fcl::Convex));
          static_cast<fcl::Convex*>(buffer)->num_points =
              static_cast<int>(s.points.size());
          return new (buffer) fcl::Convex(
              s.plane_normals.data(), s.plane_dis.data(),
              static_cast<int>(s.plane_normals.size()), s.points.data(),
              static_cast<int>(s.points.size()), s.polygons.data());
        }();

        s.geometry = decltype(s.geometry)(new collision_detection::FCLGeometry(
            fcl, link_model, static_cast<int>(shape_index)));
        s.edges.resize(s.points.size());
        std::vector<std::unordered_set<size_t>> edge_sets(s.points.size());
        for (size_t edge_index = 0;
             edge_index < static_cast<size_t>(fcl->num_edges); edge_index++) {
          auto edge = fcl->edges[edge_index];
          auto first_index = static_cast<size_t>(edge.first);
          auto second_index = static_cast<size_t>(edge.second);
          if (edge_sets[first_index].find(second_index) ==
              edge_sets[first_index].end()) {
            edge_sets[first_index].insert(second_index);
            s.edges[first_index].push_back(second_index);
          }
          if (edge_sets[second_index].find(first_index) ==
              edge_sets[second_index].end()) {
            edge_sets[second_index].insert(first_index);
            s.edges[second_index].push_back(first_index);
          }
        }
        for (auto& p : s.points) s.vertices.emplace_back(p[0], p[1], p[2]);
      } else {
        s.geometry = collision_detection::createCollisionGeometry(
            link_model->getShapes()[shape_index], link_model,
            static_cast<int>(shape_index));
      }
      // LOG("b");
    }
    // getchar();
  }
}

double TouchGoal::evaluate(const GoalContext& context) const {
  double dmin = DBL_MAX;
  size_t last_collision_vertex = 0;
  auto& fb = context.getLinkFrame();
  size_t link_index = link_model->getLinkIndex();
  auto& collision_link = collision_model->collision_links[link_index];
  for (size_t shape_index = 0; shape_index < link_model->getShapes().size();
       shape_index++) {
    if (!collision_link.shapes[shape_index]->geometry) continue;
    auto* shape = link_model->getShapes()[shape_index].get();
    // LOG(shape_index, typeid(*shape).name());
    if (auto* mesh = dynamic_cast<const shapes::Mesh*>(shape)) {
      auto& s = collision_link.shapes[shape_index];
      double d = DBL_MAX;
      auto goal_normal = normal_;
      quat_mul_vec(fb.rot.inverse(), goal_normal, goal_normal);
      quat_mul_vec(s->frame.rot.inverse(), goal_normal, goal_normal);
      /*{
          size_t array_index = 0;
          for(size_t vertex_index = 0; vertex_index < mesh->vertex_count;
      vertex_index++)
          {
              double dot_x = mesh->vertices[array_index++] * goal_normal.x();
              double dot_y = mesh->vertices[array_index++] * goal_normal.y();
              double dot_z = mesh->vertices[array_index++] * goal_normal.z();
              double e = dot_x + dot_y + dot_z;
              if(e < d) d = e;
          }
      }*/
      if (mesh->vertex_count > 0) {
        size_t vertex_index = last_collision_vertex;
        double vertex_dot_normal = goal_normal.dot(s->vertices[vertex_index]);
        // size_t loops = 0;
        while (true) {
          bool repeat = false;
          for (auto vertex_index_2 : s->edges[vertex_index]) {
            auto vertex_dot_normal_2 =
                goal_normal.dot(s->vertices[vertex_index_2]);
            if (vertex_dot_normal_2 < vertex_dot_normal) {
              vertex_index = vertex_index_2;
              vertex_dot_normal = vertex_dot_normal_2;
              repeat = true;
              break;
            }
          }
          if (!repeat) break;
          // loops++;
        }
        // LOG_VAR(loops);
        d = vertex_dot_normal;
        last_collision_vertex = vertex_index;
      }
      d -= normal_.dot(position_ - fb.pos);
      // ROS_INFO("touch goal");
      if (d < dmin) dmin = d;
    } else {
      double offset = 10000;
      static fcl::Sphere shape1(offset);
      fcl::DistanceRequest request;
      fcl::DistanceResult result;
      auto pos1 = position_ - normal_ * offset * 2;
      auto* shape2 = collision_link.shapes[shape_index]
                         ->geometry->collision_geometry_.get();
      auto frame2 = Frame(fb.pos, fb.rot.normalized()) *
                    collision_link.shapes[shape_index]->frame;
      double d = fcl::distance(
          &shape1, fcl::Transform3f(fcl::Vec3f(pos1.x(), pos1.y(), pos1.z())),
          shape2,
          fcl::Transform3f(
              fcl::Quaternion3f(frame2.rot.w(), frame2.rot.x(), frame2.rot.y(),
                                frame2.rot.z()),
              fcl::Vec3f(frame2.pos.x(), frame2.pos.y(), frame2.pos.z())),
          request, result);
      d -= offset;
      if (d < dmin) dmin = d;
    }
  }
  return dmin * dmin;
}
#endif

void BalanceGoal::describe(GoalContext& context) const {
  Goal::describe(context);
  balance_infos.clear();
  double total = 0.0;
  for (auto& link_name : context.getRobotModel().getLinkModelNames()) {
    auto link_urdf = context.getRobotModel().getURDF()->getLink(link_name);
    if (!link_urdf) continue;
    if (!link_urdf->inertial) continue;
    const auto& center_urdf = link_urdf->inertial->origin.position;
    tf2::Vector3 center(center_urdf.x, center_urdf.y, center_urdf.z);
    double mass = link_urdf->inertial->mass;
    if (!(mass > 0)) continue;
    balance_infos.emplace_back();
    balance_infos.back().center = center;
    balance_infos.back().weight = mass;
    total += mass;
    context.addLink(link_name);
  }
  for (auto& b : balance_infos) {
    b.weight /= total;
  }
}

double BalanceGoal::evaluate(const GoalContext& context) const {
  tf2::Vector3 center(0, 0, 0);
  for (size_t i = 0; i < balance_infos.size(); i++) {
    auto& info = balance_infos[i];
    auto& frame = context.getLinkFrame(i);
    auto c = info.center;
    quat_mul_vec(frame.rot, c, c);
    c += frame.pos;
    center += c * info.weight;
  }
  center -= target_;
  center -= axis_ * axis_.dot(center);
  return center.length2();
}
}  // namespace bio_ik

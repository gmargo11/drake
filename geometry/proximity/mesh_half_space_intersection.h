#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "drake/common/eigen_types.h"
#include "drake/common/sorted_pair.h"
#include "drake/geometry/geometry_ids.h"
#include "drake/geometry/proximity/bvh.h"
#include "drake/geometry/proximity/contact_surface_utility.h"
#include "drake/geometry/proximity/posed_half_space.h"
#include "drake/geometry/proximity/triangle_surface_mesh.h"
#include "drake/geometry/query_results/contact_surface.h"

namespace drake {
namespace geometry {
namespace internal {

/*
 Computes the intersection between a triangle and a half space.
 The intersecting geometry (e.g. vertices and triangles) are added to the
 provided collections. This method is intended to be used in contexts where
 zero area triangles and triangles coplanar with the half space boundary are
 unimportant (see note below).

 This function is a component of the larger operation of clipping a
 TriangleSurfaceMesh by a half space.

 For `representation` = ContactPolygonRepresentation::kCentroidSubdivision,
 we do not introduce any duplicate vertices (beyond those already in the input
 mesh). This function uses bookkeeping to prevent the addition of such
 duplicate vertices, which can be created when either a vertex lies inside/on
 the half space or when an edge intersects the half space. The method tracks
 these resultant vertices using `vertices_to_newly_created_vertices` and
 `edges_to_newly_created_vertices`, respectively.

 For `representation` = ContactPolygonRepresentation::kSingleTriangle, it
 creates 3 or 4 times fewer triangles than the other choice by representing
 the intersected polygon (triangle or quadrilateral) as one triangle. However,
 it allows duplicate vertices and ignores `vertices_to_newly_created_vertices`
 and `edges_to_newly_created_vertices`.

 @param mesh_F
     The surface mesh to intersect, measured and expressed in Frame F.
 @param tri_index
     The index of the triangle, drawn from `mesh_F` to intersect with the half
     space.
 @param half_space_F
     The half space measured and expressed in Frame F.
 @param X_WF
     The relative pose between Frame F and Frame W.
 @param[in] representation
     The preferred representation of the intersected polygon.
 @param[in,out] new_vertices_W
     The accumulator for all of the vertices in the intersecting mesh. It should
     be empty for the first call and will gradually accumulate all of the
     vertices with each subsequent call to this method. The vertex positions are
     measured and expressed in frame W.
 @param[in,out] new_faces
     The accumulator for all of the faces in the intersecting mesh. It should be
     empty for the first call and will gradually accumulate all of the faces
     with each subsequent call to this method.
 @param[in,out] vertices_to_newly_created_vertices
     The accumulated mapping from indices in `mesh_F.vertices()` to indices in
     `new_vertices_W`. It should be empty for the first call and will gradually
     accumulate elements with each subsequent call to this method.
 @param[in,out] edges_to_newly_created_vertices
     The accumulated mapping from pairs of indices in `mesh_F.vertices()` to
     indices in `new_vertices_W`. It should be empty for the first call and
     will gradually accumulate elements with each subsequent call to this
     method.

 @note Unlike most geometric intersection routines, this method does not
       require the user to provide (or the algorithm to compute) a reasonable
       floating point tolerance for zero. This simple interface implies both
       that the method may construct degenerate triangles and that it may
       fail to register an intersection with a triangle that is coplanar with
       the half space surface. In certain applications (e.g., hydroelastic
       contact), both degenerate triangles and triangles coplanar with the
       half space surface are innocuous (in hydroelastic contact, for example,
       both cases contribute nothing to the contact wrench).
*/
template <typename T>
void ConstructTriangleHalfspaceIntersectionPolygon(
    const TriangleSurfaceMesh<double>& mesh_F, int tri_index,
    const PosedHalfSpace<T>& half_space_F, const math::RigidTransform<T>& X_WF,
    ContactPolygonRepresentation representation,
    std::vector<Vector3<T>>* new_vertices_W,
    std::vector<SurfaceTriangle>* new_faces,
    std::unordered_map<int, int>* vertices_to_newly_created_vertices,
    std::unordered_map<SortedPair<int>, int>* edges_to_newly_created_vertices);

/*
 Computes a triangular surface mesh by intersecting a half space with a set of
 triangles drawn from the given surface mesh. The indicated triangles would
 typically be the result of broadphase culling.

 For `representation` = ContactPolygonRepresentation::kCentroidSubdivision,
 each triangle in `mesh_W` is a proper subset of one triangle in `input_mesh_F`
 (i.e., fully contained within the triangle in `input_mesh_F`). As such,
 the face normal for each triangle in `mesh_W` will point in the same direction
 as its containing triangle in `input_mesh_F`.

 For `representation` = ContactPolygonRepresentation::kSingleTriangle, each
 triangle in `mesh_W` corresponds to one triangle in `input_mesh_F` that
 intersects the half space. Their face normals will point in the same direction.

 @param input_mesh_F
     The mesh with vertices measured and expressed in some frame, F.
 @param half_space_F
     The half space measured and expressed in Frame F.
 @param tri_indices
     A collection of triangle indices in `input_mesh_F`.
 @param X_WF
     The relative pose of Frame F with the world frame W.
 @param[in] representation
     The preferred representation of each intersected polygon.
 @retval `mesh_W`, the TriangleSurfaceMesh corresponding to the intersection
         between the mesh and the half space; the vertices are measured and
         expressed in Frame W. `nullptr` if there is no intersection.
 */
template <typename T>
std::unique_ptr<TriangleSurfaceMesh<T>>
ConstructSurfaceMeshFromMeshHalfspaceIntersection(
    const TriangleSurfaceMesh<double>& input_mesh_F,
    const PosedHalfSpace<T>& half_space_F, const std::vector<int>& tri_indices,
    const math::RigidTransform<T>& X_WF,
    ContactPolygonRepresentation representation);

/*
 Computes the ContactSurface formed by a soft half space and the given rigid
 mesh.

 The definition of the half space is implicit in the call -- it is the type
 defined by the HalfSpace class, thus, only its id, its pose in a common frame
 (the world frame), and the `pressure_scale` is necessary.

 @param[in] id_S            The id of the soft half space.
 @param[in] X_WS            The relative pose of Frame S (the soft half space's
                            canonical frame) and the world frame W.
 @param[in] pressure_scale  A linear scale factor that transforms penetration
                            depth into pressure values. Generally,
                            `pressure_scale = hydroelastic_modulus / thickness`.
 @param[in] id_R            The id of the rigid mesh.
 @param[in] mesh_R          The rigid mesh. The mesh vertices are measured and
                            expressed in Frame R.
 @param[in] bvh_R           The bounding-volume hierarchy of the rigid surface
                            mesh measured and expressed in Frame R.
 @param[in] X_WR            The relative pose between Frame R -- the frame the
                            mesh is defined in -- and the world frame W.
 @param[in] representation  The preferred representation of each contact
                            polygon.
 @returns `nullptr` if there is no collision, otherwise the ContactSurface
          between geometries S and R. Each triangle in the contact surface is a
          piece of a triangle in the input `mesh_R`; the normals of the former
          will match the normals of the latter.
 @tparam T The underlying scalar type. Must be a valid Eigen scalar. Currently,
           only double is supported.
 */
template <typename T>
std::unique_ptr<ContactSurface<T>>
ComputeContactSurfaceFromSoftHalfSpaceRigidMesh(
    GeometryId id_S, const math::RigidTransform<T>& X_WS, double pressure_scale,
    GeometryId id_R, const TriangleSurfaceMesh<double>& mesh_R,
    const Bvh<Obb, TriangleSurfaceMesh<double>>& bvh_R,
    const math::RigidTransform<T>& X_WR,
    ContactPolygonRepresentation representation);

}  // namespace internal
}  // namespace geometry
}  // namespace drake

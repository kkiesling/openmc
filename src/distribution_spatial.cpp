#include "openmc/distribution_spatial.h"

#include "openmc/error.h"
#include "openmc/mesh.h"
#include "openmc/random_lcg.h"
#include "openmc/search.h"
#include "openmc/xml_interface.h"

namespace openmc {

//==============================================================================
// CartesianIndependent implementation
//==============================================================================

CartesianIndependent::CartesianIndependent(pugi::xml_node node)
{
  // Read distribution for x coordinate
  if (check_for_node(node, "x")) {
    pugi::xml_node node_dist = node.child("x");
    x_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at x=0
    double x[] {0.0};
    double p[] {1.0};
    x_ = UPtrDist {new Discrete {x, p, 1}};
  }

  // Read distribution for y coordinate
  if (check_for_node(node, "y")) {
    pugi::xml_node node_dist = node.child("y");
    y_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at y=0
    double x[] {0.0};
    double p[] {1.0};
    y_ = UPtrDist {new Discrete {x, p, 1}};
  }

  // Read distribution for z coordinate
  if (check_for_node(node, "z")) {
    pugi::xml_node node_dist = node.child("z");
    z_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at z=0
    double x[] {0.0};
    double p[] {1.0};
    z_ = UPtrDist {new Discrete {x, p, 1}};
  }
}

Position CartesianIndependent::sample(uint64_t* seed) const
{
  return {x_->sample(seed), y_->sample(seed), z_->sample(seed)};
}

//==============================================================================
// CylindricalIndependent implementation
//==============================================================================

CylindricalIndependent::CylindricalIndependent(pugi::xml_node node)
{
  // Read distribution for r-coordinate
  if (check_for_node(node, "r")) {
    pugi::xml_node node_dist = node.child("r");
    r_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at r=0
    double x[] {0.0};
    double p[] {1.0};
    r_ = make_unique<Discrete>(x, p, 1);
  }

  // Read distribution for phi-coordinate
  if (check_for_node(node, "phi")) {
    pugi::xml_node node_dist = node.child("phi");
    phi_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at phi=0
    double x[] {0.0};
    double p[] {1.0};
    phi_ = make_unique<Discrete>(x, p, 1);
  }

  // Read distribution for z-coordinate
  if (check_for_node(node, "z")) {
    pugi::xml_node node_dist = node.child("z");
    z_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at z=0
    double x[] {0.0};
    double p[] {1.0};
    z_ = make_unique<Discrete>(x, p, 1);
  }

  // Read cylinder center coordinates
  if (check_for_node(node, "origin")) {
    auto origin = get_node_array<double>(node, "origin");
    if (origin.size() == 3) {
      origin_ = origin;
    } else {
      fatal_error(
        "Origin for cylindrical source distribution must be length 3");
    }
  } else {
    // If no coordinates were specified, default to (0, 0, 0)
    origin_ = {0.0, 0.0, 0.0};
  }
}

Position CylindricalIndependent::sample(uint64_t* seed) const
{
  double r = r_->sample(seed);
  double phi = phi_->sample(seed);
  double x = r * cos(phi) + origin_.x;
  double y = r * sin(phi) + origin_.y;
  double z = z_->sample(seed) + origin_.z;
  return {x, y, z};
}

//==============================================================================
// SphericalIndependent implementation
//==============================================================================

SphericalIndependent::SphericalIndependent(pugi::xml_node node)
{
  // Read distribution for r-coordinate
  if (check_for_node(node, "r")) {
    pugi::xml_node node_dist = node.child("r");
    r_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at r=0
    double x[] {0.0};
    double p[] {1.0};
    r_ = make_unique<Discrete>(x, p, 1);
  }

  // Read distribution for cos_theta-coordinate
  if (check_for_node(node, "cos_theta")) {
    pugi::xml_node node_dist = node.child("cos_theta");
    cos_theta_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at
    // cos_theta=0
    double x[] {0.0};
    double p[] {1.0};
    cos_theta_ = make_unique<Discrete>(x, p, 1);
  }

  // Read distribution for phi-coordinate
  if (check_for_node(node, "phi")) {
    pugi::xml_node node_dist = node.child("phi");
    phi_ = distribution_from_xml(node_dist);
  } else {
    // If no distribution was specified, default to a single point at phi=0
    double x[] {0.0};
    double p[] {1.0};
    phi_ = make_unique<Discrete>(x, p, 1);
  }

  // Read sphere center coordinates
  if (check_for_node(node, "origin")) {
    auto origin = get_node_array<double>(node, "origin");
    if (origin.size() == 3) {
      origin_ = origin;
    } else {
      fatal_error("Origin for spherical source distribution must be length 3");
    }
  } else {
    // If no coordinates were specified, default to (0, 0, 0)
    origin_ = {0.0, 0.0, 0.0};
  }
}

Position SphericalIndependent::sample(uint64_t* seed) const
{
  double r = r_->sample(seed);
  double cos_theta = cos_theta_->sample(seed);
  double phi = phi_->sample(seed);
  // sin(theta) by sin**2 + cos**2 = 1
  double x = r * std::sqrt(1 - cos_theta * cos_theta) * cos(phi) + origin_.x;
  double y = r * std::sqrt(1 - cos_theta * cos_theta) * sin(phi) + origin_.y;
  double z = r * cos_theta + origin_.z;
  return {x, y, z};
}

//==============================================================================
// MeshSpatial implementation
//==============================================================================

MeshSpatial::MeshSpatial(pugi::xml_node node)
{
  // No in-tet distributions implemented, could include distributions for the
  // barycentric coords Read in unstructured mesh from mesh_id value
  int32_t mesh_id = std::stoi(get_node_value(node, "mesh_id"));
  // Get pointer to spatial distribution
  mesh_idx_ = model::mesh_map.at(mesh_id);

  auto mesh_ptr =
    dynamic_cast<UnstructuredMesh*>(model::meshes.at(mesh_idx_).get());
  if (!mesh_ptr) {
    fatal_error("Only unstructured mesh is supported for source sampling.");
  }

  // ensure that the unstructured mesh contains only linear tets
  for (int bin = 0; bin < mesh_ptr->n_bins(); bin++) {
    if (mesh_ptr->element_type(bin) != ElementType::LINEAR_TET) {
      fatal_error(
        "Mesh specified for source must contain only linear tetrahedra.");
    }
  }

  int32_t n_bins = this->n_sources();
  std::vector<double> strengths(n_bins, 0.0);

  mesh_CDF_.resize(n_bins + 1);
  mesh_CDF_[0] = {0.0};
  total_strength_ = 0.0;

  // Create cdfs for sampling for an element over a mesh
  // Volume scheme is weighted by the volume of each tet
  // File scheme is weighted by an array given in the xml file
  mesh_strengths_ = std::vector<double>(n_bins, 1.0);
  if (check_for_node(node, "strengths")) {
    strengths = get_node_array<double>(node, "strengths");
    if (strengths.size() != n_bins) {
      fatal_error(
        fmt::format("Number of entries in the source strengths array {} does "
                    "not match the number of entities in mesh {} ({}).",
          strengths.size(), mesh_id, n_bins));
    }
    mesh_strengths_ = std::move(strengths);
  }

  if (get_node_value_bool(node, "volume_normalized")) {
    for (int i = 0; i < n_bins; i++) {
      mesh_strengths_[i] *= mesh()->volume(i);
    }
  }

  total_strength_ =
    std::accumulate(mesh_strengths_.begin(), mesh_strengths_.end(), 0.0);

  for (int i = 0; i < n_bins; i++) {
    mesh_CDF_[i + 1] = mesh_CDF_[i] + mesh_strengths_[i] / total_strength_;
  }

  if (fabs(mesh_CDF_.back() - 1.0) > FP_COINCIDENT) {
    fatal_error(
      fmt::format("Mesh sampling CDF is incorrectly formed. Final value is: {}",
        mesh_CDF_.back()));
  }
  mesh_CDF_.back() = 1.0;
}

Position MeshSpatial::sample(uint64_t* seed) const
{
  // Create random variable for sampling element from mesh
  double eta = prn(seed);
  // Sample over the CDF defined in initialization above
  int32_t elem_idx = lower_bound_index(mesh_CDF_.begin(), mesh_CDF_.end(), eta);
  return mesh()->sample(seed, elem_idx);
}

//==============================================================================
// SpatialBox implementation
//==============================================================================

SpatialBox::SpatialBox(pugi::xml_node node, bool fission)
  : only_fissionable_ {fission}
{
  // Read lower-right/upper-left coordinates
  auto params = get_node_array<double>(node, "parameters");
  if (params.size() != 6)
    openmc::fatal_error("Box/fission spatial source must have six "
                        "parameters specified.");

  lower_left_ = Position {params[0], params[1], params[2]};
  upper_right_ = Position {params[3], params[4], params[5]};
}

Position SpatialBox::sample(uint64_t* seed) const
{
  Position xi {prn(seed), prn(seed), prn(seed)};
  return lower_left_ + xi * (upper_right_ - lower_left_);
}

//==============================================================================
// SpatialPoint implementation
//==============================================================================

SpatialPoint::SpatialPoint(pugi::xml_node node)
{
  // Read location of point source
  auto params = get_node_array<double>(node, "parameters");
  if (params.size() != 3)
    openmc::fatal_error("Point spatial source must have three "
                        "parameters specified.");

  // Set position
  r_ = Position {params.data()};
}

Position SpatialPoint::sample(uint64_t* seed) const
{
  return r_;
}

} // namespace openmc

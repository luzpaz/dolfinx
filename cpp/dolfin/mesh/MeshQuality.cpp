// Copyright (C) 2013 Garth N. Wells
//
// This file is part of DOLFIN (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

#include "MeshQuality.h"
#include "Cell.h"
#include "Mesh.h"
#include "MeshFunction.h"
#include "MeshIterator.h"
#include "Vertex.h"
#include <dolfin/common/MPI.h>
#include <sstream>

using namespace dolfin;
using namespace dolfin::mesh;

//-----------------------------------------------------------------------------
MeshFunction<double>
MeshQuality::radius_ratios(std::shared_ptr<const Mesh> mesh)
{
  // Create MeshFunction
  MeshFunction<double> cf(mesh, mesh->topology().dim(), 0.0);

  // Compute radius ration
  for (auto& cell : MeshRange<Cell>(*mesh))
    cf[cell] = cell.radius_ratio();

  return cf;
}
//-----------------------------------------------------------------------------
std::pair<double, double> MeshQuality::radius_ratio_min_max(const Mesh& mesh)
{
  double qmin = std::numeric_limits<double>::max();
  double qmax = 0.0;
  for (auto& cell : MeshRange<Cell>(mesh))
  {
    qmin = std::min(qmin, cell.radius_ratio());
    qmax = std::max(qmax, cell.radius_ratio());
  }

  qmin = MPI::min(mesh.mpi_comm(), qmin);
  qmax = MPI::max(mesh.mpi_comm(), qmax);
  return {qmin, qmax};
}
//-----------------------------------------------------------------------------
std::pair<std::vector<double>, std::vector<double>>
MeshQuality::radius_ratio_histogram_data(const Mesh& mesh, std::size_t num_bins)
{
  std::vector<double> bins(num_bins), values(num_bins, 0.0);
  assert(radius_ratio_min_max(mesh).second <= 1.0);

  const double interval = 1.0 / static_cast<double>(num_bins);
  for (std::size_t i = 0; i < num_bins; ++i)
    bins[i] = static_cast<double>(i) * interval + interval / 2.0;

  std::cout << interval << std::endl;
  std::cout << num_bins << std::endl;
  std::cout << static_cast<double>(num_bins) << std::endl;

  for (auto& cell : MeshRange<Cell>(mesh))
  {
    const double ratio = cell.radius_ratio();

    // Compute 'bin' index, and handle special case that ratio = 1.0
    const std::size_t slot
        = std::min(static_cast<std::size_t>(ratio / interval), num_bins - 1);

    values[slot] += 1.0;
  }

  for (std::size_t i = 0; i < values.size(); ++i)
    values[i] = MPI::sum(mesh.mpi_comm(), values[i]);

  return {bins, values};
}
//-----------------------------------------------------------------------------
std::string
MeshQuality::radius_ratio_matplotlib_histogram(const Mesh& mesh,
                                               std::size_t num_intervals)
{
  // Compute data
  std::pair<std::vector<double>, std::vector<double>> data
      = radius_ratio_histogram_data(mesh, num_intervals);

  assert(!data.first.empty());
  assert(data.first.size() == data.second.size());

  // Create Matplotlib string
  std::stringstream matplotlib;
  matplotlib << "def plot_histogram():" << std::endl;
  matplotlib << "    import matplotlib.pyplot" << std::endl;
  std::stringstream bins, values;
  bins << "    bins = [" << data.first[0];
  values << "    values = [" << data.second[0];
  for (std::size_t i = 1; i < data.first.size(); ++i)
  {
    bins << ", " << data.first[i];
    values << ", " << data.second[i];
  }
  bins << "]";
  values << "]";

  matplotlib << bins.str() << std::endl;
  matplotlib << values.str() << std::endl;
  matplotlib << std::endl;

  matplotlib << "    matplotlib.pylab.xlim([0, 1])" << std::endl;
  matplotlib << "    width = 0.7*(bins[1] - bins[0])" << std::endl;
  matplotlib << "    matplotlib.pylab.xlabel('radius ratio')" << std::endl;
  matplotlib << "    matplotlib.pylab.ylabel('number of cells')" << std::endl;
  matplotlib
      << "    matplotlib.pylab.bar(bins, values, align='center', width=width)"
      << std::endl;
  matplotlib << "    matplotlib.pylab.show()" << std::endl;

  matplotlib << std::endl;
  matplotlib << "try:" << std::endl;
  matplotlib << "    import matplotlib.pylab" << std::endl;
  matplotlib << "except ImportError:" << std::endl;
  matplotlib
      << "    print(\"Plotting mesh quality histogram requires Matplotlib\")"
      << std::endl;
  matplotlib << "else:" << std::endl;
  matplotlib << "    plot_histogram()" << std::endl;

  return matplotlib.str();
}
//-----------------------------------------------------------------------------
void MeshQuality::dihedral_angles(const Cell& cell,
                                  std::vector<double>& dh_angle)
{
  if (cell.dim() != 3)
  {
    log::dolfin_error("MeshQuality.cpp", "calculate dihedral angles",
                 "Only works for 3D cells");
  }

  static const std::size_t edges[6][2]
      = {{2, 3}, {1, 3}, {1, 2}, {0, 3}, {0, 2}, {0, 1}};
  const Mesh& mesh = cell.mesh();

  dh_angle.resize(6);
  for (std::uint32_t i = 0; i < 6; ++i)
  {
    const std::size_t i0 = cell.entities(0)[edges[i][0]];
    const std::size_t i1 = cell.entities(0)[edges[i][1]];
    const std::size_t i2 = cell.entities(0)[edges[5 - i][0]];
    const std::size_t i3 = cell.entities(0)[edges[5 - i][1]];
    const geometry::Point p0 = Vertex(mesh, i0).point();
    geometry::Point v1 = Vertex(mesh, i1).point() - p0;
    geometry::Point v2 = Vertex(mesh, i2).point() - p0;
    geometry::Point v3 = Vertex(mesh, i3).point() - p0;
    v1 /= v1.norm();
    v2 /= v2.norm();
    v3 /= v3.norm();
    double cphi = (v2.dot(v3) - v1.dot(v2) * v1.dot(v3))
                  / (v1.cross(v2).norm() * v1.cross(v3).norm());
    dh_angle[i] = acos(cphi);
  }
}
//-----------------------------------------------------------------------------
std::pair<double, double> MeshQuality::dihedral_angles_min_max(const Mesh& mesh)
{
  // Get the maximum and minimum value of dihedral angles in cells across a mesh

  // Get original min and max
  double d_ang_min = DOLFIN_PI + 1.0;
  double d_ang_max = -1.0;

  std::vector<double> angs(6);
  for (auto& cell : MeshRange<Cell>(mesh))
  {
    // Get the angles from the next cell
    dihedral_angles(cell, angs);

    // And then update the min and max
    d_ang_min
        = std::min(d_ang_min, *std::min_element(angs.begin(), angs.end()));
    d_ang_max
        = std::max(d_ang_max, *std::max_element(angs.begin(), angs.end()));
  }

  d_ang_min = MPI::min(mesh.mpi_comm(), d_ang_min);
  d_ang_max = MPI::max(mesh.mpi_comm(), d_ang_max);

  return {d_ang_min, d_ang_max};
}
//-----------------------------------------------------------------------------
std::pair<std::vector<double>, std::vector<double>>
MeshQuality::dihedral_angles_histogram_data(const Mesh& mesh,
                                            std::size_t num_bins)
{
  std::vector<double> bins(num_bins), values(num_bins, 0.0);

  // May need to assert the minimum and maximum possible angle
  assert(dihedral_angles_min_max(mesh).first
                >= 0.0); // Is this really needed?
  assert(dihedral_angles_min_max(mesh).second <= M_PI);

  // Currently min value is 0.0 and max is M_PI
  const double interval = M_PI / (static_cast<double>(num_bins));

  for (std::size_t i = 0; i < num_bins; ++i)
    bins[i] = static_cast<double>(i) * interval + interval / 2.0;

  std::vector<double> angs(6);
  for (auto& cell : MeshRange<Cell>(mesh))
  {
    // this one should return the value of the angle
    dihedral_angles(cell, angs);

    // Iterate through the collected vector
    for (std::size_t i = 0; i < angs.size(); i++)
    {
      // Compute 'bin' index, and handle special case that angle = M_PI
      const std::size_t slot = std::min(
          static_cast<std::size_t>(angs[i] / interval), num_bins - 1);

      // const std::size_t slot = static_cast<std::size_t>((angs[i] -
      // d_ang_min)/interval);
      values[slot] += 1.0;
    }
  }

  for (std::size_t i = 0; i < values.size(); ++i)
  {
    values[i] = MPI::sum(mesh.mpi_comm(), values[i]);
  }

  return {bins, values};
}
//-----------------------------------------------------------------------------
std::string
MeshQuality::dihedral_angles_matplotlib_histogram(const Mesh& mesh,
                                                  std::size_t num_intervals)
{
  // Compute data
  std::pair<std::vector<double>, std::vector<double>> data
      = dihedral_angles_histogram_data(mesh, num_intervals);

  assert(!data.first.empty());
  assert(data.first.size() == data.second.size());

  // Create Matplotlib string
  std::stringstream matplotlib;
  matplotlib << "def plot_histogram():" << std::endl;
  matplotlib << "    import matplotlib.pyplot" << std::endl;
  std::stringstream bins, values;
  bins << "    bins = [" << data.first[0];
  values << "    values = [" << data.second[0];
  for (std::size_t i = 1; i < data.first.size(); ++i)
  {
    bins << ", " << data.first[i];
    values << ", " << data.second[i];
  }
  bins << "]";
  values << "]";

  matplotlib << bins.str() << std::endl;
  matplotlib << values.str() << std::endl;
  matplotlib << std::endl;

  matplotlib << "    width = 0.7*(bins[1] - bins[0])" << std::endl;
  matplotlib << "    matplotlib.pylab.xlabel('dihedral angles')" << std::endl;
  matplotlib << "    matplotlib.pylab.ylabel('number of edges')"
             << std::endl; // this is weird...
  matplotlib
      << "    matplotlib.pylab.bar(bins, values, align='center', width=width)"
      << std::endl;
  matplotlib << "    matplotlib.pylab.show()" << std::endl;

  matplotlib << std::endl;
  matplotlib << "try:" << std::endl;
  matplotlib << "    import matplotlib.pylab" << std::endl;
  matplotlib << "except ImportError:" << std::endl;
  matplotlib
      << "    print(\"Plotting mesh quality histogram requires Matplotlib\")"
      << std::endl;
  matplotlib << "else:" << std::endl;
  matplotlib << "    plot_histogram()" << std::endl;

  return matplotlib.str();
}

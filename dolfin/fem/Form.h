// Copyright (C) 2007-2014 Anders Logg
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
//
// Modified by Garth N. Wells 2008-2011
// Modified by Martin Alnes 2008
//
// First added:  2007-04-02
// Last changed: 2014-02-14

#ifndef __FORM_H
#define __FORM_H

#include <map>
#include <vector>
#include <memory>

#include <dolfin/common/types.h>

// Forward declaration
namespace ufc
{
  class form;
}

namespace dolfin
{

  class FunctionSpace;
  class GenericFunction;
  class Mesh;
  template <typename T> class MeshFunction;

  /// Base class for UFC code generated by FFC for DOLFIN with option -l.

  /// @verbatim embed:rst:leading-slashes
  /// A note on the order of trial and test spaces: FEniCS numbers
  /// argument spaces starting with the leading dimension of the
  /// corresponding tensor (matrix). In other words, the test space is
  /// numbered 0 and the trial space is numbered 1. However, in order
  /// to have a notation that agrees with most existing finite element
  /// literature, in particular
  ///
  ///     a = a(u, v)
  ///
  /// the spaces are numbered from right to
  ///
  ///     a: V_1 x V_0 -> R
  ///
  /// .. note::
  ///
  ///     Figure out how to write this in math mode without it getting
  ///     messed up in the Python version.
  ///
  /// This is reflected in the ordering of the spaces that should be
  /// supplied to generated subclasses. In particular, when a bilinear
  /// form is initialized, it should be initialized as
  ///
  /// .. code-block:: c++
  ///
  ///     a(V_1, V_0) = ...
  ///
  /// where ``V_1`` is the trial space and ``V_0`` is the test space.
  /// However, when a form is initialized by a list of argument spaces
  /// (the variable ``function_spaces`` in the constructors below, the
  /// list of spaces should start with space number 0 (the test space)
  /// and then space number 1 (the trial space).
  /// @endverbatim

  class Form
  {
  public:

    /// Create form of given rank with given number of coefficients
    ///
    /// @param[in]    rank (std::size_t)
    ///         The rank.
    /// @param[in]    num_coefficients (std::size_t)
    ///         The number of coefficients.
    Form(std::size_t rank, std::size_t num_coefficients);

    /// Create form (shared data)
    ///
    /// @param[in] ufc_form (ufc::form)
    ///         The UFC form.
    /// @param[in] function_spaces (std::vector<_FunctionSpace_>)
    ///         Vector of function spaces.
    Form(std::shared_ptr<const ufc::form> ufc_form,
         std::vector<std::shared_ptr<const FunctionSpace>> function_spaces);

    /// Destructor
    virtual ~Form();

    /// Return rank of form (bilinear form = 2, linear form = 1,
    /// functional = 0, etc)
    ///
    /// @return std::size_t
    ///         The rank of the form.
    std::size_t rank() const;

    /// Return number of coefficients
    ///
    /// @return std::size_t
    ///         The number of coefficients.
    std::size_t num_coefficients() const;

    /// Return original coefficient position for each coefficient (0
    /// <= i < n)
    ///
    /// @return std::size_t
    ///         The position of coefficient i in original ufl form coefficients.
    std::size_t original_coefficient_position(std::size_t i) const;

    /// Return coloring type for colored assembly of form over a mesh
    /// entity of a given dimension
    ///
    /// @param[in] entity_dim (std::size_t)
    ///         Dimension.
    ///
    /// @return std::vector<std::size_t>
    ///         Coloring type.
    std::vector<std::size_t> coloring(std::size_t entity_dim) const;

    /// Set mesh, necessary for functionals when there are no function
    /// spaces
    ///
    /// @param[in] mesh (_Mesh_)
    ///         The mesh.
    void set_mesh(std::shared_ptr<const Mesh> mesh);

    /// Extract common mesh from form
    ///
    /// @return Mesh
    ///         Shared pointer to the mesh.
    std::shared_ptr<const Mesh> mesh() const;

    /// Return function space for given argument
    ///
    /// @param  i (std::size_t)
    ///         Index
    ///
    /// @return FunctionSpace
    ///         Function space shared pointer.
    std::shared_ptr<const FunctionSpace> function_space(std::size_t i) const;

    /// Return function spaces for arguments
    ///
    /// @return    std::vector<_FunctionSpace_>
    ///         Vector of function space shared pointers.
    std::vector<std::shared_ptr<const FunctionSpace>> function_spaces() const;

    /// Set coefficient with given number (shared pointer version)
    ///
    /// @param[in]  i (std::size_t)
    ///         The given number.
    /// @param[in]    coefficient (_GenericFunction_)
    ///         The coefficient.
    void set_coefficient(std::size_t i,
                         std::shared_ptr<const GenericFunction> coefficient);

    /// Set coefficient with given name (shared pointer version)
    ///
    /// @param[in]    name (std::string)
    ///         The name.
    /// @param[in]    coefficient (_GenericFunction_)
    ///         The coefficient.
    void set_coefficient(std::string name,
                         std::shared_ptr<const GenericFunction> coefficient);

    /// Set all coefficients in given map. All coefficients in the
    /// given map, which may contain only a subset of the coefficients
    /// of the form, will be set.
    ///
    /// @param[in]    coefficients (std::map<std::string, _GenericFunction_>)
    ///         The map of coefficients.
    void set_coefficients(std::map<std::string,
                          std::shared_ptr<const GenericFunction>> coefficients);

    /// Set some coefficients in given map. Each coefficient in the
    /// given map will be set, if the name of the coefficient matches
    /// the name of a coefficient in the form.
    ///
    /// This is useful when reusing the same coefficient map for
    /// several forms, or when some part of the form has been
    /// outcommented (for testing) in the UFL file, which means that
    /// the coefficient and attaching it to the form does not need to
    /// be outcommented in a C++ program using code from the generated
    /// UFL file.
    ///
    /// @param[in] coefficients (std::map<std::string, _GenericFunction_>)
    ///         The map of coefficients.
    void set_some_coefficients(std::map<std::string,
                               std::shared_ptr<const GenericFunction>> coefficients);

    /// Return coefficient with given number
    ///
    /// @param[in] i (std::size_t)
    ///         Index
    ///
    /// @return     _GenericFunction_
    ///         The coefficient.
    std::shared_ptr<const GenericFunction> coefficient(std::size_t i) const;

    /// Return coefficient with given name
    ///
    /// @param[in]    name (std::string)
    ///         The name.
    ///
    /// @return     _GenericFunction_
    ///         The coefficient.
    std::shared_ptr<const GenericFunction> coefficient(std::string name) const;

    /// Return all coefficients
    ///
    /// @return     std::vector<_GenericFunction_>
    ///         All coefficients.
    std::vector<std::shared_ptr<const GenericFunction>> coefficients() const;

    /// Return the number of the coefficient with this name
    ///
    /// @param[in]    name (std::string)
    ///         The name.
    ///
    /// @return     std::size_t
    ///         The number of the coefficient with the given name.
    virtual std::size_t coefficient_number(const std::string & name) const;

    /// Return the name of the coefficient with this number
    ///
    ///  @param[in]   i (std::size_t)
    ///         The number
    ///
    /// @return     std::string
    ///         The name of the coefficient with the given number.
    virtual std::string coefficient_name(std::size_t i) const;

    /// Return cell domains (zero pointer if no domains have been
    /// specified)
    ///
    /// @return     _MeshFunction_ <std::size_t>
    ///         The cell domains.
    std::shared_ptr<const MeshFunction<std::size_t>> cell_domains() const;

    /// Return exterior facet domains (zero pointer if no domains have
    /// been specified)
    ///
    /// @return     std::shared_ptr<_MeshFunction_ <std::size_t>>
    ///         The exterior facet domains.
    std::shared_ptr<const MeshFunction<std::size_t>>
      exterior_facet_domains() const;

    /// Return interior facet domains (zero pointer if no domains have
    /// been specified)
    ///
    /// @return     _MeshFunction_ <std::size_t>
    ///         The interior facet domains.
    std::shared_ptr<const MeshFunction<std::size_t>>
      interior_facet_domains() const;

    /// Return vertex domains (zero pointer if no domains have been
    /// specified)
    ///
    /// @return     _MeshFunction_ <std::size_t>
    ///         The vertex domains.
    std::shared_ptr<const MeshFunction<std::size_t>> vertex_domains() const;

    /// Set cell domains
    ///
    /// @param[in]    cell_domains (_MeshFunction_ <std::size_t>)
    ///         The cell domains.
    void set_cell_domains(std::shared_ptr<const MeshFunction<std::size_t>>
                          cell_domains);

    /// Set exterior facet domains
    ///
    ///  @param[in]   exterior_facet_domains (_MeshFunction_ <std::size_t>)
    ///         The exterior facet domains.
    void set_exterior_facet_domains(std::shared_ptr<const MeshFunction<std::size_t>> exterior_facet_domains);

    /// Set interior facet domains
    ///
    ///  @param[in]   interior_facet_domains (_MeshFunction_ <std::size_t>)
    ///         The interior facet domains.
    void set_interior_facet_domains(std::shared_ptr<const MeshFunction<std::size_t>> interior_facet_domains);

    /// Set vertex domains
    ///
    ///  @param[in]   vertex_domains (_MeshFunction_ <std::size_t>)
    ///         The vertex domains.
    void set_vertex_domains(std::shared_ptr<const MeshFunction<std::size_t>> vertex_domains);

    /// Return UFC form shared pointer
    ///
    /// @return     ufc::form
    ///         The UFC form.
    std::shared_ptr<const ufc::form> ufc_form() const;

    /// Check function spaces and coefficients
    void check() const;

    /// Domain markers for cells
    std::shared_ptr<const MeshFunction<std::size_t>> dx;
    /// Domain markers for exterior facets
    std::shared_ptr<const MeshFunction<std::size_t>> ds;
    /// Domain markers for interior facets
    std::shared_ptr<const MeshFunction<std::size_t>> dS;
    /// Domain markers for vertices
    std::shared_ptr<const MeshFunction<std::size_t>> dP;

  protected:

    // The UFC form
    std::shared_ptr<const ufc::form> _ufc_form;

    // Function spaces (one for each argument)
    std::vector<std::shared_ptr<const FunctionSpace>> _function_spaces;

    // Coefficients
    std::vector<std::shared_ptr<const GenericFunction>> _coefficients;

    // The mesh (needed for functionals when we don't have any spaces)
    std::shared_ptr<const Mesh> _mesh;

  private:

    const std::size_t _rank;

  };

}

#endif

// Copyright (C) 2007-2008 Anders Logg.
// Licensed under the GNU LGPL Version 2.1.
//
// Modified by Garth N. Wells, 2008.
// Modified by Martin Alnes, 2008.
//
// First added:  2007-04-02
// Last changed: 2008-12-04

#ifndef __FORM_H
#define __FORM_H

#include <vector>
#include <tr1/memory>
#include <dolfin/common/types.h>

// Forward declaration
namespace ufc
{
  class form;
}

namespace dolfin
{

  // Forward declarations
  class FunctionSpace;
  class Function;
  class Mesh;

  /// Base class for UFC code generated by FFC for DOLFIN with option -l

  class Form
  {
  public:

    /// Constructor
    Form();

    /// Create form of given rank with given number of coefficients
    Form(dolfin::uint rank, dolfin::uint num_coefficients);

    // FIXME: Pointers need to be const here to work with SWIG. Is there a fix for this?

    /// Create form from given Constructor used in the python interface 
    Form(const ufc::form& ufc_form,
         const std::vector<FunctionSpace*>& function_spaces, 
         const std::vector<Function*>& coefficients);
    
    /// Destructor
    virtual ~Form();
    
    /// Return rank of form (bilinear form = 2, linear form = 1, functional = 0, etc)
    uint rank() const;

    /// Return mesh
    const Mesh& mesh() const;

    /// Return function space for given argument
    const FunctionSpace& function_space(uint i) const;

    /// Return function spaces for arguments
    std::vector<const FunctionSpace*> function_spaces() const;

    /// Return function for given coefficient
    const Function& coefficient(uint i) const;

    /// Return coefficient functions
    std::vector<const Function*> coefficients() const;

    /// Return UFC form
    const ufc::form& ufc_form() const;

    /// Check function spaces and coefficients
    void check() const;

    /// Friends
    friend class Coefficient;
    friend class LinearPDE;
    friend class NonlinearPDE;

  protected:

    // Function spaces (one for each argument)
    std::vector<std::tr1::shared_ptr<const FunctionSpace> > _function_spaces;

    // Coefficients
    std::vector<std::tr1::shared_ptr<const Function> > _coefficients;

    // The UFC form
    std::tr1::shared_ptr<const ufc::form> _ufc_form;

  };

}

#endif

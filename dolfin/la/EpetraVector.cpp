// Copyright (C) 2008 Martin Sandve Alnes, Kent-Andre Mardal and Johannes Ring.
// Licensed under the GNU LGPL Version 2.1.
//
// Modified by Garth N. Wells, 2008-2010.
//
// First added:  2008-04-21
// Last changed: 2010-11-26

#ifdef HAS_TRILINOS

#include <cmath>
#include <cstring>
#include <numeric>
#include <utility>
#include <boost/scoped_ptr.hpp>

#include <Epetra_FEVector.h>
#include <Epetra_Export.h>
#include <Epetra_Import.h>
#include <Epetra_Map.h>
#include <Epetra_MultiVector.h>
#include <Epetra_MpiComm.h>
#include <Epetra_SerialComm.h>
#include <Epetra_Vector.h>

#include <dolfin/common/Array.h>
#include <dolfin/common/Set.h>
#include <dolfin/main/MPI.h>
#include <dolfin/math/dolfin_math.h>
#include <dolfin/log/dolfin_log.h>
#include "uBLASVector.h"
#include "PETScVector.h"
#include "EpetraFactory.h"
#include "EpetraVector.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
EpetraVector::EpetraVector(std::string type) : type(type)
{
  // Do nothing
}
//-----------------------------------------------------------------------------
EpetraVector::EpetraVector(uint N, std::string type) : type(type)
{
  // Create Epetra vector
  resize(N);
}
//-----------------------------------------------------------------------------
EpetraVector::EpetraVector(boost::shared_ptr<Epetra_FEVector> x) : x(x)
{
  // Do nothing
}
//-----------------------------------------------------------------------------
EpetraVector::EpetraVector(const Epetra_Map& map)
{
  x.reset(new Epetra_FEVector(map));
}
//-----------------------------------------------------------------------------
EpetraVector::EpetraVector(const EpetraVector& v)
{
  *this = v;
}
//-----------------------------------------------------------------------------
EpetraVector::~EpetraVector()
{
  // Do nothing
}
//-----------------------------------------------------------------------------
void EpetraVector::resize(uint N)
{
  if (x && this->size() == N)
    return;

  // Get local range
  const std::pair<uint, uint> range = MPI::local_range(N);
  const uint n = range.second - range.first;

  // Resize vector
  std::vector<uint> ghost_indices;
  resize(N, n, ghost_indices);
}
//-----------------------------------------------------------------------------
void EpetraVector::resize(uint N, uint n, const std::vector<uint>& ghost_indices)
{
  if (x && !x.unique())
      error("Cannot resize EpetraVector. More than one object points to the underlying Epetra object.");

  // Create ghost data structures
  ghost_global_to_local.clear();

  // Pointer to Epetra map
  boost::scoped_ptr<Epetra_Map> epetra_map;

  // Epetra factory and serial communicator
  const EpetraFactory& f = EpetraFactory::instance();
  Epetra_SerialComm serial_comm = f.get_serial_comm();

  // Create vector
  if (N == n || type == "local")
  {
    if (ghost_indices.size() != 0)
      error("Serial EpetraVectors do not suppprt ghost points.");

    // Create map
    epetra_map.reset(new Epetra_Map(N, N, 0, serial_comm));
  }
  else
  {
    // Create map
    Epetra_MpiComm mpi_comm = f.get_mpi_comm();
    epetra_map.reset(new Epetra_Map(-1, n, 0, mpi_comm));

    assert(epetra_map->LinearMap());

    // Build global-to-local map for ghost indices
    for (uint i = 0; i < ghost_indices.size(); ++i)
      ghost_global_to_local.insert(std::pair<uint, uint>(ghost_indices[i], i));
  }

  // Create vector
  x.reset(new Epetra_FEVector(*epetra_map));

  // Create local ghost vector
  const int num_ghost_entries = ghost_indices.size();
  const int* ghost_entries = reinterpret_cast<const int*>(&ghost_indices[0]);
  Epetra_Map ghost_map(num_ghost_entries, num_ghost_entries,
                       ghost_entries, 0, serial_comm);
  x_ghosted.reset(new Epetra_MultiVector(ghost_map, 1));
}
//-----------------------------------------------------------------------------
EpetraVector* EpetraVector::copy() const
{
  assert(x);
  return new EpetraVector(*this);
}
//-----------------------------------------------------------------------------
dolfin::uint EpetraVector::size() const
{
  return x ? x->GlobalLength(): 0;
}
//-----------------------------------------------------------------------------
dolfin::uint EpetraVector::local_size() const
{
  return x ? x->MyLength(): 0;
}
//-----------------------------------------------------------------------------
std::pair<dolfin::uint, dolfin::uint> EpetraVector::local_range() const
{
  assert(x);

  if (x->Comm().NumProc() == 1)
    return std::make_pair<uint, uint>(0, size());
  else
  {
    assert(x->Map().LinearMap());
    const Epetra_BlockMap& map = x->Map();
    return std::make_pair<uint, uint>(map.MinMyGID(), map.MaxMyGID() + 1);
  }
}
//-----------------------------------------------------------------------------
void EpetraVector::zero()
{
  assert(x);
  const int err = x->PutScalar(0.0);
  if (err != 0)
    error("EpetraVector::zero: Did not manage to perform Epetra_Vector::PutScalar.");
}
//-----------------------------------------------------------------------------
void EpetraVector::apply(std::string mode)
{
  assert(x);
  int err = 0;
  if (mode == "add")
    err = x->GlobalAssemble(Add);
  else if (mode == "insert")
    err = x->GlobalAssemble(Insert);
  else
    error("Unknown apply mode in EpetraVector::apply");

  if (err != 0)
    error("EpetraVector::apply: Did not manage to perform Epetra_Vector::GlobalAssemble.");
}
//-----------------------------------------------------------------------------
std::string EpetraVector::str(bool verbose) const
{
  assert(x);

  std::stringstream s;
  if (verbose)
  {
    warning("Verbose output for EpetraVector not implemented, calling Epetra Print directly.");
    x->Print(std::cout);
  }
  else
    s << "<EpetraVector of size " << size() << ">";

  return s.str();
}
//-----------------------------------------------------------------------------
void EpetraVector::get_local(Array<double>& values) const
{
  assert(x);
  const uint local_size = x->MyLength();
  if (values.size() != local_size)
    error("EpetraVector::get_local: length of values array is not equal to local vector size.");

  const int err = x->ExtractCopy(values.data().get(), 0);
  if (err!= 0)
    error("EpetraVector::get: Did not manage to perform Epetra_Vector::ExtractCopy.");
}
//-----------------------------------------------------------------------------
void EpetraVector::set_local(const Array<double>& values)
{
  assert(x);
  const uint local_size = x->MyLength();
  if (values.size() != local_size)
    error("EpetraVector::set_local: length of values array is not equal to local vector size.");

  for (uint i = 0; i < local_size; ++i)
    (*x)[0][i] = values[i];
}
//-----------------------------------------------------------------------------
void EpetraVector::add_local(const Array<double>& values)
{
  assert(x);
  const uint local_size = x->MyLength();
  if (values.size() != local_size)
    error("EpetraVector::add_local: length of values array is not equal to local vector size.");

  for (uint i = 0; i < local_size; ++i)
    (*x)[0][i] += values[i];
}
//-----------------------------------------------------------------------------
void EpetraVector::get(double* block, uint m, const uint* rows) const
{
  // If vector is local this function will call get_local. For distributed
  // vectors, perform first a gather into a local vector

  if (local_range().first == 0 && local_range().second == size())
    get_local(block, m, rows);
  else
  {
    // Create local vector
    EpetraVector y("local");

    // Wrap row array
    const Array<uint> indices(m, const_cast<uint*>(rows));

    // Gather values into local vector y
    gather(y, indices);
    assert(y.size() == m);

    // Get entries of y
    const boost::shared_ptr<Epetra_FEVector> _y = y.vec();
    for (uint i = 0; i < m; ++i)
      block[i] = (*_y)[0][i];
  }
}
//-----------------------------------------------------------------------------
void EpetraVector::set(const double* block, uint m, const uint* rows)
{
  assert(x);
  const int err = x->ReplaceGlobalValues(m, reinterpret_cast<const int*>(rows),
                                         block, 0);

  if (err != 0)
    error("EpetraVector::set: Did not manage to perform Epetra_Vector::ReplaceGlobalValues.");
}
//-----------------------------------------------------------------------------
void EpetraVector::add(const double* block, uint m, const uint* rows)
{
  assert(x);
  int err = x->SumIntoGlobalValues(m, reinterpret_cast<const int*>(rows),
                                   block, 0);

  if (err != 0)
    error("EpetraVector::add: Did not manage to perform Epetra_Vector::SumIntoGlobalValues.");
}
//-----------------------------------------------------------------------------
void EpetraVector::get_local(double* block, uint m, const uint* rows) const
{
  assert(x);
  const Epetra_BlockMap& map = x->Map();
  assert(x->Map().LinearMap());
  const uint n0 = map.MinMyGID();

  // Get values
  if (ghost_global_to_local.size() == 0)
  {
    for (uint i = 0; i < m; ++i)
      block[i] = (*x)[0][rows[i] - n0];
  }
  else
  {
    assert(x_ghosted);
    const uint n1 = map.MaxMyGID();
    const Epetra_BlockMap& ghost_map = x_ghosted->Map();
    for (uint i = 0; i < m; ++i)
    {
      if (rows[i] >= n0 && rows[i] <= n1)
        block[i] = (*x)[0][rows[i] - n0];
      else
      {
        // FIXME: Check if look-up in std::map is faster than Epetra_BlockMap::LID
        const int local_index = ghost_map.LID(rows[i]);
        assert(local_index != -1);
        block[i] = (*x_ghosted)[0][local_index];
      }
    }
  }
}
//-----------------------------------------------------------------------------
void EpetraVector::gather(GenericVector& y,
                          const Array<dolfin::uint>& indices) const
{
  // FIXME: This can be done better. Problem is that the GenericVector interface
  //        is PETSc-centric for the parallel case. It should be improved.

  assert(x);

  const EpetraFactory& f = EpetraFactory::instance();
  Epetra_SerialComm serial_comm = f.get_serial_comm();

  // Down cast to a EpetraVector and resize
  EpetraVector& _y = y.down_cast<EpetraVector>();

  // Check that y is a local vector (check communicator)

  // Create map
  std::vector<int> _indices(indices.size());
  for (uint i = 0; i < indices.size(); ++i)
    _indices[i] = indices[i];

  Epetra_Map target_map(indices.size(), indices.size(), &_indices[0], 0, serial_comm);

  // FIXME: Check that the data belonging to y is not shared
  _y.reset(target_map);
  Epetra_Import importer(_y.vec()->Map(), x->Map());
  _y.vec()->Import(*x, importer, Insert);
}
//-----------------------------------------------------------------------------
void EpetraVector::reset(const Epetra_Map& map)
{
  x.reset(new Epetra_FEVector(map));
}
//-----------------------------------------------------------------------------
boost::shared_ptr<Epetra_FEVector> EpetraVector::vec() const
{
  return x;
}
//-----------------------------------------------------------------------------
double EpetraVector::inner(const GenericVector& y) const
{
  assert(x);

  const EpetraVector& v = y.down_cast<EpetraVector>();
  if (!v.x)
    error("Given vector is not initialized.");

  double a;
  const int err = x->Dot(*(v.x), &a);
  if (err!= 0)
    error("EpetraVector::inner: Did not manage to perform Epetra_Vector::Dot.");

  return a;
}
//-----------------------------------------------------------------------------
void EpetraVector::axpy(double a, const GenericVector& y)
{
  assert(x);

  const EpetraVector& _y = y.down_cast<EpetraVector>();
  if (!_y.x)
    error("Given vector is not initialized.");

  if (size() != _y.size())
    error("The vectors must be of the same size.");

  const int err = x->Update(a, *(_y.vec()), 1.0);
  if (err != 0)
    error("EpetraVector::axpy: Did not manage to perform Epetra_Vector::Update.");
}
//-----------------------------------------------------------------------------
LinearAlgebraFactory& EpetraVector::factory() const
{
  return EpetraFactory::instance();
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator= (const GenericVector& v)
{
  *this = v.down_cast<EpetraVector>();
  return *this;
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator= (double a)
{
  assert(x);
  x->PutScalar(a);
  return *this;
}
//-----------------------------------------------------------------------------
void EpetraVector::update_ghost_values()
{
  assert(x);
  assert(x_ghosted);
  assert(x_ghosted->MyLength() == (int) ghost_global_to_local.size());

  assert(x->Map().LinearMap());

  // Create importer
  Epetra_Import importer(x_ghosted->Map(), x->Map());

  // Import into ghost vector
  x_ghosted->Import(*x, importer, Insert);
  assert(x->Map().LinearMap());
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator= (const EpetraVector& v)
{
  // FIXME: Epetra assignment operator leads to an errror. Must vectors have
  //        the same size for assigenment to work?

  assert(v.x);
  if (this != &v)
    x.reset(new Epetra_FEVector(*(v.vec())));
  return *this;
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator+= (const GenericVector& y)
{
  assert(x);
  axpy(1.0, y);
  return *this;
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator-= (const GenericVector& y)
{
  assert(x);
  axpy(-1.0, y);
  return *this;
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator*= (double a)
{
  assert(x);
  const int err = x->Scale(a);
  if (err!= 0)
    error("EpetraVector::operator*=: Did not manage to perform Epetra_Vector::Scale.");
  return *this;
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator*= (const GenericVector& y)
{
  assert(x);
  const EpetraVector& v = y.down_cast<EpetraVector>();

  if (!v.x)
    error("Given vector is not initialized.");

  if (size() != v.size())
    error("The vectors must be of the same size.");

  const int err = x->Multiply(1.0, *x, *v.x, 0.0);
  if (err!= 0)
    error("EpetraVector::operator*=: Did not manage to perform Epetra_Vector::Multiply.");
  return *this;
}
//-----------------------------------------------------------------------------
const EpetraVector& EpetraVector::operator/=(double a)
{
  *this *= 1.0/a;
  return *this;
}
//-----------------------------------------------------------------------------
double EpetraVector::norm(std::string norm_type) const
{
  assert(x);

  double value = 0.0;
  int err = 0;
  if (norm_type == "l1")
    err = x->Norm1(&value);
  else if (norm_type == "l2")
    err = x->Norm2(&value);
  else
    err = x->NormInf(&value);

  if (err != 0)
    error("EpetraVector::norm: Did not manage to compute the norm.");
  return value;
}
//-----------------------------------------------------------------------------
double EpetraVector::min() const
{
  assert(x);
  double value = 0.0;
  const int err = x->MinValue(&value);
  if (err!= 0)
    error("EpetraVector::min: Did not manage to perform Epetra_Vector::MinValue.");
  return value;
}
//-----------------------------------------------------------------------------
double EpetraVector::max() const
{
  assert(x);
  double value = 0.0;
  const int err = x->MaxValue(&value);
  if (err != 0)
    error("EpetraVector::min: Did not manage to perform Epetra_Vector::MinValue.");
  return value;
}
//-----------------------------------------------------------------------------
double EpetraVector::sum() const
{
  assert(x);
  const uint local_size = x->MyLength();

  // Get local values
  Array<double> x_local(local_size);
  get_local(x_local);

  // Compute local sum
  //double local_sum = std::accumulate(&x_local[0], &x_local[local_size], 0.0);
  double local_sum = 0.0;
  for (uint i = 0; i < local_size; ++i)
    local_sum += x_local[i];

  // Compute global sum
  double global_sum = 0.0;
  x->Comm().SumAll(&local_sum, &global_sum, 1);

  return global_sum;
}
//-----------------------------------------------------------------------------
double EpetraVector::sum(const Array<uint>& rows) const
{
  assert(x);
  const uint n0 = local_range().first;
  const uint n1 = local_range().second;

  // Build sets of local and nonlocal entries
  Set<uint> local_rows;
  Set<uint> nonlocal_rows;
  for (uint i = 0; i < rows.size(); ++i)
  {
    if (rows[i] >= n0 && rows[i] < n1)
      local_rows.insert(rows[i]);
    else
      nonlocal_rows.insert(rows[i]);
  }

  // Send nonlocal rows indices to other processes
  const uint num_processes  = MPI::num_processes();
  const uint process_number = MPI::process_number();
  for (uint i = 1; i < num_processes; ++i)
  {
    // Receive data from process p - i (i steps to the left), send data to
    // process p + i (i steps to the right)
    const uint source = (process_number - i + num_processes) % num_processes;
    const uint dest   = (process_number + i) % num_processes;

    // Size of send and receive data
    uint send_buffer_size = nonlocal_rows.size();
    uint recv_buffer_size = 0;
    MPI::send_recv(&send_buffer_size, 1, dest, &recv_buffer_size, 1, source);

    // Send and receive data
    std::vector<uint> received_nonlocal_rows(recv_buffer_size);
    MPI::send_recv(&(nonlocal_rows.set())[0], send_buffer_size, dest,
                   &received_nonlocal_rows[0], recv_buffer_size, source);

    // Add rows which reside on this process
    for (uint j = 0; j < received_nonlocal_rows.size(); ++j)
    {
      if (received_nonlocal_rows[j] >= n0 && received_nonlocal_rows[j] < n1)
        local_rows.insert(received_nonlocal_rows[j]);
    }
  }

  // Compute local sum
  double local_sum = 0.0;
  for (uint i = 0; i < local_rows.size(); ++i)
    local_sum += (*x)[0][local_rows[i] - n0];

  // Compute global sum
  double global_sum = 0.0;
  x->Comm().SumAll(&local_sum, &global_sum, 1);

  return global_sum;
}
//-----------------------------------------------------------------------------
#endif

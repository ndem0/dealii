/* $Id$ */
/* Copyright W. Bangerth, University of Heidelberg, 1998 */


#include <fe/fe.h>
#include <base/quadrature.h>
#include <grid/tria.h>
#include <grid/tria_iterator.h>
#include <grid/dof_accessor.h>
#include <grid/tria_boundary.h>


#define UNDEF 0


/*------------------------------- FiniteElementData ----------------------*/

template <int dim>
FiniteElementData<dim>::FiniteElementData ()
{
  Assert (false, ExcNotImplemented());
};


template <int dim>
FiniteElementData<dim>::FiniteElementData (const unsigned int dofs_per_vertex,
					   const unsigned int dofs_per_line,
					   const unsigned int dofs_per_quad,
					   const unsigned int dofs_per_hex,
					   const unsigned int n_transform_functions,
					   const unsigned int n_components) :
		dofs_per_vertex(dofs_per_vertex),
		dofs_per_line(dofs_per_line),
		dofs_per_quad(dofs_per_quad),
		dofs_per_hex(dofs_per_hex),
		dofs_per_face(GeometryInfo<dim>::vertices_per_face * dofs_per_vertex+
			      GeometryInfo<dim>::lines_per_face * dofs_per_line +
			      dofs_per_quad),
		first_line_index(GeometryInfo<dim>::vertices_per_cell
				 * dofs_per_vertex),
		first_quad_index(first_line_index+
				 GeometryInfo<dim>::lines_per_cell
				 * dofs_per_line),
		first_hex_index(first_quad_index+
				GeometryInfo<dim>::faces_per_cell
				* dofs_per_quad),
		first_face_line_index(GeometryInfo<dim-1>::vertices_per_cell
				      * dofs_per_vertex),
		first_face_quad_index(first_line_index+
				      GeometryInfo<dim-1>::lines_per_cell
				      * dofs_per_line),
		total_dofs (first_hex_index+dofs_per_hex),
		n_transform_functions (n_transform_functions),
		n_components(n_components)
{
  Assert(dim==3, ExcDimensionMismatch(3,dim));
};



template <int dim>
FiniteElementData<dim>::FiniteElementData (const unsigned int dofs_per_vertex,
					   const unsigned int dofs_per_line,
					   const unsigned int dofs_per_quad,
					   const unsigned int n_transform_functions,
					   const unsigned int n_components) :
		dofs_per_vertex(dofs_per_vertex),
		dofs_per_line(dofs_per_line),
		dofs_per_quad(dofs_per_quad),
		dofs_per_hex(0),
		dofs_per_face(GeometryInfo<dim>::vertices_per_face * dofs_per_vertex+
			      dofs_per_line),
		first_line_index(GeometryInfo<dim>::vertices_per_cell * dofs_per_vertex),
		first_quad_index(first_line_index+
				 GeometryInfo<dim>::lines_per_cell * dofs_per_line),
		first_hex_index(first_quad_index+
				GeometryInfo<dim>::quads_per_cell*dofs_per_quad),
		first_face_line_index(GeometryInfo<dim-1>::vertices_per_cell
				      * dofs_per_vertex),
		first_face_quad_index(first_line_index+
				      GeometryInfo<dim-1>::lines_per_cell
				      * dofs_per_line),
		total_dofs (first_quad_index+dofs_per_quad),
		n_transform_functions (n_transform_functions),
		n_components(n_components)
{
  Assert(dim==2, ExcDimensionMismatch(2,dim));
};



template <int dim>
FiniteElementData<dim>::FiniteElementData (const unsigned int dofs_per_vertex,
					   const unsigned int dofs_per_line,
					   const unsigned int n_transform_functions,
					   const unsigned int n_components) :
		dofs_per_vertex(dofs_per_vertex),
		dofs_per_line(dofs_per_line),
		dofs_per_quad(0),
		dofs_per_hex(0),
		dofs_per_face(dofs_per_vertex),
		first_line_index(GeometryInfo<dim>::vertices_per_cell * dofs_per_vertex),
		first_quad_index(first_line_index+
				 GeometryInfo<dim>::lines_per_cell * dofs_per_line),
		first_hex_index(first_quad_index+
				GeometryInfo<dim>::quads_per_cell*dofs_per_quad),
		first_face_line_index(GeometryInfo<dim-1>::vertices_per_cell
				      * dofs_per_vertex),
		first_face_quad_index(first_line_index+
				      GeometryInfo<dim-1>::lines_per_cell
				      * dofs_per_line),
		total_dofs (first_line_index+dofs_per_line),
		n_transform_functions (n_transform_functions),
		n_components(n_components)
{
  Assert(dim==1, ExcDimensionMismatch(1,dim));
};


template<int dim>
bool FiniteElementData<dim>::operator== (const FiniteElementData<dim> &f) const
{
  return ((dofs_per_vertex == f.dofs_per_vertex) &&
	  (dofs_per_line == f.dofs_per_line) &&
	  (dofs_per_quad == f.dofs_per_quad) &&
	  (dofs_per_hex == f.dofs_per_hex) &&
	  (n_transform_functions == f.n_transform_functions) &&
	  (n_components == f.n_components));
};



/*------------------------------- FiniteElementBase ----------------------*/



template <int dim>
FiniteElementBase<dim>::FiniteElementBase (const FiniteElementData<dim> &fe_data) :
		FiniteElementData<dim> (fe_data),
		system_to_component_table(total_dofs),
		face_system_to_component_table(dofs_per_face),
		component_to_system_table(n_components, vector<unsigned>(total_dofs)),
		face_component_to_system_table(n_components, vector<unsigned>(dofs_per_face))
{
  for (unsigned int i=0; i<GeometryInfo<dim>::children_per_cell; ++i) 
    {
      restriction[i].reinit (total_dofs, total_dofs);
      prolongation[i].reinit (total_dofs, total_dofs);
    };

  switch (dim)
    {
      case 1:
	    interface_constraints.reinit (1,1);
	    interface_constraints(0,0)=1.;
	    break;
	    
      case 2:
	    interface_constraints.reinit (dofs_per_vertex+2*dofs_per_line,
					  dofs_per_face);
	    break;

      case 3:
	    interface_constraints.reinit (5*dofs_per_vertex +
					  12*dofs_per_line  +
					  4*dofs_per_quad,
					  dofs_per_face);
	    break;

      default:
	    Assert (false, ExcNotImplemented());
    };
	    
  				   // this is the default way, if there is only
				   // one component; if there are several, then
				   // the constructor of the derived class needs
				   // to fill these arrays
  for (unsigned int j=0 ; j<total_dofs ; ++j)
    {
      system_to_component_table[j] = pair<unsigned,unsigned>(0,j);
      component_to_system_table[0][j] = j;
    }
  for (unsigned int j=0 ; j<dofs_per_face ; ++j)
    {
      face_system_to_component_table[j] = pair<unsigned,unsigned>(0,j);
      face_component_to_system_table[0][j] = j;
    }
};



template <int dim>
const FullMatrix<double> &
FiniteElementBase<dim>::restrict (const unsigned int child) const {
  Assert (child<GeometryInfo<dim>::children_per_cell, ExcInvalidIndex(child));
  return restriction[child];
};



template <int dim>
const FullMatrix<double> &
FiniteElementBase<dim>::prolongate (const unsigned int child) const {
  Assert (child<GeometryInfo<dim>::children_per_cell, ExcInvalidIndex(child));
  return prolongation[child];
};



template <int dim>
const FullMatrix<double> &
FiniteElementBase<dim>::constraints () const {
  if (dim==1)
    Assert ((interface_constraints.m()==1) && (interface_constraints.n()==1),
	    ExcWrongInterfaceMatrixSize(interface_constraints.m(),
					interface_constraints.n()));
  
  return interface_constraints;
};



template <int dim>
bool FiniteElementBase<dim>::operator == (const FiniteElementBase<dim> &f) const {
  return ((static_cast<FiniteElementData<dim> >(*this) ==
	   static_cast<FiniteElementData<dim> >(f)) &&
	  (interface_constraints == f.interface_constraints));
};





/*------------------------------- FiniteElement ----------------------*/


template <int dim>
FiniteElement<dim>::FiniteElement (const FiniteElementData<dim> &fe_data) :
		FiniteElementBase<dim> (fe_data) {};

#if deal_II_dimension == 1

template <>
void FiniteElement<1>::get_support_points (const DoFHandler<1>::cell_iterator &cell,
					   vector<Point<1> > &support_points) const;


template <>
void FiniteElement<1>::fill_fe_values (const DoFHandler<1>::cell_iterator &cell,
				       const vector<Point<1> > &unit_points,
				       vector<Tensor<2,1> >    &jacobians,
				       const bool            compute_jacobians,
				       vector<Tensor<3,1> > &jacobians_grad,
				       const bool            compute_jacobians_grad,
				       vector<Point<1> >    &support_points,
				       const bool            compute_support_points,
				       vector<Point<1> >    &q_points,
				       const bool            compute_q_points,
				       const FullMatrix<double>       &,
				       const vector<vector<Tensor<1,1> > > &) const {
  Assert ((!compute_jacobians) || (jacobians.size() == unit_points.size()),
	  ExcWrongFieldDimension(jacobians.size(), unit_points.size()));
  Assert ((!compute_jacobians_grad) || (jacobians_grad.size() == unit_points.size()),
	  ExcWrongFieldDimension(jacobians_grad.size(), unit_points.size()));
  Assert ((!compute_q_points) || (q_points.size() == unit_points.size()),
	  ExcWrongFieldDimension(q_points.size(), unit_points.size()));
  Assert ((!compute_support_points) || (support_points.size() == total_dofs),
	  ExcWrongFieldDimension(support_points.size(), total_dofs));


				   // local mesh width
  const double h=(cell->vertex(1)(0) - cell->vertex(0)(0));

  for (unsigned int i=0; i<q_points.size(); ++i) 
    {
      if (compute_jacobians)
	jacobians[i][0][0] = 1./h;

				       // gradient of jacobian is zero
      if (compute_jacobians_grad)
	jacobians_grad[i] = Tensor<3,1>();
      
      if (compute_q_points)
					 // assume a linear mapping from unit
					 // to real space. overload this
					 // function if you don't like that
	q_points[i] = cell->vertex(0) + h*unit_points[i];
    };

				   // compute support points. The first ones
				   // belong to vertex one, the second ones
				   // to vertex two, all following are
				   // equally spaced along the line
  if (compute_support_points)
    get_support_points (cell, support_points);
};



template <>
void FiniteElement<1>::fill_fe_face_values (const DoFHandler<1>::cell_iterator &,
					    const unsigned int       ,
					    const vector<Point<0> > &,
					    const vector<Point<1> > &,
					    vector<Tensor<2,1> >    &,
					    const bool               ,
					    vector<Tensor<3,1> >    &,
					    const bool               ,
					    vector<Point<1> >       &,
					    const bool               ,
					    vector<Point<1> >       &,
					    const bool               ,
					    vector<double>          &,
					    const bool              ,
					    vector<Point<1> >       &,
					    const bool,
					    const FullMatrix<double>          &,
					    const vector<vector<Tensor<1,1> > > &) const {
  Assert (false, ExcNotImplemented());
};


template <>
void FiniteElement<1>::fill_fe_subface_values (const DoFHandler<1>::cell_iterator &,
					       const unsigned int       ,
					       const unsigned int       ,
					       const vector<Point<0> > &,
					       const vector<Point<1> > &,
					       vector<Tensor<2,1> >    &,
					       const bool               ,
					       vector<Tensor<3,1> >    &,
					       const bool               ,
					       vector<Point<1> >       &,
					       const bool               ,
					       vector<double>          &,
					       const bool               ,
					       vector<Point<1> >       &,
					       const bool,
					       const FullMatrix<double>          &,
					       const vector<vector<Tensor<1,1> > > &) const {
  Assert (false, ExcNotImplemented());
};



template <>
void FiniteElement<1>::get_unit_support_points (vector<Point<1> > &support_points) const {
  Assert (support_points.size() == total_dofs,
	  ExcWrongFieldDimension(support_points.size(), total_dofs));
				   // compute support points. The first ones
				   // belong to vertex one, the second ones
				   // to vertex two, all following are
				   // equally spaced along the line
  unsigned int next = 0;
				   // first the dofs in the vertices
  for (unsigned int i=0; i<dofs_per_vertex; ++i)
    support_points[next++] = Point<1>(0.0);
  for (unsigned int i=0; i<dofs_per_vertex; ++i)
    support_points[next++] = Point<1>(1.0);
  
				   // now dofs on line
  for (unsigned int i=0; i<dofs_per_line; ++i) 
    support_points[next++] = Point<1>((i+1.0)/(dofs_per_line+1.0));
};



template <>
void FiniteElement<1>::get_support_points (const DoFHandler<1>::cell_iterator &cell,
					   vector<Point<1> > &support_points) const {
  Assert (support_points.size() == total_dofs,
	  ExcWrongFieldDimension(support_points.size(), total_dofs));
				   // compute support points. The first ones
				   // belong to vertex one, the second ones
				   // to vertex two, all following are
				   // equally spaced along the line
  unsigned int next = 0;
				   // local mesh width
  const double h=(cell->vertex(1)(0) - cell->vertex(0)(0));
				   // first the dofs in the vertices
  for (unsigned int vertex=0; vertex<2; vertex++) 
    for (unsigned int i=0; i<dofs_per_vertex; ++i)
      support_points[next++] = cell->vertex(vertex);
  
				   // now dofs on line
  for (unsigned int i=0; i<dofs_per_line; ++i) 
    support_points[next++] = cell->vertex(0) +
			    Point<1>((i+1.0)/(dofs_per_line+1.0)*h);
};

#endif



template <int dim>
void FiniteElement<dim>::fill_fe_values (const DoFHandler<dim>::cell_iterator &,
					 const vector<Point<dim> > &,
					 vector<Tensor<2,dim> > &,
					 const bool,
					 vector<Tensor<3,dim> > &,
					 const bool,
					 vector<Point<dim> > &,
					 const bool,
					 vector<Point<dim> > &,
					 const bool,
					 const FullMatrix<double>      &,
					 const vector<vector<Tensor<1,dim> > > &) const {
  Assert (false, ExcPureFunctionCalled());
};



template <int dim>
void FiniteElement<dim>::fill_fe_face_values (const DoFHandler<dim>::cell_iterator &cell,
					      const unsigned int           face_no,
					      const vector<Point<dim-1> > &unit_points,
					      const vector<Point<dim> > &global_unit_points,
					      vector<Tensor<2,dim> >    &jacobians,
					      const bool           compute_jacobians,
					      vector<Tensor<3,dim> >    &jacobians_grad,
					      const bool           compute_jacobians_grad,
					      vector<Point<dim> > &support_points,
					      const bool           compute_support_points,
					      vector<Point<dim> > &q_points,
					      const bool           compute_q_points,
					      vector<double>      &face_jacobi_determinants,
					      const bool           compute_face_jacobians,
					      vector<Point<dim> > &normal_vectors,
					      const bool           compute_normal_vectors,
					      const FullMatrix<double>      &shape_values_transform,
					      const vector<vector<Tensor<1,dim> > > &shape_gradients_transform) const {
  Assert (jacobians.size() == unit_points.size(),
	  ExcWrongFieldDimension(jacobians.size(), unit_points.size()));
  Assert (q_points.size() == unit_points.size(),
	  ExcWrongFieldDimension(q_points.size(), unit_points.size()));
  Assert (global_unit_points.size() == unit_points.size(),
	  ExcWrongFieldDimension(global_unit_points.size(), unit_points.size()));
  Assert (support_points.size() == dofs_per_face,
	  ExcWrongFieldDimension(support_points.size(), dofs_per_face));
  
  vector<Point<dim> > dummy(total_dofs);
  fill_fe_values (cell, global_unit_points,
		  jacobians, compute_jacobians,
		  jacobians_grad, compute_jacobians_grad,
		  dummy, false,
		  q_points, compute_q_points,
		  shape_values_transform, shape_gradients_transform);
  
  if (compute_support_points)
    get_face_support_points (cell->face(face_no), support_points);

  if (compute_face_jacobians)
    get_face_jacobians (cell->face(face_no),
			unit_points,
			face_jacobi_determinants);

  if (compute_normal_vectors)
    get_normal_vectors (cell, face_no, unit_points, normal_vectors);
};




template <int dim>
void FiniteElement<dim>::fill_fe_subface_values (const DoFHandler<dim>::cell_iterator &cell,
						 const unsigned int           face_no,
						 const unsigned int           subface_no,
						 const vector<Point<dim-1> > &unit_points,
						 const vector<Point<dim> > &global_unit_points,
						 vector<Tensor<2,dim> >    &jacobians,
						 const bool           compute_jacobians,
						 vector<Tensor<3,dim> >    &jacobians_grad,
						 const bool           compute_jacobians_grad,
						 vector<Point<dim> > &q_points,
						 const bool           compute_q_points,
						 vector<double>      &face_jacobi_determinants,
						 const bool           compute_face_jacobians,
						 vector<Point<dim> > &normal_vectors,
						 const bool           compute_normal_vectors,
						 const FullMatrix<double>      &shape_values_transform,
						 const vector<vector<Tensor<1,dim> > > &shape_gradients_transform) const {
  Assert (jacobians.size() == unit_points.size(),
	  ExcWrongFieldDimension(jacobians.size(), unit_points.size()));
  Assert (q_points.size() == unit_points.size(),
	  ExcWrongFieldDimension(q_points.size(), unit_points.size()));
  Assert (global_unit_points.size() == unit_points.size(),
	  ExcWrongFieldDimension(global_unit_points.size(), unit_points.size()));

  vector<Point<dim> > dummy(total_dofs);
  fill_fe_values (cell, global_unit_points,
		  jacobians, compute_jacobians,
		  jacobians_grad, compute_jacobians_grad,
		  dummy, false,
		  q_points, compute_q_points,
		  shape_values_transform, shape_gradients_transform);
  
  if (compute_face_jacobians)
    get_subface_jacobians (cell->face(face_no), subface_no,
			   unit_points, face_jacobi_determinants);

  if (compute_normal_vectors)
    get_normal_vectors (cell, face_no, subface_no,
			unit_points, normal_vectors);
};



template <int dim>
void
FiniteElement<dim>::get_unit_support_points (vector<Point<dim> > &) const
{
  Assert (false, ExcPureFunctionCalled());
};



template <int dim>
void
FiniteElement<dim>::get_support_points (const DoFHandler<dim>::cell_iterator &,
					    vector<Point<dim> > &) const
{
  Assert (false, ExcPureFunctionCalled());
};


template <int dim>
unsigned int
FiniteElement<dim>::n_base_elements() const
{
  return 1;
}

template <int dim>
const FiniteElement<dim>&
FiniteElement<dim>::base_element(unsigned index) const
{
  Assert (index==0, ExcIndexRange(index,0,1));
  return *this;
}

/*------------------------------- Explicit Instantiations -------------*/

template class FiniteElementData<deal_II_dimension>;
template class FiniteElementBase<deal_II_dimension>;
template class FiniteElement<deal_II_dimension>;




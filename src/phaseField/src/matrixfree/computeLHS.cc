//vmult() and getLHS() method for MatrixFreePDE class

#include "../../include/matrixFreePDE.h"

//vmult operation for LHS
template <int dim, int degree>
void MatrixFreePDE<dim,degree>::vmult (vectorType &dst, const vectorType &src) const{
  //log time
  computing_timer.enter_section("matrixFreePDE: computeLHS");

  //call cell_loop
  dst=0.0;
  if (!generatingInitialGuess){
      matrixFreeObject.cell_loop (&MatrixFreePDE<dim,degree>::getLHS, this, dst, src);
  }
  else {
      matrixFreeObject.cell_loop (&MatrixFreePDE<dim,degree>::getLaplaceLHS, this, dst, src);
  }

  //Account for Dirichlet BC's (essentially copy dirichlet DOF values present in src to dst, although it is unclear why the constraints can't just be distributed here)
  for (std::map<types::global_dof_index, double>::const_iterator it=valuesDirichletSet[currentFieldIndex]->begin(); it!=valuesDirichletSet[currentFieldIndex]->end(); ++it){
    if (dst.in_local_range(it->first)){
      dst(it->first) = src(it->first); //*jacobianDiagonal(it->first);
    }
  }

  //end log
  computing_timer.exit_section("matrixFreePDE: computeLHS");

}

template <int dim, int degree>
void  MatrixFreePDE<dim,degree>::getLHS(const MatrixFree<dim,double> &data,
				 vectorType &dst,
				 const vectorType &src,
				 const std::pair<unsigned int,unsigned int> &cell_range) const{

    variableContainer<dim,degree,dealii::VectorizedArray<double> > variable_list(data,userInputs.varInfoListLHS,userInputs.varChangeInfoListLHS);

	//loop over cells
	for (unsigned int cell=cell_range.first; cell<cell_range.second; ++cell){

		// Initialize, read DOFs, and set evaulation flags for each variable
        //variable_list.reinit_and_eval_LHS(src,solutionSet,cell,currentFieldIndex);
        variable_list.reinit_and_eval(solutionSet,cell);
        variable_list.reinit_and_eval_change_in_solution(src,cell,currentFieldIndex);

		unsigned int num_q_points = variable_list.get_num_q_points();

		//loop over quadrature points
		for (unsigned int q=0; q<num_q_points; ++q){
            variable_list.q_point = q;

            dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc = variable_list.get_q_point_location();

			// Calculate the residuals
            equationLHS(variable_list,q_point_loc);

		}

        // Integrate the residuals and distribute from local to global
        variable_list.integrate_and_distribute_change_in_solution_LHS(dst,currentFieldIndex);

	}
}

#include "../../include/matrixFreePDE_template_instantiations.h"

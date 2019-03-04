#include "../../include/matrixFreePDE.h"

template <int dim, int degree>
class customPDE: public MatrixFreePDE<dim,degree>
{
public:
	customPDE(userInputParameters<dim> _userInputs): MatrixFreePDE<dim,degree>(_userInputs) , userInputs(_userInputs) {
		c_dependent_misfit = false;
		for (unsigned int i=0; i<dim; i++){
			for (unsigned int j=0; j<dim; j++){
				if (std::abs(sfts_linear1[i][j])>1.0e-12){
					c_dependent_misfit = true;
				}
			}
		}
        integrated_c_before_set = false;

        Kn1[0][0] = 18.0 * (interfacial_energy_11/2000.0)*(interfacial_energy_11/2000.0)/W;
        Kn1[1][1] = 18.0 * (interfacial_energy_22/2000.0)*(interfacial_energy_22/2000.0)/W;

	};

    // Function to set the initial conditions (in ICs_and_BCs.h)
    void setInitialCondition(const dealii::Point<dim> &p, const unsigned int index, double & scalar_IC, dealii::Vector<double> & vector_IC);

    // Function to set the non-uniform Dirichlet boundary conditions (in ICs_and_BCs.h)
    void setNonUniformDirichletBCs(const dealii::Point<dim> &p, const unsigned int index, const unsigned int direction, const double time, double & scalar_BC, dealii::Vector<double> & vector_BC);

    void solve();

private:
    #include "../../include/typeDefs.h"

    const userInputParameters<dim> userInputs;

    // Function to set the RHS of the governing equations for explicit time dependent equations (in equations.h)
    void explicitEquationRHS(variableContainer<dim,degree,dealii::VectorizedArray<double> > & variable_list,
                     dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const;

    // Function to set the RHS of the governing equations for all other equations (in equations.h)
    void nonExplicitEquationRHS(variableContainer<dim,degree,dealii::VectorizedArray<double> > & variable_list,
                     dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const;

    // Function to set the LHS of the governing equations (in equations.h)
    void equationLHS(variableContainer<dim,degree,dealii::VectorizedArray<double> > & variable_list,
                     dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const;

    // Function to set postprocessing expressions (in postprocess.h)
    #ifdef POSTPROCESS_FILE_EXISTS
    void postProcessedFields(const variableContainer<dim,degree,dealii::VectorizedArray<double> > & variable_list,
                    variableContainer<dim,degree,dealii::VectorizedArray<double> > & pp_variable_list,
                    const dealii::Point<dim, dealii::VectorizedArray<double> > q_point_loc) const;
    #endif

    // Function to set the nucleation probability (in nucleation.h)
    #ifdef NUCLEATION_FILE_EXISTS
    double getNucleationProbability(variableValueContainer variable_value, double dV, dealii::Point<dim> p, unsigned int variable_index) const;
    #endif

	// ================================================================
	// Methods specific to this subclass
	// ================================================================

    void solveIncrement(bool skip_time_dependent);


	// ================================================================
	// Model constants specific to this subclass
	// ================================================================

	double McV = userInputs.get_model_constant_double("McV");
	double Mn1V = userInputs.get_model_constant_double("Mn1V");


	//double W = userInputs.get_model_constant_double("W");

    double interfacial_thickness = userInputs.get_model_constant_double("interfacial_thickness");
    double interfacial_energy_11 = userInputs.get_model_constant_double("interfacial_energy_11");
    double interfacial_energy_22 = userInputs.get_model_constant_double("interfacial_energy_22");

    double W = 2.2 * 6.0 * interfacial_energy_11/2000.0 / interfacial_thickness;

    dealii::Tensor<2,dim> Kn1;

	bool n_dependent_stiffness = userInputs.get_model_constant_bool("n_dependent_stiffness");
	dealii::Tensor<2,dim> sfts_linear1 = userInputs.get_model_constant_rank_2_tensor("sfts_linear1");
	dealii::Tensor<2,dim> sfts_const1 = userInputs.get_model_constant_rank_2_tensor("sfts_const1");

    double A2 = userInputs.get_model_constant_double("A_curvature");
    double A_minimum = userInputs.get_model_constant_double("A_minimum");
	double A1 = -2.0 * A2 * A_minimum;
	double A0 = A2 * A_minimum * A_minimum;

	double B2 = userInputs.get_model_constant_double("B_curvature");
    double B_minimum = userInputs.get_model_constant_double("B_minimum");
	double B1 = -2.0 * B2 * B_minimum;
	double B0 = B2 * B_minimum * B_minimum;

	const static unsigned int CIJ_tensor_size =2*dim-1+dim/3;
	dealii::Tensor<2,CIJ_tensor_size> CIJ_Mg = userInputs.get_model_constant_elasticity_tensor("CIJ_Mg");
	dealii::Tensor<2,CIJ_tensor_size> CIJ_Beta = userInputs.get_model_constant_elasticity_tensor("CIJ_Beta");

    double equilbrium_tol = userInputs.get_model_constant_double("equilbrium_tol");

	bool c_dependent_misfit;

	double integrated_c_before;
    bool integrated_c_before_set;

	double dt_modifier;

	// ================================================================

};

template <int dim, int degree>
void customPDE<dim,degree>::solve(){
    //log time
    this->computing_timer.enter_section("matrixFreePDE: solve");
    this->pcout << "\nsolving...\n\n";

    std::vector<std::vector<double>> list_of_integrated_postprocessed_fields;

    //time dependent BVP
    if (this->isTimeDependentBVP){

        // If grain reassignment is activated, reassign grains
        if (this->userInputs.grain_remapping_activated and (this->currentIncrement%this->userInputs.skip_grain_reassignment_steps == 0 or this->currentIncrement == 0) ) {
            this->reassignGrains();
        }

        // For any nonlinear equation, set the initial guess as the solution to Laplace's equations
        this->generatingInitialGuess = true;
        this->setNonlinearEqInitialGuess();
        this->generatingInitialGuess = false;

        // Do an initial solve to set the elliptic fields
        solveIncrement(true);

        //output initial conditions for time dependent BVP
        if (this->userInputs.outputTimeStepList[this->currentOutput] == this->currentIncrement) {

            for(unsigned int fieldIndex=0; fieldIndex<this->fields.size(); fieldIndex++){
                this->constraintsDirichletSet[fieldIndex]->distribute(*this->solutionSet[fieldIndex]);
                this->constraintsOtherSet[fieldIndex]->distribute(*this->solutionSet[fieldIndex]);
                this->solutionSet[fieldIndex]->update_ghost_values();
            }
            this->outputResults();
            this->currentOutput++;
        }

        if (this->userInputs.checkpointTimeStepList[this->currentCheckpoint] == this->currentIncrement) {
            this->save_checkpoint();
            this->currentCheckpoint++;
        }

        // Increase the current increment from 0 to 1 now that the initial conditions have been output
        this->currentIncrement++;

        // Cycle up to the proper output and checkpoint counters
        while (this->userInputs.outputTimeStepList.size() > 0 && this->userInputs.outputTimeStepList[this->currentOutput] < this->currentIncrement){
            this->currentOutput++;
        }
        while (this->userInputs.checkpointTimeStepList.size() > 0 && this->userInputs.checkpointTimeStepList[this->currentCheckpoint] < this->currentIncrement){
            this->currentCheckpoint++;
        }

        //time stepping
        this->pcout << "\nTime stepping parameters: timeStep: " << this->userInputs.dtValue << "  timeFinal: " << this->userInputs.finalTime << "  timeIncrements: " << this->userInputs.totalIncrements << "\n";

        // This is the main time-stepping loop
        for (; this->currentIncrement<=this->userInputs.totalIncrements; ++this->currentIncrement){

            //increment current time
            this->currentTime+=this->userInputs.dtValue;
            if (this->currentIncrement%userInputs.skip_print_steps==0){
                this->pcout << "\ntime increment:" << this->currentIncrement << "  time: " << this->currentTime << "\n";
            }

            //check and perform adaptive mesh refinement
            this->adaptiveRefine(this->currentIncrement);

            // Update the list of nuclei (if relevant)
            this->updateNucleiList();

            // If grain reassignment is activated, reassign grains
            if (this->userInputs.grain_remapping_activated and (this->currentIncrement%this->userInputs.skip_grain_reassignment_steps == 0 or this->currentIncrement == 0) ) {
                this->reassignGrains();
            }
            //solve time increment
            solveIncrement(false);

            // Output results to file (on the proper increments)
            if (this->userInputs.outputTimeStepList[this->currentOutput] == this->currentIncrement) {

                for(unsigned int fieldIndex=0; fieldIndex<this->fields.size(); fieldIndex++){
                    this->constraintsDirichletSet[fieldIndex]->distribute(*this->solutionSet[fieldIndex]);
                    this->constraintsOtherSet[fieldIndex]->distribute(*this->solutionSet[fieldIndex]);
                    this->solutionSet[fieldIndex]->update_ghost_values();
                }
                this->outputResults();
                this->currentOutput++;


                list_of_integrated_postprocessed_fields.push_back(this->integrated_postprocessed_fields);

                unsigned int n_ouputs_so_far = list_of_integrated_postprocessed_fields.size();
                if (n_ouputs_so_far > 1){
                    if (abs(list_of_integrated_postprocessed_fields[n_ouputs_so_far-2][0] - list_of_integrated_postprocessed_fields[n_ouputs_so_far-1][0]) < equilbrium_tol){
                        if (n_ouputs_so_far < this->userInputs.outputTimeStepList.size()-1){
                            this->currentIncrement = this->userInputs.outputTimeStepList[this->currentOutput]-1;
                        }

                    }
                }
            }

            // Create a checkpoint (on the proper increments)
            if (this->userInputs.checkpointTimeStepList[this->currentCheckpoint] == this->currentIncrement) {
                this->save_checkpoint();
                this->currentCheckpoint++;
            }

        }
    }

    //time independent BVP
    else{
        this->generatingInitialGuess = false;

        //solve
        this->solveIncrement(false);

        //output results to file
        this->outputResults();
    }

    //log time
    this->computing_timer.exit_section("matrixFreePDE: solve");
}


#include <deal.II/lac/solver_cg.h>

template <int dim, int degree>
void customPDE<dim,degree>::solveIncrement(bool skip_time_dependent){

    //log time
    this->computing_timer.enter_section("matrixFreePDE: solveIncrements");
    Timer time;
    char buffer[200];

    // Get the RHS of the explicit equations
    if (this->hasExplicitEquation && !skip_time_dependent){
        this->computeExplicitRHS();
    }


    //solve for each field
    for(unsigned int fieldIndex=0; fieldIndex<this->fields.size(); fieldIndex++){
        this->currentFieldIndex = fieldIndex; // Used in computeLHS()

        // Add Neumann BC terms to the residual vector for the current field, if appropriate
        // Currently commented out because it isn't working yet
        //applyNeumannBCs();

        //Parabolic (first order derivatives in time) fields
        if (this->fields[fieldIndex].pdetype==EXPLICIT_TIME_DEPENDENT && !skip_time_dependent){

            double nominal_dt;
			if (!integrated_c_before_set){
				dt_modifier = 0.0;
			}
			else{
				dt_modifier = 1.0;
			}

			if (fieldIndex == 0 && !integrated_c_before_set){
				this->computeIntegralMF(integrated_c_before, fieldIndex, this->solutionSet);
                                integrated_c_before_set = true;
			}

            // Explicit-time step each DOF
            // Takes advantage of knowledge that the length of solutionSet and residualSet is an integer multiple of the length of invM for vector variables
            unsigned int invM_size = this->invM.local_size();
            for (unsigned int dof=0; dof<this->solutionSet[fieldIndex]->local_size(); ++dof){
                this->solutionSet[fieldIndex]->local_element(dof)=			\
                this->invM.local_element(dof%invM_size)*this->residualSet[fieldIndex]->local_element(dof);
            }

            if (fieldIndex == 0){
				this->constraintsOtherSet[fieldIndex]->distribute(*(this->solutionSet[fieldIndex]));
				this->constraintsDirichletSet[fieldIndex]->distribute(*(this->solutionSet[fieldIndex]));
	            this->solutionSet[fieldIndex]->update_ghost_values();

				double integrated_c_after;
				this->computeIntegralMF(integrated_c_after, fieldIndex, this->solutionSet);

				double domain_volume = 1.0;
				for (unsigned int i=0; i<dim; i++){
					domain_volume *= userInputs.domain_size[i];
				}

				for (unsigned int dof=0; dof<this->solutionSet[fieldIndex]->local_size(); ++dof){
	                this->solutionSet[fieldIndex]->local_element(dof) -= (integrated_c_after - integrated_c_before)/(domain_volume);
	            }
				//this->pcout << "Before, after, shift:" << integrated_c_before << " " << integrated_c_after << " " << (integrated_c_after - integrated_c_before)/(domain_volume) << std::endl;
			}

            // Set the Dirichelet values (hanging node constraints don't need to be distributed every time step, only at output)
            if (this->has_Dirichlet_BCs){
                this->constraintsDirichletSet[fieldIndex]->distribute(*this->solutionSet[fieldIndex]);
            }
            //computing_timer.enter_section("matrixFreePDE: updateExplicitGhosts");
            this->solutionSet[fieldIndex]->update_ghost_values();
            //computing_timer.exit_section("matrixFreePDE: updateExplicitGhosts");

            // Print update to screen and confirm that solution isn't nan
            if (this->currentIncrement%userInputs.skip_print_steps==0){
                double solution_L2_norm = this->solutionSet[fieldIndex]->l2_norm();

                sprintf(buffer, "field '%2s' [explicit solve]: current solution: %12.6e, current residual:%12.6e\n", \
                this->fields[fieldIndex].name.c_str(),				\
                solution_L2_norm,			\
                this->residualSet[fieldIndex]->l2_norm());
                this->pcout<<buffer;

                if (!numbers::is_finite(solution_L2_norm)){
                    sprintf(buffer, "ERROR: field '%s' solution is NAN. exiting.\n\n",
                    this->fields[fieldIndex].name.c_str());
                    this->pcout<<buffer;
                    exit(-1);
               }

            }
        }

    }

    // Now, update the non-explicit variables
    // For the time being, this is just the elliptic equations, but implicit parabolic and auxilary equations should also be here
    if (this->hasNonExplicitEquation){

        bool nonlinear_it_converged = false;
        unsigned int nonlinear_it_index = 0;

        while (!nonlinear_it_converged){
            nonlinear_it_converged = true; // Set to true here and will be set to false if any variable isn't converged

            // Update residualSet for the non-explicitly updated variables
            //compute_nonexplicit_RHS()
            // Ideally, I'd just do this for the non-explicit variables, but for now I'll do all of them
            // this is a little redundant, but hopefully not too terrible
            this->computeNonexplicitRHS();

            for(unsigned int fieldIndex=0; fieldIndex<this->fields.size(); fieldIndex++){
                this->currentFieldIndex = fieldIndex; // Used in computeLHS()

                if ( (this->fields[fieldIndex].pdetype == IMPLICIT_TIME_DEPENDENT && !skip_time_dependent) || this->fields[fieldIndex].pdetype == TIME_INDEPENDENT){

                    if (this->currentIncrement%userInputs.skip_print_steps==0 && userInputs.var_nonlinear[fieldIndex]){
                        sprintf(buffer, "field '%2s' [nonlinear solve]: current solution: %12.6e, current residual:%12.6e\n", \
                        this->fields[fieldIndex].name.c_str(),				\
                        this->solutionSet[fieldIndex]->l2_norm(),			\
                        this->residualSet[fieldIndex]->l2_norm());
                        this->pcout<<buffer;
                    }

                    dealii::parallel::distributed::Vector<double> solution_diff = *this->solutionSet[fieldIndex];

                    //apply Dirichlet BC's
                    // Loops through all DoF to which ones have Dirichlet BCs applied, replace the ones that do with the Dirichlet value
                    // This clears the residual where we want to apply Dirichlet BCs, otherwise the solver sees a positive residual
                    for (std::map<types::global_dof_index, double>::const_iterator it=this->valuesDirichletSet[fieldIndex]->begin(); it!=this->valuesDirichletSet[fieldIndex]->end(); ++it){
                        if (this->residualSet[fieldIndex]->in_local_range(it->first)){
                            (*this->residualSet[fieldIndex])(it->first) = 0.0;
                        }
                    }

                    //solver controls
                    double tol_value;
                    if (MatrixFreePDE<dim,degree>::userInputs.linear_solver_parameters.getToleranceType(fieldIndex) == ABSOLUTE_RESIDUAL){
                        tol_value = MatrixFreePDE<dim,degree>::userInputs.linear_solver_parameters.getToleranceValue(fieldIndex);
                    }
                    else {
                        tol_value = MatrixFreePDE<dim,degree>::userInputs.linear_solver_parameters.getToleranceValue(fieldIndex)*this->residualSet[fieldIndex]->l2_norm();
                    }

                    SolverControl solver_control(MatrixFreePDE<dim,degree>::userInputs.linear_solver_parameters.getMaxIterations(fieldIndex), tol_value);

                    // Currently the only allowed solver is SolverCG, the SolverType input variable is a dummy
                    SolverCG<vectorType> solver(solver_control);

                    //solve
                    try{
                        if (this->fields[fieldIndex].type == SCALAR){
                            this->dU_scalar=0.0;
                            solver.solve(*this, this->dU_scalar, *this->residualSet[fieldIndex], IdentityMatrix(this->solutionSet[fieldIndex]->size()));
                        }
                        else {
                            this->dU_vector=0.0;
                            solver.solve(*this, this->dU_vector, *this->residualSet[fieldIndex], IdentityMatrix(this->solutionSet[fieldIndex]->size()));
                        }
                    }
                    catch (...) {
                        this->pcout << "\nWarning: implicit solver did not converge as per set tolerances. consider increasing maxSolverIterations or decreasing solverTolerance.\n";
                    }

                    if (userInputs.var_nonlinear[fieldIndex]){

                        // Now that we have the calculated change in the solution, we need to select a damping coefficient
                        double damping_coefficient;

                        if (MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getBacktrackDampingFlag(fieldIndex)){
                            vectorType solutionSet_old = *this->solutionSet[fieldIndex];
                            double residual_old = this->residualSet[fieldIndex]->l2_norm();

                            damping_coefficient = 1.0;
                            bool damping_coefficient_found = false;
                            while (!damping_coefficient_found){
                                if (this->fields[fieldIndex].type == SCALAR){
                                    this->solutionSet[fieldIndex]->sadd(1.0,damping_coefficient,this->dU_scalar);
                                }
                                else {
                                    this->solutionSet[fieldIndex]->sadd(1.0,damping_coefficient,this->dU_vector);
                                }

                                this->computeNonexplicitRHS();

                                for (std::map<types::global_dof_index, double>::const_iterator it=this->valuesDirichletSet[fieldIndex]->begin(); it!=this->valuesDirichletSet[fieldIndex]->end(); ++it){
                                    if (this->residualSet[fieldIndex]->in_local_range(it->first)){
                                        (*this->residualSet[fieldIndex])(it->first) = 0.0;
                                    }
                                }

                                double residual_new = this->residualSet[fieldIndex]->l2_norm();

                                if (this->currentIncrement%userInputs.skip_print_steps==0){
                                    this->pcout << "    Old residual: " << residual_old << " Damping Coeff: " << damping_coefficient << " New Residual: " << residual_new << std::endl;
                                }

                                // An improved approach would use the Armijo–Goldstein condition to ensure a sufficent decrease in the residual. This way is just scales the residual.
                                if ( (residual_new < (residual_old*MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getBacktrackResidualDecreaseCoeff(fieldIndex))) || damping_coefficient < 1.0e-4){
                                    damping_coefficient_found = true;
                                }
                                else{
                                    damping_coefficient *= MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getBacktrackStepModifier(fieldIndex);
                                    *this->solutionSet[fieldIndex] = solutionSet_old;
                                }
                            }
                        }
                        else{
                            damping_coefficient = MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getDefaultDampingCoefficient(fieldIndex);

                            if (this->fields[fieldIndex].type == SCALAR){
                                this->solutionSet[fieldIndex]->sadd(1.0,damping_coefficient,this->dU_scalar);
                            }
                            else {
                                this->solutionSet[fieldIndex]->sadd(1.0,damping_coefficient,this->dU_vector);
                            }
                        }

                        if (this->currentIncrement%userInputs.skip_print_steps==0){
                            double dU_norm;
                            if (this->fields[fieldIndex].type == SCALAR){
                                dU_norm = this->dU_scalar.l2_norm();
                            }
                            else {
                                dU_norm = this->dU_vector.l2_norm();
                            }
                            sprintf(buffer, "field '%2s' [implicit solve]: initial residual:%12.6e, current residual:%12.6e, nsteps:%u, tolerance criterion:%12.6e, solution: %12.6e, dU: %12.6e\n", \
                            this->fields[fieldIndex].name.c_str(),			\
                            this->residualSet[fieldIndex]->l2_norm(),			\
                            solver_control.last_value(),				\
                            solver_control.last_step(), solver_control.tolerance(), this->solutionSet[fieldIndex]->l2_norm(), dU_norm);
                            this->pcout<<buffer;
                        }

                        // Check to see if this individual variable has converged
                        if (MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getToleranceType(fieldIndex) == ABSOLUTE_SOLUTION_CHANGE){
                            double diff;

                            if (this->fields[fieldIndex].type == SCALAR){
                                diff = this->dU_scalar.l2_norm();
                            }
                            else {
                                diff = this->dU_vector.l2_norm();
                            }
                            if (this->currentIncrement%userInputs.skip_print_steps==0){
                                this->pcout << "Relative difference between nonlinear iterations: " << diff << " " << nonlinear_it_index << " " << this->currentIncrement << std::endl;
                            }

                            if (diff > MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getToleranceValue(fieldIndex) && nonlinear_it_index < MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getMaxIterations()){
                                nonlinear_it_converged = false;
                            }
                        }
                        else {
                            std::cerr << "PRISMS-PF Error: Nonlinear solver tolerance types other than ABSOLUTE_CHANGE have yet to be implemented." << std::endl;
                        }
                    }
                    else {
                        if (nonlinear_it_index ==0){

                            if (this->fields[fieldIndex].type == SCALAR){
                                *this->solutionSet[fieldIndex] += this->dU_scalar;
                            }
                            else {
                                *this->solutionSet[fieldIndex] += this->dU_vector;
                            }

                            if (this->currentIncrement%userInputs.skip_print_steps==0){
                                double dU_norm;
                                if (this->fields[fieldIndex].type == SCALAR){
                                    dU_norm = this->dU_scalar.l2_norm();
                                }
                                else {
                                    dU_norm = this->dU_vector.l2_norm();
                                }
                                sprintf(buffer, "field '%2s' [implicit solve]: initial residual:%12.6e, current residual:%12.6e, nsteps:%u, tolerance criterion:%12.6e, solution: %12.6e, dU: %12.6e\n", \
                                this->fields[fieldIndex].name.c_str(),			\
                                this->residualSet[fieldIndex]->l2_norm(),			\
                                solver_control.last_value(),				\
                                solver_control.last_step(), solver_control.tolerance(), this->solutionSet[fieldIndex]->l2_norm(), dU_norm);
                                this->pcout<<buffer;
                            }
                        }

                    }
                }
                else if (this->fields[fieldIndex].pdetype == AUXILIARY){

                    if (userInputs.var_nonlinear[fieldIndex] || nonlinear_it_index == 0){

                        // If the equation for this field is nonlinear, save the old solution
                        if (userInputs.var_nonlinear[fieldIndex]){
                            if (this->fields[fieldIndex].type == SCALAR){
                                this->dU_scalar = *this->solutionSet[fieldIndex];
                            }
                            else {
                                this->dU_vector = *this->solutionSet[fieldIndex];
                            }
                        }

                        // Explicit-time step each DOF
                        // Takes advantage of knowledge that the length of solutionSet and residualSet is an integer multiple of the length of invM for vector variables
                        unsigned int invM_size = this->invM.local_size();
                        for (unsigned int dof=0; dof<this->solutionSet[fieldIndex]->local_size(); ++dof){
                            this->solutionSet[fieldIndex]->local_element(dof)=			\
                            this->invM.local_element(dof%invM_size)*this->residualSet[fieldIndex]->local_element(dof);
                        }

                        // Set the Dirichelet values (hanging node constraints don't need to be distributed every time step, only at output)
                        this->constraintsDirichletSet[fieldIndex]->distribute(*this->solutionSet[fieldIndex]);
                        this->solutionSet[fieldIndex]->update_ghost_values();

                        // Print update to screen
                        if (this->currentIncrement%userInputs.skip_print_steps==0){
                            sprintf(buffer, "field '%2s' [auxiliary solve]: current solution: %12.6e, current residual:%12.6e\n", \
                            this->fields[fieldIndex].name.c_str(),				\
                            this->solutionSet[fieldIndex]->l2_norm(),			\
                            this->residualSet[fieldIndex]->l2_norm());
                            this->pcout<<buffer;
                        }

                        // Check to see if this individual variable has converged
                        if (userInputs.var_nonlinear[fieldIndex]){
                            if (MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getToleranceType(fieldIndex) == ABSOLUTE_SOLUTION_CHANGE){

                                double diff;

                                if (this->fields[fieldIndex].type == SCALAR){
                                    this->dU_scalar -= *this->solutionSet[fieldIndex];
                                    diff = this->dU_scalar.l2_norm();
                                }
                                else {
                                    this->dU_vector -= *this->solutionSet[fieldIndex];
                                    diff = this->dU_vector.l2_norm();
                                }
                                if (this->currentIncrement%userInputs.skip_print_steps==0){
                                    this->pcout << "Relative difference between nonlinear iterations: " << diff << " " << nonlinear_it_index << " " << this->currentIncrement << std::endl;
                                }

                                if (diff > MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getToleranceValue(fieldIndex) && nonlinear_it_index < MatrixFreePDE<dim,degree>::userInputs.nonlinear_solver_parameters.getMaxIterations()){
                                    nonlinear_it_converged = false;
                                }

                            }
                            else {
                                std::cerr << "PRISMS-PF Error: Nonlinear solver tolerance types other than ABSOLUTE_CHANGE have yet to be implemented." << std::endl;
                            }
                        }
                    }
                }

                //check if solution is nan
                if (!numbers::is_finite(this->solutionSet[fieldIndex]->l2_norm())){
                    sprintf(buffer, "ERROR: field '%s' solution is NAN. exiting.\n\n",
                    this->fields[fieldIndex].name.c_str());
                    this->pcout<<buffer;
                    exit(-1);
                }

            }

            nonlinear_it_index++;
        }
    }

    if (this->currentIncrement%userInputs.skip_print_steps==0){
        this->pcout << "wall time: " << time.wall_time() << "s\n";
    }
    //log time
    this->computing_timer.exit_section("matrixFreePDE: solveIncrements");

}

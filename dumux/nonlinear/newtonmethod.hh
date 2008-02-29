#ifndef DUNE_NEWTONMETHOD_HH
#define DUNE_NEWTONMETHOD_HH

namespace Dune {
	template<class G, class Model>
	class NewtonMethod 
	{
		typedef typename Model::FunctionType FunctionType;
		typedef typename Model::OperatorAssembler OperatorAssembler;
		typedef typename Model::LocalJacobian LocalJacobian;
	public:
		void execute(bool verbose = true)
		{
			double oneByMagnitude = 1.0/std::max((*u).two_norm(), 1e-5);
			double error = 1e100;			
			double dt = localJacobian.getDt();
			bool divided = false;
			
			while (dt > minDt && error > tolerance) {
				double error = 1e100;
				int iter = 0;
				while (error > tolerance && iter < maxIter) {
					iter ++;
					*uOldNewtonStep = *u;
					//printvector(std::cout, *uOldNewtonStep, "uOldNewtonStep", "row", 200, 1, 3);
					//printvector(std::cout, *(model.uOldTimeStep), "uOldTimeStep", "row", 200, 1, 3);
					*f = 0;
					localJacobian.clearVisited();
					A.assemble(localJacobian, u, f);
					//printmatrix(std::cout, *A, "global stiffness matrix", "row", 11, 3);
					std::cout << "matrix norm: " << (*A).infinity_norm() << std::endl;
					//printvector(std::cout, *f, "right hand side", "row", 200, 1, 3);
					model.solve();
					error = oneByMagnitude*((*u).two_norm());
					//printvector(std::cout, *u, "update", "row", 200, 1, 3);
					*u *= -1.0;
					*u += *uOldNewtonStep; 
					for (int idx = 0; idx < (*u).size(); idx++) 
						if ((*u)[idx][1] < -1.0 || (*u)[idx][1] > 2.0) 
							error = 1e100;

					//printvector(std::cout, *u, "u", "row", 200, 1, 3);
					if (verbose)
						std::cout << "Newton step " << iter << ", defect = " << error << std::endl;
				}
				
				if (error > tolerance) {
					std::cout << "NewtonMethod::execute(), tolerance = " << tolerance 
						<< ": did not converge in " << iter << " iterations" << std::endl; 
					dt  = 0.5*dt;
					std::cout << "retry with reduced time step size of " << dt << std::endl;
					localJacobian.setDt(dt);
					*u = *(model.uOldTimeStep);
					divided = true;
				}
				else { 
					if (!divided && iter < goodIter) {
						dt = 2.0*dt;
						std::cout << "Below " << goodIter 
							<< " Newton iterations. Time step size doubled to " << dt << std::endl;
					}
					localJacobian.setDt(dt);
						
					return;
				}
			}
			
			if (dt <= minDt) 
				DUNE_THROW(MathError, "NewtonMethod:: time step size below minimum " << minDt << ".");
			
			
			return;
		}
		
		NewtonMethod(const G& g, Model& mod, double tol = 1e-6, int maxIt = 12, double mind = 1e-5, int goodIt = 5)
		: grid(g), model(mod), u(mod.u), f(mod.f), A(mod.A), localJacobian(mod.localJacobian), 
		  uOldNewtonStep(g), tolerance(tol), maxIter(maxIt), minDt(mind), goodIter(goodIt)
		{ }
		
		NewtonMethod(const G& g, Model& mod, int level, double tol = 1e-6, int maxIt = 12, double mind = 1e-5, int goodIt = 5)
		: grid(g), model(mod), u(mod.u), f(mod.f), A(mod.A), localJacobian(mod.localJacobian), 
		  uOldNewtonStep(g, level), tolerance(tol), maxIter(maxIt), minDt(mind), goodIter(goodIt)
		{ }
		
	private:
		const G& grid;
		Model& model;
		FunctionType& u;
		FunctionType& f;
		OperatorAssembler& A;
		LocalJacobian& localJacobian;
		FunctionType uOldNewtonStep;
		double tolerance;
		int maxIter;
		double minDt;
		int goodIter;
	};
}
#endif

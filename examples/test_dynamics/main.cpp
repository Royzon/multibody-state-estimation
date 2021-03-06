/*+-------------------------------------------------------------------------+
  |            Multi Body State Estimation (mbse) C++ library               |
  |                                                                         |
  | Copyright (C) 2014-2020 University of Almeria                           |
  | Copyright (C) 2020 University of Salento                                |
  | See README for list of authors and papers                               |
  | Distributed under 3-clause BSD license                                  |
  |  See: <https://opensource.org/licenses/BSD-3-Clause>                    |
  +-------------------------------------------------------------------------+ */

// Example of dynamics
// ------------------------------------------------------------
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/slam/PriorFactor.h>
#include <iostream>
#include <fstream>
#include <mbse/CAssembledRigidModel.h>
#include <mbse/CModelDefinition.h>
#include <mbse/factors/FactorDynamics.h>
#include <mbse/factors/FactorConstraints.h>
#include <mbse/factors/FactorConstraintsVel.h>
#include <mbse/factors/FactorTrapInt.h>
#include <mbse/dynamics/dynamic-simulators.h>
#include <mbse/model-examples.h>

#include <mrpt/gui/CDisplayWindowPlots.h>

using namespace std;
using namespace mbse;

void test_dynamics()
{
	using gtsam::symbol_shorthand::A;
	using gtsam::symbol_shorthand::Q;
	using gtsam::symbol_shorthand::V;
	// using namespace mbse; /Already used in the global section above

	// Create the multibody object:
	CModelDefinition model;
	mbse::buildFourBarsMBS(model);

	std::shared_ptr<CAssembledRigidModel> aMBS = model.assembleRigidMBS();
	aMBS->setGravityVector(0, -9.81, 0);

	CDynamicSimulator_R_matrix_dense dynSimul(aMBS);

	// Must be called before solve_ddotq(), needed inside the dynamics factors
	dynSimul.prepare();

	// Create the empty factor graph:
	gtsam::NonlinearFactorGraph graph;
	gtsam::Values values;

	// Add factors:
	// Create factor noises:
	const auto n = aMBS->q_.size();
	const auto m = aMBS->Phi_q_.getNumRows();

	const double noise_vel_sigma = 0.01, noise_acc_sigma = 0.01;

	auto noise_vel = gtsam::noiseModel::Isotropic::Sigma(n, noise_vel_sigma);
	auto noise_acc = gtsam::noiseModel::Isotropic::Sigma(n, noise_acc_sigma);

	// x1, *y1*, x2, y2
	// 0   1     2   3
	std::vector<size_t> indepCoordIndices;
	indepCoordIndices.push_back(0);

	// Velocity prior: large sigma for all dq(i), except dq(i_indep)
	gtsam::Vector prior_dq_sigmas;
	const double large_std = 1e6;
	const double small_std = 1e-3;
	prior_dq_sigmas.setConstant(n, large_std);
	for (auto idx : indepCoordIndices) prior_dq_sigmas(idx) = small_std;

	auto noise_prior_dq = gtsam::noiseModel::Diagonal::Sigmas(prior_dq_sigmas);
	auto noise_prior_q = gtsam::noiseModel::Isotropic::Sigma(n, 0.1);
	auto noise_dyn = gtsam::noiseModel::Isotropic::Sigma(n, 0.1);
	auto noise_constr_q = gtsam::noiseModel::Isotropic::Sigma(m, 0.001);
	auto noise_constr_dq = gtsam::noiseModel::Isotropic::Sigma(m, 0.001);

	const double dt = 0.01;
	const double t_end = 10.0;
	double t = 0;
	unsigned int N = t_end / dt;

	// Create null vector, for use in velocity and accelerations:
	const state_t zeros = gtsam::Vector(gtsam::Vector::Zero(n, 1));

	// Create a feasible Q(0):
	aMBS->q_.setZero();
	aMBS->dotq_.setZero();
	aMBS->ddotq_.setZero();

	CAssembledRigidModel::TComputeDependentParams cdp;  // default params
	CAssembledRigidModel::TComputeDependentResults cdr;
	// Solve the position problem:
	aMBS->q_[0] = 1;
	aMBS->q_[1] = 0.1;
	aMBS->q_[3] = 5;

	aMBS->computeDependentPosVelAcc(indepCoordIndices, true, true, cdp, cdr);
	std::cout << "Position problem final |Phi(q)|=" << cdr.pos_final_phi
			  << "\n";
	ASSERT_BELOW_(cdr.pos_final_phi, 1e-4);

	// Extract q_ from the assembled multibody problem:
	state_t q_0 = gtsam::Vector(aMBS->q_);
	std::cout << "q0: " << q_0.transpose() << "\n";
	state_t last_q = q_0, last_dq = zeros, last_ddq = zeros;

	// Create Prior factors:
	graph.emplace_shared<gtsam::PriorFactor<state_t>>(Q(0), q_0, noise_prior_q);
	graph.emplace_shared<gtsam::PriorFactor<state_t>>(
		V(0), zeros, noise_prior_dq);

	gtsam::LevenbergMarquardtParams lmp;
	// just a few iterations since we will run it many times with partial,
	// incrementally-built problem:
	lmp.maxIterations = 5;

	for (unsigned int nn = 0; nn < N; nn++, t += dt)
	{
		// Create Trapezoidal Integrator factors:
		graph.emplace_shared<FactorTrapInt>(
			dt, noise_vel, Q(nn), Q(nn + 1), V(nn), V(nn + 1));
		graph.emplace_shared<FactorTrapInt>(
			dt, noise_acc, V(nn), V(nn + 1), A(nn), A(nn + 1));

		// Create Dynamics factors:
		graph.emplace_shared<FactorDynamics>(
			&dynSimul, noise_dyn, Q(nn), V(nn), A(nn));

		// Add dependent-coordinates constraint factor:
		graph.emplace_shared<FactorConstraints>(aMBS, noise_constr_q, Q(nn));
		graph.emplace_shared<FactorConstraintsVel>(
			aMBS, noise_constr_dq, Q(nn), V(nn));

		// Create initial estimates:
		if (values.find(Q(nn)) == values.end()) values.insert(Q(nn), last_q);
		if (values.find(V(nn)) == values.end()) values.insert(V(nn), last_dq);
		if (values.find(A(nn)) == values.end()) values.insert(A(nn), last_ddq);

		if (nn == N - 1)
		{
			// Create Dynamics factors:
			graph.emplace_shared<FactorDynamics>(
				&dynSimul, noise_dyn, Q(nn + 1), V(nn + 1), A(nn + 1));
		}

		// Create initial estimates (so we can run LevMarq)
		values.insert(Q(nn + 1), last_q);
		values.insert(V(nn + 1), last_dq);
		values.insert(A(nn + 1), last_ddq);

		// Once in a while, run the optimizer so the initial values are not so
		// far from the optimal place and the problem is easier to solve:
		// Also, make sure we run at the LAST timestep:
		if ((nn % 50) == 0 || nn == N - 1)
		{
			if (nn == (N - 1)) lmp.maxIterations = 10;

			std::cout << "Running optimization at t=" << nn << "/" << N << "\n";
			const double err_init = graph.error(values);

			gtsam::LevenbergMarquardtOptimizer optimizer(graph, values, lmp);
			values = optimizer.optimize();

			const double err_final = graph.error(values);

			// Uncomment to see per-factor errors:
			// graph.printErrors(values, "ERRORS:");

			std::cout << " Initial factors error: " << err_init
					  << ", RMSE=" << std::sqrt(err_init / graph.size())
					  << "\n";
			std::cout << " Final factors error: " << err_final
					  << ", RMSE=" << std::sqrt(err_final / graph.size())
					  << "\n";
			std::cout << " Optimization iterations: " << optimizer.iterations()
					  << "\n";
		}

		last_q = values.at<state_t>(Q(nn));
		last_dq = values.at<state_t>(V(nn));
		last_ddq = values.at<state_t>(A(nn));
	}

	// Run optimizer:
	// std::cout.precision(3);
	// graph.print("Factor graph: ");
	// values.print("values");
	graph.printErrors(values);

	// Save states to files:
	mrpt::math::CMatrixDouble Qs(N, n), dotQs(N, n), ddotQs(N, n);
	for (unsigned int step = 0; step < N; step++)
	{
		const state_t q_val = values.at<state_t>(Q(step));
		const state_t dq_val = values.at<state_t>(V(step));
		const state_t ddq_val = values.at<state_t>(A(step));

		Qs.row(step) = q_val;
		dotQs.row(step) = dq_val;
		ddotQs.row(step) = ddq_val;
	}
	std::cout << "Saving results to TXT files...\n";
	Qs.saveToTextFile("q.txt");
	dotQs.saveToTextFile("dq.txt");
	ddotQs.saveToTextFile("ddq.txt");

	// Save graph:
	if (0)
	{
		std::ofstream f("graph.dot");
		gtsam::GraphvizFormatting gvf;
		graph.saveGraph(f, values, gvf);
	}

	/* mrpt::gui::CDisplayWindowPlots win;

	std::vector<double> xs, ys;
	for (int i = 0; i < 100; i++)
	{
		xs.push_back(i);
		ys.push_back(sin(i * 0.01));
	}

	win.plot(xs, ys, "r.5");
	win.waitForKey();*/
}

int main()
{
	try
	{
		test_dynamics();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << "\n";
	}
}

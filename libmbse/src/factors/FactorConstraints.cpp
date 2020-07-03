/*+-------------------------------------------------------------------------+
  |            Multi Body State Estimation (mbse) C++ library               |
  |                                                                         |
  | Copyright (C) 2014-2020 University of Almeria                           |
  | Copyright (C) 2020 University of Salento                                |
  | See README for list of authors and papers                               |
  | Distributed under 3-clause BSD license                                  |
  |  See: <https://opensource.org/licenses/BSD-3-Clause>                    |
  +-------------------------------------------------------------------------+ */

#include <mbse/factors/FactorConstraints.h>
#include <mbse/CAssembledRigidModel.h>

using namespace mbse;

FactorConstraints::~FactorConstraints() = default;

gtsam::NonlinearFactor::shared_ptr FactorConstraints::clone() const
{
	return boost::static_pointer_cast<gtsam::NonlinearFactor>(
		gtsam::NonlinearFactor::shared_ptr(new This(*this)));
}

void FactorConstraints::print(
	const std::string& s, const gtsam::KeyFormatter& keyFormatter) const
{
	std::cout << s << "FactorConstrints(" << keyFormatter(this->key()) << ")\n";
	noiseModel_->print("  noise model: ");
}

bool FactorConstraints::equals(
	const gtsam::NonlinearFactor& expected, double tol) const
{
	const This* e = dynamic_cast<const This*>(&expected);
	return e != nullptr && Base::equals(*e, tol);
}

gtsam::Vector FactorConstraints::evaluateError(
	const state_t& q_k, boost::optional<gtsam::Matrix&> H1) const
{
	MRPT_START

	const auto n = q_k.size();
	if (n < 1) throw std::runtime_error("Empty state vector!");

	// Set q in the multibody model:
	arm_->q_ = q_k.vector();

	// Update Jacobians:
	arm_->update_numeric_Phi_and_Jacobians();

	// Evaluate error:
	gtsam::Vector err = arm_->Phi_;

	// Get the Jacobians required for optimization:
	// d err / d q_k
	if (H1)
	{
		auto& Hv = H1.value();
		Hv = arm_->getPhi_q_dense();
	}

	return err;

	MRPT_END
}

/*+-------------------------------------------------------------------------+
  |            Multi Body State Estimation (mbse) C++ library               |
  |                                                                         |
  | Copyright (C) 2014-2020 University of Almeria                           |
  | Copyright (C) 2020 University of Salento                                |
  | See README for list of authors and papers                               |
  | Distributed under 3-clause BSD license                                  |
  |  See: <https://opensource.org/licenses/BSD-3-Clause>                    |
  +-------------------------------------------------------------------------+ */

#pragma once

#include <mbse/mbse-common.h>
#include <mrpt/opengl/CRenderizable.h>

namespace mbse
{
/** 2D generic body */
struct CBody
{
	CBody();  //!< Default ctor.

	std::string name;

	/** A 2D body is defined (in natural coords) with 2 points
	 * Indices of the body 2 points (from the list of all
	 * points in the problem); may include one fixed point
	 * (not a variable) */
	size_t points[2];

	/** In (kg) */
	inline double mass() const { return mass_; }
	inline double& mass()
	{
		mass_matrices_cached_ = false;
		return mass_;
	}

	/** Center of gravity (in local coordinates, origin=first point) */
	inline mrpt::math::TPoint2D cog() const { return cog_; }
	inline mrpt::math::TPoint2D& cog()
	{
		mass_matrices_cached_ = false;
		return cog_;
	}

	/** Fixed length (distance) between points 0-1 (constant since this is a
	 * rigid body) */
	inline double length() const { return length_; }
	inline double& length()
	{
		mass_matrices_cached_ = false;
		return length_;
	}

	/** Moment of inertia wrt point 0 */
	inline double I0() const { return I0_; }
	inline double& I0()
	{
		mass_matrices_cached_ = false;
		return I0_;
	}

	/** Computes the 3 different 2x2 blocks of the 4x4 mass matrix of a generic
	 * planar rigid element:
	 * \code
	 *     [ M00   |  M01  ]
	 * M = [ ------+------ ]
	 *     [ M01^t |  M11  ]
	 * \endcode
	 */
	void evaluateMassMatrix(
		Eigen::Matrix2d& M00, Eigen::Matrix2d& M11, Eigen::Matrix2d& M01) const;

	/** Computes (or gets cached) mass mat. */
	const Eigen::Matrix2d& getM00() const;
	/** Computes (or gets cached) mass mat. */
	const Eigen::Matrix2d& getM11() const;
	/** Computes (or gets cached) mass mat. */
	const Eigen::Matrix2d& getM01() const;

   private:
	/** Cached versions of mass submatrices, stored here after calling
	 * evaluateMassMatrix() */
	mutable Eigen::Matrix2d M00_, M11_, M01_;
	mutable bool mass_matrices_cached_;

	/** Computes the 3 different 2x2 blocks of the 4x4 mass matrix of a generic
	 * planar rigid element */
	void internal_update_mass_submatrices() const;

	double mass_;  //!< In (kg)
	mrpt::math::TPoint2D cog_;  //!< Center of gravity (in local coordinates,
								 //!< origin=first point)
	double length_;  //!< Fixed length (distance) between points 0-1 (constant
					  //!< since this is a rigid body)
	double I0_;  //!< Moment of inertia wrt point 0

   public:
	/**  Creates a 3D representation of the body */
	mrpt::opengl::CRenderizable::Ptr get3DRepresentation() const;

	/** Type of 3D object in which the body will be converted */
	enum render_style_t : uint8_t
	{
		reLine = 0,  //<! A simple line
		reCylinder  //<! A cylinder
	};

	struct TRenderParams
	{
		TRenderParams() = default;

		render_style_t render_style = reCylinder;  //!< Kind of object

		/* ==== Common options  ==== */
		bool show_grounds = true;  //!< Draws ground points as independent
								   //!< "ground solids"
		double z_layer = 0;  //!< Emulates links in "layers": an increment to be
							 //!< added to the Z coordinate of the object.

		/* ==== Render as lines ==== */
		uint8_t line_alpha = 0x8f;  //!< Transparency (0x00 - 0xff)
		float line_width = 1.0f;  //!< Line width (in pixels)

		/* ====  Render as cylinder ====  */
		double cyl_diameter = 0.05;
	};

	TRenderParams render_params;

	// EIGEN_MAKE_ALIGNED_OPERATOR_NEW    // Required for aligned mem allocator
	// (only needed in classes containing fixed-size Eigen matrices)
};

}  // namespace mbse

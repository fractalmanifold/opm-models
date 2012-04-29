// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2010 by Katherina Baber, Klaus Mosthaf                    *
 *   Copyright (C) 2008-2009 by Bernd Flemisch, Andreas Lauser               *
 *   Institute for Modelling Hydraulic and Environmental Systems             *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/
/*!
 * \ingroup Properties
 * \ingroup BoxProperties
 * \ingroup BoxStokesModel
 *
 * \file
 *
 * \brief Defines the properties required for the Stokes box model.
 */

#ifndef DUMUX_STOKESPROPERTIES_HH
#define DUMUX_STOKESPROPERTIES_HH

#include <dumux/boxmodels/common/boxproperties.hh>

namespace Dumux
{

namespace Properties
{
//////////////////////////////////////////////////////////////////
// Type tags
//////////////////////////////////////////////////////////////////

//! The type tag for the stokes problems
NEW_TYPE_TAG(BoxStokes, INHERITS_FROM(BoxModel));

//////////////////////////////////////////////////////////////////
// Property tags
//////////////////////////////////////////////////////////////////
NEW_PROP_TAG(Indices); //!< Enumerations used by the model

//NEW_PROP_TAG(Soil); //!< The type of the soil properties object
NEW_PROP_TAG(EnableGravity); //!< Returns whether gravity is considered in the problem
NEW_PROP_TAG(MassUpwindWeight); //!< The value of the upwind parameter for the mobility
NEW_PROP_TAG(StokesIndices); //!< Enumerations for the Stokes models
NEW_PROP_TAG(Indices); //!< Enumerations for the Stokes models accessible using a generic name
NEW_PROP_TAG(Fluid);
NEW_PROP_TAG(FluidSystem);
NEW_PROP_TAG(FluidState);

NEW_PROP_TAG(StokesPhaseIndex); //!< The index of the considered fluid phase

NEW_PROP_TAG(BaseProblem); //!< The type of the base class for problems using the stokes model
}
}

#endif

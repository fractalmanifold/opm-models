// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2008-2009 by Klaus Mosthaf                                *
 *   Copyright (C) 2008-2011 by Andreas Lauser                               *
 *   Copyright (C) 2008-2009 by Bernd Flemisch                               *
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
 * \file
 *
 * \brief Definition of a problem, where air is injected under a low permeable layer.
 */
#ifndef DUMUX_INJECTION_PROBLEM_HH
#define DUMUX_INJECTION_PROBLEM_HH

#include <dune/grid/io/file/dgfparser/dgfs.hh>

#include <dumux/boxmodels/2p2c/2p2cmodel.hh>
#include <dumux/material/fluidsystems/h2on2fluidsystem.hh>

#include <dumux/material/fluidmatrixinteractions/2p/linearmaterial.hh>
#include <dumux/material/fluidmatrixinteractions/2p/regularizedbrookscorey.hh>
#include <dumux/material/fluidmatrixinteractions/2p/efftoabslaw.hh>
#include <dumux/material/fluidmatrixinteractions/Mp/2padapter.hh>

#include <dumux/material/heatconduction/somerton.hh>

namespace Dumux
{

template <class TypeTag>
class InjectionProblem;

namespace Properties
{
NEW_TYPE_TAG(InjectionProblem, INHERITS_FROM(BoxTwoPTwoC));

// declare some injection problem specific property tags
NEW_PROP_TAG(FluidSystemPressureLow);
NEW_PROP_TAG(FluidSystemPressureHigh);
NEW_PROP_TAG(FluidSystemNumPressure);
NEW_PROP_TAG(FluidSystemTemperatureLow);
NEW_PROP_TAG(FluidSystemTemperatureHigh);
NEW_PROP_TAG(FluidSystemNumTemperature);

NEW_PROP_TAG(InitialConditionsMaxDepth);
NEW_PROP_TAG(InitialConditionsTemperature);
NEW_PROP_TAG(SimulationControlName);

// Set the grid type
SET_PROP(InjectionProblem, Grid)
{
    typedef Dune::SGrid<2,2> type;
};

// Set the problem property
SET_PROP(InjectionProblem, Problem)
{
    typedef Dumux::InjectionProblem<TypeTag> type;
};

// Set fluid configuration
SET_PROP(InjectionProblem, FluidSystem)
{ private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    static const bool useComplexRelations = false;
public:
    typedef Dumux::FluidSystems::H2ON2<Scalar, useComplexRelations> type;
};

// Set the material Law
SET_PROP(InjectionProblem, MaterialLaw)
{
private:
    // define the material law which is parameterized by effective
    // saturations
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef RegularizedBrooksCorey<Scalar> EffMaterialLaw;
    // define the material law parameterized by absolute saturations
    typedef EffToAbsLaw<EffMaterialLaw> TwoPMaterialLaw;

    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    enum { lPhaseIdx = FluidSystem::lPhaseIdx };

public:
    typedef TwoPAdapter<lPhaseIdx, TwoPMaterialLaw> type;
};

// Set the heat conduction law
SET_PROP(InjectionProblem, HeatConductionLaw)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;

public:
    // define the material law parameterized by absolute saturations
    typedef Dumux::Somerton<FluidSystem::lPhaseIdx, Scalar> type;
};

// Enable gravity
SET_BOOL_PROP(InjectionProblem, EnableGravity, true);

SET_BOOL_PROP(InjectionProblem, EnableJacobianRecycling, true);

// set the defaults for some injection problem specific properties
SET_SCALAR_PROP(InjectionProblem, FluidSystemPressureLow, 1e6);
SET_SCALAR_PROP(InjectionProblem, FluidSystemPressureHigh, 3e7);
SET_INT_PROP(InjectionProblem, FluidSystemNumPressure, 100);
SET_SCALAR_PROP(InjectionProblem, FluidSystemTemperatureLow, 273.15);
SET_SCALAR_PROP(InjectionProblem, FluidSystemTemperatureHigh, 373.15);
SET_INT_PROP(InjectionProblem, FluidSystemNumTemperature, 100);

SET_SCALAR_PROP(InjectionProblem, InitialConditionsMaxDepth, 2500);
SET_SCALAR_PROP(InjectionProblem, InitialConditionsTemperature, 293.15);
SET_STRING_PROP(InjectionProblem, SimulationControlName, "injection");
}


/*!
 * \ingroup TwoPTwoCModel
 * \ingroup BoxTestProblems
 * \brief Problem where air is injected under a low permeable layer in a depth of 2700m.
 *
 * The domain is sized 60m times 40m and consists of two layers, a moderately
 * permeable spatial parameters (\f$ K=10e-12\f$) for \f$ y>22m\f$ and one with a lower permeablility (\f$ K=10e-13\f$)
 * in the rest of the domain.
 *
 * Air enters a water-filled aquifer, which is situated 2700m below sea level, at the right boundary
 * (\f$ 5m<y<15m\f$) and migrates upwards due to buoyancy. It accumulates and
 * partially enters the lower permeable aquitard.
 * This problem uses the \ref TwoPTwoCModel.
 *
 * To run the simulation execute the following line in shell:
 * <tt>./test_2p2c -parameterFile ./test_2p2c.input</tt>
 */
template <class TypeTag>
class InjectionProblem 
    : public GET_PROP_TYPE(TypeTag, BaseProblem)
{
    typedef typename GET_PROP_TYPE(TypeTag, BaseProblem) ParentType;
    
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;

    enum {
        // Grid and world dimension
        dim = GridView::dimension,
        dimWorld = GridView::dimensionworld
    };

    // copy some indices for convenience
    typedef typename GET_PROP_TYPE(TypeTag, TwoPTwoCIndices) Indices;
    enum {
        numPhases = FluidSystem::numPhases,
        numComponents = FluidSystem::numComponents,

        gPhaseIdx = FluidSystem::gPhaseIdx,
        lPhaseIdx = FluidSystem::lPhaseIdx,

        N2Idx = FluidSystem::N2Idx,
        H2OIdx = FluidSystem::H2OIdx,

        conti0EqIdx = Indices::conti0EqIdx,
        contiN2EqIdx = conti0EqIdx + N2Idx,
        contiH2OEqIdx = conti0EqIdx + H2OIdx
    };

    typedef typename GET_PROP_TYPE(TypeTag, PrimaryVariables) PrimaryVariables;
    typedef typename GET_PROP_TYPE(TypeTag, RateVector) RateVector;

    typedef typename GET_PROP_TYPE(TypeTag, MaterialLaw) MaterialLaw;

    typedef typename GET_PROP_TYPE(TypeTag, BoundaryTypes) BoundaryTypes;
    typedef typename GET_PROP_TYPE(TypeTag, TimeManager) TimeManager;

    typedef typename GET_PROP_TYPE(TypeTag, MaterialLawParams) MaterialLawParams;
    typedef typename GET_PROP_TYPE(TypeTag, HeatConductionLaw) HeatConductionLaw;
    typedef typename HeatConductionLaw::Params HeatConductionLawParams;

    typedef Dune::FieldVector<Scalar, dimWorld> GlobalPosition;

    typedef Dune::FieldVector<Scalar, numComponents> ComponentVector;
    typedef Dune::FieldVector<Scalar, numPhases> PhaseVector;

public:
    /*!
     * \brief The constructor
     *
     * \param timeManager The time manager
     * \param gridView The grid view
     */
    InjectionProblem(TimeManager &timeManager)
        : ParentType(timeManager, GET_PROP_TYPE(TypeTag, GridCreator)::grid().leafView())
    {      
        eps_ = 1e-6;

        temperatureLow_ = GET_PARAM_FROM_GROUP(TypeTag, Scalar, FluidSystem, TemperatureLow);
        temperatureHigh_ = GET_PARAM_FROM_GROUP(TypeTag, Scalar, FluidSystem, TemperatureHigh);
        nTemperature_ = GET_PARAM_FROM_GROUP(TypeTag, int, FluidSystem, NumTemperature);

        nPressure_ = GET_PARAM_FROM_GROUP(TypeTag, int, FluidSystem, NumPressure);
        pressureLow_ = GET_PARAM_FROM_GROUP(TypeTag, Scalar, FluidSystem, PressureLow);
        pressureHigh_ = GET_PARAM_FROM_GROUP(TypeTag, Scalar, FluidSystem, PressureHigh);
        
        temperature_ = GET_PARAM_FROM_GROUP(TypeTag, Scalar, InitialConditions, Temperature);
        maxDepth_ = GET_PARAM_FROM_GROUP(TypeTag, Scalar, InitialConditions, MaxDepth);
        name_ = GET_PARAM_FROM_GROUP(TypeTag, std::string, SimulationControl, Name);

        // initialize the tables of the fluid system
        //FluidSystem::init();
        FluidSystem::init(/*Tmin=*/temperatureLow_,
                          /*Tmax=*/temperatureHigh_,
                          /*nT=*/nTemperature_,
                          /*pmin=*/pressureLow_,
                          /*pmax=*/pressureHigh_,
                          /*np=*/nPressure_);

        
        layerBottom_ = 22.0;

        // intrinsic permeabilities
        fineK_ = 1e-13;
        coarseK_ = 1e-12;

        // porosities
        finePorosity_ = 0.3;
        coarsePorosity_ = 0.3;

        // residual saturations
        fineMaterialParams_.setSwr(0.2);
        fineMaterialParams_.setSnr(0.0);
        coarseMaterialParams_.setSwr(0.2);
        coarseMaterialParams_.setSnr(0.0);

        // parameters for the Brooks-Corey law
        fineMaterialParams_.setPe(1e4);
        coarseMaterialParams_.setPe(1e4);
        fineMaterialParams_.setLambda(2.0);
        coarseMaterialParams_.setLambda(2.0);

        // parameters for the somerton law of heat conduction
        computeHeatCondParams_(fineHeatCondParams_, finePorosity_);
        computeHeatCondParams_(coarseHeatCondParams_, coarsePorosity_);
    }

    /*!
     * \brief Called directly after the time integration.
     */
    void postTimeStep()
    {
        // Calculate storage terms
        PrimaryVariables storageL, storageG;
        this->model().globalPhaseStorage(storageL, /*phaseIdx=*/0);
        this->model().globalPhaseStorage(storageG, /*phaseIdx=*/1);

        // Write mass balance information for rank 0
        if (this->gridView().comm().rank() == 0) {
            std::cout<<"Storage: liquid=[" << storageL << "]"
                     << " gas=[" << storageG << "]\n";
        }
    }

    /*!
     * \brief Apply the intrinsic permeability tensor to a pressure
     *        potential gradient.
     *
     * \param element The current finite element
     * \param fvElemGeom The current finite volume geometry of the element
     * \param scvIdx The index of the sub-control volume
     */
    template <class Context>
    const Scalar intrinsicPermeability(const Context &context, int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &pos = context.pos(spaceIdx, timeIdx);
        if (isFineMaterial_(pos))
            return fineK_;
        return coarseK_;
    }

    /*!
     * \brief Define the porosity \f$[-]\f$ of the spatial parameters
     *
     * \param element The finite element
     * \param fvElemGeom The finite volume geometry
     * \param scvIdx The local index of the sub-control volume where
     *                    the porosity needs to be defined
     */
    template <class Context>
    Scalar porosity(const Context &context, int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &pos = context.pos(spaceIdx, timeIdx);
        if (isFineMaterial_(pos))
            return finePorosity_;
        return coarsePorosity_;
    }

    /*!
     * \brief Return the parameter object for the Brooks-Corey material law which depends on the position
     *
     * \param element The current finite element
     * \param fvElemGeom The current finite volume geometry of the element
     * \param scvIdx The index of the sub-control volume
     */
    template <class Context>
    const MaterialLawParams& materialLawParams(const Context &context, int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &pos = context.pos(spaceIdx, timeIdx);
        if (isFineMaterial_(pos))
            return fineMaterialParams_;
        return coarseMaterialParams_;
    }

    /*!
     * \brief Returns the volumetric heat capacity \f$[J/m^3 K]\f$ of
     *        the rock matrix.
     *
     * Porosity is _not_ taken into account by this method. This is
     * only required for non-isothermal models.
     *
     * \param element The finite element
     * \param fvElemGeom The finite volume geometry
     * \param scvIdx The local index of the sub-control volume where
     *                    the heat capacity needs to be defined
     */
    template <class Context>
    Scalar heatCapacitySolid(const Context &context, int spaceIdx, int timeIdx) const
    {
        return
            790 // specific heat capacity of granite [J / (kg K)]
            * 2700; // density of granite [kg/m^3]
    }

    /*!
     * \brief Return the parameter object for the heat conductivty law
     *        for a given position
     */
    template <class Context>
    const HeatConductionLawParams&
    heatConductionParams(const Context &context, int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &pos = context.pos(spaceIdx, timeIdx);
        if (isFineMaterial_(pos))
            return fineHeatCondParams_;
        return coarseHeatCondParams_;
    }

    /*!
     * \name Problem parameters
     */
    // \{

    /*!
     * \brief The problem name.
     *
     * This is used as a prefix for files generated by the simulation.
     */
    const std::string name() const
    { return name_; }

    /*!
     * \brief Returns the temperature within the domain.
     *
     * This problem assumes a temperature of 10 degrees Celsius.
     */
    template <class Context>
    Scalar temperature(const Context &context, int spaceIdx, int timeIdx) const
    { return temperature_; };

    template <class Context>
    void source(RateVector &values,
                const Context &context,
                int spaceIdx, int timeIdx) const
    { values = 0; }

    // \}

    /*!
     * \name Boundary conditions
     */
    // \{

    /*!
     * \brief Specifies which kind of boundary condition should be
     *        used for which equation on a given boundary segment.
     *
     * \param values The boundary types for the conservation equations
     * \param vertex The vertex for which the boundary type is set
     */
    template <class Context>
    void boundaryTypes(BoundaryTypes &values, const Context &context, int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &globalPos = context.pos(spaceIdx, timeIdx);

        if (globalPos[0] < eps_)
            values.setAllDirichlet();
        else
            values.setAllNeumann();
    }

    /*!
     * \brief Evaluate the boundary conditions for a dirichlet
     *        boundary segment.
     *
     * \param values The dirichlet values for the primary variables
     * \param vertex The vertex for which the boundary type is set
     *
     * For this method, the \a values parameter stores primary variables.
     */
    template <class Context>
    void dirichlet(PrimaryVariables &values, const Context &context, int spaceIdx, int timeIdx) const
    { initial(values, context, spaceIdx, timeIdx); }

    /*!
     * \brief Evaluate the boundary conditions for a neumann
     *        boundary segment.
     *
     * For this method, the \a values parameter stores the mass flux
     * in normal direction of each phase. Negative values mean influx.
     */
    template <class Context>
    void neumann(RateVector &values,
                 const Context &context,
                 int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &globalPos = context.pos(spaceIdx, timeIdx);

        RateVector massRate(0.0);
        if (globalPos[1] < 15 && globalPos[1] > 5) {
            massRate[contiN2EqIdx] = -1e-3; // kg/(s*m^2)
        }

        values = massRate;
        //values.setMassRate(massRate);
    }

    // \}

    /*!
     * \name Volume terms
     */
    // \{

    /*!
     * \brief Evaluate the initial value for a control volume.
     *
     * \param values The initial values for the primary variables
     * \param element The finite element
     * \param fvElemGeom The finite-volume geometry in the box scheme
     * \param scvIdx The local vertex index
     *
     * For this method, the \a values parameter stores primary
     * variables.
     */
    template <class Context>
    void initial(PrimaryVariables &values, const Context &context, int spaceIdx, int timeIdx) const
    {
        const GlobalPosition &globalPos = context.pos(spaceIdx, timeIdx);

        Dumux::CompositionalFluidState<Scalar, FluidSystem> fs;

        
        //////
        // set temperatures
        //////
        fs.setTemperature(temperature_);
        
        //////
        // set saturations
        //////
        fs.setSaturation(FluidSystem::lPhaseIdx, 1.0);
        fs.setSaturation(FluidSystem::gPhaseIdx, 0.0);

        //////
        // set pressures
        //////
        Scalar densityL = FluidSystem::H2O::liquidDensity(temperature_, 1e5);
        Scalar pl = 1e5 - densityL*this->gravity()[1]*(maxDepth_ - globalPos[1]);
        PhaseVector pC;
        const auto &matParams = this->materialLawParams(context, spaceIdx, timeIdx);
        MaterialLaw::capillaryPressures(pC, matParams, fs);

        fs.setPressure(FluidSystem::lPhaseIdx, pl + (pC[lPhaseIdx] - pC[lPhaseIdx]));
        fs.setPressure(FluidSystem::gPhaseIdx, pl + (pC[gPhaseIdx] - pC[lPhaseIdx]));

        //////
        // set composition of the liquid phase
        //////
        fs.setMoleFraction(lPhaseIdx, N2Idx,
                           pl*0.95/
                           BinaryCoeff::H2O_N2::henry(temperature_));
        fs.setMoleFraction(lPhaseIdx, H2OIdx,
                           1.0 - fs.moleFraction(lPhaseIdx, N2Idx));

        //////
        // set composition of the gas phase
        //////
        fs.setMoleFraction(gPhaseIdx, N2Idx, 0.9);
        fs.setMoleFraction(gPhaseIdx, H2OIdx, 0.0);

        //////
        // set the primary variables
        //////
        values.template assignMassConservative<MaterialLaw>(fs, matParams, /*inEquilibrium=*/true);
    }


    void computeHeatCondParams_(HeatConductionLawParams &params, Scalar poro)
    {
        Scalar lambdaWater = 0.6;
        Scalar lambdaGranite = 2.8;

        Scalar lambdaWet = std::pow(lambdaGranite, (1-poro)) * std::pow(lambdaWater, poro);
        Scalar lambdaDry = std::pow(lambdaGranite, (1-poro));

        params.setFullySaturatedLambda(gPhaseIdx, lambdaDry);
        params.setFullySaturatedLambda(lPhaseIdx, lambdaWet);
    }

    bool isFineMaterial_(const GlobalPosition &pos) const
    { return pos[dim-1] > layerBottom_; };

    Scalar fineK_;
    Scalar coarseK_;
    Scalar layerBottom_;

    Scalar finePorosity_;
    Scalar coarsePorosity_;

    MaterialLawParams fineMaterialParams_;
    MaterialLawParams coarseMaterialParams_;

    HeatConductionLawParams fineHeatCondParams_;
    HeatConductionLawParams coarseHeatCondParams_;

    Scalar temperature_;
    Scalar maxDepth_;
    Scalar eps_;

    int nTemperature_;
    int nPressure_;

    std::string name_ ;

    Scalar pressureLow_, pressureHigh_;
    Scalar temperatureLow_, temperatureHigh_;
};
} //end namespace

#endif

// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \copydoc Opm::BlackOilLocalResidual
 */
#ifndef EWOMS_BLACK_OIL_LOCAL_TPFA_RESIDUAL_HH
#define EWOMS_BLACK_OIL_LOCAL_TPFA_RESIDUAL_HH

#include "blackoilproperties.hh"
#include "blackoilsolventmodules.hh"
#include "blackoilextbomodules.hh"
#include "blackoilpolymermodules.hh"
#include "blackoilenergymodules.hh"
#include "blackoilfoammodules.hh"
#include "blackoilbrinemodules.hh"
#include "blackoildiffusionmodule.hh"
#include "blackoilmicpmodules.hh"
#include <opm/material/fluidstates/BlackOilFluidState.hpp>
#include <opm/input/eclipse/EclipseState/Grid/FaceDir.hpp>


namespace Opm {
/*!
 * \ingroup BlackOilModel
 *
 * \brief Calculates the local residual of the black oil model.
 */
template <class TypeTag>
class BlackOilLocalResidualTPFA : public GetPropType<TypeTag, Properties::DiscLocalResidual>
{
    using IntensiveQuantities = GetPropType<TypeTag, Properties::IntensiveQuantities>;
    using ExtensiveQuantities = GetPropType<TypeTag, Properties::ExtensiveQuantities>;
    using ElementContext = GetPropType<TypeTag, Properties::ElementContext>;
    using Indices = GetPropType<TypeTag, Properties::Indices>;
    using Scalar = GetPropType<TypeTag, Properties::Scalar>;
    using Evaluation = GetPropType<TypeTag, Properties::Evaluation>;
    using EqVector = GetPropType<TypeTag, Properties::EqVector>;
    using RateVector = GetPropType<TypeTag, Properties::RateVector>;
    using FluidSystem = GetPropType<TypeTag, Properties::FluidSystem>;
    using GridView = GetPropType<TypeTag, Properties::GridView>;
    using Problem = GetPropType<TypeTag, Properties::Problem>;
    using FluidState = typename IntensiveQuantities::FluidState;

    enum { conti0EqIdx = Indices::conti0EqIdx };
    enum { numEq = getPropValue<TypeTag, Properties::NumEq>() };
    enum { numPhases = getPropValue<TypeTag, Properties::NumPhases>() };
    enum { numComponents = getPropValue<TypeTag, Properties::NumComponents>() };

    enum { dimWorld = GridView::dimensionworld };
    enum { gasPhaseIdx = FluidSystem::gasPhaseIdx };
    enum { oilPhaseIdx = FluidSystem::oilPhaseIdx };
    enum { waterPhaseIdx = FluidSystem::waterPhaseIdx };

    enum { gasCompIdx = FluidSystem::gasCompIdx };
    enum { oilCompIdx = FluidSystem::oilCompIdx };
    enum { waterCompIdx = FluidSystem::waterCompIdx };
    enum { compositionSwitchIdx = Indices::compositionSwitchIdx };

    static const bool waterEnabled = Indices::waterEnabled;
    static const bool gasEnabled = Indices::gasEnabled;
    static const bool oilEnabled = Indices::oilEnabled;
    static const bool compositionSwitchEnabled = (compositionSwitchIdx >= 0);

    static constexpr bool blackoilConserveSurfaceVolume = getPropValue<TypeTag, Properties::BlackoilConserveSurfaceVolume>();

    static constexpr bool enableSolvent = getPropValue<TypeTag, Properties::EnableSolvent>();
    static constexpr bool enableExtbo = getPropValue<TypeTag, Properties::EnableExtbo>();
    static constexpr bool enablePolymer = getPropValue<TypeTag, Properties::EnablePolymer>();
    static constexpr bool enableEnergy = getPropValue<TypeTag, Properties::EnableEnergy>();
    static constexpr bool enableFoam = getPropValue<TypeTag, Properties::EnableFoam>();
    static constexpr bool enableBrine = getPropValue<TypeTag, Properties::EnableBrine>();
    static constexpr bool enableDiffusion = getPropValue<TypeTag, Properties::EnableDiffusion>();
    static constexpr bool enableMICP = getPropValue<TypeTag, Properties::EnableMICP>();

    using SolventModule = BlackOilSolventModule<TypeTag>;
    using ExtboModule = BlackOilExtboModule<TypeTag>;
    using PolymerModule = BlackOilPolymerModule<TypeTag>;
    using EnergyModule = BlackOilEnergyModule<TypeTag>;
    using FoamModule = BlackOilFoamModule<TypeTag>;
    using BrineModule = BlackOilBrineModule<TypeTag>;
    using DiffusionModule = BlackOilDiffusionModule<TypeTag, enableDiffusion>;
    using MICPModule = BlackOilMICPModule<TypeTag>;

    using Toolbox = MathToolbox<Evaluation>;

public:
    /*!
     * \copydoc FvBaseLocalResidual::computeStorage
     */
    template <class LhsEval>
    void computeStorage(Dune::FieldVector<LhsEval, numEq>& storage,
                        const ElementContext& elemCtx,
                        unsigned dofIdx,
                        unsigned timeIdx) const
    {
        const IntensiveQuantities& intQuants = elemCtx.intensiveQuantities(dofIdx, timeIdx);
        computeStorage(storage,
                       intQuants);
    }

    template <class LhsEval>
    static void computeStorage(Dune::FieldVector<LhsEval, numEq>& storage,
                               const IntensiveQuantities& intQuants)
    {
        OPM_TIMEBLOCK_LOCAL(computeStorage);
        // retrieve the intensive quantities for the SCV at the specified point in time
        const auto& fs = intQuants.fluidState();
        storage = 0.0;

        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            if (!FluidSystem::phaseIsActive(phaseIdx)) {
                continue;
            }
            unsigned activeCompIdx = Indices::canonicalToActiveComponentIndex(FluidSystem::solventComponentIndex(phaseIdx));
            LhsEval surfaceVolume =
                Toolbox::template decay<LhsEval>(fs.saturation(phaseIdx))
                * Toolbox::template decay<LhsEval>(fs.invB(phaseIdx))
                * Toolbox::template decay<LhsEval>(intQuants.porosity());

            storage[conti0EqIdx + activeCompIdx] += surfaceVolume;

            // account for dissolved gas
            if (phaseIdx == oilPhaseIdx && FluidSystem::enableDissolvedGas()) {
                unsigned activeGasCompIdx = Indices::canonicalToActiveComponentIndex(gasCompIdx);
                storage[conti0EqIdx + activeGasCompIdx] +=
                    Toolbox::template decay<LhsEval>(intQuants.fluidState().Rs())
                    * surfaceVolume;
            }

            // account for dissolved gas in water
            if (phaseIdx == waterPhaseIdx && FluidSystem::enableDissolvedGasInWater()) {
                unsigned activeGasCompIdx = Indices::canonicalToActiveComponentIndex(gasCompIdx);
                storage[conti0EqIdx + activeGasCompIdx] +=
                    Toolbox::template decay<LhsEval>(intQuants.fluidState().Rsw())
                    * surfaceVolume;
            }

            // account for vaporized oil
            if (phaseIdx == gasPhaseIdx && FluidSystem::enableVaporizedOil()) {
                unsigned activeOilCompIdx = Indices::canonicalToActiveComponentIndex(oilCompIdx);
                storage[conti0EqIdx + activeOilCompIdx] +=
                    Toolbox::template decay<LhsEval>(intQuants.fluidState().Rv())
                    * surfaceVolume;
            }

            // account for vaporized water
            if (phaseIdx == gasPhaseIdx && FluidSystem::enableVaporizedWater()) {
                unsigned activeWaterCompIdx = Indices::canonicalToActiveComponentIndex(waterCompIdx);
                storage[conti0EqIdx + activeWaterCompIdx] +=
                    Toolbox::template decay<LhsEval>(intQuants.fluidState().Rvw())
                    * surfaceVolume;
            }
        }

        adaptMassConservationQuantities_(storage, intQuants.pvtRegionIndex());

        // deal with solvents (if present)
        SolventModule::addStorage(storage, intQuants);

        // deal with zFracton (if present)
        ExtboModule::addStorage(storage, intQuants);

        // deal with polymer (if present)
        PolymerModule::addStorage(storage, intQuants);

        // deal with energy (if present)
        EnergyModule::addStorage(storage, intQuants);

        // deal with foam (if present)
        FoamModule::addStorage(storage, intQuants);

        // deal with salt (if present)
        BrineModule::addStorage(storage, intQuants);

        // deal with micp (if present)
        MICPModule::addStorage(storage, intQuants);
    }

    /*!
     * This function works like the ElementContext-based version with
     * one main difference: The darcy flux is calculated here, not
     * read from the extensive quantities of the element context.
     */
    static void computeFlux(RateVector& flux,
                            RateVector& darcy,
                            const Problem& problem,
                            const unsigned globalIndexIn,
                            const unsigned globalIndexEx,
                            const IntensiveQuantities& intQuantsIn,
                            const IntensiveQuantities& intQuantsEx,
                            const Scalar trans,
                            const Scalar faceArea,
                            const FaceDir::DirEnum facedir)
    {
        OPM_TIMEBLOCK_LOCAL(computeFlux);
        flux = 0.0;
        darcy = 0.0;
        Scalar Vin = problem.model().dofTotalVolume(globalIndexIn);
        Scalar Vex = problem.model().dofTotalVolume(globalIndexEx);

        Scalar thpres = problem.thresholdPressure(globalIndexIn, globalIndexEx);

        // estimate the gravity correction: for performance reasons we use a simplified
        // approach for this flux module that assumes that gravity is constant and always
        // acts into the downwards direction. (i.e., no centrifuge experiments, sorry.)
        Scalar g = problem.gravity()[dimWorld - 1];

        // this is quite hacky because the dune grid interface does not provide a
        // cellCenterDepth() method (so we ask the problem to provide it). The "good"
        // solution would be to take the Z coordinate of the element centroids, but since
        // ECL seems to like to be inconsistent on that front, it needs to be done like
        // here...
        Scalar zIn = problem.dofCenterDepth(globalIndexIn);
        Scalar zEx = problem.dofCenterDepth(globalIndexEx);

        // the distances from the DOF's depths. (i.e., the additional depth of the
        // exterior DOF)
        Scalar distZ = zIn - zEx; // NB could be precalculated

        calculateFluxes_(flux,
                         darcy,
                         intQuantsIn,
                         intQuantsEx,
                         Vin,
                         Vex,
                         globalIndexIn,
                         globalIndexEx,
                         distZ * g,
                         thpres,
                         trans,
                         faceArea,
                         facedir);
    }

    // This function demonstrates compatibility with the ElementContext-based interface.
    // Actually using it will lead to double work since the element context already contains
    // fluxes through its stored ExtensiveQuantities.
    static void computeFlux(RateVector& flux,
                            const ElementContext& elemCtx,
                            unsigned scvfIdx,
                            unsigned timeIdx)
    {
        OPM_TIMEBLOCK_LOCAL(computeFlux);              
        assert(timeIdx == 0);

        flux = 0.0;
        RateVector darcy = 0.0;
        // need for dary flux calculation
        const auto& problem = elemCtx.problem();
        const auto& stencil = elemCtx.stencil(timeIdx);
        const auto& scvf = stencil.interiorFace(scvfIdx);

        unsigned interiorDofIdx = scvf.interiorIndex();
        unsigned exteriorDofIdx = scvf.exteriorIndex();
        assert(interiorDofIdx != exteriorDofIdx);

        // unsigned I = stencil.globalSpaceIndex(interiorDofIdx);
        // unsigned J = stencil.globalSpaceIndex(exteriorDofIdx);
        Scalar Vin = elemCtx.dofVolume(interiorDofIdx, /*timeIdx=*/0);
        Scalar Vex = elemCtx.dofVolume(exteriorDofIdx, /*timeIdx=*/0);
        const auto& globalIndexIn = stencil.globalSpaceIndex(interiorDofIdx);
        const auto& globalIndexEx = stencil.globalSpaceIndex(exteriorDofIdx);
        Scalar trans = problem.transmissibility(elemCtx, interiorDofIdx, exteriorDofIdx);
        Scalar faceArea = scvf.area();
        const auto& materialLawManager = problem.materialLawManager();
        FaceDir::DirEnum facedir = FaceDir::DirEnum::Unknown; // Use an arbitrary
        if (materialLawManager->hasDirectionalRelperms()) {
            facedir = scvf.faceDirFromDirId();
        }
        Scalar thpres = problem.thresholdPressure(globalIndexIn, globalIndexEx);

        // estimate the gravity correction: for performance reasons we use a simplified
        // approach for this flux module that assumes that gravity is constant and always
        // acts into the downwards direction. (i.e., no centrifuge experiments, sorry.)
        Scalar g = problem.gravity()[dimWorld - 1];
        const auto& intQuantsIn = elemCtx.intensiveQuantities(interiorDofIdx, timeIdx);
        const auto& intQuantsEx = elemCtx.intensiveQuantities(exteriorDofIdx, timeIdx);

        // this is quite hacky because the dune grid interface does not provide a
        // cellCenterDepth() method (so we ask the problem to provide it). The "good"
        // solution would be to take the Z coordinate of the element centroids, but since
        // ECL seems to like to be inconsistent on that front, it needs to be done like
        // here...
        Scalar zIn = problem.dofCenterDepth(elemCtx, interiorDofIdx, timeIdx);
        Scalar zEx = problem.dofCenterDepth(elemCtx, exteriorDofIdx, timeIdx);

        // the distances from the DOF's depths. (i.e., the additional depth of the
        // exterior DOF)
        Scalar distZ = zIn - zEx;

        calculateFluxes_(flux,
                         darcy,
                         intQuantsIn,
                         intQuantsEx,
                         Vin,
                         Vex,
                         globalIndexIn,
                         globalIndexEx,
                         distZ * g,
                         thpres,
                         trans,
                         faceArea,
                         facedir);
    }

    static void calculateFluxes_(RateVector& flux,
                                 RateVector& darcy,
                                 const IntensiveQuantities& intQuantsIn,
                                 const IntensiveQuantities& intQuantsEx,
                                 const Scalar& Vin,
                                 const Scalar& Vex,
                                 const unsigned& globalIndexIn,
                                 const unsigned& globalIndexEx,
                                 const Scalar& distZg,
                                 const Scalar& thpres,
                                 const Scalar& trans,
                                 const Scalar& faceArea,
                                 const FaceDir::DirEnum facedir)
    {
        OPM_TIMEBLOCK_LOCAL(calculateFluxes);
        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            if (!FluidSystem::phaseIsActive(phaseIdx))
                continue;
            // darcy flux calculation
            short dnIdx;
            //
            short upIdx;
            // fake intices should only be used to get upwind anc compatibility with old functions
            short interiorDofIdx = 0; // NB
            short exteriorDofIdx = 1; // NB
            Evaluation pressureDifference;
            ExtensiveQuantities::calculatePhasePressureDiff_(upIdx,
                                                             dnIdx,
                                                             pressureDifference,
                                                             intQuantsIn,
                                                             intQuantsEx,
                                                             phaseIdx, // input
                                                             interiorDofIdx, // input
                                                             exteriorDofIdx, // intput
                                                             Vin,
                                                             Vex,
                                                             globalIndexIn,
                                                             globalIndexEx,
                                                             distZg,
                                                             thpres);



            const IntensiveQuantities& up = (upIdx == interiorDofIdx) ? intQuantsIn : intQuantsEx;
            unsigned globalUpIndex = (upIdx == interiorDofIdx) ? globalIndexIn : globalIndexEx;
            const Evaluation& transMult = up.rockCompTransMultiplier();
            Evaluation darcyFlux;
            if (pressureDifference == 0) {
                darcyFlux = 0.0; // NB maybe we could drop calculations
            } else {
                if (globalUpIndex == globalIndexIn)
                    darcyFlux = pressureDifference * up.mobility(phaseIdx, facedir) * transMult * (-trans / faceArea);
                else
                    darcyFlux = pressureDifference *
                       (Toolbox::value(up.mobility(phaseIdx, facedir)) * Toolbox::value(transMult) * (-trans / faceArea));
            }
            unsigned activeCompIdx = Indices::canonicalToActiveComponentIndex(FluidSystem::solventComponentIndex(phaseIdx));
            darcy[conti0EqIdx + activeCompIdx] = darcyFlux.value() * faceArea; // For the FLORES fluxes

            unsigned pvtRegionIdx = up.pvtRegionIndex();
            // if (upIdx == globalFocusDofIdx){
            if (globalUpIndex == globalIndexIn) {
                const auto& invB
                    = getInvB_<FluidSystem, FluidState, Evaluation>(up.fluidState(), phaseIdx, pvtRegionIdx);
                const auto& surfaceVolumeFlux = invB * darcyFlux;
                evalPhaseFluxes_<Evaluation, Evaluation, FluidState>(
                    flux, phaseIdx, pvtRegionIdx, surfaceVolumeFlux, up.fluidState());
            } else {
                const auto& invB = getInvB_<FluidSystem, FluidState, Scalar>(up.fluidState(), phaseIdx, pvtRegionIdx);
                const auto& surfaceVolumeFlux = invB * darcyFlux;
                evalPhaseFluxes_<Scalar, Evaluation, FluidState>(
                    flux, phaseIdx, pvtRegionIdx, surfaceVolumeFlux, up.fluidState());
            }
        }

        // deal with solvents (if present)
        static_assert(!enableSolvent, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // SolventModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with zFracton (if present)
        static_assert(!enableExtbo, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // ExtboModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with polymer (if present)
        static_assert(!enablePolymer, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // PolymerModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with energy (if present)
        static_assert(!enableEnergy, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // EnergyModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with foam (if present)
        static_assert(!enableFoam, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // FoamModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with salt (if present)
        static_assert(!enableBrine, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // BrineModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with diffusion (if present)
        static_assert(!enableDiffusion, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // DiffusionModule::addDiffusiveFlux(flux, elemCtx, scvfIdx, timeIdx);

        // deal with micp (if present)
        static_assert(!enableMICP, "Relevant computeFlux() method must be implemented for this module before enabling.");
        // MICPModule::computeFlux(flux, elemCtx, scvfIdx, timeIdx);

    }

    template <class BoundaryConditionData>
    static void computeBoundaryFlux(RateVector& bdyFlux,
                                    const Problem& problem,
                                    const BoundaryConditionData& bdyInfo,
                                    const IntensiveQuantities& insideIntQuants,
                                    unsigned globalSpaceIdx)
    {
        if (bdyInfo.type == BCType::RATE) {
            computeBoundaryFluxRate(bdyFlux, bdyInfo);
        } else if (bdyInfo.type == BCType::FREE || bdyInfo.type == BCType::DIRICHLET) {
            computeBoundaryFluxFree(problem, bdyFlux, bdyInfo, insideIntQuants, globalSpaceIdx);
        } else {
            throw std::logic_error("Unknown boundary condition type " + std::to_string(static_cast<int>(bdyInfo.type)) + " in computeBoundaryFlux()." );
        }
    }

    template <class BoundaryConditionData>
    static void computeBoundaryFluxRate(RateVector& bdyFlux,
                                        const BoundaryConditionData& bdyInfo)
    {
        bdyFlux.setMassRate(bdyInfo.massRate, bdyInfo.pvtRegionIdx);
    }

    template <class BoundaryConditionData>
    static void computeBoundaryFluxFree(const Problem& problem,
                                        RateVector& bdyFlux,
                                        const BoundaryConditionData& bdyInfo,
                                        const IntensiveQuantities& insideIntQuants,
                                        unsigned globalSpaceIdx)
    {
        OPM_TIMEBLOCK_LOCAL(computeBoundaryFluxFree);
        std::array<short, numPhases> upIdx;
        std::array<short, numPhases> dnIdx;
        RateVector volumeFlux;
        RateVector pressureDifference;
        ExtensiveQuantities::calculateBoundaryGradients_(problem,
                                                         globalSpaceIdx,
                                                         insideIntQuants,
                                                         bdyInfo.boundaryFaceIndex,
                                                         bdyInfo.faceArea,
                                                         bdyInfo.faceZCoord,
                                                         bdyInfo.exFluidState,
                                                         upIdx,
                                                         dnIdx,
                                                         volumeFlux,
                                                         pressureDifference);

        ////////
        // advective fluxes of all components in all phases
        ////////
        bdyFlux = 0.0;
        for (unsigned phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            if (!FluidSystem::phaseIsActive(phaseIdx)) {
                continue;
            }
            const auto& pBoundary = bdyInfo.exFluidState.pressure(phaseIdx);
            const Evaluation& pInside = insideIntQuants.fluidState().pressure(phaseIdx);
            const unsigned pvtRegionIdx = insideIntQuants.pvtRegionIndex();

            RateVector tmp;

            // mass conservation
            if (pBoundary < pInside) {
                // outflux
                const auto& invB = getInvB_<FluidSystem, FluidState, Evaluation>(insideIntQuants.fluidState(), phaseIdx, pvtRegionIdx);
                Evaluation surfaceVolumeFlux = invB * volumeFlux[phaseIdx];
                evalPhaseFluxes_<Evaluation>(tmp,
                                             phaseIdx,
                                             insideIntQuants.pvtRegionIndex(),
                                             surfaceVolumeFlux,
                                             insideIntQuants.fluidState());
            } else if (pBoundary > pInside) {
                // influx
                using ScalarFluidState = decltype(bdyInfo.exFluidState);
                const auto& invB = getInvB_<FluidSystem, ScalarFluidState, Scalar>(bdyInfo.exFluidState, phaseIdx, pvtRegionIdx);
                Evaluation surfaceVolumeFlux = invB * volumeFlux[phaseIdx];
                evalPhaseFluxes_<Scalar>(tmp,
                                         phaseIdx,
                                         insideIntQuants.pvtRegionIndex(),
                                         surfaceVolumeFlux,
                                         bdyInfo.exFluidState);
            }

            for (unsigned i = 0; i < tmp.size(); ++i) {
                bdyFlux[i] += tmp[i];
            }

            static_assert(!enableEnergy, "Relevant treatment of boundary conditions must be implemented before enabling.");
            // Add energy flux treatment per phase here.
        }

        static_assert(!enableSolvent, "Relevant treatment of boundary conditions must be implemented before enabling.");
        static_assert(!enablePolymer, "Relevant treatment of boundary conditions must be implemented before enabling.");
        static_assert(!enableMICP, "Relevant treatment of boundary conditions must be implemented before enabling.");

        // make sure that the right mass conservation quantities are used
        adaptMassConservationQuantities_(bdyFlux, insideIntQuants.pvtRegionIndex());

        // heat conduction
        static_assert(!enableEnergy, "Relevant treatment of boundary conditions must be implemented before enabling.");

#ifndef NDEBUG
        for (unsigned i = 0; i < numEq; ++i) {
            Valgrind::CheckDefined(bdyFlux[i]);
        }
        Valgrind::CheckDefined(bdyFlux);
#endif
    }

    static void computeSource(RateVector& source,
                              const Problem& problem,
                              unsigned globalSpaceIdex,
                              unsigned timeIdx)
    {
        OPM_TIMEBLOCK_LOCAL(computeSource);
        // retrieve the source term intrinsic to the problem
        problem.source(source, globalSpaceIdex, timeIdx);

        // deal with MICP (if present)
        // deal with micp (if present)
        static_assert(!enableMICP, "Relevant addSource() method must be implemented for this module before enabling.");
        // MICPModule::addSource(source, elemCtx, dofIdx, timeIdx);

        // scale the source term of the energy equation
        if (enableEnergy)
            source[Indices::contiEnergyEqIdx] *= getPropValue<TypeTag, Properties::BlackOilEnergyScalingFactor>();
    }

    static void computeSourceDense(RateVector& source,
                                   const Problem& problem,
                                   unsigned globalSpaceIdex,
                                   unsigned timeIdx)
    {
        source = 0.0;
        problem.addToSourceDense(source, globalSpaceIdex, timeIdx);

        // deal with MICP (if present)
        // deal with micp (if present)
        static_assert(!enableMICP, "Relevant addSource() method must be implemented for this module before enabling.");
        // MICPModule::addSource(source, elemCtx, dofIdx, timeIdx);

        // scale the source term of the energy equation
        if (enableEnergy)
            source[Indices::contiEnergyEqIdx] *= getPropValue<TypeTag, Properties::BlackOilEnergyScalingFactor>();
    }

    /*!
     * \copydoc FvBaseLocalResidual::computeSource
     */
    void computeSource(RateVector& source,
                       const ElementContext& elemCtx,
                       unsigned dofIdx,
                       unsigned timeIdx) const
    {
        OPM_TIMEBLOCK_LOCAL(computeSource);
        // retrieve the source term intrinsic to the problem
        elemCtx.problem().source(source, elemCtx, dofIdx, timeIdx);

        // deal with MICP (if present)
        MICPModule::addSource(source, elemCtx, dofIdx, timeIdx);

        // scale the source term of the energy equation
        if (enableEnergy)
            source[Indices::contiEnergyEqIdx] *= getPropValue<TypeTag, Properties::BlackOilEnergyScalingFactor>();
    }

    template <class UpEval, class FluidState>
    static void evalPhaseFluxes_(RateVector& flux,
                                 unsigned phaseIdx,
                                 unsigned pvtRegionIdx,
                                 const ExtensiveQuantities& extQuants,
                                 const FluidState& upFs)
    {

        const auto& invB = getInvB_<FluidSystem, FluidState, UpEval>(upFs, phaseIdx, pvtRegionIdx);
        const auto& surfaceVolumeFlux = invB * extQuants.volumeFlux(phaseIdx);
        evalPhaseFluxes_<UpEval>(flux, phaseIdx, pvtRegionIdx, surfaceVolumeFlux, upFs);
    }

    /*!
     * \brief Helper function to calculate the flux of mass in terms of conservation
     *        quantities via specific fluid phase over a face.
     */
    template <class UpEval, class Eval,class FluidState>
    static void evalPhaseFluxes_(RateVector& flux,
                                 unsigned phaseIdx,
                                 unsigned pvtRegionIdx,
                                 const Eval& surfaceVolumeFlux,
                                 const FluidState& upFs)
    {
        unsigned activeCompIdx = Indices::canonicalToActiveComponentIndex(FluidSystem::solventComponentIndex(phaseIdx));

        if (blackoilConserveSurfaceVolume)
            flux[conti0EqIdx + activeCompIdx] += surfaceVolumeFlux;
        else
            flux[conti0EqIdx + activeCompIdx] += surfaceVolumeFlux*FluidSystem::referenceDensity(phaseIdx, pvtRegionIdx);

        if (phaseIdx == oilPhaseIdx) {
            // dissolved gas (in the oil phase).
            if (FluidSystem::enableDissolvedGas()) {
                const auto& Rs = BlackOil::getRs_<FluidSystem, FluidState, UpEval>(upFs, pvtRegionIdx);

                unsigned activeGasCompIdx = Indices::canonicalToActiveComponentIndex(gasCompIdx);
                if (blackoilConserveSurfaceVolume)
                    flux[conti0EqIdx + activeGasCompIdx] += Rs*surfaceVolumeFlux;
                else
                    flux[conti0EqIdx + activeGasCompIdx] += Rs*surfaceVolumeFlux*FluidSystem::referenceDensity(gasPhaseIdx, pvtRegionIdx);
            }
        } else  if (phaseIdx == waterPhaseIdx) {
            // dissolved gas (in the water phase).
            if (FluidSystem::enableDissolvedGasInWater()) {
                const auto& Rsw = BlackOil::getRsw_<FluidSystem, FluidState, UpEval>(upFs, pvtRegionIdx);

                unsigned activeGasCompIdx = Indices::canonicalToActiveComponentIndex(gasCompIdx);
                if (blackoilConserveSurfaceVolume)
                    flux[conti0EqIdx + activeGasCompIdx] += Rsw*surfaceVolumeFlux;
                else
                    flux[conti0EqIdx + activeGasCompIdx] += Rsw*surfaceVolumeFlux*FluidSystem::referenceDensity(gasPhaseIdx, pvtRegionIdx);
            }
        }
        else if (phaseIdx == gasPhaseIdx) {
            // vaporized oil (in the gas phase).
            if (FluidSystem::enableVaporizedOil()) {
                const auto& Rv = BlackOil::getRv_<FluidSystem, FluidState, UpEval>(upFs, pvtRegionIdx);

                unsigned activeOilCompIdx = Indices::canonicalToActiveComponentIndex(oilCompIdx);
                if (blackoilConserveSurfaceVolume)
                    flux[conti0EqIdx + activeOilCompIdx] += Rv*surfaceVolumeFlux;
                else
                    flux[conti0EqIdx + activeOilCompIdx] += Rv*surfaceVolumeFlux*FluidSystem::referenceDensity(oilPhaseIdx, pvtRegionIdx);
            }
             // vaporized water (in the gas phase).
            if (FluidSystem::enableVaporizedWater()) {
                const auto& Rvw = BlackOil::getRvw_<FluidSystem, FluidState, UpEval>(upFs, pvtRegionIdx);

                unsigned activeWaterCompIdx = Indices::canonicalToActiveComponentIndex(waterCompIdx);
                if (blackoilConserveSurfaceVolume)
                    flux[conti0EqIdx + activeWaterCompIdx] += Rvw*surfaceVolumeFlux;
                else
                    flux[conti0EqIdx + activeWaterCompIdx] += Rvw*surfaceVolumeFlux*FluidSystem::referenceDensity(waterPhaseIdx, pvtRegionIdx);
            }
        }
    }

    /*!
     * \brief Helper function to convert the mass-related parts of a Dune::FieldVector
     *        that stores conservation quantities in terms of "surface-volume" to the
     *        conservation quantities used by the model.
     *
     * Depending on the value of the BlackoilConserveSurfaceVolume property, the model
     * either conserves mass by means of "surface volume" of the components or mass
     * directly. In the former case, this method is a no-op; in the latter, the values
     * passed are multiplied by their respective pure component's density at surface
     * conditions.
     */
    template <class Scalar>
    static void adaptMassConservationQuantities_(Dune::FieldVector<Scalar, numEq>& container, unsigned pvtRegionIdx)
    {
        if (blackoilConserveSurfaceVolume)
            return;

        // convert "surface volume" to mass. this is complicated a bit by the fact that
        // not all phases are necessarily enabled. (we here assume that if a fluid phase
        // is disabled, its respective "main" component is not considered as well.)

        if (waterEnabled) {
            unsigned activeWaterCompIdx = Indices::canonicalToActiveComponentIndex(waterCompIdx);
            container[conti0EqIdx + activeWaterCompIdx] *=
                FluidSystem::referenceDensity(waterPhaseIdx, pvtRegionIdx);
        }

        if (gasEnabled) {
            unsigned activeGasCompIdx = Indices::canonicalToActiveComponentIndex(gasCompIdx);
            container[conti0EqIdx + activeGasCompIdx] *=
                FluidSystem::referenceDensity(gasPhaseIdx, pvtRegionIdx);
        }

        if (oilEnabled) {
            unsigned activeOilCompIdx = Indices::canonicalToActiveComponentIndex(oilCompIdx);
            container[conti0EqIdx + activeOilCompIdx] *=
                FluidSystem::referenceDensity(oilPhaseIdx, pvtRegionIdx);
        }
    }
};

} // namespace Opm

#endif

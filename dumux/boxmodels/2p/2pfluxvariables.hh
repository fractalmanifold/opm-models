// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2008-2011 by Andreas Lauser                               *
 *   Copyright (C) 2008 by Bernd Flemisch                                    *
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
 * \brief This file contains the data which is required to calculate
 *        all fluxes of fluid phases over a face of a finite volume.
 *
 * This means pressure and temperature gradients, phase densities at
 * the integration point, etc.
 */
#ifndef DUMUX_2P_FLUX_VARIABLES_HH
#define DUMUX_2P_FLUX_VARIABLES_HH

#include "2pproperties.hh"

#include <dumux/common/parameters.hh>
#include <dumux/common/math.hh>

namespace Dumux
{

/*!
 * \ingroup TwoPBoxModel
 * \ingroup BoxFluxVariables
 * \brief This template class contains the data which is required to
 *        calculate the fluxes of the fluid phases over a face of a
 *        finite volume for the two-phase model.
 *
 * This means pressure and concentration gradients, phase densities at
 * the intergration point, etc.
 */
template <class TypeTag>
class TwoPFluxVariables
{
    typedef typename GET_PROP_TYPE(TypeTag, Problem) Problem;
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;

    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GridView::template Codim<0>::Entity Element;
    enum {
        dim = GridView::dimension,
        dimWorld = GridView::dimensionworld,
        numPhases = GET_PROP_VALUE(TypeTag, NumPhases)
    };

    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef Dune::FieldVector<Scalar, dimWorld> Vector;
    typedef Dune::FieldMatrix<Scalar, dimWorld, dimWorld> Tensor;

public:
    /*
     * \brief The constructor
     */
    TwoPFluxVariables()
    {}

    /*!
     * \brief Caclulates the quantities required on a sub-control
     *        volume face for the 2p box model.
     */
    void update(const ElementContext &elemCtx, int scvfIdx, int timeIdx)
    {
        insideScvIdx_ = elemCtx.fvElemGeom(timeIdx).subContVolFace[scvfIdx].i;
        outsideScvIdx_ = elemCtx.fvElemGeom(timeIdx).subContVolFace[scvfIdx].j;

        extrusionFactor_ =
            (elemCtx.volVars(insideScvIdx_, timeIdx).extrusionFactor()
             + elemCtx.volVars(outsideScvIdx_, timeIdx).extrusionFactor()) / 2;

        calculateGradients_(elemCtx, scvfIdx, timeIdx);
        calculateNormalFluxes_(elemCtx, scvfIdx, timeIdx);
    };

    /*!
     * \brief Return the extrusion factor of the SCVF.
     */
    Scalar extrusionFactor() const
    { return extrusionFactor_; }

    /*!
     * \brief Return a phase's pressure potential gradient.
     *
     * \param phaseIdx The index of the fluid phase
     */
    const Vector &potentialGrad(int phaseIdx) const
    { return potentialGrad_[phaseIdx]; }

    /*!
     * \brief Return a phase's pressure potential gradient times
     *        intrinsic permeability times the phase mobility.
     *
     * \param phaseIdx The index of the fluid phase
     */
    const Vector &filterVelocity(int phaseIdx) const
    { return filterVelocity_[phaseIdx]; }

    /*!
     * \brief Return a phase's pressure potential gradient times
     *        intrinsic permeability times the normal of the sub
     *        control volume face times the area of the SCVF.
     *
     * \param phaseIdx The index of the fluid phase
     */
    Scalar filterVelocityNormal(int phaseIdx) const
    { return filterVelocityNormal_[phaseIdx]; }

    /*!
     * \brief Return the local index of the control volume which is on
     *        the "inside" of the sub-control volume face.
     */
    short insideIdx() const
    { return insideScvIdx_; }

    /*!
     * \brief Return the local index of the control volume which is on
     *        the "outside" of the sub-control volume face.
     */
    short outsideIdx() const
    { return outsideScvIdx_; }

    /*!
     * \brief Return the local index of the upstream control volume
     *        for a given phase as a function of the normal flux.
     *
     * \param phaseIdx The index of the fluid phase for which the upstream
     *                 direction is requested.
     */
    short upstreamIdx(int phaseIdx) const
    { return (filterVelocityNormal_[phaseIdx] > 0)?insideScvIdx_:outsideScvIdx_; }

    /*!
     * \brief Return the local index of the downstream control volume
     *        for a given phase as a function of the normal flux.
     *
     * \param phaseIdx The index of the fluid phase for which the downstream
     *                 direction is requested.
     */
    short downstreamIdx(int phaseIdx) const
    { return (filterVelocityNormal_[phaseIdx] > 0)?outsideScvIdx_:insideScvIdx_; }

    /*!
     * \brief Return the weight of the upstream control volume
     *        for a given phase as a function of the normal flux.
     *
     * \param phaseIdx The index of the fluid phase
     */
    Scalar upstreamWeight(int phaseIdx) const
    { return 1.0; }

    /*!
     * \brief Return the weight of the downstream control volume
     *        for a given phase as a function of the normal flux.
     *
     * \param phaseIdx The index of the fluid phase
     */
    Scalar downstreamWeight(int phaseIdx) const
    { return 0.0; }

protected:
    void calculateGradients_(const ElementContext &elemCtx,
                             int scvfIdx,
                             int timeIdx)
    {
        // reset all gradients to 0
        for (int phase = 0; phase < numPhases; ++phase) {
            potentialGrad_[phase] = Scalar(0);
        }

        const auto &scvf = elemCtx.fvElemGeom(timeIdx).subContVolFace[scvfIdx];

        // calculate gradients
        for (int scvIdx = 0; scvIdx < elemCtx.numScv(); ++ scvIdx)
        {
            // FE gradient at vertex idx
            const Vector &feGrad = scvf.grad[scvIdx];
            const auto &fs = elemCtx.volVars(scvIdx, timeIdx).fluidState();

            // compute sum of pressure gradients for each phase
            for (int phase = 0; phase < numPhases; phase++)
            {
                // the pressure gradient [Pa/m]
                Vector tmp(feGrad);
                tmp *= fs.pressure(phase);
                potentialGrad_[phase] += tmp;
            }
        }

        ///////////////
        // correct the pressure gradients by the gravitational acceleration
        ///////////////
        if (GET_PARAM(TypeTag, bool, EnableGravity))
        {
            // estimate the gravitational acceleration at a given SCV face
            // using the arithmetic mean
            Vector g(elemCtx.problem().gravity(elemCtx, insideScvIdx_, timeIdx));
            g += elemCtx.problem().gravity(elemCtx, outsideScvIdx_, timeIdx);
            g /= 2;

            const auto &fsI = elemCtx.volVars(insideScvIdx_, timeIdx).fluidState();
            const auto &fsJ = elemCtx.volVars(outsideScvIdx_, timeIdx).fluidState();

            for (int phaseIdx=0; phaseIdx < numPhases; phaseIdx++)
            {
                // calculate the phase density at the integration point. we
                // only do this if the wetting phase is present in both cells
                Scalar SI = fsI.saturation(phaseIdx);
                Scalar SJ = fsJ.saturation(phaseIdx);
                Scalar rhoI = fsI.density(phaseIdx);
                Scalar rhoJ = fsJ.density(phaseIdx);
                Scalar fI = std::max(0.0, std::min(SI/1e-5, 0.5));
                Scalar fJ = std::max(0.0, std::min(SJ/1e-5, 0.5));
                if (fI + fJ == 0)
                    // doesn't matter because no wetting phase is present in
                    // both cells!
                    fI = fJ = 0.5;
                Scalar density = (fI*rhoI + fJ*rhoJ)/(fI + fJ);

                // make gravity acceleration a force
                Vector f(g);
                f *= density;

                // calculate the final potential gradient
                potentialGrad_[phaseIdx] -= f;
            }
        }
    }

    void calculateNormalFluxes_(const ElementContext &elemCtx,
                                int scvfIdx,
                                int timeIdx)
    {
        const auto &problem = elemCtx.problem();

        // calculate the intrinsic permeability
        Tensor K;
        problem.meanK(K,
                      problem.intrinsicPermeability(elemCtx,
                                                    insideScvIdx_,
                                                    timeIdx),
                      problem.intrinsicPermeability(elemCtx,
                                                    outsideScvIdx_,
                                                    timeIdx));
        
        const Vector &normal = elemCtx.fvElemGeom(timeIdx).subContVolFace[scvfIdx].normal;

        // calculate the flux in the normal direction of the
        // current sub control volume face:
        //
        // v = - (K grad p) * n
        //
        // (the minus comes from the Darcy law which states that
        // the flux is from high to low pressure potentials.)
        for (int phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            K.mv(potentialGrad_[phaseIdx], filterVelocity_[phaseIdx]);

            // velocity goes along negative potential gradients
            filterVelocity_[phaseIdx] *= -1;

            // scalar product with the face normal
            filterVelocityNormal_[phaseIdx] = 0.0;
            for (int i = 0; i < Vector::size; ++i)
                filterVelocityNormal_[phaseIdx] += filterVelocity_[phaseIdx][i]*normal[i];

            // multiply both with the upstream mobility
            const auto &up = elemCtx.volVars(upstreamIdx(phaseIdx), timeIdx);
            filterVelocityNormal_[phaseIdx] *= up.mobility(phaseIdx);
            filterVelocity_[phaseIdx] *= up.mobility(phaseIdx);
        }

    }

    // local indices of the inside and the outside sub-control volumes
    short insideScvIdx_;
    short outsideScvIdx_;

    // extrusion factor
    Scalar extrusionFactor_;

    // pressure potential gradient
    Vector potentialGrad_[numPhases];

    // filter velocity
    Vector filterVelocity_[numPhases];

    // normal fluxes
    Scalar filterVelocityNormal_[numPhases];
};

} // end namepace

#endif

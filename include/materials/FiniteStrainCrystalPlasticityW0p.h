/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/
#ifndef FINITESTRAINCRYSTALPLASTICITYW0P_H
#define FINITESTRAINCRYSTALPLASTICITYW0P_H

#include "FiniteStrainCrystalPlasticity.h"

/**
 * FiniteStrainCrystalPlasticity uses the multiplicative decomposition of deformation gradient
 * and solves the PK2 stress residual equation at the intermediate configuration to evolve the material state.
 * The internal variables are updated using an interative predictor-corrector algorithm.
 * Backward Euler integration rule is used for the rate equations.
 * added plastic work and bulk viscosity
 */
class FiniteStrainCrystalPlasticityW0p;

template<>
InputParameters validParams<FiniteStrainCrystalPlasticityW0p>();

class FiniteStrainCrystalPlasticityW0p : public FiniteStrainCrystalPlasticity
{
public:
  FiniteStrainCrystalPlasticityW0p(const InputParameters & parameters);

protected:
  /**
   * This function set variables for internal variable solve.
   */
  virtual void preSolveStatevar();

  /**
   * This function solves internal variables.
   */
  virtual void solveStatevar();

  /**
   * This function update internal variable after solve.
   */
  virtual void postSolveStatevar();

  /**
   * Update elastic and plastic work
   */
  virtual void update_energies();

  /**
   * This function calculate stress residual.
   */
  virtual void calcResidual( RankTwoTensor & );

  virtual void getSlipIncrements();

  // Von Neumann coefficient
  const Real _C0;

  // Landshoff coefficient
  const Real _C1;

  MaterialProperty<Real> & _W0e;
  MaterialProperty<Real> & _W0p;
  const MaterialProperty<Real> & _W0p_old;
  MaterialProperty<RankTwoTensor> & _dW0e_dstrain;
  MaterialProperty<RankTwoTensor> & _dW0p_dstrain;

  Real _W0p_tmp;
  Real _W0p_tmp_old;
};

#endif //FINITESTRAINCRYSTALPLASTICITYW0P_H

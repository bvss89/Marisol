/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/

#include "ComputePolycrystalElasticityTensorCP.h"
#include "RotationTensor.h"
#include "EulerAngleProvider.h"

template<>
InputParameters validParams<ComputePolycrystalElasticityTensorCP>()
{
  InputParameters params = validParams<ComputeElasticityTensorBase>();
  params.addClassDescription("Compute an evolving elasticity tensor coupled to a grain growth phase field model.");
  params.addRequiredParam<UserObjectName>("grain_tracker", "Name of GrainTracker user object that provides RankFourTensors");
  params.addRequiredParam<UserObjectName>("grain_tracker_crysrot", "Name of GrainTracker user object that provides RankTwoTensors");
  params.addParam<Real>("length_scale", 1.0e-9, "Lengthscale of the problem, in meters");
  params.addParam<Real>("pressure_scale", 1.0e6, "Pressure scale of the problem, in pa");
  params.addRequiredCoupledVarWithAutoBuild("v", "var_name_base", "op_num", "Array of coupled variables");
//  params.addParam<UserObjectName>("read_prop_user_object","The ElementReadPropertyFile GeneralUserObject to read element specific property values from file");
  return params;
}

ComputePolycrystalElasticityTensorCP::ComputePolycrystalElasticityTensorCP(const InputParameters & parameters) :
    ComputeElasticityTensorBase(parameters),
    _length_scale(getParam<Real>("length_scale")),
    _pressure_scale(getParam<Real>("pressure_scale")),
    _grain_tracker(getUserObject<GrainDataTracker<RankFourTensor>>("grain_tracker")),
    _grain_tracker_crysrot(getUserObject<GrainDataTracker<EulerAngles>>("grain_tracker_crysrot")),
    _op_num(coupledComponents("v")),
    _vals(_op_num),
    _D_elastic_tensor(_op_num),

    _crysrot(declareProperty<RankTwoTensor>("crysrot")),
    //_angle2(declareProperty<RealVectorValue>("angle2")),
            
    //_euler(getUserObject<EulerAngleProvider>("euler_angle_provider")),

    _JtoeV(6.24150974e18)
{
  // Loop over variables (ops)
  for (auto op_index = decltype(_op_num)(0); op_index < _op_num; ++op_index)
  {
    // Initialize variables
    _vals[op_index] = &coupledValue("v", op_index);

    // declare elasticity tensor derivative properties
    _D_elastic_tensor[op_index] = &declarePropertyDerivative<RankFourTensor>(_elasticity_tensor_name, getVar("v", op_index)->name());
    _console << "firstCP Finished inside of GrainTracker" << std::endl;
      
  }
    
 }


void
ComputePolycrystalElasticityTensorCP::computeQpElasticityTensor()
{

    
  // Get list of active order parameters from grain tracker
  const auto & op_to_grains = _grain_tracker.getVarToFeatureVector(_current_elem->id());

  // Calculate elasticity tensor
  _elasticity_tensor[_qp].zero();
  _crysrot[_qp].zero();
    
  EulerAngles angles;
  RealVectorValue angle2;
  angle2.zero();
    
  Real sum_h = 0.0;
    
 // _console << "beforeCP Finished inside of GrainTracker" << std::endl;
    
  for (auto op_index = beginIndex(op_to_grains); op_index < op_to_grains.size(); ++op_index)
  {
    auto grain_id = op_to_grains[op_index];
    if (grain_id == FeatureFloodCount::invalid_id)
      continue;

    // Interpolation factor for elasticity tensors
    Real h = (1.0 + std::sin(libMesh::pi * ((*_vals[op_index])[_qp] - 0.5))) / 2.0;

    // Sum all rotated elasticity tensors
    _elasticity_tensor[_qp] += _grain_tracker.getData(grain_id) * h;
    sum_h += h;
  }
    
  const Real tol = 1.0e-10;
  sum_h = std::max(sum_h, tol);
  _elasticity_tensor[_qp] /= sum_h;
 
    
  const auto & op_to_grains_euler = _grain_tracker_crysrot.getVarToFeatureVector(_current_elem->id());
  sum_h = 0.0;
  for (auto op_index_euler = beginIndex(op_to_grains_euler); op_index_euler < op_to_grains_euler.size(); ++op_index_euler)
    {
        auto grain_id_euler = op_to_grains_euler[op_index_euler];
        if (grain_id_euler == FeatureFloodCount::invalid_id)
            continue;
        
        // Interpolation factor for elasticity tensors
        Real h = (1.0 + std::sin(libMesh::pi * ((*_vals[op_index_euler])[_qp] - 0.5))) / 2.0;
        
        // Sum all rotated elasticity tensors
        angles = _grain_tracker_crysrot.getData(grain_id_euler) ;
        angle2 += RealVectorValue(angles) * h;
       
        sum_h += h;
    }
    
    sum_h = std::max(sum_h, tol);
    angle2 /= sum_h;
    
    //RealVectorValue angle2 = RealVectorValue(angles);
    RotationTensor R(angle2);
    R.update(angle2);
    _crysrot[_qp] = R.transpose();
    

  // Calculate elasticity tensor derivative: Cderiv = dhdopi/sum_h * (Cop - _Cijkl)
  for (auto op_index = decltype(_op_num)(0); op_index < _op_num; ++op_index)
    (*_D_elastic_tensor[op_index])[_qp].zero();

  for (auto op_index = beginIndex(op_to_grains); op_index < op_to_grains.size(); ++op_index)
  {
    auto grain_id = op_to_grains[op_index];
    if (grain_id == FeatureFloodCount::invalid_id)
      continue;

    Real dhdopi = libMesh::pi * std::cos(libMesh::pi * ((*_vals[op_index])[_qp] - 0.5)) / 2.0;
    RankFourTensor & C_deriv = (*_D_elastic_tensor[op_index])[_qp];

    C_deriv = (_grain_tracker.getData(grain_id) - _elasticity_tensor[_qp]) * dhdopi / sum_h;

    // Convert from XPa to eV/(xm)^3, where X is pressure scale and x is length scale;
    C_deriv *= _JtoeV * (_length_scale * _length_scale * _length_scale) * _pressure_scale;
  }
}

/*
 * Copyright 2012-2019 CNRS-UM LIRMM, CNRS-AIST JRL
 */

#pragma once

// includes
// std
#include <map>

// Eigen
#include <Eigen/Core>

// RBDyn
#include <RBDyn/FD.h>
#include <RBDyn/Jacobian.h>
#include <RBDyn/Friction.h>
#include <RBDyn/TorqueFeedbackTerm.h>

// Tasks
#include "QPSolver.h"

namespace tasks
{
struct TorqueBound;
struct PolyTorqueBound;

namespace qp
{

class TASKS_DLLAPI PositiveLambda : public ConstraintFunction<Bound>
{
public:
  PositiveLambda();

  // Constraint
  virtual void updateNrVars(const std::vector<rbd::MultiBody> & mbs, const SolverData & data) override;
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbc,
                      const SolverData & data) override;

  // Description
  virtual std::string nameBound() const override;
  virtual std::string descBound(const std::vector<rbd::MultiBody> & mbs, int line) override;

  // Bound Constraint
  virtual int beginVar() const override;

  virtual const Eigen::VectorXd & Lower() const override;
  virtual const Eigen::VectorXd & Upper() const override;

private:
  struct ContactData
  {
    ContactId cId;
    int lambdaBegin, nrLambda; // lambda index in x
  };

private:
  int lambdaBegin_;
  Eigen::VectorXd XL_, XU_;

  std::vector<ContactData> cont_; // only usefull for descBound
};

class TASKS_DLLAPI MotionConstrCommon : public ConstraintFunction<GenInequality>
{
public:
  MotionConstrCommon(const std::vector<rbd::MultiBody> & mbs, int robotIndex,
                     const std::shared_ptr<rbd::ForwardDynamics> fd);

  virtual void computeTorque(const Eigen::VectorXd & alphaD, const Eigen::VectorXd & lambda);
  const Eigen::VectorXd & torque() const;
  void torque(const std::vector<rbd::MultiBody> & mbs, std::vector<rbd::MultiBodyConfig> & mbcs) const;

  // Constraint
  virtual void updateNrVars(const std::vector<rbd::MultiBody> & mbs, const SolverData & data) override;

  void computeMatrix(const std::vector<rbd::MultiBody> & mb, const std::vector<rbd::MultiBodyConfig> & mbcs);

  // Description
  virtual std::string nameGenInEq() const override;
  virtual std::string descGenInEq(const std::vector<rbd::MultiBody> & mbs, int line) override;

  // Inequality Constraint
  virtual int maxGenInEq() const override;

  virtual const Eigen::MatrixXd & AGenInEq() const override;
  virtual const Eigen::VectorXd & LowerGenInEq() const override;
  virtual const Eigen::VectorXd & UpperGenInEq() const override;

protected:
  struct ContactData
  {
    ContactData() {}
    ContactData(const rbd::MultiBody & mb,
                const std::string & bodyName,
                int lambdaBegin,
                std::vector<Eigen::Vector3d> points,
                const std::vector<FrictionCone> & cones);

    int bodyIndex;
    int lambdaBegin;
    rbd::Jacobian jac;
    std::vector<Eigen::Vector3d> points;
    // BEWARE generator are minus to avoid one multiplication by -1 in the
    // update method
    std::vector<Eigen::Matrix<double, 3, Eigen::Dynamic>> minusGenerators;
  };

protected:
  int robotIndex_, alphaDBegin_, nrDof_, lambdaBegin_;
  // rbd::ForwardDynamics fd_;
  std::shared_ptr<rbd::ForwardDynamics> fd_;
  Eigen::MatrixXd fullJacLambda_, jacTrans_, jacLambda_;
  std::vector<ContactData> cont_;

  Eigen::VectorXd curTorque_;

  Eigen::MatrixXd A_;
  Eigen::VectorXd AL_, AU_;
};

class TASKS_DLLAPI MotionConstr : public MotionConstrCommon
{
public:
  MotionConstr(const std::vector<rbd::MultiBody> & mbs, int robotIndex,
               const std::shared_ptr<rbd::ForwardDynamics> fd, const TorqueBound & tb);

  void computeTorque(const Eigen::VectorXd& alphaD, const Eigen::VectorXd& lambda) override;
  
  // Constraint
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbcs,
                      const SolverData & data) override;
  // Matrix
  const Eigen::MatrixXd matrix() const
  {
    return A_;
  }
  // Contact torque
  Eigen::MatrixXd contactMatrix() const;
  // Access fd...
  const std::shared_ptr<rbd::ForwardDynamics> fd() const;
  
  const Eigen::VectorXd & computedTorque() const
  {
    return computedTorque_;
  }

protected:
  Eigen::VectorXd computedTorque_;
  Eigen::VectorXd torqueL_, torqueU_;
};

struct SpringJoint
{
  SpringJoint() {}
  SpringJoint(const std::string & jName, double K, double C, double O) : jointName(jName), K(K), C(C), O(O) {}

  std::string jointName;
  double K, C, O;
};

class TASKS_DLLAPI MotionSpringConstr : public MotionConstr
{
public:
  MotionSpringConstr(const std::vector<rbd::MultiBody> & mbs, int robotIndex,
                     const std::shared_ptr<rbd::ForwardDynamics> fd, const TorqueBound & tb,
                     const std::vector<SpringJoint> & springs);

  // Constraint
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbc,
                      const SolverData & data) override;

protected:
  struct SpringJointData
  {
    int index;
    int posInDof;
    double K;
    double C;
    double O;
  };

protected:
  std::vector<SpringJointData> springs_;
};

/**
 * @brief Use polynome in function of q to compute torque limits.
 * BEWARE: Only work with 1 dof/param joint
 */
class TASKS_DLLAPI MotionPolyConstr : public MotionConstrCommon
{
public:
  MotionPolyConstr(const std::vector<rbd::MultiBody> & mbs, int robotIndex,
                   const std::shared_ptr<rbd::ForwardDynamics> fd, const PolyTorqueBound & ptb);

  // Constraint
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbcs,
                      const SolverData & data) override;

protected:
  std::vector<Eigen::VectorXd> torqueL_, torqueU_;
  std::vector<int> jointIndex_;
};

class TASKS_DLLAPI MotionFrictionConstr : public MotionConstr
{
public:
  MotionFrictionConstr(const std::vector<rbd::MultiBody> & mbs, int robotIndex,
                       const std::shared_ptr<rbd::ForwardDynamics> fd,
		       const std::shared_ptr<rbd::Friction> friction,
		       const TorqueBound & tb);

  void computeTorque(const Eigen::VectorXd& alphaD, const Eigen::VectorXd& lambda) override;

  // Constraint
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbcs,
                      const SolverData & data);

  const Eigen::VectorXd& frictionTorque() const
  {
    return frictionTorque_;
  }
  
private:
  std::shared_ptr<rbd::Friction> friction_;
  Eigen::VectorXd frictionTorque_;
};
 
class TASKS_DLLAPI TorqueFbTermMotionConstr : public MotionConstr
{
public:
  TorqueFbTermMotionConstr(const std::vector<rbd::MultiBody>& mbs, int robotIndex,
                           const std::shared_ptr<rbd::ForwardDynamics> fd,
                           const std::shared_ptr<torque_control::TorqueFeedbackTerm> fbTerm,
                           const TorqueBound& tb);  

  void computeTorque(const Eigen::VectorXd& alphaD, const Eigen::VectorXd& lambda) override;

  // Constraint
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbcs,
                      const SolverData & data);

private:
  std::shared_ptr<torque_control::TorqueFeedbackTerm> fbTerm_;
};

class TASKS_DLLAPI TorqueFbTermMotionFrictionConstr : public MotionFrictionConstr
{
public:
  TorqueFbTermMotionFrictionConstr(const std::vector<rbd::MultiBody>& mbs, int robotIndex,
                                   const std::shared_ptr<rbd::ForwardDynamics> fd,
                                   const std::shared_ptr<rbd::Friction> friction,
                                   const std::shared_ptr<torque_control::TorqueFeedbackTerm> fbTerm,
                                   const TorqueBound& tb);  

  void computeTorque(const Eigen::VectorXd& alphaD, const Eigen::VectorXd& lambda) override;

  // Constraint
  virtual void update(const std::vector<rbd::MultiBody> & mbs,
                      const std::vector<rbd::MultiBodyConfig> & mbcs,
                      const SolverData & data);

private:
  std::shared_ptr<torque_control::TorqueFeedbackTerm> fbTerm_;
};

} // namespace qp

} // namespace tasks

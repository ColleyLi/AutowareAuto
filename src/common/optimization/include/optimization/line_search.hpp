// Copyright 2019 Apex.AI, Inc.
// Co-developed by Tier IV, Inc. and Apex.AI, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPTIMIZATION__LINE_SEARCH_HPP_
#define OPTIMIZATION__LINE_SEARCH_HPP_

#include <helper_functions/crtp.hpp>
#include <optimization/visibility_control.hpp>
#include <limits>
#include <cmath>

namespace autoware
{
namespace common
{
namespace optimization
{
/// Base class (CRTP) to mange the step length during optimization.
template<typename Derived>
class OPTIMIZATION_PUBLIC LineSearch : public common::helper_functions::crtp<Derived>
{
public:
  // TODO(zozen): should this be forced to be positive?
  using StepT = double;

  /// Constructor.
  /// \param step_max Maximum step length. By default initialized to the minimum value.
  explicit LineSearch(const StepT step_max = std::numeric_limits<StepT>::min())
  {
    m_step_max = step_max;
  }

  /// Computes the optimal step length for the optimization problem
  /// \tparam OptimizationProblemT Optimization problem type. Must be an
  /// implementation of `common::optimization::OptimizationProblem`.
  /// \param optimization_problem optimization objective
  /// \return The resulting step length.
  template<typename OptimizationProblemT>
  StepT compute_step_length(OptimizationProblemT & optimization_problem)
  {
    return this->impl().compute_step_length_(optimization_problem);
  }

  /// Getter for the maximum step length
  /// \return The maximum step length.
  StepT get_step_max() const noexcept
  {
    return m_step_max;
  }

  /// Setter for the maximum step length
  /// \param step_max the new maximal step length
  void set_step_max(const StepT step_max) noexcept
  {
    m_step_max = step_max;
  }

private:
  StepT m_step_max;
};

/// Class to use a fixed step length during optimization.
class OPTIMIZATION_PUBLIC FixedLineSearch : public LineSearch<FixedLineSearch>
{
public:
  /// Constructor.
  /// \param step Fixed step to be used.
  explicit FixedLineSearch(const StepT step = std::numeric_limits<StepT>::min())
  : LineSearch(step) {}
  /// Returns directly the pre-set (maximum) step length
  /// \return The fixed step length.
  template<typename OptimizationProblemT>
  StepT compute_step_length_(OptimizationProblemT &) const noexcept
  {
    return get_step_max();
  }
};

class OPTIMIZATION_PUBLIC MT
{
public:
    /// Returns directly the pre-set (maximum) step length
    /// \return The fixed step length.
    template<typename OptimizationProblemT>
    double compute_step_length_(OptimizationProblemT & problem,
            typename OptimizationProblemT::DomainValue curr_x,
             Eigen::Ref<typename OptimizationProblemT::Jacobian> step_dir,
             double step_min, double step_max)
    {
        problem.evaluate(curr_x, ComputeMode{}.set_score().set_jacobian());
        auto score = problem(curr_x);
        typename OptimizationProblemT::Jacobian score_gradient;
        problem.jacobian(curr_x, score_gradient);
        double phi_0 = -score;
        double d_phi_0 = -(score_gradient.dot(step_dir));

        typename OptimizationProblemT::DomainValue x_t;

        if (d_phi_0 >= 0) {
            if (d_phi_0 == 0) {
                return 0;
            } else {
                d_phi_0 *= -1;
                step_dir *= -1;
            }
        }

        int max_step_iterations = 10;
        int step_iterations = 0;

        double mu = 1.e-4;
        double nu = 0.9;
        double a_l = 0, a_u = 0;

        double f_l = auxilaryFunction_PsiMT(a_l, phi_0, phi_0, d_phi_0, mu);
        double g_l = auxilaryFunction_dPsiMT(d_phi_0, d_phi_0, mu);

        double f_u = auxilaryFunction_PsiMT(a_u, phi_0, phi_0, d_phi_0, mu);
        double g_u = auxilaryFunction_dPsiMT(d_phi_0, d_phi_0, mu);

        bool interval_converged = (step_max - step_min) > 0, open_interval = true;

        double a_t = step_dir.norm();
        a_t = std::min(a_t, step_max);
        a_t = std::max(a_t, step_min);

        x_t = curr_x + step_dir * a_t;

//        final_transformation_ = (Eigen::Translation<float, 3>(static_cast<float>(x_t(0)), static_cast<float>(x_t(1)), static_cast<float>(x_t(2))) *
//                                 Eigen::AngleAxis<float>(static_cast<float>(x_t(3)), Eigen::Vector3f::UnitX()) *
//                                 Eigen::AngleAxis<float>(static_cast<float>(x_t(4)), Eigen::Vector3f::UnitY()) *
//                                 Eigen::AngleAxis<float>(static_cast<float>(x_t(5)), Eigen::Vector3f::UnitZ())).matrix();

//        transformPointCloud(*source_cloud_, trans_cloud, final_transformation_);

//        score = computeDerivatives(score_gradient, hessian, trans_cloud, x_t, true);

        problem.evaluate(x_t, ComputeMode{}.set_score().set_jacobian());
        score = problem(x_t);
        problem.jacobian(x_t, score_gradient);

        double phi_t = -score;
        double d_phi_t = -(score_gradient.dot(step_dir));
        double psi_t = auxilaryFunction_PsiMT(a_t, phi_t, phi_0, d_phi_0, mu);
        double d_psi_t = auxilaryFunction_dPsiMT(d_phi_t, d_phi_0, mu);

        while (!interval_converged && step_iterations < max_step_iterations && !(psi_t <= 0 && d_phi_t <= -nu * d_phi_0)) {
            if (open_interval) {
                a_t = trialValueSelectionMT(a_l, f_l, g_l, a_u, f_u, g_u, a_t, psi_t, d_psi_t);
            } else {
                a_t = trialValueSelectionMT(a_l, f_l, g_l, a_u, f_u, g_u, a_t, phi_t, d_phi_t);
            }

            a_t = (a_t < step_max) ? a_t : step_max;
            a_t = (a_t > step_min) ? a_t : step_min;

            x_t = curr_x + step_dir * a_t;

            problem.evaluate(x_t, ComputeMode{}.set_score().set_jacobian());
            score = problem(x_t);
            problem.jacobian(x_t, score_gradient);

            phi_t -= score;
            d_phi_t -= (score_gradient.dot(step_dir));
            psi_t = auxilaryFunction_PsiMT(a_t, phi_t, phi_0, d_phi_0, mu);
            d_psi_t = auxilaryFunction_dPsiMT(d_phi_t, d_phi_0, mu);

            if (open_interval && (psi_t <= 0 && d_psi_t >= 0)) {
                open_interval = false;

                f_l += phi_0 - mu * d_phi_0 * a_l;
                g_l += mu * d_phi_0;

                f_u += phi_0 - mu * d_phi_0 * a_u;
                g_u += mu * d_phi_0;
            }

            if (open_interval) {
                interval_converged = updateIntervalMT(a_l, f_l, g_l, a_u, f_u, g_u, a_t, psi_t, d_psi_t) > 0.0;
            } else {
                interval_converged = updateIntervalMT(a_l, f_l, g_l, a_u, f_u, g_u, a_t, phi_t, d_phi_t) > 0.0;
            }
            step_iterations++;
        }

        return a_t;
    }



    ///////////////////////////////////


    double trialValueSelectionMT (double a_l, double f_l, double g_l,
                                                                                                  double a_u, double f_u, double g_u,
                                                                                                  double a_t, double f_t, double g_t)
    {
        // Case 1 in Trial Value Selection [More, Thuente 1994]
        if (f_t > f_l) {
            // Calculate the minimizer of the cubic that interpolates f_l, f_t, g_l and g_t
            // Equation 2.4.52 [Sun, Yuan 2006]
            double z = 3 * (f_t - f_l) / (a_t - a_l) - g_t - g_l;
            double w = std::sqrt (z * z - g_t * g_l);
            // Equation 2.4.56 [Sun, Yuan 2006]
            double a_c = a_l + (a_t - a_l) * (w - g_l - z) / (g_t - g_l + 2 * w);

            // Calculate the minimizer of the quadratic that interpolates f_l, f_t and g_l
            // Equation 2.4.2 [Sun, Yuan 2006]
            double a_q = a_l - 0.5 * (a_l - a_t) * g_l / (g_l - (f_l - f_t) / (a_l - a_t));

            if (std::fabs (a_c - a_l) < std::fabs (a_q - a_l)) {
                return (a_c);
            } else {
                return (0.5 * (a_q + a_c));
            }
        }
            // Case 2 in Trial Value Selection [More, Thuente 1994]
        else if (g_t * g_l < 0) {
            // Calculate the minimizer of the cubic that interpolates f_l, f_t, g_l and g_t
            // Equation 2.4.52 [Sun, Yuan 2006]
            double z = 3 * (f_t - f_l) / (a_t - a_l) - g_t - g_l;
            double w = std::sqrt (z * z - g_t * g_l);
            // Equation 2.4.56 [Sun, Yuan 2006]
            double a_c = a_l + (a_t - a_l) * (w - g_l - z) / (g_t - g_l + 2 * w);

            // Calculate the minimizer of the quadratic that interpolates f_l, g_l and g_t
            // Equation 2.4.5 [Sun, Yuan 2006]
            double a_s = a_l - (a_l - a_t) / (g_l - g_t) * g_l;

            if (std::fabs (a_c - a_t) >= std::fabs (a_s - a_t)) {
                return (a_c);
            } else {
                return (a_s);
            }
        }
            // Case 3 in Trial Value Selection [More, Thuente 1994]
        else if (std::fabs (g_t) <= std::fabs (g_l)) {
            // Calculate the minimizer of the cubic that interpolates f_l, f_t, g_l and g_t
            // Equation 2.4.52 [Sun, Yuan 2006]
            double z = 3 * (f_t - f_l) / (a_t - a_l) - g_t - g_l;
            double w = std::sqrt (z * z - g_t * g_l);
            double a_c = a_l + (a_t - a_l) * (w - g_l - z) / (g_t - g_l + 2 * w);

            // Calculate the minimizer of the quadratic that interpolates g_l and g_t
            // Equation 2.4.5 [Sun, Yuan 2006]
            double a_s = a_l - (a_l - a_t) / (g_l - g_t) * g_l;

            double a_t_next;

            if (std::fabs (a_c - a_t) < std::fabs (a_s - a_t)) {
                a_t_next = a_c;
            } else {
                a_t_next = a_s;
            }

            if (a_t > a_l) {
                return (std::min (a_t + 0.66 * (a_u - a_t), a_t_next));
            } else {
                return (std::max (a_t + 0.66 * (a_u - a_t), a_t_next));
            }
        }
            // Case 4 in Trial Value Selection [More, Thuente 1994]
        else {
            // Calculate the minimizer of the cubic that interpolates f_u, f_t, g_u and g_t
            // Equation 2.4.52 [Sun, Yuan 2006]
            double z = 3 * (f_t - f_u) / (a_t - a_u) - g_t - g_u;
            double w = std::sqrt (z * z - g_t * g_u);
            // Equation 2.4.56 [Sun, Yuan 2006]
            return (a_u + (a_t - a_u) * (w - g_u - z) / (g_t - g_u + 2 * w));
        }
    }

//Copied from ndt.hpp
    double updateIntervalMT (double &a_l, double &f_l, double &g_l,
                                                                                             double &a_u, double &f_u, double &g_u,
                                                                                             double a_t, double f_t, double g_t)
    {
        // Case U1 in Update Algorithm and Case a in Modified Update Algorithm [More, Thuente 1994]
        if (f_t > f_l) {
            a_u = a_t;
            f_u = f_t;
            g_u = g_t;
            return (false);
        }
            // Case U2 in Update Algorithm and Case b in Modified Update Algorithm [More, Thuente 1994]
        else if (g_t * (a_l - a_t) > 0) {
            a_l = a_t;
            f_l = f_t;
            g_l = g_t;
            return (false);
        }
            // Case U3 in Update Algorithm and Case c in Modified Update Algorithm [More, Thuente 1994]
        else if (g_t * (a_l - a_t) < 0) {
            a_u = a_l;
            f_u = f_l;
            g_u = g_l;

            a_l = a_t;
            f_l = f_t;
            g_l = g_t;
            return (false);
        }
            // Interval Converged
        else {
            return (true);
        }
    }

    double auxilaryFunction_PsiMT(double a, double f_a, double f_0, double g_0, double mu)
    {
        return (f_a - f_0 - mu * g_0 * a);
    }

    double auxilaryFunction_dPsiMT(double g_a, double g_0, double mu)
    {
        return (g_a - mu * g_0);
    }


};


}  // namespace optimization
}  // namespace common
}  // namespace autoware

#endif  // OPTIMIZATION__LINE_SEARCH_HPP_

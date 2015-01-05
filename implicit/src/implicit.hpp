#ifndef IMPLICIT_H
#define IMPLICIT_H

#include "RcppArmadillo.h"
#include <boost/math/tools/roots.hpp>
#include <boost/function.hpp>
#include <boost/bind/bind.hpp>
#include <boost/ref.hpp>
#include <math.h>
#include <string>
#include <cstddef>

using namespace arma;

#define nullptr NULL
#define DEBUG 1

struct Imp_DataPoint;
struct Imp_Dataset;
struct Imp_OnlineOutput;
struct Imp_Identity;
struct Imp_Exp;
struct Imp_Experiment;
struct Imp_Size;

struct Imp_Identity_Transfer;
struct Imp_Exp_Transfer;
struct Imp_Logistic_Transfer;

struct Imp_Unidim_Learn_Rate;
struct Imp_Pxdim_Learn_Rate;

typedef boost::function<double (double)> uni_func_type;
typedef boost::function<mat (const mat&, const Imp_DataPoint&)> score_func_type;
typedef boost::function<mat (const mat&, const Imp_DataPoint&, unsigned, unsigned)> learning_rate_type;

struct Imp_DataPoint {
  Imp_DataPoint(): x(mat()), y(0) {}
  Imp_DataPoint(mat xin, double yin):x(xin), y(yin) {}
//@members
  mat x;
  double y;
};

struct Imp_Dataset
{
  Imp_Dataset():X(mat()), Y(mat()) {}
  Imp_Dataset(mat xin, mat yin):X(xin), Y(yin) {}
//@members
  mat X;
  mat Y;
//@methods
  mat covariance() const {
    return cov(X);
  }
};

struct Imp_OnlineOutput{
  //Construct Imp_OnlineOutput compatible with
  //the shape of data
  Imp_OnlineOutput(const Imp_Dataset& data):estimates(mat(data.X.n_cols, data.X.n_rows)){}
  Imp_OnlineOutput(){}
//@members
  mat estimates;
//@methods
  mat last_estimate(){
    return estimates.col(estimates.n_cols-1);
  }
};

/* 1 dimension (scalar) learning rate, suggested in Xu's paper
 */
struct Imp_Unidim_Learn_Rate
{
  static mat learning_rate(const mat& theta_old, const Imp_DataPoint& data_pt, 
                          unsigned t, unsigned p,
                          double gamma, double alpha, double c, double scale) {
    double lr = scale * gamma * pow(1 + alpha * gamma * t, -c);
    mat lr_mat = mat(p, p, fill::eye) * lr;
    return lr_mat;
  }
};

// p dimension learning rate
struct Imp_Pxdim_Learn_Rate
{
  static mat learning_rate(const mat& theta_old, const Imp_DataPoint& data_pt, 
                          unsigned t, unsigned p,
                          score_func_type score_func) {
    mat Idiag(p, p, fill::eye);
    mat Gi = score_func(theta_old, data_pt);
    Idiag = Idiag + diagmat(Gi * Gi.t());

    for (unsigned i = 0; i < p; ++i) {
      if (abs(Idiag.at(i, i)) > 1e-8) {
        Idiag.at(i, i) = 1. / Idiag.at(i, i);
      }
    }
    return Idiag;
  }
};

// Identity transfer function
struct Imp_Identity_Transfer {
  static double transfer(double u) {
    return u;
  }

  static mat transfer(const mat& u) {
    return u;
  }

  static double first_derivative(double u) {
    return 1.;
  }

  static double second_derivative(double u) {
    return 0.;
  }

  static double link(double u) {

  }
};

// Exponentional transfer function
struct Imp_Exp_Transfer {
  static double transfer(double u) {
    return exp(u);
  }

  static mat transfer(const mat& u) {
    mat result = mat(u);
    for (unsigned i = 0; i < result.n_rows; ++i) {
      result(i, 0) = transfer(u(i, 0));
    }
    return result;
  }

  static double first_derivative(double u) {
    return exp(u);
  }

  static double second_derivative(double u) {
    return exp(u);
  }
};

// Logistic transfer function
struct Imp_Logistic_Transfer {
  static double transfer(double u) {
    return sigmoid(u);
  }

  static mat transfer(const mat& u) {
    mat result = mat(u);
    for (unsigned i = 0; i < result.n_rows; ++i) {
      result(i, 0) = transfer(u(i, 0));
    }
    return result;
  }

  static double first_derivative(double u) {
    double sig = sigmoid(u);
    return sig * (1. - sig);
  }

  static double second_derivative(double u) {
    double sig = sigmoid(u);
    return 2*pow(sig, 3) - 3*pow(sig, 2) + 2*sig;
  }

private:
  // sigmoid function
  static double sigmoid(double u) {
      return 1. / (1. + exp(-u));
  }
};

// gaussian model family
struct Imp_Gaussian {
  static std::string family;
  
  static double variance(double u) {
    return 1.;
  }

  static double deviance(const mat& y, const mat& mu, const mat& wt) {
    return sum(vec(wt % ((y-mu) % (y-mu))));
  }
};

std::string Imp_Gaussian::family = "gaussian";

// poisson model family
struct Imp_Poisson {
  static std::string family;
  
  static double variance(double u) {
    return u;
  }

  static double deviance(const mat& y, const mat& mu, const mat& wt) {
    vec r = vec(mu % wt);
    for (unsigned i = 0; i < r.n_elem; ++i) {
      if (y(i) > 0.) {
        r(i) = wt(i) * (y(i) * log(y(i)/mu(i)) - (y(i) - mu(i)));
      }
    }
    return sum(2. * r);
  }
};

std::string Imp_Poisson::family = "poisson";

// binomial model family
struct Imp_Binomial {
  static std::string family;
  
  static double variance(double u) {
    return u * (1. - u);
  }

  // In R the dev.resids of Binomial family is not exposed.
  // Found one [here](http://pages.stat.wisc.edu/~st849-1/lectures/GLMDeviance.pdf)
  static double deviance(const mat& y, const mat& mu, const mat& wt) {
    vec r(y.n_elem);
    for (unsigned i = 0; i < r.n_elem; ++i) {
      r(i) = 2. * wt(i) * (y_log_y(y(i), mu(i)) + y_log_y(1.-y(i), 1.-mu(i)));
    }
    return sum(r);
  }
private:
  static double y_log_y(double y, double mu) {
    return (y) ? (y * log(y/mu)) : 0.;
  }
};

std::string Imp_Binomial::family = "binomial";

struct Imp_Experiment {
//@members
  unsigned p;
  unsigned n_iters;
  std::string model_name;
//@methods
  Imp_Experiment(std::string transfer_name) {
    if (transfer_name == "identity") {
      //transfer_ = boost::bind(&Imp_Identity_Transfer::transfer, _1);
      transfer_ = boost::bind(static_cast<double (*)(double)>(
                      &Imp_Identity_Transfer::transfer), _1);
      transfer_first_deriv_ = boost::bind(
                                  &Imp_Identity_Transfer::first_derivative, _1);
      transfer_second_deriv_ = boost::bind(
                                  &Imp_Identity_Transfer::second_derivative, _1);
    }
    else if (transfer_name == "exp") {
      transfer_ = boost::bind(static_cast<double (*)(double)>(
                      &Imp_Exp_Transfer::transfer), _1);
      transfer_first_deriv_ = boost::bind(
                                  &Imp_Exp_Transfer::first_derivative, _1);
      transfer_second_deriv_ = boost::bind(
                                  &Imp_Exp_Transfer::second_derivative, _1);
    }
    else if (transfer_name == "logistic") {
      transfer_ = boost::bind(static_cast<double (*)(double)>(
                      &Imp_Logistic_Transfer::transfer), _1);
      transfer_first_deriv_ = boost::bind(
                                  &Imp_Logistic_Transfer::first_derivative, _1);
      transfer_second_deriv_ = boost::bind(
                                  &Imp_Logistic_Transfer::second_derivative, _1);
    }
  }

  void init_uni_dim_learning_rate(double gamma, double alpha, double c, double scale) {
    //lr_ = boost::bind(&uni_dim_learning_rate, _1, _2, _3, gamma, alpha, c, scale);
    lr_ = boost::bind(&Imp_Unidim_Learn_Rate::learning_rate, 
                      _1, _2, _3, _4, gamma, alpha, c, scale);
  }

  void init_px_dim_learning_rate() {
    score_func_type score_func = boost::bind(&Imp_Experiment::score_function, this, _1, _2);
    lr_ = boost::bind(&Imp_Pxdim_Learn_Rate::learning_rate,
                      _1, _2, _3, _4, score_func);
  }

  mat learning_rate(const mat& theta_old, const Imp_DataPoint& data_pt, unsigned t) const {
    //return lr(t);
    return lr_(theta_old, data_pt, t, p);
  }

  mat score_function(const mat& theta_old, const Imp_DataPoint& datapoint) const {
    return ((datapoint.y - h_transfer(as_scalar(datapoint.x * theta_old)))*datapoint.x).t();
  }

  double h_transfer(double u) const {
    return transfer_(u);
  }

  //YKuang
  double h_first_derivative(double u) const{
    return transfer_first_deriv_(u);
  }
  //YKuang
  double h_second_derivative(double u) const{
    return transfer_second_deriv_(u);
  }

private:
  uni_func_type transfer_;
  uni_func_type transfer_first_deriv_;
  uni_func_type transfer_second_deriv_;

  learning_rate_type lr_;
};

struct Imp_Size{
  Imp_Size():nsamples(0), p(0){}
  Imp_Size(unsigned nin, unsigned pin):nsamples(nin), p(pin) {}
  unsigned nsamples;
  unsigned p;
};

// Compute score function coeff and its derivative for Implicit-SGD update
struct Get_score_coeff{

  //Get_score_coeff(const Imp_Experiment<TRANSFER>& e, const Imp_DataPoint& d,
  Get_score_coeff(const Imp_Experiment& e, const Imp_DataPoint& d,
      const mat& t, double n) : experiment(e), datapoint(d), theta_old(t), normx(n) {}

  double operator() (double ksi) const{
    return datapoint.y-experiment.h_transfer(dot(theta_old, datapoint.x)
                     + normx * ksi);
  }

  double first_derivative (double ksi) const{
    return experiment.h_first_derivative(dot(theta_old, datapoint.x)
           + normx * ksi)*normx;
  }

  double second_derivative (double ksi) const{
    return experiment.h_second_derivative(dot(theta_old, datapoint.x)
             + normx * ksi)*normx*normx;
  }

  //const Imp_Experiment<TRANSFER>& experiment;
  const Imp_Experiment& experiment;
  const Imp_DataPoint& datapoint;
  const mat& theta_old;
  double normx;
};

// Root finding functor for Implicit-SGD update
struct Implicit_fn{
  typedef boost::math::tuple<double, double, double> tuple_type;

  //Implicit_fn(double a, const Get_score_coeff<TRANSFER>& get_score): at(a), g(get_score){}
  Implicit_fn(double a, const Get_score_coeff& get_score): at(a), g(get_score){}
  tuple_type operator() (double u) const{
    double value = u - at * g(u);
    double first = 1 + at * g.first_derivative(u);
    double second = at * g.second_derivative(u);
    tuple_type result(value, first, second);
    return result;
  }
  
  double at;
  //const Get_score_coeff<TRANSFER>& g;
  const Get_score_coeff& g;
};

#endif

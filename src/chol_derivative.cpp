#include <Rcpp.h>
#define INCLUDE_RCPP
#include "utils.h"
#include "covariance.h"

using namespace Rcpp;
using std::string;

struct chol {
  int dim_cov_mat;
  string cov_type;
  chol(int dim, string cov): dim_cov_mat(dim), cov_type(cov) {};
  template <class T>
    vector<T> operator() (vector<T> &theta) {  // Evaluate function
      //return theta;
      return get_cov_lower_chol_grouped(theta, this->dim_cov_mat, this->cov_type, 1, false).vec();
    }
};

struct chol_jacobian {
  int dim_cov_mat;
  string cov_type;
  chol mychol;
  chol_jacobian(int dim, string cov): dim_cov_mat(dim), cov_type(cov), mychol(dim, cov) {};
  template<class T>
    vector<T> operator() (vector<T> &theta) {
      return autodiff::jacobian(this->mychol, theta).vec();
    }
};

template<class Type>
struct return_result {
  Type value;
  return_result(Type value): value(value) {};
  Type operator() () {
    return this->value;
  }
};

vector<double> as_vector(NumericVector input) {
  vector<double> ret(as<std::vector<double>>(input));
  return ret;
}
vector<int> as_vector(IntegerVector input) {
  vector<int> ret(as<std::vector<int>>(input));
  return ret;
}
NumericVector as_nv(vector<double> input) {
  NumericVector ret(input.size());
  for (int i = 0; i < input.size(); i++) {
    ret[i] = input(i);
  }
  return ret;
}

NumericMatrix as_mv(matrix<double> input) {
  vector<double> input_v = input.vec();
  NumericVector input_nv = as_nv(input_v);
  NumericMatrix ret(int(input.rows()), int(input.cols()), input_nv.begin());
  return ret;
}
matrix<double> as_matrix(NumericMatrix input) {
  matrix<double> ret(input.rows(), input.cols());
  return ret;
}

template <class Type>
std::map<std::string, matrix<Type>> derivatives(int n_visits, std::string cov_type, vector<Type> theta) {
  std::map<std::string, matrix<Type>> ret;
  chol c1(n_visits, cov_type);
  chol_jacobian c2(n_visits, cov_type);
  matrix<Type> l = c1(theta).matrix();
  l.resize(n_visits, n_visits);
  //matrix<Type> sigmainv = tcrossprod<Type>(l.inverse(), true);
  ret["chol"] = l;
  vector<Type> g = autodiff::jacobian(c1, theta).vec(); // g is (dim * dim * l_theta)
  vector<Type> h = autodiff::jacobian(c2, theta).vec(); // h is (dim * dim * l_theta * l_theta)
  matrix<Type> ret_d1 = matrix<Type>(n_visits * theta.size(), n_visits);
  matrix<Type> ret_d2 = matrix<Type>(n_visits * theta.size() * theta.size(), n_visits);
  for (int i = 0; i < theta.size(); i++) {
    matrix<Type> d1 = g.segment(i * n_visits, n_visits * n_visits).matrix();
    d1.resize(n_visits, n_visits);
    matrix<Type> pllt = d1 * l.transpose();
    auto sigma_d1_i = pllt + pllt.transpose();
    ret_d1.block(i * n_visits, 0, n_visits, n_visits) = sigma_d1_i;
    for (int j = 0; j < theta.size(); j++) {
      matrix<Type> d2 = h.segment( (i * theta.size() + j) * n_visits * n_visits, n_visits * n_visits).matrix();
      d2.resize(n_visits, n_visits);
      auto p2llt = d2 * l.transpose();
      auto sigma_d2_ij = p2llt + p2llt.transpose() + 2 * pllt;
      ret_d2.block((i * theta.size() + j) * n_visits, 0, n_visits, n_visits) = sigma_d2_ij;
    }
  }
  ret["derivative1"] = ret_d1;
  ret["derivative2"] = ret_d2;
  return ret;
}

template <class Type>
struct chols {
  std::map<std::vector<int>, matrix<Type>> inverse_cache;
  std::map<std::vector<int>, matrix<Type>> sigmad1_cache;
  std::map<std::vector<int>, matrix<Type>> sigmad2_cache;
  std::map<std::vector<int>, matrix<Type>> sigma_cache;
  std::string cov_type;
  std::vector<int> full_visit;
  int n_visits;
  int n_theta; // theta.size()
  vector<Type> theta;
  chols(bool spatial, vector<Type> theta, int n_visits, std::string cov_type): n_visits(n_visits), cov_type(cov_type) {
    this->theta = theta;
    for (int i = 0; i < n_theta; i++) {
      this->full_visit(i) = i;
    }
    this->n_theta = theta.size();
    auto allret = derivatives(n_visits, cov_type, theta);
    auto l1 = allret["derivative1"];
    auto l2 = allret["derivative2"];
    auto l = allret["chol"];
    auto l1_lt = l1 * l.transpose();
    auto l2_lt = l2 * l.transpose();
    this->sigmad1_cache[this->full_visit] = l + l.transpose();
    this->sigmad1_cache[this->full_visit] = l1_lt + l1_lt.transpose();
    this->sigmad2_cache[this->full_visit] = l2_lt + l2_lt.transpose() + l1 * l1.transpose();
    this->inverse_cache[this->full_visit] = allret["chol"];
  }
  matrix<Type> get_sigma_derivative1(std::vector<int> visits) {
     if (this->sigmad1_cache.contains(visits)) {
      return this->sigmad1_cache[visits];
    } else {
      Eigen::SparseMatrix<Type> sel_mat = get_select_matrix<Type>(visits, this -> n_visits);
      int n_visists_i = visits.size();
      matrix<Type> ret = matrix<Type>(this->n_theta * n_visists_i, n_visists_i);
      for (int i = 0; i < this->n_theta; i++) {
        ret.block(i  * n_visists_i, 0, n_visists_i, n_visists_i) = sel_mat * this->sigmad1_cache[visits].block(i  * this->n_visits, 0, n_visits, n_visits) * sel_mat.transpose();
      }
      this->sigmad1_cache[visits] = ret;
      return ret;
    }
  }
  matrix<Type> get_sigma_derivative2(std::vector<int> visits) {
     if (this->sigmad2_cache.contains(visits)) {
      return this->sigmad2_cache[visits];
    } else {
      Eigen::SparseMatrix<Type> sel_mat = get_select_matrix<Type>(visits, this -> n_visits);
      int n_visists_i = visits.size();
      matrix<Type> ret = matrix<Type>(this->n_theta * n_visists_i, n_visists_i);
      for (int i = 0; i < this->n_theta; i++) {
        ret.block(i  * n_visists_i, 0, n_visists_i, n_visists_i) = sel_mat * this->sigmad2_cache[visits].block(i  * this->n_visits, 0, n_visits, n_visits) * sel_mat.transpose();
      }
      this->sigmad2_cache[visits] = ret;
      return ret;
    }
  }
  matrix<Type> get_sigma(std::vector<int> visits) {
     if (this->sigma_cache.contains(visits)) {
      return this->sigma_cache[visits];
    } else {
      Eigen::SparseMatrix<Type> sel_mat = get_select_matrix<Type>(visits, this -> n_visits);
      int n_visists_i = visits.size();
      matrix<Type> ret = sel_mat * this->sigma_cache[visits] * sel_mat.transpose();
      this->sigma_cache[visits] = ret;
      return ret;
    }
  }
  matrix<Type> get_inverse(std::vector<int> visits) {
    if (this->inverse_cache.contains(visits)) {
      return this->inverse_cache[visits];
    } else {
      Eigen::SparseMatrix<Type> sel_mat = get_select_matrix<Type>(visits, this -> n_visits);
      matrix<Type> Ltildei = sel_mat * this->chol_cache[this->full_visit];
      matrix<Type> cov_i = tcrossprod(Ltildei);
      Eigen::LLT<Eigen::Matrix<Type,Eigen::Dynamic,Eigen::Dynamic> > cov_i_chol(cov_i);
      auto ret = cov_i_chol.matrixL();
      auto cholinv = ret.solve();
      auto sigmainv = tcrossprod(cholinv, true);
      this->inverse_cache[visits] = sigmainv;
      return sigmainv;
    }
  }
};

List test2(NumericMatrix x, IntegerVector subject_zero_inds, IntegerVector visits_zero_inds, int n_subjects, IntegerVector subject_n_visits, int n_visits, string cov_type, bool is_spatial, NumericVector theta) {
  auto theta_v = as_vector(theta);
  auto mychol = chols(is_spatial, theta_v, n_visits, cov_type);
  int p = x.cols();
  int n_theta = theta.size();
  matrix<double> P(p * n_theta, p);
  matrix<double> Q(p * n_theta * n_theta, p);
  matrix<double> R(p * n_theta * n_theta, p);
  
  for (int i = 0; i < n_subjects; i++) {
    int start_i = subject_zero_inds[i];
    int n_visits_i = subject_n_visits[i];
    int start_i = subject_zero_inds[i];
    int n_visits_i = subject_n_visits[i];

    auto visiti = as_vector(visits_zero_inds).segment(start_i, n_visits_i);
    std::vector<int> visit_i = std::vector<int>(vector<int>(visiti));
    auto x_matrix = as_matrix(x);
    matrix<double> Xi = x_matrix.block(start_i, 0, n_visits_i, x_matrix.cols());
    auto sigma_inv = mychol.get_inverse(visit_i);
    auto sigma_d1 = mychol.get_sigma_derivative1(visit_i);
    auto sigma_d2 = mychol.get_sigma_derivative1(visit_i);
    auto sigma = mychol.get_sigma(visit_i);
    matrix<double> sigma_inv_d1(sigma_d1.rows(), sigma_d1.cols());
    for (int r = 0; r < theta.size(); r++) {
      sigma_inv_d1.block(r * n_visits_i, 0, n_visits_i, n_visits_i) = sigma_inv * sigma_d1.block(r * n_visits_i, 0, n_visits_i, n_visits_i) *sigma_inv;
    }
    for (int r = 0; r < theta.size(); r ++) {
      auto Pi = Xi.transpose() * sigma_inv_d1.block(r * n_visits_i, 0, n_visits_i, n_visits_i) * Xi;
      P.block(r * p, 0, p, p) += Pi;
      for (int j = 0; j < theta.size(); j++) {
        auto Qij = Xi.transpose() * sigma_inv_d1.block(r * n_visits_i, 0, n_visits_i, n_visits_i) * sigma * sigma_inv_d1.block(j * n_visits_i, 0, n_visits_i, n_visits_i) * Xi;
        Q.block((r * p + j) * p, 0, p, p) += Qij;
        auto Rij = Xi.transpose() * sigma_inv * sigma_d2.block((r * n_visits_i + j) * n_visits_i, 0, n_visits_i, n_visits_i) * sigma_inv * Xi;
        R.block((r * p + j) * p, 0, p, p) += Rij;
      }
    }
  }
  return List::create(
    as_mv(P),
    as_mv(Q),
    as_mv(R)
  );
}

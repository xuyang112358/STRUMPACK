/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The
 * Regents of the University of California, through Lawrence Berkeley
 * National Laboratory (subject to receipt of any required approvals
 * from the U.S. Dept. of Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As
 * such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research
 *             Division).
 */

#include "StrumpackSparseSolver.hpp"

#if defined(STRUMPACK_USE_PAPI)
#include <papi.h>
#endif

#include "misc/Tools.hpp"
#include "misc/TaskTimer.hpp"
#include "StrumpackOptions.hpp"
#include "sparse/ordering/MatrixReordering.hpp"
#include "sparse/EliminationTree.hpp"
#include "sparse/iterative/IterativeSolvers.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t>
  StrumpackSparseSolver<scalar_t,integer_t>::StrumpackSparseSolver
  (bool verbose, bool root)
    : StrumpackSparseSolverBase<scalar_t,integer_t>
    (verbose, root) { }

  template<typename scalar_t,typename integer_t>
  StrumpackSparseSolver<scalar_t,integer_t>::StrumpackSparseSolver
  (int argc, char* argv[], bool verbose, bool root)
    : StrumpackSparseSolverBase<scalar_t,integer_t>
    (argc, argv, verbose, root) { }

  template<typename scalar_t,typename integer_t>
  StrumpackSparseSolver<scalar_t,integer_t>::
  ~StrumpackSparseSolver() = default;


  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::setup_tree() {
    tree_.reset(new EliminationTree<scalar_t,integer_t>
                (opts_, *mat_, nd_->tree()));
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::setup_reordering() {
    nd_.reset(new MatrixReordering<scalar_t,integer_t>(matrix()->size()));
  }

  template<typename scalar_t,typename integer_t> int
  StrumpackSparseSolver<scalar_t,integer_t>::compute_reordering
  (const int* p, int base, int nx, int ny, int nz,
   int components, int width) {
    if (p) return nd_->set_permutation(opts_, *mat_, p, base);
    return nd_->nested_dissection
      (opts_, *mat_, nx, ny, nz, components, width);
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::separator_reordering() {
    nd_->separator_reordering(opts_, *mat_, tree_->root());
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::set_matrix
  (const CSRMatrix<scalar_t,integer_t>& A) {
    mat_.reset(new CSRMatrix<scalar_t,integer_t>(A));
    factored_ = reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::update_matrix_values
  (const CSRMatrix<scalar_t,integer_t>& A) {
    if (!(mat_ && A.size() == mat_->size() && A.nnz() <= mat_->nnz())) {
      // matrix() has been made symmetric, can have more nonzeros
      this->print_wrong_sparsity_error();
      return;
    }
    mat_.reset(new CSRMatrix<scalar_t,integer_t>(A));
    permute_matrix_values();
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::set_csr_matrix
  (integer_t N, const integer_t* row_ptr, const integer_t* col_ind,
   const scalar_t* values, bool symmetric_pattern) {
    mat_.reset(new CSRMatrix<scalar_t,integer_t>
               (N, row_ptr, col_ind, values, symmetric_pattern));
    factored_ = reordered_ = false;
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::update_matrix_values
  (integer_t N, const integer_t* row_ptr, const integer_t* col_ind,
   const scalar_t* values, bool symmetric_pattern) {
    if (!(mat_ && N == mat_->size() && row_ptr[N] <= mat_->nnz())) {
      // matrix() has been made symmetric, can have more nonzeros
      this->print_wrong_sparsity_error();
      return;
    }
    mat_.reset(new CSRMatrix<scalar_t,integer_t>
               (N, row_ptr, col_ind, values, symmetric_pattern));
    permute_matrix_values();
  }

  template<typename scalar_t,typename integer_t> void
  StrumpackSparseSolver<scalar_t,integer_t>::permute_matrix_values() {
    if (reordered_) {
      if (opts_.matching() != MatchingJob::NONE) {
        if (opts_.matching() == MatchingJob::MAX_DIAGONAL_PRODUCT_SCALING)
          matrix()->apply_scaling(Dr_, Dc_);
        matrix()->apply_column_permutation(cperm_);
      }
      matrix()->symmetrize_sparsity();
      matrix()->permute(reordering()->iperm(), reordering()->perm());
      if (opts_.compression() != CompressionType::NONE)
        separator_reordering();
    }
    factored_ = false;
  }

  template<typename scalar_t,typename integer_t> ReturnCode
  StrumpackSparseSolver<scalar_t,integer_t>::solve_internal
  (const scalar_t* b, scalar_t* x, bool use_initial_guess) {
    auto N = matrix()->size();
    auto B = ConstDenseMatrixWrapperPtr(N, 1, b, N);
    DenseMW_t X(N, 1, x, N);
    return this->solve(*B, X, use_initial_guess);
  }

  // TODO make this const
  //  Krylov its and flops, bytes, time are modified!!
  // pass those as a pointer to a struct ??
  // this can also call factor if not already factored!!
  template<typename scalar_t,typename integer_t> ReturnCode
  StrumpackSparseSolver<scalar_t,integer_t>::solve_internal
  (const DenseM_t& b, DenseM_t& x, bool use_initial_guess) {
    if (!this->factored_ &&
        opts_.Krylov_solver() != KrylovSolver::GMRES &&
        opts_.Krylov_solver() != KrylovSolver::BICGSTAB) {
      ReturnCode ierr = this->factor();
      if (ierr != ReturnCode::SUCCESS) return ierr;
    }
    TaskTimer t("solve");
    this->perf_counters_start();
    t.start();

    integer_t N = matrix()->size(), d = b.cols();
    assert(N < std::numeric_limits<int>::max());

    DenseM_t bloc(b.rows(), b.cols());

    // TODO this fails when the reordering was not done, for instance
    // for iterative solvers!!!
    auto iperm = reordering()->iperm();
    if (use_initial_guess &&
        opts_.Krylov_solver() != KrylovSolver::DIRECT) {
      if (opts_.matching() != MatchingJob::NONE) {
        if (opts_.matching() == MatchingJob::MAX_DIAGONAL_PRODUCT_SCALING) {
          for (integer_t j=0; j<d; j++)
#pragma omp parallel for
            for (integer_t i=0; i<N; i++) {
              auto pi = iperm[cperm_[i]];
              bloc(i, j) = x(pi, j) / Dc_[pi];
            }
        } else {
          for (integer_t j=0; j<d; j++)
#pragma omp parallel for
            for (integer_t i=0; i<N; i++)
              bloc(i, j) = x(iperm[cperm_[i]], j);
        }
      } else {
        for (integer_t j=0; j<d; j++)
#pragma omp parallel for
          for (integer_t i=0; i<N; i++)
            bloc(i, j) = x(iperm[i], j);
      }
      x.copy(bloc);
    }
    if (opts_.matching() == MatchingJob::MAX_DIAGONAL_PRODUCT_SCALING) {
      for (integer_t j=0; j<d; j++)
#pragma omp parallel for
        for (integer_t i=0; i<N; i++) {
          auto pi = iperm[i];
          bloc(i, j) = Dr_[pi] * b(pi, j);
        }
    } else {
      for (integer_t j=0; j<d; j++)
#pragma omp parallel for
        for (integer_t i=0; i<N; i++)
          bloc(i, j) = b(iperm[i], j);
    }

    Krylov_its_ = 0;

    auto spmv = [&](const scalar_t* x, scalar_t* y) {
      matrix()->spmv(x, y);
    };

    auto gmres_solve =
      [&](const std::function<void(scalar_t*)>& prec) {
        iterative::GMRes<scalar_t>
          (spmv, prec, x.rows(), x.data(), bloc.data(),
           opts_.rel_tol(), opts_.abs_tol(), Krylov_its_, opts_.maxit(),
           opts_.gmres_restart(), opts_.GramSchmidt_type(),
           use_initial_guess, opts_.verbose() && is_root_);
      };
    auto bicgstab_solve =
      [&](const std::function<void(scalar_t*)>& prec) {
        iterative::BiCGStab<scalar_t>
          (spmv, prec, x.rows(), x.data(), bloc.data(),
           opts_.rel_tol(), opts_.abs_tol(), Krylov_its_, opts_.maxit(),
           use_initial_guess, opts_.verbose() && is_root_);
      };
    auto MFsolve =
      [&](scalar_t* w) {
        DenseMW_t X(x.rows(), 1, w, x.ld());
        tree()->multifrontal_solve(X);
      };
    auto refine =
      [&]() {
        iterative::IterativeRefinement<scalar_t,integer_t>
          (*matrix(), [&](DenseM_t& w) { tree()->multifrontal_solve(w); },
           x, bloc, opts_.rel_tol(), opts_.abs_tol(),
           Krylov_its_, opts_.maxit(), use_initial_guess,
           opts_.verbose() && is_root_);
      };

    switch (opts_.Krylov_solver()) {
    case KrylovSolver::AUTO: {
      if (opts_.compression() != CompressionType::NONE && x.cols() == 1)
        gmres_solve(MFsolve);
      else refine();
    }; break;
    case KrylovSolver::DIRECT: {
      x = bloc;
      tree()->multifrontal_solve(x);
    }; break;
    case KrylovSolver::REFINE: {
      refine();
    }; break;
    case KrylovSolver::PREC_GMRES: {
      assert(x.cols() == 1);
      gmres_solve(MFsolve);
    }; break;
    case KrylovSolver::GMRES: {
      assert(x.cols() == 1);
      gmres_solve([](scalar_t* x){});
    }; break;
    case KrylovSolver::PREC_BICGSTAB: {
      assert(x.cols() == 1);
      bicgstab_solve(MFsolve);
    }; break;
    case KrylovSolver::BICGSTAB: {
      assert(x.cols() == 1);
      bicgstab_solve([](scalar_t* x){});
    }; break;
    }

    if (opts_.matching() != MatchingJob::NONE) {
      if (opts_.matching() == MatchingJob::MAX_DIAGONAL_PRODUCT_SCALING) {
        for (integer_t j=0; j<d; j++)
#pragma omp parallel for
          for (integer_t i=0; i<N; i++) {
            auto ipi = cperm_[iperm[i]];
            bloc(ipi, j) = x(i, j) * Dc_[ipi];
          }
      } else {
        for (integer_t j=0; j<d; j++)
#pragma omp parallel for
          for (integer_t i=0; i<N; i++)
            bloc(cperm_[iperm[i]], j) = x(i, j);
      }
    } else {
      auto perm = reordering()->perm();
      for (integer_t j=0; j<d; j++)
#pragma omp parallel for
        for (integer_t i=0; i<N; i++)
          bloc(i, j) = x(perm[i], j);
    }
    x.copy(bloc);

    t.stop();
    this->perf_counters_stop("DIRECT/GMRES solve");
    this->print_solve_stats(t);
    return ReturnCode::SUCCESS;
  }

  // explicit template instantiations
  template class StrumpackSparseSolver<float,int>;
  template class StrumpackSparseSolver<double,int>;
  template class StrumpackSparseSolver<std::complex<float>,int>;
  template class StrumpackSparseSolver<std::complex<double>,int>;

  template class StrumpackSparseSolver<float,long int>;
  template class StrumpackSparseSolver<double,long int>;
  template class StrumpackSparseSolver<std::complex<float>,long int>;
  template class StrumpackSparseSolver<std::complex<double>,long int>;

  template class StrumpackSparseSolver<float,long long int>;
  template class StrumpackSparseSolver<double,long long int>;
  template class StrumpackSparseSolver<std::complex<float>,long long int>;
  template class StrumpackSparseSolver<std::complex<double>,long long int>;

} //end namespace strumpack

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
 *
 */
#ifndef FRONTAL_MATRIX_CUBLAS_HPP
#define FRONTAL_MATRIX_CUBLAS_HPP

#include "FrontalMatrixDense.hpp"
#include "dense/CUDAWrapper.hpp"

namespace strumpack {

  template<typename scalar_t,typename integer_t> class FrontalMatrixCUBLAS
    : public FrontalMatrix<scalar_t,integer_t> {
    using F_t = FrontalMatrix<scalar_t,integer_t>;
    using FC_t = FrontalMatrixCUBLAS<scalar_t,integer_t>;
    using DenseM_t = DenseMatrix<scalar_t>;
    using DenseMW_t = DenseMatrixWrapper<scalar_t>;
    using SpMat_t = CompressedSparseMatrix<scalar_t,integer_t>;
    using uniq_scalar_t = std::unique_ptr
      <scalar_t[], std::function<void(scalar_t*)>>;

  public:
    FrontalMatrixCUBLAS
    (integer_t sep, integer_t sep_begin, integer_t sep_end,
     std::vector<integer_t>& upd);

    void release_work_memory() override;

    void extend_add_to_dense
    (DenseM_t& paF11, DenseM_t& paF12, DenseM_t& paF21, DenseM_t& paF22,
     const F_t* p, int task_depth) override;

    void sample_CB
    (const SPOptions<scalar_t>& opts, const DenseM_t& R,
     DenseM_t& Sr, DenseM_t& Sc, F_t* pa, int task_depth) override;
    void sample_CB
    (Trans op, const DenseM_t& R, DenseM_t& S, F_t* pa,
     int task_depth=0) const override;

    void sample_CB_to_F11
    (Trans op, const DenseM_t& R, DenseM_t& S, F_t* pa,
     int task_depth=0) const override;
    void sample_CB_to_F12
    (Trans op, const DenseM_t& R, DenseM_t& S, F_t* pa,
     int task_depth=0) const override;
    void sample_CB_to_F21
    (Trans op, const DenseM_t& R, DenseM_t& S, F_t* pa,
     int task_depth=0) const override;
    void sample_CB_to_F22
    (Trans op, const DenseM_t& R, DenseM_t& S, F_t* pa,
     int task_depth=0) const override;

    void multifrontal_factorization
    (const SpMat_t& A, const SPOptions<scalar_t>& opts,
     int etree_level=0, int task_depth=0) override;

    //void multifrontal_solve(DenseM_t& b) const override;

    void forward_multifrontal_solve
    (DenseM_t& b, DenseM_t* work, int etree_level=0,
     int task_depth=0) const override;
    void backward_multifrontal_solve
    (DenseM_t& y, DenseM_t* work, int etree_level=0,
     int task_depth=0) const override;

    void extract_CB_sub_matrix
    (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
     DenseM_t& B, int task_depth) const override;

    std::string type() const override { return "FrontalMatrixCUBLAS"; }

#if defined(STRUMPACK_USE_MPI)
    void extend_add_copy_to_buffers
    (std::vector<std::vector<scalar_t>>& sbuf,
     const FrontalMatrixMPI<scalar_t,integer_t>* pa) const override;
#endif

  private:
    uniq_scalar_t factor_mem_;
    DenseMW_t F11_, F12_, F21_, F22_;
    std::vector<int> piv; // regular int because it is passed to BLAS
    void* all_work_mem_ = nullptr;


    FrontalMatrixCUBLAS(const FrontalMatrixCUBLAS&) = delete;
    FrontalMatrixCUBLAS& operator=(FrontalMatrixCUBLAS const&) = delete;

    void fwd_solve_phase2
    (DenseM_t& b, DenseM_t& bupd, int etree_level, int task_depth) const;
    void bwd_solve_phase1
    (DenseM_t& y, DenseM_t& yupd, int etree_level, int task_depth) const;

    using F_t::lchild_;
    using F_t::rchild_;
    using F_t::dim_sep;
    using F_t::dim_upd;
  };

} // end namespace strumpack

#endif // FRONTAL_MATRIX_CUBLAS_HPP

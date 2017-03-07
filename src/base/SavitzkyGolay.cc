#include "SavitzkyGolay.h"

#include "base/std_ext/string.h"

extern "C" {
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_poly.h>
}

using namespace std;
using namespace ant;

struct SavitzkyGolay::gsl_matrix : ::gsl_matrix {};

SavitzkyGolay::SavitzkyGolay(int n_l, int n_r, int m) :
    d_sav_gol_points(n_l),
    d_smooth_points(n_r),
    d_polynom_order(m)
{
    int points = d_sav_gol_points + d_smooth_points + 1;
    int polynom_order = d_polynom_order;

    // define some unique_ptr alloc for matrix
    auto gsl_matrix_alloc = [] (size_t n1, size_t n2) {
        return gsl_matrix_t(static_cast<gsl_matrix*>(::gsl_matrix_alloc(n1, n2)), ::gsl_matrix_free);
    };

    // the code in the following will  eventually set h (the precomputed Savitzky Golay coefficients)
    h = gsl_matrix_alloc(points, points);

    // gsl_permutation is only used here, so provide just the exception-safe unique_ptr allocator
    auto gsl_permutation_alloc = [] (size_t n) {
        return deleted_unique_ptr<gsl_permutation>(static_cast<gsl_permutation*>(::gsl_permutation_alloc(n)), ::gsl_permutation_free);
    };

    // provide error handling, as we're using unique_ptr, the code is exception safe!
    auto catch_gsl_error = [] (int error) {
        if(error == GSL_SUCCESS)
            return;
        throw std::runtime_error(std_ext::formatter() << "Encountered GSL error: " << gsl_strerror(error));
    };

    // compute Vandermonde matrix
    auto vandermonde = gsl_matrix_alloc(points, polynom_order + 1);
    for (int i = 0; i < points; ++i){
        gsl_matrix_set(vandermonde.get(), i, 0, 1.0);
        for (int j = 1; j <= polynom_order; ++j)
            gsl_matrix_set(vandermonde.get(), i, j, gsl_matrix_get(vandermonde.get(), i, j - 1) * i);
    }

    // compute V^TV
    auto vtv = gsl_matrix_alloc(polynom_order + 1, polynom_order + 1);
    catch_gsl_error(gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, vandermonde.get(), vandermonde.get(), 0.0, vtv.get()));

    // compute (V^TV)^(-1) using LU decomposition
    auto p = gsl_permutation_alloc(polynom_order + 1);
    int signum;
    catch_gsl_error(gsl_linalg_LU_decomp(vtv.get(), p.get(), &signum));

    auto vtv_inv = gsl_matrix_alloc(polynom_order + 1, polynom_order + 1);
    catch_gsl_error(gsl_linalg_LU_invert(vtv.get(), p.get(), vtv_inv.get()));

    // compute (V^TV)^(-1)V^T
    auto vtv_inv_vt = gsl_matrix_alloc(polynom_order + 1, points);
    catch_gsl_error(gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1.0, vtv_inv.get(), vandermonde.get(), 0.0, vtv_inv_vt.get()));

    // finally, compute H = V(V^TV)^(-1)V^T
    catch_gsl_error(gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, vandermonde.get(), vtv_inv_vt.get(), 0.0, h.get()));
}

vector<double> SavitzkyGolay::Smooth(const vector<double>& y)
{
    vector<double> result(y.size());
    int d_n = y.size();
    int points = d_sav_gol_points + d_smooth_points + 1;

    // handle left edge by zero padding
    for (int i = 0; i < d_sav_gol_points; i++){
        double convolution = 0.0;
        for (int k = d_sav_gol_points - i; k < points; k++)
            convolution += gsl_matrix_get(h.get(), d_sav_gol_points, k) * y[i - d_sav_gol_points + k];
        result[i] = convolution;
    }
    // central part: convolve with fixed row of h (as given by number of left points to use)
    for (int i = d_sav_gol_points; i < d_n - d_smooth_points; i++){
        double convolution = 0.0;
        for (int k = 0; k < points; k++)
            convolution += gsl_matrix_get(h.get(), d_sav_gol_points, k) * y[i - d_sav_gol_points + k];
        result[i] = convolution;
    }

    // handle right edge by zero padding
    for (int i = d_n - d_smooth_points; i < d_n; i++){
        double convolution = 0.0;
        for (int k = 0; i - d_sav_gol_points + k < d_n; k++)
            convolution += gsl_matrix_get(h.get(), d_sav_gol_points, k) * y[i - d_sav_gol_points + k];
        result[i] = convolution;
    }

    return result;
}

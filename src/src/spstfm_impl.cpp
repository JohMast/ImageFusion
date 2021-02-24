// /**
//  * @file
//  * @brief Implementation details of SPSTFM
//  */
// 
// #include <vector>
// #include <algorithm>
// #include <random>
// #include <chrono>
// #include <numeric>
// #include <map>
// #include <set>
// 
// #include <armadillo>
// 
// #include <opencv2/opencv.hpp>
// 
// #include "exceptions.h"
// #include "type.h"
// #include "image.h"
// #include "spstfm.h"
// #include <Rcpp.h>
// namespace imagefusion {
// namespace spstfm_impl_detail {
// 
// 
// arma::mat extractPatch(imagefusion::ConstImage const& img, int pxi, int pyi, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     imagefusion::Point p0{pxi * static_cast<int>(patchSize - patchOverlap) + sampleArea.x,
//                           pyi * static_cast<int>(patchSize - patchOverlap) + sampleArea.y};
//     return imagefusion::CallBaseTypeFunctor::run(GetPatchFunctor{img, p0, patchSize, channel}, img.type());
// }
// 
// arma::mat extractPatch(imagefusion::ConstImage const& img1, imagefusion::ConstImage const& img2, int pxi, int pyi, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     return extractPatch(img1, pxi, pyi, patchSize, patchOverlap, sampleArea, channel) - extractPatch(img2, pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
// }
// 
// 
// // in a special case, when patchSize is even, patchOverlap = patchSize / 2 and the image has no bound at all on one side (which this function does not know), then actually the 'optimal size' is one patch more than optimal
// imagefusion::Rectangle calcRequiredArea(imagefusion::Rectangle predArea, unsigned int patchSize, unsigned int patchOverlap) {
//     assert(patchOverlap <= patchSize / 2 && "multi patch overlap is currently not supported");
// 
//     // number of patches
//     unsigned int npx = (predArea.width  + patchOverlap - 1) / (patchSize - patchOverlap) + 1;
//     unsigned int npy = (predArea.height + patchOverlap - 1) / (patchSize - patchOverlap) + 1;
// 
//     imagefusion::Rectangle r;
//     r.width  = npx * (patchSize - patchOverlap) + patchOverlap;
//     r.height = npy * (patchSize - patchOverlap) + patchOverlap;
// 
//     int diffX = r.width  - predArea.width;
//     int diffY = r.height - predArea.height;
//     r.x = predArea.x - diffX / 2;
//     r.y = predArea.y - diffY / 2;
// 
//     return r;
// }
// 
// 
// 
// arma::vec gpsr(arma::mat const& y, arma::mat const& A, imagefusion::SpstfmOptions::GPSROptions const& opt, double* tau_out) {
//     constexpr double alphamin = 1e-30;
//     constexpr double alphamax = 1e30;
//     unsigned int n = static_cast<unsigned int>(A.n_cols);
//     unsigned int dim = static_cast<unsigned int>(y.n_rows);
//     assert(static_cast<unsigned int>(A.n_rows) == dim && "y and A should have the same number of rows");
//     assert(y.n_cols == 1 && "y must be a single vector");
// 
//     // init tau with 0.1 * max(|A' * y|) or take the explicitly set value
//     arma::vec Aty = (y.t() * A).t();
//     double min = arma::min(Aty);
//     double max = arma::max(Aty);
//     double finaltau = opt.tau;
//     if (finaltau < 0)
//         finaltau = 0.1 * std::max(std::abs(min), std::abs(max));
//     if (tau_out != nullptr)
//         *tau_out = finaltau;
//     double tau = finaltau;
//     double tola = opt.tolA;
//     if (opt.continuation) {
//         tau *= 2;
//         tola *= 10;
//     }
// 
//     double alpha = 1;
// 
//     arma::vec x(n, arma::fill::zeros);
//     arma::vec u(n, arma::fill::zeros);
//     arma::vec v(n, arma::fill::zeros);
//     arma::vec uv_min(n);
//     arma::vec res = y;
//     arma::vec Ax(dim, arma::fill::zeros);
//     arma::vec term(n);
//     arma::vec gradu(n);
//     arma::vec gradv(n);
//     arma::vec du(n);
//     arma::vec dv(n);
//     arma::vec dx(n);
//     arma::vec Adx(dim);
//     auto fun = [&] () { return 0.5 * arma::dot(res,res) + tau * arma::norm(x, 1); };
//     double f_val = fun();
//     double f_val_old;
// 
//     unsigned int it = 0;
//     unsigned int nnz;
// //    do {
//     // continuation loop
//     do {
//         // update tau
//         if (it > 0) {
//             arma::vec gradq = (res.t() * A).t();
//             tau = std::max({0.2 * std::abs(arma::min(gradq)),
//                             0.2 * std::abs(arma::max(gradq)),
//                             finaltau});
//             if (tau == finaltau)
//                 tola = opt.tolA;
// //            Rcpp::Rcerr << "Changed tau to " << tau << std::endl;
//         }
// 
//         // main loop
//         do {
//             // compute gradient
//             term = (Ax.t() * A).t() - Aty;
//             gradu = tau + term;
//             gradv = tau - term;
// 
//             // projection and computation of search direction vector
//             du = arma::clamp(u - alpha * gradu, 0.0, std::numeric_limits<double>::max()) - u;
//             dv = arma::clamp(v - alpha * gradv, 0.0, std::numeric_limits<double>::max()) - v;
//             dx = du - dv;
// 
//             // find minimizing lambda along the direction (du,dv) (for monotonicity)
//             Adx = A * dx;
//             double dGd = arma::dot(Adx, Adx);
//             double lambda = - (arma::dot(gradu, du) + arma::dot(gradv, dv)) / (std::numeric_limits<double>::denorm_min() + dGd);
//             if (lambda < 0) {
//                 Rcpp::Rcerr << "lambda < 0. Debug info:" << std::endl;
//                 Rcpp::Rcerr << "gradu: " << gradu << std::endl;
//                 Rcpp::Rcerr << "du: " << du << std::endl;
//                 Rcpp::Rcerr << "gradv: " << gradv << std::endl;
//                 Rcpp::Rcerr << "dv: " << dv << std::endl;
//                 Rcpp::Rcerr << "dGd: " << dGd << std::endl;
//                 Rcpp::Rcerr << "alpha: " << alpha << std::endl;
//                 Rcpp::Rcerr << "setting lambda to 1 and continuing." << std::endl;
//                 lambda = 1;
//             }
// //            assert(lambda >= 0 && "lambda is negative. Abort.");
//             lambda = std::min(lambda, 1.0);
// 
//             // compute new coefficients
//             u += lambda * du;
//             v += lambda * dv;
//             uv_min = arma::min(u, v);
//             u -= uv_min;
//             v -= uv_min;
//             x = u - v;
// 
//             // evaluate stopping criteria
//             {
//                 arma::vec nzs = arma::nonzeros(x);
//                 nnz = nzs.n_elem;
//             }
//             Ax += lambda * Adx;
//             res = y - Ax;
//             f_val_old = f_val;
//             f_val = fun();
// 
//             // compute new alpha
//             double dd = arma::dot(du,du) + arma::dot(dv,dv);
//             if (dGd <= 0) {
// //                Rcpp::Rcerr << "dgd = " << dGd << ", nonpositive curvature detected" << std::endl;
//                 alpha = alphamax;
//             }
//             else
//                 alpha = std::min(std::max(alphamin, dd / dGd), alphamax);
// 
// //            Rcpp::Rcerr << "It: " << it << ", obj: " << f_val << ", alpha: " << alpha << ", nnz: " << nnz << std::endl;
//             ++it;
//         } while(it <= opt.minIterA || (std::abs(f_val - f_val_old) / f_val_old > tola && it <= opt.maxIterA));
//     } while (tau > finaltau);
// //    } while (nnz > dim);
// 
//     // debiasing via conjugate gradients method
//     it = 0;
//     if (opt.debias && 0 < nnz && nnz <= dim) {
//         arma::uvec zeroIdx = find(x == 0);
//         res *= -1;
//         arma::vec rvec = (res.t() * A).t();
//         rvec(zeroIdx).zeros();
//         double rTr = arma::dot(rvec,rvec);
//         if (rTr == 0)
//             return x;
// 
//         // set convergence threshold for the residual || RW x_debias - y ||_2
//         double converge = opt.tolD * rTr;
//         arma::vec pvec = -rvec;
//         arma::vec RWpvec;
//         arma::vec Apvec;
//         do {
//             // calculate A*p = Wt * Rt * R * W * pvec
//             RWpvec = A * pvec;
//             Apvec = (RWpvec.t() * A).t();
//             Apvec(zeroIdx).zeros();
// 
//             // calculate alpha for CG
//             double alpha_cg = rTr / arma::dot(pvec, Apvec);
//             if (!std::isfinite(alpha_cg))
//                 break;
// 
//             // take the step
//             x += alpha_cg * pvec;
//             res += alpha_cg * RWpvec;
//             rvec += alpha_cg * Apvec;
// 
//             double rTr_plus = arma::dot(rvec,rvec);
//             double beta = rTr_plus / rTr;
//             pvec *= beta;
//             pvec -= rvec;
//             rTr = rTr_plus;
// 
//             ++it;
//         } while (it < opt.minIterD || (rTr > converge && it < opt.maxIterD));
//     }
// //    Rcpp::Rcerr << "done " << it << " debias iterations." << std::endl;
//     return x;
// }
// 
// 
// void ksvd_iteration(unsigned int k, arma::mat const& samples, arma::mat const& dict,  arma::mat& new_dict, arma::mat& coeff, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling) {
//     unsigned int natoms   = static_cast<unsigned int>(dict.n_cols);
// 
//     // get indices of coefficients
//     arma::uvec row_indices = arma::linspace<arma::uvec>(0, natoms - 1, natoms);
//     row_indices.shed_row(k);
// 
//     arma::uvec col_indices = arma::find(coeff.row(k));
//     unsigned int nnz = col_indices.n_elem;
//     if (nnz == 0)
//         return;
// 
//     // reduce and add Y alias samples
//     auto samples_r = samples.cols(col_indices);
//     auto const dict_r = useOnlineMode ? new_dict.cols(row_indices)
//                                       : dict.cols(row_indices);
//     auto coeff_r = coeff.submat(row_indices, col_indices);
// 
//     // calculate E_r
//     arma::mat E_r = samples_r - dict_r * coeff_r;
// 
//     // svd
//     arma::mat u;
//     arma::vec s;
//     arma::mat v;
// //    svd_econ(u, s, v, E_r, modifyCoeff ? "both" : "left");
//     svd_econ(u, s, v, E_r);
// 
//     // update
//     new_dict.col(k) = u.col(0);
//     double sv = s(0);
//     if (singularValueHandling == SpstfmOptions::DictionaryNormalization::none ||
//         singularValueHandling == SpstfmOptions::DictionaryNormalization::fixed)
//     {
//         double n = 1;
//         if (singularValueHandling == SpstfmOptions::DictionaryNormalization::fixed)
//             n = k == 0 ? sv : arma::norm(new_dict.col(0));
// 
//         new_dict.col(k) *= sv / n;
//         sv = n;
//     }
// 
//     if (useOnlineMode) { // K-SVD says update coefficients, SPSTFM says coefficients are fixed for this step
//         auto crk = coeff.row(k);
//         for (unsigned int ci = 0; ci < nnz; ++ci)
//             crk(col_indices[ci]) = v(ci, 0) * sv;
//     }
// 
// }
// 
// arma::mat
// ksvd(arma::mat const& samples, arma::mat const& dict, arma::mat& coeff, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling) {
//     unsigned int natoms   = static_cast<unsigned int>(dict.n_cols);
//     assert(samples.n_rows == dict.n_rows && "samples and dict should have the same number of rows (dimension)");
//     assert(coeff.n_rows == natoms && "coeff should have as much rows as dict has columns (number of atoms)");
//     assert(coeff.n_cols == samples.n_cols && "coeff and samples must have the same number of cols (number of samples)");
// 
//     arma::mat new_dict = dict;
//     for (unsigned int k = 0; k < natoms; ++k)
//         ksvd_iteration(k, samples, dict, new_dict, coeff, useOnlineMode, singularValueHandling);
//     return new_dict;
// }
// 
// void
// doubleKSVDIteration(unsigned int k,
//                     arma::mat const& highSamples, arma::mat const& highDict, arma::mat& highDictNew,
//                     arma::mat const& lowSamples,  arma::mat const& lowDict,  arma::mat& lowDictNew,
//                     arma::mat& coeff, imagefusion::SpstfmOptions::TrainingResolution res, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling)
// {
//     unsigned int natoms   = static_cast<unsigned int>(highDict.n_cols);
// 
//     // get indices of coefficients
//     arma::uvec row_indices = arma::linspace<arma::uvec>(0, natoms - 1, natoms);
//     row_indices.shed_row(k);
// 
//     arma::uvec col_indices = arma::find(coeff.row(k));
//     unsigned int nnz = col_indices.n_elem;
//     if (nnz == 0)
//         return;
// 
//     // reduce and add Y alias samples
//     auto highSamples_r = highSamples.cols(col_indices);
//     auto lowSamples_r  = lowSamples.cols(col_indices);
//     auto const highDict_r = useOnlineMode ? highDictNew.cols(row_indices)
//                                           : highDict.cols(row_indices);
//     auto const lowDict_r  = useOnlineMode ? lowDictNew.cols(row_indices)
//                                           : lowDict.cols(row_indices);
//     auto coeff_r = coeff.submat(row_indices, col_indices);
// 
//     // calculate E_r
//     arma::mat highE_r = highSamples_r - highDict_r * coeff_r;
//     arma::mat lowE_r  = lowSamples_r  - lowDict_r  * coeff_r;
// 
//     // svd
//     arma::mat highU, lowU;
//     arma::vec highS, lowS;
//     arma::mat highV, lowV;
//     svd_econ(highU, highS, highV, highE_r);
//     svd_econ(lowU,  lowS,  lowV,  lowE_r);
// 
//     // update
//     double cor = arma::dot(highV.col(0), lowV.col(0));
//     double sign = cor < 0 ? -1 : 1;
//     double hs = highS(0);
//     double ls = lowS(0);
//     highDictNew.col(k) = highU.col(0) * sign;
//     lowDictNew.col(k)  = lowU.col(0);
// 
//     if (singularValueHandling == SpstfmOptions::DictionaryNormalization::none ||
//         singularValueHandling == SpstfmOptions::DictionaryNormalization::fixed)
//     {
//         double n = 1;
//         if (singularValueHandling == SpstfmOptions::DictionaryNormalization::fixed)
//             n = k == 0 ? hs : arma::norm(highDictNew.col(0));
// 
//         highDictNew.col(k) *= hs / n;
//         lowDictNew.col(k)  *= ls / n;
//         hs = ls = n;
//     }
//     else if (singularValueHandling == SpstfmOptions::DictionaryNormalization::pairwise) {
//         // if dictionary is scaled with singular values, it is divided by the max singular value to not disturb the tolerance setting of GPSR
//         highDictNew.col(k) *= hs / std::max(hs, ls);
//         lowDictNew.col(k)  *= ls / std::max(hs, ls);
//         hs = ls = std::max(hs, ls);
//     }
//     // else DictionaryNormalization::independent, which means scale coeffs, i.e. do not scale atoms and do not modify hs and ls, i.e. do nothing
// 
//     if (useOnlineMode) { // K-SVD says update coefficients, SPSTFM says coefficients are fixed for this step
//         arma::vec newCoeff;
//         if (res == imagefusion::SpstfmOptions::TrainingResolution::low)
//             newCoeff = ls * lowV.col(0);
//         else if (res == imagefusion::SpstfmOptions::TrainingResolution::high)
//             newCoeff = (sign * hs) * highV.col(0);
//         else // if (res == imagefusion::SpstfmOptions::TrainingResolution::average)
//             newCoeff = (0.5 * hs * sign) * highV.col(0) + (0.5 * ls) * lowV.col(0);
//         auto crk = coeff.row(k);
//         for (unsigned int ci = 0; ci < nnz; ++ci)
//             crk(col_indices[ci]) = newCoeff(ci);
//     }
// }
// 
// std::pair<arma::mat,arma::mat>
// doubleKSVD(arma::mat const& highSamples, arma::mat const& highDict,
//            arma::mat const& lowSamples,  arma::mat const& lowDict,
//            arma::mat& coeff, imagefusion::SpstfmOptions::TrainingResolution res, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling)
// {
//     unsigned int natoms   = static_cast<unsigned int>(highDict.n_cols);
//     assert(static_cast<unsigned int>(lowDict.n_cols) == natoms && "High and low-resolution dictionaries should have the same number of atoms.");
//     assert(static_cast<unsigned int>(coeff.n_rows) == natoms && "coeff should have as much rows as dict has columns");
//     assert(lowDict.n_rows == highDict.n_rows && "High and low-resolution dictionaries should have the same dimensions.");
//     assert(lowSamples.n_cols == highSamples.n_cols && "High and low-resolution training data should have the same number of samples.");
//     assert(lowSamples.n_rows == highSamples.n_rows && "High and low-resolution training data should have the same dimensions");
//     assert(lowSamples.n_rows == highDict.n_rows && "samples and dict should have the same number of rows (dimension)");
//     assert(coeff.n_cols == highSamples.n_cols && "coeff and samples must have the same number of cols");
//     assert(res != imagefusion::SpstfmOptions::TrainingResolution::concat && "doubleKSVD cannot be used with concatenated matrices");
// 
//     arma::mat highDictNew = highDict;
//     arma::mat lowDictNew  = lowDict;
//     for (unsigned int k = 0; k < natoms; ++k)
//         doubleKSVDIteration(k, highSamples, highDict, highDictNew, lowSamples, lowDict, lowDictNew, coeff, res, useOnlineMode, singularValueHandling);
//     return std::make_pair(highDictNew, lowDictNew);
// }
// 
// double objective_simple(arma::mat const& samples, arma::mat const& dict, arma::mat const& coeff, double tau) {
//     double l2 = arma::norm(samples - dict * coeff, "fro");
//     double l1 = arma::norm(arma::vectorise(coeff), 1);
//     return (l2 * l2 + tau * l1) / samples.n_elem;
// }
// 
// double objective_improved(arma::mat const& samples, arma::mat const& dict, arma::mat const& coeff, std::vector<double> const& taus) {
//     double l2 = arma::norm(samples - dict * coeff, "fro");
//     double l1 = 0;
//     for (unsigned int i = 0; i < taus.size(); ++i)
//         l1 += arma::norm(coeff.col(i), 1) * taus[i];
//     return (l2 * l2 + l1) / samples.n_elem;
// }
// 
// double testSetError(arma::mat const& highTestSamples, arma::mat const& lowTestSamples, arma::mat const& dictConcat, imagefusion::SpstfmOptions::GPSROptions const& gpsrOpts, double normFactorForHigh) {
//     unsigned int nTestSamples = lowTestSamples.n_cols;
//     assert(nTestSamples == highTestSamples.n_cols);
//     arma::mat dictHigh = highMatView(dictConcat);
//     arma::mat dictLow  = lowMatView(dictConcat);
// 
//     double l1_sum = 0;
//     for (unsigned int c = 0; c < nTestSamples; ++c) {
//         arma::vec rep = gpsr(lowTestSamples.col(c), dictLow, gpsrOpts);
//         double l1 = arma::norm(highTestSamples.col(c) - dictHigh * rep, 1);
//         l1_sum += l1;
//     }
//     return l1_sum * normFactorForHigh / highTestSamples.n_elem;
// }
// 
// std::vector<size_t> uniqueRandomVector(unsigned int count) {
//     std::random_device rd;
//     std::minstd_rand rng(rd());
// 
//     // create all numbers and shuffle
//     std::vector<size_t> numbers(count);
//     std::iota(numbers.begin(), numbers.end(), 0u);
//     std::shuffle(numbers.begin(), numbers.end(), rng);
//     return numbers;
// }
// 
// std::vector<size_t> mostVariance(imagefusion::ConstImage const& img, imagefusion::ConstImage const& mask, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     // get std deviation per patch
//     unsigned int dist = patchSize - patchOverlap;
//     unsigned int npx = (sampleArea.width  - patchOverlap) / dist;
//     unsigned int npy = (sampleArea.height - patchOverlap) / dist;
//     std::vector<double> stddevs(npx * npy);
//     unsigned int maskChannel = mask.channels() > channel ? channel : 0;
//     for (unsigned int pyi = 0; pyi < npy; ++pyi) {
//         for (unsigned int pxi = 0; pxi < npx; ++pxi) {
//             arma::mat imgPatch  = extractPatch(img,  pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
//             arma::mat maskPatch = extractPatch(mask, pxi, pyi, patchSize, patchOverlap, sampleArea, maskChannel);
// 
//             unsigned int pi = npx * pyi + pxi;
//             stddevs.at(pi) = arma::stddev(imgPatch.elem(arma::find(maskPatch != 0)));
// 
//         }
//     }
// 
//     // return sorted indices
//     return sort_indices(stddevs);
// }
// 
// std::vector<size_t> mostVariance(imagefusion::ConstImage const& img1, imagefusion::ConstImage const& img2, imagefusion::ConstImage const& mask, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     // get std deviation per patch
//     unsigned int dist = patchSize - patchOverlap;
//     unsigned int npx = (sampleArea.width  - patchOverlap) / dist;
//     unsigned int npy = (sampleArea.height - patchOverlap) / dist;
//     std::vector<double> stddevs(npx * npy);
//     unsigned int maskChannel = mask.channels() > channel ? channel : 0;
//     for (unsigned int pyi = 0; pyi < npy; ++pyi) {
//         for (unsigned int pxi = 0; pxi < npx; ++pxi) {
//             arma::mat imgPatch1 = extractPatch(img1, pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
//             arma::mat imgPatch2 = extractPatch(img2, pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
//             arma::mat maskPatch = extractPatch(mask, pxi, pyi, patchSize, patchOverlap, sampleArea, maskChannel);
// 
//             unsigned int pi = npx * pyi + pxi;
//             double var1 = arma::stddev(imgPatch1.elem(arma::find(maskPatch != 0)));
//             double var2 = arma::stddev(imgPatch2.elem(arma::find(maskPatch != 0)));
//             stddevs.at(pi) = var1 + var2;
//         }
//     }
// 
//     // return sorted indices
//     return sort_indices(stddevs);
// }
// 
// std::vector<size_t> duplicatesPatches(imagefusion::ConstImage const& img, imagefusion::ConstImage const& mask, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     unsigned int dist = patchSize - patchOverlap;
//     unsigned int npx = (sampleArea.width  - patchOverlap) / dist;
//     unsigned int npy = (sampleArea.height - patchOverlap) / dist;
//     unsigned int dim = patchSize * patchSize;
//     unsigned int range = 1000000;
//     unsigned int maskChannel = mask.channels() > channel ? channel : 0;
//     double min, max;
//     cv::minMaxIdx(img.cvMat(), &min, &max);
//     std::multimap<int, imagefusion::Point> sums;
//     for (unsigned int pyi = 0; pyi < npy; ++pyi) {
//         for (unsigned int pxi = 0; pxi < npx; ++pxi) {
//             arma::mat imgPatch  = extractPatch(img,  pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
//             auto      maskPatch = extractPatch(mask, pxi, pyi, patchSize, patchOverlap, sampleArea, maskChannel);
//             imgPatch.elem(arma::find(maskPatch == 0)).fill(0);
// 
//             double sum = (arma::accu(imgPatch) - dim * min) * (range / (max - min) / dim); // range: [0, 1e4]
//             sums.emplace(static_cast<int>(sum), imagefusion::Point(pxi, pyi));
//         }
//     }
// 
// //    unsigned int num_sums = sums.size();
// //    unsigned int num_comparisons = 0;
//     std::vector<size_t> duplicate_patches;
//     double tol = img.basetype() == imagefusion::Type::float32 ? 1e-1 : 1e-7; // for integer image types this means "no difference at all"
//     for (unsigned int i = 0; i < range; ++i) {
//         auto first = sums.lower_bound(i);
//         auto last  = sums.upper_bound(i+1); // two patches that are approx equal could be in neigbouring bins
//         while (first != last) {
//             auto idx_ref = first->second;
//             arma::mat imgPatch_ref  = extractPatch(img,  idx_ref.x, idx_ref.y, patchSize, patchOverlap, sampleArea, channel);
//             arma::mat maskPatch_ref = extractPatch(mask, idx_ref.x, idx_ref.y, patchSize, patchOverlap, sampleArea, maskChannel);
//             imgPatch_ref.elem(arma::find(maskPatch_ref == 0)).fill(0);
// 
//             auto it = first;
//             ++it;
//             while (it != last) {
// //                ++num_comparisons;
//                 auto idx_cur = it->second;
//                 arma::mat imgPatch_cur  = extractPatch(img,  idx_cur.x, idx_cur.y, patchSize, patchOverlap, sampleArea, channel);
//                 arma::mat maskPatch_cur = extractPatch(mask, idx_cur.x, idx_cur.y, patchSize, patchOverlap, sampleArea, maskChannel);
//                 imgPatch_cur.elem(arma::find(maskPatch_cur == 0)).fill(0);
// 
//                 bool equal = arma::approx_equal(imgPatch_ref, imgPatch_cur, "absdiff", tol);
//                 if (equal) {
//                     unsigned int pi = npx * idx_cur.y + idx_cur.x;
//                     duplicate_patches.push_back(pi);
//                     it = sums.erase(it);
//                 }
//                 else {
//                     ++it;
//                 }
//             }
//             ++first;
//         }
//     }
// //    Rcpp::Rcerr << "Calculated " << num_sums << " sums and compared " << num_comparisons << " patches." << std::endl;
//     return duplicate_patches;
// }
// 
// 
// std::vector<size_t> mostlyInvalidPatches(imagefusion::ConstImage const& mask, double tol, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     if (mask.empty())
//         return {};
//     assert(mask.basetype() == Type::uint8 && "mostlyInvalidPatches() requires a mask image as first argument");
// 
//     if (mask.channels() == 1)
//         channel = 0;
//     assert(channel < mask.channels());
// 
//     unsigned int dist = patchSize - patchOverlap;
//     unsigned int npx = (sampleArea.width  - patchOverlap) / dist;
//     unsigned int npy = (sampleArea.height - patchOverlap) / dist;
//     unsigned int dim = patchSize * patchSize;
//     std::vector<size_t> invalid;
//     for (unsigned int pyi = 0; pyi < npy; ++pyi) {
//         for (unsigned int pxi = 0; pxi < npx; ++pxi) {
//             auto patch = extractPatch(mask, pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
//             double nInvalidPixels = arma::accu(patch == 0);
//             if (nInvalidPixels / dim > tol) {
//                 unsigned int pi = npx * pyi + pxi;
//                 invalid.push_back(pi);
//             }
//         }
//     }
// 
//     return invalid;
// }
// 
// 
// std::vector<size_t> getOrderedPatchIndices(imagefusion::SpstfmOptions::SamplingStrategy s, imagefusion::ConstImage const& imgHigh, imagefusion::ConstImage const& imgLow, imagefusion::ConstImage const& mask, double maskInvalidTol, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     using namespace std::chrono;
//     std::vector<size_t> patch_indices;
//     switch (s) {
//     case imagefusion::SpstfmOptions::SamplingStrategy::random: {
//         unsigned int dist = patchSize - patchOverlap;
//         unsigned int npx = (sampleArea.width  - patchOverlap) / dist;
//         unsigned int npy = (sampleArea.height - patchOverlap) / dist;
//         patch_indices = uniqueRandomVector(npx * npy);
//         break;
//     }
//     case imagefusion::SpstfmOptions::SamplingStrategy::variance:
//         patch_indices = mostVariance(imgLow, imgHigh, mask, patchSize, patchOverlap, sampleArea, channel);
//         break;
//     }
// 
// //    steady_clock::time_point start = steady_clock::now();
//     std::vector<size_t> toBeRemoved = duplicatesPatches(imgLow, mask, patchSize, patchOverlap, sampleArea, channel);
//     std::vector<size_t> invalid     = mostlyInvalidPatches(mask, maskInvalidTol, patchSize, patchOverlap, sampleArea, channel);
// //    steady_clock::time_point end = steady_clock::now();
// //    duration<double> time = duration_cast<duration<double>>(end - start);
// //    Rcpp::Rcerr << "deduplication time: " << time.count() << std::endl;
// 
// 
//     size_t nP = patch_indices.size();
//     for (size_t pi : toBeRemoved)
//         patch_indices.erase(std::remove(patch_indices.begin(), patch_indices.end(), pi), patch_indices.end());
//     for (size_t pi : invalid)
//         patch_indices.erase(std::remove(patch_indices.begin(), patch_indices.end(), pi), patch_indices.end());
//     if (!toBeRemoved.empty() || !invalid.empty())
//         Rcpp::Rcerr << "Found " << toBeRemoved.size() << " duplicate patches (duplicate in low-res diff image) and "
//                   << invalid.size() << " patches with too many invalid pixels. Removed " << (nP - patch_indices.size()) << " in total." << std::endl;
// 
//     return patch_indices;
// }
// 
// 
// arma::mat samples(imagefusion::ConstImage const& highDiff, imagefusion::ConstImage const& lowDiff, imagefusion::ConstImage const& mask, std::vector<size_t> patch_indices, double meanForHigh, double meanForLow, double normFactorForHigh, double normFactorForLow, double fillHigh, double fillLow, unsigned int psize, unsigned int pover, imagefusion::Rectangle sampleArea, unsigned int channel) {
//     unsigned int nsamples = patch_indices.size();
//     unsigned int dim = psize * psize;
//     arma::mat samplesConcat(2 * dim, nsamples);
// 
//     arma::subview<double> samplesHigh = highMatView(samplesConcat);
//     initSingleSamples(highDiff, samplesHigh, mask, fillHigh, patch_indices, psize, pover, sampleArea, channel);
// 
//     arma::subview<double> samplesLow = lowMatView(samplesConcat);
//     initSingleSamples(lowDiff,  samplesLow,  mask, fillLow,  patch_indices, psize, pover, sampleArea, channel);
// 
//     samplesHigh -= meanForHigh;
//     samplesHigh *= 1 / normFactorForHigh;
//     samplesLow  -= meanForLow;
//     samplesLow  *= 1 / normFactorForLow;
// 
//     return samplesConcat;
// }
// 
// void drawDictionary(arma::mat const& dictConcat, std::string const& filename) {
//     arma::mat highDict = highMatView(dictConcat);
//     arma::mat lowDict  = lowMatView(dictConcat);
//     unsigned int psize = std::sqrt(highDict.n_rows);
//     unsigned int natoms = highDict.n_cols;
//     double min = dictConcat.min();
//     double max = dictConcat.max();
//     double range = std::max(std::abs(min), std::abs(max));
// 
//     imagefusion::Image out(1 + natoms * (psize + 1), 1 + 2 * (psize + 1), imagefusion::Type::uint8x1);
//     out.set(0);
// 
//     for (unsigned int k = 0; k < natoms; ++k) {
//         arma::mat h = highDict.col(k);
//         h.reshape(psize, psize);
//         h = (h.t() + range) * (255 / range / 2);
// 
//         arma::mat l = lowDict.col(k);
//         l.reshape(psize, psize);
//         l = (l.t() + range) * (255 / range / 2);
// 
//         int xh = 1 + k * (psize + 1);
//         int xl = xh;
//         int yh = 1;
//         int yl = 1 + psize + 1;
// 
//         for (unsigned int y = 0; y < psize; ++y) {
//             for (unsigned int x = 0; x < psize; ++x) {
//                 out.at<uint8_t>(xh + x, yh + y) = cv::saturate_cast<uint8_t>(h(y, x));
//                 out.at<uint8_t>(xl + x, yl + y) = cv::saturate_cast<uint8_t>(l(y, x));
//             }
//         }
//     }
//     out.write(filename);
// }
// 
// void drawWeights(arma::mat const& weights, std::string const& filename) {
//     imagefusion::Image out(weights.n_cols, weights.n_rows, imagefusion::Type::uint8x1);
//     for (unsigned int x = 0; x < weights.n_cols; ++x)
//         for (unsigned int y = 0; y < weights.n_rows; ++y)
//             out.at<uint8_t>(x, y) = cv::saturate_cast<uint8_t>(weights(y, x) * 255);
//     out.write(filename);
// }
// 
// 
// } /* namespace spstfm_impl_detail */
// } /* namespace imagefusion */

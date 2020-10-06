#pragma once

#include "datafusor.h"
#include "estarfm_options.h"

#include <cmath>
#include <iostream>

#include <opencv2/opencv.hpp>

#include <boost/math/distributions/fisher_f.hpp>

namespace imagefusion {

/**
 * @brief Implementation details of ESTARFM -- not to be used by library users
 *
 * This namespace holds three functors. They are all called from StarfmFusor::predict():
 *  * ComputeLocalWeights is called via EstarfmFusor::computeLocalWeights() before the
 *    moving window loops.
 *  * SumAndTolHelper is called via its constructor before the moving window
 *    loops as well.
 *  * PredictPixel is called via a CallBaseTypeFunctor::run() to predict
 *    values of all channels for the central pixel in the moving window loops.
 */
namespace estarfm_impl_detail {

/**
 * @brief This functor predicts the values for all channels of the central pixel of a window
 *
 * @param opt holds the ESTARFM options. It is only used to get the data range limits via
 * @ref EstarfmOptions::isDataRangeSet() "isDataRangeSet()",
 * @ref EstarfmOptions::getDataRangeMin() "getDataRangeMin()" and
 * @ref EstarfmOptions::getDataRangeMax() "getDataRangeMax()".
 *
 * @param x_center is the center x-coordinate relative to the window origin.
 *
 * @param y_center is the center y-coordinate relative to the window origin.

 * @param h1_win is the current window of the high resolution image at date 1.
 * @param h3_win is the current window of the high resolution image at date 3.
 * @param l1_win is the current window of the low resolution image at date 1.
 * @param l2_win is the current window of the low resolution image at date 2.
 * @param l3_win is the current window of the low resolution image at date 3.
 *
 * @param lw_win is the current window of the local weights, see ComputeLocalWeights. It is denoted
 * by \f$ L \f$ below.
 *
 * @param dw_win contains the current window of the distance weights defined as
 * \f$ D(x,y) = 1 + \frac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{\tfrac s 2} \f$.
 * This is generally always the same but on image boundaries it is cropped. See
 * computeDistanceWeights().
 *
 * @param sm_win is the current window of the (sample) mask. This may have multiple channels or be
 * empty (for no mask).
 *
 * @param tol1 contains the tolerance values for the current window for all channels for the input
 * pair at date 1. This tolerance is denoted by \f$ \varepsilon_1 \f$ below for the considered
 * channel.
 *
 * @param tol3 contains the tolerance values for the current window for all channels for the input
 * pair at date 3. This tolerance is denoted by \f$ \varepsilon_3 \f$ below for the considered
 * channel.
 *
 * @param sumL1 contains the sums of the valid values of the low resolution image at date 1 of the
 * current window for all channels.
 *
 * @param sumL2 contains the sums of the valid values of the low resolution image at date 2 of the
 * current window for all channels.
 *
 * @param sumL3 contains the sums of the valid values of the low resolution image at date 3 of the
 * current window for all channels.
 *
 * @param out_pixel is the (shared copy of the) central pixel of the output image.
 *
 * So basically it receives all relevant variables for the current window and it loops over all
 * channels doing the same procedure for each channel. This procedure is described in the
 * following.
 *
 * The algorithm relies on using only pixels of the window, which are similar to the central pixels
 * in all channels. This is done for both pairs. Let the central pixel be at \f$ (x_c, y_c) \f$ and
 * the pixels in question at \f$ (x, y) \f$. The pixels at location \f$ (x, y) \f$ is considered
 * similar if they satisfy
 *  * \f$ |h_1(x, y) - h_1(x_c, y_c)| < \varepsilon_1 \f$ and
 *  * \f$ |h_3(x, y) - h_3(x_c, y_c)| < \varepsilon_3 \f$
 * for all of their channels. Then, if these conditions hold, a weight is calculated from the local
 * weight \f$ L \f$ and the distance weight \f$ D \f$ as \f$ W = \frac{1}{L \, D} \f$.
 *
 * Then the weights of all similar pixels in the current window are summed up. Assuming all other
 * weights are set to 0, this can be written as \f$ s_w = \sum_{x, y} W(x, y) \f$. The low
 * resolution changes from date 1 to 2 and from date 3 to 2 are also weighted and summed up to
 * \f$ s_{l_1} \f$ and \f$ s_{l_3} \f$, respectively:
 * \f[ s_{l_k} = \sum_{x, y} (l_2(x, y) - l_k(x, y)) \, W(x, y)  \quad\text{for } k = 1, 3. \f]
 * For a special case also the high resolution pixels are weighted and summed up:
 * \f[ s_{h_k} = \sum_{x, y} h_k(x, y) \, W(x, y)  \quad\text{for } k = 1, 3. \f]
 *
 * Next, for all low resolution candidates \f$ L_c \f$ and high resolution candidates \f$ H_c \f$
 * one linear regression is made with the model \f$ H_c = a + b \, L_c \f$. The coefficient b will
 * be used as additional common weight for \f$ s_{l_k} \f$.
 *
 * Then, one prediction from each side is made with the weighted average:
 * \f[ p_1 := h_1(x_c, y_c) + b \, \frac{s_{l_1}}{s_w}
 *     \quad\text{and}\quad
 *     p_3 := h_3(x_c, y_c) + b \, \frac{s_{l_3}}{s_w} \f]
 * However, if there were less than 6 similar pixels in the window, just the high resolution center
 * pixels will be used as predictions.
 *
 * The final prediction will weight both predictions with the total change of all valid pixels in
 * the low resolution image windows. Therefore
 * \f[ T_1 := \frac{1}{|\sum_{x, y} l_1(x, y) - l_2(x, y)|}
 *     \quad\text{and}\quad
 *     T_3 := \frac{1}{|\sum_{x, y} l_3(x, y) - l_2(x, y)|} \f]
 * is used in a normed way to weight the predictions:
 * \f[ p_2 := \frac{T_1}{T_1 + T_3} \, p_1 + \frac{T_3}{T_1 + T_3} \, p_3 \f]
 *
 * Now, if \f$ p_2 \f$ is out of the data range it is replaced by the high resolution weighted
 * average:
 * \f[ p_2 := \frac{T_1}{T_1 + T_3} \, \frac{s_{h_1}}{s_w}
 *          + \frac{T_3}{T_1 + T_3} \, \frac{s_{h_3}}{s_w} \f]
 *
 * Note, the regression coefficient is different for every channel, but the same for the whole
 * window (currently). The local weight (correlation coefficient) is the same for all channels, but
 * different for each location in the window.
 */
struct PredictPixel {
    EstarfmOptions const& opt;
    unsigned int x_center;
    unsigned int y_center;
    ConstImage const& h1_win;
    ConstImage const& h3_win;
    ConstImage const& l1_win;
    ConstImage const& l2_win;
    ConstImage const& l3_win;
    ConstImage const& lw_win;
    ConstImage const& dw_win;
    ConstImage const& sm_win;
    std::vector<double> const& tol1;
    std::vector<double> const& tol3;
    std::vector<double> const& sumL1;
    std::vector<double> const& sumL2;
    std::vector<double> const& sumL3;
    Image& out_pixel;

    template<Type basetype>
    void operator()() const;
};


/**
 * @brief This functor precomputes all local weights
 *
 * @param h1 is the high resolution image at date 1.
 * @param h3 is the high resolution image at date 3.
 * @param l1 is the low resolution image at date 1.
 * @param l3 is the low resolution image at date 3.
 * @param m is either empty, a single-channel mask or a multi-channel mask.
 *
 * This calculates the local weights. These are pointwise operations and used in PredictPixel. This
 * precalculation saves PredictPixel to calculate the same values over and over again. This functor
 * is called in StarfmFusor::predict() before the moving window with a call to
 * CallBaseTypeFunctor::run().
 *
 * The local weights correspond to the correlation coefficient of high and low resolution.
 * Considering the pixels at one single location, let \f$ H_1 \f$ be the vector of all channel
 * values of the high resolution image at date 1 and similarly let \f$ H_3, L_1, L3 \f$ be the
 * vectors of their respective resolution and date. Then let
 * \f$ H := \begin{pmatrix} H_1 \\ H_3 \end{pmatrix} \f$ and
 *
 * \f$ L := \begin{pmatrix} L_1 \\ L_3 \end{pmatrix} \f$. So to clarify this, H would have six
 * elements for three-channel input images. Now the local weight is just the correlation
 * coefficient of H and L:
 * \f[ \rho_{H,L} = \frac{\mathrm{cov}(H, L)}{\sigma_H \, \sigma_L}
 *                = \frac{\sum_i (h_i - \bar h) \, (l_i - \bar l)}
 *                       {\sqrt{\sum_i (h_i - \bar h)^2} \, \sqrt{\sum_i (l_i - \bar l)^2}}. \f]
 * This is calculated once for every location.
 *
 * @return double image with the local weights. It has the same size and number of channels as the
 * input images.
 */
struct ComputeLocalWeights {
    ConstImage const& h1;
    ConstImage const& h3;
    ConstImage const& l1;
    ConstImage const& l3;
    ConstImage const& m;

    template<Type basetype>
    Image operator()() const;
};


/**
 * @brief The SumAndTolHelper functor precomputes the tolerances and sums of each window
 * @param opt ESTARFM options. Used to get the window size via
 * @ref EstarfmOptions::getWinSize() "getWinSize()" and the number of classes
 * @ref EstarfmOptions::getNumberClasses() "getNumberClasses()". Last but not least the tolerance
 * is only initialized and computed, iff @ref EstarfmOptions::getUseLocalTol() "getUseLocalTol()"
 * is true.
 *
 * @param h1 is the high resolution image at date 1.
 * @param h3 is the high resolution image at date 3.
 * @param l1 is the low resolution image at date 1.
 * @param l2 is the low resolution image at date 2.
 * @param l3 is the low resolution image at date 3.
 * @param m is either empty, a single-channel mask or a multi-channel mask.
 *
 * @param predArea is the prediction area. This is used to loop through all windows with central
 * pixels exactly like EstarfmFusor::predict() does it before calling PredictPixel.
 *
 * This fusor is used just by calling the constructor. This internally uses
 * CallBaseTypeFunctor::run() on itself. The constructed object will hold all output objects.
 *
 * This functor calculates the window sums of the low resolution images and maybe the window
 * tolerances, which require the window sums of the high resolution images. The sums of moving
 * windows can be calculated very efficiently. Its basic idea is to start with an initial sum and
 * add the new values and subtract the old. In a 1D moving sum, this would be like follows:
 *
 *     | 1 + 6 + 3 + 2 | 8   4   would be 12. Moving one to the right gives
 *       -               +
 *       1 | 6 + 3 + 2 + 8 | 4   would be 12 - 1 + 8 = 19. And again
 *           -               +
 *       1   6 | 3 + 2 + 8 + 4 | would be 19 - 6 + 4 = 17.
 *
 * This is what this helper does, but in 2D, with sums of h1, h3, l1, l2 and l3, sum of squares of
 * h1 and h3 and valid pixel counts of h1 and h3. The tolerances are calculated for the high
 * resolution images, like
 * \f[ \sigma_1 = \sqrt{\frac{\sum_i^N h_{1,i}^2}{N} - \left( \frac{\sum_i^N h_{1,i}}{N} \right)^2} \f]
 * and
 * \f[ \emph{tol}_1 := \frac{2 \, \sigma_1}{C}, \f]
 * where \f$ C \f$ is the number of classes.
 *
 * This allows for a runtime complexity of \f$ c \, W \, H \f$ instead of \f$ d \, W \, H \, S^2
 * \f$ with a naive approach, where \f$ c, d\f$ are some constants, \f$ W \f$ and \f$ H \f$ are the
 * width and height of the image (or actually the sample area) and \f$ S \f$ is the window size (by
 * default 51).
 *
 * @return nothing as return value, but in Image variables of the constructed object:
 *  * tol1 is the tolerance for input image pair at date 1
 *  * tol3 is the tolerance for input image pair at date 3
 *  * sumL1 is the window sum for low resolution image at date 1
 *  * sumL2 is the window sum for low resolution image at date 2
 *  * sumL3 is the window sum for low resolution image at date 3
 * All images have the size of the prediction area (`predArea`), the same number of channels as the
 * source images and type double. At location (0, 0) are the values for the first window, which
 * have the central pixel at (predArea.x, predArea.y).
 *
 * Note, again, if @ref EstarfmOptions::getUseLocalTol() "getUseLocalTol()" is false, tol1 and tol3
 * will be empty images.
 */
struct SumAndTolHelper {
    /**
     * @brief The Stats struct helps the SumAndTolHelper with all required values
     */
    struct Stats {
        double sum_h1    = 0;
        double sum_h3    = 0;
        double sqrsum_h1 = 0;
        double sqrsum_h3 = 0;
        size_t cnt_h1    = 0;
        size_t cnt_h3    = 0;
        double sum_l1    = 0;
        double sum_l2    = 0;
        double sum_l3    = 0;

        void operator+=(Stats const& s) {
            sum_h1    += s.sum_h1;
            sum_h3    += s.sum_h3;
            sqrsum_h1 += s.sqrsum_h1;
            sqrsum_h3 += s.sqrsum_h3;
            cnt_h1    += s.cnt_h1;
            cnt_h3    += s.cnt_h3;
            sum_l1    += s.sum_l1;
            sum_l2    += s.sum_l2;
            sum_l3    += s.sum_l3;
        }

        void operator-=(Stats const& s) {
            sum_h1    -= s.sum_h1;
            sum_h3    -= s.sum_h3;
            sqrsum_h1 -= s.sqrsum_h1;
            sqrsum_h3 -= s.sqrsum_h3;
            cnt_h1    -= s.cnt_h1;
            cnt_h3    -= s.cnt_h3;
            sum_l1    -= s.sum_l1;
            sum_l2    -= s.sum_l2;
            sum_l3    -= s.sum_l3;
        }
    };

    // input arguments
    EstarfmOptions const& opt;
    ConstImage const& h1;
    ConstImage const& h3;
    ConstImage const& l1;
    ConstImage const& l2;
    ConstImage const& l3;
    ConstImage const& m;
    Rectangle const& predArea;

    // output objects
    Image tol1;
    Image tol3;
    Image sumL1;
    Image sumL2;
    Image sumL3;

    SumAndTolHelper(EstarfmOptions const& opt, ConstImage const& h1, ConstImage const& h3, ConstImage const& l1, ConstImage const& l2, ConstImage const& l3, ConstImage const& m, Rectangle const& predArea)
        : opt(opt), h1(h1), h3(h3), l1(l1), l2(l2), l3(l3), m(m), predArea(predArea),
          tol1{},
          tol3{},
          sumL1{predArea.size(), getFullType(Type::float64, l2.channels())},
          sumL2{sumL1.size(), sumL1.type()},
          sumL3{sumL1.size(), sumL1.type()}
    {
        if (opt.getUseLocalTol()) {
            tol1 = Image{sumL1.size(), sumL1.type()};
            tol3 = Image{sumL1.size(), sumL1.type()};
        }

        CallBaseTypeFunctor::run(*this, l2.type());
    }

    /**
     * @brief Compute a Stats object from the cropped input arguments and channel
     *
     * For the very first window this will be called for the whole window. For all successive calls
     * this will be called with the new stripe, where the window moved to and the old stripe, where
     * the window came from.
     */
    template<Type basetype>
    Stats collectStats(ConstImage h1_win, ConstImage h3_win, ConstImage l1_win, ConstImage l2_win, ConstImage l3_win, ConstImage m_win, unsigned int c) const;

    template<Type basetype>
    void operator()();
};

/**
 * @brief Compute the linear regression for the input data
 *
 * @tparam val_t is the value type of the input data
 *
 * @param x_vec is the input data for X
 * @param y_vec is the input data for Y
 * @param smooth is a setting to blend linear with the quality the regression coefficient into 1.
 *
 * This assumes a model \f$ Y = 1 \, a + X \, b + \varepsilon \f$, where a and b are the parameters
 * to regress, but only b will be returned and only if it is between 0 and 5 and the regression
 * quality is good enough. Otherwise a 1 will be returned.
 *
 * The regression works as follows. To find the best coefficients, we neglect \f$ \varepsilon \f$.
 * We also vectorize the formulation by putting the 1-vector together with X into the matrix
 * \f$ Z := \begin{pmatrix}1 & X\end{pmatrix} \f$. Then we can reformulate the model:
 * \f[ Y = Z \, \begin{pmatrix} a\\ b \end{pmatrix}
 *     \quad\Leftrightarrow\quad
 *     Z^\top \, Y = Z^\top \, Z \, \begin{pmatrix} a\\ b \end{pmatrix}
 * \f]
 * This can be solved for the coefficients:
 * \f{equation*}{
 *     \begin{pmatrix} a\\ b \end{pmatrix}
 *       = (Z^\top \, Z)^{-1} \, Z^\top \, Y
 *       = \begin{pmatrix}1 \cdot 1 & X \cdot 1 \\
 *                        X \cdot 1 & X \cdot X \end{pmatrix}^{-1} \,
 *         \begin{pmatrix}Y \cdot 1 \\ X \cdot Y \end{pmatrix}\\
 *       = \begin{pmatrix} n          & \sum_i x_i\\
 *                         \sum_i x_i & \sum_i x_i^2 \end{pmatrix}^{-1} \,
 *         \begin{pmatrix} \sum_i y_i \\ \sum_i x_i \, y_i \end{pmatrix}
 *       = \frac{1}{n \sum_i x_i^2 - \left( \sum_i x_i \right)^2}
 *         \begin{pmatrix} \sum_i x_i^2 & -\sum_i x_i \\
 *                        -\sum_i x_i   &  n \end{pmatrix} \,
 *         \begin{pmatrix} \sum_i y_i \\ \sum_i x_i \, y_i \end{pmatrix}
 * \f}
 *
 * Then if b < 0 or b > 5 just 1 is returned instead of the coefficient b. Otherwise we go on to
 * check for the quality. We do this by:
 * \f[ r = Y - Z \, \begin{pmatrix} a\\ b \end{pmatrix} \f]
 * \f[ R_2 = 1 - \frac{r \cdot r}{n \, \mathrm{var}(Y)} \f]
 * \f[ \mathcal F = \frac{n - p}{p - 1} \, \frac{R_2}{1 - R_2} \f]
 * \f[ Q = F(\mathcal F; 1, 2 \, n - 2) \f]
 * Hereby n is the number of samples, p = 2 (number of regression coefficients to calculate) and
 * \f$ Q \in [0, 1) \f$ is the quality.
 *
 * In the case that smooth is false and if Q < 95%, then again 1 is returned instead of the
 * coefficient b. Otherwise b is finally returned. In the case that smooth is true b * Q + (1 - Q)
 * is returned.
 */
template<class val_t>
inline double regress(std::vector<val_t> const& x_vec, std::vector<val_t> const& y_vec, bool smooth = false) {
    assert(x_vec.size() == y_vec.size() && "Regress function: vectors have a different number of elements.");
    unsigned int n = x_vec.size();
    cv::Mat x_cv(x_vec);
    cv::Mat y_cv(y_vec);

    // regress
    cv::Scalar mean_y, stddev_y;
    cv::meanStdDev(y_cv, mean_y, stddev_y);
    double y_dot_1 = mean_y[0] * n;
    double x_dot_1 = cv::sum(x_cv)[0];
    double x_dot_x = x_cv.dot(x_cv);
    double x_dot_y = x_cv.dot(y_cv);

    double det = n * x_dot_x - x_dot_1 * x_dot_1;
    if (std::abs(det) < 1e-14)
        return 1;
    double a = (x_dot_x * y_dot_1 - x_dot_1 * x_dot_y) / det;
    double b = (n * x_dot_y - x_dot_1 * y_dot_1) / det;

    // exclude strange cases
    if (b < 0 || 5 < b || n - 2 <= 0)
        return 1;

    // get fvalue
    double sqrerr = 0;
    for (unsigned int i = 0; i < n; ++i) {
        double x = x_vec.at(i);
        double y = y_vec.at(i);
        double err = y - x * b - a;
        sqrerr += err * err;
    }
    double r2 = 1 - sqrerr / (stddev_y[0] * stddev_y[0] * n);
    double fvalue = (n - 2) / (1 / r2 - 1);

    // check regression quality
    boost::math::fisher_f f_dist(1, n - 2); // n is here 2 * number of candidates
    if (fvalue < 0 || !std::isfinite(fvalue))
        return b;

    double fisher = cdf(f_dist, fvalue);
    if (!smooth) {
        if (fisher < 0.95)
            return 1;
        return b;
    }
    return b * fisher + (1 - fisher);
}

/**
 * @brief Get the Pearson correlation coefficient
 *
 * @tparam val_t is the type of the input data
 * @param x_vec is one signal
 * @param y_vec is the other signal
 *
 * @returns the linear correlation \f$ \rho_{X,Y} \in [-1, 1] \f$ of X and Y, as
 * \f[ \rho_{X,Y} = \frac{\mathrm{cov}(X, Y)}{\sigma_X \, \sigma_Y}
 *                = \frac{\sum_i (x_i - \bar x) \, (y_i - \bar y)}
 *                       {\sqrt{\sum_i (x_i - \bar x)^2} \, \sqrt{\sum_i (y_i - \bar y)^2}}. \f]
 */
template<class val_t>
inline double correlate(std::vector<val_t> const& x_vec, std::vector<val_t> const& y_vec) {
    unsigned int n = x_vec.size();
    cv::Mat x_cv(x_vec);
    cv::Mat y_cv(y_vec);
    cv::Scalar mean_x, mean_y, stddev_x, stddev_y;
    cv::meanStdDev(x_cv, mean_x, stddev_x);
    cv::meanStdDev(y_cv, mean_y, stddev_y);

    double cov_xy = 0;
    for (unsigned int i = 0; i < n; ++i) {
        double x = x_vec.at(i);
        double y = y_vec.at(i);
        cov_xy += (x - mean_x[0]) * (y - mean_y[0]);
    }

    return cov_xy / (stddev_x[0] * stddev_y[0] * n);
}


} /* namespace estarfm_impl_detail */

/**
 * @brief The EstarfmFusor class is the implementation of the ESTARFM algorithm
 *
 * ESTARFM stands for *enhanced spatial and temporal adaptive reflectance fusion model* so it
 * claims to be an enhanced STARFM. It can yield better results in some situations.
 *
 * For ESTARFM five images required. For simplicity the dates are numbered 1, 2, 3 and then the
 * image to predict is the high resolution image at date 2, see the following table:
 * <table>
 * <caption id="estarfm_image_structure">ESTARFM image structure</caption>
 *   <tr><td><span style="display:inline-block; width: 5em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">1</td>
 *       <td align="center" valign="top">2</td>
 *       <td align="center" valign="top">3</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center"><table><tr><th style="width: 5em;">High 1</th></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High 2</b></td></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">High 3</th></tr></table></td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low 1</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low 2</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low 3</th></tr></table></td>
 *   </tr>
 * </table>
 *
 * ESTARFM basically iterates through all pixels in an image and looks in a region around (window)
 * for similar high resolution pixels. These are only considered as similar, when they are
 * approximately equal to the center pixel in all layers. From the similar pixels weights \f$ W \f$
 * are calculated by their distance weight \f$ D \f$ (see computeDistanceWeights()) and local
 * weight \f$ L \f$, which corresponds to a correlation coefficient based on all channels and both
 * pairs between both resolutions. The weight is calculated by \f$ W = \frac{1}{L \, D} \f$. The
 * weights of one window are summed up as well as the corresponding changes from date 1 to 2 and
 * from date 3 to 2 multiplied with the weight and a regression coefficient
 * \f$ R \f$: \f$ (l_2 - l_k) \, W \, R \f$ for k = 1, 3. Denote the sum of weights by \f$ s_w \f$
 * and the sums of weighted changes by \f$ s_1 \f$ and \f$ s_3 \f$.
 *
 * Now the weighted averages predict the changes from date 1 to 2 and from date 3 to 2 and is added
 * to the high resolution center pixels:
 * \f[ p_1 := h_{1,\text{center}} + \frac{s_1}{s_w}
 *     \quad\text{and}\quad
 *     p_3 := h_{3,\text{center}} + \frac{s_3}{s_w} \f]
 * These two predictions for \f$ h_2 \f$ are weighted with the mean absolute difference of the low
 * resolution images of the whole window, denoted by \f$ T_1 \f$ and \f$ T_3 \f$, respectively, to
 * get the final prediction:
 * \f[ p_2 := \frac{T_1}{T_1 + T_3} \, p_1 + \frac{T_3}{T_1 + T_3} \, p_3 \f]
 *
 * There are a lot of special cases and details, but this is the basic principle. There is a
 * difference to the algorithm as described in the paper though. The regression for \f$ R \f$ is
 * now done with all candidates of one window. This complies to the reference implementation, but
 * not to the paper, since there the regression is done only for the candidates that belong to one
 * single coarse pixel. However, the coarse grid is not known at prediction and not necessarily
 * trivial to find out (e. g. in case of higher order interpolation).
 *
 * For parallelization @ref Parallelizer "Parallelizer<EstarfmFusor>" can be used with a
 * @ref ParallelizerOptions "ParallelizerOptions<EstarfmOptions>". This is analogue to the
 * @ref parallelusage "example" on the main page.
 */
class EstarfmFusor : public DataFusor {
public:
    /**
     * @brief This declares which option type to use
     *
     * This is done for Parallelizer to allow to default the AlgOpt template argument. So it is
     * enough to specify the fusor type, like
     * @code
     * Parallelizer<EstarfmFusor>
     * @endcode
     * instead of
     * @code
     * Parallelizer<EstarfmFusor, EstarfmOptions>
     * @endcode
     */
    using options_type = EstarfmOptions;


    /**
     * @brief Process the ESTARFM options
     *
     * @param o is an options object of type EstarfmOptions and replaces the current options
     * object.
     *
     * This function basically saves the options and checks it for
     * contradictions.
     *
     * @see getOptions()
     */
    void processOptions(Options const& o) override;

    // inherited documentation
    options_type const& getOptions() const override {
        return opt;
    }

    /**
     * @brief Predict an image at the specified date
     *
     * @param date2 is the prediction date and it is used to get the right Image from `#imgs`. See
     * also \ref estarfm_image_structure "ESTARFM image structure".
     *
     * @param validMask is either empty or a mask in the size of the source images. It can be
     * single-channel or multi-channel. Locations with zero values are not used at all and the
     * result of the @ref outputImage() is undefined at these locations. If the argument is an
     * empty image, all locations will be considered as valid.
     *
     * @param predMask is either empty or a single-channel mask in the size of the source images.
     * It specifies the locations that should be predicted (255) and the locations that should not
     * be predicted (0). However, a prediction can only be done for valid locations, as specified
     * by the `validMask`. The result of the @ref outputImage() is undefined at locations where no
     * prediction occurs.
     *
     * @throws logic_error if source images have not been set.
     * @throws not_found_error if not all required images are available.
     * @throws image_type_error if the types (basetypes or channels) of images or masks mismatch
     * @throws size_error if the sizes of images or masks mismatch
     */
    void predict(int date2, ConstImage const& validMask = {}, ConstImage const& predMask = {}) override;

protected:
    /// EstarfmOptions to use for the next prediction
    options_type opt;

    /**
     * @brief Get area where pixels are read
     * @param fullImgSize size of the source image. This is used as bounds.
     * @param predArea is the prediction area used. It must be valid (not just all-zero).
     *
     * The sample area is the area from which pixels are read. That is the prediction area with the
     * window size around, but limited by the image bounds.
     *
     * @return sample area rectangle.
     */
    Rectangle findSampleArea(Size const& fullImgSize, Rectangle const& predArea) const;

    /**
     * @brief Get weights map for the distance to the center pixel
     *
     * This makes a map for the relative distance in the size of the window. So it precomputes all
     * results of the formula \f$ 1 + \frac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{A} \f$, where
     * \f$ (x_c, y_c) \f$ is the center pixel of the moving window and \f$ A \f$ is a constant and
     * usually half of the window size.
     *
     * @return distance weights as single-channel image of type double.
     */
    Image computeDistanceWeights() const;

    /**
     * @brief Check the input images size, type, etc.
     * @param validMask will be checked for size, type and channels
     * @param predMask will be checked for size, type and channels
     * @param date2 is the prediction date to get the corresponding image
     *
     * @throws logic_error if source images have not been set.
     * @throws not_found_error if not all required images are available.
     * @throws image_type_error if the types (basetypes or channels) of images or masks mismatch
     * @throws size_error if the sizes of images or masks mismatch
     */
    void checkInputImages(ConstImage const& validMask, ConstImage const& predMask, int date2) const;

    /// This just hides the CallBaseTypeFunctor to ComputeLocalWeights
    Image computeLocalWeights(ConstImage const& h1, ConstImage const& h3, ConstImage const& l1, ConstImage const& l3, ConstImage const& mask) const {
        return CallBaseTypeFunctor::run(estarfm_impl_detail::ComputeLocalWeights{
                   h1, h3, l1, l3, mask},
                   h1.type());
    }
};

} /* namespace imagefusion */

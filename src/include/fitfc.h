#pragma once

#include "DataFusor.h"
#include "fitfc_options.h"
#include <cmath>
#include <iostream>
#include <opencv2/opencv.hpp>

#ifdef WITH_OMP
    #include "Parallelizer.h"
#endif /* WITH_OMP */


namespace imagefusion {

/**
 * @brief Implementation details of Fit-FC -- not to be used by library users
 *
 * This namespace contains some helping functors.
 */
namespace fitfc_impl_detail {

/**
 * @brief The RegressionMapper computes the regression model and the residual
 * @param opt are the Fit-FC options. Used to get the window size via
 * @ref FitFCOptions::getWinSize() "getWinSize()" and the number of threads
 * @ref FitFCOptions::getNumberThreads() const "getNumberThreads()" to use.
 *
 * @param h1 is the high resolution image at date 1. This will be used for x.
 * @param l1 is the low resolution image at date 1. This will be used for x.
 * @param l2 is the low resolution image at date 2. This will be used for y.
 * @param m is either empty or a single-channel mask. The masked out pixels are not used for
 * regression and not for mapping. Their output is undefined (unchanged).
 *
 * This fusor is used by calling FitFCFusor::regress(), which uses CallBaseTypeFunctor::run(). All
 * the input images are shared copies in size of the sample area. The result will be influenced by
 * the border with a width of the half window size.
 *
 * This functor iterates through all pixels. For each pixel it uses a window around that central
 * pixel. It regresses a linear model from `l1` to `l2` using all pixels of the window (no
 * filtering here). To be more specific, `l1` is considered as x and `l2` as y to regress a and b
 * in the model \f$ y = a\,x + b \f$ by using least squares method. Then this linear model is used
 * to map the central pixel of `h1` to `h2` (prediction), i. e. \f$ h_2 = a\,h_1 + b \f$. Also the
 * residual of the central pixel from `l1` to `l2` is saved, i. e. \f$ R = l_2 - (a\,l_1 + b) \f$.
 *
 * The regression requires some sums of the low resolution images over all pixels in a window. The
 * sums of moving windows can be calculated very efficiently. Its basic idea is to start with an
 * initial sum and add the new values and subtract the old. In a 1D moving sum, this would be like
 * follows:
 *
 *     | 1 + 6 + 3 + 2 | 8   4   would be 12. Moving one to the right
 *       -               +
 *       1 | 6 + 3 + 2 + 8 | 4   would be 12 - 1 + 8 = 19. And again
 *           -               +
 *       1   6 | 3 + 2 + 8 + 4 | would be 19 - 6 + 4 = 17.
 *
 * This is how this helper computes the sums, but in 2D, for the sum of l1, l2, l1*l1 and l1*l2 as
 * well as the pixel count (can vary at borders and by masks). This allows for a runtime complexity
 * of \f$ c \, W \, H \f$ instead of \f$ d \, W \, H \, S^2 \f$ with a naive approach, where \f$ c,
 * d\f$ are some constants, \f$ W \f$ and \f$ H \f$ are the width and height of the image (or
 * actually the sample area) and \f$ S \f$ is the window size (by default 51).
 *
 * Note: This whole procedure is done for each channel separately. So the most outer loop iterates
 * over the channels. This loop is also parallelized, since due to the independency of the
 * channels, there are no problems and no overhead.
 *
 * @return two images. First is the predicted image, which is in the paper denoted by
 * \f$ \hat F_{\mathrm{RM}} \f$. It has the same data type, size and number of channels as `h1`.
 * The second is the coarse residual, which has the same size and number of channels, but is of
 * type double (Type::float64).
 */
struct RegressionMapper {
    struct Stats {
        double x_dot_1 = 0; ///< \f$ X \cdot 1 = \sum_i x_i \f$ is the sum of all valid pixel values of `l1` in the current window
        double y_dot_1 = 0; ///< \f$ Y \cdot 1 = \sum_i y_i \f$ is the sum of all valid pixel values of `l2` in the current window
        double x_dot_x = 0; ///< \f$ X \cdot X = \sum_i x_i^2 \f$ is the sum of squares of all valid pixel values of `l1` in the current window
        double x_dot_y = 0; ///< \f$ X \cdot Y = \sum_i x_i \, y_i \f$ is the sum of all valid pixel values of `l1` · `l2` in the current window
        size_t n       = 0; ///< \f$ 1 \cdot 1 = n \f$ is the count of valid pixel values in the current window

        void operator+=(Stats const& s) {
            x_dot_1 += s.x_dot_1;
            y_dot_1 += s.y_dot_1;
            x_dot_x += s.x_dot_x;
            x_dot_y += s.x_dot_y;
            n       += s.n;
        }

        void operator-=(Stats const& s) {
            x_dot_1 -= s.x_dot_1;
            y_dot_1 -= s.y_dot_1;
            x_dot_x -= s.x_dot_x;
            x_dot_y -= s.x_dot_y;
            n       -= s.n;
        }
    };

    // input arguments
    FitFCOptions const& opt;
    ConstImage const& h1;
    ConstImage const& l1;
    ConstImage const& l2;
    ConstImage const& m;

    /**
     * @brief Compute a Stats object from the cropped input arguments and channel
     *
     * For the very first location this will be called for the part of the window that is in the
     * image. For all successive calls this will be called with the new stripe, where the window
     * moved to and the old stripe, where the window came from.
     */
    template<typename imgval_t>
    Stats collectStats(ConstImage h1_win, ConstImage l1_win, ConstImage l2_win, ConstImage m_win, int c) const;

    /**
     * @brief Regresses linear model and maps with it `h1` to `h2` and finds the residual when mapping `l1` to `l2`
     *
     * @tparam imgval_t is the C++ type of the image pixel values
     * @param s are the window sums (dot products) on which the regression is based, see the
     * description of @ref Stats.
     * @param h1_val is the central pixel of `h1`.
     * @param l1_val is the central pixel of `l1`.
     * @param l2_val is the central pixel of `l2`.
     *
     * This assumes a model \f$ Y = 1 \, a \, X + b + R \f$, where a and b are the parameters to
     * regress. This is accomplished solely with the values in `s` (using the fomula below).
     *
     * The regression works as follows. To find the best coefficients, we neglect \f$ R \f$. We
     * also vectorize the formulation by putting X together with the 1-vector into the matrix
     * \f$ Z := \begin{pmatrix}X & 1\end{pmatrix} \f$. Then we can reformulate the model:
     * \f[ Y = Z \, \begin{pmatrix} a\\ b \end{pmatrix}
     *     \quad\Leftrightarrow\quad
     *     Z^\top \, Y = Z^\top \, Z \, \begin{pmatrix} a\\ b \end{pmatrix}
     * \f]
     * This can be solved for the coefficients:
     * \f{equation*}{
     *     \begin{pmatrix} a\\ b \end{pmatrix}
     *       = (Z^\top \, Z)^{-1} \, Z^\top \, Y
     *       = \begin{pmatrix}X \cdot X & X \cdot 1 \\
     *                        X \cdot 1 & 1 \cdot 1 \end{pmatrix}^{-1} \,
     *         \begin{pmatrix}X \cdot Y \\ Y \cdot 1 \end{pmatrix}\\
     *       = \begin{pmatrix} \sum_i x_i^2 & \sum_i x_i\\
     *                         \sum_i x_i   & n \end{pmatrix}^{-1} \,
     *         \begin{pmatrix} \sum_i x_i \, y_i \\ \sum_i y_i \end{pmatrix}
     *       = \frac{1}{n \sum_i x_i^2 - \left( \sum_i x_i \right)^2}
     *         \begin{pmatrix} n          & -\sum_i x_i \\
     *                        -\sum_i x_i &  \sum_i x_i^2 \end{pmatrix} \,
     *         \begin{pmatrix} \sum_i x_i \, y_i \\ \sum_i y_i \end{pmatrix}
     * \f}
     *
     *
     * @return predicted high resolution value `a * h1_val + b` and the residual
     * `l2_val - (a * l1_val + b)`. If the regression cannot be done, which happens when all X
     * values are equal, the predicted value is just `h1_val` and the residual `l2_val - l1_val`,
     * which is equivalent to a = 1 and b = 0.
     */
    template<typename imgval_t>
    std::pair<imgval_t, double> regressPixel(Stats s, imgval_t h1_val, imgval_t l1_val, imgval_t l2_val) const;

    template<Type basetype>
    std::pair<Image, Image> operator()() const;
};


/**
 * @brief This functor uses the best neighbors to predict the central pixel of a window
 *
 * @param opt holds the Fit-FC options. This is used to get the number of neighbors to use (via
 * @ref FitFCOptions::getNumberNeighbors() "getNumberNeighbors()").
 *
 * @param x_center is the center x-coordinate relative to the window origin
 *
 * @param y_center is the center y-coordinate relative to the window origin
 *
 * @param h1_win is the current window of the high resolution image. It is used for filtering
 * (finding the most similar near pixels).
 *
 * @param frm_win contains the current window of the regression model \f$ \hat F_{\mathrm{RM}} \f$
 * (the first output of RegressionMapper).
 *
 * @param r_win contains the current window of the fine residual \f$ R \f$ (the second output of
 * RegressionMapper, but bicubicly filtered).
 *
 * @param mask_win is either empty or contains the current window of the given single-channel mask.
 *
 * @param dw_win contains the current window of the inverse distance weights defined as
 * \f$ \frac{1}{d_i} \f$ with
 * \f$ d_i := 1 + \frac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{\tfrac w 2} \f$,
 * where w is the window size. This is generally the same image but on image boundaries it is
 * cropped.
 *
 * @param out_pixel is the (shared copy of the) central pixel of the output image.
 *
 * This functor is called from FitFCFusor::predict() with help of CallBaseTypeFunctor::run():
 * @code
 * CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
 *             opt, x_win, y_win, h1_win, frm_win, r_win, mask_win, dw_win, out_pixel},
 *             output.type());
 * @endcode
 *
 * Firstly, it calculates the RMSE of each valid pixel to the central pixel over all bands, i. e.
 * \f[ D(x, y) := \frac 1 2 \sqrt{\sum_{b=1}^{n_b} \left( h_1(x, y, b) - h_1(x_c, y_c, b) \right)^2}
 *     \quad \forall x, y. \f]
 * The `N` best pixels will be selected, where `N` is the number of neighbors from the FitFCOptions
 * `opt`. When there are multiple values with the same RMSE, the nearest will be used. Then these
 * locations are used to collect the inverse distance weights \f$ \frac{1}{d_i} \f$, the regression
 * model pixels from \f$ \hat F_{\mathrm{RM}} \f$ and the bicubic interpolated residuals from
 * \f$ r \f$. Finally, the output is
 * \f[ h_2(x_c, y_c, b)
 *     = \frac{1}{\sum_{i=1}^N \frac{1}{d_i}} \,
 *       \sum_{i=1}^N \frac{1}{d_i} \left( \hat F_{\mathrm{RM}}(x_i, y_i, b) + r(x_i, y_i, b) \right) \f]
 */
struct FilterStep {
    struct Score {
        double diff;
        unsigned int x;
        unsigned int y;
        unsigned int xc;
        unsigned int yc;

        Score(double diff, unsigned int x, unsigned int y, unsigned int xc, unsigned int yc)
            : diff(diff), x(x), y(y), xc (xc), yc(yc)
        { }

        bool operator<(Score const& s) const {
            return diff < s.diff ||
                    (diff == s.diff && (x-xc) * (x-xc) + (y-yc) * (y-yc) < (s.x-xc) * (s.x-xc) + (s.y-yc) * (s.y-yc));
        }
    };

    FitFCOptions const& opt;
    unsigned int x_center;
    unsigned int y_center;
    ConstImage const& h1_win;
    ConstImage const& frm_win;
    ConstImage const& r_win;
    ConstImage const& mask_win;
    ConstImage const& dw_win;
    Image& out_pixel;

    /**
     * @brief This function is required for the functor pattern for dynamic
     * image type paradigm.
     *
     * @see CallBaseTypeFunctor for the pattern and the detailed description of FilterStep above
     * for a description of what this method actually does.
     */
    template<Type basetype>
    void operator()() const;
};


/**
 * @brief Downscale with averaging and upscale with cubic interpolation again
 *
 * This is used for the *bicubic interpolation* of the residual image. The scale factor is taken
 * from the FitFCOptions::getResolutionFactor().
 */
Image cubic_filter(Image i, double scale);

} /* namespace fitfc_impl_detail */



/**
 * @brief The FitFCFusor class is the implementation of the Fit-FC algorithm
 *
 * Fit-FC is *a three-step method consisting of regression model fitting (RM fitting), spatial
 * filtering (SF) and residual compensation (RC)*. It requires a relatively low amount of
 * computation time for prediction.
 *
 * For Fit-FC three images on two dates are required. The date 1 is the input image pair date and
 * date 2 is the prediction date, see the following table:
 * <table>
 * <caption id="fitfc_3_image_structure">Fit-FC image structure with three images</caption>
 *   <tr><td><span style="display:inline-block; width: 6em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">1</td>
 *       <td align="center" valign="top">2</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center"><table><tr><th style="width: 6em;">High 1</th></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 6em;"><b>High 2</b></td></tr></table></td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 1</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 2</th></tr></table></td>
 *   </tr>
 * </table>
 * Note, despite the numbering, date 1 can actually be a date after date 2.
 *
 * Fit-FC basically iterates through all pixels in an image. For each pixel (called center pixel)
 * it regresses a linear model with all pixels of a surrounding window from Low 1 to Low 2. This
 * model maps the central pixel of High 1 to the non-existing High 2.
 *
 * After this is done for every pixel Fit-FC iterates again through all pixels. But now it selects
 * the N most similar pixels and uses their regressed value, adds the filtered residual (of the
 * regression) and weights them.
 *
 * There are differences of this implementation to the algorithm described in the original paper.
 * These are listed here:
 *
 *  * The paper does not define which pixels should be preferred in filtering stage, when the `N`th
 *    and `(N+1)`th pixels (and maybe more) have equal differences. We propose to prefer the closer
 *    pixels. So the ordering is determined by
 *    \f$ (D_i < D_j) \vee (D_i = D_j \wedge (x_i - x_c)^2 + (y_i - y_c)^2 < (x_j - x_c)^2 + (y_j - y_c)^2) \f$.
 *  * In general, DataFusor%s in the image fusion framework are not imposed to do preprocessing.
 *    Although it would be possible to handle that, it would lead to inconsistent handling and so the
 *    input images are assumed to have the same resolution (low resolution is already upscaled /
 *    warped). The Fit-FC algorithm as described in the paper includes upsampling as part of the
 *    algorithm. So to be close to the paper the low resolution images should use nearest neighbor
 *    method when upsampled.
 *  * The regression of the coefficients a and b in the regression model is not described well in
 *    the paper. There must be used more than one coarse pixel for regression, otherwise the residual
 *    would vanish. So the window must in principle be able to cover multiple coarse pixels. In that
 *    case it is not completely clear how a coarse pixel, which is not completely included in the
 *    window, is used. It might be weighted by the coverage of the window, but the paper does not
 *    state anything about it. However, this implementation does exactly that, since it is natural to
 *    do that when the images have the same resolution. So when the low resolution images were
 *    upscaled with the nearest neighbor method, there are blocks of pixels with the same values.
 *    When multiple of these pixels are covered of the window, the values are used multiple times,
 *    which is equivalent to weight the coarse pixels with the coverage. However, if the paper did
 *    not meant to do that (and probably it would describe that weighting if it had meant that), we
 *    do that differently. We also do not get a blocky regression model because of that, but rather a
 *    bilinear filtered one, because the coverage weights are behaving bilinearly.
 *
 * Parallelization is done implicitly and the usage of @ref Parallelizer "Parallelizer<FitFCFusor>"
 * is forbidden with this algorithm. The reason is that the filtering of the residual would cause
 * different results, when the prediction area was split up for parallelization.
 */
class FitFCFusor : public DataFusor {
public:
    /**
     * @brief This declares which option type to use
     */
    using options_type = FitFCOptions;

    /**
     * @brief Process the FitFCOptions
     * @param o is an options object of type FitFCOptions and replaces the current options object.
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
     * also \ref fitfc_3_image_structure "Fit-FC image structures".
     *
     * @param maskParam should either be empty or an arbitrary mask in the size of the source
     * images. It can be single-channel or multi-channel, but when it is multi- channel, it is
     * converted to single-channel and locations are marked invalid, if one of the channels is
     * invalid. Zero values prevent the usage of any image at these locations. The result at these
     * locations is undefined.
     */
    void predict(int date2, ConstImage const& maskParam = ConstImage{}) override;

protected:
    /// FitFCOptions to use for the next prediction
    options_type opt;

    /**
     * @brief Get area where pixels are read
     * @param fullImgSize size of the source image. This is used as bounds.
     * @param predArea is the prediction area used. It must be valid (not just all-zero).
     *
     * The sample area is the area from which pixels are read. That is the prediction area with the
     * full window size on each side around, but limited by the image bounds.
     *
     * @return sample area rectangle.
     */
    Rectangle findSampleArea(Size const& fullImgSize, Rectangle const& predArea) const;

    /**
     * @brief Get weights map for the distance to the center pixel
     *
     * This makes a map for the inverse relative distance in the size of the window. So it
     * precomputes all results of the formula
     * \f$ \frac{1}{d_i} \f$ with
     * \f$ d_i := 1 + \frac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{\tfrac w 2} \f$,
     * where \f$ (x_c, y_c) \f$ is the center pixel of the moving window and
     * \f$ w \f$ is the window size.
     *
     * @return inverse distance weights as single-channel image of type double.
     */
    Image computeDistanceWeights() const;

    /**
     * @brief Check the input images size, type, etc.
     * @param mask will also be checked
     * @param date2 is the prediction date to get the corresponding image
     */
    void checkInputImages(ConstImage const& mask, int date2) const;

    /**
     * @brief Regress coarse images
     *
     * This just calls the @ref fitfc_impl_detail::RegressionMapper "RegressionMapper" functor.
     */
    std::pair<Image, Image> regress(ConstImage const& h1, ConstImage const& l1, ConstImage const& l2, ConstImage const& mask) const;
};


/// \cond
#ifdef WITH_OMP
namespace fitfc_impl_detail {
template <typename... T>
struct specialization_not_allowed : std::false_type
{ };
} /* namespace fitfc_impl_detail */

/**
 * @brief Forbid to use FitFCFusor with Parallelizer.
 *
 * Since parallelization on a macro level would change the resulting image compared to
 * none-parallel execution, parallelization is done internally. The reason for the change is that
 * the bicubic interpolation of the residual depends strongly on the borders of the sample area,
 * which would be different when using prediction areas. The Parallelizer splits the image into
 * separate prediction areas for parallelization.
 */
template <class AlgOpt>
class Parallelizer<FitFCFusor, AlgOpt> {
    static_assert(fitfc_impl_detail::specialization_not_allowed<AlgOpt>::value, "Parallelizer not supported for FitFCFusor, but it is internally parallelized with OpenMP.");
};
#endif /* WITH_OMP */
/// \endcond

} /* namespace imagefusion */

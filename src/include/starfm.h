#pragma once

#include "datafusor.h"
#include "starfm_options.h"
#include <cmath>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace imagefusion {

/**
 * @brief Implementation details of STARFM -- not to be used by library users
 *
 * This namespace just contains the functor, which predicts one pixel in a window.
 */
namespace starfm_impl_detail {

/**
 * @brief This functor predicts the value for one channel of the central pixel of a window
 *
 * @param opt holds the STARFM options. This is used for getting the mode (via @ref
 * StarfmOptions::isSinglePairModeConfigured() "isSinglePairModeConfigured()" and @ref
 * StarfmOptions::isDoublePairModeConfigured() "isDoublePairModeConfigured()"), uncertainties (via
 * @ref StarfmOptions::getTemporalUncertainty() "getTemporalUncertainty()" and @ref
 * StarfmOptions::getSpectralUncertainty() "getSpectralUncertainty()"), temporal difference usage
 * (via @ref StarfmOptions::getUseTempDiffForWeights() "getUseTempDiffForWeights()") and the strict
 * filtering configuration (via @ref StarfmOptions::getUseStrictFiltering()
 * "getUseStrictFiltering()").
 *
 * @param x_center is the center x-coordinate relative to the window origin
 *
 * @param y_center is the center y-coordinate relative to the window origin
 *
 * @param c is the channel in which the pixel value should be predicted. So this functor predicts
 * only one value for a single channel.
 *
 * @param tol_vec is a vector conaining one or two cv::Scalar objects (depending on the mode
 * configuration; single or double pair). Each cv::Scalar object contains the tolerances for the
 * input pairs for all channels. In the formulae below the tolerance for the current channel of
 * input pair \f$ k \f$ is called \f$ \varepsilon_k \f$.
 *
 * @param dt_win_vec contains the current window of the temporal difference images (one for single
 * pair mode, two for double pair mode).
 *
 * @param ds_win_vec contains the current window of the spatial difference images (one for single
 * pair mode, two for double pair mode).
 *
 * @param lv_win_vec contains the current window of the local value images (one for single pair
 * mode, two for double pair mode). The local values are the trivial prediction like
 * lv = h1 + l2 - l1.
 *
 * @param hk_win_vec contains the current window of the high resolution images (one for single pair
 * mode, two for double pair mode).
 *
 * @param mask_win is either empty or contains the current window of the given single-channel or
 * multi-channel mask.
 *
 * @param dw_win contains the current window of the distance weights defined as
 * \f$ 1 + \frac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{\tfrac s 2} \f$. This is generally always the
 * same but on image boundaries it is cropped.
 *
 * @param out_pixel is the (shared copy of the) central pixel of the output image.
 *
 * This functor is called from StarfmFusor::predict() with help of CallBaseTypeFunctor::run():
 * @code
 * CallBaseTypeFunctor::run(starfm_impl_detail::PredictPixel{
 *         opt, x_win, y_win, c, tol_vec, dt_win_vec, ds_win_vec, lv_win_vec, hk_win_vec, mask_win, dw_win, out_pixel},
 *         output.type());
 * @endcode
 *
 * Firstly, it calculates the temporal uncertainty \f$ \sigma_t \f$, spectral uncertainty \f$
 * \sigma_s \f$ and combined uncertainty \f$ \sigma_c := \sqrt{\sigma_t^2 + \sigma_s^2} \f$. Then
 * each pixel in each input pair is checked for similarity to the central pixel at \f$ (x_c, y_c)
 * \f$. The pixel at \f$ (x, y) \f$ of pair \f$ k \f$ is similar if
 *  * the high resolution reflectance value is similar to the central one, i. e.
 *    \f$ |h_k(x, y) - h_k(x_c, y_c)| < \varepsilon_k \f$
 *  * the temporal difference is lower than the central one, i. e.
 *    \f$ |l_2(x, y) - l_k(x, y)| < \min_k |l_2(x_c, y_c) - l_k(x_c, y_c)| \f$
 *  * the spectral difference is lower than the central one, i. e.
 *    \f$ |h_k(x, y) - l_k(x, y)| < \min_k |h_k(x_c, y_c) - l_k(x_c, y_c)| \f$
 *
 * Then (for similar pixels) weights are calculated:
 * \f[
 *  w_k(x, y) =
 * \begin{cases}
 *   \dfrac{1}{(S + 1) \, (T + 1) \, D} & \text{if } (S + 1) \, (T + 1) \ge \sigma_c\\
 *   1                                  & \text{if } (S + 1) \, (T + 1) < \sigma_c
 * \end{cases},
 * \f]
 * where \f$ S = |h_k(x, y) - l_k(x, y)| \f$ = `ds_win`(x, y) is the spectral distance,
 *       \f$ T = |l_2(x, y) - l_k(x, y)| \f$ = `dt_win`(x, y) is the temporal distance and
 *       \f$ D = 1 + \dfrac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{\tfrac s 2} \f$ = `dw_win`(x, y)
 * with \f$ s \f$ being the window size. Depending on the @ref
 * StarfmOptions::setUseTempDiffForWeights() "option" whether the temporal difference should be
 * used for weighting, \f$ T \f$ can also be 0 here. For the sake of simplicity in following
 * formulas we assume weights of non-similar pixels are 0.
 *
 * The weights of all pairs (one or two) in a window are summed up, as well as their local
 * prediction values multiplied by the weight, i. e.
 * \f$ w_k(x, y) \, (h_k(x, y) + l_2(x, y) - l_k(x, y)) \f$. The weighted average from these two
 * sums gives the prediction at the center:
 * \f[ h_2(x_c, y_c) = \frac{\sum\limits_{k,x,y} w_k(x, y) \, (h_k(x, y) + l_2(x, y) - l_k(x, y))}
 *                          {\sum\limits_{k,x,y} w_k(x, y)} \f]
 *
 * In the rare case that there is no similar pixel in the current window the local prediction value
 * is used directly for single-pair mode and the average of both local prediction values for
 * double-pair mode.
 */
struct PredictPixel {
    StarfmOptions const& opt;
    unsigned int x_center;
    unsigned int y_center;
    unsigned int c;
    std::vector<std::vector<double>> const& tol_vec;
    std::vector<ConstImage> const& dt_win_vec;
    std::vector<ConstImage> const& ds_win_vec;
    std::vector<ConstImage> const& lv_win_vec;
    std::vector<ConstImage> const& hk_win_vec;
    ConstImage const& mask_win;
    ConstImage const& dw_win;
    Image& out_pixel;

    /**
     * @brief This function is required for the functor pattern for dynamic image type paradigm.
     *
     * @see CallBaseTypeFunctor for the pattern and the detailed description of PredictPixel above
     * for a description of what this method actually does.
     */
    template<Type basetype>
    void operator()() const;
};

} /* namespace starfm_impl_detail */



/**
 * @brief The StarfmFusor class is the implementation of the STARFM algorithm
 *
 * STARFM stands for *spatial and temporal adaptive reflectance fusion model*. It requires a
 * relatively low amount of computation time for prediction.
 *
 * For STARFM either three images on two dates or five images on three dates are required. The date
 * 1 or 3 are the input image pair dates and date 2 is the prediction date, see the following
 * tables:
 * <table>
 * <caption id="starfm_3_image_structure">STARFM image structure with three images</caption>
 *   <tr><td><span style="display:inline-block; width: 6em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">1 or 3</td>
 *       <td align="center" valign="top">2</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center"><table><tr><th style="width: 6em;">High 1 or 3</th></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 6em;"><b>High 2</b></td></tr></table></td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 1 or 3</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 2</th></tr></table></td>
 *   </tr>
 * </table>
 * <table>
 * <caption id="starfm_5_image_structure">STARFM image structure with five images</caption>
 *   <tr><td><span style="display:inline-block; width: 6em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">1</td>
 *       <td align="center" valign="top">2</td>
 *       <td align="center" valign="top">3</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center"><table><tr><th style="width: 6em;">High 1</th></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 6em;"><b>High 2</b></td></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 6em;">High 3</th></tr></table></td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 1</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 2</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 6em;">Low 3</th></tr></table></td>
 *   </tr>
 * </table>
 *
 * STARFM basically iterates through all pixels in an image and looks in a region around (window)
 * for similar high resolution pixels. From the similar pixels weights \f$ W \f$ are calculated by
 * their distance weight \f$ D \f$ (see computeDistanceWeights()), temporal difference
 * \f$ T = |L_2 - L_k| \f$ and spectral difference \f$ S = |H_k - L_k| \f$ by
 * \f[ C = (S + 1) \, (T + 1) \, D \f]
 * and then inverted to get \f$ W = \frac 1 C \f$. From a local prediction value
 * \f[ H_2 = L_2 + H_k - L_k = H_k + L_2 - L_k \f]
 * the predictions of similar, near pixels are used in a weighted average to predict the current
 * (also: central) pixel. A near pixel is similar, if the absolute difference to the central pixel
 * in the high resolution image is low enough and the spectral and temporal differences are lower
 * than the ones of the central pixel. Therefore similarity and weighting are key features of this
 * algorithm and determine its accuracy.
 *
 * There are differences of this implementation to the algorithm described in the original paper.
 * These are listed here:
 *  * For the weighting (10) states: \f$ C = S \, T \, D \f$, but we use
 *    \f$ C = (S + 1) \, (T + 1) \, D \f$ according to the reference implementation. With
 *    StarfmOptions::setLogScaleFactor(double b) the weighting formula can be changed to
 *    \f$ C = \ln(S \, b + 2) \, \ln(T \, b + 2) \, D \f$.
 *  * In addition to the temporal uncertainty \f$ \sigma_t \f$ (see
 *    StarfmOptions::setTemporalUncertainty()) and the spectral uncertainty \f$ \sigma_s \f$ (see
 *    StarfmOptions::setSpectralUncertainty()) there will be used a *combined uncertainty*
 *    \f$ \sigma_c := \sqrt{\sigma_t^2 + \sigma_s^2} \f$. This will be used in the candidate
 *    weighting: If \f$ (S + 1) \, (T + 1) < \sigma_c \f$, then \f$ C = 1 \f$, instead of the
 *    formula above.
 *  * Considering candidate weighting again, there is an option
 *    (StarfmOptions::setUseTempDiffForWeights()) to not use the temporal difference for the
 *    weighting (also not for the combined uncertainty check above), i. e. T = 0 then. This is also
 *    the default behaviour.
 *  * The basic assumption of the original paper that with zero spectral or temporal difference the
 *    central pixel will be chosen is wrong since there might be multiple pixels with zero difference
 *    within one window. Also due to the addition of 1 to the spectral and temporal differences, the
 *    weight will not increase so dramatical from a zero difference. However, these assumptions can
 *    be enforced with StarfmOptions::setDoCopyOnZeroDiff(), which is the default behaviour.
 *  * The paper states that a good candidate should satisfy (15) *and* (16). This can be set with
 *    StarfmOptions::setUseStrictFiltering(), which is by default used. However the other behaviour,
 *    that a candidate should fulfill (15) *or* (16), as in the reference implementation, can be also
 *    be selected with that option.
 *  * The paper uses max in (15) and (16), which would choose the largest spectral and temporal
 *    difference from all input pairs (only one or two are possible). Since this should filter out
 *    bad candidates, we believe this is a mistake and should be min instead of max, like it is done
 *    in the reference implementation. So this implementation uses min here.
 *
 * For parallelization @ref Parallelizer "Parallelizer<StarfmFusor>" can be used with @ref
 * ParallelizerOptions "ParallelizerOptions<StarfmOptions>". The \ref parallelusage "example" on
 * the main page shows how to do that.
 */
class StarfmFusor : public DataFusor {
public:
    /**
     * @brief This declares which option type to use
     *
     * This is done for Parallelizer to allow to default the AlgOpt template argument. So it is
     * enough to specify the fusor type, like
     * @code
     * Parallelizer<StarfmFusor>
     * @endcode
     * instead of
     * @code
     * Parallelizer<StarfmFusor, StarfmOptions>
     * @endcode
     */
    using options_type = StarfmOptions;

    /**
     * @brief Process the STARFM options
     * @param o is an options object of type StarfmOptions and replaces the current options object.
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
     * also \ref starfm_3_image_structure "STARFM image structures".
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
    /// StarfmOptions to use for the next prediction
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
     * results of the formula \f$ 1 + \frac{\sqrt{(x - x_c)^2 + (y - y_c)^2}}{\tfrac s 2} \f$,
     * where \f$ (x_c, y_c) \f$ is the center pixel of the moving window and \f$ s \f$ is the
     * window size.
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
};


} /* namespace imagefusion */

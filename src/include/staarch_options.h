#pragma once

#include "starfm_options.h"
#include "exceptions.h"

#ifdef _OPENMP
    #include <omp.h>
#endif /* _OPENMP*/

namespace imagefusion {

/**
 * @brief Contains all options regarding STAARCH and prediction with STARFM
 *
 * STAARCH itself is an algorithm to find the date of disturbance for each pixel location, where
 * a disturbance is detected. This date of disturbance image can also help when predicting using
 * STARFM. This is integrated in the StaarchFusor::predict() method. However, since STARFM is
 * involved here, the options for STARFM can also be set. Have a look into the documentation of the
 * methods -- it is states when the setting is used by the underlying STARFM algorithm.
 */
class StaarchOptions : public Options {
public:

    // STARFM options ////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Set the window size in which will be searched for similar pixels
     *
     * @param size for the search window. Must be an odd number. Example: The default value of 51
     * means 25 pixel up and down, or left and right, plus the center pixel.
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @throws invalid_argument_error if size is even.
     * @see getWinSize()
     */
    void setWinSize(unsigned int size = 51) {
        s_opt.setWinSize(size);
    }

    /**
     * @brief Get the window size in which is searched for similar pixels
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return The window size in pixel.
     * @see setWinSize()
     */
    unsigned int getWinSize() const {
        return s_opt.getWinSize();
    }


    /**
     * @brief Set the number of classes to influence similarity tolerance
     *
     * @param classes denoted by \f$ n \f$ in the following, is the value by which the doubled
     * standard deviation \f$ s \f$ is divided to set the tolerance for similarity, i. e.
     * \f$ \emph{tol} = 2 \, \frac s n \f$. To be accepted, candidates have to satisfy the condition
     * \f$ |h(x_c, y_c) - h(x, y) | \le \emph{tol} \f$ where \f$ (x_c, y_c) \f$ is the center
     * location of a window. However they also have to have a lower spectral and temporal
     * difference. Note, this is considered separately for each channel and pair.
     *
     * This option is used by the underlying STARFM algorithm. For the number of land classes, see
     * #setNumberLandClasses(), which determines the number of clusters for the standardizations.
     *
     * @see getNumberSTARFMClasses()
     */
    void setNumberSTARFMClasses(double classes = 40) {
        s_opt.setNumberClasses(classes);
    }

    /**
     * @brief Get the number of classes
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return number of classes.
     * @see setNumberSTARFMClasses()
     */
    double getNumberSTARFMClasses() const {
        return s_opt.getNumberClasses();
    }


    /**
     * @brief Set the temporal uncertainty
     *
     * @param sigmaT temporal uncertainty. This will be multiplied by sqrt(2) and then added to the
     * central temporal difference in each window. It must be a non-negative number.
     *
     * The default value is 1, which might fit to 8 bit images. For 16 bit images with a data range
     * from 0 to 10000 the reference implementation uses 50 here.
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @throws invalid_argument_error if sigmaT < 0.
     * @see getTemporalUncertainty(), setSpectralUncertainty()
     */
    void setTemporalUncertainty(double sigmaT = 1) {
        s_opt.setTemporalUncertainty(sigmaT);
    }

    /**
     * @brief Get temporal uncertainty
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return temporal uncertainty
     * @see setTemporalUncertainty()
     */
    double getTemporalUncertainty() const {
        return s_opt.getTemporalUncertainty();
    }


    /**
     * @brief Set the spectral uncertainty
     *
     * @param sigmaS spectral uncertainty. The norm of this and the temporal uncertainty will be
     * added to the central spectral difference in each window. It must be a non-negative number.
     *
     * The default value is 1, which might fit to 8 bit images. For 16 bit images with a data range
     * from 0 to 10000 the reference implementation uses 50 here.
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @throws invalid_argument_error if sigmaS < 0.
     * @see getSpectralUncertainty(), setTemporalUncertainty()
     */
    void setSpectralUncertainty(double sigmaS = 1) {
        s_opt.setSpectralUncertainty(sigmaS);
    }

    /**
     * @brief Get spectral uncertainty
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return spectral uncertainty
     * @see setSpectralUncertainty()
     */
    double getSpectralUncertainty() const {
        return s_opt.getSpectralUncertainty();
    }

    /**
     * @brief Set whether to use strict filtering
     * @param strict specifies, whether to use strict filtering as in the STARFM paper
     *
     * So the original paper says in the text before (13) and (14) that a good candidate must
     * satisfy both equations (actually (15) and (16) to be more precise). This is meant here with
     * strict. In contrast, the reference implementation (by the USDA) of STARFM will accept
     * candidates that satisfy just one of the equations. This behaviour can be selected with
     * `strict == false`. This will result in a lot more candidates used, that might not be
     * appropriate.
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @see getUseStrictFiltering()
     */
    void setUseStrictFiltering(bool strict = false) {
        s_opt.setUseStrictFiltering(strict);
    }

    /**
     * @brief Get setting whether to use strict filtering
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return current setting
     * @see setUseStrictFiltering() for a detailed description
     */
    bool getUseStrictFiltering() const {
        return s_opt.getUseStrictFiltering();
    }


    /**
     * @brief Set whether to copy values on zero spectral or temporal difference
     * @param copy specifies, whether to copy the pixel value, when the difference is zero.
     *
     * The original paper makes some basic assumptions: It states that if no change in the temporal
     * difference appears, the result will be the high resolution pixel and if no change in the
     * spectral difference appears, it will be the new low resolution pixel. These assumptions are
     * wrong, since there can be multiple candidates with zero difference, but different values.
     * However, with this option the assumptions will be forced, by copying the values for zero
     * difference pixels.
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @see getDoCopyOnZeroDiff()
     */
    void setDoCopyOnZeroDiff(bool copy = false) {
        s_opt.setDoCopyOnZeroDiff(copy);
    }

    /**
     * @brief Get setting whether to copy values on zero spectral or temporal difference
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return current setting
     * @see setDoCopyOnZeroDiff() for a detailed description
     */
    bool getDoCopyOnZeroDiff() const {
        return s_opt.getDoCopyOnZeroDiff();
    }


    /**
     * @brief Set whether to use the temporal difference for weighting
     *
     * @param use setting whether to use the temporal differents for weighting of the candidates.
     *
     * The original paper uses the distance factor, spectral and temporal differences in the
     * weighting of candidates (10):
     * \f[ C = (S + 1) \, (T + 1) \, D. \f]
     * However, the reference implementation uses the temporal difference only when using multiple
     * pairs. Otherwise only the distance factor and spectral weight are used:
     * \f[ C = (S + 1) \, D. \f]
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @see getUseTempDiffForWeights(), #imagefusion::StarfmOptions::TempDiffWeighting
     */
    void setUseTempDiffForWeights(StarfmOptions::TempDiffWeighting use = StarfmOptions::TempDiffWeighting::enable) {
        s_opt.setUseTempDiffForWeights(use);
    }

    /**
     * @brief Get setting whether to use the temporal difference for weighting
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return current setting
     * @see setUseTempDiffForWeights() for a detailed description
     */
    StarfmOptions::TempDiffWeighting getUseTempDiffForWeights() const {
        return s_opt.getUseTempDiffForWeights();
    }

    /**
     * @brief Set and enable/disable the logarithmic scale factor in weighting
     *
     * @param b is the factor. If `b` is 0 logarithmic scaling is not used in weighting. The paper
     * suggests to use the upper range limit for this factor (e. g. 10000 for MODIS), when using
     * it.
     *
     * When using a `b` > 0, the logistic formula
     * \f[ C = \ln(S \, b + 2) \, \ln(T \, b + 2) \, D \f]
     * is used for weighting. This is (11) in the original paper. Note, this option is not
     * implemented in the reference implementation.
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @see getLogScaleFactor()
     */
    void setLogScaleFactor(double b = 0) {
        s_opt.setLogScaleFactor(b);
    }

    /**
     * @brief Get and enable/disable the logarithmic scale factor in weighting
     *
     * This option is used by the underlying STARFM algorithm.
     *
     * @return the factor
     * @see setLogScaleFactor()
     */
    double getLogScaleFactor() const {
        return s_opt.getLogScaleFactor();
    }



    // STAARCH options ///////////////////////////////////////////////////////////////////////////

    /**
     * \brief Options for the window alignment for avaraging
     *
     * You can specify the window alignment for the averaging of the low resolution disturbance
     * index images with setDIMovingAverageWindow() and there are three options.
     *
     * Example with n = 3 (see setNumberImagesForAveraging()), when averaging for position i:
     * <table style="border: none;">
     *  <tr>
     *    <td>MovingAverageWindow::backward</td>
     *    <td align="center" style="width: 3em;">i-3</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i-2</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i-1</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i</td>
     *    <td align="center" style="width: 3em;">i+1</td>
     *    <td align="center" style="width: 3em;">i+2</td>
     *    <td align="center" style="width: 3em;">i+3</td>
     *  </tr>
     *  <tr>
     *    <td>MovingAverageWindow::center</td>
     *    <td align="center" style="width: 3em;">i-3</td>
     *    <td align="center" style="width: 3em;">i-2</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i-1</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i+1</td>
     *    <td align="center" style="width: 3em;">i+2</td>
     *    <td align="center" style="width: 3em;">i+3</td>
     *  </tr>
     *  <tr>
     *    <td>MovingAverageWindow::forward</td>
     *    <td align="center" style="width: 3em;">i-3</td>
     *    <td align="center" style="width: 3em;">i-2</td>
     *    <td align="center" style="width: 3em;">i-1</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i+1</td>
     *    <td align="center" style="width: 3em; background-color: #B0E0E6;">i+2</td>
     *    <td align="center" style="width: 3em;">i+3</td>
     *  </tr>
     *
     */
    enum class MovingAverageWindow {
        backward, ///< For image i use the average of images i - (n-1), ..., i.
        center,   ///< For image i use the average of images i - n/2, ..., i + n/2 (rounded with floor).
        forward   ///< For image i use the average of images i, ..., i + n-1. This is used in the paper.
    };

    /**
     * @brief Specify the alignment of the moving average filter window
     * @param a is the alignment.
     *
     * The paper suggests to average the DI i using images i, i+1, i+2. This corresponds to
     * [MovingAverageWindow::forward](#imagefusion::StaarchOptions::MovingAverageWindow::forward) and 3 images used for averaging. For a visual example, see
     * the documentation of #StaarchFusor.
     *
     * @see getDIMovingAverageWindow(), MovingAverageWindow, setNumberImagesForAveraging()
     */
    void setDIMovingAverageWindow(MovingAverageWindow a = MovingAverageWindow::forward) {
        avgWin = a;
    }

    /**
     * @brief Get the current setting for the alignment of the moving average filter window
     * @return alignment
     * @see setDIMovingAverageWindow(), MovingAverageWindow
     */
    MovingAverageWindow getDIMovingAverageWindow() const {
        return avgWin;
    }

    /**
     * @brief Set the number of land classes to cluster the first high res image
     *
     * @param classes are the number of clusters used in the k-means algorithm, which is applied on
     * the first high res image. On each of these clusters -– representing land classes -– the
     * disturbance index is standardized separately.
     *
     * The clustering is performed in the tasseled cap color space (brightness, greeness, wetness)
     * using the k-means++ algorithm.
     *
     * @note The clustering will not be performed, if a non-empty image has been set in
     * setClusterImage(). In that case, the provided cluter image will be used.
     *
     * @see getNumberLandClasses()
     */
    void setNumberLandClasses(unsigned int classes = 10) {
        clusters = classes;
    }

    /**
     * @brief Get setting for the number of land classes
     * @return number of land classes used for clustering of the first high res image.
     *
     * @see setNumberLandClasses()
     */
    unsigned int getNumberLandClasses() const {
        return clusters;
    }

    /**
     * @brief Specify an image with cluster labels for land classes
     *
     * @param img is a single-channel integer image that specifies the land class clusters, which
     * are used for standardization of the high resolution tasseled cap and NDVI images. Each land
     * class is separately standardized. Negative values are considered as invalid locations. For
     * them a standardization will not be performed.
     * Default: empty
     *
     * The land classification can be done with a k-means algorithm. For that, you just have to
     * leave this image empty and then the number specified in setNumberLandClasses() will be used
     * as k in the k-means clustering algorithm. However, k-means is a non-deterministic algorithm,
     * so the outcome is differently at each run. So with this method, you can provide your own
     * land classification. To get the same land classification as StaarchFusor, the following code
     * could be used:
     *
     * @code
     * Image highImgLeft = ...;   // left high resolution image
     * Image highMask = ...;      // mask to mark locations that are valid in both left and right high resolution images
     *
     * StaarchOptions opt;
     * opt.set ...   // set the options for sensor type and source channels, we will use them for the clustering
     *
     * // clustering is done in tasseled cap color space
     * auto tc_mapping = StaarchOptions::sensorTypeToTasseledCap(opt.getHighResSensor());
     * Image highTCLeft = highImgLeft.convertColor(tc_mapping, Type::float32, opt.getHighResSourceChannels());
     *
     * unsigned int nclusters = 4;    // choose the number of clusters. Default: 10
     * Image landClasses = staarch_impl_detail::cluster(highTCLeft, highMask, nclusters);
     * opt.setClusterImage(std::move(landClasses));
     * @endcode
     */
    void setClusterImage(Image img = {}) {
        using std::to_string;
        if (img.channels() != 1)
            IF_THROW_EXCEPTION(invalid_argument_error("The cluster image must be a single-channel integer image. The one you gave has " + to_string(img.channels()) + " channels.")) << errinfo_image_type(img.type());
        if (isFloatType(img.basetype()))
            IF_THROW_EXCEPTION(invalid_argument_error("The cluster image must be a single-channel integer image. The one you gave is floating point image of type " + to_string(img.basetype()) + ".")) << errinfo_image_type(img.type());

        clusterImage = std::move(img);
    }

    ConstImage const& getClusterImage() const {
        return clusterImage;
    }

    /**
     * @brief Set the interval dates and drop the date of disturbance image
     *
     * @param left is the lower interval bound
     * @param right is the upper interval bound
     *
     * See also
     * \ref staarch_image_structure "STAARCH image structure".
     *
     * Remember to call StaarchFusor::processOptions() after setting the interval dates! This will
     * drop the date of disturbance image (if one of the dates is different to the previous ones).
     *
     * @throws invalid_argument_error if left >= right.
     * @see getIntervalDates()
     */
    void setIntervalDates(int left, int right) {
        dateLeft = left;
        dateRight = right;
        if (left >= right)
            IF_THROW_EXCEPTION(invalid_argument_error("The left (lower) date of an interval must be smaller than the right (upper) date. You gave [" + std::to_string(left) + ", " + std::to_string(right) + "]"));
        areDatesSet = true;
    }

    /**
     * @brief Get the interval dates
     * @returns the current interval dates.
     * @throws runtime_error if they have not been set yet.
     * @see setIntervalDates()
     */
    std::pair<int, int> getIntervalDates() const {
        if (!areDatesSet)
            IF_THROW_EXCEPTION(runtime_error("The interval dates have not been set yet."));
        return {dateLeft, dateRight};
    }


    /**
     * @brief Get the resolution tag for high resolution
     *
     * @return high resolution tag
     * @see setHighResTag()
     */
    std::string const& getHighResTag() const {
        return highTag;
    }

    /**
     * @brief Set the resolution tag for high resolution
     * @param tag new high resolution tag
     *
     * This tag is used together with the interval dates to get the high resolution images from
     * `StaarchFusor::imgs`. Note, the tag for the mask images corresponding to the high resolution
     * images can be set with #setHighResMaskTag().
     *
     * @see getHighResTag(), setLowResTag(), setIntervalDates(), StaarchFusor::predict(),
     * StaarchFusor::srcImages().
     */
    void setHighResTag(std::string const& tag) {
        highTag = tag;
        // the tag in starfm options (s_opt) will be set right before prediction
    }

    /**
     * @brief Get the resolution tag for low resolution
     * @return low resolution tag
     * @see setLowResTag()
     */
    std::string const& getLowResTag() const {
        return lowTag;
    }

    /**
     * @brief Set the resolution tag for low resolution
     * @param tag new low resolution tag
     *
     * This tag is used together with the interval dates to get the low resolution images from
     * `StaarchFusor::imgs`. Note, the tag for the mask images corresponding to the low resolution
     * images can be set with #setLowResMaskTag().
     *
     * @see getLowResTag(), setHighResTag(), setIntervalDates(), StaarchFusor::predict()
     * StaarchFusor::srcImages().
     */
    void setLowResTag(std::string const& tag) {
        lowTag = tag;
        // the tag in starfm options (s_opt) will be set right before prediction
    }

    /**
     * @brief Get the resolution tag for low resolution masks
     *
     * They are used for the generation of the date of disturbance map.
     *
     * @return tag
     * @see setLowResMaskTag()
     */
    std::string const& getLowResMaskTag() const {
        return lowMaskTag;
    }

    /**
     * @brief Set the resolution tag for low resolution masks
     * @param tag new low resolution mask tag
     *
     * This tag is used together with the interval dates to get the masks for the low resolution
     * images from `StaarchFusor::imgs`. They are used for the generation of the date of
     * disturbance map.
     *
     * @see getLowResMaskTag(), StaarchFusor::generateDODImage()
     * StaarchFusor::srcImages().
     */
    void setLowResMaskTag(std::string const& tag) {
        lowMaskTag = tag;
    }

    /**
     * @brief Get the resolution tag for high resolution masks
     *
     * They are used for the generation of the date of disturbance map.
     *
     * @return tag
     * @see setHighResMaskTag()
     */
    std::string const& getHighResMaskTag() const {
        return highMaskTag;
    }

    /**
     * @brief Set the resolution tag for high resolution masks
     * @param tag new high resolution mask tag
     *
     * This tag is used together with the interval dates to get the masks for the high resolution
     * images from `StaarchFusor::imgs`. They are used for the generation of the date of
     * disturbance map.
     *
     * @see getHighResMaskTag(), StaarchFusor::generateDODImage()
     * StaarchFusor::srcImages().
     */
    void setHighResMaskTag(std::string const& tag) {
        highMaskTag = tag;
    }

    /**
     * @brief Specify the number of images to use for the moving average of disturbance index
     *
     * @param nImg is the number of images to use for the moving average to find the date of
     * disturbance
     *
     * The paper suggests to use three subsequent low res composites. This is the default here, too.
     * The average is made with the forward images, e. g. the average at date i is made from the
     * images at dates i, i+1 and i+2 for `nImg = 3`.
     */
    void setNumberImagesForAveraging(unsigned int nImg = 3) {
        n_imgs_or_days = static_cast<int>(nImg);
    }

    /**
     * @brief Get setting for the number of images to use for averaging
     * @return the current setting for the number of images.
     */
    unsigned int getNumberImagesForAveraging() const {
        return n_imgs_or_days;
    }

//    /**
//     * @brief Specify the number of days to use for moving average of disturbance index
//     *
//     * @param nDays is the number of days to use for the moving average to find the date of
//     * disturbance
//     *
//     * Using this method switches to using days instead of a fixed number of images. This might be
//     * interesting in case of missing images to avoid using images from too far away, temporally.
//     * So to get a similar behaviour instead of using 3 images of 8-day composite images, 25 days
//     * could be used.
//     *
//     * Note this requires that the dates are given in absolute number of days, so instead of using
//     * 20191231 and 20200101 as dates, you should use the day counting from some reference date,
//     * like 364 and 365 (using 2019-01-01 as reference).
//     */
//    void setNumberDaysForAveraging(unsigned int nDays) {
//        n_imgs_or_days = -static_cast<int>(nDays);
//    }

    /**
     * @brief Specify the low resolution ratio threshold of the disturbance index
     *
     * @param t is the ratio between the temporal min and max of the low resolution disturbance
     * index. The first date, when the disturbance index exceeds the specified value, is marked as
     * date of disturbance (DoD).
     *
     * This threshold is used to identify the date of disturbance as the first date that satisfies
     * \f[ \bar{DI} \ge \bar{DI_{min}} + t \cdot (\bar{DI_{max}} - \bar{DI_{min}}) \f]
     *
     * @see getLowResDIRatio()
     */
    void setLowResDIRatio(double t = 2./3.) {
        if (t <= 0 || t >= 1)
            IF_THROW_EXCEPTION(invalid_argument_error("The low resolution change threshold to detect a disturbance must be in (0, 1). You gave: " + std::to_string(t)));
        lowResDIRaio = t;
    }

    /**
     * @brief Get the current low resolution change threshold value
     * @return change threshold \f$ t \f$
     * @see setLowResThreshold()
     */
    double getLowResDIRatio() const {
        return lowResDIRaio;
    }

    /**
     * @brief Specify the high resolution disturbance index range
     *
     * @param range is the interval of the standardized disturbance index.
     *
     * If the DI at a location is within this range, the pixel might be disturbed, if also one or
     * more neighboring DI lies withing this range and brightness, greeness, wetness and NDVI at
     * the location are in specified ranges, too.
     *
     * @see getHighResDIRange(), setHighResBrightnessRange(), setHighResGreenessRange(),
     * setHighResWetnessRange(), setHighResNDVIRange()
     */
    void setHighResDIRange(Interval range = Interval::closed(2, inf)) {
        highResDIRange = range;
    }

    /**
     * @brief Get the current setting for the high resolution disturbance index range
     *
     * @return disturbance index interval in which a location might be marked as disturbed.
     *
     * @see setHighResDIRange(), setHighResBrightnessRange(), setHighResGreenessRange(),
     * setHighResWetnessRange(), setHighResNDVIRange()
     */
    Interval const& getHighResDIRange() const {
        return highResDIRange;
    }


    /**
     * @brief Specify the high resolution brightness range
     *
     * @param range is the interval for the standardized brightness.
     *
     * A brightness value lying in this range is one condition for a high resolution pixel to be
     * flagged as disturbed. There are also ranges for the disturbance index, greeness, wetness and
     * NDVI.
     *
     * @see getHighResBrightnessRange(), setHighResDIRange(), setHighResGreenessRange(),
     * setHighResWetnessRange(), setHighResNDVIRange()
     */
    void setHighResBrightnessRange(Interval range = Interval::closed(-3, inf)) {
        brightnessRange = range;
    }

    /**
     * @brief Get the current setting for the high resolution brightness range
     *
     * @return the brightness interval in which a location might be marked as disturbed.
     *
     * @see setHighResBrightnessRange(), setHighResDIRange(), setHighResGreenessRange(),
     * setHighResWetnessRange(), setHighResNDVIRange()
     */
    Interval const& getHighResBrightnessRange() const {
        return brightnessRange;
    }


    /**
     * @brief Specify the high resolution greeness range
     *
     * @param range is the interval for the standardized greeness.
     *
     * A greeness value lying in this range is one condition for a high resolution pixel to be
     * flagged as disturbed. This is not specified in the paper, but seems like an obvious
     * abstraction. There are also ranges for the disturbance index, brightness, wetness and NDVI.
     *
     * @see getHighResGreenessRange(), setHighResDIRange(), setHighResBrightnessRange(),
     * setHighResWetnessRange(), setHighResNDVIRange()
     */
    void setHighResGreenessRange(Interval range = Interval::closed(-inf, inf)) {
        greenessRange = range;
    }

    /**
     * @brief Get the current setting for the high resolution greeness range
     *
     * @return the greeness interval in which a location might be marked as disturbed.
     *
     * @see setHighResGreenessRange(), setHighResDIRange(), setHighResBrightnessRange(),
     * setHighResWetnessRange(), setHighResNDVIRange()
     */
    Interval const& getHighResGreenessRange() const {
        return greenessRange;
    }


    /**
     * @brief Specify the high resolution wetness range
     *
     * @param range is the interval for the standardized wetness.
     *
     * A wetness value lying in this range is one condition for a high resolution pixel to be
     * flagged as disturbed. There are also ranges for the disturbance index, brightness, greeness
     * and NDVI.
     *
     * @see getHighResWetnessRange(), setHighResDIRange(), setHighResBrightnessRange(),
     * setHighResGreenessRange(), setHighResNDVIRange()
     */
    void setHighResWetnessRange(Interval range = Interval::closed(-inf, -1)) {
        wetnessRange = range;
    }

    /**
     * @brief Get the current setting for the high resolution wetness range
     *
     * @return the wetness interval in which a location might be marked as disturbed.
     *
     * @see setHighResWetnessRange(), setHighResDIRange(), setHighResBrightnessRange(),
     * setHighResGreenessRange(), setHighResNDVIRange()
     */
    Interval const& getHighResWetnessRange() const {
        return wetnessRange;
    }


    /**
     * @brief Specify the high resolution NDVI range
     *
     * @param range is the interval for the standardized NDVI.
     *
     * A NDVI value lying in this range is one condition for a high resolution pixel to be flagged
     * as disturbed. There are also ranges for the disturbance index, brightness, greeness and
     * wetness.
     *
     * @see getHighResNDVIRange(), setHighResDIRange(), setHighResBrightnessRange(), setHighResGreenessRange(),
     * setHighResWetnessRange()
     */
    void setHighResNDVIRange(Interval range = Interval::closed(-inf, 0)) {
        ndviRange = range;
    }

    /**
     * @brief Get the current setting for the high resolution NDVI range
     *
     * @return the NDVI interval in which a location might be marked as disturbed.
     *
     * @see setHighResNDVIRange(), setHighResDIRange(), setHighResBrightnessRange(), setHighResGreenessRange(),
     * setHighResWetnessRange()
     */
    Interval const& getHighResNDVIRange() const {
        return ndviRange;
    }


    /**
     * @brief Shape for the neighborhood of the disturbance index
     *
     * One condition for a location to be flagged as disturbed is that its disturbance index and
     * the one of at least one neighbor lies within a specified range, see setHighResDIRange().
     * However, neighborhood can mean the four neighboring locations, which can be specified with
     * `setNeighborShape(NeighborShape::cross)`, or the eight neighboring locations with
     * `setNeighborShape(NeighborShape::square)`.
     */
    enum class NeighborShape {
        cross, ///< also called 5-star, means each pixel has got four neighbors
        square ///< also called 9-star, means each pixel has got eight neighbors
    };

    /**
     * @brief Set which neighbors are considered to check the disturbance index value
     *
     * @param s is the shape of the neighborhood, either NeighborShape::cross or
     * NeighborShape::square.
     *
     * One condition for a location to be flagged as disturbed is that its disturbance index and
     * the one of at least one neighbor lies within a specified range, see setHighResDIRange().
     * You can specify whether to use four or eight neighbors, see #NeighborShape.
     */
    void setNeighborShape(NeighborShape s = NeighborShape::cross) {
        neighborShape = s;
    }

    /**
     * @brief Get the current setting for the neighborhood setting
     * @return the neighborhood setting for the disturbance index range check.
     */
    NeighborShape getNeighborShape() const {
        return neighborShape;
    }


    /**
     * @brief Satellite sensors
     *
     * These sensor types can be used with the STAARCH algorithm. For STAARCH it is important to
     * know which sensor type is used for the low and which is used for the high resolution images
     * because of the tasseled cap transformation. The coefficients are different for each sensor
     * type. Set the sensor type with setLowResSensor() and setHighResSensor() directly or as
     * string.
     *
     * @see strToSensorType()
     */
    enum class SensorType {
        unsupported, ///< A sensor type for which no tasseled cap transformation is known
        modis,       ///< MODIS sensors
        landsat,     ///< Landsat-7 and Landsat-8 sensors
        sentinel2,   ///< Sentinel-2A and Sentinel-2B (tasseled cap transformation not implemented yet)
        sentinel3    ///< Sentinel-3A and Sentinel-3B (tasseled cap transformation not implemented yet)
    };
    
    /**
     * @brief Convert a str to a sensor type enum value
     * @param s is the string to convert, case is ignored.
     *
     * @return
     * - SensorType::modis for the string `"modis"`
     * - SensorType::landsat for the string `"landsat"`
     * - SensorType::sentinel2 for the strings `"sentinel2"`, `"sentinel-2"`, `"sentinel 2"`
     * - SensorType::sentinel3 for the strings `"sentinel3"`, `"sentinel-3"`, `"sentinel 3"`
     * - SensorType::unsupported otherwise
     */
    static SensorType strToSensorType(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        SensorType st = s == "landsat"                                             ? SensorType::landsat
                      : s == "modis"                                               ? SensorType::modis
                      : s == "sentinel2" || s == "sentinel-2" || s == "sentinel 2" ? SensorType::sentinel2
                      : s == "sentinel3" || s == "sentinel-3" || s == "sentinel 3" ? SensorType::sentinel3
                                                                                   : SensorType::unsupported;
        if (st == SensorType::unsupported)
            IF_THROW_EXCEPTION(invalid_argument_error("The sensor type " + s + " is unknown. Select one of landsat, modis, sentinel2 or sentinel3!"));
        return st;
    }

    /**
     * @brief Convert a sensor type to tasseled cap color mapping
     * @param s is the sensor type enum value to convert.
     *
     * @return
     * - ColorMapping::Modis_to_TasseledCap for SensorType::modis
     * - ColorMapping::Landsat_to_TasseledCap for SensorType::landsat
     */
    static ColorMapping sensorTypeToTasseledCap(SensorType s) {
        if (s == SensorType::unsupported || s == SensorType::sentinel2 || s == SensorType::sentinel3)
            IF_THROW_EXCEPTION(invalid_argument_error("The sensor type is not supported. Select one of Landsat, MODIS for now! Sentinel support is planned for the future."));

        // TODO: Add more sensor types when more transformations are available
        return s == StaarchOptions::SensorType::modis ? ColorMapping::Modis_to_TasseledCap
                                                      : ColorMapping::Landsat_to_TasseledCap;
    }


    /**
     * @brief Specify the low spatial resolution sensor
     * @param s is the sensor (satellite) to use for low resolution
     *
     * The information about the sensor is required for the tasseled cap transformation, which is
     * different for each sensor.
     *
     * @see getLowResSensor(), setHighResSensor()
     */
    void setLowResSensor(SensorType s = SensorType::unsupported) {
        lowSensor = s;
    }

    /// \copydoc setLowResSensor()
    void setLowResSensor(std::string const& s) {
        lowSensor = strToSensorType(s);
    }

    /**
     * @brief Get the current setting for the low spatial resolution sensor
     * @return low spatial resolution sensor type
     */
    SensorType getLowResSensor() const {
        return lowSensor;
    }


    /**
     * @brief Specify the high spatial resolution sensor
     * @param s is the sensor (satellite) to use for high resolution
     *
     * The information about the sensor is required for the tasseled cap transformation, which is
     * different for each sensor.
     *
     *
     * @see getHighResSensor(), setLowResSensor()
     */
    void setHighResSensor(SensorType s = SensorType::unsupported) {
        highSensor = s;
    }

    /// \copydoc setHighResSensor()
    void setHighResSensor(std::string const& s) {
        highSensor = strToSensorType(s);
    }

    /**
     * @brief Get the current setting for the high spatial resolution sensor
     * @return high spatial resolution sensor type
     */
    SensorType getHighResSensor() const {
        return highSensor;
    }


    /**
     * @brief Override the default channel order for the low resolution images
     *
     * @param srcChans is the order of the channels that are required for the tasseled cap
     * transformation. The required channels depend on the sensor type. Use an empty vector for
     * saying that the default order should be used for the sensor type.
     *
     * Specify the order of the required channels like in the following example. Assuming you use
     * MODIS images, the order for the channels for the tasseled cap transformation should be:
     *
     * Channel:   | 0       | 1       | 2       | 3       | 4         | 5         | 6
     * :--------: | :-----: | :-----: | :-----: | :-----: | :-------: | :-------: | :-------:
     * Name       | Red     | Near-IR | Blue    | Green   | M-IR 1    | M-IR 2    | M-IR 3
     * Wavelength | 620-670 | 841-876 | 459-479 | 545-565 | 1230-1250 | 1628-1652 | 2105-2155
     *
     * Now assume your images have channels 1 and 3 swapped:
     *
     * Channel:   | 0       | 1       | 2       | 3       | 4         | 5         | 6
     * :--------: | :-----: | :-----: | :-----: | :-----: | :-------: | :-------: | :-------:
     * Name       | Red     | Green   | Blue    | Near-IR | M-IR 1    | M-IR 2    | M-IR 3
     * Wavelength | 620-670 | 545-565 | 459-479 | 841-876 | 1230-1250 | 1628-1652 | 2105-2155
     *
     * Then you can fix this with:
     * @code
     * // specify the channel indexes for Red, Near-IR, Blue, Green, M-IR 1, M-IR 2, M-IR 3
     * staarchOpt.setLowResSourceChannels({0, 3, 2, 1, 4, 5, 6});
     * @endcode
     *
     * This can also be useful, if your images have more channels than required for the tasseled
     * cap transformation. Then you still specify here the channel indexes for the required
     * channels.
     *
     * @see getLowResSourceChannels, setHighResSourceChannels
     */
    void setLowResSourceChannels(std::vector<unsigned int> srcChans) {
        lowSrcChannels = std::move(srcChans);
    }

    /**
     * @brief Get the current channel order setting for the low resolution images
     *
     * @return the current setting, but note that an empty vector means that the default setting
     * for the respective sensor will be used
     *
     * @see setLowResSourceChannels
     */
    std::vector<unsigned int> const& getLowResSourceChannels() const {
        return lowSrcChannels;
    }


    /**
     * @brief Override the default channel order for the high resolution images
     *
     * @param srcChans is the order of the channels that are required for the tasseled cap and NDVI
     * transformation. The required channels depend on the sensor type. Use an empty vector for
     * saying that the default order should be used for the sensor type.
     *
     * Specify the order of the required channels like in the following example. Assuming you use
     * Landsat 8 images, the order for the channels for the tasseled cap transformation should be:
     *
     * Channel:   | 0       | 1       | 2       | 3       | 4         | 5
     * :--------: | :-----: | :-----: | :-----: | :-----: | :-------: | :-------:
     * Name       |  Blue   | Green   | Red     | NIR     | SWIR1     | SWIR2
     * Wavelength | 450–520 | 520–600 | 630–690 | 770–900 | 1550–1750 | 2080–2450

     * Now assume your images have the following channel order:
     *
     * Channel:   | 0       | 1       | 2       | 3         | 4       | 5         | 6
     * :--------: | :-----: | :-----: | :-----: | :-------: | :-----: | :-------: | :-------:
     * Name       | Red     | Green   | Blue    | Deep-Blue | NIR     | SWIR2     | SWIR1
     * Wavelength | 630–690 | 520–600 | 450–520 | 430 – 450 | 770–900 | 2080–2450 | 1550–1750
     *
     * Then you can fix this with:
     * @code
     * // specify the channel indexes for  Blue, Green, Red, NIR, SWIR1, SWIR2
     * staarchOpt.setHighResSourceChannels({2, 1, 0, 4, 6, 5});
     * @endcode
     *
     * This can also be useful, if your images have more channels than required for the tasseled
     * cap transformation. Then you still specify here the channel indexes for the required
     * channels.
     *
     * @see getHighResSourceChannels, setLowResSourceChannels
     */
    void setHighResSourceChannels(std::vector<unsigned int> srcChans) {
        highSrcChannels = std::move(srcChans);
    }

    /**
     * @brief Get the current channel order setting for the high resolution images
     *
     * @return the current setting, but note that an empty vector means that the default setting
     * for the respective sensor will be used
     *
     * @see setHighResSourceChannels
     */
    std::vector<unsigned int> const& getHighResSourceChannels() const {
        return highSrcChannels;
    }


    /**
     * @brief Set the bands that will be fused
     *
     * @param bands are the band names used for fusion, default is `{"red", "green", "blue"}`.
     * Allowed bands are:
     * - `"red"`
     * - `"nir"`
     * - `"blue"`
     * - `"green"`
     * - `"swir1"`
     * - `"swir2"`
     *
     * For STAARCH the high and low resolution images usually have a different number of channels
     * (depending on the sensor type). E. g. MODIS images need 7 channels, while Landsat images
     * need 6 channels, see ColorMapping::Modis_to_TasseledCap and
     * ColorMapping::Landsat_to_TasseledCap, respectively. However, these are mainly used for the
     * date of disturbance image. The prdiction is done with STARFM and STARFM requires images with
     * the same channels. A common set of channels is extracted as specified by here before fusion.
     * The predicted images will have the channels, that are specified here.
     */
    void setOutputBands(std::vector<std::string> bands = {"red", "green", "blue"}) {
        if (bands.empty())
            IF_THROW_EXCEPTION(invalid_argument_error("The output bands argument you provided is empty. If you only want to generate the date of disturbance image instead of predicting, the output bands are not used anyway."));

        std::set<std::string> known_common_bands = {"red", "nir", "blue", "green", "swir1", "swir2"};
        for (std::string& b : bands) {
            std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return std::tolower(c); });
            if (known_common_bands.count(b) == 0) // when switching to C++20, replace this by !known_common_bands.contains(b)
                IF_THROW_EXCEPTION(invalid_argument_error("You requested an unknown band name as output: " + b + ". The known bands are: red, nir, blue, green, swir1, swir2"));
        }
        outputBands = std::move(bands);
    }

    /**
     * @brief Get the current setting for the bands, that will be fused
     *
     * @return the vector of strings for the bands that will be fused and put out as predicted
     * image
     */
    std::vector<std::string> const& getOutputBands() const {
        return outputBands;
    }

//#ifdef _OPENMP
//    /**
//     * @brief Set the number of threads to use
//     *
//     * @param t is the number of threads &le; number of processors. Choosing it greater than
//     * [`omp_get_num_procs()`](https://gcc.gnu.org/onlinedocs/libgomp/omp_005fget_005fnum_005fprocs.html)
//     * will set it to `omp_get_num_procs()`.
//     *
//     * By default (on construction) this is set to `omp_get_num_procs()`.
//     */
//    void setNumberThreads(unsigned int t) {
//        threads = std::min((int)t, std::max(omp_get_num_procs(), 1));
//    }

//    /**
//     * @brief Get the number of threads used for parallelization
//     * @return number of threads
//     * @see setNumberThreads()
//     */
//    unsigned int getNumberThreads() const {
//        return threads;
//    }
//#endif /* _OPENMP*/

protected:
    /**
     * @brief Starfm Options
     *
     * This holds all STARFM options except resolution tags and dates. Those will be set right
     * before prediction
     */
    StarfmOptions s_opt;

    /// Left interval date
    int dateLeft;

    /// Right interval date
    int dateRight;

    /// Helper for interval dates
    bool areDatesSet = false;


    /// High resolution tag
    std::string highTag;

    /// Low resolution tag
    std::string lowTag;

    /// High resolution mask tag, see StaarchFusor::srcImages()
    std::string highMaskTag;

    /// Low resolution mask tag, see StaarchFusor::srcImages()
    std::string lowMaskTag;


    /// Alignment of the moving average filter
    MovingAverageWindow avgWin = MovingAverageWindow::forward;

    /// Number of images used for averaging
    int n_imgs_or_days = 3; // maybe in future: positive value means n_imgs, negative value means n_days

    /// Number of clusters for land classification
    unsigned int clusters = 10;

    /// Preclustered image instead of k-means
    Image clusterImage;

    /// Number of neighbors for disturbance detection
    NeighborShape neighborShape = NeighborShape::cross;


    /// Relative threshold for low resolution disturbance index
    double lowResDIRaio = 2./3.;

    /// Range for high resolution disturbance index
    Interval highResDIRange  = Interval::closed(2, inf); // TODO: open and closed intervals are both closed for floating point for now, we actually like to have open

    /// Range for high resolution brightness (from tasseled cap transformation)
    Interval brightnessRange = Interval::closed(-3, inf);

    /// Range for high resolution greeness (from tasseled cap transformation)
    Interval greenessRange   = Interval::closed(-inf, inf);

    /// Range for high resolution wetness (from tasseled cap transformation)
    Interval wetnessRange    = Interval::closed(-inf, -1);

    /// Range for high resolution NDVI (from tasseled cap transformation)
    Interval ndviRange       = Interval::closed(-inf, 0);


    /// Low resolution sensor type
    SensorType lowSensor = SensorType::unsupported;

    /// High resolution sensor type
    SensorType highSensor = SensorType::unsupported;

    /// Low resolution channel numbers
    std::vector<unsigned int> lowSrcChannels;

    /// High resolution channel numbers
    std::vector<unsigned int> highSrcChannels;

    /// Output bands used for fusion
    std::vector<std::string> outputBands = {"red", "green", "blue"};

//#ifdef _OPENMP
//    unsigned int threads = omp_get_num_procs();
//#endif /* _OPENMP*/

    friend class StaarchFusor;

private:
    static constexpr float inf = std::numeric_limits<float>::infinity();
};

} /* namespace imagefusion */

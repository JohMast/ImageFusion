#pragma once

#include "options.h"
#include "exceptions.h"

namespace imagefusion {

class StarfmOptions : public Options {
public:
    /**
     * @brief States for temporal weighting setting
     *
     * This can be used for setUseTempDiffForWeights(). It decides whether to use the temporal
     * difference for weighting of the candidates.
     */
    enum class TempDiffWeighting {
        on_double_pair, ///< If using double pair mode, use temporal difference and if using single pair mode do not.
                        ///< This is the default setting and also the behaviour of the reference implementation.
        enable,         ///< Use temporal difference, i. e. \f$ C = (S + 1) \, (T + 1) \, D \f$
        disable         ///< Do not use temporal difference, i. e. \f$ C = (S + 1) \, D \f$
    };


    /**
     * @brief Configure for single image pair mode and set the pair date
     *
     * @param pairDate of the input image pair. These two images will be used for the spectral
     * difference and their low resolution image also for the temporal difference. See also \ref
     * starfm_3_image_structure "STARFM image structure with three images".
     *
     * @see getSinglePairDate(), setDoublePairDates(), setLowResTag(), setHighResTag()
     */
    void setSinglePairDate(int pairDate) {
        date1 = pairDate;
        isDate1Set = true;
        isDate3Set = false;
    }

    /**
     * @brief Get the date of the input pair when using single pair mode
     * @throws runtime_error if it is not configured for single pair mode.
     * @returns the current input pair date.
     * @see setSinglePairDate()
     */
    int getSinglePairDate() const {
        if (!isDate1Set)
            IF_THROW_EXCEPTION(runtime_error("No date has been set yet."));

        if (isDate3Set)
            IF_THROW_EXCEPTION(runtime_error("Options are configured for double pair mode."));

        return date1;
    }

    /**
     * @brief Check whether single pair mode has been selected
     *
     * @return true if setSinglePairDate() has been called and setDoublePairDates() has not been
     * called after that.
     */
    bool isSinglePairModeConfigured() const {
        return isDate1Set && !isDate3Set;
    }

    /**
     * @brief Configure for double image pair mode and set both pair dates
     *
     * @param d1 is date 1
     * @param d3 is date 3
     *
     * The dates specify both pair dates for the double pair mode. Each pair will be used on its
     * own with the prediction date and not with each other. So the two images of a pair will be
     * used for the spectral difference and their low resolution image also for the temporal
     * difference to the prediction date. See also \ref starfm_5_image_structure
     * "STARFM image structure with five images".
     *
     * @see getDoublePairDates(), setSinglePairDate(), setLowResTag(), setHighResTag()
     */
    void setDoublePairDates(int d1, int d3) {
        if (d1 == d3)
            IF_THROW_EXCEPTION(invalid_argument_error("When using double pair mode for STARFM, you have to provide two different dates. You gave date " + std::to_string(d1) + " for both pairs."));

        date1 = d1;
        date3 = d3;
        isDate1Set = true;
        isDate3Set = true;
    }

    /**
     * @brief Get the dates of the both input pairs when using double pair mode
     * @throws runtime_error if it is not configured for double pair mode.
     * @returns pair with date 1 as first number and date 3 as second.
     * @see setDoublePairDates()
     */
    std::pair<int, int> getDoublePairDates() const {
        if (!isDate1Set)
            IF_THROW_EXCEPTION(runtime_error("No date has been set yet."));

        if (!isDate3Set)
            IF_THROW_EXCEPTION(runtime_error("Options are configured for single pair mode."));
        return {date1, date3};
    }

    /**
     * @brief Check whether double pair mode has been selected
     *
     * @return true if setDoublePairDates() has been called and setSinglePairDate() has not been
     * called after that.
     */
    bool isDoublePairModeConfigured() const {
        return isDate1Set && isDate3Set;
    }

    /**
     * @brief Set the window size in which will be searched for similar pixels
     *
     * @param size for the search window. Must be an odd number. Example: The default value of 51
     * means 25 pixel up and down, or left and right, plus the center pixel.
     *
     * @throws invalid_argument_error if size is even.
     * @see getWinSize()
     */
    void setWinSize(unsigned int size = 51) {
        if (size % 2 == 0)
            IF_THROW_EXCEPTION(invalid_argument_error("The window size must be an odd number. You tried " + std::to_string(size)));
        winSize = size;
    }

    /**
     * @brief Get the window size in which is searched for similar pixels
     * @return The window size in pixel.
     * @see setWinSize()
     */
    unsigned int getWinSize() const {
        return winSize;
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
     * @see getNumberClasses()
     */
    void setNumberClasses(double classes = 40) {
        numClasses = classes;
    }

    /**
     * @brief Get the number of classes
     * @return number of classes.
     * @see setNumberClasses()
     */
    double getNumberClasses() const {
        return numClasses;
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
     * @throws invalid_argument_error if sigmaT < 0.
     * @see getTemporalUncertainty(), setSpectralUncertainty()
     */
    void setTemporalUncertainty(double sigmaT = 1) {
        if (sigmaT < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("The temporal uncertainty must be a non-negative number. You tried " + std::to_string(sigmaT)));
        sigma_t = sigmaT;
    }

    /**
     * @brief Get temporal uncertainty
     * @return temporal uncertainty
     * @see setTemporalUncertainty()
     */
    double getTemporalUncertainty() const {
        return sigma_t;
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
     * @throws invalid_argument_error if sigmaS < 0.
     * @see getSpectralUncertainty(), setTemporalUncertainty()
     */
    void setSpectralUncertainty(double sigmaS = 1) {
        if (sigmaS < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("The spectral uncertainty must be a non-negative number. You tried " + std::to_string(sigmaS)));
        sigma_s = sigmaS;
    }

    /**
     * @brief Get spectral uncertainty
     * @return spectral uncertainty
     * @see setSpectralUncertainty()
     */
    double getSpectralUncertainty() const {
        return sigma_s;
    }

    /**
     * @brief Get the resolution tag for high resolution
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
     * This tag is used together with the input pair date to get the high resolution images from
     * `StarfmFusor::imgs`.
     *
     * @see getHighResTag(), setLowResTag(), setSinglePairDate(), setDoublePairDates(),
     * StarfmFusor::predict()
     */
    void setHighResTag(std::string const& tag) {
        highTag = tag;
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
     * This tag is used together with the input pair date and the prediction date to get the low
     * resolution images from `StarfmFusor::imgs`.
     *
     * @see getLowResTag(), setHighResTag(), setSinglePairDate(), setDoublePairDates(),
     * StarfmFusor::predict()
     */
    void setLowResTag(std::string const& tag) {
        lowTag = tag;
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
     * @see getUseStrictFiltering()
     */
    void setUseStrictFiltering(bool strict = false) {
        useStrict = strict;
    }

    /**
     * @brief Get setting whether to use strict filtering
     * @return current setting
     * @see setUseStrictFiltering() for a detailed description
     */
    bool getUseStrictFiltering() const {
        return useStrict;
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
     * @see getDoCopyOnZeroDiff()
     */
    void setDoCopyOnZeroDiff(bool copy = false) {
        doCopyOnZeroDiff = copy;
    }

    /**
     * @brief Get setting whether to copy values on zero spectral or temporal difference
     * @return current setting
     * @see setDoCopyOnZeroDiff() for a detailed description
     */
    bool getDoCopyOnZeroDiff() const {
        return doCopyOnZeroDiff;
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
     * @see getUseTempDiffForWeights(), #TempDiffWeighting
     */
    void setUseTempDiffForWeights(TempDiffWeighting use = TempDiffWeighting::enable) {
        useTempDiff = use;
    }

    /**
     * @brief Get setting whether to use the temporal difference for weighting
     * @return current setting
     * @see setUseTempDiffForWeights() for a detailed description
     */
    TempDiffWeighting getUseTempDiffForWeights() const {
        return useTempDiff;
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
     * @see getLogScaleFactor()
     */
    void setLogScaleFactor(double b = 0) {
        if (b < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("Starfm logarithmic scale factor cannot be negative."));
        logScale = b;
    }

    /**
     * @brief Get and enable/disable the logarithmic scale factor in weighting
     * @return the factor
     * @see setLogScaleFactor()
     */
    double getLogScaleFactor() const {
        return logScale;
    }


protected:
    bool isDate1Set = false;
    bool isDate3Set = false;
    int date1;
    int date3;

    unsigned int winSize = 51;
    double numClasses = 40;
    double sigma_t = 1; // uncertainty of coarse resolution pixels
    double sigma_s = 1; // uncertainty of fine resolution pixels
    std::string highTag;
    std::string lowTag;

    bool useStrict = false;
    bool doCopyOnZeroDiff = false;
    TempDiffWeighting useTempDiff = TempDiffWeighting::enable;
    double logScale = 0;

    friend class StarfmFusor;
};

} /* namespace imagefusion */

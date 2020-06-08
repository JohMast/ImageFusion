#pragma once

#include "Options.h"

namespace imagefusion {

class EstarfmOptions : public Options {
public:
    /**
     * @brief Get the date of the first input image pair
     * @return date1
     * @see setDate1()
     */
    int getDate1() const {
        if (!isDate1Set)
            IF_THROW_EXCEPTION(runtime_error("The date of the first input pair (date1) has not been set yet."));
        return date1;
    }

    /**
     * @brief Set the date of the first input image pair in `EstarfmFusor::imgs`
     * @param date1 is the date of the first input image pair in `EstarfmFusor::imgs`.
     *
     * This date is used together with the resolution tags to get the images from
     * `EstarfmFusor::imgs`. The notation is like shown in Table
     * \ref estarfm_image_structure "ESTARFM image structure".
     *
     * @see getDate1(), setDate3(), setHighResTag(), setLowResTag()
     */
    void setDate1(int date1) {
        isDate1Set = true;
        this->date1 = date1;
    }

    /**
     * @brief Get the date of the last input image pair
     * @return date3
     * @see setDate3()
     */
    int getDate3() const {
        if (!isDate3Set)
            IF_THROW_EXCEPTION(runtime_error("The date of the second input pair (date3) has not been set yet."));
        return date3;
    }

    /**
     * @brief Set the date of the last input image pair in `EstarfmFusor::imgs`
     * @param date3 is the date of the last input image pair in `EstarfmFusor::imgs`
     *
     * This date is used together with the resolution tags to get the images from
     * `EstarfmFusor::imgs`. The notation is like shown in Table
     * \ref estarfm_image_structure "ESTARFM image structure".
     *
     * @see getDate3(), setDate1(), setHighResTag(), setLowResTag()
     */
    void setDate3(int date3) {
        isDate3Set = true;
        this->date3 = date3;
    }


    /**
     * @brief Set the window size in which is searched for similar pixels
     * @param size for the search window. Must be an odd number.
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
     * @see getWinSize()
     */
    unsigned int getWinSize() const {
        return winSize;
    }

    /**
     * @brief Set the number of classes to influence similarity tolerance
     *
     * @param classes denoted by \f$ n \f$ in the following, is the value by which the doubled
     * standard deviation \f$ s \f$ is divided to set the tolerance for similarity, i. e. \f$
     * \emph{tol} = 2 \, \frac s n \f$. Note, for similarity a location (x, y) has to satisfy the
     * condition \f$ |h(x_c, y_c) - h(x, y) | \le \emph{tol} \f$ for all channels and dates, where
     * \f$ (x_c, y_c) \f$ is the center location of a window.
     *
     * @see getNumberClasses()
     */
    void setNumberClasses(double classes = 4) {
        if (classes < 1)
            IF_THROW_EXCEPTION(invalid_argument_error("The number of classes must be set to a value greater or equal to 1. You tried " + std::to_string(classes)));
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
     * @brief Get the resolution tag for high resolution
     * @return tag
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
     * `EstarfmFusor::imgs`.
     *
     * @see getHighResTag(), setLowResTag(),  setPairDate()
     */
    void setHighResTag(std::string const& tag) {
        highTag = tag;
    }

    /**
     * @brief Get the resolution tag for low resolution
     * @return tag
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
     * resolution images from `EstarfmFusor::imgs`.
     *
     * @see getLowResTag(), setHighResTag(),  setPairDate(),
     * EstarfmFusor::predict()
     */
    void setLowResTag(std::string const& tag) {
        lowTag = tag;
    }

    /**
     * @brief Set the valid data range to limit predicted values
     * @param min is the lower bound of the data range
     * @param max is the upper bound of the data range
     *
     * When predicting pixel values ESTARFM can exceed the values that appear in the image. To
     * prevent from writing invalid values (out of a known data range) you can set bounds. When not
     * setting this data range, the value range will be limited by the natural data range (e. g.
     * -32768 and 32767 for 16 bit signed integer).
     */
    void setDataRange(double min, double max) {
        isRangeSet = true;
        rangeMin = min;
        rangeMax = max;
    }

    /**
     * @brief Get lower bound of data range set by user
     * @return lower bound, as set by setDataRange()
     * @throws runtime_error if data range is not yet set. Check with isDataRangeSet() before.
     * @see setDataRange(), getDataRangeMax(), isDataRangeSet()
     */
    double getDataRangeMin() const {
        if (!isRangeSet)
            IF_THROW_EXCEPTION(runtime_error("Data range has not been set. Cannot return lower bound."));
        return rangeMin;
    }

    /**
     * @brief Get upper bound of data range set by user
     * @return upper bound, as set by setDataRange()
     * @throws runtime_error if data range is not yet set. Check with isDataRangeSet() before.
     * @see setDataRange(), getDataRangeMin(), isDataRangeSet()
     */
    double getDataRangeMax() const {
        if (!isRangeSet)
            IF_THROW_EXCEPTION(runtime_error("Data range has not been set. Cannot return upper bound."));
        return rangeMax;
    }

    /**
     * @brief Check whether data range has been set
     * @return true if this object has a data range, false otherwise
     * @see setDataRange(), getDataRangeMin(), getDataRangeMax()
     */
    bool isDataRangeSet() const {
        return isRangeSet;
    }

    /**
     * @brief Use local tolerance to find similar pixels
     * @param use
     *
     * So when searching similar pixels, a tolerance of
     * \f[ \sigma \, \frac 2 m \f]
     * is used (eq (13) in the paper). This options sets whether \f$ \sigma \f$ is calculated only
     * from the local window region around the central pixel or from the whole image.
     *
     * @see getUseLocalTol();
     */
    void setUseLocalTol(bool use = false) {
        useLocalTol = use;
    }

    /**
     * @brief Get current setting whether local tolerance will be used
     * @return current setting
     *
     * @see setUseLocalTol();
     */
    bool getUseLocalTol() const {
        return useLocalTol;
    }

    /**
     * @brief Use the regression quality to smoothly weight the regression coefficient
     * @param use
     *
     * When the regression is used to weight the candidates, a quality parameter \f$ q \in [0,1]
     * \f$ is available. The reference implementation makes a hard cut here. If q < 95%, they do
     * not use the regression coefficient r at all (by using 1) and otherwise they use it without
     * compromises. This option will weight it linearly agains 1, like
     * \f[ r \, q + (1 - q) \f]
     *
     * @see getUseQualityWeightedRegression();
     */
    void setUseQualityWeightedRegression(bool use = false) {
        useRegressionQuality = use;
    }

    /**
     * @brief Get current setting whether the regression quality should be used to smoothly weight the regression coefficient
     * @return current setting
     *
     * @see setUseQualityWeightedRegression();
     */
    bool getUseQualityWeightedRegression() const {
        return useRegressionQuality;
    }

    /**
     * @brief Set the uncertainty factor with respect to the data range maximum
     *
     * @param f is the factor with which the maximum of the data range is multiplied, if the data
     * range is set. Also a factor \f$ \sqrt 2 \f$ is added. So, for example with a data range of
     * [0, 10000] and an uncertainty factor of 0.2%, the uncertainty will be 28.2843.
     *
     * The uncertainty is for the calculation of the conversion coefficient V. V is only calculated
     * if \f$ \sigma_L > u \f$, where \f$ \sigma_L \f$ is the standard deviation of the low
     * resolution candidates and \f$ u \f$ is the uncertainty (28.2843 in the example above). If V
     * is not calculated it is set to 1.
     *
     * @see getUncertaintyFactor()
     */
    void setUncertaintyFactor(double f = 0.002) {
        uncertainty = f;
    }

    /**
     * @brief Get the uncertainty factor
     * @return the factor
     * @see getUncertaintyFactor()
     */
    double getUncertaintyFactor() const {
        return uncertainty;
    }

protected:
    bool isDate1Set = false;
    bool isDate3Set = false;
    int date1;
    int date3;

    bool isRangeSet = false;
    double rangeMin;
    double rangeMax;

    bool useLocalTol = false;
    bool useRegressionQuality = false;

    double uncertainty = 0.002;

    unsigned int winSize = 51;
    double numClasses = 4;
    std::string highTag;
    std::string lowTag;

    friend class EstarfmFusor;
};

}

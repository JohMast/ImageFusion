#pragma once

#include "options.h"
#include "exceptions.h"

#ifdef _OPENMP
    #include <omp.h>
#endif /* _OPENMP*/

namespace imagefusion {

class FitFCOptions : public Options {
public:

    /**
     * @brief Set the pair date
     *
     * @param pairDate of the input image pair. See also
     * \ref fitfc_3_image_structure "Fit-FC image structure with three images".
     *
     * @see getPairDate()
     */
    void setPairDate(int pairDate) {
        date1 = pairDate;
        isDate1Set = true;
    }

    /**
     * @brief Get the date of the input pair
     * @returns the current input pair date.
     * @throws runtime_error if it has not been set yet.
     * @see setPairDate()
     */
    int getPairDate() const {
        if (!isDate1Set)
            IF_THROW_EXCEPTION(runtime_error("The date of the input pair has not been set yet."));
        return date1;
    }

    /**
     * @brief Set the window size in which will be searched for similar pixels
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
     * @see setWinSize()
     */
    unsigned int getWinSize() const {
        return winSize;
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
     * `FitFCFusor::imgs`.
     *
     * @see getHighResTag(), setLowResTag(), setPairDate(), FitFCFusor::predict()
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
     * resolution images from `FitFCFusor::imgs`.
     *
     * @see getLowResTag(), setHighResTag(), setPairDate(), FitFCFusor::predict()
     */
    void setLowResTag(std::string const& tag) {
        lowTag = tag;
    }

    /**
     * @brief Set the number of neighbors used for correlation
     * @param n is the number of near pixels
     *
     * The pixel locations are selected from the high resolution image only inside the window. The
     * central pixel is compared to all pixels in the window over all channels with
     * \f[ D(x, y) := \frac 1 2 \sqrt{\sum_{b=1}^{n_b} \left( h_1(x, y, b) - h_1(x_c, y_c, b) \right)^2}
     *     \quad \forall x, y. \f]
     * The `n` best pixels will be selected. Then these locations are used to collect the distance
     * weights, the regression model pixels from \f$ \hat F_{\mathrm{RM}} \f$ and the bicubic
     * interpolated residuals from \f$ r \f$.
     *
     * @see getNumberNeighbors()
     */
    void setNumberNeighbors(unsigned int n = 10) {
        neighbors = n;
    }

    /**
     * @brief Get the number of neighbors used for correlation
     * @return n
     * @see setNumberNeighbors()
     */
    unsigned int getNumberNeighbors() const {
        return neighbors;
    }


    /**
     * @brief Set the scale factor from low resolution to high resolution
     * @param f is the factor.
     *
     * Note, the low resolution images and high resolution images must have the same size and
     * resolution, when using FitFCFusor. So the low resolution must be upscaled before. FitFCFusor
     * will use this factor only to downscale (using averaging) and upscale (using bicubic
     * interpolation) the residuals to get a bicubic behavior. To disable this filtering step just
     * set the factor to 1.
     *
     * @see getResolutionFactor()
     */
    void setResolutionFactor(double f = 30) {
        if (f <= 0)
            IF_THROW_EXCEPTION(invalid_argument_error("The Resolution factor must be a positive number. You tried " + std::to_string(f)));
        blocksize = f;
    }

    /**
     * @brief Get the scale factor from low resolution to high resolution
     * @return scaling factor.
     * @see setResolutionFactor()
     */
    double getResolutionFactor() const {
        return blocksize;
    }


#ifdef _OPENMP
    /**
     * @brief Set the number of threads to use
     *
     * @param t is the number of threads &le; number of processors. Choosing it greater than
     * [`omp_get_num_procs()`](https://gcc.gnu.org/onlinedocs/libgomp/omp_005fget_005fnum_005fprocs.html)
     * will set it to `omp_get_num_procs()`.
     *
     * By default (on construction) this is set to `omp_get_num_procs()`.
     */
    void setNumberThreads(unsigned int t) {
        threads = std::min((int)t, std::max(omp_get_num_procs(), 1));
    }

    /**
     * @brief Get the number of threads used for parallelization
     * @return number of threads
     * @see setNumberThreads()
     */
    unsigned int getNumberThreads() const {
        return threads;
    }
#endif /* _OPENMP*/

protected:
    int date1;
    bool isDate1Set = false;

    unsigned int winSize = 51;
    std::string highTag;
    std::string lowTag;

    unsigned int neighbors = 10;

    double blocksize = 30;

#ifdef _OPENMP
    unsigned int threads = omp_get_num_procs();
#endif /* _OPENMP*/

    friend class FitFCFusor;
};

} /* namespace imagefusion */

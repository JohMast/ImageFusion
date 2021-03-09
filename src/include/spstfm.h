#pragma once

#include "datafusor.h"
#include "multiresimages.h"
#include "imagefusion.h"
#include "type.h"
#include "image.h"
#include "exceptions.h"

#ifdef _OPENMP
    #include "parallelizer.h"
#endif /* _OPENMP */

#include <armadillo>

namespace imagefusion {

/**
 * @brief Options to control the SPSTFM algorithm (SpstfmFusor).
 *
 * This provides a lot of parameters that change the behaviour of SpstfmFusor. In addition there
 * are also options to make the SpstfmFusor use a previously learned dictionary or to improve it.
 * All options, except a debug option, are set and read only with setter and getter functions. The
 * defaults are given as default argument.
 *
 * The default options should perform well, but for specific applications other options might be
 * superior. As an example: If the predicted product for a specific kind of images is not expected
 * to improve by training, the training can be switched of with
 * @code
 * opt.setMaxTrainIter(0);
 * opt.setMinTrainIter(0);
 * @endcode
 * There are a lot of other application cases where it might be useful to vary options.
 */
class SpstfmOptions : public Options {
public:

    /**
     * @brief Simple structure to set the GPSR options easier.
     *
     * The GPSROptions struct is a very simple structure that includes some important options to
     * control the GPSR algorithms used by the SPSTFM algorithm. Basically there are two places
     * where it is used:
     *  * during the training stage to find the initial sparse representation coefficients for the
     *    dictionary update with K-SVD and to estimate the current dictionary quality with a test
     *    set. The parameters for this GPSR usage can be set with `setGPSRReconstructionOptions()`.
     *  * after training during reconstruction to find the sparse representation coefficients to
     *    predict the high resolution difference patch. The GPSR parameters of this stage can be
     *    set with `setGPSRReconstructionOptions()`
     */
    struct GPSROptions {
        /**
         * @brief Tolerance for main loop of GPSR. (default: 1e-5)
         *
         * This tolerance makes the main loop of the GPSR algorithm stop, when the relative change
         * in the objective function becomes less than `tolA`, i.e.
         * \f[ \frac{F(\lambda_k) - F(\lambda_{k-1})}{F(\lambda_{k-1})} < \epsilon, \f]
         * where \f$ \epsilon \f$ is `tolA`,
         *       \f$ F \f$ is the objective function and
         *       \f$ \lambda_k \f$ is the sparse representation coefficient vector at iteration \f$ k \f$.
         * Since the objective function optimizes not only the value itself, but also the \f$ L_1
         * \f$-norm of the coefficients, a smaller value for the tolerance will result in a sparser
         * solution. But this only holds if the number of iterations will not hit the limit
         * `maxIterA`.
         */
        double tolA = 1e-5;

        /**
         * @brief Minimum number of iterations for main loop of GPSR. (default: 5)
         *
         * So even if the relative change in the objection function is less than `tolA`, this
         * number of iterations is done, except the change is zero.
         */
        unsigned int minIterA = 5;

        /**
         * @brief Maximum number of iterations for main loop of GPSR. (default: 5000)
         *
         * This is the maximum number of iterations done by the main loop of the GPSR algorithm,
         * even if the relative change of the objection function is still greater than `tolA`.
         */
        unsigned int maxIterA = 5000;


        /**
         * @brief Turn on or off debiasing after main loop of GPSR. (default: true)
         *
         * Debiasing is the second big loop of GPSR. When the main loop stops, the found sparse
         * representation is optimized also for a low number of coefficients. However, this
         * prevents to optimize the value more exactly, or in other words: The coefficients are
         * biased by their \f$ L_1 \f$-norm. This switch decides whether or not to optimize the
         * non-zero entries of the representation for the value without considering their norm
         * (after the main loop). This is performed with a modified CG algorithm.
         */
        bool debias = true;

        /**
         * @brief Tolerance for the debias loop of GPSR. (default: 1e-1)
         *
         * A too strict (low) tolerance might increase noise in noisy situations. A too loose
         * (high) tolerance might be inaccurate.
         */
        double tolD = 1e-1;

        /**
         * @brief Minimum number of iterations for the debias loop. (default: 1)
         */
        unsigned int minIterD = 1;

        /**
         * @brief Maximum number of iterations for the debias loop. (default: 200)
         */
        unsigned int maxIterD = 200;

        /**
         * @brief Break up the main loop in multiple optimizations. (default: true)
         *
         * This starts the main loop with a larger `tau` (times 2) and `tolA` (times 10) and uses
         * the found solution as initialization for the next main loop run. In the second run `tau`
         * is decreased. This goes on until the original `tau` is reached where also the original
         * `tolA` is used.
         *
         * The purpose of this is speed up.
         */
        bool continuation = true;

        /**
         * @brief Weighting for the coefficients in the objective function. (default: -1, see below)
         *
         * The GPSR optimizes for
         * \f[ \min_{\lambda} \frac 1 2 \| p - D \, \lambda \|_2^2 + \tau \| \lambda \|_1, \f]
         * where \f$ p \f$ is the vector to match by finding coefficients \f$ \lambda \f$ for
         * dictionary \f$ D \f$. So a larger `tau` makes the sparsity of the coefficients more
         * important and thus results in a sparser solution. However with a fixed tolerance this
         * also means that more iterations are required. The number of iterations in the main loop
         * is limited by `maxIterA`.
         *
         * If a negative `tau` is given (default), \f$ \tau = 0.1 \| D^\top \, p \|_\infty \f$ is
         * used, as suggested in the respective paper.
         */
        double tau = -1;

        /**
         * @brief Make the default GPSROptions for reconstruction stage
         *
         * This is used as default argument for setGPSRReconstructionOptions(). The GPSR options
         * are left to their defaults.
         *
         * @return default GPSROptions for reconstruction.
         */
        static constexpr GPSROptions reconstructionDefaults() {
            return GPSROptions{};
        }

        /**
         * @brief Make the default GPSROptions for training stage
         *
         * This is used as default argument for setGPSRTrainingOptions(). The GPSR options are left
         * to their defaults, except #tolA, which is set to 1e-6.
         *
         * @return default GPSROptions for training.
         */
        static GPSROptions trainingDefaults() {
            GPSROptions g{};
            g.tolA = 1e-6;
            return g;
        }
    };


    /**
     * @brief Switch for recording the training stop functions
     *
     * There are four training stop functions. These split up into the SPSTFM objective functions:
     *  * Objective function with a different GPSROptions::tau for every representation coefficient
     *    vector, TrainingStopFunction::objective:
     *    \f[ (\| P - D \, \Lambda \|_F^2 + \| \Lambda \, \diag((\tau_i)) \|_1) \cdot \frac{1}{N \, n} \f]
     *  * Objective function with the maximum GPSROptions::tau of all representation coefficient
     *    vector, TrainingStopFunction::objective_maxtau:
     *    \f[ (\| P - D \, \Lambda \|_F^2 + \max(\tau_i) \| \Lambda \|_1) \cdot \frac{1}{N \, n} \f]
     *
     * Hereby
     * \f$ P \in \mathbf R^{n \times N} \f$ is the training samples as columns,
     * \f$ D \in \mathbf R^{n \times m} \f$ is the dictionary with the atoms as columns and
     * \f$ \Lambda \in \mathbf R^{m \times N} \f$ has the representation coefficient vectors as columns.
     *
     * Then there are errors that mimic a reconstruction with known results:
     *
     *  * Test set error, for which a number \f$ K \f$ of random low resolution test samples are
     *    used to predict the corresponding high resolution test samples of the difference image,
     *    TrainingStopFunction::test_set_error:
     *    \f[ \|Q_{\mathrm h} - \hat Q_{\mathrm h} \|_1 \cdot \frac{1}{K \, n}. \f]
     *    \f$ K \f$ can be set with `setTrainingStopNumberTestSamples()`. This is expensive to
     *    calculate, as finding sparse coefficients with GPSR is the main cost of SPSTFM and this
     *    is done here to predict the high resolution test samples.
     *
     *  * Training set error, for which the \f$ N \f$ samples from the training set are used with
     *    the found representation coefficients. Like in the test set error the representation
     *    coefficients are found by using solely the low resolution samples. Therefore the train
     *    set error is only available, when using low resolution for the coefficients with
     *    `opt.setSparseCoeffTrainingResolution(SpstfmOptions::TrainingResolution::low)`, which is
     *    the default. But then, since the coefficients are required for training anyway, this
     *    train set error is for free and the quality is similar as with the test set error. The
     *    coefficients are then used to predict the corresponding high resolution training samples
     *    of the difference image and compare them, TrainingStopFunction::train_set_error:
     *    \f[ \|Q_{\mathrm h} - \hat Q_{\mathrm h} \|_1 \cdot \frac{1}{N \, n}. \f]
     *    The number of training samples \f$ K \f$ can be set with `setNumberTrainingSamples()`.
     *
     * For these methods
     * \f$ \hat Q_{\mathrm h} \f$ are the predicted test or training samples and
     * \f$ Q_{\mathrm h} \f$ are the reference test or training samples.
     *
     * So setting `dbg_recordTrainingStopFunctions` to `true` will collect all of these values in
     * corresponding vectors. This can be helpful to decide, which one to use, but usually the
     * training set error is preferable. You can access the vectors from `SpstfmFusor`.
     *
     * @see SpstfmFusor::getDbgObjective(),
     *      SpstfmFusor::getDbgObjectiveMaxTau(),
     *      SpstfmFusor::getDbgTestSetError(),
     *      SpstfmFusor::getDbgTrainSetError()
     */
    bool dbg_recordTrainingStopFunctions = false;

    /**
     * @brief Sampling strategy for test samples and initial dictionary
     *
     * This can be selected with `setSamplingStrategy()`.
     */
    enum class SamplingStrategy {
        random,  ///< Select random samples from the image.
        variance ///< Select the samples with the highest variance from the image. (default)
    };

    /**
     * @brief Handling of an existing dictionary in training
     *
     * This can be used in `setDictionaryReuse()`.
     */
    enum class ExistingDictionaryHandling {
        clear,   ///< Do not reuse an existing dictionary. Start with a new, fresh dictionary initialized from the samples. (default)
        improve, ///< If there is an existing dictionary, use it as initial dictionary and perform training to improve it. This might be handy when going to the next time series (i.e. changing input image pairs).
        use      ///< If there is an existing dictionary, use it as it is without new training. This can be used within one time series, since there the input image pairs are the same anyways.
    };

    /**
     * @brief Error measures used as stop condition for training
     *
     * This can be used in `setTrainingStopFunction()`. See `#dbg_recordTrainingStopFunctions` for
     * the formulas and descriptions of the stop functions.
     *
     * @see TrainingStopCondition, setTrainingStopTolerance()
     */
    enum class TrainingStopFunction {
        objective,        ///< Use SPSTFM Objective function with a distinct tau for every representation coefficient vector. (default)
        objective_maxtau, ///< Use SPSTFM Objective function with the maximum tau of all representation coefficient vectors.
        test_set_error,   ///< Use reconstruction error of the test set. Number of test set samples can be set with `setTrainingStopNumberTestSamples()`.
        train_set_error   ///< Use reconstruction error of the training set. Only available, when using low resolution for the coefficients in training, which is the default.
    };

    /**
     * @brief Error measures used to decide on the best dictionary state
     *
     * This can be used in `setBestShotErrorSet()`. The `BestShotErrorSet::test_set` and
     * `BestShotErrorSet::train_set` values use the same error measure as described in
     * `#dbg_recordTrainingStopFunctions`.
     */
    enum class BestShotErrorSet {
        none,     ///< Do not use best shot dictionary at all. Use the dictionary from the last training iteration instead.
        test_set, ///< Use the dictionary that had the lowest error according to the test set during training (expensive to evaluate).
        train_set ///< Use the dictionary that had the lowest error according to the training set during training (only available if using low resolution for the coefficients in training, which is the default). (default)
    };

    /**
     * @brief Condition used for the stop function values to actually stop
     *
     * This can be used in `setTrainingStopCondition()`. For the description of the single enum
     * values \f$ E^j \f$ denotes the value of the stop function (see `#TrainingStopFunction`) at
     * the current iteration and \f$ E^{j-1} \f$ at the previous. \f$ \varepsilon \f$ denotes the
     * tolerance set with `setTrainingStopTolerance()`.
     */
    enum class TrainingStopCondition {
        val_less,            ///< \f$ E^j < \varepsilon \f$
        abs_change_less,     ///< \f$ |E^{j-1} - E^j| < \varepsilon \f$
        abs_rel_change_less, ///< \f$ \dfrac{|E^{j-1} - E^j|}{|E^{j-1}|} < \varepsilon \f$
        change_less,         ///< \f$ E^{j-1} - E^j < \varepsilon \f$ (default)
        rel_change_less      ///< \f$ \dfrac{E^{j-1} - E^j}{E^{j-1}} < \varepsilon \f$
    };

    /**
     * @brief Resolution setting for various purposes in training
     *
     * This can be used in `setColumnUpdateCoefficientResolution()`,
     * `setSparseCoeffTrainingResolution()` and `setTrainingStopObjectiveFunctionResolution()`.
     * Basically, this decides about the resolution to compute the representation coefficients
     * from.
     */
    enum class TrainingResolution {
        high,   ///< Use high resolution samples to compute the representation coefficients.
        low,    ///< Use low resolution samples to compute the representation coefficients. (default)
        concat, ///< Use the samples in a concatenated matrix to compute the representation coefficients.
        average ///< Use both resolution samples separately to compute the representation coefficients and then average them.
    };

    /**
     * @brief Normalization handling of the training samples
     *
     * This can be used in `setSubtractMeanUsage()` and in `setDivideNormalizationFactor()`. For
     * the latter, see also `useStdDevForSampleNormalization()`.
     *
     * This will decide whether to do a normalization of the samples and if so with values of which
     * resolution. For a detailed description, see `setSubtractMeanUsage()` and in
     * `setDivideNormalizationFactor()`.
     */
    enum class SampleNormalization {
        none,    ///< Do not normalize. (default for subtracting mean)
        high,    ///< Normalize by high resolution difference image mean, variance or standard deviation.
        low,     ///< Normalize by low resolution difference image mean, variance or standard deviation.
        separate ///< Normalize the samples separately by the mean, variance or standard deviation of the difference image of the according resolution. (default for dividing by factor)
    };

    /**
     * @brief Normalization handling of the dictionary
     *
     * This can be used in `setDictionaryInitNormalization()` and
     * `setDictionaryKSVDNormalization()`.
     *
     * It decides whether to normalize the atoms and if so how.
     */
    enum class DictionaryNormalization {
        none,        ///< Do not normalize the atoms at all.
        fixed,       ///< This will divide all atoms by the same factor. The factor is the norm of the first high resolution atom.
        independent, ///< This will divide each atom in both resolutions by its respective norm. So every atom will have a norm of 1 with this. (default)
        pairwise     ///< This will divide each atom pair (high and low resolution atom) by the larger norm. So the ratio between their length will be preserved and the larger norm is 1.
    };

    /**
     * @brief Default constructor setting the default values
     */
    SpstfmOptions() {
        // some empirical values for the GPSR tolerances
        gpsrOptsTraining.tolA       = 1e-6;
        gpsrOptsReconstruction.tolA = 1e-5;
    }

    /**
     * @brief Set existing dictionary reuse handling
     * @param e is the setting.
     *
     * So for the first prediction of a time series this can be set to
     * `ExistingDictionaryHandling::improve`. For the following predictions that have the same
     * input image pairs the dictionary can just be used without training by setting it to
     * `ExistingDictionaryHandling::use`.
     *
     * @see ExistingDictionaryHandling for a detailed description. getDictionaryReuse() returns the
     * current setting.
     */
    void setDictionaryReuse(ExistingDictionaryHandling e = ExistingDictionaryHandling::clear) {
        dictHandling = e;
    }

    /**
     * @brief Get current reuse setting for handling an existing dictionary
     * @return current setting
     *
     * @see ExistingDictionaryHandling for a detailed description. setDictionaryReuse() allows to
     * change the setting.
     */
    ExistingDictionaryHandling getDictionaryReuse() const {
        return dictHandling;
    }

    /**
     * @brief Get current setting for the sampling strategy
     * @return current setting
     * @see SamplingStrategy for a detailed description. setSamplingStrategy() allows to change the
     * setting.
     */
    SamplingStrategy getSamplingStrategy() const {
        return sampStrat;
    }

    /**
     * @brief Set the sampling strategy for the test data and dictionary initialization
     * @param s is the setting.
     *
     * @see SamplingStrategy for a detailed description. setSamplingStrategy() returns the current
     * setting.
     */
    void setSamplingStrategy(SamplingStrategy s = SamplingStrategy::variance) {
        sampStrat = s;
    }

    /**
     * @brief Get current setting for the percentage of allowed invalid pixels in a patch
     * @return current setting
     * @see setInvalidPixelTolerance()
     */
    double getInvalidPixelTolerance() const {
        return maskInvalidTol;
    }

    /**
     * @brief Set the percentage of allowed invalid pixels in a patch
     * @param tol the tolerance (0.2 meaning 20%) of invalid pixels in a patch, which is allowed in
     * a patch.
     *
     * This will set the tolerance above which patches are neither used as training / test data nor
     * as initial dictionary patches. Invalid pixels, which have invalid (maybe negative) values,
     * in patches are not used as they are anyway, but replaced by the mean value of the valid
     * pixels. So patches that will be tolerated as training data are modified appropriately.
     * However, patches with too much replaced pixel values do not hold enough information about
     * the image structure details to improve the dictionary. This tolerance specifies what is
     * acceptable and assumed to improve the dictionary even with invalid pixels.
     *
     * By default 15% is set, which means for a patch size of 7 x 7 that 7 invalid pixels are
     * tolerated.
     *
     * Note, for reconstruction every patch is used (with mean values replacing invalid values),
     * except the complete patch consists of invalid pixels (to save time).
     *
     * @see getInvalidPixelTolerance()
     */
    void setInvalidPixelTolerance(double tol = 0.15) {
        maskInvalidTol = tol;
    }

    /**
     * @brief Get the patch size
     * @return current setting for the patch size
     * @see setPatchSize()
     */
    unsigned int getPatchSize() const {
        return patchSize;
    }

    /**
     * @brief Set the patch size setting
     * @param size is the new size to use for `size` x `size` patches.
     *
     * This also determines the number of rows for the training samples and dictionary atoms to be
     * 2 `size`². The theoretical minimum patch size is 2 and it must be twice the patch overlap.
     *
     * @see getPatchSize(), setPatchOverlap()
     */
    void setPatchSize(unsigned int size = 7) {
        if (size < 2)
            IF_THROW_EXCEPTION(invalid_argument_error("Patch size must be at least 2. You tried: " + std::to_string(size)));
        patchSize = size;
    }

    /**
     * @brief Get the patch overlap setting
     * @return current setting for the patch overlap
     */
    unsigned int getPatchOverlap() const {
        return patchOverlap;
    }

    /**
     * @brief Set the patch overlap setting
     * @param overlap is the new overlap to use for the patches. May be 0.
     *
     * The overlap must be less or equal to half of the patch size.
     *
     * @see getPatchOverlap(), setPatchSize()
     */
    void setPatchOverlap(unsigned int overlap = 2) {
        patchOverlap = overlap;
    }

    /**
     * @brief Get the current setting for the number of training samples
     * @return number of training samples in the training set.
     * @see setNumberTrainingSamples()
     */
    unsigned int getNumberTrainingSamples() const {
        return numberTrainingSamples;
    }

    /**
     * @brief Set the number of training samples to use as training data
     * @param num is the positive number of training samples in the training set.
     * @see getNumberTrainingSamples()
     */
    void setNumberTrainingSamples(unsigned int num = 2000) {
        if (num == 0)
            IF_THROW_EXCEPTION(invalid_argument_error("Training size must be positive, e. g. 2000. You tried: 0"));
        numberTrainingSamples = num;
    }

    /**
     * @brief Get the number of atoms in the dictionary
     * @return setting for the number of dictionary atoms.
     * @see `setDictSize()`
     */
    unsigned int getDictSize() const {
        return dictSize;
    }

    /**
     * @brief Define the number of atoms the dictionary will have
     * @param num is the number of dictionary atoms.
     *
     * The dictionary size, i.e. the number of atoms, should be much larger than the dimension of
     * the atoms, which is patch size². In the paper the dictionary size is 256, while its atoms
     * have 49 entries (patch size 7). This makes the dictionary overcomplete.
     *
     * @see getDictSize(), getPatchSize()
     */
    void setDictSize(unsigned int num = 256) {
        if (num == 0)
            IF_THROW_EXCEPTION(invalid_argument_error("Dictionary size must be positive, e. g. 256. You tried: 0"));
        dictSize = num;
    }

    /**
     * @brief Get the minimal number of iterations the training will do
     * @return current setting for the minimal number of training iterations.
     * @see setMinTrainIter()
     */
    unsigned int getMinTrainIter() const {
        return minTrainIter;
    }

    /**
     * @brief Set the minimal number of iterations the training will do
     * @param numit is the minimal number of training iterations. May be 0 and may be `getMaxTrainIter()`.
     *
     * This determines the minimal number of iterations that will be done, regardless of what the
     * stop criterion is. If the minimum and the maximum number of training iterations are equal
     * the stop criterion will not be used at all. Both numbers can also be 0 in which case no
     * training will be performed (same effect as setting
     * `opt.setDictionaryReuse(ExistingDictionaryHandling::use)`).
     *
     * @see getMinTrainIter(), setMaxTrainIter()
     */
    void setMinTrainIter(unsigned int numit = 10) {
        minTrainIter = numit;
    }

    /**
     * @brief Get the maximal number of iterations the training will do
     * @return current setting for the maximal number of training iterations.
     * @see setMaxTrainIter()
     */
    unsigned int getMaxTrainIter() const {
        return maxTrainIter;
    }

    /**
     * @brief Set the maximal number of iterations the training will do
     * @param numit is the maximal number of training iterations. May be `getMinTrainIter()`.
     *
     * This determines the maximal number of iterations that will be done, regardless of what the
     * stop criterion is. If the minimum and the maximum number of training iterations are equal
     * the stop criterion will not be used at all. Both numbers can also be 0 in which case no
     * training will be performed (same effect as setting
     * `opt.setDictionaryReuse(ExistingDictionaryHandling::use)`).
     *
     * @see getMaxTrainIter(), setMinTrainIter()
     */
    void setMaxTrainIter(unsigned int numit = 20) {
        maxTrainIter = numit;
    }

    /**
     * @brief Get the build-up threshold
     * @return threshold above the build-up index is interpreted as build-up area.
     * @see setBUThreshold()
     */
    double getBUThreshold() const {
        return buThreshold;
    }

    /**
     * @brief Set the threshold for the continuous buid-up index
     *
     * @param threshold is used for decision making what is considered as build-up area and what
     * not. Must be in the range [-1, 1].
     *
     * @see getBUThreshold(), setUseBuildUpIndexForWeights()
     */
    void setBUThreshold(double threshold = 0) {
        if (threshold < -1 || threshold > 1)
            IF_THROW_EXCEPTION(invalid_argument_error("The Build-Up Index threshold must be in the range [-1, 1], e. g. 0.1. You tried: " + std::to_string(threshold)));
        buThreshold = threshold;
    }

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
     * @brief Set the date of the first input image pair in `SpstfmFusor::imgs`
     * @param date1 is the date of the first input image pair in `SpstfmFusor::imgs`.
     *
     * This date is used together with the resolution tags to get the images from
     * `SpstfmFusor::imgs`. The notation is like shown in Table \ref spstfm_image_structure
     * "SPSTFM image structure".
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
     * @brief Set the date of the last input image pair in `SpstfmFusor::imgs`
     * @param date3 is the date of the last input image pair in `SpstfmFusor::imgs`.
     *
     * This date is used together with the resolution tags to get the images from
     * `SpstfmFusor::imgs`. The notation is like shown in Table \ref spstfm_image_structure
     * "SPSTFM image structure".
     *
     * @see getDate3(), setDate1(), setHighResTag(), setLowResTag()
     */
    void setDate3(int date3) {
        isDate3Set = true;
        this->date3 = date3;
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
     * @param tag is the new high resolution tag.
     *
     * This tag is used together with the dates 1 and 3 to get the high resolution images from
     * `SpstfmFusor::imgs`. For the notation of the images see Table \ref spstfm_image_structure
     * "SPSTFM image structure".
     *
     * @see getHighResTag(), setLowResTag(),  setDate1(), setDate3()
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
     * @param tag is the new low resolution tag.
     *
     * This tag is used together with the dates 1, 2 and 3 to get the low resolution images from
     * `SpstfmFusor::imgs`. For the notation of the images see Table \ref spstfm_image_structure
     * "SPSTFM image structure".
     *
     * @see getLowResTag(), setHighResTag(),  setDate1(), setDate3(),
     * SpstfmFusor::predict()
     */
    void setLowResTag(std::string const& tag) {
        lowTag = tag;
    }

    /**
     * @brief Check the current setting for the online mode in K-SVD
     * @return current setting
     * @see setUseKSVDOnlineMode()
     */
    bool useKSVDOnlineMode() const {
        return useKSVDOnline;
    }

    /**
     * @brief Set whether to use the online mode in the K-SVD algorithm
     * @param useOnlineMode determines whether to use online mode or not.
     *
     * The K-SVD algorithm does the update of the dictionary atoms during training. The online mode
     * for K-SVD is analog to the Gauß-Seidel compared to the Jacobi method when solving a linear
     * system. So in the update procedure of a dictionary atom, all coefficient vectors that use
     * that atom are considered. From these vectors the coefficients that get multiplied with the
     * atom, will also be updated at the end of the procedure. So in later update procedures there
     * have been changed coefficients from previous atom updates. With online mode the modified
     * coefficients will be used for the update, while offline mode will always use the original
     * coefficients before any update took place (the coefficients found by GPSR).
     *
     * @see useKSVDOnlineMode()
     */
    void setUseKSVDOnlineMode(bool useOnlineMode = true) {
        useKSVDOnline = useOnlineMode;
    }

    /**
     * @brief Get the current setting which stop function will be used to check convergence
     * @return current setting for the stop function
     * @see setTrainingStopFunction()
     */
    TrainingStopFunction getTrainingStopFunction() const {
        return stopFun;
    }

    /**
     * @brief Set which stop function to use for checking convergence
     *
     * @param fun is the stop function; one of TrainingStopFunction::objective,
     * TrainingStopFunction::objective_maxtau, TrainingStopFunction::test_set_error,
     * TrainingStopFunction::train_set_error. See there for details and also at
     * `#dbg_recordTrainingStopFunctions`, where the formulas are shown!
     *
     * The stop function is only one part for checking convergence. The following lists all parts
     * that play a role for that:
     *  * setTrainingStopFunction() - objective function or error set
     *  * setTrainingStopObjectiveFunctionResolution() - only relevant if using an objective function as stop function
     *  * setTrainingStopNumberTestSamples() - only relevent if using the test set error as stop function
     *  * setTrainingStopCondition() - absolute or signed, pure or relative change or just the value
     *  * setTrainingStopTolerance() - right hand side tolerance value
     *
     * @remark The training might go on or stop independent of convergence, but because of the
     * minimal and maximal number of training iterations, see setMinTrainIter() and
     * setMaxTrainIter(), respectively.
     *
     * @see getTrainingStopFunction()
     */
    void setTrainingStopFunction(TrainingStopFunction fun = TrainingStopFunction::objective) {
        stopFun = fun;
    }

    /**
     * @brief Get the current setting which resolution to use in an objective stop function
     * @return current setting for the objective function resolution
     * @see setTrainingStopObjectiveFunctionResolution()
     */
    TrainingResolution getTrainingStopObjectiveFunctionResolution() const {
        return stopFunRes;
    }

    /**
     * @brief Set which resolution to use in an objective stop function
     *
     * @param res is the resolution to use in the objective fuction.
     *
     * There are two objective functions to choose:
     *  * TrainingStopFunction::objective_maxtau
     *    \f[ (\| P - D \, \Lambda \|_F^2 + \| \Lambda \, \diag((\tau_i)) \|_1) \cdot \frac{1}{N \, n} \f]
     *  * TrainingStopFunction::objective
     *    \f[ (\| P - D \, \Lambda \|_F^2 + \max(\tau_i) \| \Lambda \|_1) \cdot \frac{1}{N \, n} \f]
     *
     * Hereby \f$ \Lambda \in \mathbf R^{m \times N} \f$ are in any case the representation
     * coefficients. For `res` being TrainingResolution::high, TrainingResolution::low or
     * TrainingResolution::concat \f$ P \f$ is the high resolution, low resolution or concatenated
     * training sample matrix, respectively and \f$ D \f$ is the high resolution, low resolution or
     * concatenated dictionary, respectively. For `res` being TrainingResolution::average the
     * values of the objective function evaluated with TrainingResolution::high and
     * TrainingResolution::low are averaged.
     *
     * Note, when using an error set this resolution setting is ignored.
     *
     * The objective function resolution is only one part for checking convergence. The following
     * lists all parts that play a role for that:
     *  * setTrainingStopFunction() - objective function or error set
     *  * setTrainingStopObjectiveFunctionResolution() - only relevant if using an objective function as stop function
     *  * setTrainingStopNumberTestSamples() - only relevent if using the test set error as stop function
     *  * setTrainingStopCondition() - absolute or signed, pure or relative change or just the value
     *  * setTrainingStopTolerance() - right hand side tolerance value
     *
     * @remark The training might go on or stop independent of convergence, but because of the
     * minimal and maximal number of training iterations, see setMinTrainIter() and
     * setMaxTrainIter(), respectively.
     *
     *
     * @see getTrainingStopObjectiveFunctionResolution()
     */
    void setTrainingStopObjectiveFunctionResolution(TrainingResolution res = TrainingResolution::low) {
        stopFunRes = res;
    }

    /**
     * @brief Get the current setting which condition to use for checking convergence
     * @return current setting for the convergence condition
     * @see setTrainingStopCondition()
     */
    TrainingStopCondition getTrainingStopCondition() const {
        return stopCon;
    }

    /**
     * @brief Set the convergence condition
     *
     * @param cond is the convergence condition, see #TrainingStopCondition for a detailed
     * explanation.
     *
     * So this setting controls how the stop function values are compared with with the tolerance.
     * But the stop condition is only one part for checking convergence. The following lists all
     * parts that play a role for that:
     *  * setTrainingStopFunction() - objective function or error set
     *  * setTrainingStopObjectiveFunctionResolution() - only relevant if using an objective function as stop function
     *  * setTrainingStopNumberTestSamples() - only relevent if using the test set error as stop function
     *  * setTrainingStopCondition() - absolute or signed, pure or relative change or just the value
     *  * setTrainingStopTolerance() - right hand side tolerance value
     *
     * @remark The training might go on or stop independent of convergence, but because of the
     * minimal and maximal number of training iterations, see setMinTrainIter() and
     * setMaxTrainIter(), respectively.
     *
     *
     * @see getTrainingStopCondition()
     */
    void setTrainingStopCondition(TrainingStopCondition cond = TrainingStopCondition::change_less) {
        stopCon = cond;
    }

    /**
     * @brief Get the current setting what tolerance to use for checking convergence
     * @return current setting for the tolerance
     * @see setTrainingStopTolerance()
     */
    double getTrainingStopTolerance() const {
        return stopVal;
    }

    /**
     * @brief Set the tolerance when it should be considered as converged
     *
     * @param tol is the value to compare with. It is denoted by \f$ \varepsilon \f$ in
     * #TrainingStopCondition.
     *
     * So this tolerance defines, when the training should be considered as converged. But the
     * tolerance is only one part for checking convergence. The following lists all parts that play
     * a role for that:
     *  * setTrainingStopFunction() - objective function or error set
     *  * setTrainingStopObjectiveFunctionResolution() - only relevant if using an objective function as stop function
     *  * setTrainingStopNumberTestSamples() - only relevent if using the test set error as stop function
     *  * setTrainingStopCondition() - absolute or signed, pure or relative change or just the value
     *  * setTrainingStopTolerance() - right hand side tolerance value
     *
     * @remark The training might go on or stop independent of convergence, but because of the
     * minimal and maximal number of training iterations, see setMinTrainIter() and
     * setMaxTrainIter(), respectively.
     *
     *
     * @see getTrainingStopCondition()
     */
    void setTrainingStopTolerance(double tol = 1e-10) {
        stopVal = tol;
    }

    /**
     * @brief Get the current setting for the number of test samples in the test set
     * @return current setting for the number of test samples
     * @see setTrainingStopNumberTestSamples()
     */
    unsigned int getTrainingStopNumberTestSamples() const {
        return stopNumberTestSamples;
    }

    /**
     * @brief Set the number of test samples in the test set
     *
     * @param num is the number of test samples in the test set. Note, when using the test set
     * error as stop function the predictions made are only used for testing purposes and a high
     * number of test samples can make it the main computational cost of SPSTFM.
     *
     * The following lists all parts that play a role for convergence checking:
     *  * setTrainingStopFunction() - objective function or error set
     *  * setTrainingStopObjectiveFunctionResolution() - only relevant if using an objective function as stop function
     *  * setTrainingStopNumberTestSamples() - only relevent if using the test set error as stop function
     *  * setTrainingStopCondition() - absolute or signed, pure or relative change or just the value
     *  * setTrainingStopTolerance() - right hand side tolerance value
     *
     * @remark The training might go on or stop independent of convergence, but because of the
     * minimal and maximal number of training iterations, see setMinTrainIter() and
     * setMaxTrainIter(), respectively.
     *
     * @see getTrainingStopCondition()
     */
    void setTrainingStopNumberTestSamples(unsigned int num = 4000) {
        stopNumberTestSamples = num;
    }

    /**
     * @brief Get the tolerance of the weights difference
     * @return tolerance
     * @see setWeightsDiffTol()
     */
    double getWeightsDiffTol() const {
        return weightTol;
    }

    /**
     * @brief Set the tolerance of the weights difference
     *
     * @param tol called \f$ \delta \f$ in the paper this value is the limit where the weights are
     * calculated in the normal way. When this limit is exceeded, the weights are set to 0 and 1.
     *
     * At reconstruction stage the prediction of the high resolution difference patches can be done
     * from image 1 to image 2 (from left) or from image 3 to image 2 (from right), see also Table
     * \ref spstfm_image_structure "SPSTFM image structure". Both can be done and used, but usually
     * not in the same amount. They are weighted by the weights \f$ w_1 \f$ and \f$ w_3 \f$. To
     * calculate \f$ w_1 \f$ and \f$ w_3 \f$ the parameters \f$ v_1 \f$ and \f$ v_3 \f$, which
     * represent the amount of change found in the low resolution patches, are determined. However,
     * if there is very little change from one side while there is a lot of change from the other
     * side, the predicted difference patch from the side with the little change should be used
     * solely by setting its weight to 1 and the other to 0. `tol` or \f$ \delta \f$ is the limit
     * of the difference of \f$ v_1 \f$ and \f$ v_3 \f$, where the usual way to calculate the
     * weights
     * \f[ w_i = \dfrac{v_1 \, v_3}{v_i \, v_1 + v_i \, v_3} \f]
     * is used. If \f$ |v_1 - v_3| > \delta \f$ weights 1 and 0 are used. In the special case of
     * \f$ v_1 = v_3 = 0 \f$ equal weights of \f$ v_1 = v_3 = \frac 1 2 \f$ are used.
     */
    void setWeightsDiffTol(double tol = 0.2) {
        weightTol = tol;
    }

    /**
     * @brief Setting whether to use build-up index for calculating weights (only available for multi-channel images)
     * @return current setting
     * @see setUseBuildUpIndexForWeights()
     */
    bool useBuildUpIndexForWeights() const {
        return useBuildUpIndexWeights;
    }

    /**
     * @brief Set whether to use build-up index for calculating weights (only available for multi-channel images)
     * @param use determines whether to use build-up index.
     *
     * The build-up index can only be used for a special kind of multi-channel images. The image
     * requires Red, Near Infrared and Shortwave Infrared 1 channels. The order these channels
     * appear in the image can be set with `setRedNirSwirOrder()`. Also the pure images are used
     * for the build-up images instead of the difference images, so a shifted mean will maybe deny
     * to use a build-up index.
     *
     * Nevertheless, if the images are appropriate for the build-up index weighting, it works as
     * follows:
     *  1. All three low resolution images are converted to build-up index using the order of
     *     channels as specified with `setRedNirSwirOrder()` and the threshold set with
     *     `setBUThreshold()`.
     *  2. For each patch use the differences of the build-up index patches from image 1 to image 2
     *     and from image 3 to image 2.
     *  3. Count how many build-up pixels actually changed. This means if an area is build-up in
     *     both patches, it has not changed.
     *  4. This count is then normed by dividing by the total number of pixels in a patch to yield
     *     the averaged build-up change count per pixel. Denote the average count from image 1 to
     *     image 2 by \f$ v_1 \f$ and the average count from image 3 to image 2 by \f$ v_3 \f$.
     *  5. Calculate the weights for each patch as described in setWeightsDiffTol(). These weights
     *     will be used in the reconstruction of every channel.
     *
     * If not using the build-up index then the weights will be calculated for each channel
     * separately as follows:
     *  1. The absolute difference images from image 1 to 2 and from image 3 to 2 are computed.
     *  2. For each patch the difference values are summed up and normed by the maximum difference
     *     value found in the difference images and by the total number of pixels in a patch. This
     *     gives the average difference per pixel and is denoted by \f$ v_1 \f$ and \f$ v_3 \f$,
     *     respectively.
     *  3. Calculate the weights for each patch as described in setWeightsDiffTol(). These weights
     *     will be used in the reconstruction of every channel.
     *
     * @see getUseBuildUpIndexForWeights(), setRedNirSwirOrder(), setBUThreshold(),
     * setWeightsDiffTol(), ColorMapping::Red_NIR_SWIR_to_BU, ConstImage::convertColor()
     */
    void setUseBuildUpIndexForWeights(bool use = false) {
        useBuildUpIndexWeights = use;
    }

    /**
     * @brief Get the expected order of Red, Near Infrared and Shortwave Infrared channels used to
     * calculate the build-up index
     *
     * @return order as vector of channel indices
     * @see setRedNirSwirOrder()
     */
    std::array<unsigned int, 3> const& getRedNirSwirOrder() const {
        return red_nir_swir_order;
    }

    /**
     * @brief Set the order of Red, Near Infrared and Shortwave Infrared channels to use for
     * calculation of the build-up index
     *
     * @param order is the vector of channel indices. order[0] will be used as red channel, order[1] as
     * near infrared channel and order[2] as shortwave infrared channel.
     *
     * @see getRedNirSwirOrder(), setUseBuildUpIndexForWeights(), ColorMapping::Red_NIR_SWIR_to_BU,
     * ConstImage::convertColor()
     */
    void setRedNirSwirOrder(std::array<unsigned int, 3> const& order = {0, 1, 2}) {
        red_nir_swir_order = order;
    }

    /**
     * @brief Current setting for resolution to use for GPSR in training before K-SVD
     *
     * @return setting for the resolution of the samples and dictionary used for getting the sparse
     * representation coefficients by GPSR during training.
     *
     * @see setSparseCoeffTrainingResolution()
     */
    TrainingResolution getSparseCoeffTrainingResolution() const {
        return sparseRepresentationCoefficientResolution;
    }

    /**
     * @brief Set the resolution to use for GPSR in training before K-SVD
     *
     * @param res is the resolution, one of TrainingResolution::high, TrainingResolution::low,
     * TrainingResolution::average, TrainingResolution::concat.
     *
     * This setting is used for the GPSR algorithm that finds the sparse representation
     * coefficients that are required before K-SVD can be applied. Depending on this setting the
     * low resolution, high resolution or concatenated dictionary and sample is used in GPSR. For
     * average setting low and high resolution solutions are averaged.
     *
     * @see getSparseCoeffTrainingResolution(), setColumnUpdateCoefficientResolution()
     */
    void setSparseCoeffTrainingResolution(TrainingResolution res = TrainingResolution::low) {
        sparseRepresentationCoefficientResolution = res;
    }

    /**
     * @brief Current setting for the resolution to use for the dictionary update with K-SVD
     * @return setting for K-SVD dictionary update process.
     * @see setColumnUpdateCoefficientResolution() for a detailed description.
     */
    TrainingResolution getColumnUpdateCoefficientResolution() const {
        return columnUpdateCoefficientResolution;
    }

    /**
     * @brief Set which resolution to use for updating the dictionary with K-SVD
     * @param res is the resolution setting.
     *
     * For the dictionary update several options are possible. If `res` is
     * TrainingResolution::concat, the error matrix is concatenated and K-SVD makes a single SVD of
     * the error matrix. Then the update of the atom and the corresponding coefficients are
     * straight forward.
     *
     * For the other options there are always two error matrices -- one for each resolution -- and
     * two SVDs, accordingly. The dictionary atoms are updated separately from the SVD
     * corresponding to their resolution. The coefficients, however, are shared for both
     * dictionaries. Their update depends on `res`. If `res` is
     *  * TrainingResolution::low, the new coefficients are taken from the SVD of the low
     *     resolution error matrix.
     *  * TrainingResolution::high, the new coefficients are taken from the SVD of the high
     *     resolution error matrix.
     *  * TrainingResolution::average, the new coefficients are the average of the coefficients of
     *     both SVDs.
     *
     * Note, when not using the online update mode, see setUseKSVDOnlineMode(), all of the three
     * latter options bahve the same, since the updated coefficients are not used at all then.
     * However, TrainingResolution::concat is still different from these.
     *
     * @see getColumnUpdateCoefficientResolution(), setSparseCoeffTrainingResolution()
     */
    void setColumnUpdateCoefficientResolution(TrainingResolution res = TrainingResolution::low) {
        columnUpdateCoefficientResolution = res;
    }

    /**
     * @brief Current setting for subtracting the mean of the difference image from the sample data
     * @return current setting
     * @see setSubtractMeanUsage()
     */
    SampleNormalization getSubtractMeanUsage() const {
        return subtractMeanValue;
    }

    /**
     * @brief Set whether to subtract the mean of the difference image from the sample data
     * @param usage determines whether and if so how to subtract a mean value.
     *
     * With `usage` being
     *  * SampleNormalization::none no mean is subtracted from the sample data,
     *  * SampleNormalization::high the mean of the high resolution difference image is subtracted from the sample data,
     *  * SampleNormalization::low the mean of the low resolution difference image is subtracted from the sample data,
     *  * SampleNormalization::separate the mean of the high resolution difference image is
     *    subtracted from the high resolution difference samples and the mean of the low resolution
     *    difference image is subtracted from the low resolution difference samples.
     *
     * @remark It it strongly suggested to choose `SampleNormalization::none` here, since it does
     * not seem to make sense to subtract the mean of the *difference* image from it. The means of
     * the images themselves are not of importance, because only difference images are used for
     * SPSTFM.
     *
     * @see getSubtractMeanUsage(), setDivideNormalizationFactor(),
     * setUseStdDevForSampleNormalization()
     */
    void setSubtractMeanUsage(SampleNormalization usage = SampleNormalization::none) {
        subtractMeanValue = usage;
    }

    /**
     * @brief Current setting whether the samples should be normalized by a factor
     * @return setting for dividing samples.
     * @see setDivideNormalizationFactor()
     */
    SampleNormalization getDivideNormalizationFactor() const {
        return divideNormalizationFactor;
    }

    /**
     * @brief Set whether the samples should be normalized by a factor
     * @param usage is the setting for dividing sample normalization.
     *
     * This calculates a scaler factor and divides all samples by that. The factor can be the
     * standard deviation or (its square) the variance, depending on the setting of
     * setUseStdDevForSampleNormalization(). Then, with `usage` being
     *  * SampleNormalization::none, the sample data is not divided by anything,
     *  * SampleNormalization::high, the sample data is divided by the factor of the high resolution difference image,
     *  * SampleNormalization::low, the sample data is divided by the factor of the low resolution difference image,
     *  * SampleNormalization::separate, the high resolution samples are divided by the factor of
     *    the high resolution difference image and the low resolution samples are divided by the
     *    factor of the low resolution difference image. When using the standard deviation as factor,
     *    this normalization allows the dictionary to cope with images that have different data
     *    ranges across low and high resolution images. For images that have the same data range,
     *    there is no drawback.
     *
     * @see getDivideNormalizationFactor(), setUseStdDevForSampleNormalization()
     */
    void setDivideNormalizationFactor(SampleNormalization usage = SampleNormalization::separate) {
        divideNormalizationFactor = usage;
    }

    /**
     * @brief Get current setting whether the standard deviation is used for sample normalization
     *
     * @return current setting. True means that the standard deviation is used in the normalization
     * step and false means that the variance is used.
     *
     * @see setUseStdDevForSampleNormalization()
     */
    bool useStdDevForSampleNormalization() const {
        return stdDevAsSampleNormalization;
    }

    /**
     * @brief Set whether to use the standard deviation instead of variance for normalization
     *
     * @param use must be true for standard deviation, false for variance of the difference image.
     *
     * @remark For normalization the standard deviation should be used, generally. Tests show this
     * allows to use the normal dictionary even in situations where the data ranges of low and high
     * resolution images differ.
     *
     * @see useStdDevForSampleNormalization(), setDivideNormalizationFactor()
     */
    void setUseStdDevForSampleNormalization(bool use = true) {
        stdDevAsSampleNormalization = use;
    }

    /**
     * @brief Get GPSR options for the training stage
     * @return GPSR options objects
     * @see setGPSRTrainingOptions()
     */
    GPSROptions const& getGPSRTrainingOptions() const {
        return gpsrOptsTraining;
    }

    /**
     * @brief Set the GPSR options used for the training stage
     * @param opts is the new options object for the GPSR algorithm.
     *
     * These GPSR options will be used in the GPSR algorithms in the training. This means it is
     * used for the retrieval of the representation coefficients that are used by the K-SVD
     * algorithm, but also for the test set to determine the error.
     *
     * The default values used here are shown in GPSROptions, except for GPSROptions::tolA, which
     * is 1e-6 instead of 1e-5 here.
     *
     * @see getGPSRTrainingOptions(), setGPSRReconstructionOptions, GPSROptions
     */
    void setGPSRTrainingOptions(GPSROptions const& opts = GPSROptions::trainingDefaults()) {
        checkGPSROptions(opts);
        gpsrOptsTraining = opts;
    }

    /**
     * @brief Get GPSR options for the reconstruction stage
     * @return GPSR options objects.
     * @see setGPSRReconstructionOptions()
     */
    GPSROptions const& getGPSRReconstructionOptions() const {
        return gpsrOptsReconstruction;
    }

    /**
     * @brief Set the GPSR options used for the reconstruction stage
     * @param opts is the new options object for the GPSR algorithm.
     *
     * These GPSR options will be used in the GPSR algorithm for the prediction in the
     * reconstruction stage. So it receives the low resolution difference patches, returns the
     * representation coefficients, which are used with the high resolution dictionary to predict
     * the corresponding high resolution difference patches.
     *
     * The default values are shown in GPSROptions.
     *
     * @see getGPSRTrainingOptions(), setGPSRReconstructionOptions, GPSROptions
     */
    void setGPSRReconstructionOptions(GPSROptions const& opts = GPSROptions::reconstructionDefaults()) {
        checkGPSROptions(opts);
        gpsrOptsReconstruction = opts;
    }

    /**
     * @brief Get the current setting for the best shot dictionary
     * @return current setting.
     * @see setBestShotErrorSet()
     */
    BestShotErrorSet getBestShotErrorSet() const {
        return useBestShot;
    }

    /**
     * @brief Determine whether to use the best shot dictionary and which error set to use for it
     * @param set the error set to use:
     *  * BestShotErrorSet::none: Best shot dictionary is not used at all. The dictionary of the
     *   last training iteration is used for reconstruction.
     *  * BestShotErrorSet::test_set: The test set error is used to determine the best dictionary.
     *    This is used after the training.
     *  * BestShotErrorSet::train_set: The training set error is used to determine the best
     *    dictionary. This is used after the training. This option is only available if the GPSR
     *    algorithm in the training uses the low resolution dictionary and sample to determine the
     *    representation coefficients, see `setSparseCoeffTrainingResolution()`.
     *
     * @see getBestShotErrorSet()
     */
    void setBestShotErrorSet(BestShotErrorSet set = BestShotErrorSet::train_set) {
        useBestShot = set;
    }

    /**
     * @brief Set if and how the dictionary should be normalized during update by K-SVD
     * @param normalization setting
     *
     * So, roughly speaking, in K-SVD after the SVD the matrices U, S and V are available. U and V
     * are orthogonal and thus their column are normal, S is diagonal and ordered from large to
     * small. Then, to update one dictionary atom and the corresponding coefficients, the first
     * column of U replaces the old dictionary atom and the first column of V is replaces the used
     * coefficients. The first value of S is used to scale the columns to receive the optimal first
     * rank approximation. This is the original procedure, which results in normal columns in the
     * dictionary, independent of the resolution.
     *  * When choosing DictionaryNormalization::independent just that will be done.
     *  * DictionaryNormalization::none will do it the other way round. So the dictionary atom is
     *    scaled instead of the coefficients. This allows for images with different data ranges
     *    across low and high resolution that the dictionary reflects this property and not the
     *    coefficients. This makes sense, since the coefficients are shared for both resolutions and
     *    thus cannot reflect different data ranges.
     *  * DictionaryNormalization::fixed will yield a similar result. It will just divide all atoms
     *    and multiply the coefficients by a factor additionally. The factor is the norm of the first
     *    high resolution atom. So the dictionary atoms' norms will be around 1, but all ratios of
     *    atom norms are preserved.
     *  * The two latter settings preserved the ratios between the norms across different atoms.
     *    Choosing DictionaryNormalization::pairwise will also scale the dictionary atoms but instead
     *    of dividing all atoms by the same factor, as with DictionaryNormalization::fixed, this will
     *    divide each atom pair (high and low resolution atom) by the larger norm of both and
     *    multiply the coefficients by that factor. So this setting only preserves the ratio between
     *    the norms of two atoms within one pair and each atom pair has one atom with the norm equal
     *    to 1.
     *
     * Note, when using TrainingResolution::concat in setColumnUpdateCoefficientResolution(), K-SVD
     * does not differ between the resolutions. Thus it might not matter which side (atom or
     * coefficients) is scaled by the singular value. Also there DictionaryNormalization::pairwise
     * is the same as DictionaryNormalization::independent.
     *
     * In practice a test has shown that normalizing the samples with their separate standard
     * deviation and using DictionaryNormalization::independent is the best combination. However
     * results may vary in other situations.
     *
     * @see getDictionaryKSVDNormalization(), setDivideNormalizationFactor()
     */
    void setDictionaryKSVDNormalization(DictionaryNormalization normalization = DictionaryNormalization::independent) {
        scaleDictInsteadCoeff = normalization;
    }

    /**
     * @brief Current setting for the normalization of the dictionary during update by K-SVD
     * @return current setting
     * @see setDictionaryKSVDNormalization()
     */
    DictionaryNormalization getDictionaryKSVDNormalization() const {
        return scaleDictInsteadCoeff;
    }

    /**
     * @brief Set if and how the dictionary should be normalization at initialization
     * @param normalization setting
     *
     * At initialization, patches are sampled from the high and low resolution difference images.
     * The data ranges can vary largely, since the data type could be 8 bit or 16 bit or the sensor
     * has lower precision (e.g. 10 bit) and the contrast is not changed. So it can make sense to
     * normalize the sample matrix, see setDivideNormalizationFactor() for that. Then the
     * dictionary is initialized from the sample matrix. There another normalization can be done
     * and the very step can be controlled with this setting.
     *  * Choosing DictionaryNormalization::none will just leave the dictionary as it is. No
     *    additional normalization it performed.
     *  * Choosing DictionaryNormalization::fixed will divide all atoms by the same factor. The
     *    factor is the norm of the first high resolution atom. This preserves the ratios between the
     *    norms of any two atoms.
     *  * Choosing DictionaryNormalization::pairwise will divide each atom pair (high and low
     *    resolution atom) by the larger norm of both. So the ratio between their norms will be
     *    preserved by this setting, but not the ratios between the norms of atoms from different
     *    pairs.
     *  * Choosing DictionaryNormalization::independent will divide each atom in each of the both
     *    resolutions by its respective norm. So every atom will have a norm of 1 with this.
     *
     * In practice a test has shown that normalizing the samples with their separate standard
     * deviation and using DictionaryNormalization::independent is the best combination. However
     * results may vary in other situations.
     *
     * @see getDictionaryInitNormalization(), setDivideNormalizationFactor()
     */
    void setDictionaryInitNormalization(DictionaryNormalization normalization = DictionaryNormalization::independent) {
        dictInit = normalization;
    }

    /**
     * @brief Current setting for the normalization of the dictionary at initialization
     * @return current setting
     * @see setDictionaryInitNormalization()
     */
    DictionaryNormalization getDictionaryInitNormalization() const {
        return dictInit;
    }


protected:
    // How to handle an existing dictionary on next prediction? Throw away (clear), improve or just use it as it is?
    ExistingDictionaryHandling dictHandling = ExistingDictionaryHandling::clear;

    // Sampling strategy: just random, most variance
    SamplingStrategy sampStrat = SamplingStrategy::variance;

    // invalid pixels percentage tolerance
    double maskInvalidTol = 0.15;

    // stopping criterion
    TrainingStopFunction  stopFun               = TrainingStopFunction::objective;
    TrainingResolution    stopFunRes            = TrainingResolution::low;  // only used if stopFun == TrainingStopFunction::objective or objective_maxtau
    TrainingStopCondition stopCon               = TrainingStopCondition::change_less;
    double                stopVal               = 1e-10;
    unsigned int          stopNumberTestSamples = 4000; // only used if stopFun == TrainingStopFunction::test_set_error or useBestShot == BestShotErrorSet::test_set

    // save the best dictionary and test set errors within one training call and use the best one afterwards?
    BestShotErrorSet useBestShot = BestShotErrorSet::train_set;

    // tolerance of weights difference (delta = 0.2 in the paper)
    double weightTol = 0.2;

    // use improved build-up index to calculate weights? Otherwise aad is used. This affects multi-channel (nchannels >= 3) only, and in addition should only be used with red, NIR and SWIR components.
    bool useBuildUpIndexWeights = false;
    std::array<unsigned int, 3> red_nir_swir_order = {0, 1, 2};

    // BU threshold [-1, 1]
    double buThreshold = 0;

    // In K-SVD: use online mode (use updated columns and coefficients directly) or block mode (update columns afterwards and do not use updated coefficients)
    bool useKSVDOnline = true;

    // to get the sparse representation before K-SVD, which resolution should be used for samples and dictionary?
    TrainingResolution sparseRepresentationCoefficientResolution = TrainingResolution::low;

    // to get the sparse representation in K-SVD, which resolution should be used for sample and dictionary? When not using online training, high, low and average behave the same.
    TrainingResolution columnUpdateCoefficientResolution         = TrainingResolution::low;

    // patchSize x patchSize gives the dimension (e. g. 49 in the paper)
    unsigned int patchSize = 7;

    // pixels on each side that should overlap (e. g. 2 in the paper)
    unsigned int patchOverlap = 2;

    // number of training samples (e. g. 2000 in the paper)
    unsigned int numberTrainingSamples = 2000;

    // should the mean value of difference image be subtracted from patch matrix? And then divided by the std dev or variance?
    SampleNormalization subtractMeanValue         = SampleNormalization::none;
    SampleNormalization divideNormalizationFactor = SampleNormalization::separate;
    bool stdDevAsSampleNormalization = true; // false means variance is used (only, used if divideNormalizationFactor != none)

    // number of atoms in the dictionary (e. g. 256 in the paper)
    unsigned int dictSize = 256;

    // how to scale the dictionary at initialization?
    DictionaryNormalization dictInit = DictionaryNormalization::independent;

    // scale dictionary or coefficients with singular values and how (for concatenated probably meaningless):
    // * none (scale dict direct):     multiply the atoms separately with singular values
    // * fixed:                        multiply the atoms separately with singular values and divide by a fixed factor (norm of first atom)
    // * independent (scale coeffs):   multiply the coefficients with one or two singular values
    // * pairwise (scale dict normal): multiply the atoms separately with singular values and divide by the larger singular values (for columnUpdateCoefficientResolution == concatenated the same as independent)
    DictionaryNormalization scaleDictInsteadCoeff = DictionaryNormalization::independent;

    // max number of iterations for the dictionary learning (J in the paper)
    unsigned int minTrainIter = 10; // wild guess, no value given in the paper

    // max number of iterations for the dictionary learning (J in the paper)
    unsigned int maxTrainIter = 20; // wild guess, no value given in the paper

    // GPSR algorithm options
    GPSROptions gpsrOptsTraining;
    GPSROptions gpsrOptsReconstruction;

    // dates for the input pairs
    bool isDate1Set = false;
    int date1 = 0;
    bool isDate3Set = false;
    int date3 = 0;

    // resolution tags
    std::string highTag;
    std::string lowTag;

    void checkGPSROptions(GPSROptions const& o) {
        if (o.tolA < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("The tolerance of the main loop of the GPSR algorithm is set to a negative number (" + std::to_string(o.tolA) + "). It must be non-negative"));

        if (o.tolD < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("The tolerance of the debias loop of the GPSR algorithm is set to a negative number (" + std::to_string(o.tolD) + "). It must be non-negative"));
    }

    friend class SpstfmFusor;
};


/**
 * @brief Implementation details of SPSTFM -- not to be used by library users
 *
 * This namespace defines the dictionary trainer and a lot of helper functions.
 */
namespace spstfm_impl_detail {

/**
 * @brief Trains and holds dictionaries and reconstructs from them
 *
 * This helper class is the work horse of SpstfmFusor. It initializes and holds the most important
 * part of SPSTFM; the dictionaries. It also initializes and holds the weights for reconstruction,
 * mean values and standard deviations (or variances, respectively). The training and
 * reconstruction are the most important parts of SPSTFM and both done in the DictTrainer. However,
 * the usage is not as easy as with normal, public, user accessible classes. Some states must be
 * set in a specific order, which helps to keep the implementation a bit easier.
 *
 * Before the training it has to be initialized. This means the sampleMask has to be set, the
 * dictionary storage dictsConcat has to be resized to the correct number of channels (one element
 * per channel) if required and the statistics (meanForHighDiff_cv, meanForLowDiff_cv,
 * normFactorsHighDiff, normFactorsLowDiff, meansOfHighDiff, meansOfLowDiff) have to be set. Also
 * the SPSTFM options opt must be set. Only then the training samples and maybe validation samples
 * can be aquired via getSamples(). These samples are not saved in the DictTrainer though, since
 * they are only required temporarily. With the training samples the dictionaries can be
 * initialized via initDictsFromSamples(). Finally, both the training samples and the validation
 * samples are used for the training via train(). The sampling, initialization of the dictionary
 * and training must be done for each channel separately. The whole training procedure is
 * controlled from SpstfmFusor::train().
 *
 * For the prediction the training of the dictionaries has to be done and then these can be used
 * for reconstruction (prediction). This is what SpstfmFusor::predict() controls and hence it calls
 * SpstfmFusor::train() before doing the reconstruction. For the reconstruction the output should
 * be set to a shared copy of the real output and the reconstruction weights should be initialized
 * via initWeights(). This can be either done for all channels, when using the build-up index or
 * for a single channel, right before the reconstruction for this channel begins via
 * reconstructImage().
 */
struct DictTrainer {
    /**
     * @brief Dictionary storage
     *
     * It saves one concatenated dictionary matrix for each channel. A concatenated matrix consists
     * of the high resolution dictionary in the head rows and the corresponding low resolution
     * dictionary in the tail rows.
     */
    std::vector<arma::mat> dictsConcat;
//    std::vector<unsigned int> trainedAtoms;


    // weights
    /**
     * @brief Temporary storage for weights from date 1
     *
     * This is set in initWeights() and used in reconstructPatchRow(), which gets called from
     * reconstructImage(). If the weights should be different for each channel, this has to be
     * overwritten right before calling reconstructImage().
     */
    arma::mat weights1;

    /**
     * @brief Temporary storage for weights from date 3
     *
     * \copydetails weights1
     */
    arma::mat weights3;


    // normalization values
    /**
     * @brief Mean value used for normalization of high resolution samples
     *
     * The value depends on SpstfmOptions::getSubtractMeanUsage(). It is set in
     * SpstfmFuser::train() and used in SpstfmFuser::predict() and other places. With this variable
     * it has only to be computed once.
     */
    std::vector<double> meanForHighDiff_cv = std::vector<double>(25, 0.0); // default values for test functions, which do not call SpstfmFusor::train() or SpstfmFusor::predict()

    /**
     * @brief Mean value used for normalization of low resolution samples
     * \copydetails meanForHighDiff_cv
     */
    std::vector<double> meanForLowDiff_cv  = std::vector<double>(25, 0.0);

    /**
     * @brief Normalization factor used for high resolution samples after subtraction of mean value
     *
     * The value depends on SpstfmOptions::getDivideNormalizationFactor(). It is set in
     * SpstfmFuser::train() and used in SpstfmFuser::predict() and other places. With this variable
     * it has only to be computed once.
     */
    std::vector<double> normFactorsHighDiff = std::vector<double>(25, 1.0);

    /**
     * @brief Normalization factor used for high resolution samples after subtraction of mean value
     * \copydetails normFactorsHighDiff
     */
    std::vector<double> normFactorsLowDiff  = std::vector<double>(25, 1.0);


    // fill values for invalid pixels in sampling
    /**
     * @brief Mean value of valid high resolution diff pixels used as fill value for invalid pixels
     *
     * It is set in SpstfmFuser::train() and used in SpstfmFuser::predict() and other places. With
     * this variable it has only to be computed once.
     */
    std::vector<double> meansOfHighDiff = std::vector<double>(25, 0.0);

    /**
     * @brief Mean value of valid low resolution diff pixels used as fill value for invalid pixels
     *
     * \copydetails meansOfHighDiff
     */
    std::vector<double> meansOfLowDiff  = std::vector<double>(25, 0.0);

    /// This must have the same size as the source images and have only trues where sampling is allowed
    Image sampleMask;

    /// This must have the same size as the source images and have only trues where prediction is desired
    ConstImage writeMask;

    /// Output image for reconstructImage(). Can be a shared copy.
    Image output;

    /// The SPSTFM options
    SpstfmOptions opt;

    /**
     * @brief Initialize the weights for reconstruction
     *
     * @param high1 is the high resolution image at date 1.
     * @param high3 is the high resolution image at date 3.
     * @param low1 is the high resolution image at date 1.
     * @param low2 is the high resolution image at date 2.
     * @param low3 is the high resolution image at date 3.
     *
     * @param sampleArea is the full sample area, which includes the prediction area, but is maybe
     * extended to have full patches. This may be out of of the image bounds and is usually
     * returned by calcRequiredArea().
     *
     * @param channels if a single channel is given, this will make weights from the difference of
     * low1 and low2 and of low3 and low2, respectively. If three channels are given, these are
     * used for getting the build-up index and then the number of changed pixels are used for
     * calculating the weights. The latter should be used for all channels, while the
     * single-channel weights should be made freshly for every channel before calling
     * reconstructImage().
     *
     * So this initializes weights1 and weights3. Their size corresponds to the number of patches,
     * since there is one weight for each patch from date 1 and one from date 3.
     */
    void initWeights(ConstImage const& high1, ConstImage const& high3, ConstImage const& low1, ConstImage const& low2, ConstImage const& low3, Rectangle sampleArea, std::vector<unsigned int> const& channels = {});

    /**
     * @brief Sample the difference images for training and validation data
     * @param highDiff is the high resolution difference images.
     * @param lowDiff is the low resolution difference images.
     *
     * @param sampleArea is the full sample area, which includes the prediction area, but is maybe
     * extended to have full patches. This may be out of of the image bounds and is usually
     * returned by calcRequiredArea().
     *
     * @param channel to sample.
     *
     * @return training samples and validation samples. Each as concatenated matrix with the high
     * resolution part in the head rows and the low resolution part in the tail rows.
     */
    std::pair<arma::mat, arma::mat> getSamples(ConstImage const& highDiff, ConstImage const& lowDiff, Rectangle sampleArea, unsigned int channel) const;

    /**
     * @brief Initialize the dictionaries from training samples
     * @param samplesConcat are the training samples from getSamples()
     * @param channel to initialize the dicitonary
     */
    void initDictsFromSamples(arma::mat const& samplesConcat, unsigned int channel);

    /**
     * @brief Train the dictionaries
     * @param samplesConcat are the training samples
     * @param validationSamplesConcat are the validation samples or is empty.
     * @param channel to train.
     *
     * The training of the dictionaries is the main contribution of the SPSTFM. It includes finding
     * sparse coefficients with the GPSR algorithm and improving the dictionary with the K-SVD
     * algorithm.
     */
    void train(arma::mat& samplesConcat, arma::mat& validationSamplesConcat, unsigned int channel); // Algorithm 1

    // reconstruct patch-wise to save memory
    /**
     * @brief Reconstruct one row of patches
     * @param high1 is the high resolution image at date 1.
     * @param high3 is the high resolution image at date 3.
     * @param low1 is the low resolution image at date 1.
     * @param low2 is the low resolution image at date 2.
     * @param low3 is the low resolution image at date 3.
     * @param fillL21 is the mean difference from low1 to low2.
     * @param fillL23 is the mean difference from low3 to low2.
     * @param pyi patch y index (patch row) to reconstruct.
     *
     * @param sampleArea is the full sample area, which includes the prediction area, but is maybe
     * extended to have full patches. This may be out of of the image bounds and is usually
     * returned by calcRequiredArea().
     *
     * @param channel to reconstruct.
     *
     * This will sample difference patches from dates 1 and 3 to date 2 from the low resolution
     * image the source images, find sparse representation coefficients with respect to the trained
     * dictionary pair and use the corresponding high resolution representation to add to the high
     * resolution patches from dates 1 and 3, respectively. Invalid values will be replaced with
     * `fillL21` or `fillL23` before finding sparse coefficients to minimize their influence.
     * Normalization is applied inbetween according to the options.
     *
     * @return one row of patches.
     */
    std::vector<arma::mat> reconstructPatchRow(ConstImage const& high1, ConstImage const& high3, ConstImage const& low1, ConstImage const& low2, ConstImage const& low3, double fillL21, double fillL23, unsigned int pyi, imagefusion::Rectangle sampleArea, unsigned int channel) const;

    /**
     * @brief Average and output one row of patches
     *
     * @param topPatches the upper patch row. In case of `pyi == 0` (very first row) it should be
     * empty.
     *
     * @param[in,out] bottomPatches the lower patch row. In case of `pyi == 0` (very first row) it
     * must contain the patches of the very first row. The bottom patches will be modified in the
     * overlapping region, but not the bottom. So the last opt.getPatchOverlap() rows will not get
     * changed, except if `pyi == npy - 1`.
     *
     * @param pyi patch y index (patch row) to average.
     *
     * @param crop is the rectangle where the output is made. This is basically the prediction
     * area, but as origin it uses the sample area (with maybe negative origin coordinates),
     * because the sample area is the area where the patches are aligned to.
     *
     * @param npx is the number of patches in x direction.
     *
     * @param npy is the number of patches in y direction.
     *
     * @param channel to reconstruct.
     *
     * Generally patches are overlapping in the SPSTFM algorithm. This method takes two
     * neighbouring rows of patches denoted by `bottomPatches` and `topPatches` and averages and
     * output a part of them to `output`. The part that is averaged is usually the first
     * `opt.getPatchSize() - opt.getPatchOverlap()` rows of the `bottomPatches`, see the following
     * figure, which assumes `pyi == 1`.
     * @image html averaging_overlap.png
     * The case `pyi == npy - 1` is an exception, since there the lower border is also averaged.
     * Note, the averaged region will be modified in the `bottomPatches`
     */
    void outputAveragedPatchRow(std::vector<arma::mat> const& topPatches, std::vector<arma::mat>& bottomPatches, unsigned int pyi, Rectangle crop, unsigned int npx, unsigned int npy, unsigned int channel);

    /**
     * @brief Reconstruct the image from the source images and the trained dictionary pair
     * @param high1 is the high resolution image at date 1.
     * @param high3 is the high resolution image at date 3.
     * @param low1 is the low resolution image at date 1.
     * @param low2 is the low resolution image at date 2.
     * @param low3 is the low resolution image at date 3.
     * @param fillL21 is the mean difference from low1 to low2.
     * @param fillL23 is the mean difference from low3 to low2.
     * @param predArea is the prediction area.
     * @param sampleArea is the sampling area, which expands the prediction area to full patches
     * (and may be out of bounds).
     * @param channel to reconstruct for.
     *
     * This will use the source images and the trained dictionaries to reconstruct the output
     * image. Invalid values will be replaced with `fillL21` or `fillL23` before finding sparse
     * coefficients to minimize the influence of invalid pixels. This method relies on
     * reconstructPatchRow() and outputAveragedPatchRow() to do their job.
     */
    void reconstructImage(ConstImage const& high1, ConstImage const& high3, ConstImage const& low1, ConstImage const& low2, ConstImage const& low3, double fillL21, double fillL23, Rectangle predArea, Rectangle sampleArea, unsigned int channel);


    // stats
    /// Debug data storage. See, SpstfmOptions::dbg_recordTrainingStopFunctions.
    std::vector<double> dbg_objective;
    /// \copydoc dbg_objective
    std::vector<double> dbg_objectiveMaxTau;
    /// \copydoc dbg_objective
    std::vector<double> dbg_testSetError;
    /// \copydoc dbg_objective
    std::vector<double> dbg_trainSetError;
};

} /* namespace spstfm_impl_detail */


/**
 * @brief The SpstfmFusor class is the implementation of the SPSTFM algorithm
 *
 * SPSTFM is a dictionary learning based algorithm, which is computationally expensive in training
 * and application, but can give good quality predictions.
 *
 * For SPSTFM five images required. For simplicity the dates are numbered 1, 2, 3 and then the
 * image to predict is the high resolution image at date 2, see the following table:
 * <table>
 * <caption id="spstfm_image_structure">SPSTFM image structure</caption>
 *   <tr><td><span style="display:inline-block; width: 5em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">1</td>
 *       <td align="center" valign="top">2</td>
 *       <td align="center" valign="top">3</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center"><table><tr><th>High 1</th></tr></table></td>
 *       <td align="center"><table><tr><td bgcolor="red"><b>High 2</b></td></tr></table></td>
 *       <td align="center"><table><tr><th>High 3</th></tr></table></td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center"><table><tr><th>Low 1</th></tr></table></td>
 *       <td align="center"><table><tr><th>Low 2</th></tr></table></td>
 *       <td align="center"><table><tr><th>Low 3</th></tr></table></td>
 *   </tr>
 * </table>
 *
 * Basically SPSTFM uses the difference images from High 1 to High 3 and from Low 1 to Low 3 for
 * the training. It works with patches (7x7 by default), which are saved as columns in a training
 * sample matrix pair (one matrix for high resolution and one for low). From these matrices a
 * dictionary pair is initialized, which contrains some training patches as its atoms in the
 * beginning. The dictionaries are overcomplete, i.e. they have more atoms than the dimension is
 * large. This allows to find sparse representation vectors of coefficients with the GPSR
 * algorithm, that, when multiplied to the dictionary, yield the samples approximately. The
 * training process tries to optimize the dictionary such that all samples can be represented with
 * high accuracy and very sparse. The training works iteratively and can be separated in two steps:
 *  1. Find representation coefficients for all training samples using the GPSR algorithm.
 *  2. Update all dictionary atoms using the K-SVD algorithm.
 *
 * When the training stops there is a dictionary pair to represent difference patches in high *and*
 * low resolution with the *same* sparse coefficients. For that only the images at dates 1 and 3
 * have been used. Now to predict the image High 2 Low 2. is required. For that the difference from
 * Low 1 to Low 2 is used. For each patch of that difference image coefficients are found using
 * GPSR again to represent that patch with the low resolution dictionary. Since the same
 * coefficients can be used to represent the corresponding high resolution patch with the high
 * resolution dictionary, just that is done to predict the difference from High 1 to High 2, which
 * is added to High 1 to yield High 2. However, the same can be done from date 3 instead of date 1.
 * So there are two different solutions for each patch and a weighting method is used to get the
 * best of both.
 *
 * For a detailed explanation how SPSTFM works there is the original paper and the thesis, which
 * yielded this implementation. The latter explains also all available options and shows some test
 * results. However, the default options should give good results.
 *
 * From the code perspective, please note that Parallelizer<SpstfmFusor> will not work and give a
 * compile error. The parallelization of SpstfmFusor is done on a micro level (matrix operations).
 * This requires to use an appropriate BLAS library as OpenBLAS, though.
 */
class SpstfmFusor : public DataFusor {
public:
    /**
     * @brief This declares which option type to use
     *
     * Ususally this is done for Parallelizer to allow to default the AlgOpt template argument, but
     * since Spstfm does not work with Parallelizer this is not really required currently and just
     * declared for consistency.
     */
    using options_type = SpstfmOptions;

    /**
     * @brief Process the SPSTFM Options
     * @param o is an options object of type SpstfmOptions and replaces the current options object.
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
     * also \ref spstfm_image_structure "SPSTFM image structure".
     *
     * @param validMask is either empty or a mask in the size of the source images. It can be
     * single-channel or multi-channel. Locations with zero values are not used at all and the
     * result of the @ref outputImage() is undefined at these locations. If the argument is an
     * empty image, all locations will be considered as valid.
     *
     * @param predMask is either empty or a single-channel mask in the size of the source images.
     * It specifies the locations that should be predicted (255) and the locations that should not
     * be predicted (0). However, since prediction is done in patches, only if all pixels in a
     * `predMask`-patch are 0, prediction for that patch is skipped.
     *
     * Calling this method will do everything that is required for prediction. This includes
     * initialization of training data and dictionary, the training process itself and of course
     * the reconstruction (prediction).
     *
     * @throws logic_error if source images have not been set.
     * @throws not_found_error if not all required images are available.
     * @throws image_type_error if the types (basetypes or channels) of images or masks mismatch
     * @throws size_error if the sizes of images or masks mismatch
     */
    void predict(int date2, ConstImage const& validMask = {}, ConstImage const& predMask = {}) override;

    /**
     * @brief Train the dictionary-pair only, without reconstructing afterwards
     *
     * @param validMask is either empty or a mask in the size of the source images. It can be
     * single-channel or multi-channel. Locations with zero values are not used at all and the
     * result of the @ref outputImage() is undefined at these locations. If the argument is an
     * empty image, all locations will be considered as valid.
     *
     * @param predMask is either empty or a single-channel mask in the size of the source images.
     * It specifies the locations that should be predicted (255) and the locations that should not
     * be predicted (0). However, since prediction is done in patches, only if all pixels in a
     * `predMask`-patch are 0, prediction for that patch is skipped.
     *
     * This will only perform the training of the dictionary using the difference of the input
     * image pair. Then, the dictionary can be saved to a file with getDictionary() and
     * [save()](http://arma.sourceforge.net/docs.html#save_load_mat) or used for reconstruction
     * with predict() combined with the option SpstfmOptions::ExistingDictionaryHandling::use in
     * SpstfmOptions::setDictionaryReuse() to avoid clearing or improving the dictionary.
     *
     * @throws logic_error if source images have not been set.
     * @throws not_found_error if not all required images are available.
     * @throws image_type_error if the types (basetypes or channels) of images or masks mismatch
     * @throws size_error if the sizes of images or masks mismatch
     */
    void train(ConstImage const& validMask = ConstImage{}, ConstImage const& predMask = ConstImage{});

    /**
     * @brief Get the dictionary-pair as concatenated dictionary
     *
     * @param channel specifies which dictionary should be returned. In case of multi-channel
     * images, each image channel has its own dictionary.
     *
     * This returns the dictionary-pair as concatenated dictionary. If you need the high or low
     * resolution part, use the heading or trailing half, of the matrix, or use
     * spstfm_impl_detail::highMatView() or spstfm_impl_detail::lowMatView().
     *
     * To save the dictionary to a file, just use
     * [arma::save()](http://arma.sourceforge.net/docs.html#save_load_mat), like
     * @code
     * df.getDictionary().save("trained_dict");
     * @endcode
     *
     * @return the dictionary in concatenated format.
     */
    arma::mat const& getDictionary(unsigned int channel = 0) const {
        return t.dictsConcat.at(channel);
    }

    /**
     * @brief Set the dictionary to the specified one
     *
     * @param dict is the new concatenated dictionary (i. e. a block matrix with the high
     * resolution dictionary in the top and the low resolution dictionary in the bottom).
     *
     * @param channel specifies the image channel to which the dictionary belongs to. Each channel
     * has its own dictionary-pair, since the different channels can vary a lot. Obviously, this is
     * only interesting for multi-channel images.
     *
     * This overwrites a maybe existing dictionary by the specified one. A common use case is to
     * load a pre-trained dictionary from a file, like
     * @code
     * arma::mat dict;
     * dict.load("trained_dict");
     * df.setDictionary(dict);
     * @endcode
     * See [arma::load()](http://arma.sourceforge.net/docs.html#save_load_mat).
     */
    void setDictionary(arma::mat dict, unsigned int channel = 0) {
        if (t.dictsConcat.size() < channel + 1)
            t.dictsConcat.resize(channel + 1);
        t.dictsConcat.at(channel) = std::move(dict);
    }

    /// Get objective function values (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double> const& getDbgObjective()       const { return t.dbg_objective; }
    /// Get scalar tau objective function values (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double> const& getDbgObjectiveMaxTau() const { return t.dbg_objectiveMaxTau; }
    /// Get test set errors (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double> const& getDbgTestSetError()    const { return t.dbg_testSetError; }
    /// Get train set errors (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double> const& getDbgTrainSetError()   const { return t.dbg_trainSetError; }

    /// Get objective function values (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double>& getDbgObjective()       { return t.dbg_objective; }
    /// Get scalar tau objective function values (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double>& getDbgObjectiveMaxTau() { return t.dbg_objectiveMaxTau; }
    /// Get test set errors (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double>& getDbgTestSetError()    { return t.dbg_testSetError; }
    /// Get train set errors (requires `SpstfmOptions::dbg_recordTrainingStopFunctions == true`)
    std::vector<double>& getDbgTrainSetError()   { return t.dbg_trainSetError; }

protected:
    /// SpstfmOptions to use for the next prediction
    options_type opt;

    /**
     * @brief This is the work horse for SPSTFM
     *
     * The DictTrainer object is the object that does most of the work for the SPSTFM algorithm. It
     * controls the training and reconstruction and uses some additional functions like K-SVD or
     * GPSR for this. The DictTrainer as well as the additional helper function are implementation
     * details and thus only documented in the internal documentation, which can be made with
     *
     *     make docinternal
     * They are specified in the sub-namespace `spstfm_impl_detail`.
     */
    spstfm_impl_detail::DictTrainer t;

private:
    void checkInputImages(ConstImage const& validMask, ConstImage const& predMask, int date2 = 0, bool useDate2 = false) const;
};

/// \cond
#ifdef _OPENMP
namespace spstfm_impl_detail {
template <typename... T>
struct specialization_not_allowed : std::false_type
{ };
} /* namespace spstfm_impl_detail */

/**
 * @brief Forbid to use SpstfmFusor with Parallelizer.
 *
 * Since parallelization on a macro level would require to parallelize specific parts explicitly
 * (mainly for loops for GPSR algorithms) and let others run in serial the implementation would be
 * different to what Parallelizer automatically does. However, SpstfmFusor itself should already
 * run in parallel on a micro level. This is because armadillo is used for matrix operations and
 * armadillo uses BLAS and choosing open BLAS makes the matrix operations run in parallel.
 * Therefore there is no need in additional parallelization.
 */
template <class AlgOpt>
class Parallelizer<SpstfmFusor, AlgOpt> {
    static_assert(spstfm_impl_detail::specialization_not_allowed<AlgOpt>::value, "Parallelizer not supported for SpstfmFusor. Armadillo and underlying BLAS library should make the code run in parallel (when using OpenBLAS) automatically.");
};
#endif /* _OPENMP */
/// \endcond


// More implementation details
namespace spstfm_impl_detail {

/**
 * @brief Copy a Armadillo matrix to a spcified channel of an Image
 *
 * @tparam T is the C++ data type, like `uint8_t`, that will be used to write into the Image.
 * @tparam mattype arma matrix type with double elements as arma::mat or some submatrix view.
 * @param from is the single-channel source matrix.
 * @param to is the multi-channel destination image.
 * @param channel is the channel to write to.
 *
 * This copies the whole matrix `from` to the Image `to`, which might be larger. This function is
 * only called from CopyFromToFunctor and never directly.
 */
template<typename T, typename mattype> // TODO: protect signed integer from underflow (< 0)
inline void copy(mattype const& from, imagefusion::Image& to, unsigned int channel) {
    const int rows = from.n_rows;
    const int cols = from.n_cols;
    assert(rows <= to.height());
    assert(cols <= to.width());
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            to.at<T>(x, y, channel) = cv::saturate_cast<T>(from(y, x));
}

/**
 * @brief Copy a Armadillo matrix to a spcified channel of an Image
 *
 * @tparam mattype arma matrix type with double elements as arma::mat or some submatrix view.
 * @param from is the single-channel source matrix.
 * @param to is the multi-channel destination image.
 * @param channel is the channel to write to.
 *
 * This copies the whole matrix `from` to the Image `to`, which might be larger. This functor is
 * only used in DictTrainer::outputAveragedPatchRow to help to write an averaged patch into the
 * output Image.
 */
template<typename mattype>
struct CopyFromToFunctor {
    mattype const& from;
    imagefusion::Image& to;
    unsigned int channel;

    template<imagefusion::Type imfutype>
    void operator()() {
        using type = typename imagefusion::DataType<imfutype>::base_type;
        copy<type>(from, to, channel);
    }
};

/**
 * @brief Extract a patch from a source image as Armadillo matrix (column vector)
 *
 * @param src is the source Image from which the patch is extracted.
 * @param p0 is the top-left point of the patch to extract.
 * @param patchSize is the patch size, so `patchSize == 7` would extract a 7 by 7 patch.
 * @param channel is the channel to use from `src`.
 *
 * This samples a patch from the source image and copies the values to an armadillo matrix of
 * double type. The matrix will be a column vector of size `patchSize` · `patchSize`. The
 * coordinates may be out of bounds in which case a mirror bound will be applied.
 *
 * @return patch as column vector
 */
struct GetPatchFunctor {
    imagefusion::ConstImage const& src;
    imagefusion::Point p0;
    unsigned int patchSize;
    unsigned int channel;

    template<imagefusion::Type imfutype>
    arma::mat operator()() {
        static_assert(getChannels(imfutype) == 1, "GetPatchFunctor can only be used as base type functor."); // full type functors have been removed anyways
        if (p0.x < -src.width()  || p0.x + (int)patchSize - 1 >= 2 * src.width()  ||
            p0.y < -src.height() || p0.y + (int)patchSize - 1 >= 2 * src.height()   )
            IF_THROW_EXCEPTION(size_error(
                    "Coordinate out of bounds. You were trying to get a patch at "
                    "(" + std::to_string(p0.x) + ", " + std::to_string(p0.y) + ") to "
                    "(" + std::to_string(p0.x+patchSize-1) + ", " + std::to_string(p0.y+patchSize-1) + ") "
                    "from an image of size " + to_string(src.size()) + ". There are mirror boundaries "
                    "but only once, so acceptable were anything fully inside "
                    "(" + std::to_string(-src.width()) + ", " + std::to_string(-src.height()) + ") to "
                    "(" + std::to_string(2*src.width() - 1) + ", " + std::to_string(2*src.height()-1) + ")."))
                    << errinfo_size(src.size());

        using srcbasetype = typename imagefusion::BaseType<imfutype>::base_type;
        arma::mat dst(patchSize * patchSize, 1);
        auto it_dst = dst.begin();
        for (int y = p0.y; y < p0.y + (int)patchSize; ++y) {
            for (int x = p0.x; x < p0.x + (int)patchSize; ++x) {
                // mirror coordinates if required
                int xs = x;
                int ys = y;
                if (xs < 0)
                    xs = -xs - 1;
                if (ys < 0)
                    ys = -ys - 1;
                if (xs >= src.width())
                    xs = 2 * src.width()  - 1 - xs;
                if (ys >= src.height())
                    ys = 2 * src.height() - 1 - ys;

                // copy patch
                *it_dst = static_cast<double>(src.at<srcbasetype>(xs, ys, channel));
                ++it_dst;
            }
        }
        return dst;
    }
};

/**
 * @brief Extract a patch from a source image as Armadillo matrix (column vector)
 *
 * @param img is the source Image from which the patch is extracted.
 * @param pxi is the patch x index (patch column).
 * @param pyi is the patch y index (patch row).
 * @param patchSize is the patch size, so `patchSize == 7` would extract a 7 by 7 patch.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is used to as origin for patch (0, 0).
 * @param channel is the channel to use from `src`.
 *
 * This samples a patch from the source image and copies the values to an armadillo matrix of
 * double type. The matrix will be a column vector of size `patchSize`². The coordinates may be out
 * of bounds in which case a mirror bound will be applied.
 *
 * @return patch as column vector
 */
arma::mat extractPatch(imagefusion::ConstImage const& img, int pxi, int pyi, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel = 0);

/**
 * @brief Extract a patch from the diff of two source images as Armadillo matrix (column vector)
 *
 * @param img1 is the first source Image
 * @param img2 is the first source Image
 * @param pxi is the patch x index (patch column).
 * @param pyi is the patch y index (patch row).
 * @param patchSize is the patch size, so `patchSize == 7` would extract a 7 by 7 patch.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is used to as origin for patch (0, 0).
 * @param channel is the channel to use from `src`.
 *
 * This samples a patch from the `img1 - img2` and copies the values to an armadillo matrix of
 * double type. The matrix will be a column vector of size `patchSize`². The coordinates may be out
 * of bounds in which case a mirror bound will be applied.
 *
 * @return patch as column vector
 */
arma::mat extractPatch(imagefusion::ConstImage const& img1, imagefusion::ConstImage const& img2, int pxi, int pyi, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel = 0);

/**
 * @brief Calculate the rectangle that is required to have an area of full patches that cover the prediction area
 * @param predArea is the prediction area.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 *
 * This is often referred to as sample area. It includes the prediction area completely, but
 * expands it, if necessary, to cover the area by full patches.
 *
 * So to cover an area of width \f$ w \f$ and height \f$ h \f$ with patch size \f$ s \f$ and patch
 * overlap \f$ o \f$ the following number of patches in each direction is required:
 * \f[ n_x = \left\lfloor \frac{w + o - 1}{s - o} \right\rfloor + 1
 *     \quad\text{and}\quad
 *     n_y = \left\lfloor \frac{h + o - 1}{s - o} \right\rfloor + 1. \f]
 * Then the width and height to have full patches is obviously:
 * \f[ \bar w = n_x \, (s - o) + o
 *     \quad\text{and}\quad
 *     \bar h = n_y \, (s - o) + o. \f]
 * And the origin is just shifted by the half size difference:
 * \f[ \bar x_0 = x_0 - \left\lfloor \frac 1 2 \, (\bar w - w) \right\rfloor
 *     \quad\text{and}\quad
 *     \bar y_0 = y_0 - \left\lfloor \frac 1 2 \, (\bar h - h) \right\rfloor. \f]
 *
 * @return sample area, which might be out of bounds of the actual source image.
 */
imagefusion::Rectangle calcRequiredArea(imagefusion::Rectangle predArea, unsigned int patchSize, unsigned int patchOverlap);

/**
 * @brief GPSR algorithm -- used to find sparse representation coefficients with respect to an overcomplete dictionary
 * @param y is the sample that should be represented by the dictionary the coefficients.
 * @param A is the dictionary.
 * @param opt are the options that control the algorithm (tolerances, etc.).
 * @param tau_out can be a pointer to a double where the tau will be written to.
 *
 * This implements a part of the MATLAB reference implementation GPSR_BB.m from
 * http://www.lx.it.pt/~mtf/GPSR/
 *
 * GPSR-BB stands for Gradient Projection for Sparse Representation and for the variant with the
 * Barzilai-Borwein approach to increase efficiency. It has a few differences compared to the
 * reference implementation though:
 *  - tau is by default 0.1 * max(|A' * y|), but can be set to something else with `opt`.
 *  - StopCriterion is always 1
 *  - Monotone is always 1
 *  - ContinuationSteps is always -1 (only used if continuation is true in `opt`)
 *  - AlphaMin is always 1e-30
 *  - AlphaMax is always 1e30
 *  - Initialization is always 0 (sparse coeffs x initialized with the zero vector)
 *  - it will output the last (maybe debiased) x only (instead of biased coefficients additionally)
 *
 * This is one of the most important parts of the SPSTFM algorithm. It is used to find a sparse
 * representation x for a sample `y` with respect to the overcomplete dictionary `A`, i. e.
 * \f[ \min_x \frac 1 2 \| y - A \, x \|_2^2 + \tau \| x \|_1, \f]
 * where the 1-norm represents an approximation to the original problem (sparsity would be number
 * of non-zeros). The coefficients are used during training as initial state for the dictionary
 * update and for reconstruction.
 *
 * The algorithm is quite expensive and constitutes the major part of the computational cost. So
 * any performance optimization should involve GPSR, either by optimizing it directly, adjusting
 * its parameters, giving it initial vectors to reduce the number of iterations or just decreasing
 * the number of calls to it by caching or so.
 *
 * @return the sparse representation coefficients x of `y` w.r.t. `A`.
 */
arma::vec gpsr(arma::mat const& y, arma::mat const& A, imagefusion::SpstfmOptions::GPSROptions const& opt, double* tau_out = nullptr);

/**
 * @brief Inner iteration of the standard K-SVD algorithm
 * @param k is the column of the dictionary to update.
 * @param samples are the training samples.
 * @param dict is the concatenated dictionary, but is only used as input in block mode (online mode
 * off). Otherwise it is unused.
 * @param new_dict is the output for the updated columns and also the input in online mode.
 * @param coeff are the sparse representation coefficients for the training samples. In online mode
 * these are updated, too.
 * @param useOnlineMode determines whether coefficients and dictionary gets updated while iterating
 * through all dictionary columns.
 * @param singularValueHandling specifies whether the dictionary column should be normed or not.
 * See SpstfmOptions::setDictionaryKSVDNormalization().
 *
 * This should not be called by any other code than ksvd(). It is just one iteration in the
 * `for`-loop of ksvd().
 */
void ksvd_iteration(unsigned int k, arma::mat const& samples, arma::mat const& dict,  arma::mat& new_dict, arma::mat& coeff, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling);

/**
 * @brief Standard K-SVD algorithm to update a concatenated dictionary
 * @param samples are the training samples.
 * @param dict is the concatenated dictionary.
 * @param coeff are the sparse representation coefficients for the training samples. In online mode
 * these are updated, too.
 * @param useOnlineMode determines whether coefficients and dictionary gets updated while iterating
 * through all dictionary columns.
 * @param singularValueHandling specifies whether the dictionary column should be normed or not.
 * See SpstfmOptions::setDictionaryKSVDNormalization().
 *
 * The K-SVD algorithm is responsible for updating the dictionary during training. So together with
 * GPSR this is the other part SPSTFM is based on.
 *
 * However, training does not always improve the outcome, maybe for very practical reasons as bad
 * input images. In test situations with rather simple visible object structures the training is
 * very important and improves the outcome considerably.
 *
 * @return the updated dictionary. Note the coefficients are also updated, when online mode is
 * selected. Usually the coefficients are not used after the dictionary update anymore, but could
 * be valueable for performance speedup in the training procedure.
 */
arma::mat ksvd(arma::mat const& samples, arma::mat const& dict, arma::mat& coeff, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling);

/**
 * @brief Inner iteration of the double K-SVD algorithm
 *
 * @param k is the column of the dictionary to update.
 *
 * @param highSamples are the high resolution training samples.
 *
 * @param highDict is the high resolution dictionary, but is only used as input in block mode
 * (online mode off). Otherwise it is unused.
 *
 * @param highDictNew is the output for the updated high resolution columns and also the input in
 * online mode.
 *
 * @param lowSamples are the low resolution training samples.
 *
 * @param lowDict is the low resolution dictionary, but is only used as input in block mode (online
 * mode off). Otherwise it is unused.
 *
 * @param lowDictNew is the output for the updated low resolution columns and also the input in
 * online mode.
 *
 * @param coeff are the sparse representation coefficients for the training samples. In online mode
 * these are updated, too. The updated value depends on `res`.
 *
 * @param res is the resolution from which the coefficients should be updated.
 *  * low: `coeffs` will get updated with the coefficients of the low resolution SVD.
 *  * high: `coeffs` will get updated with the coefficients of the high resolution SVD.
 *  * average: `coeffs` will get updated with the average of the coefficients of the low resolution
 *    SVD and the coefficients of the high resolution SVD.
 * See also SpstfmOptions::setColumnUpdateCoefficientResolution().
 *
 * @param useOnlineMode determines whether coefficients and dictionary gets updated while iterating
 * through all dictionary columns.
 *
 * @param singularValueHandling specifies whether the dictionary column should be normed or not.
 * See SpstfmOptions::setDictionaryKSVDNormalization().
 *
 * This should not be called by any other code than doubleKSVD(). It is just one iteration in the
 * `for`-loop of doubleKSVD().
 */
void doubleKSVDIteration(unsigned int k,
                         arma::mat const& highSamples, arma::mat const& highDict, arma::mat& highDictNew,
                         arma::mat const& lowSamples,  arma::mat const& lowDict,  arma::mat& lowDictNew,
                         arma::mat& coeff, imagefusion::SpstfmOptions::TrainingResolution res, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling);

/**
 * @brief Double K-SVD algorithm to update a dictionary pair
 *
 * @param highSamples are the high resolution training samples.
 *
 * @param highDict is the high resolution dictionary.
 *
 * @param lowSamples are the low resolution training samples.
 *
 * @param lowDict is the low resolution dictionary.
 *
 * @param coeff are the sparse representation coefficients for the training samples. In online mode
 * these are updated, too. The updated value depends on `res`.
 *
 * @param res is the resolution from which the coefficients should be updated.
 *  * low: `coeffs` will get updated with the coefficients of the low resolution SVD.
 *  * high: `coeffs` will get updated with the coefficients of the high resolution SVD.
 *  * average: `coeffs` will get updated with the average of the coefficients of the low resolution
 *    SVD and the coefficients of the high resolution SVD.
 * See also SpstfmOptions::setColumnUpdateCoefficientResolution().
 *
 * @param useOnlineMode determines whether coefficients and dictionary gets updated while iterating
 * through all dictionary columns.
 *
 * @param singularValueHandling specifies whether the dictionary column should be normed or not.
 * See SpstfmOptions::setDictionaryKSVDNormalization().
 *
 * The K-SVD algorithm is responsible for updating the dictionary during training. So together with
 * GPSR this is the other part SPSTFM is based on. This double K-SVD is an extended K-SVD algorithm
 * for pairs of dictionaries and samples that share the same coefficients. It takes care that the
 * dictionary atoms of both resolutions do not loose their correspondence.
 *
 * However, training does not always improve the outcome, maybe for very practical reasons as bad
 * input images. In test situations with rather simple visible object structures the training is
 * very important and improves the outcome considerably.
 *
 * @return the updated dictionary. Note the coefficients are also updated, when online mode is
 * selected. Usually the coefficients are not used after the dictionary update anymore, but could
 * be valueable for performance speedup in the training procedure.
 */
std::pair<arma::mat,arma::mat> doubleKSVD(arma::mat const& highSamples, arma::mat const& highDict,
                                          arma::mat const& lowSamples,  arma::mat const& lowDict,
                                          arma::mat& coeff, imagefusion::SpstfmOptions::TrainingResolution res, bool useOnlineMode, SpstfmOptions::DictionaryNormalization singularValueHandling);

/**
 * @brief Calculate a simple version of the objective function with a scalar tau
 *
 * @param samples are the training or validation samples of a single resolution or the concatenated
 * samples of both resolutions.
 *
 * @param dict is the dictionary of a single resolution or the concatenated dictionary of both
 * resolutions.
 *
 * @param coeff are the sparse representation coefficients.
 *
 * @param tau is a scalar to weight the l1-norm of the coefficients.
 *
 * @return \f$ (\| P - D \, \Lambda \|_F^2 + \tau \| \Lambda \|_1) \cdot \frac{1}{N \, n} \f$,
 * where N is the number of samples and n is the dimension of a sample (number of elements in a
 * column).
 */
double objective_simple(arma::mat const& samples, arma::mat const& dict, arma::mat const& coeff, double tau);

/**
 * @brief Calculates an improved version of the objective function with the taus corresponding to the coefficients
 *
 * @param samples are the training or validation samples of a single resolution or the concatenated
 * samples of both resolutions.
 *
 * @param dict is the dictionary of a single resolution or the concatenated dictionary of both
 * resolutions.
 *
 * @param coeff are the sparse representation coefficients.
 *
 * @param taus is a vector of tau values that have been used to find the coefficients, see gpsr().
 * The tau values are used to weight the corresponding coefficient vector before taking the l1-norm
 * of it.
 *
 * @return \f$ (\| P - D \, \Lambda \|_F^2 + \| \Lambda \, \diag((\tau_i)) \|_1) \cdot \frac{1}{N
 * \, n} \f$, where N is the number of samples and n is the dimension of a sample (number of
 * elements in a column).
 */
double objective_improved(arma::mat const& samples, arma::mat const& dict, arma::mat const& coeff, std::vector<double> const& taus);

/**
 * @brief Get the high resolution part of the concatenated dictionary matrix
 * @tparam mattype arma matrix type with double elements as arma::mat or some submatrix view.
 * @param m is the concatenated dictionary.
 * @return a submatrix view on the upper half of `m`.
 * @see lowMatView()
 */
template<typename mattype>
inline arma::subview<double> highMatView(mattype& m) {
    return m.rows(0, m.n_rows / 2 - 1); // head_rows
}

/**
 * @brief Get the low resolution part of the concatenated dictionary matrix
 * @tparam mattype arma matrix type with double elements as arma::mat or some submatrix view.
 * @param m is the concatenated dictionary.
 * @return a submatrix view on the lower half of `m`.
 * @see highMatView()
 */
template<typename mattype>
inline arma::subview<double> lowMatView(mattype& m) {
    return m.rows(m.n_rows / 2, m.n_rows - 1); // tail_rows
}

/**
 * @brief Calculate the test set error as alternative to the objective function
 * @param highTestSamples are the high resolution validation samples, denoted
 * by \f$ Q_{\mathrm f} \f$ below.
 * @param lowTestSamples are the low resolution validation samples.
 * @param dictConcat is the concatenated dictionary.
 * @param gpsrOpts are the options for the GPSR algorithm.
 * @param normFactorForHigh is the scaling \f$ s \f$ with which the high resolution samples are
 * normed. It is used afterwards to bring the error in the original data range.
 *
 * This simulates a reconstruction of some validation sample pairs to estimate the quality of the
 * dictionary. So it uses each low resolution sample, finds its coefficients with respect to the
 * low resolution dictionary, applies them to the high resolution dictionary and compares the
 * reconstructed high resolution patch to the corresponding high resolution validation sample with
 * an l1-norm.
 *
 * @return \f$ \|Q_{\mathrm f} - \hat Q_{\mathrm f} \|_1 \cdot \frac{s}{K \, n} \f$, where \f$ \hat
 * Q_{\mathrm f} \f$ are the reconstructed high resolution samples, K is the number of validation
 * samples and n is the dimension of a high (or low) resolution validation sample.
 */
double testSetError(arma::mat const& highTestSamples, arma::mat const& lowTestSamples, arma::mat const& dictConcat, imagefusion::SpstfmOptions::GPSROptions const& gpsrOpts, double normFactorForHigh);


/**
 * @brief Get the indices to sort a vector in descending order
 * @tparam T is the type of elements to sort.
 * @param v is the vector of unsorted elements.
 *
 * This finds the indices to sort v in descending order. For example:
 * @code
 * // element indices:              0, 1, 2, 3, 4, 5
 * for (auto i : sort_indices<int>({5, 3, 4, 0, 2, 1}))
 * @endcode
 * will print "0 2 1 4 5 3". It is used in the mostVariance() functions to return the patch indices
 * sorted by their variance.
 *
 * This has been adapted from http://stackoverflow.com/a/12399290/2414411
 *
 * @return the indices as vector.
 */
template <typename T>
inline std::vector<size_t> sort_indices(std::vector<T> const& v) {
  // initialize original index locations
  std::vector<size_t> idx(v.size());
  std::iota(idx.begin(), idx.end(), 0);

  // sort indexes based on comparing values in v
  std::sort(idx.begin(), idx.end(),
            [&v](size_t i1, size_t i2) { return v[i1] > v[i2]; });
  return idx;
}

/**
 * @brief Get a random vector of unsigned integer
 * @param count is the number of elements
 * @return a shuffled vector with all integer numbers in [0, count).
 */
std::vector<size_t> uniqueRandomVector(unsigned int count);

/**
 * @brief Get the patch indices sorted by variance in the patches in descending order
 * @param img is the image to sample from.
 * @param mask is the mask to exclude invalid pixels. Invalid pixels will not be used to compute
 * the variance.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * This function samples every patch from img and computes its variance (or standard deviation,
 * which does not matter). Then it returns all patch indices sorted by the variance in the patches
 * in descending order. The first samples have the most variance and thus if one requires a fixed
 * number of samples with the most variance, the rest can be cut off.
 *
 * @return patch indices.
 *
 * @note this function seems to be unused in the current version.
 */
std::vector<size_t> mostVariance(imagefusion::ConstImage const& img, imagefusion::ConstImage const& mask, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel);

/**
 * @brief Get the patch indices sorted by variance in the patches in descending order
 * @param img1 is the first image to sample from.
 * @param img2 is the second image to sample from.
 * @param mask is the mask to exclude invalid pixels. Invalid pixels will not be used to compute
 * the variance.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * This function samples every patch from img1 and img2, computes their variances (or standard
 * deviations, which does not matter) and adds them. Then it returns all patch indices sorted by
 * the variance sum in the patches in descending order. The first samples have the most variance
 * and thus if one requires a fixed number of samples with the most variance, the rest can be cut
 * off.
 *
 * @return patch indices.
 */
std::vector<size_t> mostVariance(imagefusion::ConstImage const& img1, imagefusion::ConstImage const& img2, imagefusion::ConstImage const& mask, unsigned int patchSize, unsigned int patchOverlap, Rectangle sampleArea, unsigned int channel);

/**
 * @brief Find duplicates patches
 * @param img is the image to sample from.
 * @param mask is the mask to exclude invalid pixels. These will not be compared to each other and
 * excluded for taking the patch sums.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * This finds duplicate patches. For performance reasons not every patch is compared to every other
 * patch, but only those, with the same sums.
 *
 * @return patch indices of patches that have duplicates.
 */
std::vector<size_t> duplicatesPatches(imagefusion::ConstImage const& img, imagefusion::ConstImage const& mask, unsigned int patchSize, unsigned int patchOverlap, Rectangle sampleArea, unsigned int channel);

/**
 * @brief Get patch indices ordered by random or most variance
 * @param s is the sampling strategy; random or most variance.
 * @param imgHigh high resolution difference image. Used only for most variance strategy.
 * @param imgLow low resolution difference image. Used for most variance strategy and to find
 * duplicate patches.
 * @param mask is the mask to exclude invalid pixels.
 * @param maskInvalidTol is the tolerance that specifies number of invalid pixels in a patch is
 * acceptable, see SpstfmOptions::setInvalidPixelTolerance().
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * This uses
 *  * uniqueRandomVector() for random sampling strategy.
 *  * mostVariance() for most variance sampling strategy.
 *  * duplicatesPatches() to remove low resolution duplicates.
 *  * mostlyInvalidPatches() to exclude patches that have to many invalid pixels.
 * It is called from DictTrainer::getSamples() to get the training samples. For the validation
 * samples DictTrainer::getSamples() uses uniqueRandomVector() and mostlyInvalidPatches() directly.
 *
 * @return patch indices of unique patches with a tolerable number of invalid pixels, ordered with
 * the specified strategy.
 */
std::vector<size_t> getOrderedPatchIndices(imagefusion::SpstfmOptions::SamplingStrategy s, imagefusion::ConstImage const& imgHigh, imagefusion::ConstImage const& imgLow, imagefusion::ConstImage const& mask, double maskInvalidTol, unsigned int patchSize, unsigned int patchOverlap, Rectangle sampleArea, unsigned int channel);

/**
 * @brief Find patches that have too many invalid pixels
 * @param mask is the mask that specifies which pixels are invalid. So this is the image that will
 * be sampled here.
 * @param tol is the relative tolerance that specifies how many invalid pixels are tolerable, see
 * SpstfmOptions::setInvalidPixelTolerance(). It is denoted by \f$ \varepsilon \f$ below.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * @return patch indices with too many invalid pixels n, i. e. \f$ \dfrac n d > \varepsilon. \f$,
 * where d = `patchSize`².
 */
std::vector<size_t> mostlyInvalidPatches(imagefusion::ConstImage const& mask, double tol, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel);

/**
 * @brief Fill a sample matrix with samples
 * @tparam mattype arma matrix type with double elements as arma::mat or some submatrix view for the samples.
 * @param diff is the difference image to sample from.
 * @param samples is the single-resolution sample matrix (half concatenated matrix, see
 * highMatView() and lowMatView()) to fill with samples. It must be initialized to the correct size
 * already.
 * @param mask is the mask to exclude invalid pixels.
 * @param fillVal is the mean value of the difference image and used as fill value for invalid
 * pixels.
 * @param patch_indices are the patch indices of the patches that should get sampled.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * This is a small helper function for samples().
 */
template<typename mattype>
inline void initSingleSamples(imagefusion::ConstImage const& diff, mattype& samples, imagefusion::ConstImage const& mask, double fillVal, std::vector<size_t> const& patch_indices, unsigned int patchSize, unsigned int patchOverlap, imagefusion::Rectangle sampleArea, unsigned int channel) {
    // init samples

    unsigned int dist = patchSize - patchOverlap;
    unsigned int npx = (sampleArea.width  - patchOverlap) / dist;
#ifndef NDEBUG
    unsigned int npy = (sampleArea.height - patchOverlap) / dist;
    unsigned int nsamples = samples.n_cols;
    unsigned int dim = patchSize * patchSize;
#endif
    assert(nsamples == patch_indices.size());
    assert(static_cast<unsigned int>(samples.n_rows) == dim);
    assert(npx * npy >= nsamples);
    unsigned int maskChannel = mask.channels() > channel ? channel : 0;

    for (unsigned int sidx = 0; sidx < patch_indices.size(); ++sidx) {
        size_t pi = patch_indices[sidx];
        unsigned int pyi = pi / npx;
        unsigned int pxi = pi % npx;
        arma::mat diffPatch = extractPatch(diff, pxi, pyi, patchSize, patchOverlap, sampleArea, channel);
        arma::mat maskPatch = extractPatch(mask, pxi, pyi, patchSize, patchOverlap, sampleArea, maskChannel);
        diffPatch.elem(arma::find(maskPatch == 0)).fill(fillVal);
        samples.col(sidx) = diffPatch;
    }
}

/**
 * @brief Get a concatenated sample matrix
 * @param highDiff is the high resolution difference image.
 * @param lowDiff is the low resolution difference image.
 * @param mask is the mask to exclude invalid pixels.
 * @param patch_indices are the patch indices of the patches that should get sampled.
 * @param meanForHigh will be subtracted from the high resolution samples for normalization.
 * @param meanForLow will be subtracted from the low resolution samples for normalization.
 * @param normFactorForHigh is the divisor after the subtraction for normalization of the high
 * resolution samples.
 * @param normFactorForLow is the divisor after the subtraction for normalization of the low
 * resolution samples.
 * @param fillHigh is the mean values of the high resolution difference image and used as fill
 * value for invalid pixels.
 * @param fillLow is the mean values of the low resolution difference image and used as fill value
 * for invalid pixels.
 * @param patchSize is the patch size.
 * @param patchOverlap is the patch overlap.
 * @param sampleArea is the sample area required for sampling.
 * @param channel is the channel to use for sampling.
 *
 * @return a concatenated sample matrix with the high resolution samples in the upper half and the
 * corresponding low resolution samples in the lower half.
 */
arma::mat samples(imagefusion::ConstImage const& highDiff, imagefusion::ConstImage const& lowDiff, imagefusion::ConstImage const& mask, std::vector<size_t> patch_indices, double meanForHigh, double meanForLow, double normFactorForHigh, double normFactorForLow, double fillHigh, double fillLow, unsigned int patchSize, unsigned int patchOverlap, Rectangle sampleArea, unsigned int channel);

/**
 * @brief Debug function to draw a concatenated dictionary to an image file
 * @param dictConcat is the concatenated dictionary.
 * @param filename is the filename of the iamge file to write to.
 */
void drawDictionary(arma::mat const& dictConcat, std::string const& filename);

/**
 * @brief Debug function to draw the reconstruction weights to an image file
 * @param weights is the weights matrix.
 * @param filename is the filename of the iamge file to write to.
 */
void drawWeights(arma::mat const& weights, std::string const& filename);

} /* namespace spstfm_impl_detail */


} /* namespace imagefusion */


#pragma once

#ifndef WITH_OMP
    #error "Sorry, if you want to use ParallelizerOptions, you need to install OpenMP first."
#endif

#include "options.h"

#include <omp.h>

namespace imagefusion {

/**
 * @brief Options for the Parallelizer meta DataFusor
 *
 * The ParallelizerOptions add to the inherited prediction area the number of threads and nested
 * options for the underlying DataFusor algorithm.
 *
 * Note that although the nested options also have a prediction area like every Options sub class,
 * these are ignored and only the prediction area of the ParallelizerOptions are used. However, the
 * nested algorithm options are of course used for all other algorithm specific settings.
 */
template<class AlgOpt>
class ParallelizerOptions : public Options {
public:

    /**
     * @brief Get the number of threads to use
     * @return number of threads
     */
    unsigned int getNumberOfThreads() const;


    /**
     * @brief Set the number of threads to use
     * @param num is the number of threads &le; number of processors
     *
     * The number of threads determines the number of the underlying DataFusor%s, which run in
     * parallel to predict an image. Choosing it greater than
     * [`omp_get_num_procs()`](https://gcc.gnu.org/onlinedocs/libgomp/omp_005fget_005fnum_005fprocs.html)
     * will set it to `omp_get_num_procs()`.
     *
     * By default (on construction) this is set to `omp_get_num_procs()`.
     */
    void setNumberOfThreads(unsigned int num);


    /**
     * @brief Get the nested DataFusor algorithm options object
     * @return nested options
     */
    AlgOpt const& getAlgOptions() const;


    /**
     * @brief Set the nested DataFusor algorithm options
     * @param o are the nested options of the algorithm.
     *
     * When processing the options in the Parallelizer, the prediction area is set to a horizontal
     * stripe according to the DataFusor%s thread number. The remaining options stay as set in `o`.
     */
    void setAlgOptions(AlgOpt const& o);

private:
    unsigned int numberThreads = omp_get_num_procs();
    AlgOpt algOpt;
};




template<class AlgOpt>
inline unsigned int ParallelizerOptions<AlgOpt>::getNumberOfThreads() const {
    return numberThreads;
}


template<class AlgOpt>
inline void ParallelizerOptions<AlgOpt>::setNumberOfThreads(unsigned int num) {
    numberThreads = std::min((int)num, std::max(omp_get_num_procs(), 1));
}


template<class AlgOpt>
inline AlgOpt const& ParallelizerOptions<AlgOpt>::getAlgOptions() const {
    return algOpt;
}


template<class AlgOpt>
inline void ParallelizerOptions<AlgOpt>::setAlgOptions(AlgOpt const& o) {
    algOpt = o;
}



} /* namespace imagefusion */

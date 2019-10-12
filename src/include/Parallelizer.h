#pragma once

#ifndef WITH_OMP
    #error "Sorry, if you want to use Parallelizer, you need to install OpenMP first."
#endif

#include "DataFusor.h"
#include "ParallelizerOptions.h"
#include "Image.h"
#include "MultiResImages.h"
#include "exceptions.h"

#include <omp.h>

#include <cmath>
#include <iostream>
#include <memory>


namespace imagefusion {


/**
 * @brief Meta DataFusor to parallelize a DataFusor
 *
 * This meta `DataFusor` allows simple parallelization of almost arbitrary `DataFusor`s. Assume a
 * default-constructible `DataFusor` called `ExampleFusor` and a corresponding `Options` class
 * `ExampleOptions`, which is available through `ExampleFusor::options_type`. The following example
 * shows how to use `Parallelizer`:
 * @code
 * // declare:
 * //  - std::shared_ptr<MultiResImages> imgs (image store, filled with source images)
 * //  - Rectangle predictionArea             (can be e. g. imgs.getAny().getCropWindow())
 * //  - int predictionDate                   (date to predict)
 *
 * ExampleOptions eOpt;
 * // ... set fusor specific options in eOpt except output tag and prediction area
 *
 * // set options for Parallelizer
 * ParallelizerOptions<ExampleOptions> pOpt;
 * pOpt.setNumberOfThreads(4); // optional, otherwise by default set to omp_get_num_procs()
 * pOpt.setPredictionArea(predictionArea);
 * pOpt.setAlgOptions(eOpt);
 *
 * // execute ExampleFusor in parallel
 * Parallelizer<ExampleFusor> p;
 * p.srcImages(imgs);
 * p.processOptions(pOpt);
 * p.predict(predictionDate);
 *
 * // get predicted image and write to a file
 * Image const& dstImg = p.outputImage();
 * dstImg.write("predicted.tiff");
 * @endcode
 * For that example to work, some requirements for `ExampleFusor` exist. It was assumed to be
 * default-constructible. For `Parallelizer<Alg>` to work, `Alg` needs at least to be
 * copy-constructible. In the case it is not default-constructible, but copy-constructible,
 * `Parallelizer<ExampleFusor>` could be constructed with an instance of `ExampleFusor`, which
 * `Parallelizer<ExampleFusor>` can use as a sample to copy-construct more instances internally:
 * @code
 * // instanciate one sample of ExampleFusor
 * // ExampleFusor sample = ...
 * Parallelizer<ExampleFusor> p{sample};
 * @endcode
 * If making the data fusor copy-constructible is not possible, a meta `DataFusor`, which acts as
 * `Proxy` can be specified. This might not always be easy and can often be avoided. See `Proxy`
 * for a description on how to do that, but even if the data fusion requires the @ref Type of the
 * images at compile time, there might be an alternative solution to enable copy construction of
 * such a `DataFusor`. See for example the following pattern:
 * @code
 * struct FusionFunctor {
 *     // this method does the prediction
 *     template<Type t>
 *     inline void operator()();
 *
 *     // other stuff, like image references, constructor, etc.
 *     ConstImage const& src;
 *     Image& out;
 *     FusionFunctor(ConstImage const& src, Image& out) : src{src}, out{out} { }
 * };
 *
 * class ExampleFusor : public DataFusor {
 * public:
 *     void predict(int date, ConstImage const& mask = ConstImage{}) override {
 *         ConstImage const& src = imgs->get("src", date);
 *         CallBaseTypeFunctor::run(FusionFunctor{src, output}, src.type());
 *     }
 *
 *     // option handling
 *     // ...
 * };
 * @endcode
 * The actual processing requires the knowledge of the image type as compile time constant.
 * However, this is outsourced into a separate functor (`FusionFunctor`), which is indirectly
 * called from `predict` with runtime information only. The functor receives the type as compile
 * time value. This allows the `ExampleFusor` to stay copy-constructible (even
 * default-constructible). The same goal can be achieved by doing something similar:
 * @code
 * template<Type t>
 * class PredictionAlgorithm {
 *     // ... a template class, which uses the image type to access pixel values
 * };
 *
 * class ExampleFusor : public DataFusor {
 * public:
 *     void predict(int date, ConstImage const& mask = ConstImage{}) override {
 *         ConstImage const& src = imgs->get("src", date);
 *         CallBaseTypeFunctor::run(*this, src.type());
 *     }
 *
 *     template<Type t>
 *     inline void operator()() {
 *         PredictionAlgorithm<t> alg;
 *         // ... give alg the required source images, the output buffer,
 *         //     maybe cached resources and let it predict!
 *     }
 *
 *     // option handling
 *     // ...
 * };
 * @endcode
 * Here, the `ExampleFusor` uses itself as functor, which allows to access everything in the
 * operator(). So the prediction could be implemented there as well and only call some template
 * functions where necessary. Though, in the example this is delegated to a template class, which
 * might be cleaner.
 *
 * Remark, if the corresponding Options class is not available through
 * `ExampleFusor::options_type`, Parallelizer needs to know how it is called. This can be done with
 * the second template parameter, like `Parallelizer<ExampleFusor, ExampleOptions>`.
 */
template<class Alg, class AlgOpt = typename Alg::options_type>
class Parallelizer : public DataFusor {
public:

    Parallelizer() = default;

    /**
     * @brief Construct a Parallelizer with a sample of the underlying DataFusor
     *
     * @param sample of a data fusor that will be copied in processOptions.
     *
     * If the underlying DataFusor requires special handling on construction, a well-formed object
     * can be given as `sample`, which will be used for copy construction to the number of parallel
     * working DataFusor objects.
     */
    explicit Parallelizer(Alg sample) : fusorSample{std::move(sample)} { }


    /**
     * @brief Process options for the parallelizer and the underlying DataFusor
     * @param o are the options of type ParallelizerOptions<AlgOpt>.
     *
     * The parallelizer options `o` are used to copy-construct DataFusor%s from the sample
     * according to the number of threads specified in the options. Then the prediction area of `o`
     * (not of the underlying options object) is used to determine and set the stripes for all
     * underlying options objects as prediction area. This is visualized in the image below. These
     * options are then given to the underlying DataFusor%s to process them.
     *
     * @image html predarea_stripes.png
     *
     * @throws size_error if the prediction area is too small, i. e. width < 1 or height / threads < 1
     */
    void processOptions(Options const& o) override;


    /**
     * @brief Get the previously set options
     * @return parallelizer options with the initial underlying options
     */
    ParallelizerOptions<AlgOpt> const& getOptions() const override;


    /**
     * @brief Predict the image with help of the underlying DataFusor%s
     * @param date to predict
     *
     * @param mask is an optional mask in the size of the input images to mark invalid input data
     * (e. g. fill values). Your predict method should check for a valid mask, like done in
     * parallelizer_test.cpp.
     *
     *
     * To predict the image at the specified date, the underlying DataFusor%s are called. But
     * before this, the output image buffer is constructed in the size of the prediction area if
     * not already there. Then each DataFusor gets a shared copy stripe of this output image buffer
     * cropped to the underlying DataFusor's prediction area. The DataFusor%s should use this
     * preconstructed image to write the result into. But if they do not use it and construct a new
     * output image buffer, the Parallelizer will merge them afterwareds. So notice, for best
     * performance a DataFusor should check if there is an image available as result, for example
     * like this:
     * @code
     * void predict(int date) {
     *     Rectangle predArea = options.getPredictionArea();
     *     if (output.size() != predArea.size() || output.type() != imgs->getAny().type())
     *         output = Image{predArea.width, predArea.height, imgs->getAny().type()}; // create a new one
     *
     *     // ...
     * }
     * @endcode
     */
    void predict(int date, ConstImage const& mask = ConstImage{}) override;

private:
    ParallelizerOptions<AlgOpt> options;
    std::vector<Alg> fusors;
    Alg fusorSample;
};



template<class Alg, class AlgOpt>
inline void Parallelizer<Alg,AlgOpt>::processOptions(Options const& o) {
    options = dynamic_cast<ParallelizerOptions<AlgOpt> const&>(o);
}

template<class Alg, class AlgOpt>
inline ParallelizerOptions<AlgOpt> const& Parallelizer<Alg,AlgOpt>::getOptions() const {
    return options;
}

template<class Alg, class AlgOpt>
inline void Parallelizer<Alg,AlgOpt>::predict(int date, ConstImage const& mask) {
    if (!imgs)
        IF_THROW_EXCEPTION(not_found_error("Parallelizer's source image collection is empty. You have to give it one via srcImages."));

    Rectangle pa = options.getPredictionArea();
    if (pa.height < 0 || pa.width < 0 || (pa.area() == 0 && (pa.width != 0 || pa.height != 0)))
        IF_THROW_EXCEPTION(size_error("Prediction area is invalid (negative or zero dimension, but not empty)! "
                                      "Note the prediction area of the algorithm options is ignored."))
                << errinfo_size(Size{pa.width, pa.height});

    // if no prediction area has been set, use full img size
    if (pa.x == 0 && pa.y == 0 && pa.width == 0 && pa.height == 0) {
        pa.width  = imgs->getAny().width();
        pa.height = imgs->getAny().height();
    }

    // get full size target image
    if (output.size() != pa.size() || output.type() != imgs->getAny().type())
        output = Image{pa.width, pa.height, imgs->getAny().type()}; // create a new one

    // reduce number of threads if height too less
    if ((unsigned int)pa.height < options.getNumberOfThreads())
        options.setNumberOfThreads(pa.height);
    unsigned int nt = options.getNumberOfThreads();

    // using fusorSample allows to use classes, which are not default constructible
    fusorSample.outputImage() = Image{};
    fusors.resize(nt, fusorSample);

    AlgOpt ao = options.getAlgOptions();
    if (ao.getPredictionArea().x != 0 || ao.getPredictionArea().y != 0 || ao.getPredictionArea().width != 0 || ao.getPredictionArea().height != 0)
        std::cout << "Warning: Note that the algorithm option's prediction area is ignored and replaced by the split up ParallelizerOption's prediction area." << std::endl;

    double stepsize = (double)(pa.height) / nt;
    double curY = pa.y;
    Rectangle fusorPredArea = pa;
    for (unsigned int i = 0; i < fusors.size(); ++i) {
        // set all child options to use a horizontal stripe as prediction window
        fusorPredArea.y = static_cast<int>(std::lround(curY));
        curY += stepsize;
        fusorPredArea.height = static_cast<int>(std::lround(curY)) - fusorPredArea.y;
        ao.setPredictionArea(fusorPredArea);
        fusors.at(i).processOptions(ao);
    }

    ThreadExceptionHelper ex;
    #pragma omp parallel for num_threads(fusors.size())
    for (unsigned int i = 0; i < fusors.size(); ++i) {
        // give fusor algorithm access to the source images and the (yet full sized) target image
        fusors.at(i).srcImages(imgs);

        // crop the target image to the individual prediction area, so the algorithm does not need to create an image
        Rectangle roi = fusors.at(i).getOptions().getPredictionArea();
        roi.x = 0;
        roi.y -= pa.y;
        Image outputPart{output.sharedCopy(roi)};
        fusors.at(i).outputImage() = Image{outputPart.sharedCopy()};

        // let them work and catch exceptions to escort the last one out of the parallel section
        ex.run([&]{
            fusors.at(i).predict(date, mask);
        });

        // check if the fusors used the available cropped image and if so continue
        // if they created an own image each, merge them to the big image, which is already available
        Image& out = fusors.at(i).outputImage();
        if (!out.isSharedWith(output))
            outputPart.copyValuesFrom(out);
    }

    // if ex caught an exception, rethrow it
    ex.rethrow();
}


} /* namespace imagefusion */

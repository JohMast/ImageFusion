#pragma once

#include "DataFusor.h"

namespace imagefusion {


/**
 * @brief Meta DataFusor, which acts as Proxy to the real DataFusor
 *
 * For usage with the Parallelizer a DataFusor must be copy constructible. This might be hard for
 * templated DataFusors, which require a factory for construction. Often this can be avoided by
 * using a pattern as shown in the documentation of Parallelizer. If it is not possible to make a
 * DataFusor copy constructible and parallelization is desired, this Proxy meta DataFusor can be
 * used to declare a proxy class that forwards the requests to the real DataFusor. The goal is a
 * class that is copy constructible and behaves like the DataFusor that is not. This allows usage
 * with the Parallelizer.
 *
 * The implementation of such a proxy can be done like shown in the following example. There an
 * ExampleFusor exists, which requires construction with a compile-time value of the enum @ref Type
 * (this is a requirement for the template parameter of this Proxy class). It likes to get the base
 * type. Proxy<ExampleFusor>::SimpleFactory can do this. The tricky thing about implementing a
 * proxy is to care about the underlying object Proxy::df. This has to be considered when copying
 * from a sample. In the copy constructor below this is handled by delegating to the constructor to
 * make a new sample, which is saved in Proxy::df. In the copy / move assignment this is handled
 * using the Proxy's swap function in the ExampleProxy's swap. A move is optional, but if it is
 * specified, Proxy::df can simply be moved.
 * @code
 * template<Type t>
 * class ExampleFusor;
 *
 * class ExampleProxy : public Proxy<ExampleFusor> {
 * public:
 *     // used for first construction to make a sample and for copying
 *     ExampleProxy(Type t)
 *         : Proxy{SimpleFactory::create(t)}, t{t}
 *     { }
 *
 *     // used by Parallelizer to make copies
 *     ExampleProxy(ExampleProxy const& sample)
 *         : ExampleProxy{sample.t}
 *     { }
 *
 *     ExampleProxy& operator=(ExampleProxy sample) noexcept {
 *         swap(*this, sample);
 *         return *this;
 *     }
 *
 *     friend void swap(ExampleProxy& first, ExampleProxy& second) noexcept {
 *         using std::swap;
 *         // call Proxy's swap to swap df, very important!
 *         swap(static_cast<Proxy<ExampleFusor>&>(first), static_cast<Proxy<ExampleFusor>&>(second));
 *         swap(first.t, second.t);
 *     }
 *
 *     // move is not required, but nice to have
 *     ExampleProxy(ExampleProxy&& sample) noexcept
 *         : Proxy{std::move(sample.df)}, t{sample.t}
 *     { }
 *
 *     using options_type = ExampleFusor::options_type;
 *
 * private:
 *     Type t;
 * };
 * @endcode
 */
template<template <Type> class Impl>
class Proxy : public DataFusor {
public:

    struct SimpleFactory {
        template<Type t>
        std::unique_ptr<DataFusor> operator()() const {
            return std::unique_ptr<DataFusor>(new Impl<t>{});
        }

        static std::unique_ptr<DataFusor> create(Type const& t) {
            return CallBaseTypeFunctor::run(SimpleFactory{}, t);
        }
    };


    /**
     * @brief Create a proxy with the given object as DataFusor
     * @param df is the real underlying DataFusor object.
     *
     * All requests, like predict etc., will be forwarded to `df`. This is what the Proxy does.
     */
    explicit Proxy(std::unique_ptr<DataFusor> df);


    /**
     * @brief Let the real DataFusor process the options
     * @param o are the options to process.
     * @see DataFusor::processOptions(Options const& o)
     */
    void processOptions(Options const& o) override;


    /**
     * @brief Get options from the real DataFusor
     * @return options.
     * @see DataFusor::getOptions() const
     */
    Options const& getOptions() const override;


    /**
     * @brief Let the real DataFusor predict an image
     * @param date to predict
     * @param mask is an optional mask to mark invalid image data.
     * @see DataFusor::predict(int date, ConstImage const& mask = ConstImage{})
     */
    void predict(int date, ConstImage const& mask = ConstImage{}) override;


    /**
     * @brief Get the MultiResImages collection from the real DataFusor
     * @return image collection
     * @see DataFusor::srcImages() const
     */
    MultiResImages const& srcImages() const override;



    /**
     * @brief Let the real DataFusor take a MultiResImages collection
     * @param imgs is the image collection that will be moved into the real DataFusor.
     * @see DataFusor::srcImages(std::shared_ptr<MultiResImages const> imgs)
     */
    void srcImages(std::shared_ptr<MultiResImages const> imgs) override;


    /**
     * @brief Get the output image buffer of the real DataFusor
     * @return output image buffer
     * @see DataFusor::outputImage()
     */
    Image const& outputImage() const override;

    /// \copydoc outputImage() const
    Image& outputImage() override;

protected:

    /**
     * @brief This holds ownership of the real DataFusor
     *
     * This is the real DataFusor object. A subclass must take care about this. So when copying a
     * Proxy, a new DataFusor must be created and given into this object. When moving, this object
     * has to be moved. The @ref swap function can also be used to swap these objects, which can be
     * useful for the copy-and-swap idiom.
     *
     * Note, using a Proxy, which has no object in `df` is undefined behaviour!
     */
    std::unique_ptr<DataFusor> df;
};




template<template <Type> class Impl>
inline Proxy<Impl>::Proxy(std::unique_ptr<DataFusor> df)
    : df{std::move(df)}
{ }

// forward *every* DataFusor request to the real DataFusor obj
template<template <Type> class Impl>
inline void Proxy<Impl>::processOptions(Options const& o) {
    df->processOptions(o);
}

template<template <Type> class Impl>
inline Options const& Proxy<Impl>::getOptions() const {
    return df->getOptions();
}

template<template <Type> class Impl>
inline void Proxy<Impl>::predict(int date, ConstImage const& mask) {
    df->predict(date, mask);
}

template<template <Type> class Impl>
inline MultiResImages const& Proxy<Impl>::srcImages() const {
    return df->srcImages();
}

template<template <Type> class Impl>
inline void Proxy<Impl>::srcImages(std::shared_ptr<MultiResImages const> imgs) {
    df->srcImages(std::move(imgs));
}

template<template <Type> class Impl>
inline Image const& Proxy<Impl>::outputImage() const {
    return df->outputImage();
}

template<template <Type> class Impl>
inline Image& Proxy<Impl>::outputImage() {
    return df->outputImage();
}

} /* namespace imagefusion */

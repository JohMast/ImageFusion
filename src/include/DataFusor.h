#pragma once

#include "MultiResImages.h"
#include "Options.h"


namespace imagefusion {

/**
 * @brief Abstract base for all data fusors
 *
 * This class provides a common interface for all data fusors and meta-fusors, like Parallelizer or
 * Proxy. To allow for the Proxy implementation, all methods are virtual.
 */
class DataFusor {
public:

    DataFusor()                              = default;
    DataFusor(DataFusor const& f)            = default;
    DataFusor(DataFusor&& f)                 = default;
    DataFusor& operator=(DataFusor const& f) = default;
    DataFusor& operator=(DataFusor&& f)      = default;

    /**
     * @brief Default destructor virtual for inheritance
     */
    virtual ~DataFusor()                     = default;


    /**
     * @brief Get the source image collection
     *
     * This is the collection of source images, from which the data fusor can read images.
     *
     * @return the source images collection.
     */
    virtual MultiResImages const& srcImages() const;


    /**
     * @brief Set the source image collection
     *
     * @param images is the source image collection (read-only) as shared pointer. If the data
     * fusor is the only owner of the image collection it cannot be modified anymore.
     *
     * @see srcImages()
     */
    virtual void srcImages(std::shared_ptr<MultiResImages const> images);


    /**
     * @brief Reference to the output buffer image
     *
     * This method provides access to the prediction result. You can either take this image by
     * making a shared copy and clearing the buffer afterwards to decouple it:
     * @code
     * Image o = Image{df.outputImage().sharedCopy()};
     * df.outputImage() = Image{};
     * @endcode
     *
     * Or you can simply copy the values from the buffer or clone it:
     * @code
     * Image o = df.outputImage();
     * @endcode
     *
     * The third alternative is that you provided a shared copy to an image before you start the
     * prediction. However, currently there is no way to enforce this behaviour. Then you should
     * check afterwards if the buffer is still shared with the target image and if not, you have to
     * copy the values:
     * @code
     * Image buffer{size, type};
     * df.outputImage() = Image{buffer.sharedCopy()};
     *
     * df.predict();
     *
     * if (!buffer.isSharedWith(df.outputImage()))
     *     // DataFusor did not respect the intent or size or type was wrong
     *     buffer = df.outputImage();
     * @endcode
     * This could also be a cropped part of an existing image:
     * @code
     * Image buffer{existingImage.sharedCopy(cropRectangle)};
     * df.outputImage() = Image{buffer.sharedCopy()};
     *
     * df.predict();
     *
     * if (!buffer.isSharedWith(df.outputImage()))
     *     // DataFusor did not respect the intent or size or type was wrong
     *     // Copy the values into the existing image
     *     buffer.copyValuesFrom(df.outputImage());
     * @endcode
     * An example for a data fusor that respects the output image is shown in
     * test/parallelizer_test.cpp.
     *
     * Note, on successive predictions the output image might (and should) be reused by the data
     * fusor. If you only make a shared copy the memory contents might get overwritten at the next
     * prediction.
     *
     * Also note, when using a data fusor as slave of a Parallelizer, the Parallelizer will set the
     * output image of the slave. The user can still set the output image, but for the
     * Parallelizer.
     *
     * @return reference to the internal output image buffer.
     */
    virtual Image const& outputImage() const;
    /// \copydoc outputImage() const
    virtual Image& outputImage();


    /**
     * @brief Predict an image at a specified date
     *
     * @param date is the date for which the image should be predicted.
     *
     * @param mask is an optional mask in the size of the input images to mark invalid input data
     * (e. g. fill values). Your predict method should check for a valid mask, like done in
     * parallelizer_test.cpp. If separate masks for the input images are meaningful and supported
     * by a data fusor this mask parameter is the mask for the low resolution image at the
     * prediction date. Otherwise it should be considered as a common mask for all input images.
     *
     * Note, all other necessary settings, such as setting the source images and other algorithm
     * specific settings should be set *before* calling predict!
     *
     * @see processOptions(Options const& o), srcImages(std::shared_ptr<MultiResImages const> images)
     */
    virtual void predict(int date, ConstImage const& mask = ConstImage{}) = 0;


    /**
     * @brief Set options for the data fusor to predict an image
     *
     * @param o are the algorithm specific options inherited from Options. This sets the prediction
     * area and algorithm specific options, like source tags and dates, window size or similar
     * things. Note, that when using a Parallelizer, only the prediction area of the
     * ParallelizerOptions are considered. The prediction area in the nested algorithm specific
     * options are ignored.
     */
    virtual void processOptions(Options const& o) = 0;


    /**
     * @brief Get the options object that was set before
     *
     * @return the options that were set with processOptions(Options const& o)
     */
    virtual Options const& getOptions() const = 0;


    /**
     * @brief Swap two data fusors
     * @param f1 is fusor 1.
     * @param f2 is fusor 2.
     */
    friend void swap(DataFusor& f1, DataFusor& f2) noexcept;

protected:
    /**
     * @brief Collection of source images
     *
     * This is a read-only parameter, so data fusors cannot write the output into it or modify
     * source images.
     */
    std::shared_ptr<MultiResImages const> imgs;


    /**
     * @brief Output buffer
     *
     * When implementing a data fusor, you should use an existing output image if appropriate.
     * Assuming you have a shared copy of a source image and you cropped it to the prediction area,
     * you can do that by:
     * @code
     * if (output.width()  != src.width() ||
     *     output.height() != src.height()
     *     output.type()   != src.type())
     * {
     *     output = Image{src.size(), src.type()}; // create new image
     * }
     * @endcode
     */
    Image output;
};



inline MultiResImages const& DataFusor::srcImages() const {
    return *imgs;
}

inline void DataFusor::srcImages(std::shared_ptr<MultiResImages const> images) {
    imgs = std::move(images);
}

inline Image const& DataFusor::outputImage() const {
    return output;
}

inline Image& DataFusor::outputImage() {
    return output;
}

inline void swap(DataFusor& f1, DataFusor& f2) noexcept {
    using std::swap;
    swap(f1.imgs, f2.imgs);
    swap(f1.output, f2.output);
}

} /* namespace imagefusion */

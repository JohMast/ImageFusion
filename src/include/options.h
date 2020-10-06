#pragma once

#include "image.h"

namespace imagefusion {


/**
 * @brief The Options class is the base class for algorithm specific option classes
 *
 * It has only the one attribute that every DataFusor algorithm will need: a prediction area. Sub
 * classes can add more attributes, like source resolution tags and dates, window size, etc.
 */
class Options {
public:
    /**
     * @brief Construct an empty Options object
     *
     * This constructor sets the prediction area to offsets 0 and size 0. The output tag is empty.
     */
    Options()                            = default;

    Options(Options const& f)            = default;
    Options(Options&& f)                 = default;
    Options& operator=(Options const& f) = default;
    Options& operator=(Options&& f)      = default;

    /**
     * @brief Default destructor virtual for inheritance
     */
    virtual ~Options()                   = default;


    /**
     * @brief Get prediction area
     * @return prediction area, which should have been set with setPredictionArea(Rectangle r)
     */
    Rectangle const& getPredictionArea() const;


    /**
     * @brief Set prediction area
     *
     * @param r is the prediction area, i. e. the part of the image that should be predicted by the
     * DataFusor. No DataFusor is required to predict the image outside of this area.
     *
     * This just saves the prediction area in the options object. To apply the option, see
     * DataFusor::processOptions.
     *
     * Note, when using Parallelizer only the prediction area in ParallelizerOptions will be used.
     * The prediction area in the nested options object in ParallelizerOptions will be ignored.
     *
     * @see predictionArea
     */
    void setPredictionArea(Rectangle r);


protected:

    /**
     * @brief Define size of the fused image and offset in the source images
     *
     * The prediction area defines the size of the resulting fused image. Additionally, it also
     * defines the offset in the corresponding source images. So in total the prediction area
     * defines how a resulting fused image is aligned to the source image.
     *
     * A DataFusor is only required to predict the resulting image in this region of the image.
     * Yet, the sourrounding image parts might be important for the algorithm. So defining a
     * prediction area is not a crop!
     *
     * As an example, assume a DataFusor that has a window size c and the window moves through the
     * image predicting one pixel in the center for each window position (like STARFM). Then the
     * prediction area should be offset by at least c/2 and have at most a width of w - c + 1 and a
     * height of h - c + 1 where w and h are the source image width and height, respectively. This
     * is illustrated in the following image:
     * @image html predarea.png
     *
     * Note, when using Parallelizer only the prediction area in ParallelizerOptions will be used.
     * The prediction area in the nested options object in ParallelizerOptions will be ignored.
     */
    Rectangle predictionArea{0,0,0,0};
};



inline Rectangle const& Options::getPredictionArea() const {
    return predictionArea;
}


inline void Options::setPredictionArea(Rectangle r) {
    predictionArea = r;
}


} /* namespace imagefusion */

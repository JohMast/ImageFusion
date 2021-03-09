#pragma once

#include "datafusor.h"
#include "staarch_options.h"
#include <cmath>
#include <iostream>
#include <opencv2/opencv.hpp>

#ifdef _OPENMP
    #include "parallelizer.h"
#endif /* _OPENMP */


namespace imagefusion {

/**
 * @brief STAARCH helper functions
 *
 * These are made public to be able to test them. The functions inside are not meant to be used by
 * library users, but only by the STAARCH implementation.
 */
namespace staarch_impl_detail {

/**
 * @brief Helper for finding a finding a value that also has a neighboring value
 *
 * @param valid_d is a mask. Here it is the mask of valid disturbed pixels. A location has 255 for a valid disturbed
 * pixel and 0 for invalid or not disturbed pixels.
 *
 * @param fourNeighbors specifies whether four or eight neighbors are used.
 *
 * @return a new image with +10 for a disturbed location and +1 for each neighbor. So a value of 12
 * means that the location and two of the neighbors have the value 255 in d.
 */
Image diNeighborFilter(ConstImage const& valid_d, bool fourNeighbors = true);

/**
 * @brief Find pixels with value in range that have a neighbor with the value also in range
 * @param di is the disturbance index (single-channel, float32)
 * @param mask is a single-channel mask representing the valid locations of `di`
 * @param range is the interval in which the values of di must be within to be marked.
 * @param fourNeighbors should be true for four neighbors and false for eight neighbors
 *
 * @return a single-channel uint8 mask image with 0s where di not in range or where is no neighbor
 * that satisfies di in range, and 255s for pixels with di in range and neighbors with di in range.
 */
Image exceedDIWithNeighbor(ConstImage const& di, ConstImage const& mask, Interval const& range, bool fourNeighbors = true);

/**
 * @brief Cluster image with k-means++
 * @param im is the image to cluster, float32
 * @param mask is the mask (uint8x1) for the image, with 255 for valid locations, 0 for invalid
 * @param k is the number of clusters to make
 *
 * The clustering considers the channels as features. So for a tasseled cap transformed image, the
 * features space is three dimensional.
 *
 * @return a labeled image (int32x1) with labels from 0 to k-1 and additionally -1 for
 * invalid pixels
 */
Image cluster(Image im, ConstImage const& mask, unsigned int k);

/**
 * @brief Standardize an image with mask
 * @param i is the image to standardize
 * @param mask is the mask. The image is only standardized for valid locations.
 *
 * This calculates the mean and standard deviation of the valid locations and standardizes these
 * locations.
 *
 * @return the image standardized in the valid locations as specified by the mask.
 */
Image standardize(Image i, ConstImage const& mask = {});

/**
 * @brief Get unique cluster labels for valid locations
 * @param clustered is a label image as returned by cluster() or provided in
 * StaarchOptions::getClusterImage()
 *
 * @return generally the sorted positive unique values from the image `clustered`, but if clustered
 * is the output from cluster(), this is just a vector with values 0 to k-1, where k is the
 * parameter in cluster().
 */
std::vector<int> getUniqueLandClasses(ConstImage const& clustered);

struct AveragingFunctor {
    std::vector<Image>& imgs;
    std::vector<Image>& masks;
    unsigned int n_imgs_window;
    StaarchOptions::MovingAverageWindow alignment;
    AveragingFunctor(std::vector<Image>& imgs, std::vector<Image>& masks, unsigned int n_imgs_window, StaarchOptions::MovingAverageWindow alignment)
        : imgs{imgs}, masks{masks}, n_imgs_window{n_imgs_window}, alignment{alignment} { }

    template<Type t>
    void operator()() {
        static_assert(getChannels(t) == 1, "This functor only accepts base type to reduce code size.");
        //static_assert(t == Type::float32 || t == Type::float64, "This functor is only made for disturbance indices and thus requires floating point type."); // For testing we use uint8
        using type = typename DataType<t>::base_type;
        int w = imgs.front().width();
        int h = imgs.front().height();
        unsigned int nimg = imgs.size();
        assert(imgs.front().channels() == 1 && "Disturbance Index must be single-channel");

        if (alignment == StaarchOptions::MovingAverageWindow::forward || alignment == StaarchOptions::MovingAverageWindow::backward) {
            bool forward = alignment == StaarchOptions::MovingAverageWindow::forward;
            // use forward average, e. g. for d2 average d2, d3, d4 by using (d2 + d3 + d4) / 3 or (d2 + d4) / 2 if d3 is invalid
            // or backward average, e. g. for d4 average d4, d3, d2 by using (d4 + d3 + d2) / 3 or (d4 + d2) / 2 if d3 is invalid
            for (unsigned int i = 0; i < nimg; ++i) {
                unsigned int tgt_iter = forward ? i : nimg - 1 - i;
                for (int y = 0; y < h; ++y) {
                    for (int x = 0; x < w; ++x) {
                        // Evaluate whether it is faster to work vectorwise here (add masked images and sum up masks/255, then imgs/masks)
                        double val = 0;
                        int count = 0;
                        for (unsigned int j = i; j < i + n_imgs_window && j < nimg; ++j) {
                            unsigned int win_iter = forward ? j : nimg - 1 - j;
                            if (masks.at(win_iter).empty() || masks.at(win_iter).boolAt(x, y, 0)) {
                                val += imgs.at(win_iter).at<type>(x, y);
                                ++count;
                            }
                        }
                        if (count > 0)
                            imgs.at(tgt_iter).at<type>(x, y) = cv::saturate_cast<type>(val / count);
                    }
                }
            }

            for (unsigned int i = 0; i < nimg; ++i) {
                unsigned int tgt_iter = forward ? i : nimg - 1 - i;
                if (!masks.at(tgt_iter).empty()) {
                    for (unsigned int j = i + 1; j < i + n_imgs_window && j < nimg; ++j) {
                        unsigned int win_iter = forward ? j : nimg - 1 - j;
                        if (masks.at(win_iter).empty()) {
                            // empty image means "all valid", so just remove mask
                            masks.at(tgt_iter) = Image{};
                            break;
                        }
                        masks.at(tgt_iter) = std::move(masks.at(tgt_iter)).bitwise_or(masks.at(win_iter));
                    }
                }
            }
        }
        else { // alignment == StaarchOptions::MovingAverageWindow::center
            int n_img_win_sym = n_imgs_window / 2;
            {
                // copy all images as source and use `imgs` as target
                std::vector<ConstImage> src_imgs;
                for (Image const& i : imgs)
                    src_imgs.emplace_back(i.clone());

                // calculate average and save in `imgs`
                for (int i = 0; i < static_cast<int>(nimg); ++i) {
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            // Evaluate whether it is faster to work vectorwise here (add masked images and sum up masks/255, then imgs/masks)
                            double val = 0;
                            int count = 0;
                            for (int j = i - n_img_win_sym; j <= i + n_img_win_sym && j < static_cast<int>(nimg); ++j) {
                                if (j < 0)
                                    continue;

                                if (masks.at(j).empty() || masks.at(j).boolAt(x, y, 0)) {
                                    val += src_imgs.at(j).at<type>(x, y);
                                    ++count;
                                }
                            }
                            if (count > 0)
                                imgs.at(i).at<type>(x, y) = cv::saturate_cast<type>(val / count);
                        }
                    }
                }
            }

            {
                // copy all masks as source and use `masks` as target
                std::vector<ConstImage> src_masks;
                for (Image const& m : masks)
                    src_masks.emplace_back(m.clone());

                // combine masks and save in `masks`
                for (int i = 0; i < static_cast<int>(nimg); ++i) {
                    if (!masks.at(i).empty()) {
                        for (int j = i - n_img_win_sym; j <= i + n_img_win_sym && j < static_cast<int>(nimg); ++j) {
                            if (i == j || j < 0)
                                continue;

                            if (src_masks.at(j).empty()) {
                                // empty image means "all valid", so just remove mask
                                masks.at(i) = Image{};
                                break;
                            }
                            masks.at(i) = std::move(masks.at(i)).bitwise_or(src_masks.at(j));
                        }
                    }
                }
            }
        }
    }
};

} /* staarch_impl_detail */




/**
 * @brief The StaarchFusor class is the implementation of the STAARCH algorithm with underlying modified STARFM
 *
 * STAARCH stands for *Spatial Temporal Adaptive Algorithm for mapping Reflectance Change*. It is
 * an extension for STARFM, which uses the predictions of two STARFM calls for each image; one
 * prediction from left and one from right, but it cannot blend them both.
 *
 * For STAARCH at least five images on three dates are required, but it is made for large gaps. The
 * dates 1 and n are the input image pair dates and each date inbetween is a prediction date, see
 * the following table:
 * <table style="margin-left: auto; margin-right: auto;">
 * <caption id="staarch_image_structure">STAARCH image structure</caption>
 *   <tr><td><span style="display:inline-block; width: 5em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">1</td>
 *       <td align="center" valign="top">2</td>
 *       <td align="center" valign="top">3</td>
 *       <td align="center" valign="top">...</td>
 *       <td align="center" valign="top">n-1</td>
 *       <td align="center" valign="top">n</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center"><table><tr><th style="width: 5em;">High 1</th></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High 2</b></td></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High 3</b></td></tr></table></td>
 *       <td align="center">...</td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High n-1</b></td></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">High n</th></tr></table></td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low 1</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low 2</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low 3</th></tr></table></td>
 *       <td align="center">...</td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low n-1</th></tr></table></td>
 *       <td align="center"><table><tr><th style="width: 5em;">Low n</th></tr></table></td>
 *   </tr>
 * </table>
 *
 * STAARCH detects disturbed pixels in both high resolution images using some color transformations
 * (tasseled cap, disturbed index, NDVI) and thresholding. Then it searches in the low resolution
 * images for the time point (by using thresholding in the disturbed index space) when some fixed
 * relative threshold between min and max value is exceeded. This yields the date of disturbance
 * (DOD) image, which is the main output of the algorithm. For prediction STARFM is used twice,
 * once with the left high-low-pair and once with the right pair, but not both. Disturbed locations
 * after their date of disturbance are set from the right prediction, the remaining pixels are set
 * from the left prediction. The algorithm apparently assumes that the disturbance is monotonic in
 * time or a hard switch from off to on.
 *
 * There are differences of this implementation to the algorithm described in the original paper.
 * These are listed here:
 *
 * * The tasseled cap transformation is not specified in details, so this might be done differently
 *   regarding norming, than in the paper.
 * * We do not want to rely on land cover classification products and use a simple k-means
 *   clustering on the left high resolution image in tasseled cap color space instead. The number
 *   of clusters can be set in the options.
 * * The not-disturbed locations are predicted using STARFM with both surrounding image pairs. The
 *   paper states, that STARFM works well for these situations, just not for disturbed pixels.
 *
 * Parallelization is done implicitly and the usage of @ref Parallelizer "Parallelizer<StaarchFusor>"
 * is forbidden with this algorithm.
 *
 * Example
 * -------
 *
 * Let's now dive into an example and into the details of the intermediate results. We use the test
 * images that show the clearing of some forested area to build the Tesla giga factory in Germany.
 * The coordinates are 52°23'41.6"N 13°47'27.7"E. The clearing started on 2020-02-13 and was
 * finished eleven days later on 2020-02-24. We found the following Landsat-8 and daily MODIS
 * (MOD09GA, MYD09GA) images:
 *
 * <table style="margin-left: auto; margin-right: auto;">
 * <caption id="staarch_example_images">Images (RGB part) of the clearing.</caption>
 *   <tr><td><span style="display:inline-block; width: 5em;"></span>i<br>
 *           <span style="display:inline-block; width: 5em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">0<br>2019-10-14</td>
 *       <td align="center" valign="top">1<br>2020-02-05</td>
 *       <td align="center" valign="top">2<br>2020-03-24</td>
 *       <td align="center" valign="top">3<br>2020-04-07</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center">@image html "staarch-landsat-rgb-0.png"</td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 6em; height: 6em;"><b>High 1</b></td></tr></table></td>
 *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 6em; height: 6em;"><b>High 2</b></td></tr></table></td>
 *       <td align="center">@image html "staarch-landsat-rgb-3.png"</td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center">@image html "staarch-modis-rgb-0.png"</td>
 *       <td align="center">@image html "staarch-modis-rgb-1.png"</td>
 *       <td align="center">@image html "staarch-modis-rgb-2.png"</td>
 *       <td align="center">@image html "staarch-modis-rgb-3.png"</td>
 *   </tr>
 * </table>
 *
 * The first step we look at is how the disturbed pixels are identified. For that a tasseled cap
 * transformation is used. We show now how the left and right Landsat images look in this color
 * space, which consists of the following three channels:
 *
 * * brightness (shown as red channel in the following)
 * * greeness (shown as green channel in the following)
 * * wetness (shown as blue channel in the following)
 *
 * <table style="border: none; margin-left: auto; margin-right: auto;">
 *   <tr>
 *     <td align="center">@image html "staarch-landsat-tc-0.png"</td>
 *     <td align="center">@image html "staarch-landsat-tc-3.png"</td>
 *   </tr>
 *   <tr>
 *     <td align="center">left</td>
 *     <td align="center">right</td>
 *   </tr>
 * </table>
 *
 * The cleared forest can be easily recognized. To make the differences even more visible, the
 * images are now channelwise standardized, but separately for every land type (forest, buildings
 * etc.). Since we do not want to use pre-made land classifications, we simply use a k-means
 * algorithm on the left tasseled cap image to find four land classes (see
 * StaarchOptions::setNumberLandClasses()):
 * @image html "staarch-landsat-clusters-0.png"
 * Standardizing each channel for each cluster yields:
 *
 * <table style="border: none; margin-left: auto; margin-right: auto;">
 *   <tr>
 *     <td align="center">@image html "staarch-landsat-tc-std-0.png"</td>
 *     <td align="center">@image html "staarch-landsat-tc-std-3.png"</td>
 *   </tr>
 *   <tr>
 *     <td align="center">left</td>
 *     <td align="center">right</td>
 *   </tr>
 * </table>
 *
 * From that the disturbance index is calculated as `brighness` - `greeness` - `wetness`. Now we
 * are almost done for the change map. To find the disturbed pixels in both images, a thresholding
 * follows with the follwing default ranges:
 *
 * * disturbance index in [2, inf) and one neighbor disturbance index also in that range
 * * standardized brightness in [-3, inf)
 * * standardized greeness in (-inf, inf) (not used in the paper)
 * * standardized wetness in (-inf, 1]
 * * standardized NDVI in (-inf, 0] (Normalized Difference Vegetation Index not shown here, but
 *   standardized in the same way)
 *
 * From the two resulting disturbance masks for the left and the right image the change mask is
 * computed, using the simple rule, that a pixel that is not disturbed in the left image and is
 * disturbed in the right image is flagged in the change mask. All other images are not flagged
 * in the change mask. This can be illustrated as:
 * <table style="border: none; margin-left: auto; margin-right: auto;">
 *   <tr>
 *     <td align="center">@image html "staarch-disturbed-3.png"</td>
 *     <td align="center">--</td>
 *     <td align="center">@image html "staarch-disturbed-0.png"</td>
 *     <td align="center">=</td>
 *     <td align="center">@image html "staarch-change-mask.png"</td>
 *   </tr>
 *   <tr>
 *     <td align="center">right</td>
 *     <td align="center">--</td>
 *     <td align="center">left</td>
 *     <td align="center">=</td>
 *     <td align="center">change mask</td>
 *   </tr>
 * </table>
 *
 * Now that we have the change mask, we can find the date of disturbance from the MODIS images for
 * each pixel that is flagged as disturbed in the change mask. For this, the MODIS images are
 * transformed to Tasseled Cap space, standardized (without land classes) and transformed to
 * disturbance index.
 * <table style="border: none; margin-left: auto; margin-right: auto;">
 *   <tr>
 *     <td align="center">@image html "staarch-modis-di-0.png"</td>
 *     <td align="center">@image html "staarch-modis-di-1.png"</td>
 *     <td align="center">@image html "staarch-modis-di-2.png"</td>
 *     <td align="center">@image html "staarch-modis-di-3.png"</td>
 *   </tr>
 *   <tr>
 *     <td align="center">0</td>
 *     <td align="center">1</td>
 *     <td align="center">2</td>
 *     <td align="center">3</td>
 *   </tr>
 * </table>
 * Now the paper suggests to use a forward moving average time filter, i. e. the
 * disturbance index at i=0 at some location is the average of the disturbance indexes of the
 * disturbance indexes of i=0, i=1 and i=2 at that location.
 * <table style="border: none; margin-left: auto; margin-right: auto;">
 *   <tr>
 *     <td align="center">@image html "staarch-modis-avg-di-0.png"</td>
 *     <td align="center">@image html "staarch-modis-avg-di-1.png"</td>
 *     <td align="center">@image html "staarch-modis-avg-di-2.png"</td>
 *     <td align="center">@image html "staarch-modis-avg-di-3.png"</td>
 *   </tr>
 *   <tr>
 *     <td align="center">0</td>
 *     <td align="center">1</td>
 *     <td align="center">2</td>
 *     <td align="center">3</td>
 *   </tr>
 * </table>
 * This results in a date of disturbance image as follows:
 * @image html "staarch-dod-with-3-days-forward-avg.png"
 * where the colors mean that the pixels are disturbed on image
 * <span style="background-color: #a6611a;">0</span>,
 * <span style="background-color: #dfc27d;">1</span>,
 * <span style="background-color: #80cdc1;">2</span>,
 * <span style="background-color: #018571;">3</span> and
 * <span style="background-color: black; color: white;">black</span> means not disturbed at all.
 * When predicting for a pixel, the left image pair (i=0) is used for pixels that are not disturbed
 * yet, but will at a later date, and the right image pair (i=3) is used for pixels that are
 * disturbed at the current date or before. For pixels that are not disturbed at all, both image
 * pairs are used.
 *
 * Since we make a prediction for the images 1 and 2, this is not optimal, because we see that for
 * image 1 the upper part of the clearing would be predicted
 * <span style="background-color: #dfc27d;">from right</span>, although there is still forest. In
 * the optimal case the cleared area would be disturbed completely at image
 * <span style="background-color: #80cdc1;">2</span>. To find a better way, one might think to
 * avoid the averaging completely, but MODIS images are not very reliable. However, when using a
 * common centered averaging window (see StaarchOptions::setDIMovingAverageWindow()) over 3 images,
 * the date of disturbance image looks like this:
 * @image html "staarch-dod-with-3-days-centered-avg.png"
 * This is much better.
 *
 * Now let's go to the prediction with STARFM. The three possibilities here are:
 * <table style="border: none; margin-left: auto; margin-right: auto;">
 *   <tr>
 *     <td align="center">@image html "staarch-predicted-from-left.png"</td>
 *     <td align="center">@image html "staarch-predicted-from-both-sides.png"</td>
 *     <td align="center">@image html "staarch-predicted-from-right.png"</td>
 *   </tr>
 *   <tr>
 *     <td align="center">from left</td>
 *     <td align="center">from both sides</td>
 *     <td align="center">from right</td>
 *   </tr>
 * </table>
 *
 * These are the predictions for image 2 (2020-03-24) and usually one would use both sides, but
 * here we see, that at such a fast disturbance, it would be better to use just one pair of images.
 * STAARCH aims to do exactly that. Here are the prediction results, when the three predictions
 * above are combined with the date of disturbance image.
 *
 * <table style="margin-left: auto; margin-right: auto;">
 *   <tr><td><span style="display:inline-block; width: 5em;"></span>i<br>
 *           <span style="display:inline-block; width: 5em;"></span>date<br>
 *           resolution</td>
 *       <td align="center" valign="top">0<br>2019-10-14</td>
 *       <td align="center" valign="top">1<br>2020-02-05</td>
 *       <td align="center" valign="top">2<br>2020-03-24</td>
 *       <td align="center" valign="top">3<br>2020-04-07</td>
 *   </tr>
 *   <tr><td>High</td>
 *       <td align="center">@image html "staarch-landsat-rgb-0.png"</td>
 *       <td align="center" bgcolor="red">@image html "staarch-predicted-1.png"</td>
 *       <td align="center" bgcolor="red">@image html "staarch-predicted-2.png"</td>
 *       <td align="center">@image html "staarch-landsat-rgb-3.png"</td>
 *   </tr>
 *   <tr><td>Low</td>
 *       <td align="center">@image html "staarch-modis-rgb-0.png"</td>
 *       <td align="center">@image html "staarch-modis-rgb-1.png"</td>
 *       <td align="center">@image html "staarch-modis-rgb-2.png"</td>
 *       <td align="center">@image html "staarch-modis-rgb-3.png"</td>
 *   </tr>
 * </table>
 *
 * Here is some example C++ code to predict these images. The intermediate results shown above are
 * internal and cannot be accessed.
 *
 * @code
 * std::string highTag = "high";
 * std::string lowTag = "low";
 *
 * auto mri = std::make_shared<MultiResImages>();
 * mri->set(highTag, 20191014, Image("test_resources/images/tesla-set/LC08_L1TP_193023_20191014.tif"));
 * mri->set(highTag, 20200407, Image("test_resources/images/tesla-set/LC08_L1TP_193023_20200407.tif"));
 * mri->set(lowTag,  20191014, Image("test_resources/images/tesla-set/MOD09GA.A2019287.h18v03.006.tif"));
 * mri->set(lowTag,  20200205, Image("test_resources/images/tesla-set/MYD09GA.A2020036.h18v03.006.tif"));
 * mri->set(lowTag,  20200324, Image("test_resources/images/tesla-set/MYD09GA.A2020084.h18v03.006.tif"));
 * mri->set(lowTag,  20200407, Image("test_resources/images/tesla-set/MYD09GA.A2020098.h18v03.006.tif"));
 *
 * StaarchOptions o;
 * o.setHighResTag(highTag);
 * o.setLowResTag(lowTag);
 * o.setIntervalDates(20191014, 20200407);
 * o.setDIMovingAverageWindow(StaarchOptions::MovingAverageWindow::center);
 * o.setNumberLandClasses(4);
 * o.setNumberImagesForAveraging(3);
 * o.setHighResSensor(StaarchOptions::SensorType::landsat);
 * o.setLowResSensor(StaarchOptions::SensorType::modis);
 *
 * StaarchFusor staarch;
 * staarch.srcImages(mri);
 * staarch.processOptions(o);
 *
 * staarch.predict(20200205);
 * staarch.outputImage().write("predicted-20200205.tif");
 *
 * staarch.predict(20200324);
 * staarch.outputImage().write("predicted-20200324.tif");
 * @endcode
 *
 * The same in python can be done like this:
 *
 * @code{.py}
 * import imagefusion
 *
 * # load images
 * h1 = imagefusion.read_image('../../test/images/tesla-set/LC08_L1TP_193023_20191014.tif')
 * h4 = imagefusion.read_image('../../test/images/tesla-set/LC08_L1TP_193023_20200407.tif')
 * l1 = imagefusion.read_image('../../test/images/tesla-set/MOD09GA.A2019287.h18v03.006.tif')
 * l2 = imagefusion.read_image('../../test/images/tesla-set/MYD09GA.A2020036.h18v03.006.tif')
 * l3 = imagefusion.read_image('../../test/images/tesla-set/MYD09GA.A2020084.h18v03.006.tif')
 * l4 = imagefusion.read_image('../../test/images/tesla-set/MYD09GA.A2020098.h18v03.006.tif')
 * # if you want to have Geo Infos, use:
 * # img, gi = imagefusion.read_geo_image('filename.tif')
 *
 * # organize images by date and resolution
 * high = {1: h1,               4: h4}
 * low  = {1: l1, 2: l2, 3: l3, 4: l4}
 *
 * # create algorithm object
 * staarch = imagefusion.StaarchFusor()
 *
 * # set some options
 * staarch.temporal_uncertainty = 50
 * staarch.spectral_uncertainty = 50
 * staarch.number_classes = 40
 * staarch.window_size = 51
 * staarch.moving_average_window_alignment = imagefusion.StaarchFusor.MovingAverageWindow.center;
 * staarch.number_land_classes = 4;
 * staarch.moving_average_window_width = 3;
 * staarch.high_res_sensor_type = imagefusion.StaarchFusor.SensorType.landsat;
 * staarch.low_res_sensor_type = imagefusion.StaarchFusor.SensorType.modis;
 * staarch.output_bands = ['red', 'green', 'blue'];
 *
 * # predict h2 and h3
 * predicted = staarch.predict(low=low, high=high)
 *
 * # write predicted images for h2 and h3 into files
 * for d, img in predicted.items():
 *     imagefusion.write_image(f'predicted-{d}.tif', img)
 *     # if you want to have Geo Infos, use:
 *     # imagefusion.write_geo_image('filename.tif', img, gi)
 * @endcode
 */
class StaarchFusor : public DataFusor {
public:
    /**
     * @brief This is the date of disturbance image
     *
     * The date of disturbance image is generated by #generateDODImage(). When you call #predict()
     * directly and the `dodImage` is empty, #generateDODImage() will be called as well. But you do
     * not have to clear it manually -- whenever the high resolution dates are changed by using
     * StaarchOptions::setIntervalDates() and these are set with StaarchFusor::processOptions(),
     * the `dodImage` is cleared automatically. This is because it holds the disturbance between
     * two high resolution images. Also when you set new source images using #srcImages() it will
     * be cleared.
     *
     * It contains the dates from the DataFusor::imgs, so the image could for example have values
     * like `20190723`. However, only the locations where a change was detected in the high
     * resolution images contain such values. The other locations have the minimum int32 value, i.
     * e. `std::numeric_limits<int32_t>::%min()`. Therefore the type is Type::int32x1.
     *
     * @see generateDODImage
     */
    Image dodImage;

    /**
     * @brief This declares which option type to use
     */
    using options_type = StaarchOptions;

    /**
     * @brief Process the StaarchOptions
     * @param o is an options object of type StaarchOptions and replaces the current options object.
     *
     * When the interval dates change, the day of disturbance image will be cleared. To create a
     * new one either use generateDoD() or it will be called anyway by the next predict() call. You
     * can also save the #dodImage and put it back in, after setting the options, if you are sure,
     * that it does not change.
     *
     * @see getOptions(), generateDoD(), predict()
     */
    void processOptions(Options const& o) override;

    // inherited documentation
    options_type const& getOptions() const override {
        return opt;
    }

    /**
     * @brief Set the source image collection
     *
     * @param images is the source image collection (read-only) as shared pointer. If the data
     * fusor is the only owner of the image collection it cannot be modified anymore.
     *
     * For STAARCH the usage is different than for other algorithms. You do not only need to
     * provide the images, but also the masks for the images, if available. So the structure
     * extends to:
     * <table>
     * <caption id="staarch_image_mask_structure">STAARCH image structure with optional masks</caption>
     *   <tr><td><span style="display:inline-block; width: 5em;"></span>date<br>
     *           resolution</td>
     *       <td align="center" valign="top">1</td>
     *       <td align="center" valign="top">2</td>
     *       <td align="center" valign="top">3</td>
     *       <td align="center" valign="top">...</td>
     *       <td align="center" valign="top">n-1</td>
     *       <td align="center" valign="top">n</td>
     *   </tr>
     *   <tr><td>High</td>
     *       <td align="center"><table><tr><th style="width: 5em;">High 1</th></tr></table></td>
     *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High 2</b></td></tr></table></td>
     *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High 3</b></td></tr></table></td>
     *       <td align="center">...</td>
     *       <td align="center"><table><tr><td align="center" bgcolor="red" style="width: 5em;"><b>High n-1</b></td></tr></table></td>
     *       <td align="center"><table><tr><th style="width: 5em;">High n</th></tr></table></td>
     *   </tr>
     *   <tr><td>High mask</td>
     *       <td align="center"><table><tr><td align="center" bgcolor="SteelBlue" style="width: 5em;"><b>High 1</b></td></tr></table></td>
     *       <td></td>
     *       <td></td>
     *       <td></td>
     *       <td></td>
     *       <td></td>
     *   </tr>
     *   <tr><td>Low</td>
     *       <td align="center"><table><tr><th style="width: 5em;">Low 1</th></tr></table></td>
     *       <td align="center"><table><tr><th style="width: 5em;">Low 2</th></tr></table></td>
     *       <td align="center"><table><tr><th style="width: 5em;">Low 3</th></tr></table></td>
     *       <td align="center">...</td>
     *       <td align="center"><table><tr><th style="width: 5em;">Low n-1</th></tr></table></td>
     *       <td align="center"><table><tr><th style="width: 5em;">Low n</th></tr></table></td>
     *   </tr>
     *   <tr><td>Low mask</td>
     *       <td></td>
     *       <td align="center"><table><tr><td align="center" bgcolor="SteelBlue" style="width: 5em;"><b>Low 2</b></td></tr></table></td>
     *       <td align="center"><table><tr><td align="center" bgcolor="SteelBlue" style="width: 5em;"><b>Low 3</b></td></tr></table></td>
     *       <td></td>
     *       <td></td>
     *       <td align="center"><table><tr><td align="center" bgcolor="SteelBlue" style="width: 5em;"><b>Low n</b></td></tr></table></td>
     *   </tr>
     * </table>
     *
     * The mask images are used internally, when building the #dodImage. This is why there are not
     * only StaarchOptions::setHighResTag() and StaarchOptions::setLowResTag(), but also
     * StaarchOptions::setHighResMaskTag() and StaarchOptions::setLowResMaskTag(). The mask images
     * have to be single-channel images with Type::uint8 and contain only 0 or 255.
     *
     * The high and low resolution images should usually have a different number of channels
     * (depending on the sensor type). E. g. MODIS images need 7 channels, while Landsat images
     * need 6 channels, see ColorMapping::Modis_to_TasseledCap and
     * ColorMapping::Landsat_to_TasseledCap, respectively. However, these are mainly used for the
     * date of disturbance image. The prdiction is done with STARFM and STARFM requires images with
     * the same channels. A common set of channels is extracted as specified by
     * StaarchOptions::setOutputBands().
     *
     * Note, when setting new source images, the #dodImage is cleared, since the geographic
     * location could have changed.
     *
     * @see DataFusor::srcImages()
     */
    void srcImages(std::shared_ptr<MultiResImages const> images) override {
        imgs = std::move(images);
        *predictSrc = MultiResImages{}; // remove cached images with extracted output channels
        dodImage = Image();
    }


    /**
     * @brief Predict an image at the specified date
     *
     * @param date is the prediction date. It is used to get the correct Image and mask from #imgs.
     * See also \ref staarch_image_mask_structure "STAARCH image structure with optional masks".
     *
     * @param baseMask should either be empty or a single-channel mask with the size of the source
     * images. Zero values prevent the usage of any image at these locations. Note, that for
     * STAARCH there should be used separate masks for all images in the source image structure,
     * see #srcImages(). Then this mask here can does not need to mask away clouds, etc.
     *
     * STAARCH is mainly an algorithm to detect the date when disturbances occur for each pixel.
     * However, this date can be used to decide in a STARFM prediction which of the two surrounding
     * high-res images (left or right) to use as reference. Which channels should be used for
     * prediction can be specified by StaarchOptions::setOutputBands(). The images you set should
     * have more channels than required for prediction, see srcImages().
     *
     * When calling predict the first time for a new date interval (high res references), the date
     * of disturbance image has to be made. For that generateDOD() is called. However, you can also
     * call it manually and output the DoD images if you are interested in that, see #dodImage.
     * Then you don't have to use predict at all.
     *
     * Before you can call predict, you have to set the source images with srcImages() and set the
     * options with processOptions().
     */
    void predict(int date, ConstImage const& baseMask = {}, ConstImage const& predMask = {}) override;

    /**
     * @brief Generate the date of disturbance image
     *
     * @param baseMask should either be empty or an arbitrary mask in the size of the source
     * images. It must be single-channel. Zero values prevent the usage of any image at these
     * locations. The result at these locations is undefined. Note, that because for STAARCH there
     * should be the separate masks for all images in the source image structure, see #srcImages(),
     * the mask here can be a user region of interest and does not need to mask away clouds, etc.,
     * like with other algorithms.
     *
     * This uses the high resolution images with their respective masks and finds the change mask.
     * The change mask marks the locations that had a disturbance in the right high resolution
     * image, but not in the first. To find disturbances, the image is transformed to disturbance
     * index and NDVI and these are thresholded.
     *
     * The low resolution images with their respective masks are used to find the date of
     * disturbance for all locations marked in the change mask. For that they are converted to the
     * disturbance index (DI) and thresholded between the pixelwise min(DI) and max(DI) over time.
     * By default the threshold is set to 2/3, but can be changed using
     * StaarchOptions::setLowResDIRatio().
     *
     * @return the date of disturbance image, which is a single-channel int32 image, which just has
     * the dates specified in the source images as pixel values, see #dodImage.
     */
    ConstImage const& generateDODImage(ConstImage const& baseMask);

protected:
    /// StaarchOptions for the next prediction and generation of the date of disturbance image
    options_type opt;

    /**
     * @brief This is a cache for images extracted for prediction
     *
     * For prediction the high and low resolution images need to have the same channels, including
     * their order. This is differently when generating the date of disturbance image, see
     * srcImages(). Therefore, before STARFM can be used, the desired channels, specified by
     * StaarchOptions::setOutputBands(), are extracted from the source images. These are cached
     * here, but only the ones that are used for the next predictions, i. e. high and low
     * resolution images at the interval / pair dates and the low resolution image at the
     * prediction date.
     */
    std::shared_ptr<MultiResImages> predictSrc = std::make_shared<MultiResImages>();


    /**
     * @brief Check the input images size, number of channels, etc.
     * @param mask will also be checked
     *
     * This method is called from generateDODImage().
     */
    void checkInputImages(ConstImage const& mask) const;

    /**
     * @brief Check the input images size, number of channels, etc.
     * @param mask will also be checked
     *
     * This method is called from predict().
     */
    void checkInputImagesForPrediction(ConstImage const& validMask, ConstImage const& predMask) const;

    /**
     * @brief The dates of the available low resolution images
     *
     * @return the sorted dates of the available low resolution images starting with the left
     * interval date and ending with the right.
     */
    std::vector<int> getLowDates() const;

    /**
     * @brief Convert all low resolution images to standardized disturbance index
     * @param predArea is the prediction area. Only this region of the images will be converted.
     * @param baseMask is either empty or a single-channel mask, which can specify some
     * restrictions in addition to the images with the high and low resolution mask tags, see
     * @ref staarch_image_mask_structure. Locations with 0s will not be used.
     *
     * This converts all low resolution images in the specified interval (see
     * StaarchOptions::setIntervalDates()) to tasseled cap color space, normalizes the channels
     * (using the respective mask) and then converts it to disturbance index.
     *
     * @return vector of disturbance indexes of the low resolution images.
     */
    std::vector<Image> getLowStdDI(Rectangle const& predArea, ConstImage const& baseMask = {}) const;

    /**
     * @brief Average the disturbance indexes of neighboring images and combine their masks
     * @param lowDI are the disturbance indexes of the low resolution images, see getLowStdDI().
     * @param predArea is the prediction area. Only this region of the masks will be combined.
     *
     * This uses the setting from StaarchOptions::setNumberImagesForAveraging() and averages this
     * number of images and combines (using OR) the respective masks. Assume now the number is 3,
     * as in the paper, then to get the average for date i the images at dates i, i+1 and i+2 are
     * used, like described in the paper. The denominator in the average takes the masks into
     * account. So if e. g. a pixel location is valid at all dates the result is
     * \f$ \frac 1 3 \cdot (I_i + I_{i+1} + I_{i+2}) \f$, but if say the pixel location at date i+1
     * is invalid the result is \f$ \frac 1 2 \cdot (I_i + I_{i+2}) \f$. If a location is invalid
     * at all of the dates the combined mask will be invalid at that location, too.
     *
     * @return a vector of the averaged images and a vector of the respective masks.
     */
    std::pair<std::vector<Image>, std::vector<Image>> averageDI(std::vector<Image> lowDI, Rectangle const& predArea) const;

    /**
     * @brief Generate a distubance index threshold image
     * @param lowAvgDI are the averaged disturbance index images, see getLowStdDI() and averageDI().
     * @param lowCombinedMasks are the masks corresponding to the averaged DI, see averageDI().
     *
     * The date of disturbance is determined pixel-wise using a thresh between min and max DI
     * values, i. e. min + (max - min) * t. The min and max values are determined over time, for
     * every pixel independently. Masked values are not considered. If there are locations, that
     * are invalid for all dates, their values are undefined (in practice -inf, inf or nan).
     *
     * @return per-pixel threshold values to find the date of disturbance. The first date at which
     * a value exceeds the threshold is used as date of disturbance. This assumes the disturbance
     * is monotonic, which might not be the case in reality.
     */
    Image getLowThresh(std::vector<Image> const& lowAvgDI, std::vector<Image> const& lowCombinedMasks) const;

    /**
     * @brief Generate the high resolution change mask
     * @param predArea is the prediction area. The change mask is only build in that region.
     * @param baseMask is an additional mask. The masks corresponding to the images should be
     * provided in the source images, see srcImages().
     *
     * To generate the change mask, both high resolution images are transformed to NDVI and
     * tasseled cap space. Clusters for different land classes of the left high resolution image in
     * tasseled cap color space are prepared. The tasseled cap and NDVI images are standardized
     * channel-wise, but differently for each cluster. Then they are transformed to the disturbance
     * index. All locations get now a preselection as disturbed if the disturbance index at the
     * location and a neighboring location (see StaarchOptions::setNeighborShape()) exceeds a
     * threshold (see StaarchOptions::setHighResDIRange()). In addition the NDVI and
     * brightness, greeness, wetness values (tasseled cap space) must be in a specified range.
     * Finally a location is marked as disturbed, if it is not marked as disturbed in the left high
     * resolution image, but it is in the right.
     *
     * @return change mask of pixels disturbed between left and right high resolution images.
     */
    Image generateChangeMask(Rectangle const& predArea, ConstImage const& baseMask) const;

    /**
     * @brief Mapping from MODIS band names to channel numbers
     *
     * These are the default channel numbers used for extracting the output bands for prediction.
     * The channels to extract can be specified with StaarchOptions::setOutputBands(). The channel
     * numbers can be specified with StaarchOptions::setLowResSourceChannels() and
     * StaarchOptions::setHighResSourceChannels().
     */
    std::map<std::string, unsigned int> modisBands{{"red", 0}, {"nir", 1}, {"blue", 2}, {"green", 3}, {"swir3", 4}, {"swir1", 5}, {"swir2", 6}};

    /**
     * @brief Mapping from Landsat band names to channel numbers
     *
     * These are the default channel numbers used for extracting the output bands for prediction.
     * The channels to extract can be specified with StaarchOptions::setOutputBands(). The channel
     * numbers can be specified with StaarchOptions::setLowResSourceChannels() and
     * StaarchOptions::setHighResSourceChannels().
     */
    std::map<std::string, unsigned int> landsatBands{{"blue", 0}, {"green", 1}, {"red", 2}, {"nir", 3}, {"swir1", 4}, {"swir2", 5}};

    /**
     * @brief Extract the channels for one image
     * @param tag is the tag to use.
     * @param date is the date to use.
     * @param sensor is the sensor. This is required for the mapping from name to number, see
     * #modisBands and #landsatBands.
     *
     * This extracts from the source image specified by tag and date the desired channels and saves
     * the resulting image in predictSrc using the same tag and date.
     *
     * This procedure is required for prediction, since STARFM requires the images to have the same
     * channels. This is called from extractChannelsForPredictionImages().
     */
    void extractChannels(std::string const& tag, int date, StaarchOptions::SensorType sensor);

    /**
     * @brief Extract channels for all images required for prediction
     * @param predDate is the prediction date
     *
     * This just calls extractChannels() for the image pairs at both interval dates and for the low
     * resolution image at the prediction date.
     */
    void extractChannelsForPredictionImages(int predDate);
};


/// \cond
#ifdef _OPENMP
namespace staarch_impl_detail {
template <typename... T>
struct specialization_not_allowed : std::false_type
{ };
} /* namespace staarch_impl_detail */

/**
 * @brief Forbid to use StaarchFusor with Parallelizer.
 */
template <class AlgOpt>
class Parallelizer<StaarchFusor, AlgOpt> {
    static_assert(staarch_impl_detail::specialization_not_allowed<AlgOpt>::value, "Parallelizer not supported for StaarchFusor, but it is internally parallelized with OpenMP.");
};
#endif /* _OPENMP */
/// \endcond

} /* namespace imagefusion */

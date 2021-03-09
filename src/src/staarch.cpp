#include "staarch.h"
#include "starfm.h"

#ifdef _OPENMP
    #include "parallelizer.h"
    #include "parallelizer_options.h"
#endif /* _OPENMP */

namespace imagefusion {

void StaarchFusor::processOptions(Options const& o) {
    options_type newOpts = dynamic_cast<options_type const&>(o);
    if (!newOpts.areDatesSet)
        IF_THROW_EXCEPTION(runtime_error("Interval dates have not been set. This is required for disturbance detection as well as for prediction."));

    if (newOpts.highTag == newOpts.lowTag)
        IF_THROW_EXCEPTION(invalid_argument_error("The resolution tags for the high resolution and low resolution have to be different. You chose '" + newOpts.highTag + "' for both."));

    if (opt.getHighResSensor() == StaarchOptions::SensorType::landsat && !opt.getHighResSourceChannels().empty() && opt.getHighResSourceChannels().size() != 6)
        IF_THROW_EXCEPTION(invalid_argument_error("If you specify the source channel order, it must have the correct number of channels. For Landsat 6 channels are required, you gave " + std::to_string(opt.getHighResSourceChannels().size()) + "."));
    if (opt.getHighResSensor() == StaarchOptions::SensorType::modis && !opt.getHighResSourceChannels().empty() && opt.getHighResSourceChannels().size() != 7)
        IF_THROW_EXCEPTION(invalid_argument_error("If you specify the source channel order, it must have the correct number of channels. For MODIS 7 channels are required, you gave " + std::to_string(opt.getHighResSourceChannels().size()) + "."));
    if (opt.getLowResSensor() == StaarchOptions::SensorType::landsat && !opt.getLowResSourceChannels().empty() && opt.getLowResSourceChannels().size() != 6)
        IF_THROW_EXCEPTION(invalid_argument_error("If you specify the source channel order, it must have the correct number of channels. For Landsat 6 channels are required, you gave " + std::to_string(opt.getLowResSourceChannels().size()) + "."));
    if (opt.getLowResSensor() == StaarchOptions::SensorType::modis && !opt.getLowResSourceChannels().empty() && opt.getLowResSourceChannels().size() != 7)
        IF_THROW_EXCEPTION(invalid_argument_error("If you specify the source channel order, it must have the correct number of channels. For MODIS 7 channels are required, you gave " + std::to_string(opt.getLowResSourceChannels().size()) + "."));

    // clear date of disturbance image, when interval dates changed
    if (opt.dateLeft != newOpts.dateLeft || opt.dateRight != newOpts.dateRight)
        dodImage = Image();

    opt = newOpts;
}



void StaarchFusor::checkInputImages(ConstImage const& mask) const {
    if (!imgs)
        IF_THROW_EXCEPTION(logic_error("No MultiResImage object stored in StaarchFusor. This looks like a programming error."));

    auto highDates = opt.getIntervalDates();

    // check high res images
    std::string strHLeft  = "High resolution image (tag: " + opt.getHighResTag() + ") at lower interval date (date: " + std::to_string(highDates.first) + ")";
    std::string strHRight = "High resolution image (tag: " + opt.getHighResTag() + ") at upper interval date (date: " + std::to_string(highDates.second) + ")";

    if (!imgs->has(opt.getHighResTag(), highDates.first) || !imgs->has(opt.getHighResTag(), highDates.second)) {
        IF_THROW_EXCEPTION(not_found_error("Not all required images are available. For the high resolution change mask you need to provide:\n"
                                           " * " + strHLeft  + " [" + (imgs->has(opt.getHighResTag(), highDates.first)  ? "" : "NOT ") + "available]\n" +
                                           " * " + strHRight + " [" + (imgs->has(opt.getHighResTag(), highDates.second) ? "" : "NOT ") + "available]\n"
                                           "or set different dates with available images in the StaarchOptions"));
    }

    Type highTypeLeft  = imgs->get(opt.getHighResTag(), highDates.first).type();
    Type highTypeRight = imgs->get(opt.getHighResTag(), highDates.second).type();

    if (highTypeLeft != highTypeRight)
        IF_THROW_EXCEPTION(image_type_error("The data types for the high resolution images are different:\n"
                                            " * " + strHLeft  + " " + to_string(highTypeLeft)  + "\n" +
                                            " * " + strHRight + " " + to_string(highTypeRight) + "\n"));

    Size highSizeLeft  = imgs->get(opt.getHighResTag(), highDates.first).size();
    Size highSizeRight = imgs->get(opt.getHighResTag(), highDates.second).size();

    if (highSizeLeft != highSizeRight) {
        IF_THROW_EXCEPTION(size_error("The high resolution images have different sizes:\n"
                                      " * " + strHLeft  + " " + to_string(highSizeLeft) + "\n"
                                      " * " + strHRight + " " + to_string(highSizeRight)         + "\n"));
    }

    // check low resolution images
    auto lowDates = getLowDates();
    Type lowType  = imgs->get(opt.getLowResTag(), lowDates.front()).type();
    Size lowSize  = imgs->get(opt.getLowResTag(), lowDates.front()).size();
    std::string strL = "Low resolution image (tag: "  + opt.getLowResTag()  + ", date: ";// + std::to_string(opt.getPairDate()) + ")";

    for (int d : lowDates) {
        Type lowTypeOther  = imgs->get(opt.getLowResTag(),  d).type();
        if (lowTypeOther != lowType)
            IF_THROW_EXCEPTION(image_type_error("The data types for the low resolution images are different:\n"
                                                " * " + strL + std::to_string(lowDates.front()) + ") " + to_string(lowType)      + "\n" +
                                                " * " + strL + std::to_string(d)                + ") " + to_string(lowTypeOther) + "\n"));

        Size lowSizeOther = imgs->get(opt.getLowResTag(), d).size();
        if (lowSizeOther != lowSize) {
            IF_THROW_EXCEPTION(size_error("The low resolution images have different sizes:\n"
                                          " * " + strL + std::to_string(lowDates.front()) + ") " + to_string(lowSize)      + "\n"
                                          " * " + strL + std::to_string(d)                + ") " + to_string(lowSizeOther) + "\n"));
        }
    }

    // check difference of low and high res images
    if (getBaseType(lowType) != getBaseType(highTypeLeft))
        IF_THROW_EXCEPTION(image_type_error("The base data types for the high resolution images (" + to_string(getBaseType(highTypeLeft)) +
                                            ") and the low resolution images (" + to_string(getBaseType(lowType)) + ") are different."));

    if (highSizeLeft != lowSize)
        IF_THROW_EXCEPTION(size_error("The sizes of low resolution images (" + to_string(lowSize) + ") and of high resolution images (" + to_string(highSizeLeft) + ") are different."));

    // check mask
    if (!mask.empty() && mask.size() != highSizeLeft)
        IF_THROW_EXCEPTION(size_error("The mask has a wrong size: " + to_string(mask.size()) +
                                      ". It must have the same size as the images: " + to_string(highSizeLeft) + "."))
                << errinfo_size(mask.size());

    if (!mask.empty() && mask.basetype() != Type::uint8)
        IF_THROW_EXCEPTION(image_type_error("The mask has a wrong base type: " + to_string(mask.basetype()) +
                                            ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
                << errinfo_image_type(mask.basetype());

    if (!mask.empty() && mask.channels() != 1)
        IF_THROW_EXCEPTION(image_type_error("The mask has a wrong number of channels. It has " + std::to_string(mask.channels()) + ", but for STAARCH the mask should have 1 channel."))
                << errinfo_image_type(mask.type());

    // check required number of channels for tasseled cap transformation
    if (opt.getLowResSensor() == StaarchOptions::SensorType::modis && getChannels(lowType) != 7)
        IF_THROW_EXCEPTION(image_type_error("This algorithm requires all 7 channels of the MODIS scene in their natural order B1 - B7: red, nir, blue, green, swir3, swir1, swir2")) << errinfo_image_type(lowType);
    if (((opt.getLowResSensor() == StaarchOptions::SensorType::landsat) && getChannels(lowType) != 6) ||
        ((opt.getHighResSensor() == StaarchOptions::SensorType::landsat) && getChannels(highTypeLeft) != 6))
        IF_THROW_EXCEPTION(image_type_error("This algorithm requires the following 6 channels of the Landsat scene in their natural order: blue, green, red, nir, swir1, swir2")) << errinfo_image_type(lowType);
    // TODO: Add sentinel here

    // check cluster image
    ConstImage const& ci = opt.getClusterImage();
    if (!ci.empty() && ci.size() != lowSize)
        IF_THROW_EXCEPTION(size_error("The sizes of the cluster image (" + to_string(ci.size()) + ") is wrong. It should be: " + to_string(lowSize))) << errinfo_size(ci.size());
}


void StaarchFusor::checkInputImagesForPrediction(ConstImage const& validMask, ConstImage const& predMask) const {
    if (!imgs)
        IF_THROW_EXCEPTION(logic_error("No MultiResImage object stored in StaarchFusor while predicting. This looks like a programming error."));

    auto highDates = opt.getIntervalDates();
    auto lowDates = getLowDates();

    // check number of low res images
    if (lowDates.size() < 3)
        IF_THROW_EXCEPTION(not_found_error("At least three images are required to make a prediction using STAARCH"));

    // check that low res pair date images are available
    if (!imgs->has(opt.getLowResTag(), highDates.first) || !imgs->has(opt.getLowResTag(), highDates.second)) {
        std::string strLLeft  = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at lower interval date (date: " + std::to_string(highDates.first) + ")";
        std::string strLRight = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at upper interval date (date: " + std::to_string(highDates.second) + ")";

        IF_THROW_EXCEPTION(not_found_error("Not all required images are available. For prediction the low resolution images at the interval dates must be available. You need to provide:\n"
                                           " * " + strLLeft  + " [" + (imgs->has(opt.getLowResTag(), highDates.first)  ? "" : "NOT ") + "available]\n" +
                                           " * " + strLRight + " [" + (imgs->has(opt.getLowResTag(), highDates.second) ? "" : "NOT ") + "available]\n"));
    }

    // check mask
    Size highSizeLeft  = imgs->get(opt.getHighResTag(), highDates.first).size();
    if (!validMask.empty() && validMask.size() != highSizeLeft)
        IF_THROW_EXCEPTION(size_error("The validMask has a wrong size: " + to_string(validMask.size()) +
                                      ". It must have the same size as the images: " + to_string(highSizeLeft) + "."))
                << errinfo_size(validMask.size());

    if (!validMask.empty() && validMask.basetype() != Type::uint8)
        IF_THROW_EXCEPTION(image_type_error("The validMask has a wrong base type: " + to_string(validMask.basetype()) +
                                            ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
                << errinfo_image_type(validMask.basetype());

    if (!validMask.empty() && validMask.channels() != 1)
        IF_THROW_EXCEPTION(image_type_error("The validMask has a wrong number of channels. It has " + std::to_string(validMask.channels()) + ", but for STAARCH the mask should have 1 channel."))
                << errinfo_image_type(validMask.type());

    if (!predMask.empty() && predMask.size() != highSizeLeft)
        IF_THROW_EXCEPTION(size_error("The predMask has a wrong size: " + to_string(predMask.size()) +
                                      ". It must have the same size as the images: " + to_string(highSizeLeft) + "."))
                << errinfo_size(predMask.size());

    if (!predMask.empty() && predMask.basetype() != Type::uint8)
        IF_THROW_EXCEPTION(image_type_error("The predMask has a wrong base type: " + to_string(predMask.basetype()) +
                                      ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
                << errinfo_image_type(predMask.basetype());

    if (!predMask.empty() && predMask.channels() != 1)
        IF_THROW_EXCEPTION(image_type_error("The predMask must be a single-channel mask, but it has "
                                            + std::to_string(predMask.channels()) + " channels."))
                << errinfo_image_type(predMask.type());
}

std::vector<int> StaarchFusor::getLowDates() const {
    auto highDates = opt.getIntervalDates();
    std::vector<int> dates = imgs->getDates(opt.getLowResTag()); // dates is sorted
    std::vector<int> lowDates;
    for (int d : dates)
        if (d >= highDates.first && d <= highDates.second)
            lowDates.push_back(d);
    return dates;
}

Image staarch_impl_detail::standardize(Image i, ConstImage const& mask) {
    auto mean_std = i.meanStdDev(mask);

    for (double& s : mean_std.second)
        if (s == 0)
            s = 1;
        else
            s = 1 / s;

    Image copy_std = i.subtract(mean_std.first).multiply(mean_std.second);
    i.copyValuesFrom(copy_std, mask);

    return i;
}

namespace {
struct DisturbanceIndexFunctor {
    ConstImage const& i;
    ConstImage const& mask;

    DisturbanceIndexFunctor(ConstImage const& i, ConstImage const& mask)
        : i{i}, mask{mask} { }

    template<Type t>
    Image operator()() {
        static_assert(getChannels(t) == 1, "This functor only accepts base type to reduce code size.");
        static_assert(t == Type::float32 || t == Type::float64, "This functor is only made for standardized tasseled cap images and thus requires floating point type.");
        using type = typename DataType<t>::base_type;
        int w = i.width();
        int h = i.height();
        unsigned int chans = i.channels();
        if (chans != 3) /* standardized brightness, greeness, wetness */
            IF_THROW_EXCEPTION(logic_error("The image must have three channels: brightness, greeness, wetness"));
        Image di{w, h, t}; // disturbance index
        auto di_op = [&] (int x, int y) {
            di.at<type>(x, y) = i.at<type>(x, y, 0) - i.at<type>(x, y, 1) - i.at<type>(x, y, 2);
        };
        if (mask.empty()) {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    di_op(x, y);
        }
        else if (mask.channels() == 1) {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (mask.at<uint8_t>(x, y))
                        di_op(x, y);
        }
        else
            IF_THROW_EXCEPTION(logic_error("Mask has a bad number of channels: " +
                                           std::to_string(mask.channels()) +
                                           ", instead of 1."));

        return di;
    }
};
} /* anonymous namespace */


Image staarch_impl_detail::diNeighborFilter(ConstImage const& valid_d, bool fourNeighbors) {
    assert(valid_d.type() == Type::uint8x1 && "valid_d should be a single-channel mask here");

    Image filtered{valid_d.size(), Type::uint8x1};
    filtered.set(0); // 0 for masked pixels
    for (int y = 0; y < valid_d.height(); ++y) {
        int ybegin = y > 0 ? y - 1 : y;
        int yend   = y < valid_d.height() - 1 ? y + 1 : y;
        for (int x = 0; x < valid_d.width(); ++x) {
            uint8_t count = 0;
            if (!valid_d.boolAt(x, y, 0))
                continue;

            count += 10;

            if (fourNeighbors) {
                if (y > 0 && valid_d.boolAt(x, y-1, 0))
                    ++count;
                if (y < valid_d.height() - 1 && valid_d.boolAt(x, y+1, 0))
                    ++count;
                if (x > 0 && valid_d.boolAt(x-1, y, 0))
                    ++count;
                if (x < valid_d.width() - 1 && valid_d.boolAt(x+1, y, 0))
                    ++count;
            }
            else {
                int xbegin = x > 0 ? x - 1 : x;
                int xend   = x < valid_d.width() - 1 ? x + 1 : x;
                for (int yn = ybegin; yn <= yend; ++yn) {
                    for (int xn = xbegin; xn <= xend; ++xn) {
                        if (yn == y && xn == x) // skip center
                            continue;
                        if (!valid_d.boolAt(xn, yn, 0))
                            continue;
//                        if (fourNeighbors && xn != x && yn != y)
//                            continue;
                        ++count;
                    }
                }
            }
            filtered.at<uint8_t>(x, y, 0) = count;
        }
    }
    return filtered;
}


Image staarch_impl_detail::exceedDIWithNeighbor(ConstImage const& di, ConstImage const& mask, Interval const& di_range, bool fourNeighbors) {
    assert(di.type() == Type::float32x1 && "di (disturbed index) should be a single-channel floating point image.");
    assert(mask.type() == Type::uint8x1 && "mask should be a single-channel mask here");

    Image disturbed = di.createSingleChannelMaskFromRange({di_range});

    Image filtered; // disturbed center counts +10, every disturbed neighbor +1

    // without mask we can use OpenCV and hope that it is faster
    if (mask.empty()) {
        // make kernel to count neighbor pixels (count 1) and to check the center (counts 10).
        Image kernel{3, 3, Type::float32x1};
        constexpr float neighborFilterValue = 1. / 255.;
        constexpr float centerFilterValue  = 10. / 255.;
        if (fourNeighbors) {
            kernel.set(0);
            kernel.at<float>(1, 0) = neighborFilterValue;
            kernel.at<float>(0, 1) = neighborFilterValue;
            kernel.at<float>(1, 2) = neighborFilterValue;
            kernel.at<float>(2, 1) = neighborFilterValue;
        }
        else { // eight neighbors
            kernel.set(neighborFilterValue);
        }
        kernel.at<float>(1, 1) = centerFilterValue;

        // filter with neighbors, i. e. how many neighbors of a pixel are disturbed
        filtered = Image{disturbed.size(), Type::uint8x1};
        cv::filter2D(disturbed.cvMat(), filtered.cvMat(), CV_8U, kernel.cvMat());
    }
    else
        filtered = staarch_impl_detail::diNeighborFilter(disturbed.bitwise_and(mask), fourNeighbors);


    // binarize.
    // Values 0 to 10 (no neighbor or no center) will be 0 and
    // values > 10 (center 10 + x neighbors) will be 255
    return filtered.createSingleChannelMaskFromRange({Interval::closed(11, 255)});
}

namespace {

template<typename OP>
struct FindFirstFunctor { // TODO: Improve and put in Image-Class
    ConstImage const& i;
    ConstImage const& mask;
    OP const& op;
    FindFirstFunctor(ConstImage const& i, ConstImage const& mask, OP const& op) : i{i}, mask{mask}, op{op} { }

    template<Type t>
    ValueWithLocation operator()() {
        static_assert(getChannels(t) == 1, "This functor only accepts base type to reduce code size.");
        using type = typename DataType<t>::base_type;
        int w = i.width();
        int h = i.height();
        unsigned int chans = i.channels();
        if (mask.empty()) {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    for (unsigned int c = 0; c < chans; ++c)
                        if (op(i.at<type>(x, y, c), x, y, c))
                            return {i.doubleAt(x, y, c), Point(x, y)}; //, c; // TODO: Add channel to ValueWithLocation
        }
        else if (mask.channels() == 1) {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (mask.at<uint8_t>(x, y))
                        for (unsigned int c = 0; c < chans; ++c)
                            if (op(i.at<type>(x, y, c), x, y, c))
                                return {i.doubleAt(x, y, c), Point(x, y)}; //, c; // TODO: Add channel to ValueWithLocation
        }
        else if (mask.channels() == chans) {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    for (unsigned int c = 0; c < chans; ++c)
                        if (mask.at<uint8_t>(x, y, c))
                            if (op(i.at<type>(x, y, c), x, y, c))
                                return {i.doubleAt(x, y, c), Point(x, y)}; //, c; // TODO: Add channel to ValueWithLocation
        }
        else
            IF_THROW_EXCEPTION(logic_error("Mask has a bad number of channels."));

        return {std::numeric_limits<double>::quiet_NaN(), Point(-1, -1)}; // not found
    }
};


} /* anonymous namespace */
Image staarch_impl_detail::cluster(Image im, ConstImage const& mask, unsigned int k) {
    assert(im.basetype() == Type::float32 && "The K-Means implementation requires float32 base type, so just use it everywhere!");
    assert(mask.type() == Type::uint8x1 && "Mask type must be unint8x1!");

    // find first valid location
    auto is_invalid_op = [] (uint8_t v, int /*x*/, int /*y*/, int /*c*/) {
        return v == 0;
    };
    ValueWithLocation loc = CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8>::run(FindFirstFunctor{mask, {}, is_invalid_op}, mask.type());

    Image neg_mask;
    if (!mask.empty() && !std::isnan(loc.val)) {
        // give invalid values the same value, so they will build an additional single cluster
        neg_mask = mask.bitwise_not();
        im.set(-1000, neg_mask);
        ++k;
    }

    cv::Mat data = im.cvMat().reshape(/*cn, leave*/0, /*rows*/im.cvMat().total());

    Image labels; // result, will be int32_t
    cv::TermCriteria terminate{cv::TermCriteria::EPS + cv::TermCriteria::COUNT, /*maxCount*/100, /*epsilon*/1.0};
    cv::kmeans(data, /*K*/ k, labels.cvMat(), terminate, /*attempts*/ 3, cv::KmeansFlags::KMEANS_PP_CENTERS);

    labels.cvMat() = labels.cvMat().reshape(/*cn, leave*/0, /*rows*/im.height());

    if (!neg_mask.empty()) {
        // set invalid label to -1 and set max label to the previous invalid label
        int32_t invalid_label = labels.at<int32_t>(loc.p.x, loc.p.y);
        if (invalid_label != static_cast<int>(k)-1) {
            Image max_label_mask = labels.createSingleChannelMaskFromRange({Interval::closed(k-1, k-1)});
            labels.set(invalid_label, max_label_mask);
        }
        labels.set(-1, neg_mask);
    }

    return labels;
}

std::vector<int> staarch_impl_detail::getUniqueLandClasses(ConstImage const& clustered) {
    std::vector<double> unique = clustered.unique(); // unique seems to be sorted

    // filter out negative label, since this is the cluster of invalid locations
    std::vector<int> uniqueLandClasses;
    for (double d : unique)
        if (d >= 0)
            uniqueLandClasses.push_back(static_cast<int>(d));
    return uniqueLandClasses;
}


Image StaarchFusor::generateChangeMask(Rectangle const& predArea, ConstImage const& baseMask) const {
    auto highDates = opt.getIntervalDates();
    ConstImage highImgLeft  = imgs->get(opt.getHighResTag(), highDates.first);
    ConstImage highImgRight = imgs->get(opt.getHighResTag(), highDates.second);

    // tasseled cap transformation, NDVI
    auto tc_mapping = StaarchOptions::sensorTypeToTasseledCap(opt.getHighResSensor());

    std::vector<unsigned int> src_channels_tc = opt.getHighResSourceChannels();
    std::vector<unsigned int> src_channels_ndvi = opt.getHighResSensor() == StaarchOptions::SensorType::modis ? std::vector<unsigned int>{1, 0}
                                                                                                              : std::vector<unsigned int>{3, 2};
    if (!src_channels_tc.empty()) { // transform
        src_channels_ndvi.at(0) = src_channels_tc.at(src_channels_ndvi.at(0));
        src_channels_ndvi.at(1) = src_channels_tc.at(src_channels_ndvi.at(1));
    }

    Image highTCLeft  = highImgLeft.convertColor(tc_mapping, /* result */ Type::float32, src_channels_tc);
    Image highTCRight = highImgRight.convertColor(tc_mapping, /* result */ Type::float32, src_channels_tc);
    Image highNDVILeft  = highImgLeft.convertColor(ColorMapping::Pos_Neg_to_NDI, /* result */ Type::float32, src_channels_ndvi); // NIR - Red
    Image highNDVIRight = highImgRight.convertColor(ColorMapping::Pos_Neg_to_NDI, /* result */ Type::float32, src_channels_ndvi); // NIR - Red

    // build mask
    Image highMask;
    if (!baseMask.empty())
        highMask = baseMask.clone();
    if (imgs->has(opt.getHighResMaskTag(), opt.dateLeft))
        highMask = std::move(highMask).bitwise_and(imgs->get(opt.getHighResMaskTag(), opt.dateLeft));
    if (imgs->has(opt.getHighResMaskTag(), opt.dateRight))
        highMask = std::move(highMask).bitwise_and(imgs->get(opt.getHighResMaskTag(), opt.dateRight));

    // Standardize for each land cover class separately
    unsigned int nclusters = opt.getNumberLandClasses();
    ConstImage landClasses = opt.getClusterImage();
    if (nclusters < 2 && landClasses.empty()) {
        highTCLeft = staarch_impl_detail::standardize(std::move(highTCLeft), highMask);
        highTCRight = staarch_impl_detail::standardize(std::move(highTCRight), highMask);
        highNDVILeft = staarch_impl_detail::standardize(std::move(highNDVILeft), highMask);
        highNDVIRight = staarch_impl_detail::standardize(std::move(highNDVIRight), highMask);
    }
    else {
        std::vector<int> labels;
        if (landClasses.empty()) {
            landClasses = staarch_impl_detail::cluster(highTCLeft, highMask, nclusters);
            labels.resize(nclusters);
            std::iota(std::begin(labels), std::end(labels), 0);
        }
        else
            // user set land classes can be arbitrary labels, ignore negative ones
            labels = staarch_impl_detail::getUniqueLandClasses(landClasses);

        for (unsigned int l : labels) {
            Image landMask = landClasses.createSingleChannelMaskFromRange({Interval::closed(l, l)});

            highTCLeft = staarch_impl_detail::standardize(std::move(highTCLeft), landMask);
            highTCRight = staarch_impl_detail::standardize(std::move(highTCRight), landMask);
            highNDVILeft = staarch_impl_detail::standardize(std::move(highNDVILeft), landMask);
            highNDVIRight = staarch_impl_detail::standardize(std::move(highNDVIRight), landMask);
        }
    }
    // for exceedDIWithNeighbor we have to extend the prediction area by one pixel in each direction to get the same result as without prediction area
    Rectangle extendedPredArea = (predArea - Point(1, 1) + Size(2, 2)) & Rectangle(Point(0, 0), highImgLeft.size());
    Rectangle diffPredArea = Rectangle(predArea.tl() - extendedPredArea.tl(), predArea.size());

    highTCLeft    = Image{highTCLeft.sharedCopy(extendedPredArea)};
    highTCRight   = Image{highTCRight.sharedCopy(extendedPredArea)};
    highNDVILeft  = Image{highNDVILeft.sharedCopy(predArea)};
    highNDVIRight = Image{highNDVIRight.sharedCopy(predArea)};

    if (!highMask.empty())
        highMask = Image{highMask.sharedCopy(extendedPredArea)};

    // disturbance index
    Image highDILeft = CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(DisturbanceIndexFunctor{highTCLeft, {}}, highTCLeft.basetype());
    Image highDIRight = CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(DisturbanceIndexFunctor{highTCRight, {}}, highTCRight.basetype());

    // find DI > threshold with one neighbor also exceeding the threshold
    Interval di_range = opt.getHighResDIRange();
    bool fourNeighbors = opt.getNeighborShape() == StaarchOptions::NeighborShape::cross;
    Image disturbedLeft = staarch_impl_detail::exceedDIWithNeighbor(highDILeft, highMask, di_range, fourNeighbors);
    Image disturbedRight = staarch_impl_detail::exceedDIWithNeighbor(highDIRight, highMask, di_range, fourNeighbors);

    // get rid of the extra pixel around the prediction area
    disturbedLeft  = Image{disturbedLeft.sharedCopy(diffPredArea)};
    disturbedRight = Image{disturbedRight.sharedCopy(diffPredArea)};
    highTCLeft     = Image{highTCLeft.sharedCopy(diffPredArea)};
    highTCRight    = Image{highTCRight.sharedCopy(diffPredArea)};

    // check bounds of brightness, greeness, wetness and NDVI
    disturbedLeft = disturbedLeft.bitwise_and(highNDVILeft.createSingleChannelMaskFromRange({opt.ndviRange}));
    disturbedLeft = disturbedLeft.bitwise_and(highTCLeft.createSingleChannelMaskFromRange({opt.brightnessRange,
                                                                                           opt.greenessRange,
                                                                                           opt.wetnessRange}));

    disturbedRight = disturbedRight.bitwise_and(highNDVIRight.createSingleChannelMaskFromRange({opt.ndviRange}));
    disturbedRight = disturbedRight.bitwise_and(highTCRight.createSingleChannelMaskFromRange({opt.brightnessRange,
                                                                                              opt.greenessRange,
                                                                                              opt.wetnessRange}));

    // restrict change mask by limits
    return std::move(disturbedLeft).bitwise_not().bitwise_and(disturbedRight); // change mask: not disturbed → disturbed
}

std::vector<Image> StaarchFusor::getLowStdDI(Rectangle const& predArea, ConstImage const& baseMask) const {
    // convert all low res images to DI
    std::vector<int> lowDates = getLowDates(); // lowDates is sorted
    std::vector<Image> lowDI;
    lowDI.reserve(lowDates.size());
    for (int d : lowDates) {
        ConstImage lowImg = imgs->get(opt.getLowResTag(), d);

        // tasseled cap transformation
        auto tc_mapping = opt.getLowResSensor() == StaarchOptions::SensorType::modis ? ColorMapping::Modis_to_TasseledCap
                                                                                     : ColorMapping::Landsat_to_TasseledCap; // TODO: Add more sensor types when more color transformations are available
        Image lowTC = lowImg.convertColor(tc_mapping, /* result */ Type::float32, opt.getLowResSourceChannels());

        // standardize valid parts
        ConstImage lowMask = baseMask;
        if (imgs->has(opt.getLowResMaskTag(), d))
            lowMask = imgs->get(opt.getLowResMaskTag(), d);
        /* note: standardizing using a different mask for every image can cause issues, when clouds
         * invalidate different large parts of land classes in different images. E. g. sea could
         * be clouded in one image, while in another image forest is clouded. However, combining
         * all masks can result in no valid pixel at all. Hence, we hope for few clouded pixels
         * and use different masks.
         * The images should be standardized differently, since their overall brightness varies
         * a lot. However, it might be better to use the same places, but we decided against.
         * What is better could be analyzed in future.
         */
        lowTC = Image{staarch_impl_detail::standardize(std::move(lowTC), lowMask).sharedCopy(predArea)};

        // disturbance index
        lowDI.push_back(CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(DisturbanceIndexFunctor{lowTC, {}}, lowTC.basetype()));
    }

     // required: lowDI must have the same order as lowDates
    return lowDI;
}



// return averaged DIs and combined masks
std::pair<std::vector<Image>, std::vector<Image>> StaarchFusor::averageDI(std::vector<Image> lowDI, Rectangle const& predArea) const {
    std::vector<int> lowDates = getLowDates(); // lowDates is sorted

    // copy masks
    std::vector<Image> masks;
    masks.reserve(lowDates.size());
    for (int d : lowDates) {
        if (imgs->has(opt.getLowResMaskTag(), d))
            masks.push_back(imgs->get(opt.getLowResMaskTag(), d).clone(predArea));
        else
            masks.push_back(Image{}); // empty mask
    }

    unsigned int n_imgs = opt.getNumberImagesForAveraging();
    auto alignment = opt.getDIMovingAverageWindow();
    if (n_imgs > 1 && !(n_imgs == 2 && alignment == StaarchOptions::MovingAverageWindow::center))
        CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(
                    staarch_impl_detail::AveragingFunctor{lowDI, masks, n_imgs, alignment}, lowDI.front().basetype());
    return {lowDI, masks};

}

// find per-pixel minimum and maximum disturbance index
Image StaarchFusor::getLowThresh(std::vector<Image> const& lowAvgDI, std::vector<Image> const& lowCombinedMasks) const {
    assert(lowAvgDI.front().type() == Type::float32x1 && "We assumed float32 for simplicity.");
    Image minDI{lowAvgDI.front().size(), lowAvgDI.front().type()};
    Image maxDI{lowAvgDI.front().size(), lowAvgDI.front().type()};

    constexpr float inf = std::numeric_limits<float>::infinity();
    minDI.set(inf); // behaviour for Image::minimum() with -inf and inf is tested in Image_test.cpp. Image::maximum() is assumed to behave the same way.
    maxDI.set(-inf);

    std::vector<int> lowDates = getLowDates();
    assert(lowDates.size() == lowAvgDI.size() && "The sizes do not fit! Error.");
    for (unsigned int i = 0; i < lowDates.size(); ++i) {
        Image const& di = lowAvgDI.at(i);
        ConstImage const& lowMask = lowCombinedMasks.at(i);

        minDI = std::move(minDI).minimum(di, lowMask);
        maxDI = std::move(maxDI).maximum(di, lowMask);
    }

    // per-pixel threshold is min + (max - min) * t
    Image thresh = minDI.add(std::move(maxDI).subtract(minDI).multiply(opt.getLowResDIRatio()));
    return thresh;
}

ConstImage const& StaarchFusor::generateDODImage(ConstImage const& baseMask) {
    checkInputImages(baseMask);

    // if no prediction area has been set, use full img size
    Rectangle predArea = opt.getPredictionArea();
    if (predArea.x == 0 && predArea.y == 0 && predArea.width == 0 && predArea.height == 0) {
        predArea.width  = imgs->getAny().width();
        predArea.height = imgs->getAny().height();
    }

    // convert all low res images to DI
    std::vector<int> lowDates = getLowDates(); // lowDates is sorted
    std::vector<Image> lowDI = getLowStdDI(predArea, baseMask); // lowDI have the same order as lowDates
    assert(lowDates.size() == lowDI.size());

    auto lowAvgDI_and_combinedMasks = averageDI(std::move(lowDI), predArea);
    std::vector<Image>& lowAvgDI = lowAvgDI_and_combinedMasks.first;
    std::vector<Image>& lowCombinedMasks = lowAvgDI_and_combinedMasks.second;
    assert(lowAvgDI.size() == lowCombinedMasks.size());

    /* Per-pixel threshold min + (max - min) * t, might contain invalid values (inf, -inf, nan)
     * for always invalid locations. However, these are not used anyways, since the lowMask below
     * will have marked these locations as invalid (since they are never valid).
     */
    Image thresh = getLowThresh(lowAvgDI, lowCombinedMasks);

    Image changeMask = generateChangeMask(predArea, baseMask); // it's enough to use baseMask here, since it will propagate into the change mask
    Image pixelsLeft = changeMask;

    if (dodImage.size() != changeMask.size())
        dodImage = Image{changeMask.size(), Type::int32x1}; // dates are of type int, could be just day of year like, 234 or 2019234 or a date as integer like 20190523

    constexpr int32_t int_nan = std::numeric_limits<int32_t>::max();
    dodImage.set(int_nan);
    for (unsigned int i = 0; i < lowDates.size(); ++i) {
        int date = lowDates.at(i);
        Image& di = lowAvgDI.at(i);
        di = std::move(di).subtract(thresh);

        ConstImage const& lowMask = lowCombinedMasks.at(i);

        Image disturbed = di.createSingleChannelMaskFromRange({Interval::closed(0, std::numeric_limits<double>::infinity())});
        disturbed = std::move(disturbed).bitwise_and(pixelsLeft).bitwise_and(lowMask);

        // TODO: Add option to mark a pixel only as disturbed if it is never disturbed before thresh and always after thresh???
        dodImage.set(date, disturbed); // set date as pixel value

        pixelsLeft = pixelsLeft.bitwise_and(std::move(disturbed).bitwise_not());
    }

//    // Debug output of DOD image
//    std::cout << "DOD image with the following values:";
//    std::map<double, unsigned int> uniques = dodImage.uniqueWithCount();
//    Image dodOutput{dodImage.size(), Type::uint8x1};
//    dodOutput.set(0);
//    uint8_t step = 255 / (uniques.size() - 1);
//    uint8_t gray = 0;
//    for (auto const& p : uniques) {
//        std::cout << "[" << p.first << ": " << p.second << "] ";
//        if (std::numeric_limits<int32_t>::min() < p.first && p.first < std::numeric_limits<int32_t>::max()) {
//            gray += step;
//            Image match_mask = dodImage.createSingleChannelMaskFromRange({Interval::closed(p.first, p.first)});
//            dodOutput.set(gray, match_mask);
//        }
//    }
//    std::cout << std::endl;
//    dodOutput.write("dodImage.tif");

    return dodImage;
}



void StaarchFusor::extractChannels(std::string const& tag, int date, StaarchOptions::SensorType sensor) {
    if (predictSrc->has(tag, date))
        return;

    std::vector<std::string> const& names = opt.getOutputBands();
    std::vector<unsigned int> chans;
    for (std::string const& n : names)
        if (sensor == StaarchOptions::SensorType::modis)
            chans.push_back(modisBands[n]);
        else if (sensor == StaarchOptions::SensorType::landsat)
            chans.push_back(landsatBands[n]);
        else
            IF_THROW_EXCEPTION(not_implemented_error("Only Modis and Landsat implemented, currently. For sentinel the tesseled cap transformation is unknown."));

    ConstImage const& full = imgs->get(tag, date);
    Image extracted = merge(full.split(chans)); // TODO: reduce size of images by using a sample area, calculated from prediction area + window size. Remember to adjust the predArea setting in the StarfmOptions
    predictSrc->set(tag, date, std::move(extracted));
}

void StaarchFusor::extractChannelsForPredictionImages(int predDate) {
    // remove cached images, which are not required anymore
    std::set<int> allDates = predictSrc->getDates();
    for (int d : allDates)
        if (d != predDate && d != opt.dateLeft && d != opt.dateRight)
            predictSrc->remove(d);

    // create new ones required for prediction
    extractChannels(opt.getLowResTag(),  predDate,      opt.getLowResSensor());
    extractChannels(opt.getLowResTag(),  opt.dateLeft,  opt.getLowResSensor());
    extractChannels(opt.getLowResTag(),  opt.dateRight, opt.getLowResSensor());
    extractChannels(opt.getHighResTag(), opt.dateLeft,  opt.getHighResSensor());
    extractChannels(opt.getHighResTag(), opt.dateRight, opt.getHighResSensor());
}

namespace {
Image makeStarfmMask(ConstImage const& baseMask, MultiResImages const& imgs, StaarchOptions const& opt, int pairDate, int predDate) {
    Image mask = baseMask.clone();
    if (imgs.has(opt.getHighResMaskTag(), pairDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getHighResMaskTag(), pairDate));
    if (imgs.has(opt.getLowResMaskTag(), pairDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getLowResMaskTag(), pairDate));
    if (imgs.has(opt.getLowResMaskTag(), predDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getLowResMaskTag(), predDate));
    return mask;
}

Image makeStarfmMask(ConstImage const& baseMask, MultiResImages const& imgs, StaarchOptions const& opt, int leftPairDate, int predDate, int rightPairDate) {
    Image mask = baseMask.clone();
    if (imgs.has(opt.getHighResMaskTag(), leftPairDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getHighResMaskTag(), leftPairDate));
    if (imgs.has(opt.getLowResMaskTag(), leftPairDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getLowResMaskTag(), leftPairDate));
    if (imgs.has(opt.getHighResMaskTag(), rightPairDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getHighResMaskTag(), rightPairDate));
    if (imgs.has(opt.getLowResMaskTag(), rightPairDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getLowResMaskTag(), rightPairDate));
    if (imgs.has(opt.getLowResMaskTag(), predDate))
        mask = std::move(mask).bitwise_and(imgs.get(opt.getLowResMaskTag(), predDate));
    return mask;
}
} /* anonymous namespace */

void StaarchFusor::predict(int date, ConstImage const& baseMask, ConstImage const& predMask) {
    checkInputImagesForPrediction(baseMask, predMask);

    if (dodImage.empty())
        generateDODImage(baseMask);

    // copy channels for output
    extractChannelsForPredictionImages(date);


    // STARFM options
    StarfmOptions starfmOpts = opt.s_opt;
    starfmOpts.setHighResTag(opt.getHighResTag());
    starfmOpts.setLowResTag(opt.getLowResTag());

#ifdef _OPENMP
    imagefusion::ParallelizerOptions<imagefusion::StarfmOptions> parStarfmOpts;
    parStarfmOpts.setPredictionArea(opt.getPredictionArea());
    imagefusion::Parallelizer<imagefusion::StarfmFusor> starfm;
#else /* _OPENMP not defined */
    starfmOpts.setPredictionArea(opt.getPredictionArea());
    imagefusion::StarfmFusor starfm;
#endif /* _OPENMP */

    starfm.srcImages(predictSrc);

    // prediction mask must have full size, while dodImage is in the size of the output / prediction area
    Image starfmPredMask{predictSrc->getAny().size(), Type::uint8x1};
    Image starfmPredMaskCropped = opt.getPredictionArea().area() == 0 ? Image{starfmPredMask.sharedCopy()}
                                                                      : Image{starfmPredMask.sharedCopy(opt.getPredictionArea())};

    // predict from both sides
    Image disturbed = dodImage.createSingleChannelMaskFromRange({Interval::closed(-std::numeric_limits<int32_t>::min()+1, std::numeric_limits<int32_t>::max()-1)});
    starfmPredMask.set(0);
    starfmPredMaskCropped.copyValuesFrom(disturbed.bitwise_not());

    starfmOpts.setDoublePairDates(opt.dateLeft, opt.dateRight);
#ifdef _OPENMP
    parStarfmOpts.setAlgOptions(starfmOpts);
    starfm.processOptions(parStarfmOpts);
#else /* _OPENMP not defined */
    starfm.processOptions(starfmOpts);
#endif /* _OPENMP */
    starfm.predict(date, makeStarfmMask(baseMask, *imgs, opt, opt.dateLeft, date, opt.dateRight), starfmPredMask);
    output = starfm.outputImage().clone();

    // predict from left
    Image fromLeft = dodImage.createSingleChannelMaskFromRange({Interval::closed(date+1, std::numeric_limits<int32_t>::max()-1)});
    starfmPredMask.set(0);
    starfmPredMaskCropped.copyValuesFrom(fromLeft);

    starfmOpts.setSinglePairDate(opt.dateLeft);
#ifdef _OPENMP
    parStarfmOpts.setAlgOptions(starfmOpts);
    starfm.processOptions(parStarfmOpts);
#else /* _OPENMP not defined */
    starfm.processOptions(starfmOpts);
#endif /* _OPENMP */
    starfm.predict(date, makeStarfmMask(baseMask, *imgs, opt, starfmOpts.getSinglePairDate(), date), starfmPredMask);
    output.copyValuesFrom(starfm.outputImage(), fromLeft);

    // predict from right
    Image fromRight = dodImage.createSingleChannelMaskFromRange({Interval::closed(-std::numeric_limits<int32_t>::min()+1, date)});
    starfmPredMask.set(0);
    starfmPredMaskCropped.copyValuesFrom(fromRight);

    starfmOpts.setSinglePairDate(opt.dateRight);
#ifdef _OPENMP
    parStarfmOpts.setAlgOptions(starfmOpts);
    starfm.processOptions(parStarfmOpts);
#else /* _OPENMP not defined */
    starfm.processOptions(starfmOpts);
#endif /* _OPENMP */
    starfm.predict(date, makeStarfmMask(baseMask, *imgs, opt, starfmOpts.getSinglePairDate(), date), starfmPredMask);
    output.copyValuesFrom(starfm.outputImage(), fromRight);
}


} /* namespace imagefusion */

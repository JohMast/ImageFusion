#include "Starfm.h"
#include <math.h>


namespace imagefusion {

void StarfmFusor::processOptions(Options const& o) {
    options_type newOpts = dynamic_cast<options_type const&>(o);
    if (!newOpts.isDate1Set)
        IF_THROW_EXCEPTION(runtime_error("No input pair date has been set. At least one pair date is required for prediction"));

    if (newOpts.highTag == newOpts.lowTag)
        IF_THROW_EXCEPTION(invalid_argument_error("The resolution tags for the input pairs have to be different. You chose '" + newOpts.highTag + "' for both."));

    opt = newOpts;
}


Rectangle StarfmFusor::findSampleArea(Size const& fullImgSize, Rectangle const& predArea) const {
    Rectangle sampleArea = predArea;
    sampleArea.x -= opt.winSize / 2;
    sampleArea.y -= opt.winSize / 2;
    sampleArea.width  += opt.winSize - 1;
    sampleArea.height += opt.winSize - 1;

    return sampleArea & Rectangle(0, 0, fullImgSize.width, fullImgSize.height);
}


Image StarfmFusor::computeDistanceWeights() const {
    Image distWeights(opt.winSize, opt.winSize, Type::float64x1);
    for (int x = 0; x <= (int)(opt.winSize) / 2; ++x) {
        for (int y = x; y >= 0; --y) {
            int xp = opt.winSize / 2 + x;
            int xn = opt.winSize / 2 - x;
            int yp = opt.winSize / 2 + y;
            int yn = opt.winSize / 2 - y;
            double d = std::sqrt(x*x + y*y) * 2 / opt.winSize + 1.0;

            distWeights.at<double>(xp, yp) = d;
            distWeights.at<double>(xp, yn) = d;
            distWeights.at<double>(xn, yp) = d;
            distWeights.at<double>(xn, yn) = d;

            distWeights.at<double>(yp, xp) = d;
            distWeights.at<double>(yp, xn) = d;
            distWeights.at<double>(yn, xp) = d;
            distWeights.at<double>(yn, xn) = d;
        }
    }
    return distWeights;
}


void StarfmFusor::checkInputImages(ConstImage const& mask, int date2) const {
    if (!imgs)
        IF_THROW_EXCEPTION(logic_error("No MultiResImage object stored in StarfmFusor while predicting. This looks like a programming error."));

    bool isDoublePairMode = opt.isDate3Set;

    std::string strH1 = "High resolution image (tag: " + opt.getHighResTag() + ") at date 1 (date: " + std::to_string(opt.date1) + ")";
    std::string strL1 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 1 (date: " + std::to_string(opt.date1) + ")";
    std::string strL2 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 2 (date: " + std::to_string(    date2) + ")";
    std::string strH3 = isDoublePairMode ? "High resolution image (tag: " + opt.getHighResTag() + ") at date 3 (date: " + std::to_string(opt.date3) + ")" : "";
    std::string strL3 = isDoublePairMode ? "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 3 (date: " + std::to_string(opt.date3) + ")" : "";

    if (!imgs->has(opt.getHighResTag(),  opt.date1) || !imgs->has(opt.getLowResTag(), opt.date1) || !imgs->has(opt.getLowResTag(), date2) ||
        (isDoublePairMode && (!imgs->has(opt.getHighResTag(), opt.date3) || !imgs->has(opt.getLowResTag(), opt.date3))))
    {
        IF_THROW_EXCEPTION(not_found_error("Not all required images are available. For STARFM you need to provide:\n"
                                           " * " + strH1 + " [" + (imgs->has(opt.getHighResTag(), opt.date1) ? "" : "NOT ") + "available]\n" +
                                           " * " + strL1 + " [" + (imgs->has(opt.getLowResTag(),  opt.date1) ? "" : "NOT ") + "available]\n" +
                                           " * " + strL2 + " [" + (imgs->has(opt.getLowResTag(),      date2) ? "" : "NOT ") + "available]\n" +
                       (isDoublePairMode ? " * " + strH3 + " [" + (imgs->has(opt.getHighResTag(), opt.date3) ? "" : "NOT ") + "available]\n" +
                                           " * " + strL3 + " [" + (imgs->has(opt.getLowResTag(),  opt.date3) ? "" : "NOT ") + "available]\n" : "")));
    }

    Type highType = imgs->get(opt.getHighResTag(), opt.date1).type();
    if (isDoublePairMode && imgs->get(opt.getHighResTag(), opt.date3).type() != highType)
        IF_THROW_EXCEPTION(image_type_error("The data types for the high resolution images are different:\n"
                                            " * " + strH1 + ": " + to_string(imgs->get(opt.getHighResTag(), opt.date1).type()) + "\n"
                                            " * " + strH3 + ": " + to_string(imgs->get(opt.getHighResTag(), opt.date3).type())));

    Type lowType  = imgs->get(opt.getLowResTag(),  opt.date1).type();
    if (imgs->get(opt.getLowResTag(), date2).type() != lowType || (isDoublePairMode && imgs->get(opt.getLowResTag(), opt.date3).type() != lowType))
        IF_THROW_EXCEPTION(image_type_error("The data types for the low resolution images are different:\n"
                                            " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(), opt.date1).type()) + "\n" +
                                            " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(),     date2).type()) + "\n" +
                        (isDoublePairMode ? " * " + strL3 + " " + to_string(imgs->get(opt.getLowResTag(), opt.date3).type()) + "\n" : "")));

    if (getBaseType(lowType) != getBaseType(highType))
        IF_THROW_EXCEPTION(image_type_error("The base data types for the high resolution image (" + to_string(getBaseType(lowType)) +
                                            ") and the low resolution images (" + to_string(getBaseType(lowType)) + ") are different."));

    if (getChannels(lowType) != getChannels(highType))
        IF_THROW_EXCEPTION(image_type_error("The number of channels of the low resolution images (" + std::to_string(getChannels(lowType)) +
                                            ") are different than of the high resolution images (" + std::to_string(getChannels(highType)) + ")."));

    Size s = imgs->get(opt.getLowResTag(), opt.date1).size();
    if (imgs->get(opt.getHighResTag(), opt.date1).size() != s || imgs->get(opt.getLowResTag(), date2).size() != s ||
        (isDoublePairMode && (imgs->get(opt.getHighResTag(), opt.date3).size() != s || imgs->get(opt.getLowResTag(), opt.date3).size() != s)))
    {
        IF_THROW_EXCEPTION(size_error("The required images have a different size:\n"
                                      " * " + strH1 + " " + to_string(imgs->get(opt.getHighResTag(), opt.date1).size()) + "\n"
                                      " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.date1).size()) + "\n" +
                                      " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(),      date2).size()) + "\n" +
                  (isDoublePairMode ? " * " + strH3 + " " + to_string(imgs->get(opt.getHighResTag(), opt.date3).size()) + "\n" +
                                      " * " + strL3 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.date3).size()) + "\n" : "")));
    }

    if (!mask.empty() && mask.size() != s)
        IF_THROW_EXCEPTION(size_error("The mask has a wrong size: " + to_string(mask.size()) +
                                      ". It must have the same size as the images: " + to_string(s) + "."))
                << errinfo_size(mask.size());

    if (!mask.empty() && mask.basetype() != Type::uint8)
        IF_THROW_EXCEPTION(image_type_error("The mask has a wrong base type: " + to_string(mask.basetype()) +
                                            ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
                << errinfo_image_type(mask.basetype());

    if (!mask.empty() && mask.channels() != 1 && mask.channels() != getChannels(lowType))
        IF_THROW_EXCEPTION(image_type_error("The mask has a wrong number of channels. It has " + std::to_string(mask.channels()) + " channels while the images have "
                                            + std::to_string(getChannels(lowType)) + ". The mask should have either 1 channel or the same number of channels as the images."))
                << errinfo_image_type(mask.type());
}


void StarfmFusor::predict(int date2, ConstImage const& maskParam) {
    checkInputImages(maskParam, date2);
    Rectangle predArea = opt.getPredictionArea();

    // if no prediction area has been set, use full img size
    if (predArea.x == 0 && predArea.y == 0 && predArea.width == 0 && predArea.height == 0) {
        predArea.width  = imgs->getAny().width();
        predArea.height = imgs->getAny().height();
    }

    if (output.size() != predArea.size() || output.type() != imgs->getAny().type())
        output = Image{predArea.width, predArea.height, imgs->get(opt.getHighResTag(), opt.date1).type()}; // create a new one

    // find sample area, i. e. prediction area extended by half window
    Size fullSize = imgs->get(opt.getHighResTag(), opt.date1).size();
    Rectangle sampleArea = findSampleArea(fullSize, predArea);
    predArea.x -= sampleArea.x;
    predArea.y -= sampleArea.y;

    // get input images
    ConstImage sampleMask = maskParam.empty() ? maskParam.sharedCopy() : maskParam.sharedCopy(sampleArea);
    bool isDoublePairMode = opt.isDoublePairModeConfigured();
    std::vector<ConstImage> hkFull{imgs->get(opt.getHighResTag(), opt.date1).sharedCopy()}; // full image used to ensure that prediction area has no influence
    std::vector<ConstImage> hk_vec{imgs->get(opt.getHighResTag(), opt.date1).sharedCopy(sampleArea)};
    std::vector<ConstImage> lk_vec{imgs->get(opt.getLowResTag(),  opt.date1).sharedCopy(sampleArea)};
    ConstImage l2                = imgs->get(opt.getLowResTag(),      date2).sharedCopy(sampleArea);
    if (isDoublePairMode) {
        hkFull.emplace_back(imgs->get(opt.getHighResTag(), opt.date3).sharedCopy());
        hk_vec.emplace_back(imgs->get(opt.getHighResTag(), opt.date3).sharedCopy(sampleArea));
        lk_vec.emplace_back(imgs->get(opt.getLowResTag(),  opt.date3).sharedCopy(sampleArea));
    }

    // spectral and temporal diffs
    std::vector<Image> diffS_vec;
    std::vector<Image> diffT_vec;

    // local values
    Type resType = getResultType(l2.type());
    std::vector<Image> localValues_vec;

    // set tols
    unsigned int imgChans = l2.channels();
    std::vector<std::vector<double>> tol_vec;

    // copied pixels
    Image diffZero;
    if (opt.getDoCopyOnZeroDiff()) {
        diffZero = Image{l2.size(), getFullType(Type::uint8, l2.channels())};
        diffZero.set(0);
    }

    // do it for the pair(s)
    for (unsigned int ip = 0; ip < hk_vec.size(); ++ip) {
        ConstImage const& hk = hk_vec.at(ip);
        ConstImage const& lk = lk_vec.at(ip);

        // spectral and temporal diffs
        diffS_vec.emplace_back(lk.absdiff(hk));
        diffT_vec.emplace_back(lk.absdiff(l2));

        // local values
        Image localValues = hk.add(l2, resType).subtract(lk, resType);
        localValues_vec.emplace_back(localValues.convertTo(l2.type()));

        // set tols
        auto meanStdDev = hkFull.at(ip).meanStdDev(maskParam);
        for (double& sd : meanStdDev.second)
            sd *= 2.0 / opt.getNumberClasses();
        tol_vec.push_back(std::move(meanStdDev.second));

        // set trivial pixels (zero spectral diff) with multi-channel masks to new low res pixels
        if (opt.getDoCopyOnZeroDiff()) {
            Image diffSZero{diffS_vec.back().cvMat() == 0};
            output.copyValuesFrom(l2.sharedCopy(predArea), diffSZero.constSharedCopy(predArea));
            diffZero = std::move(diffZero).bitwise_or(diffSZero);
        }
    }

    // set trivial pixels (zero temporal diff) with multi-channel masks to high res pixels, maybe average
    if (opt.getDoCopyOnZeroDiff()) {
        // high res date 1
        Image diffT1Zero{diffT_vec.front().cvMat() == 0};
        output.copyValuesFrom(hk_vec.front().sharedCopy(predArea), diffT1Zero.constSharedCopy(predArea));
        diffZero = std::move(diffZero).bitwise_or(diffT1Zero);

        if (isDoublePairMode) {
            // high res date 3
            Image diffT2Zero{diffT_vec.back().cvMat() == 0};
            output.copyValuesFrom(hk_vec.back().sharedCopy(predArea), diffT2Zero.constSharedCopy(predArea));
            diffZero = std::move(diffZero).bitwise_or(diffT2Zero);

            // (high res date 1  +  high res date 3) / 2
            Image diffT1And2Zero = diffT1Zero.bitwise_and(std::move(diffT2Zero));
            Image temp{hk_vec.front().constSharedCopy(predArea).cvMat() * 0.5
                      + hk_vec.back().constSharedCopy(predArea).cvMat() * 0.5};
            output.copyValuesFrom(temp, diffT1And2Zero.constSharedCopy(predArea));
        }
    }
//    output.copyValuesFrom(localValues.sharedCopy(predArea));

    // init output as double type (for convenience) and get distance weights
    Image distWeights = computeDistanceWeights();
    unsigned int xmax = predArea.x + predArea.width;
    unsigned int ymax = predArea.y + predArea.height;

    // predict with moving window
    for (unsigned int y = predArea.y; y < ymax; ++y) {
        for (unsigned int x = predArea.x; x < xmax; ++x) {
            Rectangle window((int)x - opt.winSize / 2, (int)y - opt.winSize / 2, opt.winSize, opt.winSize);
            std::vector<ConstImage> hk_win_vec;
            std::vector<ConstImage> dt_win_vec;
            std::vector<ConstImage> ds_win_vec;
            std::vector<ConstImage> lv_win_vec;
            for (unsigned int ip = 0; ip < hk_vec.size(); ++ip) {
                hk_win_vec.emplace_back(hk_vec.at(ip).constSharedCopy(window));
                dt_win_vec.emplace_back(diffT_vec.at(ip).constSharedCopy(window));
                ds_win_vec.emplace_back(diffS_vec.at(ip).constSharedCopy(window));
                lv_win_vec.emplace_back(localValues_vec.at(ip).constSharedCopy(window));
            }
            ConstImage mask_win = sampleMask.empty() ? sampleMask.sharedCopy() : sampleMask.constSharedCopy(window);

            Rectangle dw_crop{std::max(0, -window.x), std::max(0, -window.y),
                              hk_win_vec.front().width(), hk_win_vec.front().height()};
            ConstImage dw_win = distWeights.sharedCopy(dw_crop);

            unsigned int x_win = opt.winSize / 2 - dw_crop.x;
            unsigned int y_win = opt.winSize / 2 - dw_crop.y;
            int x_out = x - predArea.x;
            int y_out = y - predArea.y;
            Rectangle out_pixel_crop{x_out, y_out, 1, 1};
            Image out_pixel{output.sharedCopy(out_pixel_crop)};

            for (unsigned int c = 0; c < imgChans; ++c) {
                unsigned int maskChannel = mask_win.channels() > c ? c : 0;
                if ((!sampleMask.empty() && !sampleMask.boolAt(x, y, maskChannel)) ||
                    (!diffZero.empty() && diffZero.boolAt(x, y, c)))
                {
                    continue;
                }

                CallBaseTypeFunctor::run(starfm_impl_detail::PredictPixel{
                        opt, x_win, y_win, c, tol_vec, dt_win_vec, ds_win_vec, lv_win_vec, hk_win_vec, mask_win, dw_win, out_pixel},
                        output.type());
            }
        }
    }
}


template<Type basetype>
void starfm_impl_detail::PredictPixel::operator()() const {
    assert((opt.isSinglePairModeConfigured() && hk_win_vec.size() == 1) ||
           (opt.isDoublePairModeConfigured() && hk_win_vec.size() == 2));
    using imgval_t = typename DataType<basetype>::base_type;

    // calculate uncertainties
    double sigma_t = opt.getTemporalUncertainty();
    double sigma_s = opt.getSpectralUncertainty();
    double sigma_dt = sigma_t * std::sqrt(2);
    double sigma_ds = std::sqrt(sigma_t * sigma_t + sigma_s * sigma_s);
    double sigma_combined = std::sqrt(sigma_ds * sigma_ds + sigma_dt * sigma_dt);

    // for two pairs choose smaller diffs as filter tolerance
    imgval_t dt_center = cv::saturate_cast<imgval_t>(dt_win_vec.front().at<imgval_t>(x_center, y_center, c) + sigma_dt);
    imgval_t ds_center = cv::saturate_cast<imgval_t>(ds_win_vec.front().at<imgval_t>(x_center, y_center, c) + sigma_ds);
    if (opt.isDoublePairModeConfigured()) {
        dt_center = std::min(dt_center, cv::saturate_cast<imgval_t>(dt_win_vec.back().at<imgval_t>(x_center, y_center, c) + sigma_dt));
        ds_center = std::min(ds_center, cv::saturate_cast<imgval_t>(ds_win_vec.back().at<imgval_t>(x_center, y_center, c) + sigma_ds));
    }

    bool hasCandidate = false;
    double sumWeights  = 0;
    double weightedSum = 0;
    bool useTempDiff = opt.getUseTempDiffForWeights() == StarfmOptions::TempDiffWeighting::enable ||
                       (opt.getUseTempDiffForWeights() == StarfmOptions::TempDiffWeighting::on_double_pair && opt.isDoublePairModeConfigured());
    double logScale = opt.getLogScaleFactor();
    unsigned int maskChannel = mask_win.channels() > c ? c : 0;

    // loop over all (1 or 2) pairs
    unsigned int ymax  = dw_win.height();
    unsigned int xmax  = dw_win.width();
    for (unsigned int ip = 0; ip < hk_win_vec.size(); ++ip) {
        double hk_center = hk_win_vec.at(ip).at<imgval_t>(x_center, y_center, c);

        // loop through window
        for (unsigned int y = 0; y < ymax; ++y) {
            for (unsigned int x = 0; x < xmax; ++x) {
                imgval_t dt = dt_win_vec.at(ip).at<imgval_t>(x, y, c);
                imgval_t ds = ds_win_vec.at(ip).at<imgval_t>(x, y, c);
                imgval_t hk = hk_win_vec.at(ip).at<imgval_t>(x, y, c);

                if ((!mask_win.empty() && !mask_win.boolAt(x, y, maskChannel)) ||                                                // check mask
                    std::abs(hk_center - hk) >= tol_vec.at(ip).at(c)  ||                                                              // check similarity
                    (opt.getUseStrictFiltering()  ?  (dt >= dt_center || ds >= ds_center)  :  (dt >= dt_center && ds >= ds_center)) // check valid or invalid
                   )
                {
                    continue;
                }
                hasCandidate = true;

                if (!useTempDiff)
                    dt = 0;

                double dw = dw_win.at<double>(x, y, 0);
                double weight = 1;
                if (logScale > 0)
                    weight = 1 / (std::log(2 + dt * logScale) * std::log(2 + ds * logScale) * dw);
                else {
                    double dts = (1 + dt) * (1 + ds);
                    if (dts >= sigma_combined)
                        weight = 1 / (dw * dts);
                }

                imgval_t lv = lv_win_vec.at(ip).at<imgval_t>(x, y, c);
                sumWeights  += weight;
                weightedSum += weight * lv;
            }
        }
    }

    imgval_t& out = out_pixel.at<imgval_t>(0, 0, c);
    if (hasCandidate)
        out = weightedSum / sumWeights;
    else
        out = lv_win_vec.front().at<imgval_t>(x_center, y_center, c) * 0.5
            + lv_win_vec.back().at<imgval_t>(x_center, y_center, c)  * 0.5;
}

} /* namespace imagefusion */

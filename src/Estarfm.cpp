#include "Estarfm.h"

#include <boost/math/distributions/fisher_f.hpp>

namespace imagefusion {

void EstarfmFusor::processOptions(Options const& o) {
    options_type newOpts = dynamic_cast<options_type const&>(o);
    if (!newOpts.isDate1Set)
        IF_THROW_EXCEPTION(invalid_argument_error("You have not set the date of the first input pair (date1)."));

    if (!newOpts.isDate3Set)
        IF_THROW_EXCEPTION(invalid_argument_error("You have not set the date of the second input pair (date3)."));

    if (newOpts.getDate1() == newOpts.getDate3())
        IF_THROW_EXCEPTION(invalid_argument_error("The dates for the input pairs have to be different. You chose " + std::to_string(newOpts.getDate1()) + " for both."));

    if (newOpts.getHighResTag() == newOpts.getLowResTag())
        IF_THROW_EXCEPTION(invalid_argument_error("The resolution tags for the input pairs have to be different. You chose '" + newOpts.getHighResTag() + "' for both."));

    opt = newOpts;
}


void EstarfmFusor::checkInputImages(ConstImage const& mask, int date2) const {
    if (!imgs)
        IF_THROW_EXCEPTION(logic_error("No MultiResImage object stored in EstarfmFusor while predicting. This looks like a programming error."));

    std::string strH1 = "High resolution image (tag: " + opt.getHighResTag() + ") at date 1 (date: " + std::to_string(opt.getDate1()) + ")";
    std::string strH3 = "High resolution image (tag: " + opt.getHighResTag() + ") at date 3 (date: " + std::to_string(opt.getDate3()) + ")";
    std::string strL1 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 1 (date: " + std::to_string(opt.getDate1()) + ")";
    std::string strL2 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 2 (date: " + std::to_string(date2)          + ")";
    std::string strL3 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 3 (date: " + std::to_string(opt.getDate3()) + ")";

    if (!imgs->has(opt.getLowResTag(),  opt.getDate1()) || !imgs->has(opt.getLowResTag(),  opt.getDate3()) || !imgs->has(opt.getLowResTag(), date2) ||
        !imgs->has(opt.getHighResTag(), opt.getDate1()) || !imgs->has(opt.getHighResTag(), opt.getDate3()))
    {
        IF_THROW_EXCEPTION(not_found_error("Not all required images are available. For ESTARFM you need to provide:\n"
                                           " * " + strH1 + " [" + (imgs->has(opt.getHighResTag(), opt.getDate1()) ? "" : "NOT ") + "available]\n"
                                           " * " + strH3 + " [" + (imgs->has(opt.getHighResTag(), opt.getDate3()) ? "" : "NOT ") + "available]\n"
                                           " * " + strL1 + " [" + (imgs->has(opt.getLowResTag(),  opt.getDate1()) ? "" : "NOT ") + "available]\n" +
                                           " * " + strL2 + " [" + (imgs->has(opt.getLowResTag(),  date2)          ? "" : "NOT ") + "available]\n" +
                                           " * " + strL3 + " [" + (imgs->has(opt.getLowResTag(),  opt.getDate3()) ? "" : "NOT ") + "available]"));
    }

    Type highType = imgs->get(opt.getHighResTag(), opt.getDate3()).type();
    if (imgs->get(opt.getHighResTag(), opt.getDate1()).type() != highType)
        IF_THROW_EXCEPTION(image_type_error("The data types for the high resolution images are different:\n"
                                            " * " + strH1 + ": " + to_string(imgs->get(opt.getHighResTag(), opt.getDate1()).type()) + " and\n"
                                            " * " + strH3 + ": " + to_string(imgs->get(opt.getHighResTag(), opt.getDate3()).type())));

    Type lowType  = imgs->get(opt.getLowResTag(), opt.getDate3()).type();
    if (imgs->get(opt.getLowResTag(), opt.getDate1()).type() != lowType || imgs->get(opt.getLowResTag(), date2).type() != lowType)
        IF_THROW_EXCEPTION(image_type_error("The data types for the low resolution images are different:\n"
                                            " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(), opt.getDate1()).type()) + ",\n" +
                                            " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(), date2).type())          + " and\n" +
                                            " * " + strL3 + " " + to_string(imgs->get(opt.getLowResTag(), opt.getDate3()).type())));

    Size s = imgs->get(opt.getLowResTag(), opt.getDate3()).size();
    if (imgs->get(opt.getHighResTag(), opt.getDate1()).size() != s ||
        imgs->get(opt.getHighResTag(), opt.getDate3()).size() != s ||
        imgs->get(opt.getLowResTag(),  opt.getDate1()).size() != s ||
        imgs->get(opt.getLowResTag(),  date2).size() != s)
    {
        IF_THROW_EXCEPTION(size_error("The required images have a different size:\n"
                                      " * " + strH1 + " " + to_string(imgs->get(opt.getHighResTag(), opt.getDate1()).size()) + "\n"
                                      " * " + strH3 + " " + to_string(imgs->get(opt.getHighResTag(), opt.getDate3()).size()) + "\n"
                                      " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.getDate1()).size()) + "\n" +
                                      " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(),  date2).size())       + "\n" +
                                      " * " + strL3 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.getDate3()).size())));
    }

    if (!mask.empty() && mask.size() != s)
        IF_THROW_EXCEPTION(size_error("The mask has a wrong size: " + to_string(mask.size()) +
                                      ". It must have the same size as the images: " + to_string(s) + "."))
                << errinfo_size(mask.size());

    if (!mask.empty() && mask.basetype() != Type::uint8)
        IF_THROW_EXCEPTION(image_type_error("The mask has a wrong base type: " + to_string(mask.basetype()) +
                                      ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
                << errinfo_image_type(mask.basetype());

    if (getChannels(lowType) != getChannels(highType))
        IF_THROW_EXCEPTION(image_type_error("The number of channels of the low resolution images (" + std::to_string(getChannels(lowType)) +
                                            ") are different than of the high resolution images (" + std::to_string(getChannels(highType)) + ")."));

    if (!mask.empty() && mask.channels() != 1 && mask.channels() != getChannels(lowType))
        IF_THROW_EXCEPTION(image_type_error("The mask has a wrong number of channels. It has " + std::to_string(mask.channels()) + " channels while the images have "
                                            + std::to_string(getChannels(lowType)) + ". The mask should have either 1 channel or the same number of channels as the images."))
                << errinfo_image_type(mask.type());
}


Rectangle EstarfmFusor::findSampleArea(Size const& fullImgSize, Rectangle const& predArea) const {
    Rectangle sampleArea = predArea;
    sampleArea.x -= opt.getWinSize() / 2;
    sampleArea.y -= opt.getWinSize() / 2;
    sampleArea.width  += opt.getWinSize() - 1;
    sampleArea.height += opt.getWinSize() - 1;

    return sampleArea & Rectangle(0, 0, fullImgSize.width, fullImgSize.height);
}


Image EstarfmFusor::computeDistanceWeights() const {
    Image distWeights(opt.getWinSize(), opt.getWinSize(), Type::float64x1);
    for (int x = 0; x <= (int)(opt.getWinSize()) / 2; ++x) {
        for (int y = x; y >= 0; --y) {
            int xp = opt.getWinSize() / 2 + x;
            int xn = opt.getWinSize() / 2 - x;
            int yp = opt.getWinSize() / 2 + y;
            int yn = opt.getWinSize() / 2 - y;
            double d = std::sqrt(x*x + y*y) * 2. / opt.getWinSize() + 1.0;

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


template<Type basetype>
Image estarfm_impl_detail::ComputeLocalWeights::operator()() const {
    using imgval_t = typename DataType<basetype>::base_type;

    Image weights{l1.size(), Type::float64x1};
    unsigned int ymax  = l1.height();
    unsigned int xmax  = l1.width();
    unsigned int cmax  = l1.channels();
    unsigned int mcmax = m.channels();
    for (unsigned int y = 0; y < ymax; ++y) {
        for (unsigned int x = 0; x < xmax; ++x) {
            std::vector<imgval_t> lowVec(2*cmax);
            std::vector<imgval_t> highVec(2*cmax);
            for (unsigned int c = 0; c < cmax; ++c) {
                unsigned int maskChannel = c < mcmax ? c : 0;
                if (!m.empty() && !m.boolAt(x, y, maskChannel))
                    continue;

                highVec.at(2*c)   = h1.at<imgval_t>(x, y, c);
                highVec.at(2*c+1) = h3.at<imgval_t>(x, y, c);
                lowVec.at( 2*c)   = l1.at<imgval_t>(x, y, c);
                lowVec.at( 2*c+1) = l3.at<imgval_t>(x, y, c);
            }

            if (std::equal(highVec.begin() + 1, highVec.end(), highVec.begin()) ||
                std::equal( lowVec.begin() + 1,  lowVec.end(),  lowVec.begin()))
            {
                weights.at<double>(x, y, 0) = 1;
            }
            else {
                weights.at<double>(x, y, 0) = correlate(lowVec, highVec);
                if (std::isnan(weights.at<double>(x, y, 0)))
                    IF_THROW_EXCEPTION(logic_error("Correlation coefficient NaN, although elements differen!?!"));
            }
        }
    }
    return weights;
}


template<Type basetype>
estarfm_impl_detail::SumAndTolHelper::Stats estarfm_impl_detail::SumAndTolHelper::collectStats(
        ConstImage h1_win, ConstImage h3_win, ConstImage l1_win, ConstImage l2_win, ConstImage l3_win, ConstImage m_win, unsigned int c) const
{
    using imgval_t = typename DataType<basetype>::base_type;
    Stats s;
    unsigned int maskChannel = c < m_win.channels() ? c : 0;
    int xmax = h1_win.width();
    int ymax = h1_win.height();
    for (int y = 0; y < ymax; ++y) {
        for (int x = 0; x < xmax; ++x) {
            if (!m_win.empty() && !m_win.boolAt(x, y, maskChannel))
                continue;

            if (opt.getUseLocalTol()) {
                imgval_t h1w = h1_win.at<imgval_t>(x, y, c);
                imgval_t h3w = h3_win.at<imgval_t>(x, y, c);

                s.sum_h1 += h1w;
                s.sum_h3 += h3w;
                s.sqrsum_h1 += (double)h1w * h1w;
                s.sqrsum_h3 += (double)h3w * h3w;
                ++s.cnt_h1;
                ++s.cnt_h3;
            }

            s.sum_l1 += l1_win.at<imgval_t>(x, y, c);
            s.sum_l2 += l2_win.at<imgval_t>(x, y, c);
            s.sum_l3 += l3_win.at<imgval_t>(x, y, c);
        }
    }
    return s;
}

template<Type basetype>
void estarfm_impl_detail::SumAndTolHelper::operator()() {
    unsigned int imgChans = l2.channels();
    Rectangle window(predArea.x - opt.getWinSize() / 2, predArea.y - opt.getWinSize() / 2, opt.getWinSize(), opt.getWinSize());
    for (unsigned int c = 0; c < imgChans; ++c) {

        // stats for y movement
        Stats stats_y = collectStats<basetype>(h1.sharedCopy(window), h3.sharedCopy(window), l1.sharedCopy(window), l2.sharedCopy(window), l3.sharedCopy(window), m.empty() ? m.sharedCopy() : m.sharedCopy(window), c);

        // move window in y direction
        for (int y_off = 0; y_off < predArea.height; ++y_off) {
            if (y_off != 0) {
                // subtract old upper bound
                int y_up = window.y + y_off - 1;
                if (y_up >= 0 && y_up < l2.height()) {
                    Rectangle upper(window.x, y_up, window.width, 1);
                    stats_y -= collectStats<basetype>(h1.sharedCopy(upper), h3.sharedCopy(upper), l1.sharedCopy(upper), l2.sharedCopy(upper), l3.sharedCopy(upper), m.empty() ? m.sharedCopy() : m.sharedCopy(upper), c);
                }

                // add new lower bound
                int y_low = window.y + window.height + y_off - 1;
                if (y_low >= 0 && y_low < l2.height()) {
                    Rectangle lower(window.x, y_low, window.width, 1);
                    stats_y += collectStats<basetype>(h1.sharedCopy(lower), h3.sharedCopy(lower), l1.sharedCopy(lower), l2.sharedCopy(lower), l3.sharedCopy(lower), m.empty() ? m.sharedCopy() : m.sharedCopy(lower), c);
                }
            }

            // stats for x movement
            Stats stats_x = stats_y;

            // move window in x direction
            for (int x_off = 0; x_off < predArea.width; ++x_off) {
                if (x_off != 0) {
                    // subtract old left bound
                    int x_left = window.x + x_off - 1;
                    if (x_left >= 0 && x_left < l2.width()) {
                        Rectangle left(x_left, window.y + y_off, 1, window.height);
                        stats_x -= collectStats<basetype>(h1.sharedCopy(left), h3.sharedCopy(left), l1.sharedCopy(left), l2.sharedCopy(left), l3.sharedCopy(left), m.empty() ? m.sharedCopy() : m.sharedCopy(left), c);
                    }

                    // add new right bound
                    int x_right = window.x + window.width + x_off - 1;
                    if (x_right >= 0 && x_right < l2.width()) {
                        Rectangle right(x_right, window.y + y_off, 1, window.height);
                        stats_x += collectStats<basetype>(h1.sharedCopy(right), h3.sharedCopy(right), l1.sharedCopy(right), l2.sharedCopy(right), l3.sharedCopy(right), m.empty() ? m.sharedCopy() : m.sharedCopy(right), c);
                    }
                }

                // set results
                if (opt.getUseLocalTol()) {
                    double stddev1 = std::sqrt(stats_x.sqrsum_h1 / stats_x.cnt_h1 - (stats_x.sum_h1 / stats_x.cnt_h1) * (stats_x.sum_h1 / stats_x.cnt_h1));
                    tol1.at<double>(x_off, y_off, c) = stddev1 * (2.0 / opt.getNumberClasses());

                    double stddev3 = std::sqrt(stats_x.sqrsum_h3 / stats_x.cnt_h3 - (stats_x.sum_h3 / stats_x.cnt_h3) * (stats_x.sum_h3 / stats_x.cnt_h3));
                    tol3.at<double>(x_off, y_off, c) = stddev3 * (2.0 / opt.getNumberClasses());
                }

                sumL1.at<double>(x_off, y_off, c) = stats_x.sum_l1;
                sumL2.at<double>(x_off, y_off, c) = stats_x.sum_l2;
                sumL3.at<double>(x_off, y_off, c) = stats_x.sum_l3;
            }
        }
    }
}

void EstarfmFusor::predict(int date2, ConstImage const& maskParam) {
    checkInputImages(maskParam, date2);
    Rectangle predArea = opt.getPredictionArea();

    // if no prediction area has been set, use full img size
    if (predArea.x == 0 && predArea.y == 0 && predArea.width == 0 && predArea.height == 0) {
        predArea.width  = imgs->getAny().width();
        predArea.height = imgs->getAny().height();
    }

    if (output.size() != predArea.size() || output.type() != imgs->getAny().type())
        output = Image{predArea.width, predArea.height, imgs->get(opt.getHighResTag(), opt.getDate1()).type()}; // create a new one

    // find sample area, i. e. prediction area extended by half window
    Size fullSize = imgs->get(opt.getHighResTag(), opt.getDate1()).size();
    Rectangle sampleArea = findSampleArea(fullSize, predArea);
    predArea.x -= sampleArea.x;
    predArea.y -= sampleArea.y;

    // get input images
    ConstImage const& h1_full = imgs->get(opt.getHighResTag(), opt.getDate1());
    ConstImage const& h3_full = imgs->get(opt.getHighResTag(), opt.getDate3());
    ConstImage h1 = imgs->get(opt.getHighResTag(), opt.getDate1()).sharedCopy(sampleArea);
    ConstImage h3 = imgs->get(opt.getHighResTag(), opt.getDate3()).sharedCopy(sampleArea);
    ConstImage l1 = imgs->get(opt.getLowResTag(),  opt.getDate1()).sharedCopy(sampleArea);
    ConstImage l2 = imgs->get(opt.getLowResTag(),      date2).sharedCopy(sampleArea);
    ConstImage l3 = imgs->get(opt.getLowResTag(),  opt.getDate3()).sharedCopy(sampleArea);
    ConstImage sampleMask = maskParam.empty() ? maskParam.sharedCopy() : maskParam.sharedCopy(sampleArea);

    // init output as double type (for convenience) and get distance weights, local weights
    Image distWeights = computeDistanceWeights();
    Image localWeights = computeLocalWeights(h1, h3, l1, l3, sampleMask);

    // calculate the tolerances (local or global)
    unsigned int chans = l2.channels();
    std::vector<double> tol1(l2.channels()), tol3(l2.channels()), sumL1(l2.channels()), sumL2(l2.channels()), sumL3(l2.channels());
    estarfm_impl_detail::SumAndTolHelper sum_tol{opt, h1, h3, l1, l2, l3, sampleMask, predArea};
    if (!opt.getUseLocalTol()) {
        auto meanStdDev1 = h1_full.meanStdDev(maskParam);
        auto meanStdDev3 = h3_full.meanStdDev(maskParam);
        for (unsigned int c = 0; c < chans; ++c) {
            tol1.at(c) = meanStdDev1.second.at(c) * (2.0 / opt.getNumberClasses());
            tol3.at(c) = meanStdDev3.second.at(c) * (2.0 / opt.getNumberClasses());
        }
    }

    unsigned int xmax = predArea.x + predArea.width;
    unsigned int ymax = predArea.y + predArea.height;

    // predict with moving window
    for (unsigned int y = predArea.y; y < ymax; ++y) {
        for (unsigned int x = predArea.x; x < xmax; ++x) {
            Rectangle window((int)x - opt.getWinSize() / 2, (int)y - opt.getWinSize() / 2, opt.getWinSize(), opt.getWinSize());
            ConstImage h1_win  = h1.constSharedCopy(window);
            ConstImage h3_win  = h3.constSharedCopy(window);
            ConstImage l1_win  = l1.constSharedCopy(window);
            ConstImage l2_win  = l2.constSharedCopy(window);
            ConstImage l3_win  = l3.constSharedCopy(window);
            ConstImage lw_win  = localWeights.constSharedCopy(window);
            ConstImage sm_win  = sampleMask.empty() ? sampleMask.constSharedCopy() : sampleMask.constSharedCopy(window);

            Rectangle dw_crop{std::max(0, -window.x), std::max(0, -window.y),
                              h1_win.width(), h1.height()};
            ConstImage dw_win = distWeights.sharedCopy(dw_crop);

            int x_out = x - predArea.x;
            int y_out = y - predArea.y;
            Rectangle out_pixel_crop{x_out, y_out, 1, 1};
            Image out_pixel{output.sharedCopy(out_pixel_crop)};

            for (unsigned int c = 0; c < chans; ++c) {
                if (!sum_tol.tol1.empty() && !sum_tol.tol3.empty()) { // i. e. !opt.getUseLocalTol()
                    tol1.at(c)  = sum_tol.tol1.at<double>(x_out, y_out, c);
                    tol3.at(c)  = sum_tol.tol3.at<double>(x_out, y_out, c);
                }
                sumL1.at(c) = sum_tol.sumL1.at<double>(x_out, y_out, c);
                sumL2.at(c) = sum_tol.sumL2.at<double>(x_out, y_out, c);
                sumL3.at(c) = sum_tol.sumL3.at<double>(x_out, y_out, c);
            }

            unsigned int x_win = opt.getWinSize() / 2 - dw_crop.x;
            unsigned int y_win = opt.getWinSize() / 2 - dw_crop.y;
            CallBaseTypeFunctor::run(estarfm_impl_detail::PredictPixel{
                    opt, x_win, y_win, h1_win, h3_win, l1_win, l2_win, l3_win, lw_win, dw_win, sm_win, tol1, tol3, sumL1, sumL2, sumL3, out_pixel},
                    output.type());
        }
    }
}


template<Type basetype>
void estarfm_impl_detail::PredictPixel::operator()() const {
    using imgval_t = typename DataType<basetype>::base_type;

    imgval_t const* h1c_p = &h1_win.at<imgval_t>(x_center, y_center, 0);
    imgval_t const* h3c_p = &h3_win.at<imgval_t>(x_center, y_center, 0);

    unsigned int imgChans = h1_win.channels();
    unsigned int ymax  = l2_win.height();
    unsigned int xmax  = l2_win.width();

    // loop over candidates and collect information, outer vector for channels, inner for candidates
    std::vector<std::vector<imgval_t>> lowCands_vecs(imgChans);
    std::vector<std::vector<imgval_t>> highCands_vecs(imgChans);
    for (unsigned int c = 0; c < imgChans; ++c) {
        lowCands_vecs.at(c).reserve(xmax * ymax / 10);
        highCands_vecs.at(c).reserve(xmax * ymax / 10);
    }

    std::vector<double> sumsWeights      (imgChans, 0);
    std::vector<double> weightedPredSums1(imgChans, 0);
    std::vector<double> weightedPredSums3(imgChans, 0);
    std::vector<double> weightedFineSums1(imgChans, 0);
    std::vector<double> weightedFineSums3(imgChans, 0);
    for (unsigned int y = 0; y < ymax; ++y) {
        for (unsigned int x = 0; x < xmax; ++x) {
            imgval_t const* h1w_p = &h1_win.at<imgval_t>(x, y, 0);
            imgval_t const* h3w_p = &h3_win.at<imgval_t>(x, y, 0);
            bool isCand = true;
            for (unsigned int c = 0; c < imgChans; ++c) {
                unsigned int maskChannel = sm_win.channels() > c ? c : 0;
                if ((!sm_win.empty() && !sm_win.boolAt(x, y, maskChannel)) ||
                    std::abs(h1c_p[c] - h1w_p[c]) > tol1[c] ||
                    std::abs(h3c_p[c] - h3w_p[c]) > tol3[c]) // (abs would not work for uint32_t, but uint32_t is not supported anyways!)
                {
                    isCand = false;
                    break;
                }
            }
            if (!isCand)
                continue;

            double lw  = lw_win.at<double>(x, y, 0);
            double dw  = dw_win.at<double>(x, y, 0);
            double weight = 1 / ((1 - lw) * dw + 1e-7);
            imgval_t const* l1w_p = &l1_win.at<imgval_t>(x, y, 0);
            imgval_t const* l2w_p = &l2_win.at<imgval_t>(x, y, 0);
            imgval_t const* l3w_p = &l3_win.at<imgval_t>(x, y, 0);
            for (unsigned int c = 0; c < imgChans; ++c) {
                lowCands_vecs[c].push_back(l1w_p[c]);
                lowCands_vecs[c].push_back(l3w_p[c]);
                highCands_vecs[c].push_back(h1w_p[c]);
                highCands_vecs[c].push_back(h3w_p[c]);

                sumsWeights[c] += weight;
                weightedPredSums1[c] += (l2w_p[c] - l1w_p[c]) * weight /* * reg */;
                weightedPredSums3[c] += (l2w_p[c] - l3w_p[c]) * weight /* * reg */;
                weightedFineSums1[c] += h1w_p[c] * weight;
                weightedFineSums3[c] += h3w_p[c] * weight;
            }
        }
    }

    // loop over channels and predict pixel
    for (unsigned int c = 0; c < imgChans; ++c) {
        unsigned int maskChannel = sm_win.channels() > c ? c : 0;
        if (!sm_win.empty() && !sm_win.boolAt(x_center, y_center, maskChannel))
            continue;

        // temporal weights
        double T12 = 1. / (std::abs(sumL1[c] - sumL2[c]) + 1e-10);
        double T32 = 1. / (std::abs(sumL3[c] - sumL2[c]) + 1e-10);
        double T12Norm = T12 / (T12 + T32);
        double T32Norm = T32 / (T12 + T32);

        // predict
        imgval_t& out = out_pixel.at<imgval_t>(0, 0, c);
        unsigned int nCand = lowCands_vecs.front().size() / 2;
        if (nCand <= 5)
            out = T12Norm * h1c_p[c] + T32Norm * h3c_p[c];
        else {
            // regression coefficient
            std::vector<imgval_t> const& lowCands  = lowCands_vecs[c];
            std::vector<imgval_t> const& highCands = highCands_vecs[c];
            double reg = 1;
            cv::Scalar mean, stddev;
            if (opt.isDataRangeSet()) // calculate only, when needed
                cv::meanStdDev(cv::Mat(lowCands), mean, stddev);
            if (!opt.isDataRangeSet() || stddev[0] * std::sqrt((double)(2*nCand) / (2*nCand-1)) > opt.getDataRangeMax() * opt.getUncertaintyFactor() * std::sqrt(2))
                reg = regress(lowCands, highCands, opt.getUseQualityWeightedRegression());

            out = T12Norm * (h1c_p[c] + reg * weightedPredSums1[c] / sumsWeights[c])
                      + T32Norm * (h3c_p[c] + reg * weightedPredSums3[c] / sumsWeights[c]);

            if (opt.isDataRangeSet() && (out < opt.getDataRangeMin() || out > opt.getDataRangeMax())) {
                out = T12Norm * weightedFineSums1[c] / sumsWeights[c]
                          + T32Norm * weightedFineSums3[c] / sumsWeights[c];
            }
        }
    }
}


} /* namespace imagefusion */

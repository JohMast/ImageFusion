#include "fitfc.h"

namespace imagefusion {

void FitFCFusor::processOptions(Options const& o) {
    options_type newOpts = dynamic_cast<options_type const&>(o);
    if (!newOpts.isDate1Set)
        IF_THROW_EXCEPTION(runtime_error("No input pair date has been set. The pair date is required for prediction"));

    if (newOpts.highTag == newOpts.lowTag)
        IF_THROW_EXCEPTION(invalid_argument_error("The resolution tags for the input pairs have to be different. You chose '" + newOpts.highTag + "' for both."));

    opt = newOpts;
}

Rectangle FitFCFusor::findSampleArea(Size const& fullImgSize, Rectangle const& predArea) const {
    unsigned int spare = opt.getWinSize();
    Rectangle sampleArea = predArea;
    sampleArea.x -= spare;
    sampleArea.y -= spare;
    sampleArea.width  += 2 * spare;
    sampleArea.height += 2 * spare;

    return sampleArea & Rectangle(0, 0, fullImgSize.width, fullImgSize.height);
}


Image FitFCFusor::computeDistanceWeights() const {
    Image distWeights(opt.getWinSize(), opt.getWinSize(), Type::float64x1);
    for (int x = 0; x <= (int)(opt.getWinSize()) / 2; ++x) {
        for (int y = x; y >= 0; --y) {
            int xp = opt.getWinSize() / 2 + x;
            int xn = opt.getWinSize() / 2 - x;
            int yp = opt.getWinSize() / 2 + y;
            int yn = opt.getWinSize() / 2 - y;
            double d = 1 / (std::sqrt(x*x + y*y) * 2 / opt.getWinSize() + 1.0);

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


void FitFCFusor::checkInputImages(ConstImage const& mask, int date2) const {
    if (!imgs)
        IF_THROW_EXCEPTION(logic_error("No MultiResImage object stored in FitFCFusor while predicting. This looks like a programming error."));

    std::string strH1 = "High resolution image (tag: " + opt.getHighResTag() + ") at date 1 (date: " + std::to_string(opt.getPairDate()) + ")";
    std::string strL1 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 1 (date: " + std::to_string(opt.getPairDate()) + ")";
    std::string strL2 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 2 (date: " + std::to_string(    date2)         + ")";

    if (!imgs->has(opt.getHighResTag(),  opt.getPairDate()) || !imgs->has(opt.getLowResTag(), opt.getPairDate()) || !imgs->has(opt.getLowResTag(), date2)) {
        IF_THROW_EXCEPTION(not_found_error("Not all required images are available. For STARFM you need to provide:\n"
                                           " * " + strH1 + " [" + (imgs->has(opt.getHighResTag(), opt.getPairDate()) ? "" : "NOT ") + "available]\n" +
                                           " * " + strL1 + " [" + (imgs->has(opt.getLowResTag(),  opt.getPairDate()) ? "" : "NOT ") + "available]\n" +
                                           " * " + strL2 + " [" + (imgs->has(opt.getLowResTag(),      date2)         ? "" : "NOT ") + "available]\n"));
    }

    Type highType = imgs->get(opt.getHighResTag(), opt.getPairDate()).type();

    Type lowType  = imgs->get(opt.getLowResTag(),  opt.getPairDate()).type();
    if (imgs->get(opt.getLowResTag(), date2).type() != lowType)
        IF_THROW_EXCEPTION(image_type_error("The data types for the low resolution images are different:\n"
                                            " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(), opt.getPairDate()).type()) + "\n" +
                                            " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(),     date2).type())         + "\n"));

    if (getBaseType(lowType) != getBaseType(highType))
        IF_THROW_EXCEPTION(image_type_error("The base data types for the high resolution image (" + to_string(getBaseType(lowType)) +
                                            ") and the low resolution images (" + to_string(getBaseType(lowType)) + ") are different."));

    if (getChannels(lowType) != getChannels(highType))
        IF_THROW_EXCEPTION(image_type_error("The number of channels of the low resolution images (" + std::to_string(getChannels(lowType)) +
                                            ") are different than of the high resolution images (" + std::to_string(getChannels(highType)) + ")."));

    Size s = imgs->get(opt.getLowResTag(), opt.getPairDate()).size();
    if (imgs->get(opt.getHighResTag(), opt.getPairDate()).size() != s || imgs->get(opt.getLowResTag(), date2).size() != s) {
        IF_THROW_EXCEPTION(size_error("The required images have a different size:\n"
                                      " * " + strH1 + " " + to_string(imgs->get(opt.getHighResTag(), opt.getPairDate()).size()) + "\n"
                                      " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.getPairDate()).size()) + "\n" +
                                      " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(),      date2).size())         + "\n"));
    }

    if (s.width < opt.getResolutionFactor() || s.height < opt.getResolutionFactor())
        IF_THROW_EXCEPTION(size_error("The images are smaller (in pixels) than the resolution scale factor. "
                                      "Size is " + to_string(s) + " and factor is " + std::to_string(opt.getResolutionFactor()) + ".\n"))
                << errinfo_size(s);

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



template<Type basetype>
std::pair<Image, Image> fitfc_impl_detail::RegressionMapper::operator()() const {
    assert(m.empty() || m.channels() == 1 && "Regression Mapper expects a single channel mask.");

    // init output for RM prediction ^FRM and residual R
    Image frm = Image(h1.size(), h1.type());
    Image r   = Image(h1.size(), getFullType(Type::float64, h1.channels()));

    using imgval_t = typename DataType<basetype>::base_type;
    unsigned int imgChans = h1.channels();
    unsigned int xmax = h1.width();
    unsigned int ymax = h1.height();
    Rectangle window(-static_cast<int>(opt.getWinSize()) / 2, -static_cast<int>(opt.getWinSize()) / 2,
                     opt.getWinSize(), opt.getWinSize());

    #pragma omp parallel for num_threads(opt.getNumberThreads())
    for (unsigned int c = 0; c < imgChans; ++c) {

        // stats for y movement
        Stats stats_y = collectStats<imgval_t>(h1.sharedCopy(window), l1.sharedCopy(window), l2.sharedCopy(window), m.empty() ? m.sharedCopy() : m.sharedCopy(window), c);

        // move window in y direction
        for (unsigned int y_off = 0; y_off < ymax; ++y_off) {
            if (y_off != 0) {
                // subtract old upper bound
                int y_up = window.y + y_off - 1;
                if (y_up >= 0 && y_up < h1.height()) {
                    Rectangle upper(window.x, y_up, window.width, 1);
                    stats_y -= collectStats<imgval_t>(h1.sharedCopy(upper), l1.sharedCopy(upper), l2.sharedCopy(upper), m.empty() ? m.sharedCopy() : m.sharedCopy(upper), c);
                }

                // add new lower bound
                int y_low = window.y + window.height + y_off - 1;
                if (y_low >= 0 && y_low < h1.height()) {
                    Rectangle lower(window.x, y_low, window.width, 1);
                    stats_y += collectStats<imgval_t>(h1.sharedCopy(lower), l1.sharedCopy(lower), l2.sharedCopy(lower), m.empty() ? m.sharedCopy() : m.sharedCopy(lower), c);
                }
            }

            // stats for x movement
            Stats stats_x = stats_y;

            // move window in x direction
            for (unsigned int x_off = 0; x_off < xmax; ++x_off) {
                if (x_off != 0) {
                    // subtract old left bound
                    int x_left = window.x + x_off - 1;
                    if (x_left >= 0 && x_left < h1.width()) {
                        Rectangle left(x_left, window.y + y_off, 1, window.height);
                        stats_x -= collectStats<imgval_t>(h1.sharedCopy(left), l1.sharedCopy(left), l2.sharedCopy(left), m.empty() ? m.sharedCopy() : m.sharedCopy(left), c);
                    }

                    // add new right bound
                    int x_right = window.x + window.width + x_off - 1;
                    if (x_right >= 0 && x_right < h1.width()) {
                        Rectangle right(x_right, window.y + y_off, 1, window.height);
                        stats_x += collectStats<imgval_t>(h1.sharedCopy(right), l1.sharedCopy(right), l2.sharedCopy(right), m.empty() ? m.sharedCopy() : m.sharedCopy(right), c);
                    }
                }

                // regress
                imgval_t h1_val = h1.at<imgval_t>(x_off, y_off, c);
                imgval_t l1_val = l1.at<imgval_t>(x_off, y_off, c);
                imgval_t l2_val = l2.at<imgval_t>(x_off, y_off, c);

                auto frmVal_and_rVal = regressPixel<imgval_t>(stats_x, h1_val, l1_val, l2_val);
                frm.at<imgval_t>(x_off, y_off, c) = frmVal_and_rVal.first;
                r.at<double>(x_off, y_off, c)     = frmVal_and_rVal.second;
            }
        }
    }

    return std::make_pair(frm, r);
}

template<typename imgval_t>
fitfc_impl_detail::RegressionMapper::Stats fitfc_impl_detail::RegressionMapper::collectStats(
        ConstImage h1_win, ConstImage l1_win, ConstImage l2_win, ConstImage m_win, int c) const
{
    Stats s;
    int xmax = h1_win.width();
    int ymax = h1_win.height();
    for (int y = 0; y < ymax; ++y) {
        for (int x = 0; x < xmax; ++x) {
            if (!m_win.empty() && !m_win.boolAt(x, y, 0))
                continue;

            imgval_t l1xy = l1_win.at<imgval_t>(x, y, c);
            imgval_t l2xy = l2_win.at<imgval_t>(x, y, c);
            s.x_dot_1 += l1xy;
            s.y_dot_1 += l2xy;
            s.x_dot_x += (double)l1xy * l1xy;
            s.x_dot_y += (double)l1xy * l2xy;
            ++s.n;
        }
    }
    return s;
}

template<typename imgval_t>
std::pair<imgval_t, double> fitfc_impl_detail::RegressionMapper::regressPixel(
        Stats s, imgval_t h1_val, imgval_t l1_val, imgval_t l2_val) const
{
    double det = s.n * s.x_dot_x - s.x_dot_1 * s.x_dot_1;

    if (std::abs(det) < 1e-14)
        return std::pair<imgval_t, double>(h1_val, l2_val - l1_val);

    // find factors a,b in p(x) = a * x + b
    double a = (s.n * s.x_dot_y - s.x_dot_1 * s.y_dot_1) / det;
    double b = (s.x_dot_x * s.y_dot_1 - s.x_dot_1 * s.x_dot_y) / det;

    double frm_val = a * h1_val + b;
    double res_val = l2_val - (a * l1_val + b);

    return std::make_pair(cv::saturate_cast<imgval_t>(frm_val), res_val);
}


std::pair<Image, Image> FitFCFusor::regress(ConstImage const& h1, ConstImage const& l1, ConstImage const& l2, ConstImage const& mask) const {
    return CallBaseTypeFunctor::run(fitfc_impl_detail::RegressionMapper{opt, h1, l1, l2, mask}, h1.type());
}

Image fitfc_impl_detail::cubic_filter(Image i, double scale) {
    if (scale == 1)
        return i;

    auto s = i.size();
    cv::Mat small;
    cv::resize(i.cvMat(), small, cv::Size(), 1/scale, 1/scale, cv::INTER_AREA);
    cv::resize(small, i.cvMat(), s, 0, 0, cv::INTER_CUBIC);
    return i;
}

void FitFCFusor::predict(int date2, ConstImage const& maskParam) {
    checkInputImages(maskParam, date2);
    if (opt.getNumberNeighbors() > opt.getWinSize() * opt.getWinSize()) {
        std::cerr << "Warning: You acquired more neighbors (" << opt.getNumberNeighbors()
                  << ") than pixels in the window (" << (opt.getWinSize() * opt.getWinSize()) << "). Using all pixels in the window." << std::endl;
        opt.setNumberNeighbors(opt.getWinSize() * opt.getWinSize());
    }

    Rectangle predArea = opt.getPredictionArea();

    // if no prediction area has been set, use full img size
    if (predArea.x == 0 && predArea.y == 0 && predArea.width == 0 && predArea.height == 0) {
        predArea.width  = imgs->getAny().width();
        predArea.height = imgs->getAny().height();
    }

    if (output.size() != predArea.size() || output.type() != imgs->getAny().type())
        output = Image{predArea.width, predArea.height, imgs->get(opt.getHighResTag(), opt.getPairDate()).type()}; // create a new one

    // check whether low resolution images are nearest neighbor interpolated
//    Image nnmap = getNNMap(imgs->get(opt.getLowResTag(), date2));

    // find sample area, i. e. prediction area extended by half window
    Size fullSize = imgs->get(opt.getHighResTag(), opt.getPairDate()).size();
    Rectangle sampleArea = findSampleArea(fullSize, predArea);
    predArea.x -= sampleArea.x;
    predArea.y -= sampleArea.y;

    // get input images and single-channel mask
    ConstImage sampleMask;
    if (maskParam.empty())
        sampleMask = maskParam.sharedCopy();
    else {
        sampleMask = maskParam.sharedCopy(sampleArea);
        if (maskParam.channels() > 1)
            sampleMask = sampleMask.createSingleChannelMaskFromRange({Interval::closed(1, 255)});
    }

    ConstImage h1 = imgs->get(opt.getHighResTag(), opt.getPairDate()).sharedCopy(sampleArea);
    ConstImage l1 = imgs->get(opt.getLowResTag(),  opt.getPairDate()).sharedCopy(sampleArea);
    ConstImage l2 = imgs->get(opt.getLowResTag(),      date2).sharedCopy(sampleArea);

    // coarse regression model and coarse residual
    auto frm_and_r = regress(h1, l1, l2, sampleMask);
    Image& frm = frm_and_r.first;
    Image& r   = frm_and_r.second;

    // cubic interpolation of residual to make it fine
    r = fitfc_impl_detail::cubic_filter(std::move(r), opt.getResolutionFactor());

    // get distance weights
    Image distWeights = computeDistanceWeights();
    unsigned int xmax = predArea.x + predArea.width;
    unsigned int ymax = predArea.y + predArea.height;

    // predict with moving window
    #pragma omp parallel for num_threads(opt.getNumberThreads())
    for (unsigned int y = predArea.y; y < ymax; ++y) {
        for (unsigned int x = predArea.x; x < xmax; ++x) {
            if (!sampleMask.empty() && !sampleMask.boolAt(x, y, 0))
                continue;

            Rectangle window((int)x - opt.getWinSize() / 2, (int)y - opt.getWinSize() / 2, opt.getWinSize(), opt.getWinSize());
            ConstImage h1_win = h1.constSharedCopy(window);
            ConstImage frm_win = frm.constSharedCopy(window);
            ConstImage r_win = r.constSharedCopy(window);
            ConstImage mask_win = sampleMask.empty() ? sampleMask.sharedCopy() : sampleMask.constSharedCopy(window);

            Rectangle dw_crop{std::max(0, -window.x), std::max(0, -window.y),
                              h1_win.width(), h1_win.height()};

            ConstImage dw_win = distWeights.sharedCopy(dw_crop);

            unsigned int x_win = opt.getWinSize() / 2 - dw_crop.x;
            unsigned int y_win = opt.getWinSize() / 2 - dw_crop.y;
            int x_out = x - predArea.x;
            int y_out = y - predArea.y;
            Rectangle out_pixel_crop{x_out, y_out, 1, 1};
            Image out_pixel{output.sharedCopy(out_pixel_crop)};

            CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                        opt, x_win, y_win, h1_win, frm_win, r_win, mask_win, dw_win, out_pixel},
                        output.type());
        }
    }
}

template<Type basetype>
void fitfc_impl_detail::FilterStep::operator()() const {
    using imgval_t = typename DataType<basetype>::base_type;

    unsigned int imgChans = h1_win.channels();
    std::vector<imgval_t> h1_center(imgChans);
    for (unsigned int c = 0; c < imgChans; ++c)
        h1_center.at(c) = h1_win.at<imgval_t>(x_center, y_center, c);

    unsigned int ymax  = dw_win.height();
    unsigned int xmax  = dw_win.width();
    std::vector<Score> allScores;
    allScores.reserve(xmax * ymax);
    for (unsigned int y = 0; y < ymax; ++y) { // Note: this loop requires the most time (approx. 70%) of the whole algorithm
        for (unsigned int x = 0; x < xmax; ++x) {
            if (!mask_win.empty() && !mask_win.boolAt(x, y, 0))
                continue;

            double diff = 0;
            for (unsigned int c = 0; c < imgChans; ++c) {
                double d = h1_win.at<imgval_t>(x, y, c) - h1_center.at(c);
                diff += d * d;
            }
            allScores.emplace_back(diff, x, y, x_center, y_center);
        }
    }

    unsigned int n = std::min(static_cast<size_t>(opt.getNumberNeighbors()), allScores.size());
    std::partial_sort(allScores.begin(), allScores.begin() + n, allScores.end());

    double invSumWeights = 0;
    std::vector<double> f2(imgChans);
    for (auto it = allScores.begin(), it_last = allScores.begin() + n; it != it_last; ++it) {
        invSumWeights += dw_win.at<double>(it->x, it->y);
        double w = dw_win.at<double>(it->x, it->y);
        for (unsigned int c = 0; c < imgChans; ++c) {
            double frm_val = frm_win.at<imgval_t>(it->x, it->y, c);
            double r_val = r_win.at<double>(it->x, it->y, c);
            f2.at(c) += w * (frm_val + r_val);
        }
    }

    invSumWeights = 1 / invSumWeights;
    for (unsigned int c = 0; c < imgChans; ++c)
        out_pixel.at<imgval_t>(0, 0, c) = cv::saturate_cast<imgval_t>(f2.at(c) * invSumWeights);
}

} /* namespace imagefusion */

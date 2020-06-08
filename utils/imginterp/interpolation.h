#pragma once
#include <iostream>
#include <iomanip>
#include <string>
#include <tuple>

#include <boost/filesystem.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "Type.h"
#include "imagefusion.h"
#include "Image.h"
#include "optionparser.h"
#include "GeoInfo.h"
#include "MultiResImages.h"
#include "fileformat.h"

// annonymous namespace is important for test configuration
// all functions are only defined for the corresponding test
namespace {

struct InterpStats {
    std::string filename;
    int date;
    imagefusion::Size sz;
    unsigned int nChans;
    unsigned int nNoData;
    unsigned int nInterpBefore;
    unsigned int nInterpAfter;
};

struct Interpolator {
    imagefusion::MultiResImages& imgs;
    imagefusion::MultiResImages& cloudmask;
    imagefusion::MultiResImages& maskimgs;
    std::string tag;
    int interpDate;
    bool doPreferCloudsOverNodata;

    template<imagefusion::Type basetype>
    std::tuple<imagefusion::Image, imagefusion::Image, InterpStats> operator()() const;
};

enum class PixelState : uint8_t {
    nodata = 0, noninterpolated = 64, interpolated = 192, clear = 128
};

template<imagefusion::Type basetype>
std::tuple<imagefusion::Image, imagefusion::Image, InterpStats> Interpolator::operator()() const {
    using imgval_t = typename imagefusion::DataType<basetype>::base_type;

    const unsigned int h  = imgs.getAny().height();
    const unsigned int w  = imgs.getAny().width();
    const unsigned int cn = imgs.getAny().channels();

    imagefusion::Image interped = imgs.get(tag, interpDate);
    imagefusion::Image pixelState{interped.size(), imagefusion::getFullType(imagefusion::Type::uint8, interped.channels())};
    pixelState.set(0);

    auto dates = imgs.getDates(tag);
    auto interpDateIt = std::find(std::begin(dates), std::end(dates), interpDate);
    std::array<std::vector<int>, 2> leftRightDates;
    leftRightDates[0] = std::vector<int>(std::vector<int>::reverse_iterator(interpDateIt), dates.rend()); // left (reversed)
    leftRightDates[1] = std::vector<int>(interpDateIt + 1, std::end(dates)); // right

    imagefusion::ConstImage predMask;
    unsigned int maskChannels = 0;
    if (maskimgs.has(tag, interpDate) && !maskimgs.get(tag, interpDate).empty()) {
        predMask = maskimgs.get(tag, interpDate).constSharedCopy();
        maskChannels = (unsigned int)(predMask.channels());
    }

    unsigned int nNoData = 0, nInterpBefore = 0, nInterpAfter = 0;
    #pragma omp parallel for reduction(+:nNoData) reduction(+:nInterpBefore) reduction(+:nInterpAfter)
    for (unsigned int y = 0; y < h; y++) {
        for (unsigned int x = 0; x < w; x++) {
            for (unsigned int c = 0; c < cn; c++) {
                unsigned int maskChannel = maskChannels > c ? c : 0;
                bool isInvalid = maskChannels > 0 && !predMask.boolAt(x, y, maskChannel);
                bool isCloud   = cloudmask.get(tag, interpDate).boolAt(x, y, 0);
                if (isInvalid && (!isCloud || !doPreferCloudsOverNodata)) {
                    nNoData++;
                    pixelState.at<uint8_t>(x, y, c) = static_cast<uint8_t>(PixelState::nodata);
                    continue;
                }
                if (!isCloud) {
                    pixelState.at<uint8_t>(x, y, c) = static_cast<uint8_t>(PixelState::clear);
                    continue;
                }
                // ok, this is a pixel to interpolate
                pixelState.at<uint8_t>(x, y, c) = static_cast<uint8_t>(PixelState::interpolated);
                nInterpBefore++;

                std::array<int,2> leftRightDate = {interpDate, interpDate};
                for (int dir = 0; dir < 2; ++dir) {
                    for (unsigned int date : leftRightDates.at(dir)) {
                        if (maskimgs.has(tag, date)) {
                            auto const& mask = maskimgs.get(tag, date);
                            unsigned int maskChannel = mask.channels() > c ? c : 0;
                            if (!mask.empty() && !mask.boolAt(x, y, maskChannel))
                                continue;
                        }

                        if (cloudmask.has(tag, date) && cloudmask.get(tag, date).boolAt(x, y, 0))
                            continue;

                        leftRightDate.at(dir) = date;
                        break;
                    }
                }

                int dateLeft  = leftRightDate.front();
                int dateRight = leftRightDate.back();
                if (dateRight == interpDate) {
                    // right invalid
                    if (dateLeft == interpDate) {
                        // left invalid, too, leave value as it is for now, but mark location
                        pixelState.at<uint8_t>(x, y, c) = static_cast<uint8_t>(PixelState::noninterpolated);
                        nInterpAfter++;
                    }
                    else
                        // only left valid
                        interped.at<imgval_t>(x, y, c) = imgs.get(tag, dateLeft).at<imgval_t>(x, y, c);
                }
                else if (dateLeft == interpDate)
                    // only right valid
                    interped.at<imgval_t>(x, y, c) = imgs.get(tag, dateRight).at<imgval_t>(x, y, c);
                else {
                    // both valid
                    double yLeft  = imgs.get(tag, dateLeft).at<imgval_t>(x, y, c);
                    double yRight = imgs.get(tag, dateRight).at<imgval_t>(x, y, c);
                    double yInt = (interpDate - dateLeft) * (yRight - yLeft) / (dateRight - dateLeft) + yLeft;
                    interped.at<imgval_t>(x, y, c) = cv::saturate_cast<imgval_t>(yInt);
                }
            } /*c*/
        } /*x*/
    } /*y*/

    InterpStats s{/*filename*/ "", /*date*/ interpDate, /*sz*/ imagefusion::Size(w, h), /*nChans*/ cn, /*nNoData*/ nNoData, /*nInterpBefore*/ nInterpBefore, /*nInterpAfter*/ nInterpAfter};
    return {std::move(interped), std::move(pixelState), s};
}
} /* annonymous namespace */

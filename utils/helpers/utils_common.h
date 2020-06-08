#pragma once

#include "MultiResImages.h"
#include "exceptions.h"
#include "optionparser.h"
#include "GeoInfo.h"

#include <memory>
#include <vector>
#include <string>

namespace helpers {

extern const char* usageValidRanges;
extern const char* usageInvalidRanges;
extern const char* usageMaskFile;

// Null stream, from https://stackoverflow.com/a/11826666
// NullStream null_stream; null_stream << "this is not put out anywhere";
class NullBuffer : public std::streambuf {
public:
    int overflow(int c) { return c; }
};

class NullStream : public std::ostream {
public:
    NullStream() : std::ostream(&m_sb) {}
private:
    NullBuffer m_sb;
};

template<class Parse>
inline imagefusion::GeoInfo
parseGeoInfo(std::string imgArg, imagefusion::Rectangle predArea = {}) {
    auto gi = imagefusion::GeoInfo{Parse::ImageFileName(imgArg), Parse::ImageLayers(imgArg), Parse::ImageCropRectangle(imgArg)};
    if (gi.hasGeotransform()) {
        gi.geotrans.translateImage(predArea.x, predArea.y);
        if (predArea.width != 0)
            gi.size.width = predArea.width;
        if (predArea.height != 0)
            gi.size.height = predArea.height;
    }
    return gi;
}

template<typename T>
void checkNumResTags(imagefusion::MultiResCollection<T> const& col, unsigned int numResTags, std::string const& resErrStr = "") {
    if (col.countResolutionTags() != numResTags) {
        std::string errMsg = "Please specify exactly " + std::to_string(numResTags) + " resolution tags. "
                           + resErrStr
                           + "You specified " + std::to_string(col.countResolutionTags()) + " tag(s)";
        if (col.countResolutionTags() > 0) {
            errMsg += ":\n";
            for (auto const& tag : col.getResolutionTags())
                errMsg += tag + "\n";
        }
        else
            errMsg += ".\n";

        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(errMsg));
    }
}

template<typename T>
void checkMinImages(imagefusion::MultiResCollection<T> const& col, unsigned int minImages) {
    if (col.count() < minImages) {
        std::string errMsg = "Please specify at least " + std::to_string(minImages) + " images with different date and tag. "
                             "You specified " + std::to_string(col.count()) + " different date-tag-combinations:\n";
        for (auto const& tag : col.getResolutionTags()) {
            errMsg += "For tag <" + tag + ">: ";

            for (int d : col.getDates(tag))
                errMsg += std::to_string(d) + ", ";
            errMsg.pop_back(); errMsg.pop_back();
            errMsg += ".\n";
        }
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(errMsg));
    }
}

template<class Parse>
inline std::pair<imagefusion::MultiResCollection<std::string>, imagefusion::MultiResCollection<imagefusion::GeoInfo>>
parseImgsArgsAndGeoInfo(std::vector<std::string> const& args, unsigned int minImages, unsigned int numResTags, imagefusion::Rectangle const& predArea = {}, std::string resErrStr = "")
{
    imagefusion::MultiResCollection<std::string> imgArgs;
    imagefusion::MultiResCollection<imagefusion::GeoInfo> gis;

    for (auto& arg : args) {
        std::string tag;
        if (Parse::ImageHasTag(arg))
                tag = Parse::ImageTag(arg);
        int date = Parse::ImageDate(arg);
        imgArgs.set(tag, date, arg);
        gis.set(tag, date, parseGeoInfo<Parse>(arg, predArea));

        // expand nodata and remove color table for color indexed images
        bool colorTableIgnored = Parse::ImageIgnoreColorTable(arg);
        imagefusion::GeoInfo& gi = gis.get(tag, date);
        if (!colorTableIgnored && !gi.colorTable.empty()) {
            if (gi.hasNodataValue())
                gi.setNodataValue(gi.colorTable.at(gi.getNodataValue()).front());
            gi.colorTable.clear();
        }
    }

    checkNumResTags(imgArgs, numResTags, resErrStr);
    checkMinImages(imgArgs, minImages);

    return {std::move(imgArgs), std::move(gis)};
}


// returns {highTag, lowTag
template<typename elem_type>
inline std::pair<std::string, std::string> getTags(imagefusion::MultiResCollection<elem_type> const& mri) {
    auto resTags = mri.getResolutionTags();
    if (resTags.size() != 2)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "Please specify exactly two resolution tags. The tags itself are not important. However, you specified the wrong number of tags: " + std::to_string(resTags.size())));

    std::string highTag = mri.getResolutionTags().front();
    std::string lowTag  = mri.getResolutionTags().back();

    if (mri.count(highTag) > mri.count(lowTag))
        std::swap(highTag, lowTag);
    else if (mri.count(highTag) == mri.count(lowTag))
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "Please specify more low resolution images than high resolution images, such that the missing high resolution images can be predicted. "
                "You specified " + std::to_string(mri.count(highTag)) + " low and high resolution images."));

    return {highTag, lowTag};
}


// parse jobs just calls getTags, checkPairDatesAndAllDates and then getJobs
struct JobsAndTags {
    std::map<std::vector<int>, std::vector<int>> jobs;
    std::string highTag;
    std::string lowTag;
};
JobsAndTags parseJobs(imagefusion::MultiResCollection<std::string> const& mri, unsigned int minPairs, bool doRemovePredDatesWithOnePair, bool doUseSinglePairMode);

// collect input pair dates and all dates (input pair + prediction + maybe invalid)
// return {'pair dates' (high res dates), 'all dates' (low res dates)}
std::pair<std::vector<int>, std::vector<int>>
checkPairDatesAndAllDates(std::vector<int> pairDates, std::vector<int> allDates, std::string highTag, std::string lowTag, unsigned int minPairs);

// return map with pair dates (1 or 2) as index and prediction dates as data
std::map<std::vector<int>, std::vector<int>> getJobs(std::vector<int> const& pairDates, std::vector<int> const& allDates, bool doRemovePredDatesWithOnePair, bool doUseSinglePairMode);

inline imagefusion::Image
combineMaskImages(std::vector<imagefusion::Image>& masks, std::vector<std::string> const& filenames, unsigned int imgChans, bool isMaskRangeGiven)
{
    using std::to_string;
    using imagefusion::to_string;

    imagefusion::Image baseMask;
    for (unsigned int idx = 0; idx < masks.size(); ++idx) {
        auto& temp_mask = masks[idx];
        std::string const& fn = filenames.empty() ? std::string() : filenames.at(idx);


        if (temp_mask.basetype() != imagefusion::Type::uint8)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                    (fn.empty() ? std::string("A mask image") : "The mask image '" + fn + "'")
                    + " has the wrong type: " + to_string(temp_mask.basetype())
                    + ". It should be 8 bit, unsigned integer (uint8)."));

        if (temp_mask.channels() != 1 && temp_mask.channels() != imgChans)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                    (fn.empty() ? std::string("A mask image") : "The mask image '" + fn + "'")
                    + " has the wrong number of channels: " + to_string(temp_mask.channels()) + ". It should be single-channel"
                    + (imgChans > 1 ? " or have " + std::to_string(imgChans) + "channels (same as the images)" : "")
                    + "."));

        if (baseMask.empty()) // i. e. first mask image
            baseMask = std::move(temp_mask);
        else {
            if (baseMask.size() != temp_mask.size())
                IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                        "The mask images have different sizes: " + to_string(baseMask.size()) + " and " + to_string(temp_mask.size())
                        + ". This is currently not supported. You can use the --crop option within the --mask-img option to make them equally sized."));

            if (baseMask.channels() > temp_mask.channels()) {
                std::vector<imagefusion::ConstImage> duplicate;
                for (unsigned int i = 0; i < baseMask.channels(); ++i)
                    duplicate.push_back(temp_mask.constSharedCopy());
                temp_mask.merge(duplicate);
            }
            else if (baseMask.channels() < temp_mask.channels()) {
                std::vector<imagefusion::ConstImage> duplicate;
                for (unsigned int i = 0; i < temp_mask.channels(); ++i)
                    duplicate.push_back(baseMask.constSharedCopy());
                baseMask.merge(duplicate);
            }

            baseMask = std::move(baseMask).bitwise_and(temp_mask);
        }
    }

    if (!baseMask.empty() && imgChans > 1 && baseMask.channels() == 1 && isMaskRangeGiven) {
        std::vector<imagefusion::ConstImage> duplicate;
        for (unsigned int i = 0; i < imgChans; ++i)
            duplicate.push_back(baseMask.constSharedCopy());
        baseMask.merge(duplicate);
    }

    return baseMask;
}

template<class Parse>
inline imagefusion::Image
parseAndCombineMaskImages(std::vector<std::string> const& maskImgArgs, int imgChans, bool isMaskRangeGiven)
{
    std::vector<imagefusion::Image> masks;
    std::vector<std::string> maskFilenames;
    for (auto const& arg : maskImgArgs) {
        masks.push_back(Parse::Mask(arg));
        maskFilenames.push_back(Parse::ImageFileName(arg));
    }

    return combineMaskImages(masks, maskFilenames, imgChans, isMaskRangeGiven);
}


struct HighLowIntervalSets {
    bool hasHigh = false;
    imagefusion::IntervalSet high;
    bool hasLow = false;
    imagefusion::IntervalSet low;
};

template<class Parse>
inline HighLowIntervalSets
parseAndCombineRanges(std::vector<imagefusion::option::Option> const& rangeOpts)
{
    HighLowIntervalSets validSets;
    bool firstRangeHigh = true;
    bool firstRangeLow  = true;
    for (auto const& opt : rangeOpts) {
        imagefusion::IntervalSet set = Parse::IntervalSet(opt.arg);
        if (opt.prop() == "VALID") {
            validSets.high += set;
            validSets.low  += set;
            firstRangeHigh = false;
            firstRangeLow  = false;
        }
        else if (opt.prop() == "HIGHVALID") {
            validSets.high += set;
            firstRangeHigh = false;
        }
        else if (opt.prop() == "LOWVALID") {
            validSets.low  += set;
            firstRangeLow  = false;
        }
        else {
            if (opt.prop() == "INVALID") {
                // if first range is invalid, start with all values valid and subtract invalid
                if (firstRangeHigh)
                    validSets.high += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
                if (firstRangeLow)
                    validSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

                validSets.high -= set;
                validSets.low  -= set;
                firstRangeHigh = false;
                firstRangeLow  = false;
            }
            else if (opt.prop() == "HIGHINVALID") {
                // if first range is invalid, start with all values valid and subtract invalid
                if (firstRangeHigh)
                    validSets.high += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

                validSets.high -= set;
                firstRangeHigh = false;
            }
            else if (opt.prop() == "LOWINVALID") {
                // if first range is invalid, start with all values valid and subtract invalid
                if (firstRangeLow)
                    validSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

                validSets.low  -= set;
                firstRangeLow  = false;
            }
        }
    }

    validSets.hasHigh = !firstRangeHigh;
    validSets.hasLow  = !firstRangeLow;

    if (validSets.hasHigh || validSets.hasLow)
        std::cout << "Using valid ranges ";
    if (validSets.hasHigh) {
        std::cout << validSets.high << " for high resolution images and ";
        if (!validSets.hasLow)
            std::cout << " full range for low resolution images." << std::endl;
    }
    if (validSets.hasLow) {
        std::cout << validSets.low << " for low resolution images";
        if (!validSets.hasHigh)
            std::cout << " and full range for high resolution images." << std::endl;
        else
            std::cout << "." << std::endl;
    }
    if ((validSets.hasHigh || validSets.hasLow) && (validSets.high.empty() || validSets.low.empty()))
        std::cerr << "Warning: An empty valid set means that no value is valid. Check your mask range specification!" << std::endl;

    return validSets;
}


inline std::pair<std::string, std::string>
getPrefixAndPostfix(std::vector<imagefusion::option::Option> const& prefixOpts,
                    std::vector<imagefusion::option::Option> const& postfixOpts,
                    std::string replacement, std::string name)
{
    std::string prefix;
    if (!prefixOpts.empty())
        prefix = prefixOpts.back().arg;

    std::string postfix;
    if (!postfixOpts.empty())
        postfix = postfixOpts.back().arg;

    if (prefix.empty() && postfix.empty()) {
        std::cout << "Setting " << name << " to '" << replacement << "', since both prefix and "
                     "postfix are empty. This is to prevent filename clashes." << std::endl;
        prefix = replacement;
    }

    return {prefix, postfix};
}

imagefusion::Image processSetMask(imagefusion::Image mask, imagefusion::ConstImage const& img, imagefusion::IntervalSet const& validSet);


std::string outputImageFile(imagefusion::ConstImage const& img, imagefusion::GeoInfo gi, std::string origFileName,
                            std::string prefix, std::string postfix, imagefusion::FileFormat f = imagefusion::FileFormat::unsupported,
                            int date1 = 0, int date2 = 0, int date3 = 0);


double findAppropriateNodataValue(imagefusion::ConstImage const& i, imagefusion::ConstImage const& mask);

} /* namespace helpers */

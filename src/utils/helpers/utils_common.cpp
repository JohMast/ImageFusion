#include "utils_common.h"

#include <filesystem>

namespace helpers {

const char* usageValidRanges =
    "  --mask-valid-ranges=<range-list> \tSpecify one or more intervals for valid values. Locations with invalid values will be masked out.\v"
    " Valid ranges can excluded from invalid ranges or vice versa, depending on the order of options, see --mask-invalid-ranges and example below.\v"
    "<range-list> must have the form '<range> [[,] <range> ...]', where the brackets mean that further intervals are optional.\v"
    "<range> must either be a single number or have the format '[<float>,<float>]', '(<float>,<float>)', '[<float>,<float>' or '<float>,<float>]',"
    " where the comma and round brackets are optional, but square brackets are here actual characters. Especially for half-open intervals do not use unbalanced parentheses or escape them (maybe with two '\\')!"
    " <float> can be 'infinity' (see std::stod). Additional whitespace can be added anywhere.\v"
    "Examples:\n"
    "\t  --mask-valid-ranges=[1,1000] \vwill mask out every pixel value less than 1 or greater than 1000.\n"
    "\t  --mask-valid-ranges=[100,300  --mask-invalid-ranges='(125,175) [225,275]' \vwill define valid pixel values as [100,125] U [175,224] U [276,299], assuming an interger image.\n";

const char* usageInvalidRanges =
    "  --mask-invalid-ranges=<range-list> \tSpecify one or more intervals for invalid values. These will be masked out. For the format see the description at --mask-valid-ranges."
    " Invalid intervals can be excluded from valid ranges or vice versa, depending on the order of options, see --mask-valid-ranges.\v"
    "Examples:\n"
    "\t  --mask-invalid-ranges=[1,1000] \vwill mask out every pixel value that is greater or equal to 1 and less or equal to 1000.\n"
    "\t  --mask-invalid-ranges='[-inf, 0  [30000,inf]' \vwill define valid pixel values as [0,29999], assuming an interger image.\n";

const char* usageMaskFile =
    "  -m <img>, --mask-img=<msk> \tMask image (8-bit, boolean, i. e. consists of 0 and 255). The format of <msk> is similar as <img>, "
    "see the description at --img. However, do not give a date or tag for <msk>. You can give the additional options:\n"
    "\t  -b <num-list>, --extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted "
    "from the quality layer image and then shifted to the least significant positions. By default all bits will be used.\n"
    "\t  --valid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as valid (true; 255). "
    "Can be combined with --invalid-ranges.\n"
    "\t  --invalid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as invalid (false; 0). "
    "Can be combined with --valid-ranges.\n"
    "\t<range-list> must have the form '<range> [<range> ...]', where the brackets mean that further intervals are optional. The different ranges are related as union.\v"
    "<range> should have the format '[<int>,<int>]', where the comma is optional, but the square brackets are actual characters here. Additional whitespace can be added anywhere.\v"
    "If you neither specify valid ranges nor invalid ranges, the conversion to boolean will be done by using true for all values except 0.\v"
    "A simple filename is also valid. For all input images the pixel values at the locations where the mask is 0 is replaced by the mean "
    "value. If multiple masks are given they are combined. Additionally using mask intervals will also restrict the valid locations further.\v"
    "Examples:\n"
    "\t  --mask-img=some_image.png\n"
    "\tReads some_image.png (converts a possibly existing color table) and converts then 0 values to false (0) and every other value to true (255).\n"
    "\t  --mask-img='-f \"test image.tif\"  --crop=(-x 1 -y 2 -w 3 -h 2)  -l (0 2) -b 6,7  --valid-ranges=[3,3]'\n"
    "\tReads and crops channels 0 and 2 of \"test image.tif\" and converts all values to false (0) except where bit 6 and bit 7 are both set. These will be set to true (255).\n"
    "\t  --mask-img='-f \"test.tif\"  -b 7 -b 6 -b 0  --valid-ranges=[1,7]  --invalid-ranges=[3,3]'\n"
    "\tReads test.tif and converts all values to true (255) where any of bits 0, 6 and 7 is set, but not if bit 6 and 7 are set and bit 0 is clear.\n";



JobsAndTags parseJobs(imagefusion::MultiResCollection<std::string> const& mri, unsigned int minPairs, bool doRemovePredDatesWithOnePair, bool doUseSinglePairMode) {
    auto highLowTag = helpers::getTags(mri);
    std::string& highTag = highLowTag.first;
    std::string& lowTag  = highLowTag.second;

    auto pairAndAllDates = helpers::checkPairDatesAndAllDates(mri.getDates(highTag), mri.getDates(lowTag), highTag, lowTag, minPairs);
    return JobsAndTags{
        getJobs(pairAndAllDates.first, pairAndAllDates.second, doRemovePredDatesWithOnePair, doUseSinglePairMode),
        std::move(highTag), std::move(lowTag)};
}


std::pair<std::vector<int>, std::vector<int>>
checkPairDatesAndAllDates(std::vector<int> pairDates, std::vector<int> allDates, std::string highTag, std::string lowTag, unsigned int minPairs)
{
    using std::to_string;
    using imagefusion::to_string;

    std::sort(pairDates.begin(), pairDates.end());
    std::sort(allDates.begin(), allDates.end());

    if (pairDates.size() < minPairs) {
        std::string errMsg = "Please specify at least " + to_string(minPairs) + " image pair(s) (common date(s) of low and high resolution images).";
        if (!pairDates.empty()) {
            errMsg += "You specified " + to_string(pairDates.size()) + " different pair date(s): ";
            for (int d : pairDates)
                errMsg += to_string(d) + ", ";
            errMsg.pop_back(); errMsg.pop_back();
            errMsg += ".";
        }
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(errMsg));
    }

    if (!std::includes(allDates.begin(), allDates.end(), pairDates.begin(), pairDates.end())) {
        std::string errMsg = "<" + highTag + "> is used as high resolution tag and <" + lowTag + "> as low resolution tag. But then there are images missing. "
                     "Please specify all low resolution images at the dates where you specified high resolution images. "
                     "For high resolution images (pair dates) you specified the following dates:\n";
        for (int d : pairDates)
            errMsg += to_string(d) + ", ";
        errMsg.pop_back(); errMsg.pop_back();
        errMsg += ".\n"
                  "For low resolution images (pair and prediction dates) you specified the following dates:\n";
        for (int d : allDates)
            errMsg += to_string(d) + ", ";
        errMsg.pop_back(); errMsg.pop_back();
        errMsg += ".\n";

        errMsg += "The low resolution images at dates ";
        std::vector<int> missingDates;
        std::set_difference(pairDates.begin(), pairDates.end(), allDates.begin(), allDates.end(), std::back_inserter(missingDates));
        for (int d : missingDates)
            errMsg += to_string(d) + ", ";
        errMsg.pop_back(); errMsg.pop_back();
        errMsg += " are missing.\n";
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(errMsg));
    }

    return {pairDates, allDates};
}


std::map<std::vector<int>, std::vector<int>> getJobs(std::vector<int> const& pairDates, std::vector<int> const& allDates, bool doRemovePredDatesWithOnePair, bool doUseSinglePairMode) {
    assert(!(doRemovePredDatesWithOnePair && doUseSinglePairMode) && "Cannot use both!");

    // find prediction dates
    std::vector<int> predDates;
    std::set_difference(allDates.begin(), allDates.end(), pairDates.begin(), pairDates.end(), std::back_inserter(predDates));

    if (doRemovePredDatesWithOnePair) {
        std::string outDatesStr;
        auto it     = predDates.begin();
        while (it != predDates.end()) {
            if (*it < pairDates.front() || *it > pairDates.back()) {
                outDatesStr += std::to_string(*it) + ", ";
//                mri->remove(lowTag, *it);
                it = predDates.erase(it);
            }
            else
                ++it;
        }

        if (!outDatesStr.empty())
            std::cerr << "Warning: Removed low resolution images with dates " << outDatesStr << "because they are not surrounded by pair dates. Only interpolation-style prediction is supported." << std::endl;
    }

    if (predDates.empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Please specify at least 1 low resolution image for the prediction date(s)."));

    {   // info
        std::cout << "Your input images are interpreted as job to predict at dates: ";
        std::string predDatesStr;
        for (int d : predDates)
            predDatesStr += std::to_string(d) + ", ";
        predDatesStr.pop_back(); predDatesStr.pop_back();
        std::cout << predDatesStr << "." << std::endl;
    }

    // collect the dates in a job hierarchy, like [(1) 2 3 4 (7)], [(7), 10, (14)]
    std::map<std::vector<int>, std::vector<int>> jobs;
    {   // first images before first pair
        int firstDate = pairDates.front();

        auto endDate_it = std::upper_bound(predDates.begin(), predDates.end(), firstDate);
        std::vector<int> dates(predDates.begin(), endDate_it);

        if (!dates.empty())
            jobs.emplace(std::vector<int>{firstDate}, std::move(dates));
    }
    {   // last images after last pair
        int lastDate = pairDates.back();

        auto beginDate_it = std::lower_bound(predDates.begin(), predDates.end(), lastDate);
        std::vector<int> dates(beginDate_it, predDates.end());

        if (!dates.empty())
            jobs.emplace(std::vector<int>{lastDate}, std::move(dates));
    }
    for (auto it = pairDates.begin(), it_end = pairDates.end(); it != it_end && it != it_end - 1; ++it) {
        // dates inbetween pairs
        int date1 = *it;
        int date3 = *(it+1);

        auto beginDate_it = std::lower_bound(predDates.begin(), predDates.end(), date1);
        auto endDate_it   = std::upper_bound(predDates.begin(), predDates.end(), date3);
        std::vector<int> dates(beginDate_it, endDate_it);

        if (!dates.empty()) {
            if (doUseSinglePairMode) {
                jobs[std::vector<int>{date1}].insert(jobs[std::vector<int>{date1}].end(), dates.cbegin(), dates.cend());
                jobs[std::vector<int>{date3}].insert(jobs[std::vector<int>{date3}].end(), dates.cbegin(), dates.cend());
            }
            else
                jobs.emplace(std::vector<int>{date1, date3}, std::move(dates));
        }
    }
    return jobs;
}


imagefusion::Image processSetMask(imagefusion::Image mask, imagefusion::ConstImage const& img, imagefusion::IntervalSet const& validSet) {
    imagefusion::Image tempMask = img.createMultiChannelMaskFromSet({validSet});
    if (mask.empty())
        return tempMask;

    if (mask.channels() != 1 && mask.channels() != tempMask.channels())
        IF_THROW_EXCEPTION(imagefusion::image_type_error("The mask has " + std::to_string(mask.channels()) + " channels while the image has " + std::to_string(tempMask.channels()) + ". That doesn't fit."))
                << imagefusion::errinfo_image_type(mask.type());

    // bring mask to the same number of channels
    if (mask.channels() < tempMask.channels()) {
        std::vector<imagefusion::Image> masks;
        masks.reserve(tempMask.channels());
        for (unsigned int i = 0; i < tempMask.channels(); ++i)
            masks.push_back(imagefusion::Image(mask.sharedCopy()));
        mask.merge(masks);
    }

    return std::move(mask).bitwise_and(tempMask);
}

std::string outputImageFile(imagefusion::ConstImage const& img, imagefusion::GeoInfo gi, std::string origFileName, std::string prefix, std::string postfix, imagefusion::FileFormat f, int date1, int date2, int date3) {
    std::filesystem::path p = origFileName;

    std::string extension = p.extension().string();
    if (f != imagefusion::FileFormat::unsupported) {
        extension = f.fileExtension();
        if (extension.empty()) {
            extension = f.allFileExtensions();
            std::string::size_type n = extension.find(' ');
            extension = extension.substr(0, n);
        }

        if (!extension.empty())
            extension = "." + extension;
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    std::string basename  = std::to_string(date2) + "_from_" + std::to_string(date1) + (date1 != date3 ? "_and_" + std::to_string(date3) : "");
    if (date1 == date2 && date2 == date3)
        basename = p.stem().string();
    p = prefix + basename + postfix + extension;
    auto outpath = p;

    std::string outfilename = outpath.string();
    try {
        img.write(outfilename, gi, f);

        // test color table
        imagefusion::GeoInfo test{outfilename};
        bool ctOK = gi.compareColorTables(test);
        if (!ctOK) {
            gi.colorTable.clear();
            gi.addTo(outfilename);
        }
    }
    catch (imagefusion::runtime_error& e) {
        std::cout << e.what() << std::endl;
        if (f != imagefusion::FileFormat("GTiff") && extension != ".tif" && extension != ".tiff") {
            std::cout << "Retrying with GTiff driver." << std::endl;
            return outputImageFile(img, gi, origFileName, prefix, postfix, imagefusion::FileFormat("GTiff"), date1, date2, date3);
        }
        else if (prefix != "save_") {
            std::cout << "Retrying at working directory with prefix 'save_'." << std::endl;
            return outputImageFile(img, gi, origFileName, "save_", postfix, imagefusion::FileFormat("GTiff"), date1, date2, date3);
        }
        else
            IF_THROW_EXCEPTION(imagefusion::runtime_error(e.what())) << boost::errinfo_file_name(outfilename);
    }

    return outfilename;
}


namespace {
struct SimpleHistFunctor {
    imagefusion::ConstImage const& i;
    imagefusion::ConstImage const& mask;

    template<imagefusion::Type t>
    std::vector<int> operator()() const {
        using basetype = typename imagefusion::DataType<t>::base_type;
        constexpr basetype min = static_cast<basetype>(imagefusion::getImageRangeMin(t));
        constexpr basetype max = static_cast<basetype>(imagefusion::getImageRangeMax(t));
        bool hasMask = !mask.empty();
        std::vector<int> hist(max - min + 1);
        int w = i.width();
        int h = i.height();
        unsigned int cn = i.channels();
        unsigned int mcn = mask.channels();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                for (unsigned int c = 0; c < cn; ++c) {
                    unsigned int maskChan = mcn > c ? c : 0;
                    if (!hasMask || mask.boolAt(x, y, maskChan))
                        ++hist.at(i.at<basetype>(x, y, c) - min);
                }
            }
        }
        return hist;
    }
};
}

double findAppropriateNodataValue(imagefusion::ConstImage const& i, imagefusion::ConstImage const& mask) {
    using imagefusion::Type;
    Type t = i.basetype();
    if (imagefusion::isFloatType(t))
        return -9999;
    if (t == imagefusion::Type::int32)
        return -999999;

    std::vector<int> hist = imagefusion::CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8, Type::int8, Type::uint16, Type::int16>::run(
                SimpleHistFunctor{i, mask}, t);
    int min = static_cast<int>(imagefusion::getImageRangeMin(t));

    // prefer common nodata values
    if (t == imagefusion::Type::int16 && hist.at(-min - 9999) == 0)
        return -9999;
    if (t == imagefusion::Type::int8  && hist.at(-min - 99) == 0)
        return -99;

    // otherwise take the most negative (signed) or most positive (unsigned)
    if (imagefusion::isSignedType(t)) {
        auto it = std::find(hist.begin(), hist.end(), 0);
        if (it != hist.end())
            return (it - hist.begin()) + min;
    }
    else  {
        auto it = std::find(hist.rbegin(), hist.rend(), 0);
        if (it != hist.rend())
            return &*it - &*hist.begin();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

} /* namespace helpers */

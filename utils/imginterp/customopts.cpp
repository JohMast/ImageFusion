#include "customopts.h"

#include "Image.h"
#include "Type.h"
#include "optionparser.h"

#include <vector>

std::vector<imagefusion::option::Descriptor> usageQL;

const char* usageQLText =
    "  --ql-img=<ql-img> \tSpecifies a quality layer integer image and which bits with which values should be used to determine the locations to interpolate. "
    "Possibly existing color tables will be ignored. The selected bits will be extracted from the quality layer image and then shifted to the least significant positions. "
    "Finally, the shifted values are compared to the specified ranges to make a mask for the locations to interpolate.\n"
    "\t<ql-img> has a similar format as the other images, so it has a --file, --layers, --tag and --date options, but additionally one can specify bits and ranges:\n"
    "\t  -b <num-list>, --extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be extracted "
    "from the quality layer image and then shifted to the least significant positions. By default all bits will be used.\v"
    "<num-list> must have the format '(<num>, [<num>, ...])', without commas in between or just '<num>'.\n"
    "\t  -d <num>, --date=<num> \tMaybe optional. Specifies the date (number).\n"
    "\t  -f <file>, --file=<file> \tSpecifies the image file path.\n"
    "\t  -l <num>, --layers=<num> \tOptional. Specifies the channels or layers, that will be read. Hereby a 0 means the first channel.\n"
    "\t  -t <tag>, --tag=<tag> \tMaybe optional. Specifies the resolution tag (string).\n"
    "\t  --interp-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location for interpolation. "
    "Can be combined with --non-interp-ranges.\n"
    "\t  --non-interp-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location for non-interpolation. "
    "Can be combined with --interp-ranges.\n"
    "\t\t<range-list> must have the form '<range> [<range> ...]', where the brackets mean that further intervals are optional. The different ranges are related as union.\n"
    "\t\t<range> should have the format '[<int>,<int>]', where the comma is optional, but the square brackets are actual characters here. Additional whitespace can be added anywhere.\n"
    "\t\tIf you neither specify interp ranges nor non-interp ranges, interpolation will be done for all values except 0.\n"
    "\tExamples:\v"
    "  --ql='-f fmask.tif  -t exmpl  -d 0  --interp-ranges=[2,3]'\v"
    "would create a mask with 255 where fmask.tif is 2 (cloud) or 3 (cloud shadow).\v"
    "  --ql='-f mod-qa.tif  -t exmpl  -d 0  -b 0,1,2  --interp-ranges=[1,7]  --non-interp-ranges=[3,3]'\v"
    "would create a mask with 255 where mod-qa.tif has binary state *01 (cloudy), *10 (mixed) or 1** (cloud shadow). "
    "However, locations with 011 (not set, assumed clear) would not get interpolated.\n";

namespace {

void initUsageQL() {
    using imagefusion::option::Descriptor;

    if (!usageQL.empty())
        return;

    usageQL = imagefusion::option::Parse::usageMRImage;
    usageQL.push_back(Descriptor{"INTERPRANGE", "INTERP",    "",  "interp-ranges",     ArgChecker::IntervalSet, ""});
    usageQL.push_back(Descriptor{"INTERPRANGE", "NONINTERP", "",  "non-interp-ranges", ArgChecker::IntervalSet, ""});
    usageQL.push_back(Descriptor{"BITS",        "",          "b", "extract-bits",      ArgChecker::Vector<int>, ""});
}


struct MaskFromBits {
    std::vector<int> bits;
    imagefusion::IntervalSet const& interpSet;
    imagefusion::ConstImage const& img;

    template<imagefusion::Type basetype>
    imagefusion::Image operator()();
};

} /* anonymous namespace */


// reads QL image and shifts the extracted bits to the front
imagefusion::option::ImageInput Parse::QL(std::string const& str, std::string const& /*optName*/, bool readImage, bool isDateOpt, bool isTagOpt) {
    initUsageQL();
    auto qlOptions = imagefusion::option::OptionParser::parse(usageQL, str);

    std::vector<int> bits;
    for (auto const& opt : qlOptions["BITS"]) {
        std::vector<int> b = Parse::Vector<int>(opt.arg);
        bits.insert(bits.end(), b.begin(), b.end());
    }
    std::sort(bits.begin(), bits.end());
    if (!bits.empty() && bits.front() < 0)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("You are trying to extract negative bit positions (" + std::to_string(bits.front()) + "), which does not make any sense"));

    imagefusion::IntervalSet interpSet;
    bool hasInterpRanges = !qlOptions["INTERPRANGE"].empty();
    if (!hasInterpRanges || qlOptions["INTERPRANGE"].front().prop() == "NONINTERP")
        // if first range is non-interp, start with all values interp
        interpSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    if (!hasInterpRanges)
        interpSet -= imagefusion::Interval::closed(0, 0);
    for (auto const& opt : qlOptions["INTERPRANGE"]) {
        imagefusion::IntervalSet set = Parse::IntervalSet(opt.arg);
        if (opt.prop() == "INTERP")
            interpSet += set;
        else
            interpSet -= set;
    }

    std::string overrideArgs = "  --disable-use-color-table"; // quality layers must not be converted to their visual representation
    imagefusion::option::ImageInput img = Parse::MRImage(str + overrideArgs, "", readImage, isDateOpt, isTagOpt, usageQL);
    if (!readImage)
        return img;

    if (img.i.channels() > 1)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Quality layer images may only have one channel. Use --layers <num> to extract a single channel. " + Parse::ImageFileName(str) + " has " + std::to_string(img.i.channels()) + " channels."))
            << boost::errinfo_file_name(Parse::ImageFileName(str)) << imagefusion::errinfo_date(img.date) << imagefusion::errinfo_resolution_tag(img.tag) << imagefusion::errinfo_image_type(img.i.type());

    if (!isIntegerType(img.i.type()))
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Quality layer images represent bitmasks. So only integer types are supported. " + Parse::ImageFileName(str) + " is of type " + to_string(img.i.type()) + "."))
            << boost::errinfo_file_name(Parse::ImageFileName(str)) << imagefusion::errinfo_date(img.date) << imagefusion::errinfo_resolution_tag(img.tag) << imagefusion::errinfo_image_type(img.i.type());

    try {
        using imagefusion::Type;
        img.i = imagefusion::CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8, Type::int8, Type::uint16, Type::int16, Type::int32>::run(
                    MaskFromBits{bits, interpSet, img.i}, img.i.basetype());
    }
    catch (imagefusion::invalid_argument_error& e) {
        std::string fn = Parse::ImageFileName(str);
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(e.what() + std::string(", where the image is ") + fn + " and has the type " + to_string(img.i.type())))
            << boost::errinfo_file_name(fn) << imagefusion::errinfo_date(img.date) << imagefusion::errinfo_resolution_tag(img.tag) << imagefusion::errinfo_image_type(img.i.type());
    }
    return img;
}


namespace {

template<imagefusion::Type basetype>
imagefusion::Image MaskFromBits::operator()() {
    using imgval_t = typename imagefusion::DataType<basetype>::base_type;

    int nbits = bits.size();
    unsigned int width = img.width();
    unsigned int height = img.height();
//     unsigned int maxVal = std::pow(2, nbits);
    std::string errAdd = "bits 0 - " + std::to_string(sizeof(imgval_t) * 8 - 1) + " can be extracted";// and with the number of bits you selected the resulting value range is 0 - " + std::to_string(maxVal);
    if (!bits.empty() && bits.back() >= static_cast<int>(sizeof(imgval_t) * 8))
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("You are trying to extract bits up to bit " + std::to_string(bits.back()) + " although only " + errAdd));

    imgval_t andBitMask = 0;
    for (int b : bits)
        andBitMask |= 1 << b;

    bool isContiguous = true;
    for (int p = 0; p < nbits - 1; ++p) {
        if (bits.at(p + 1) != bits.at(p) + 1) {
            isContiguous = false;
            break;
        }
    }

    imagefusion::Image mask(width, height, imagefusion::Type::uint8x1);
    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            imgval_t qlVal = img.at<imgval_t>(x, y, 0);

            imgval_t shiftedVal = 0;
            if (bits.empty())
                shiftedVal = qlVal;
            else if (isContiguous)
                shiftedVal = (qlVal & andBitMask) >> bits.front();
            else { // collect each bit and shift it to the front positions
                qlVal &= andBitMask;
                for (int p = 0; p < nbits; ++p) {
                    int b = bits.at(p);
                    imgval_t shiftedBit = qlVal & (1 << b);
                    shiftedBit >>= (b - p);
                    shiftedVal |= shiftedBit;
                }
            }

            bool doInterp = contains(interpSet, shiftedVal);
            mask.setBoolAt(x, y, 0, doInterp);
        }
    }
    return mask;
}

} /* anonymous namespace */

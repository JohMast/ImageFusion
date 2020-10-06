#include <iostream>
#include <iomanip>
#include <string>

#include <filesystem>
#include <boost/range/adaptor/reversed.hpp>

#include "optionparser.h"
#include "geoinfo.h"
#include "multiresimages.h"
#include "utils_common.h"
#include "fileformat.h"

#include "interpolation.h"
#include "customopts.h"

using namespace imagefusion;

const char* usageEnablePixelstate =
    "  --enable-output-pixelstate \tThis enables the output of the pixelstate. The pixelstates are 8 bit wide.\v"
    " * bit 6 indicates that it was a location to interpolate before,\v"
    " * bit 7 indicates that it is a clear pixel afterwards.\v"
    "This results in the follwing states (other bits are 0):\v"
    " value | b7 b6 | meaning\v"
    "-------+-------+------------------------------------------------\v"
    "     0 |  0  0 | Was nodata before and still is.\v"
    "    64 |  0  1 | Could not be interpolated and is set to nodata.\v"
    "   192 |  1  1 | Is interpolated.\v"
    "   128 |  1  0 | Was clear before and still is.\v"
    "See --out-pixelstate-prefix and --out-pixelstate-postfix for the pixelstate filenames.\n";

const char* usageImage =
    "  -i <img>, --img=<img> \tSpecify multiple images, you would like to interpolate. This utility also accepts a single image, which can be used to add a cloud mask.\n"
    "If pre-cropping or using only a subset of channels / layers is desired, <img> must have the form "
    "'-f <file> [--crop-pix=<rect>] [--crop-proj=<rect>] [-l <num-list>] [--disable-use-color-table]', "
    "where the arguments can have an arbitrary order. "
    "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"
    "\t  -f <file>, --file=<file> \tSpecifies the image file path.\n"
    "\t  -t <tag>, --tag=<tag>, \tOptional. Specifies the resolution tag (string).\n"
    "\t  -d <num>, --date=<num>, \tSpecifies the date (number).\n"
    "\t  -l <num-list>,  --layers=<num-list> \tOptional. Specifies the channel or layer, that will be read. Hereby a 0 means the first channel.\n"
    "\t\t<num-list> must have the format '(<num> [[,]<num> ...])' or just '<num>'.\n"
    "\tExamples: --img='--file=img1tointerp.tiff --date=1'\v"
    "          --img='--file=img2tointerp.tiff --date=2'\v"
    "          --img='--file=img3tointerp.tiff --date=3'\v"
    "          --img='--file=img4tointerp.tiff --date=4'\n";

const char* usageMaskFile =
    "  -m <img>, --mask-img=<msk> \tMask image (8-bit, boolean, i. e. consists of 0 and 255). The format of <msk> is similar as <img>, see the description at --img. "
    "However, do not give a date or tag for <msk>. You can give the additional options:\n"
    "\t  -b <num-list>, --extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted "
    "from the quality layer image and then shifted to the least significant positions. By default all bits will be used.\n"
    "\t  --valid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as valid (true; 255). "
    "Can be combined with --invalid-ranges.\n"
    "\t  --invalid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as invalid (false; 0). "
    "Can be combined with --valid-ranges.\n"
    "\t<range-list> must have the form '<range> [<range> ...]', where the brackets mean that further intervals are optional. The different ranges are related as union.\v"
    "<range> should have the format '[<int>,<int>]', where the comma is optional, but the square brackets are actual characters here. Additional whitespace can be added anywhere.\v"
    "If you neither specify valid ranges nor invalid ranges, the conversion to boolean will be done by using true for all values except 0.\v"
    "For all input images the pixel values at the locations where the mask is 0 are considered as invalid and are not used for interpolation as well as not interpolated. "
    "If multiple masks are given they are combined. Additionally using mask intervals will also restrict the valid locations further.\v"
    "Examples:\n"
    "\t  --mask-img='-f \"test image.tif\"  --date=1  --crop=(-x 1 -y 2 -w 3 -h 2)  -l (0 2) -b 6,7  --valid-ranges=[3,3]'\n"
    "\tReads and crops channels 0 and 2 of \"test image.tif\" and converts all values to false (0) except where bit 6 and bit 7 are both set. These will be set to true (255).\n"
    "\t  --mask-img='-f \"test.tif\"  -b 7 -b 6 -b 0  --valid-ranges=[1,7]  --invalid-ranges=[3,3]'\n"
    "\tReads test.tif and converts all values to true (255) where any of bits 0, 6 and 7 is set, but not if bit 6 and 7 are set and bit 0 is clear.\n";

const char* usageInterpRanges =
    "  --interp-ranges=<range-list> \tSpecify one or more intervals for values that should be interpolated. "
    "Interp-ranges can be from no-interp-ranges or vice versa, depending on the order of options, see --no-interp-ranges and the example below.\v"
    "<range-list> must have the form '<range> [[,] <range> ...], where the brackets mean that further intervals are optional.\v"
    "<range> must either be a single number or have the format '[<float>,<float>]', '(<float>,<float>)', '[<float>,<float>' or '<float>,<float>]',"
    " where the comma and round brackets are optional, but square brackets are here actual characters. Especially for half-open intervals do not use unbalanced parentheses or escape them (maybe with two '\\')!"
    " <float> can be 'infinity' (see std::stod). Additional whitespace can be added anywhere.\v"
    "Examples:\n"
    "\t  --interp-ranges=[1,1000] \vwill interpolate every pixel locations with values equal to or greater than 1 or equal to or less than 1000.\n"
    "\t  --interp-ranges=[100,300  --no-interp-ranges='(125,175) [225,275]' \vwill interpolate pixel locations with values which fall within the following set [100,125] U [175,224] U [276,299], assuming an interger image.\n";

const char* usageNoInterpRanges =
    "  --no-interp-ranges=<range-list> \tSpecify one or more intervals for values that should not be interpolated. "
    "No-interp-ranges can be excluded from interp-ranges or vice versa, depending on the order of options, see --interp-ranges.\v"
    "Examples:\n"
    "\t  --no-interp-ranges=[1,1000] \vwill not interpolate every pixel locations with values equal to or greater than 1 or equal to or less than 1000.\n"
    "\t  --no-interp-ranges='[-inf, 0  [30000,inf]' \vwill interpolate pixel locations with values which fall within the following set [0,29999].\n";

using Descriptor = imagefusion::option::Descriptor;
std::vector<Descriptor> usage{

    Descriptor::text("Usage: imginterp -i <img> -i <img> -i <img> [options]\n"
    "   or: imginterp --option-file=<file> [options]\n"
    "   or: imginterp \t-i <img> [--help] [--disable-output-masks] [--disable-use-nodata] [--enable-output-masks] [--enable-use-nodata] [--help] [--img] [--mask-img] "
    "[--interp-ranges] [--mask-invalid-ranges] [--mask-valid-ranges] [--no-interp-ranges] [--out-mask-postfix] [--out-mask-prefix] [--out-postfix] [--out-prefix]  "
    "[--ql-fmask] [--ql-img] [--ql-modis]\n"),
    Descriptor::breakTable(),

    Descriptor::text("This utility is developed to perform simple interpolation on a given time series of remote sensing images. "
    "This utility can also perform cloud masking on satellite images with the quality layer provided using [--ql-img] option. "
    "The quality layer can be a bit field image (ex. State_1km: Reflectance Data State QA layer from MODIS) or state image which "
    "provides the state of the pixel (ex. quality layer from FMASK). When a single image with a date and a quality layer with the "
    "same date is provided, this utility will fill the cloud (or whatever is specified) locations with the nodata value and output "
    "the modified image. If multiple images with dates are provided with quality layers, this utility will try to interpolate the "
    "bad locations linearly. When there is not enough data, the non-interpolated locations will be set to the nodata value. Note, "
    "nodata locations will not be interpolated by default. "
    "Remember to protect whitespace by quoting with '...', \"...\" or (...) or by escaping.\n\n"
    "Options:"),
    {"INTINV",        "DISABLE", "",  "disable-interp-invalid",   ArgChecker::None,                 "  --disable-interp-invalid \tDo not interpolate invalid locations. Default.\n"},
    {"PSOUT",         "DISABLE", "",  "disable-output-pixelstate",ArgChecker::None,                 "  --disable-output-pixelstate \tThis disables the output of the pixelstate that are created from interpolation. See --enable-output-pixelstate. Default.\n"},
    {"USENODATA",     "DISABLE", "",  "disable-use-nodata",       ArgChecker::None,                 "  --disable-use-nodata \tThis will not use the nodata value as invalid range for masking.\n"},
    {"INTINV",        "ENABLE",  "",  "enable-interp-invalid",    ArgChecker::None,                 "  --enable-interp-invalid \tHandle invalid locations (e.g. due to nodata value) like locations to interpolate.\n"},
    {"PSOUT",         "ENABLE",  "",  "enable-output-pixelstate", ArgChecker::None,                 usageEnablePixelstate},
    {"PRIOCLOUDS",    "ENABLE",  "",  "enable-prioritize-interp", ArgChecker::None,                 "  --enable-prioritize-interp  \tWhen a pixel location is marked as invalid and as interpolate, handle as invalid location and do not interpolate. Default."},
    {"PRIOCLOUDS",    "DISABLE", "",  "enable-prioritize-invalid",ArgChecker::None,                 "  --enable-prioritize-invalid \tWhen a pixel location is marked as invalid and as interpolate, handle as location to interpolate."},
    {"USENODATA",     "ENABLE",  "",  "enable-use-nodata",        ArgChecker::None,                 "  --enable-use-nodata  \tThis will use the nodata value as invalid range for masking. Default.\n"},
    {"HELP",          "",        "h", "help",                     ArgChecker::None,                 "  -h, --help  \tPrint usage and exit.\n"},
    {"IMAGE",         "",        "i", "img",                      ArgChecker::MRImage<false, true>, usageImage},
    {"INTERPRANGE",   "VALID",   "",  "interp-ranges",            ArgChecker::IntervalSet,          usageInterpRanges},
    {"LIMIT",         "",        "l", "limit-days",               ArgChecker::Int,                  "  -l <num>, --limit-days=<num>  \tLimit the maximum numbers of days from the interpolating day that will be considered. So using e. g. a 3 will only consider images that are 3 days apart from the interpolation day. Default 5.\n"},
    {"MASKIMG",       "",        "m", "mask-img",                 ArgChecker::MRMask<false, true>,  usageMaskFile},
    {"MASKRANGE",     "INVALID", "",  "mask-invalid-ranges",      ArgChecker::IntervalSet,          helpers::usageInvalidRanges},
    {"MASKRANGE",     "VALID",   "",  "mask-valid-ranges",        ArgChecker::IntervalSet,          helpers::usageValidRanges},
    {"INTERPRANGE",   "INVALID", "",  "no-interp-ranges",         ArgChecker::IntervalSet,          usageNoInterpRanges},
    Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file are specified in the same way as on the command line. You can use newlines between options "
    "and line comments with # (use \\# to get a non-comment #). The specified options in the file replace the --option-file=<file> argument before they are parsed.\n"),
    {"OUTPSPOSTFIX",  "",        "",  "out-pixelstate-postfix",   ArgChecker::Optional,             "  --out-pixelstate-postfix=<string> \tThis will be appended to the output filenames (including prefix and postfix) to form the pixel state bitfield filenames. Only used if pixel state output is enabled.\n"},
    {"OUTPSPREFIX",   "",        "",  "out-pixelstate-prefix",    ArgChecker::Optional,             "  --out-pixelstate-prefix=<string> \tThis will be prepended to the output filenames (including prefix and postfix) to form the pixel state bitfield filenames. Only used if pixelstate output is enabled. By default this is 'ps_'.\n"},
    {"OUTPOSTFIX",    "",        "",  "out-postfix",              ArgChecker::Optional,             "  --out-postfix=<string> \tThis will be appended to the output filenames.\n"},
    {"OUTPREFIX",     "",        "",  "out-prefix",               ArgChecker::Optional,             "  --out-prefix=<string> \tThis will be prepended to the output filenames. By default this is 'interpolated_'.\n"},
    {"QLIMG",         "",        "q", "ql-img",                   ArgChecker::QL<false, true>,      usageQLText},
    {"QLIMG",         "LANDSAT", "",  "ql-landsat",               ArgChecker::QL<false, true>,      "  --ql-landsat=<img> \tThis option is used to represent the landsat 'pixel_qa' layer and will mark the states; cloud, medium or high confidence, and cloud shadows as locations to interpolate. It is equivalent to: '-b 3,5,7  --interp-ranges=[1,7]'.\n"},
    {"QLIMG",         "MODIS",   "",  "ql-modis",                 ArgChecker::QL<false, true>,      "  --ql-modis=<img> \tThis option is used to represent the modis 'Reflectance Data State QA' layer and will mark the states; cloudy, mixed and cloud shadow as locations to interpolate. It is equivalent to: '-b 0,1,2  --interp-ranges=[1,7]  --non-interp-ranges=[3,3]'.\n"},
    {"QLIMG",         "MFMASK",  "",  "ql-matlab-fmask",          ArgChecker::QL<false, true>,      "  --ql-matlab-fmask=<img> \tThis option is used to represent the quality layers generated with the matlab version of FMASK and will mark the states; cloud and cloud shadow as locations to interpolate. It is equivalent to: --interp-ranges='[2,2] [4,4]'.\n"},
    {"QLIMG",         "PFMASK",  "",  "ql-python-fmask",          ArgChecker::QL<false, true>,      "  --ql-python-fmask=<img> \tThis option is used to represent the quality layers generated with the python version of FMASK and will mark the states; cloud and cloud shadow as locations to interpolate. It is equivalent to: --interp-ranges=[2,3].\n"},
    {"STATS",         "",       "s",  "stats",                    ArgChecker::Optional,             "  -s, --stats, -s <out>, --stats=<out> \tEnable stats (cloud pixels before and after, etc.) and output into the given file. If no file is specified it is output to stdout.\n"},
    Descriptor::breakTable(),
    Descriptor::text("\nExample 1:\n"
    "  \timginterp \t--img='-f day1.tif -d 1' --img='-f day2.tif -d 2' --img='-f day3.tif -d 3' --img='-f day4.tif -d 4' --interp-ranges=[10000,inf]\n"
    "\twill interpolate the images with pixel values greater than or equal to 10000 from day 1 to day 4 and output them to interpolated_day1.tif, "
    "interpolated_day2.tif, interpolated_day3.tif, interpolated_day4.tif.\n\n"
    "\timginterp --option-file=InterpolationOpts\n"
    "\twhere the file InterpolationOpts contains\n"
    "\t  --img=(--file=day1.tif --date=1)\n"
    "\t  --img=(--file=day2.tif --date=2)\n"
    "\t  --img=(--file=day3.tif --date=3)\n"
    "\t  --img=(--file=day4.tif --date=4)\n"
    "\t  --interp-ranges=[10000,inf]\n"
    "\tdoes the same as the first line, but is easier to handle.\n"

    "\nExample 2:\n"
    "  \timginterp \t--img='-f day1.tif -d 1' --img='-f day2.tif -d 2' --img='-f day3.tif -d 3' --img='-f day4.tif -d 4' "
                             "--ql-fmask='-f ql1.tif -d 1' --ql-fmask='-f ql2.tif -d 2' --ql-fmask='-f ql3.tif -d 3' --ql-fmask='-f ql4.tif -d 4'\n"
    "\twill mask the cloud and cloud shadow pixel locations in the images using the quality layer file provided with the --ql-fmask option "
    "and then interpolate the images from day 1 to day 4 and output them to interpolated_day1.tif, interpolated_day2.tif, interpolated_day3.tif, interpolated_day4.tif.\n\n"
    "\timginterp --option-file=InterpolationOpts\n"
    "\twhere the file InterpolationOpts contains\n"
    "\t  --img=(--file=day1.tif --date=1)\n"
    "\t  --img=(--file=day2.tif --date=2)\n"
    "\t  --img=(--file=day3.tif --date=3)\n"
    "\t  --img=(--file=day4.tif --date=4)\n"
    "\t  --ql-fmask=(--file=ql1.tif --date=1)\n"
    "\t  --ql-fmask=(--file=ql2.tif --date=2)\n"
    "\t  --ql-fmask=(--file=ql3.tif --date=3)\n"
    "\t  --ql-fmask=(--file=ql4.tif --date=4)\n"
    "\tdoes the same as the first line, but is easier to handle.")
};



int main(int argc, char* argv[]) {
    imagefusion::option::OptionParser options(usage);
    // parse arguments with accepting options after non-option arguments, like ./interpolator file1.tif file2.tif file2.tif --out-diff=diff.tif
    std::string defaultArgs = "--enable-use-nodata --disable-output-pixelstate --disable-interp-invalid --enable-prioritize-invalid --out-prefix='interpolated_' --out-pixelstate-prefix='ps_' --limit-days=5";
    options.acceptsOptAfterNonOpts = true;
    options.parse(defaultArgs).parse(argc, argv);

    if (!options["HELP"].empty() || argc == 1) {
        printUsage(usage, /*auto line width*/ -1, /*last_column_min_percent*/ 10);
        return 0;
    }

    if (options.nonOptionArgCount() > 0){
        std::string givenargs;
        for (auto& o : options.nonOptionArgs)
            givenargs += o + ", ";
        givenargs.pop_back(); givenargs.pop_back();
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Please refer the help text for the proper usage of this utility. "
                                                               "We have identified the usage of following options: " + givenargs +
                                                               ". If you intend to use option file please provide your option as --option-file=<file>"));
    }

    // collect arguments for images, quality layers and masks
    imagefusion::MultiResCollection<std::string> imgArgs;
    auto gis = std::make_shared<imagefusion::MultiResCollection<GeoInfo>>();
    for (auto const& o : options["IMAGE"]) {
        std::string tag = Parse::ImageHasTag(o.arg) ? Parse::ImageTag(o.arg) : "";
        int date = Parse::ImageDate(o.arg);
        std::string filename = Parse::ImageFileName(o.arg);
        imgArgs.set(tag, date, o.arg);
        gis->set(tag, date, GeoInfo{filename});
    }

    imagefusion::MultiResCollection<std::string> qlImgArgs;
    for (auto const& o : options["QLIMG"]) {
        std::string predefined = "";
        if (o.prop() == "MODIS")
            predefined = "  -b 0,1,2  --interp-ranges=[1,7]  --non-interp-ranges=[3,3]";
        else if (o.prop() == "LANDSAT")
            predefined = "  -b 3,5,7  --interp-ranges=[1,7]"; // cloud confidence medium can occur without clouds, but still handle as clouds
        else if (o.prop() == "PFMASK")
            predefined = "  --interp-ranges=[2,3]";
        else if (o.prop() == "MFMASK")
            predefined = "  --interp-ranges='[2,2] [4,4]'";
        std::string tag = Parse::ImageHasTag(o.arg) ? Parse::ImageTag(o.arg) : "";
        int date = Parse::ImageDate(o.arg);
        qlImgArgs.set(tag, date, o.arg + predefined);
    }

    imagefusion::MultiResCollection<std::string> maskArgs;
    for (auto const& o : options["MASKIMG"]) {
        std::string tag = Parse::ImageHasTag(o.arg) ? Parse::ImageTag(o.arg) : "";
        int date = Parse::ImageDate(o.arg);
        maskArgs.set(tag, date, o.arg);
    }


    // combine mask for valid / invalid ranges and interp range
    imagefusion::IntervalSet baseValidSet;
    bool hasValidRanges = !options["MASKRANGE"].empty();
     if (hasValidRanges && options["MASKRANGE"].front().prop() == "INVALID")
        // if first range is invalid, start with all values valid
        baseValidSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    for (auto const& opt : options["MASKRANGE"]) {
        imagefusion::IntervalSet set = Parse::IntervalSet(opt.arg);
        if (opt.prop() == "VALID")
            baseValidSet += set;
        else
            baseValidSet -= set;
    }

    imagefusion::IntervalSet baseInterpSet;
    bool hasInterpRanges = !options["INTERPRANGE"].empty();
    if (hasInterpRanges && options["INTERPRANGE"].front().prop() == "INVALID")
       baseInterpSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    for (auto const& opt : options["INTERPRANGE"]) {
        imagefusion::IntervalSet set = Parse::IntervalSet(opt.arg);
        if (opt.prop() == "VALID")
            baseInterpSet += set;
        else
            baseInterpSet -= set;
    }

    bool doOutputStats = !options["STATS"].empty();
    std::vector<InterpStats> allStats;

    int dateLimit = Parse::Int(options["LIMIT"].back().arg);
    if (dateLimit < 0)
        IF_THROW_EXCEPTION(invalid_argument_error("The -l / --limit-date option must get a non-negative value."));

    auto prepost = helpers::getPrefixAndPostfix(options["OUTPREFIX"], options["OUTPOSTFIX"], "interpolated_", "output prefix");
    std::string& prefix_new  = prepost.first;
    std::string& postfix_new = prepost.second;

    auto prepostPS = helpers::getPrefixAndPostfix(options["OUTPSPREFIX"], options["OUTPSPOSTFIX"], "ps_", "pixelstate output prefix");
    std::string& prefixPS_new  = prepostPS.first;
    std::string& postfixPS_new = prepostPS.second;

    bool doOutputPS = options["PSOUT"].back().prop() == "ENABLE";
    bool useNodataValue = options["USENODATA"].back().prop() == "ENABLE";
    bool doInterpInvalid = options["INTINV"].back().prop() == "ENABLE";

    // process tags independendly
    auto tags = imgArgs.getResolutionTags();
    for (auto tag : tags){
        auto imgs   = std::make_shared<imagefusion::MultiResImages>();
        auto qlImgs = std::make_shared<imagefusion::MultiResImages>();
        auto masks  = std::make_shared<imagefusion::MultiResImages>();

        // find the dates to interpolate (image && QL available) and QL image dates
        std::vector<int> imgDates = imgArgs.getDates(tag);
        std::vector<int> interpDates = imgDates;
        std::vector<int> qlDates;
        {
            std::vector<int> datesTag = qlImgArgs.getDates(tag);
            std::vector<int> datesNoTag = qlImgArgs.getDates("");
            std::set_union(std::begin(datesTag), std::end(datesTag), std::begin(datesNoTag), std::end(datesNoTag),
                           std::back_inserter(qlDates));
        }

        if (!hasInterpRanges) {
            interpDates.clear();
            std::set_intersection(std::begin(qlDates), std::end(qlDates), std::begin(imgDates), std::end(imgDates),
                                  std::back_inserter(interpDates));
        }

        // interpolate each date
        for (int interpDate : interpDates) {
            int firstDate = interpDate - dateLimit;
            int lastDate = interpDate + dateLimit;

            // remove previous images, masks and QLs
            auto first_it = std::lower_bound(std::begin(imgDates), std::end(imgDates), firstDate);
            auto last_it  = std::upper_bound(std::begin(imgDates), std::end(imgDates), lastDate);
            std::vector<int> currentImgDates(first_it, last_it);
            std::vector<int> beforeImgDates(std::begin(imgDates), first_it);
            for (int remDate : boost::adaptors::reverse(beforeImgDates)) {
                if (!imgs->has(tag, remDate))
                    break;
                imgs->remove(tag, remDate);

                if (masks->has(tag, remDate))
                    masks->remove(tag, remDate);
                if (masks->has("", remDate))
                    masks->remove("", remDate);

                if (qlImgs->has(tag, remDate))
                    qlImgs->remove(tag, remDate);
                if (qlImgs->has("", remDate))
                    qlImgs->remove("", remDate);
            }

            // read missing images, mask images and QL images and generate and combine masks and QLs
            for (int addDate : boost::adaptors::reverse(currentImgDates)) {
                if (imgs->has(tag, addDate))
                    break;

                // image
                imagefusion::Size sz;
                {
                    std::string arg = imgArgs.get(tag, addDate);
                    auto imgInput = Parse::MRImage(arg, "", true, false, /* isTagOpt */ true);
                    sz = imgInput.i.size();
                    imgs->set(imgInput.tag, imgInput.date, std::move(imgInput.i));
                }

                // QL
                imagefusion::Image ql;
                if (qlImgArgs.has(tag, addDate)) {
                    std::string arg = qlImgArgs.get(tag, addDate);
                    auto imgInput = Parse::QL(arg, "", true, false, /* isTagOpt */ true);
                    ql = std::move(imgInput.i);
                }
                if (qlImgArgs.has("", addDate)) {
                    std::string arg = qlImgArgs.get("", addDate);
                    auto imgInput = Parse::QL(arg, "", true, false, /* isTagOpt */ true);
                    if (!ql.empty())
                        ql = ql.bitwise_or(std::move(imgInput.i));
                    else
                        ql = std::move(imgInput.i);
                }
                if (!ql.empty() && ql.size() != sz) {
                    using std::to_string;
                    using imagefusion::to_string;
                    std::string arg = qlImgArgs.has(tag, addDate) ? qlImgArgs.get(tag, addDate) : qlImgArgs.get("", addDate);
                    IF_THROW_EXCEPTION(imagefusion::size_error("The quality layer sizes must be equal to the image sizes. At date " + to_string(addDate)
                                                               + " the quality layer from argument (" + arg + ") has got a size of " + to_string(ql.size())
                                                               + " while the image on the same date from argument (" + imgArgs.get(tag, addDate)
                                                               + ") has got a size of " + to_string(sz) + "."))
                            << errinfo_size(ql.size());
                }


                if (hasInterpRanges) {
                    imagefusion::Image rangeQL = imgs->get(tag, addDate).createSingleChannelMaskFromSet({baseInterpSet}, /*useAnd*/ false);
                    if (!ql.empty())
                        ql = ql.bitwise_or(std::move(rangeQL));
                    else
                        ql = std::move(rangeQL);
                }

                // mask
                imagefusion::Image mask;
                if (maskArgs.has(tag, addDate)) {
                    std::string arg = maskArgs.get(tag, addDate);
                    auto imgInput = Parse::MRMask(arg, "", true, false, /* isTagOpt */ true);
                    mask = std::move(imgInput.i);
                }
                if (maskArgs.has("", addDate)) {
                    std::string arg = maskArgs.get("", addDate);
                    auto imgInput = Parse::MRMask(arg, "", true, false, /* isTagOpt */ true);
                    if (!mask.empty())
                        mask = mask.bitwise_and(std::move(imgInput.i));
                    else
                        mask = std::move(imgInput.i);
                }
                if (!mask.empty() && mask.size() != sz) {
                    using std::to_string;
                    using imagefusion::to_string;
                    std::string arg = maskArgs.has(tag, addDate) ? maskArgs.get(tag, addDate) : maskArgs.get("", addDate);
                    IF_THROW_EXCEPTION(imagefusion::size_error("The mask sizes must be equal to the image sizes. At date " + to_string(addDate)
                                                               + " the mask from argument (" + arg + ") has got a size of " + to_string(mask.size())
                                                               + " while the image on the same date from argument (" + imgArgs.get(tag, addDate)
                                                               + ") has got a size of " + to_string(sz) + "."))
                            << errinfo_size(ql.size());
                }

                imagefusion::IntervalSet validSet = baseValidSet;
                if (!hasValidRanges)
                    validSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

                GeoInfo& gi = gis->get(tag, addDate);
                if (useNodataValue && gi.hasNodataValue()) {
                    imagefusion::Interval nodataInt = imagefusion::Interval::closed(gi.getNodataValue(), gi.getNodataValue());
                    validSet -= nodataInt;
                }

                if (hasValidRanges || (useNodataValue && gi.hasNodataValue())) {
                    if (mask.empty())
                        mask = imgs->get(tag, addDate).createMultiChannelMaskFromSet({validSet});
                    else {
                        Image temp_mask;
                        if (mask.channels() > 1)
                            temp_mask = imgs->get(tag, addDate).createMultiChannelMaskFromSet({validSet});
                        else
                            temp_mask = imgs->get(tag, addDate).createSingleChannelMaskFromSet({validSet});
                        mask = mask.bitwise_and(std::move(temp_mask));
                    }
                }

                if (!mask.empty()) {
                    if (doInterpInvalid)
                        // ql is always single-channel, so reduce mask and invert, since 0 in mask should be interpolated ==> 255 in ql
                        ql = mask.createSingleChannelMaskFromRange({imagefusion::Interval::closed(0, 0)}, /*useAnd*/ false /*because it is inverted*/).bitwise_or(ql);
                    else
                        masks->set(tag, addDate, std::move(mask));
                }

                if (!ql.empty())
                    qlImgs->set(tag, addDate, std::move(ql));

            }

            // interpolate
            bool doPreferCloudsOverNodata = options["PRIOCLOUDS"].back().prop() == "ENABLE";
            Type t = imgs->getAny().type();
            auto imgInterped_and_pixelState_and_stats = CallBaseTypeFunctor::run(Interpolator{*imgs, *qlImgs, *masks, tag, interpDate, doPreferCloudsOverNodata}, t);
            Image& imgInterped = std::get<0>(imgInterped_and_pixelState_and_stats);
            Image& pixelState  = std::get<1>(imgInterped_and_pixelState_and_stats);
            InterpStats& stats = std::get<2>(imgInterped_and_pixelState_and_stats);

            constexpr uint8_t nonInterpd = static_cast<uint8_t>(PixelState::noninterpolated);
            constexpr uint8_t wasInvalid = static_cast<uint8_t>(PixelState::nodata);
            Image maskNowInvalid = pixelState.createMultiChannelMaskFromSet({IntervalSet(Interval::closed(wasInvalid, wasInvalid))
                                                                                       + Interval::closed(nonInterpd, nonInterpd)});

            // try to set nodata value in metadata and set not interpolated locations to nodata value
            std::string const& inputfilename = Parse::ImageFileName(imgArgs.get(tag, interpDate));
            imagefusion::GeoInfo& gi = gis->get(tag, interpDate);
            if (!gi.hasNodataValue()) {
                double ndv = helpers::findAppropriateNodataValue(imgInterped, maskNowInvalid.bitwise_not());
                if (std::isnan(ndv)) {
                    std::cerr << "Setting the non-interpolated location to a nodata value failed, since all possible values exist in the image " << inputfilename << ".";
                    if (!doOutputPS)
                        std::cerr << " Therefore the pixelstate will be output.";
                    std::cerr << std::endl;
                }
                else {
                    gi.setNodataValue(ndv);
                    std::cout << "Changed nodata value to " << ndv << " for image " << inputfilename << "." << std::endl;
                }
            }

            if (gi.hasNodataValue())
                imgInterped.set(gi.getNodataValue(), maskNowInvalid);

            try {
                // output
                auto outformat = imagefusion::FileFormat::fromFile(inputfilename);
                std::string outfilename = helpers::outputImageFile(imgInterped, gi, inputfilename, prefix_new, postfix_new, outformat);
                std::string printStatus = "Interpolated and wrote file " + outfilename + ".";

                // output pixel state
                if (doOutputPS || !gi.hasNodataValue()) {
                    std::string outPSFilename = helpers::outputImageFile(pixelState, gi, outfilename, prefixPS_new, postfixPS_new, outformat);
                    printStatus += " Wrote pixel state bitfield to " + outPSFilename + ".";
                }
                std::cout << printStatus << std::endl;
            }
            catch (imagefusion::runtime_error&) {
                std::cout << "Could not write the output of processing " << inputfilename << ", sorry. Going on with the next one." << std::endl;
            }

            // collect stats
            if (doOutputStats) {
                stats.filename = inputfilename;
                allStats.push_back(stats);
            }
        } /*interpDate loop*/
    } /*tag loop*/

    if (doOutputPS) {
        std::cout << "Note: Pixel state bitfield have the values "
                  << static_cast<int>(PixelState::nodata) << " for nodata locations, "
                  << static_cast<int>(PixelState::noninterpolated) << " for locations that could not be interpolated, "
                  << static_cast<int>(PixelState::interpolated) << " for interpolated locations and "
                  << static_cast<int>(PixelState::clear) << " for clear locations." << std::endl;
    }

    // print stats
    if (!allStats.empty()) {
        std::string const& arg = options["STATS"].back().arg;
        if (!arg.empty()) {
            // print to CSV file
            std::ofstream fOutStats{arg};
            if (fOutStats.is_open()) {
                fOutStats << "filename; date; width; height; channels; total number of values; "
                             "number of nodata values; number of values to interpolate; "
                             "number of not interpolated values\n";
                for (auto const& s : allStats) {
                    fOutStats << s.filename      << "; "
                              << s.date          << "; "
                              << s.sz.width      << "; "
                              << s.sz.height     << "; "
                              << s.nChans        << "; "
                              << (s.sz.width * s.sz.height * s.nChans) << "; "
                              << s.nNoData       << "; "
                              << s.nInterpBefore << "; "
                              << s.nInterpAfter  << "\n";
                }
            }
        }
        else {
            // print to stdout as table
            using std::setw;

            auto it_max = std::max_element(allStats.begin(), allStats.end(), [] (InterpStats const& a, InterpStats const& b) {return a.filename.length() < b.filename.length();});
            size_t wFilename = std::max(static_cast<int>(it_max->filename.length()), 8);

            it_max = std::max_element(allStats.begin(), allStats.end(), [] (InterpStats const& a, InterpStats const& b) {return a.sz.width < b.sz.width;});
            size_t wSize = 1 + std::to_string(it_max->sz.width).length();
            it_max = std::max_element(allStats.begin(), allStats.end(), [] (InterpStats const& a, InterpStats const& b) {return a.sz.height < b.sz.height;});
            wSize = std::max(static_cast<int>(wSize + std::to_string(it_max->sz.height).length()), 4);

            it_max = std::max_element(allStats.begin(), allStats.end(), [] (InterpStats const& a, InterpStats const& b) {return std::to_string(a.date).length() < std::to_string(b.date).length();});
            size_t wDate = std::max(static_cast<int>(std::to_string(it_max->date).length()), 4);

            std::cout << "Stats:" << std::endl
                      << setw(wFilename) << "Filename" << "  " << setw(wDate) << "Date" << "  " << setw(wSize) << "Size"
                      << "  Channels  No. of values  No. of nodata values  No. of interp values  No. of not interpolated values\n";
            //              8         13             20                    20                    30
            for (auto const& s : allStats) {
                std::cout << setw(wFilename) << s.filename << "  "
                          << setw(wDate)     << s.date << "  "
                          << setw(wSize)     << (std::to_string(s.sz.width) + "x" + std::to_string(s.sz.height)) << "  "
                          << setw(8)         << s.nChans << "  "
                          << setw(13)        << (s.sz.width * s.sz.height * s.nChans) << "  "
                          << setw(20)        << s.nNoData << "  "
                          << setw(20)        << s.nInterpBefore << "  "
                          << setw(30)        << s.nInterpAfter << "\n";
            }
        }
    }

    return 0;
}


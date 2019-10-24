#include <iostream>
#include <string>
#include <iterator>
#include <memory>

#include <boost/filesystem.hpp>

#include "optionparser.h"
#include "GeoInfo.h"
#include "Image.h"
#include "MultiResImages.h"
#include "Estarfm.h"
#include "EstarfmOptions.h"
#include "fileformat.h"

#ifdef WITH_OMP
    #include "Parallelizer.h"
    #include "ParallelizerOptions.h"
#endif /* WITH_OMP */

#include "utils_common.h"


const char* usageImage =
    "  -i <img>, --img=<img> \tInput image. At least five images are required: "
    "two pairs of high and low resolution images and one low resolution image at a date inbetween "
    "to predict the corresponding missing high resolution image. "
    "If you want to predict more images, just add more. For each low resolution image lacking a corresponding "
    "high resolution image a prediction will be made. You can also add more pairs to predict multiple time series.\n"
    "\t<img> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', "
    "where the arguments can have an arbitrary order. "
    "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"
    "\t  -f <file>,     --file=<file> \tSpecifies the image file path (string).\n"
    "\t  -d <num>,      --date=<num>, \tSpecifies the date (number).\n"
    "\t  -t <tag>,      --tag=<tag>, \tSpecifies the resolution tag (string).\n"
    "\t  -l <num-list>, --layers=<num-list> \tOptional. Specifies the channels, bands or layers, that will be read. Hereby a 0 means the first channel.\n"
    "\t<num-list> must have the format '(<num> [[,]<num> ...])' or just '<num>'.\n"
    "\t  -c <rect>, --crop=<rect> \tOptional. Specifies the crop window, where the "
    "image will be read. A zero width or height means full width or height, respectively.\n"
    "\t<rect> requires either all of the following arguments:\v"
    "  -c (<num> <num), --center=(<num> <num>) x and y center\v"
    "  -w <num>, --width=<num>  width\v"
    "  -h <num>, --height=<num> height\v"
    "or x can be specified with:\v"
    "  -x <num>                 x start and\v"
    "  -w <num>, --width=<num>  width or just with\v"
    "  -x (<num> <num>)         x extents\v"
    "and y can be specified with:\v"
    "  -y <num>                 y start and\v"
    "  -h <num>, --height=<num> height or just with\v"
    "  -y (<num> <num>)         y extents\v"
    "Examples: --img='--file=\"test image.tif\" -d 0 -t HIGH'\v"
    "          --img='-f test.tif -d 0 -t HIGH --crop=(-x 1 -y 2 -w 3 -h 4) --layers=(0 2)'\v"
    "          --img='-f test.tif -d 0 -t HIGH --crop=(-x=(1 3) -y=(2 5))'\n";


using Descriptor = imagefusion::option::Descriptor;
using ArgChecker = imagefusion::option::ArgChecker;
using Parse      = imagefusion::option::Parse;
std::vector<Descriptor> usage{
    Descriptor::text("Usage: estarfm -i <img> -i <img> -i <img> -i <img> -i <img> [options]\n"
                     "   or: estarfm --option-file=<file> [options]\n\n"
                     "The order of the options can be arbitrary, but at least five images are required for prediction.  Just add more images to do more predictions. If you provide more "
                     "than two high resolution images, multiple time series will be predicted. Remember to protect whitespace by quoting with '...', \"...\" or (...) or by escaping.\n"
                     "Options:"),
    {"LOCALTOL",      "DISABLE",     "",  "disable-local-tol",            ArgChecker::None,        "  --disable-local-tol    \tThis disables the usage of local tolerances to find similar pixels and uses the global tolerance instead. Default.\n"},
    {"MASKOUT",       "DISABLE",     "",  "disable-output-masks",         ArgChecker::None,        "  --disable-output-masks \tThis disables the output of the masks that are used for the predictions.\n"},
    {"REGQUAL",       "DISABLE",     "",  "disable-smt-reg-qlty",         ArgChecker::None,        "  --disable-smt-reg-qlty \tThis disables the smooth weighting of the regression coefficient by its quality. The quality is still used, but with a hard limitation of 95% to use the regression coefficient. Default.\n"},
    {"USENODATA",     "DISABLE",     "",  "disable-use-nodata",           ArgChecker::None,        "  --disable-use-nodata   \tThis will not use the nodata value as invalid range for masking.\n"},
    {"LOCALTOL",      "ENABLE",      "",  "enable-local-tol",             ArgChecker::None,        "  --enable-local-tol     \tThis enables the usage of local tolerances to find similar pixels instead of using the global tolerance.\n"},
    {"MASKOUT",       "ENABLE",      "",  "enable-output-masks",          ArgChecker::None,        "  --enable-output-masks  \tThis enables the output of the masks that are used for the predictions. If no mask are used, there will be put out nothing. Default.\n"},
    {"REGQUAL",       "ENABLE",      "",  "enable-smt-reg-qlty",          ArgChecker::None,        "  --enable-smt-reg-qlty  \tThis enables the smooth weighting of the regression coefficient by its quality. The regression coefficient is not limited strictly by the quality, but linearly blended to 1 in case of bad quality.\n"},
    {"USENODATA",     "ENABLE",      "",  "enable-use-nodata",            ArgChecker::None,        "  --enable-use-nodata    \tThis will use the nodata value as invalid range for masking. Default.\n"},
    {"HELP",          "",            "h", "help",                         ArgChecker::None,        "  -h, --help  \tPrint usage and exit.\n"},
    {"HELPFORMAT",    "",            "",  "help-formats",                 ArgChecker::None,        "  --help-formats  \tPrint all available file formats that can be used with --out-format and exit.\n"},
    {"IMAGE",         "",            "i", "img",                          ArgChecker::MRImage<>,   usageImage},
    {"MASKIMG",       "",            "m", "mask-img",                     ArgChecker::Mask,        helpers::usageMaskFile},
    {"MASKRANGE",     "HIGHINVALID", "",  "mask-high-res-invalid-ranges", ArgChecker::IntervalSet, "  --mask-high-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the high resolution images.\n"},
    {"MASKRANGE",     "HIGHVALID",   "",  "mask-high-res-valid-ranges",   ArgChecker::IntervalSet, "  --mask-high-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the high resolution images.\n"},
    {"MASKRANGE",     "INVALID",     "",  "mask-invalid-ranges",          ArgChecker::IntervalSet, helpers::usageInvalidRanges},
    {"MASKRANGE",     "LOWINVALID",  "",  "mask-low-res-invalid-ranges",  ArgChecker::IntervalSet, "  --mask-low-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the low resolution images.\n"},
    {"MASKRANGE",     "LOWVALID",    "",  "mask-low-res-valid-ranges",    ArgChecker::IntervalSet, "  --mask-low-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the low resolution images.\n"},
    {"MASKRANGE",     "VALID",       "",  "mask-valid-ranges",            ArgChecker::IntervalSet, helpers::usageValidRanges},
    {"CLASSES",       "",            "n", "number-classes",               ArgChecker::Float,       "  -n <float>, --number-classes=<float> \tThe number of classes used for similarity. Note all channels of a pixel are considered for similarity. So this value holds for each channel, e. g. with 3 channels there are n^3 classes. Default: c-th root of 64, where c is the number of channels.\n"},
    Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file are specified in the same way as on the command line. You can use newlines between options "
                     "and line comments with # (use \\# to get a non-comment #). The specified options in the file replace the --option-file=<file> argument before they are parsed.\n"),
    {"FORMAT",        "",            "f", "out-format",                   ArgChecker::NonEmpty,    "  -f <fmt>, --out-format=<fmt>  \tUse the specified image file format, like GTiff, as output. See also --help-formats.\n"},
    {"OUTMASKPOSTFIX","",            "",  "out-mask-postfix",             ArgChecker::Optional,    "  --out-mask-postfix=<string> \tThis will be appended to the mask output filenames. Only used if mask output is enabled.\n"},
    {"OUTMASKPREFIX", "",            "",  "out-mask-prefix",              ArgChecker::Optional,    "  --out-mask-prefix=<string> \tThis will be prepended to the output filenames. Only used if mask output is enabled. By default this is 'mask_'.\n"},
    {"OUTPOSTFIX",    "",            "",  "out-postfix",                  ArgChecker::Optional,    "  --out-postfix=<string> \tThis will be appended to the output filenames.\n" },
    {"OUTPREFIX",     "",            "",  "out-prefix",                   ArgChecker::Optional,    "  --out-prefix=<string> \tThis will be prepended to the output filenames. By default this is 'predicted_'.\n"},
    {"PREDAREA",      "",            "",  "pred-area",                    ArgChecker::Rectangle,   "  --pred-area=<rect> \tSpecifies the prediction area. The prediction will only be done in this area. <rect> requires all of the following arguments:\v"
                                                                                                           "  -x <num>                 x start\v"
                                                                                                           "  -y <num>                 y start\v"
                                                                                                           "  -w <num>, --width=<num>  width\v"
                                                                                                           "  -h <num>, --height=<num> height\v"
                                                                                                           "Examples: --pred-area='-x 1 -y 2 -w 3 -h 4'\n"},
    {"WINSIZE",       "",            "w", "win-size",                     ArgChecker::Int,         "  -w <num>, --win-size=<num> \tWindow size of the rectangle around the current pixel. Default: 51.\n"},
    {"UNCERTAINTY",   "",            "u", "uncertainty",                  ArgChecker::Float,       "  -u <float>, --uncertainty=<float> \tSets the uncertainty factor. This is multiplied to the upper limit of the high resolution range. The range can be given by a mask range. Default: 0.002 (i. e. 0.2%).\n"},
    Descriptor::breakTable(),
    Descriptor::text("\nExamples:\n"
                     "  \testarfm --img='-f h1.tif -d 1 -t high' --img='-f h3.tif -d 3 -t high' --img='-f l1.tif -d 1 -t low' --img='-f l2.tif -d 2 -t low' --img='-f l3.tif -d 3 -t low'\v"
                     "will predict the high resolution image at date 2 and output it to predicted_2.tif.\v\v"
                     "estarfm --option-file=estarfmOpts\v"
                     "where the file estarfmOpts contains\v"
                     "  --img=(--file=h1.tif --date=1 --tag=high)\v"
                     "  --img=(--file=h3.tif --date=3 --tag=high)\v"
                     "  --img=(--file=l1.tif --date=1 --tag=low) \v"
                     "  --img=(--file=l2.tif --date=2 --tag=low) \v"
                     "  --img=(--file=l3.tif --date=3 --tag=low) \v"
                     "does the same as the first example, but is easier to handle.\v\v")
};


int main(int argc, char* argv[]) {
    // parse arguments
    std::string defaultArgs = "--out-prefix=predicted_ --out-mask-prefix=mask_ --disable-local-tol --enable-output-masks --win-size=51 --uncertainty=0.002 --disable-smt-reg-qlty --enable-use-nodata";
    auto options = imagefusion::option::OptionParser::parse(usage, argc, argv, defaultArgs);

    if (!options["HELP"].empty() || argc == 1) {
        printUsage(usage, -1, 10);
        return 0;
    }

    if (!options["HELPFORMAT"].empty()) {
        auto all = imagefusion::FileFormat::supportedFormats();
        std::cout << std::left << std::setw(16) << "Output formats" << " (description)\n";
        for (auto f : all)
            std::cout << std::left << std::setw(16) << f << " (" << f.longName() << ")\n";
        std::cout << std::flush;
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

    // set prediction area from options. If no prediction area was given, it will be set to full size later on
    imagefusion::Rectangle predArea;
    if (!options["PREDAREA"].empty())
        predArea = Parse::Rectangle(options["PREDAREA"].back().arg);

    // collect arguments for images and read geoinfo
    std::vector<std::string> plainImgArgs;
    for (auto& o : options["IMAGE"])
        plainImgArgs.push_back(o.arg);
    auto args_and_gis = helpers::parseImgsArgsAndGeoInfo<Parse>(
                plainImgArgs, /*minImages*/ 5, /*numResTags*/ 2, predArea,
                "One for high resolution one for low resolution. The tag with less images "
                "will be used as high resolution tag and the other one as low resolution tag. ");
    imagefusion::MultiResCollection<std::string>& imgArgs = args_and_gis.first;
    imagefusion::MultiResCollection<imagefusion::GeoInfo>& gis = args_and_gis.second;

    // collect the dates in a job hierarchy, like [(1) 3 4 (7)] [(7) 10 12 13 (14)] [(14) 15]
    helpers::JobsAndTags jat = helpers::parseJobs(imgArgs, 2, /* remove single-pair dates */ true, /* single-pair mode */ false);

    // collect and combine mask images with AND
    std::vector<std::string> maskImgArgs;
    for (auto const& opt : options["MASKIMG"])
        maskImgArgs.push_back(opt.arg);
    imagefusion::Image baseMask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, gis.getAny().channels, !options["MASKRANGE"].empty());

    // combine valid / invalid ranges
    auto baseValidSets = helpers::parseAndCombineRanges<Parse>(options["MASKRANGE"]);
    bool useNodataValue = options["USENODATA"].back().prop() == "ENABLE";

    // output name options
    auto prepost = helpers::getPrefixAndPostfix(options["OUTPREFIX"], options["OUTPOSTFIX"], "predicted_", "output prefix");
    std::string prefix  = prepost.first;
    std::string postfix = prepost.second;

    prepost = helpers::getPrefixAndPostfix(options["OUTMASKPREFIX"], options["OUTMASKPOSTFIX"], "mask_", "mask prefix");
    std::string maskprefix  = prepost.first;
    std::string maskpostfix = prepost.second;

    // output format
    imagefusion::FileFormat outformat = imagefusion::FileFormat::unsupported;
    if (!options["FORMAT"].empty())
        outformat = options["FORMAT"].back().arg;

    // set ESTARFM options
    imagefusion::EstarfmOptions estarfmOpts;
    estarfmOpts.setHighResTag(jat.highTag);
    estarfmOpts.setLowResTag(jat.lowTag);
    estarfmOpts.setWinSize(Parse::Int(options["WINSIZE"].back().arg));
    estarfmOpts.setUncertaintyFactor(Parse::Float(options["UNCERTAINTY"].back().arg));
    estarfmOpts.setUseLocalTol(options["LOCALTOL"].back().prop() == "ENABLE");
    estarfmOpts.setUseQualityWeightedRegression(options["REGQUAL"].back().prop() == "ENABLE");

    if (!baseValidSets.high.empty()) {
        imagefusion::Interval const& first = *baseValidSets.high.begin();
        imagefusion::Interval const& last  = *(--baseValidSets.high.end());
        double rangeMin = first.lower();
        double rangeMax = last.upper();
        std::cout << "Data range: [" << rangeMin << ", " << rangeMax << "]" << std::endl;
        estarfmOpts.setDataRange(rangeMin, rangeMax);
    }

    double numClasses = std::pow(64, 1. / gis.getAny().channels);
    if (!options["CLASSES"].empty())
        numClasses = Parse::Float(options["CLASSES"].back().arg);
    estarfmOpts.setNumberClasses(numClasses);

    if (predArea == imagefusion::Rectangle{})
        predArea = imagefusion::Rectangle(0, 0, gis.getAny().width(), gis.getAny().height());
    #ifdef WITH_OMP
        imagefusion::ParallelizerOptions<imagefusion::EstarfmOptions> parOpts;
        parOpts.setPredictionArea(predArea);
        imagefusion::Parallelizer<imagefusion::EstarfmFusor> estarfm;
    #else /* WITH_OMP not defined */
        estarfmOpts.setPredictionArea(predArea);
        imagefusion::EstarfmFusor estarfm;
    #endif /* WITH_OMP */

    auto mri = std::make_shared<imagefusion::MultiResImages>();
    estarfm.srcImages(mri);

    // loop over multiple time series (multiple input pairs)
    bool doWriteMasks = options["MASKOUT"].back().prop() == "ENABLE";
    for (auto const& p : jat.jobs) {
        auto const& pairDate_vec = p.first;
        assert(pairDate_vec.size() == 2 && "Found just a single pair, which is not supported here.");
        int date1 = pairDate_vec.front();
        int date3 = pairDate_vec.back();
        estarfmOpts.setDate1(date1);
        estarfmOpts.setDate3(date3);
        assert(date1 < date3 && "Jobs need to be sorted.");

        #ifdef WITH_OMP
            parOpts.setAlgOptions(estarfmOpts);
            estarfm.processOptions(parOpts);
        #else /* WITH_OMP not defined */
            estarfm.processOptions(estarfmOpts);
        #endif /* WITH_OMP */

        imagefusion::Image pairMask = baseMask;
        for (int datePair : pairDate_vec) {
            // read in pair images
            if (!mri->has(jat.highTag, datePair)) {
                auto in = Parse::MRImage(imgArgs.get(jat.highTag, datePair));
                mri->set(jat.highTag, datePair, std::move(in.i));
            }
            if (!mri->has(jat.lowTag, datePair)) {
                auto in = Parse::MRImage(imgArgs.get(jat.lowTag, datePair));
                mri->set(jat.lowTag, datePair, std::move(in.i));
            }

            // add mask from nodata value and valid / invalid ranges for pair images base mask
            auto pairValidSets = baseValidSets;
            if (useNodataValue) {
                if (!pairValidSets.hasHigh)
                    pairValidSets.high += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
                if (!pairValidSets.hasLow)
                    pairValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

                pairValidSets.hasHigh = pairValidSets.hasLow = true;

                imagefusion::GeoInfo const& giHigh = gis.get(jat.highTag, datePair);
                if (giHigh.hasNodataValue()) {
                    imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHigh.getNodataValue(), giHigh.getNodataValue());
                    pairValidSets.high -= nodataInt;
                }
                imagefusion::GeoInfo const& giLow = gis.get(jat.lowTag, datePair);
                if (giLow.hasNodataValue()) {
                    imagefusion::Interval nodataInt = imagefusion::Interval::closed(giLow.getNodataValue(), giLow.getNodataValue());
                    pairValidSets.low -= nodataInt;
                }
            }

            if (pairValidSets.hasHigh)
                pairMask = helpers::processSetMask(std::move(pairMask), mri->get(jat.highTag, datePair), pairValidSets.high);
            if (pairValidSets.hasLow)
                pairMask = helpers::processSetMask(std::move(pairMask), mri->get(jat.lowTag,  datePair), pairValidSets.low);
        }

        // loop over a single time series (multiple images with the same date 1 and 3)
        for (int date2 : p.second) {
            // read in prediction image
            if (!mri->has(jat.lowTag, date2)) {
                auto in = Parse::MRImage(imgArgs.get(jat.lowTag, date2));
                mri->set(jat.lowTag, date2, std::move(in.i));
            }

            // add mask from nodata value and valid / invalid ranges for prediction image to pair mask
            imagefusion::Image predMask = pairMask;
            imagefusion::GeoInfo giPred = gis.get(jat.lowTag, date2);
            auto predValidSets = baseValidSets;
            if (useNodataValue) {
                if (!predValidSets.hasLow)
                    predValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
                predValidSets.hasLow = true;

                if (giPred.hasNodataValue()) {
                    imagefusion::Interval nodataInt = imagefusion::Interval::closed(giPred.getNodataValue(), giPred.getNodataValue());
                    predValidSets.low -= nodataInt;
                }
            }

            if (predValidSets.hasLow)
                predMask = helpers::processSetMask(std::move(predMask), mri->get(jat.lowTag, date2), predValidSets.low);

            // predict a single image
            std::cout << "Predicting for date " << date2 << " using pairs from dates " << date1 << " and " << date3 << "." << std::endl;
            estarfm.predict(date2, predMask);
            auto& out = estarfm.outputImage();
            std::cout << "Prediction done. ";

            // output result and mask
            std::string filename = Parse::ImageFileName(imgArgs.get(jat.lowTag, date2));
            if (outformat == imagefusion::FileFormat::unsupported)
                outformat = imagefusion::FileFormat::fromFile(filename);

            try {
                std::string maskOutInfo;
                if (!predMask.empty()) {
                    if (!giPred.hasNodataValue()) {
                        double ndv = helpers::findAppropriateNodataValue(out, predMask);
                        if (!std::isnan(ndv))
                            giPred.setNodataValue(ndv);
                    }
                    if (giPred.hasNodataValue())
                        out.set(giPred.getNodataValue(), predMask.bitwise_not());

                    if (doWriteMasks) {
                        std::string outmaskfilename = helpers::outputImageFile(predMask, giPred, filename, maskprefix, maskpostfix, outformat, date1, date2, date3);
                        maskOutInfo = " and its mask to " + outmaskfilename;
                    }
                }

                std::string outfilename = helpers::outputImageFile(out, giPred, filename, prefix, postfix, outformat, date1, date2, date3);
                std::cout << "Wrote predicted image to " << outfilename << maskOutInfo << "." << std::endl;
            }
            catch (imagefusion::runtime_error&) {
                std::cout << "Could not write the output of processing " << filename << ", sorry. Going on with the next one." << std::endl;
            }

            // remove prediction image
            if (mri->has(jat.lowTag, date2))
                mri->remove(jat.lowTag, date2);
        }

        // remove first pair images
        if (mri->has(jat.highTag, date1))
            mri->remove(jat.highTag, date1);
        if (mri->has(jat.lowTag, date1))
            mri->remove(jat.lowTag, date1);
    }

    return 0;
}

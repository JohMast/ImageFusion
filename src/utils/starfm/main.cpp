#include <iostream>
#include <string>
#include <iterator>
#include <memory>

#include <boost/filesystem.hpp>

#include "optionparser.h"
#include "GeoInfo.h"
#include "Image.h"
#include "MultiResImages.h"
#include "Starfm.h"
#include "StarfmOptions.h"
#include "fileformat.h"

#ifdef WITH_OMP
    #include "Parallelizer.h"
    #include "ParallelizerOptions.h"
#endif /* WITH_OMP */

#include "utils_common.h"


const char* usageImage =
    "  -i <img>, --img=<img> \tInput image. At least three images are required: "
    "one pair of high and low resolution images and one low resolution image at a date "
    "to predict the corresponding missing high resolution image. "
    "If you want to predict more images, just add more. For each low resolution image lacking a corresponding "
    "high resolution image a prediction will be made. You can also add more pairs. Then for each prediction "
    "inbetween two pairs the prediction will be done from left and from right.\n"
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
    Descriptor::text("Usage: starfm -i <img> -i <img> -i <img> -i <img> -i <img> [options]\n"
                     "   or: starfm --option-file=<file> [options]\n\n"
                     "The order of the options can be arbitrary, but at least five images are required for prediction.  Just add more images to do more predictions. If you provide more than two high resolution images, "
                     "multiple time series will be predicted. Remember to protect whitespace by quoting with '...', \"...\" or (...) or by escaping.\n"
                     "Options:"),
    {"TEMPWEIGHT",    "",            "",  "auto-temporal-weighting",      ArgChecker::None,        "  --auto-temporal-weighting    \tUse temporal difference in the candidates weight only when having two image pairs around the prediction date. This is the behaviour of the reference implementation.\n"},
    {"COPY0DIFF",     "DISABLE",     "",  "disable-copy-on-zero-diff",    ArgChecker::None,        "  --disable-copy-on-zero-diff  \tPredict for all pixels, even for pixels with zero temporal or spectral difference (behaviour of the reference implementation). Default.\n"},
    {"TWOPAIRMODE",   "DISABLE",     "",  "disable-double-pair-mode",     ArgChecker::None,        "  --disable-double-pair-mode   \tWhen a prediction date is inbetween two pairs make two separate predictions with the single pair mode (one from the date before, one from the date after) instead of using both pairs at for one prediction.\n"},
    {"MASKOUT",       "DISABLE",     "",  "disable-output-masks",         ArgChecker::None,        "  --disable-output-masks       \tThis disables the output of the masks that are used for the predictions.\n"},
    {"STRICT",        "DISABLE",     "",  "disable-strict-filtering",     ArgChecker::None,        "  --disable-strict-filtering   \tUse loose filtering, which means that candidate pixels will be accepted if they have less temporal *or* spectral difference than the central pixel (behaviour of the reference implementation). Default.\n"},
    {"TEMPWEIGHT",    "DISABLE",     "",  "disable-temporal-weighting",   ArgChecker::None,        "  --disable-temporal-weighting \tDo not use temporal difference in the candidates weight, only distance and spectral difference (behaviour of the reference implementation for a single pair).\n"},
    {"USENODATA",     "DISABLE",     "",  "disable-use-nodata",           ArgChecker::None,        "  --disable-use-nodata   \tThis will not use the nodata value as invalid range for masking.\n" },
    {"COPY0DIFF",     "ENABLE",      "",  "enable-copy-on-zero-diff",     ArgChecker::None,        "  --enable-copy-on-zero-diff   \tCopy high resolution pixels on zero temporal difference and new low resolution pixels on zero spectral difference (like assumed in the paper).\n"},
    {"TWOPAIRMODE",   "ENABLE",      "",  "enable-double-pair-mode",      ArgChecker::None,        "  --enable-double-pair-mode    \tWhen a prediction date is inbetween two pairs use both pairs for the prediction with the double pair mode. Default.\n"},
    {"MASKOUT",       "ENABLE",      "",  "enable-output-masks",          ArgChecker::None,        "  --enable-output-masks        \tThis enables the output of the masks that are used for the predictions. If no mask are used, there will be put out nothing. Default.\n"},
    {"STRICT",        "ENABLE",      "",  "enable-strict-filtering",      ArgChecker::None,        "  --enable-strict-filtering    \tUse strict filtering, which means that candidate pixels will be accepted only if they have less temporal *and* spectral difference than the central pixel (like in the paper).\n"},
    {"TEMPWEIGHT",    "ENABLE",      "",  "enable-temporal-weighting",    ArgChecker::None,        "  --enable-temporal-weighting  \tUse temporal difference in the candidates weight (like in the paper). Default.\n"},
    {"USENODATA",     "ENABLE",      "",  "enable-use-nodata",            ArgChecker::None,        "  --enable-use-nodata    \tThis will use the nodata value as invalid range for masking. Default.\n"},
    {"HELP",          "",            "h", "help",                         ArgChecker::None,        "  -h, --help  \tPrint usage and exit.\n"},
    {"HELPFORMAT",    "",            "",  "help-formats",                 ArgChecker::None,        "  --help-formats  \tPrint all available file formats that can be used with --out-format and exit.\n"},
    {"IMAGE",         "",            "i", "img",                          ArgChecker::MRImage<>,   usageImage},
    {"LOGSCALE",      "",            "l", "log-scale",                    ArgChecker::Float,       "  -l <float>, --log-scale=<float> \tWhen using a positive scale, the logistic weighting formula is used, which reduces the influence of spectral and temporal differences. Default is 0, i. e. logistic formula not used.\n"},
    {"MASKIMG",       "",            "m", "mask-img",                     ArgChecker::Mask,        helpers::usageMaskFile},
    {"MASKRANGE",     "HIGHINVALID", "",  "mask-high-res-invalid-ranges", ArgChecker::IntervalSet, "  --mask-high-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the high resolution images.\n"},
    {"MASKRANGE",     "HIGHVALID",   "",  "mask-high-res-valid-ranges",   ArgChecker::IntervalSet, "  --mask-high-res-valid-ranges=<range-list>   \tThis is the same as --mask-valid-ranges, but is applied only for the high resolution images.\n"},
    {"MASKRANGE",     "INVALID",     "",  "mask-invalid-ranges",          ArgChecker::IntervalSet, helpers::usageInvalidRanges},
    {"MASKRANGE",     "LOWINVALID",  "",  "mask-low-res-invalid-ranges",  ArgChecker::IntervalSet, "  --mask-low-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the low resolution images.\n"},
    {"MASKRANGE",     "LOWVALID",    "",  "mask-low-res-valid-ranges",    ArgChecker::IntervalSet, "  --mask-low-res-valid-ranges=<range-list>   \tThis is the same as --mask-valid-ranges, but is applied only for the low resolution images.\n"},
    {"MASKRANGE",     "VALID",       "",  "mask-valid-ranges",            ArgChecker::IntervalSet, helpers::usageValidRanges},
    {"CLASSES",       "",            "n", "number-classes",               ArgChecker::Float,       "  -n <float>, --number-classes=<float> \tThe number of classes used for classification. This basically influences the tolerance value to decide whether pixels are similar. Default: 40.\n"},
    Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file are specified in the same way as on the command line. You can use newlines between options "
                     "and line comments with # (use \\# to get a non-comment #). The specified options in the file replace the --option-file=<file> argument before they are parsed.\n"),
    {"FORMAT",        "",            "f", "out-format",                   ArgChecker::NonEmpty,    "  -f <fmt>, --out-format=<fmt>  \tUse the specified image file format, like GTiff, as output. See also --help-formats.\n"},
    {"OUTMASKPOSTFIX","",            "",  "out-mask-postfix",             ArgChecker::Optional,    "  --out-mask-postfix=<string> \tThis will be appended to the mask output filenames. Only used if mask output is enabled.\n"},
    {"OUTMASKPREFIX", "",            "",  "out-mask-prefix",              ArgChecker::Optional,    "  --out-mask-prefix=<string> \tThis will be prepended to the output filenames. Only used if mask output is enabled. By default this is 'mask_'.\n"},
    {"OUTPOSTFIX",    "",            "",  "out-postfix",                  ArgChecker::Optional,    "  --out-postfix=<string> \tThis will be appended to the output filenames.\n"},
    {"OUTPREFIX",     "",            "",  "out-prefix",                   ArgChecker::Optional,    "  --out-prefix=<string> \tThis will be prepended to the output filenames. By default this is 'predicted_'.\n"},
    {"PREDAREA",      "",            "",  "pred-area",                    ArgChecker::Rectangle,   "  --pred-area=<rect> \tSpecifies the prediction area. The prediction will only be done in this area. <rect> requires all of the following arguments:\v"
                                                                                                                          "  -x <num>                 x start\v"
                                                                                                                          "  -y <num>                 y start\v"
                                                                                                                          "  -w <num>, --width=<num>  width\v"
                                                                                                                          "  -h <num>, --height=<num> height\v"
                                                                                                                          "Examples: --pred-area='-x 1 -y 2 -w 3 -h 4'\n"},
    {"SPECUNCERT",    "",            "s", "spectral-uncertainty",         ArgChecker::Float,       "  -s <float>, --spectral-uncertainty=<float> \tThis spectral uncertainty value will influence the spectral difference value. We suggest 1 for 8 bit images (default for 8 bit), 50 for 16 bit images (default otherwise).\n"},
    {"TEMPUNCERT",    "",            "t", "temporal-uncertainty",         ArgChecker::Float,       "  -t <float>, --temporal-uncertainty=<float> \tThis temporal uncertainty value will influence the temporal difference value. We suggest 1 for 8 bit images (default for 8 bit), 50 for 16 bit images (default otherwise).\n"},
    {"WINSIZE",       "",            "w", "win-size",                     ArgChecker::Int,         "  -w <num>, --win-size=<num> \tWindow size of the rectangle around the current pixel. Default: 51.\n"},
    Descriptor::breakTable(),
    Descriptor::text("\nExamples:\n"
                     "  \tstarfm --img='-f h1.tif -d 1 -t high' --img='-f h3.tif -d 3 -t high' --img='-f l1.tif -d 1 -t low' --img='-f l2.tif -d 2 -t low' --img='-f l3.tif -d 3 -t low'\v"
                     "will predict the high resolution image at date 2 with both pairs and output it to predicted_2_from_1_and_3.tif.\v\v"
                     "starfm --option-file=starfmOpts\v"
                     "where the file starfmOpts contains\v"
                     "  --img=(--file=h1.tif --date=1 --tag=high)\v"
                     "  --img=(--file=h3.tif --date=3 --tag=high)\v"
                     "  --img=(--file=l1.tif --date=1 --tag=low) \v"
                     "  --img=(--file=l2.tif --date=2 --tag=low) \v"
                     "  --img=(--file=l3.tif --date=3 --tag=low) \v"
                     "does the same as the first example, but is easier to handle.\v\v")
};


int main(int argc, char* argv[]) {
    // parse arguments
    std::string defaultArgs = "--out-prefix=predicted_ --out-mask-prefix=mask_ --enable-output-masks --number-classes=40 --win-size=51 --log-scale=0 --disable-strict-filtering --disable-copy-on-zero-diff --enable-temporal-weighting --enable-double-pair-mode --enable-use-nodata";
    auto options = imagefusion::option::OptionParser::parse(usage, argc, argv, defaultArgs);

    if (!options["HELP"].empty() || argc == 1) {
        printUsage(usage, /*auto line width*/ -1, /*last_column_min_percent*/ 10);
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
                plainImgArgs, /*minImages*/ 3, /*numResTags*/ 2, predArea,
                "One for high resolution one for low resolution. The tag with less images "
                "will be used as high resolution tag and the other one as low resolution tag. ");
    imagefusion::MultiResCollection<std::string>& imgArgs = args_and_gis.first;
    imagefusion::MultiResCollection<imagefusion::GeoInfo>& gis = args_and_gis.second;

    // collect the dates in a job hierarchy, like [(1) 3 4 (7)] [(7) 10 12 13 (14)] [(14) 15]
    // Order of predictions in double-pair: 3 (using 1 7), 4 (using 1 7), 10 (using 7 14), 12 (using 7 14), 13 (using 7 14), 15 (using 14),
    // Order of predictions in single-pair: 3 (using 1), 3 (using 7), 4 (using 1), 4 (using 7), 10 (using 7), 10 (using 14), 12 (using 7), 12 (using 14), 13 (using 7), 13 (using 14), 15 (using 14),
    // state double-pair mode in parseJobs to be able to handle memory more efficient in single pair mode, meaning predicting the same date twice succesively
    bool useDoublePairMode = options["TWOPAIRMODE"].back().prop() == "ENABLE";
    helpers::JobsAndTags jat = helpers::parseJobs(imgArgs, 1, /* remove single-pair dates */ false, /* single-pair mode */ false);

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

    // set STARFM options
    imagefusion::StarfmOptions starfmOpts;
    starfmOpts.setHighResTag(jat.highTag);
    starfmOpts.setLowResTag(jat.lowTag);
    starfmOpts.setWinSize(Parse::Int(options["WINSIZE"].back().arg));
    starfmOpts.setNumberClasses(Parse::Float(options["CLASSES"].back().arg));
    starfmOpts.setUseStrictFiltering(options["STRICT"].back().prop() == "ENABLE");
    starfmOpts.setDoCopyOnZeroDiff(options["COPY0DIFF"].back().prop() == "ENABLE");
    starfmOpts.setLogScaleFactor(Parse::Float(options["LOGSCALE"].back().arg));

    imagefusion::Type basetype = gis.getAny().baseType;
    unsigned int uncertainty = basetype == imagefusion::Type::uint8 || basetype == imagefusion::Type::int8 ? 1 : 50;
    starfmOpts.setSpectralUncertainty(options["SPECUNCERT"].empty() ? uncertainty : Parse::Float(options["SPECUNCERT"].back().arg));
    starfmOpts.setTemporalUncertainty(options["TEMPUNCERT"].empty() ? uncertainty : Parse::Float(options["TEMPUNCERT"].back().arg));

    imagefusion::StarfmOptions::TempDiffWeighting tempDiffSetting = imagefusion::StarfmOptions::TempDiffWeighting::on_double_pair;
    if (options["TEMPWEIGHT"].back().prop() == "ENABLE")
        tempDiffSetting = imagefusion::StarfmOptions::TempDiffWeighting::enable;
    if (options["TEMPWEIGHT"].back().prop() == "DISABLE")
        tempDiffSetting = imagefusion::StarfmOptions::TempDiffWeighting::disable;
    starfmOpts.setUseTempDiffForWeights(tempDiffSetting);

    if (predArea == imagefusion::Rectangle{})
        predArea = imagefusion::Rectangle(0, 0, gis.getAny().width(), gis.getAny().height());
    #ifdef WITH_OMP
        imagefusion::ParallelizerOptions<imagefusion::StarfmOptions> parOpts;
        parOpts.setPredictionArea(predArea);
        imagefusion::Parallelizer<imagefusion::StarfmFusor> starfm;
    #else /* WITH_OMP not defined */
        starfmOpts.setPredictionArea(predArea);
        imagefusion::StarfmFusor starfm;
    #endif /* WITH_OMP */

    auto mri = std::make_shared<imagefusion::MultiResImages>();
    starfm.srcImages(mri);

    // loop over multiple time series (different input pairs)
    bool doWriteMasks = options["MASKOUT"].back().prop() == "ENABLE";
    for (auto const& p : jat.jobs) {
        auto const& pairDate_vec = p.first;
        assert(pairDate_vec.front() <= pairDate_vec.back() && "Jobs need to be sorted.");
        assert((pairDate_vec.size() == 1 || pairDate_vec.size() == 2) && "Job hierarchy defect. Please fix!");

        std::vector<imagefusion::Image> pairMasks;
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

            // add mask from nodata value and valid / invalid ranges for pair images to base mask
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
            //We fill the mask images into our vector
            //For the first date (Or on every date in single pair mode), add a new empty mask
            if (pairMasks.empty() || !useDoublePairMode) 
                pairMasks.push_back(baseMask);
            //Update the mask by applying the ranges
            if (pairValidSets.hasHigh)
                pairMasks.back() = helpers::processSetMask(std::move(pairMasks.back()), mri->get(jat.highTag, datePair), pairValidSets.high);
            if (pairValidSets.hasLow)
                pairMasks.back() = helpers::processSetMask(std::move(pairMasks.back()), mri->get(jat.lowTag,  datePair), pairValidSets.low);
        }
        //make sure that:
        //If single pair mode, we have one mask for every date
        //Or if double pair mode, we have one combined mask that has been updated for every date
        assert(pairDate_vec.size() == pairMasks.size() || useDoublePairMode && pairMasks.size() == 1);

        // loop over a single time series (multiple images with the same date 1 and maybe date 3)
        for (int datePred : p.second) {
            // read in prediction image
            if (!mri->has(jat.lowTag, datePred)) {
                auto in = Parse::MRImage(imgArgs.get(jat.lowTag, datePred));
                mri->set(jat.lowTag, datePred, std::move(in.i));
            }

            // add mask from nodata value and valid / invalid ranges for prediction image to pair mask
            imagefusion::GeoInfo giPred = gis.get(jat.lowTag, datePred);
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

            for (unsigned int idx = 0; idx < pairMasks.size(); ++idx) {
                imagefusion::Image predMask = pairMasks.at(idx);
              //Make the pred mask
                if (predValidSets.hasLow)
                    predMask = helpers::processSetMask(std::move(predMask), mri->get(jat.lowTag, datePred), predValidSets.low);

                if (pairDate_vec.size() == 2 && useDoublePairMode)
                    starfmOpts.setDoublePairDates(pairDate_vec.front(), pairDate_vec.back());
                else
                    starfmOpts.setSinglePairDate(pairDate_vec.at(idx));
                #ifdef WITH_OMP
                    parOpts.setAlgOptions(starfmOpts);
                    starfm.processOptions(parOpts);
                #else /* WITH_OMP not defined */
                    starfm.processOptions(starfmOpts);
                #endif /* WITH_OMP */

                // predict a single image
                std::cout << "Predicting for date " << datePred;
                int date1 = pairDate_vec.front();
                int date3 = pairDate_vec.back();
                if (starfmOpts.isDoublePairModeConfigured())
                    std::cout << " using both pairs from dates " << date1 << " and " << date3 << "." << std::endl;
                else {
                    date1 = date3 = starfmOpts.getSinglePairDate();
                    std::cout << " using a single pair from date " << date1 << "." << std::endl;
                }

                starfm.predict(datePred, predMask);
                auto& out = starfm.outputImage();
                std::cout << "Prediction done. ";

                // output result and mask
                std::string filename = Parse::ImageFileName(imgArgs.get(jat.lowTag, datePred));
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
                            std::string outmaskfilename = helpers::outputImageFile(predMask, giPred, filename, maskprefix, maskpostfix, outformat, date1, datePred, date3);
                            maskOutInfo = " and its mask to " + outmaskfilename;
                        }
                    }

                    std::string outfilename = helpers::outputImageFile(out, giPred, filename, prefix, postfix, outformat, date1, datePred, date3);
                    std::cout << "Wrote predicted image to " << outfilename << maskOutInfo << "." << std::endl;
                }
                catch (imagefusion::runtime_error&) {
                    std::cout << "Could not write the output of processing " << filename << ", sorry. Going on with the next one." << std::endl;
                }
            }

            // remove prediction image
            if (mri->has(jat.lowTag, datePred))
                mri->remove(jat.lowTag, datePred);
        }

        // remove first pair images only if we have two pairs (otherwise it would be the left or right end pair)
        if (pairDate_vec.size() == 2 && mri->has(jat.highTag, pairDate_vec.front()))
            mri->remove(jat.highTag, pairDate_vec.front());
        if (pairDate_vec.size() == 2 && mri->has(jat.lowTag, pairDate_vec.front()))
            mri->remove(jat.lowTag, pairDate_vec.front());
    }

    return 0;
}

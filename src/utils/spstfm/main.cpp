#include <iostream>
#include <string>
#include <iterator>
#include <memory>

#include <filesystem>

#include "optionparser.h"
#include "geoinfo.h"
#include "image.h"
#include "multiresimages.h"
#include "spstfm.h"
#include "fileformat.h"

#include "utils_common.h"


const char* usageImage =
    "  -i <img>, --img=<img> \tInput image. At least five images are required: "
    "two pairs of high and low resolution images and one low resolution image at a date inbetween "
    "to predict the corresponding missing high resolution image. "
    "If you want to predict more images, just add more. For each low resolution image lacking a corresponding "
    "high resolution image a prediction will be made. You can also add more pairs to predict multiple time series.\v"
    "<img> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', "
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
    "  -c (<num> <num>), --center=(<num> <num>) x and y center\v"
    "  -w <num>, --width=<num>                 width\v"
    "  -h <num>, --height=<num>                height\v"
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
    Descriptor::text("Usage: spstfm -i <img> -i <img> -i <img> -i <img> -i <img> [options]\n"
                     "   or: spstfm --option-file=<file> [options]\n\n"
                     "The order of the options can be arbitrary, but at least five images are required for prediction. Just add more images to do more predictions. If multiple images with the same surrounding image pairs are predicted, the dictionary is trained only once for all of these predictions."
                     " If you provide more than two image pairs, multiple time series will be predicted. Remember to protect whitespace by quoting with '...', \"...\" or (...) or by escaping.\n"
                     "Options:"),
    {"DICTSIZE",      "",            "",  "dict-size",                    ArgChecker::Int,         "  --dict-size=<num> \tDictionary size, i. e. the number of atoms. Default: 256.\n"},
    {"REUSEDICT",     "",            "",  "dict-reuse",                   ArgChecker::NonEmpty,    "  --dict-reuse=clear|improve|use \tFor multiple time series after the first series or a dictionary loaded from file, what to do with the existing dictionary:\v"
                                                                                                                                      " * clear an existing dictionary before training\v"
                                                                                                                                      " * improve an existing dictionary (default)\v"
                                                                                                                                      " * use an existing dictionary without further training.\v"
                                                                                                                                      "In any case, if no dictionary exists, it is initialized and then trained.\n"},
    {"MASKOUT",       "DISABLE",     "",  "disable-output-masks",         ArgChecker::None,        "  --disable-output-masks       \tThis disables the output of the masks that are used for the predictions.\n"},
    {"RANDOM",        "DISABLE",     "",  "disable-random-sampling",      ArgChecker::None,        "  --disable-random-sampling \tUse the samples with the most variance for training data. Default.\n"},
    {"USENODATA",     "DISABLE",     "",  "disable-use-nodata",           ArgChecker::None,        "  --disable-use-nodata   \tThis will not use the nodata value as invalid range for masking.\n"},
    {"MASKOUT",       "ENABLE",      "",  "enable-output-masks",          ArgChecker::None,        "  --enable-output-masks        \tThis enables the output of the masks that are used for the predictions. If no mask are used, there will be put out nothing. Default.\n"},
    {"RANDOM",        "ENABLE",      "",  "enable-random-sampling",       ArgChecker::None,        "  --enable-random-sampling \tUse random samples for training data instead of the samples with the most variance.\n"},
    {"USENODATA",     "ENABLE",      "",  "enable-use-nodata",            ArgChecker::None,        "  --enable-use-nodata    \tThis will use the nodata value as invalid range for masking. Default.\n"},
    {"HELP",          "",            "h", "help",                         ArgChecker::None,        "  -h, --help  \tPrint usage and exit.\n"},
    {"HELPFORMAT",    "",            "",  "help-formats",                 ArgChecker::None,        "  --help-formats  \tPrint all available file formats that can be used with --out-format and exit.\n"},
    {"IMAGE",         "",            "i", "img",                          ArgChecker::MRImage<>,   usageImage},
    {"LOADDICT",      "",            "l", "load-dict",                    ArgChecker::NonEmpty,    "  -l <file>, --load-dict=<file> \tLoad dictionary from a file, which has been written with --save-dict before. You can give the filename you specified with --save-dict, even if there have been generated numbers in the filename (which happens in case of multi-channel images). Do not specify multiple dictionaries. Only the last will be used otherwise.\n"},
    {"MASKIMG",       "",            "m", "mask-img",                     ArgChecker::Mask,        helpers::usageMaskFile},
    {"MASKRANGE",     "HIGHINVALID", "",  "mask-high-res-invalid-ranges", ArgChecker::IntervalSet, "  --mask-high-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the high resolution images.\n"},
    {"MASKRANGE",     "HIGHVALID",   "",  "mask-high-res-valid-ranges",   ArgChecker::IntervalSet, "  --mask-high-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the high resolution images.\n"},
    {"MASKRANGE",     "INVALID",     "",  "mask-invalid-ranges",          ArgChecker::IntervalSet, helpers::usageInvalidRanges},
    {"MASKRANGE",     "LOWINVALID",  "",  "mask-low-res-invalid-ranges",  ArgChecker::IntervalSet, "  --mask-low-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the low resolution images.\n"},
    {"MASKRANGE",     "LOWVALID",    "",  "mask-low-res-valid-ranges",    ArgChecker::IntervalSet, "  --mask-low-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the low resolution images.\n"},
    {"MASKRANGE",     "VALID",       "",  "mask-valid-ranges",            ArgChecker::IntervalSet, helpers::usageValidRanges},
    {"MAXITER",       "",            "",  "max-train-iterations",         ArgChecker::Int,         "  --max-train-iterations=<num> \tMaximum number of iterations during the training. Can be 0 if the minimum is also 0. Then no training will be done, even if there is no dictionary yet (it will still be initialized). Default: 20.\n"},
    {"MINITER",       "",            "",  "min-train-iterations",         ArgChecker::Int,         "  --min-train-iterations=<num> \tMinimum number of iterations during the training. Default: 10.\n"},
    {"NSAMPLES",      "",            "",  "number-samples",               ArgChecker::Int,         "  --number-samples=<num> \tThe number of samples used for training (training data size). Default: 2000.\n"},
    Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file are specified in the same way as on the command line. You can use newlines between options "
                     "and line comments with # (use \\# to get a non-comment #). The specified options in the file replace the --option-file=<file> argument before they are parsed.\n"),
    {"FORMAT",        "",            "f", "out-format",                   ArgChecker::NonEmpty,    "  -f <fmt>, --out-format=<fmt>  \tUse the specified image file format, like GTiff, as output. See also --help-formats.\n"},
    {"OUTMASKPOSTFIX","",            "",  "out-mask-postfix",             ArgChecker::Optional,    "  --out-mask-postfix=<string> \tThis will be appended to the mask output filenames. Only used if mask output is enabled.\n"},
    {"OUTMASKPREFIX", "",            "",  "out-mask-prefix",              ArgChecker::Optional,    "  --out-mask-prefix=<string> \tThis will be prepended to the output filenames. Only used if mask output is enabled. By default this is 'mask_'.\n"},
    {"OUTPOSTFIX",    "",            "",  "out-postfix",                  ArgChecker::Optional,    "  --out-postfix=<string> \tThis will be appended to the output filenames.\n"},
    {"OUTPREFIX",     "",            "",  "out-prefix",                   ArgChecker::Optional,    "  --out-prefix=<string> \tThis will be prepended to the output filenames. By default this is 'predicted_'.\n"},
    {"POVER",         "",            "",  "patch-overlap",                ArgChecker::Int,         "  --patch-overlap=<num> \tOverlap on each side of a patch in pixel. Default: 2.\n"},
    {"PSIZE",         "",            "",  "patch-size",                   ArgChecker::Int,         "  --patch-size=<num> \tSize of a patch in pixel. Default: 7.\n"},
    {"PREDAREA",      "",            "",  "pred-area",                    ArgChecker::Rectangle,   "  --pred-area=<rect> \tSpecifies the prediction area. The prediction will only be done in this area. <rect> requires all of the following arguments:\v"
                                                                                                                          "  -x <num>                 x start\v"
                                                                                                                          "  -y <num>                 y start\v"
                                                                                                                          "  -w <num>, --width=<num>  width\v"
                                                                                                                          "  -h <num>, --height=<num> height\v"
                                                                                                                          "Examples: --pred-area='-x 1 -y 2 -w 3 -h 4'\n"},
    {"SAVEDICT",      "",            "s", "save-dict",                    ArgChecker::NonEmpty,    "  -s <outfile>, --save-dict=<outfile> \tSave the dictionary after the last training to a file. This can be used later on with --load-dict=outfile and, if you do not want to improve it, --dict-reuse=use.\n"},
    Descriptor::breakTable(),
    Descriptor::text("\nExamples:\n"
                     "  \tspstfm --img='-f h1.tif -d 1 -t high' --img='-f h3.tif -d 3 -t high' --img='-f l1.tif -d 1 -t low' --img='-f l2.tif -d 2 -t low' --img='-f l3.tif -d 3 -t low'\v"
                     "will predict the high resolution image at date 2 and output it to predicted_2.tif.\v\v"
                     "spstfm --option-file=spstfmOpts\v"
                     "where the file spstfmOpts contains\v"
                     "  --img=(--file=h1.tif --date=1 --tag=high)\v"
                     "  --img=(--file=h3.tif --date=3 --tag=high)\v"
                     "  --img=(--file=l1.tif --date=1 --tag=low) \v"
                     "  --img=(--file=l2.tif --date=2 --tag=low) \v"
                     "  --img=(--file=l3.tif --date=3 --tag=low) \v"
                     "does the same as the first example, but is easier to handle.\v\v")
};


int main(int argc, char* argv[]) {
    // parse arguments
    std::string defaultArgs = "--out-prefix=predicted_ --out-mask-prefix=mask_ --enable-output-masks --dict-reuse=improve --disable-random-sampling --dict-size=256 --number-samples=2000 --patch-size=7 --patch-overlap=2 --min-train-iterations=10 --max-train-iterations=20 --enable-use-nodata";
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

    // check spelling of reuse dictionary option
    std::string reuseOpt = options["REUSEDICT"].back().arg;
    if (reuseOpt != "improve" && reuseOpt != "clear" && reuseOpt != "use") {
        std::cerr << "For --dict-reuse you must either give 'improve', 'clear' or 'use'. You gave " << reuseOpt << "." << std::endl;
        return 5;
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

    // set SPSTFM options
    imagefusion::SpstfmOptions spstfmOpts;
    spstfmOpts.setHighResTag(jat.highTag);
    spstfmOpts.setLowResTag(jat.lowTag);
    spstfmOpts.setDictSize(Parse::Int(options["DICTSIZE"].back().arg));
    spstfmOpts.setNumberTrainingSamples(Parse::Int(options["NSAMPLES"].back().arg));
    spstfmOpts.setPatchSize(Parse::Int(options["PSIZE"].back().arg));
    spstfmOpts.setPatchOverlap(Parse::Int(options["POVER"].back().arg));
    spstfmOpts.setMinTrainIter(Parse::Int(options["MINITER"].back().arg));
    spstfmOpts.setMaxTrainIter(Parse::Int(options["MAXITER"].back().arg));
    spstfmOpts.setSamplingStrategy(options["RANDOM"].back().prop() == "ENABLE" ? imagefusion::SpstfmOptions::SamplingStrategy::random : imagefusion::SpstfmOptions::SamplingStrategy::variance);

    if (predArea == imagefusion::Rectangle{})
        predArea = imagefusion::Rectangle(0, 0, gis.getAny().width(), gis.getAny().height());
    spstfmOpts.setPredictionArea(predArea);

    imagefusion::SpstfmFusor spstfm;
    auto mri = std::make_shared<imagefusion::MultiResImages>();
    spstfm.srcImages(mri);

    // load dictionary from file
    if (!options["LOADDICT"].empty()) {
        std::string dictPath = options["LOADDICT"].back().arg;
        unsigned int chans = mri->getAny().channels();
        if (chans == 1) {
            if (!std::filesystem::exists(dictPath)) {
                std::cerr << "Could not find the dictionary file " << dictPath << " to load a single-channel dictionary." << std::endl;
                return 6;
            }

            arma::mat dict;
            bool loadOK = dict.load(dictPath);
            if (loadOK) {
                std::cout << "Using dictionary from " << dictPath << "." << std::endl;
                spstfm.setDictionary(dict);
            }
            else
                std::cerr << "Could not load dictionary from " << dictPath << " although the file exists. Defect file? Ignoring option --load-dict." << std::endl;
        }
        else {
            std::filesystem::path p = dictPath;
            std::string extension = p.extension().string();
            std::string basename  = p.stem().string();
            if (std::filesystem::exists(dictPath))
                basename.pop_back(); // drop channel number

            for (unsigned int c = 0; c < chans; ++c) {
                p = basename + std::to_string(c) + extension;
                auto outpath = p;

                std::string infilename = outpath.string();
                if (!std::filesystem::exists(infilename)) {
                    std::cerr << "Could not find the dictionary file " << infilename << " to load a part of a multi-channel dictionary. "
                                 "Give either the same filename as specified with --save-dict or one of the actual files with channel number." << std::endl;
                    return 6;
                }

                arma::mat dict;
                bool loadOK = dict.load(infilename);
                if (loadOK) {
                    std::cout << "Using dictionary from " << infilename << " for channel " << c << "." << std::endl;
                    spstfm.setDictionary(dict, c);
                }
                else {
                    std::cerr << "Could not load dictionary from " << infilename << " although the file exists. Defect file? Ignoring option --load-dict completely." << std::endl;
                    // remove eventual dictionaries set to previous channels
                    for (unsigned int i = 0; i < chans; ++i)
                        spstfm.setDictionary(dict, i);
                    break;
                }
            }
        }
    }

    // loop over multiple time series (multiple input pairs)
    bool doWriteMasks = options["MASKOUT"].back().prop() == "ENABLE";
    for (auto const& p : jat.jobs) {
        auto const& pairDate_vec = p.first;
        assert(pairDate_vec.size() == 2 && "Found just a single pair, which is not supported here.");
        int date1 = pairDate_vec.front();
        int date3 = pairDate_vec.back();
        assert(date1 < date3 && "Jobs need to be sorted.");


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

            if (pairValidSets.hasHigh)
                pairMask = helpers::processSetMask(pairMask, mri->get(jat.highTag, datePair), pairValidSets.high);
            if (pairValidSets.hasLow)
                pairMask = helpers::processSetMask(pairMask, mri->get(jat.lowTag,  datePair), pairValidSets.low);
        }

        // train dictionary (if there is one from a previous time series, improve it)
        std::cout << "Training with dates " << date1 << " and " << date3 << std::endl;
        spstfmOpts.setDate1(date1);
        spstfmOpts.setDate3(date3);

        if (reuseOpt == "improve")
            spstfmOpts.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::improve);
        else if (reuseOpt == "clear")
            spstfmOpts.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::clear);
        else // (reuseOpt == "use")
            spstfmOpts.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::use);

        spstfm.processOptions(spstfmOpts);
        spstfm.train(pairMask);

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
                predMask = helpers::processSetMask(predMask, mri->get(jat.lowTag, date2), predValidSets.low);

            // predict a single image with the trained dictionary
            std::cout << "Predicting for date " << date2 << std::endl;
            spstfmOpts.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::use);
            spstfm.processOptions(spstfmOpts);
            spstfm.predict(date2, predMask);
            auto& out = spstfm.outputImage();
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

    // save dictionary to file
    if (!options["SAVEDICT"].empty()) {
        bool success = true;
        std::string dictPath = options["SAVEDICT"].back().arg;
        unsigned int chans = mri->getAny().channels();
        if (chans == 1) {
            success = spstfm.getDictionary().save(dictPath);
            if (success)
                std::cout << "Saved dictionary to " << dictPath << "." << std::endl;
            else
                std::cerr << "Could not save dictionary to " << dictPath << "." << std::endl;
        }
        else {
            std::filesystem::path p = dictPath;
            std::string extension = p.extension().string();
            std::string basename  = p.stem().string();
            for (unsigned int c = 0; c < chans; ++c) {
                p = basename + std::to_string(c) + extension;
                auto outpath = p;

                std::string outfilename = outpath.string();
                bool saved = spstfm.getDictionary(c).save(outfilename);
                if (saved)
                    std::cout << "Saved dictionary for channel " << c << " to " << outfilename << "." << std::endl;
                success &= saved;
            }

            if (success)
                std::cout << "For loading the dictionaries later on, you can still use --load-dict=" << dictPath << "." << std::endl;
        }
    }

    return 0;
}

#include <iostream>
#include <string>
#include <fstream>

#include <boost/filesystem.hpp>

#include "imgcmp.h"
#include "optionparser.h"
#include "GeoInfo.h"

#include "utils_common.h"

const char* usageImage =
    "  -i <img>, --img=<img> \tImage to compare. One or two images can be specified. The -i or --img can be omitted.\n"
    "\t<img> can be a file path. If cropping or using only a subset of channels / layers "
    "is desired, <img> must have the form '-f <file> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', "
    "where the arguments can have an arbitrary order. "
    "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"
    "\t  -f <file>, --file=<file> \tSpecifies the image file path. GDAL subdataset paths are also valid, but have to be quoted.\n"
    "\t  -l <num-list>,  --layers=<num-list> \tOptional. Specifies the bands or subdatasets, that will be read. Hereby a 0 means the first band/subdataset.\v"
    "<num-list> must have the format '(<num> [[,]<num> ...])' or just '<num>'.\n"
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
    "Examples: --img=some_image.tif\v"
//    "          --img='-f \"test image.tif\" --crop=(-x 1 -y 2 -w 3 -h 2) -l (0 2)'"
    "          --img='-f \"test image.tif\" -l 0'\v"
    "          --img='-f test.tif --crop=(-x=(1 3) -y=(2 5))'\v"
    "          --img='-f (HDF4_EOS:EOS_GRID:\"path/MOD09GA.hdf\":MODIS_Grid_500m_2D:sur_refl_b01_1)'\n";

const char* usageHistRange =
    "  --hist-range=auto|<interval> \tSets the range of the usual histograms (for difference histogram, see --hist-diff-range). With auto the minimum and maximum are used (default).\v"
    "<interval> can have the form '[<num>,<num>]' where the comma can be replaced by a space.\n";

const char* usageHistDiffRange =
    "  --hist-diff-range=auto|<interval> \tSets the range of the difference histogram (--out-hist-diff). With auto the minimum and maximum differences are used (default).\v"
    "<interval> can have the form '[<num>,<num>]' where the comma can be replaced by a space.\n";

using Descriptor = imagefusion::option::Descriptor;
using ArgChecker = imagefusion::option::ArgChecker;
using Parse      = imagefusion::option::Parse;
std::vector<Descriptor> usage{
    Descriptor::text("Usage:\n"
                     "Comparison mode:\n"
                     "       \timgcompare -i <img> -i <img> [options]\n"
                     "Single image mode:\n"
                     "       \timgcompare -i <img> [--help] [--hist-bins=<num>] [--hist-log] [--hist-size=<size>] [--mask-img=<img>] [--mask-invalid-ranges=<range-list>] [--mask-valid-ranges=<range-list>] [--option-file=<file>] [--out-mask=<file>] [--out-hist-first=<file>]\n\n"
                     "The order of most options is arbitrary, but exactly two images are required for comparison and one image is required if only a mask or histogram of it should be created."
                     " Remember to protect whitespace by quoting with '...', \"...\" or (...) or by escaping. On bash parentheses can only be used within a quotation.\n"
                     "Options:"),
    Descriptor::breakTable(),
    {"AT",            "",        "",  "at",                  ArgChecker::Interval,    "  --at=<xy-coords> \tPrints all values at the specified coordinates. <xy-coords> are just two numbers, separated by comma. Examples: --at=5,7 --at='5, 7'\n"},
    {"GRIDS",         "DISABLE", "",  "disable-grids",       ArgChecker::None,        "  --disable-grids  \tDisable grid in scatter and histogram plot. The order of --disable-grids and --enable-grids is important insofar as only the last one is used. Disabled by default.\n"},
    {"LEGENDS",       "DISABLE", "",  "disable-legends",     ArgChecker::None,        "  --disable-legends  \tDisable legend in scatter and histogram plot. The order of --disable-legends and --enable-legends is important insofar as only the last one is used. Disabled by default.\n"},
    {"USENODATA",     "DISABLE", "",  "disable-use-nodata",  ArgChecker::None,        "  --disable-use-nodata \tThis will not use the nodata value as invalid range for masking.\n"},
    {"GRIDS",         "ENABLE",  "g", "enable-grids",        ArgChecker::None,        "  -g, --enable-grids  \tEnable grid in scatter and histogram plot. The order of --disable-grids and --enable-grids is important insofar as only the last one is used. Disabled by default.\n"},
    {"LEGENDS",       "ENABLE",  "l", "enable-legends",      ArgChecker::None,        "  -l, --enable-legends  \tEnable legend in scatter and histogram plot. The order of --disable-legends and --enable-legends is important insofar as only the last one is used. Disabled by default.\n"},
    {"USENODATA",     "ENABLE",  "",  "enable-use-nodata",   ArgChecker::None,        "  --enable-use-nodata \tThis will use the nodata value as invalid range for masking. Default.\n"},
    {"HELP",          "",        "h", "help",                ArgChecker::None,        "  -h, --help  \tPrint usage and exit.\n"},
    {"HISTBINS",      "",        "",  "hist-bins",           ArgChecker::Int,         "  --hist-bins=<num>  \tSet number of bins in the histograms. This is only used if you make a histogram plot. By default: 32.\n"},
    {"HISTLOG",       "",        "",  "hist-log",            ArgChecker::None,        "  --hist-log  \tPlot the histograms in logarithmic scale instead of linear scale.\n"},
    {"HISTDIFFRANGE", "",        "",  "hist-diff-range",     ArgChecker::NonEmpty,    usageHistDiffRange},
    {"HISTRANGE",     "",        "",  "hist-range",          ArgChecker::NonEmpty,    usageHistRange},
    {"HISTSIZE",      "",        "",  "hist-size",           ArgChecker::Size,        "  --hist-size=<size>  \tHistogram plot size in pixel (without axis etc.). Provide the size in the format '<width>x<height>' or '<width>, <height>' (comma optional). By default: 1025 x 500\n"},
    {"IMAGE",         "",        "i", "img",                 ArgChecker::Image,       usageImage},
    {"MASKIMG",       "",        "m", "mask-img",            ArgChecker::Mask,        helpers::usageMaskFile},
    {"MASKRANGE",     "INVALID", "",  "mask-invalid-ranges", ArgChecker::IntervalSet, helpers::usageInvalidRanges},
    {"MASKRANGE",     "VALID",   "",  "mask-valid-ranges",   ArgChecker::IntervalSet, helpers::usageValidRanges},
    Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file "
                     "are specified in the same way as on the command line. You can use newlines "
                     "between options and line comments with # (use \\# to get a non-comment #). "
                     "The specified options in the file replace the --option-file=<file> argument "
                     "before they are parsed.\n"),
    {"OUTMASK",       "",        "",  "out-mask",            ArgChecker::NonEmpty,    "  --out-mask=<file>  \tFile path to image where the used mask image should be written to. Will only output a mask if one has been specified, by --mask-img, --mask-valid-range or --mask-invalid-range option.\n"},
    {"OUTDIFF",       "",        "",  "out-diff",            ArgChecker::NonEmpty,    "  --out-diff=<file>  \tFile path to image where the absolute difference image should be written to.\n"},
    {"OUTDIFFBIN",    "",        "",  "out-diff-bin",        ArgChecker::NonEmpty,    "  --out-diff-bin=<file>  \tFile path to image where the binary absolute difference image should be written to. This will be a uint8 image with only 0 for no difference or 255 for difference.\n"},
    {"OUTDIFFSCALED", "",        "",  "out-diff-scaled",     ArgChecker::NonEmpty,    "  --out-diff-scaled=<file>  \tFile path to image where the scaled absolute difference image should be written to. The scaled difference image has maximum contrast for visualization.\n"},
    {"OUTHISTBOTH",   "",        "",  "out-hist-both",       ArgChecker::NonEmpty,    "  --out-hist-both=<file>  \tFile path to image or csv file (.csv or .txt) where the combined histogram of both images should be written to.\n"},
    {"OUTHISTDIFF",   "",        "",  "out-hist-diff",       ArgChecker::NonEmpty,    "  --out-hist-diff=<file>  \tFile path to image or csv file (.csv or .txt) where the histogram of the absolute difference image should be written to.\n"},
    {"OUTHIST1",      "",        "",  "out-hist-first",      ArgChecker::NonEmpty,    "  --out-hist-first=<file>  \tFile path to image or csv file (.csv or .txt) where the histogram of the first image should be written to.\n"},
    {"OUTHIST2",      "",        "",  "out-hist-second",     ArgChecker::NonEmpty,    "  --out-hist-second=<file>  \tFile path to image or csv file (.csv or .txt) where the histogram of the second image should be written to.\n"},
    {"OUTSCATTER",    "",        "",  "out-scatter",         ArgChecker::NonEmpty,    "  --out-scatter=<file>  \tFile path to image where the scatter plot should be written to. This is uint8 with only 0 or 255. The horizontal axis is the first image, the vertical axis the second.\n"},
    {"SCATTERSIZE",   "",        "",  "scatter-size",        ArgChecker::Int,         "  --scatter-size=<num>  \tScatter plot size in pixel (without axis etc.). Provide just one number, since the plot is always quadratic. Using a negative number z will result in |z| as maximum size and only use one pixel per value if the span of values is smaller. By default: -600.\n"},
    Descriptor::breakTable(),
    Descriptor::text("\nExamples:\n"
                     "imgcompare -i 'file 1.tif'  -i file2.tif  --out-diff=diff.tif  --out-scatter=scatter.tif\n"
                     "  \twill compare the specified images and output a plain absdiff image and a scatter plot.\n\n"
                     "imgcompare 'file 1.tif'  file2.tif  --out-diff-scaled=diff.tif\n"
                     "  \twill compare the same images as in the first example and output a scaled absdiff image.\n\n"
                     "imgcompare --img='-f \"L 2.tif\" -l 1'  --img='-f fused.tif -l 1'  --out-hist-both=hist_comp.tif --out-hist-diff=hist_diff.tif\n"
                     "  \twill compare channel 1 (second channel) of the specified images and output two histogram plots.\n\n"
                     "imgcompare 1.tif  2.tif  -m base_mask.tif  --mask-valid-ranges=[1000,10000]  --mask-invalid-ranges='(5000,6000)'  --out-mask=full_mask.tif\n"
                     "  \twill compare the specified images but only at points that are specified in the mask file and are in the set [1000,5000] U [6000,10000]."
                     " So the valid pixels are restricted by 'base_mask.tif' and furthermore by the specified ranges. The mask is also put out.\n\n"
                     "imgcompare 1.tif  --mask-valid-ranges='(-inf,0)'  --out-mask=negmask.tif --out-hist-first=neghist.csv\n"
                     "  \twill use a single image to make a mask of all negative values and output a histogram of these in csv format.")
};



int main(int argc, char* argv[]) {
    // parse arguments with accepting options after non-option arguments, like ./imgcompare file1.tif file2.tif --out-diff=diff.tif
    std::string defaults = "--disable-grids --disable-legends --hist-bins=32 --hist-size=1025x500 --scatter-size=-600 --hist-range=auto --hist-diff-range=auto --enable-use-nodata";
    imagefusion::option::OptionParser options(usage);
    options.acceptsOptAfterNonOpts = true;
    options.parse(defaults).parse(argc, argv);

    if (!options["HELP"].empty() || argc == 1) {
        printUsage(usage, /*auto line width*/ -1, /*last_column_min_percent*/ 10);
        return 0;
    }

    std::vector<std::string>& imgargs = options.nonOptionArgs;
    for (auto const& o : options["IMAGE"])
        imgargs.push_back(o.arg);

    if (imgargs.size() != 1 && imgargs.size() != 2) {
        std::cerr << "Please specify 1 or 2 images.";
        if (imgargs.size() > 0) {
            std::cerr << " You specified " << imgargs.size() << " images:" << std::endl;
            for (auto const& arg : imgargs)
                std::cerr << Parse::ImageFileName(arg) << std::endl;
        }
        return 1;
    }

    bool singleImgMode = imgargs.size() == 1;
    if (singleImgMode && (!options["OUTDIFF"].empty() || !options["OUTDIFFBIN"].empty() || !options["OUTDIFFSCALED"].empty()
                       || !options["OUTHIST2"].empty() || !options["OUTHISTBOTH"].empty() || !options["OUTHISTDIFF"].empty()
                       || !options["OUTSCATTER"].empty()))
    {
        std::cerr << "With one specified image you can only create a mask and a histogram of it. See 'Single image mode' at the output of --help!" << std::endl;
        return 1;
    }

    // read the image or both images
    std::array<imagefusion::Image,2> i;
    std::array<imagefusion::GeoInfo,2> gi;
    i[0] = Parse::Image(imgargs[0]);
    gi[0] = helpers::parseGeoInfo<Parse>(imgargs[0]);

    if (!singleImgMode) {
        i[1] = Parse::Image(imgargs[1]);
        gi[1] = helpers::parseGeoInfo<Parse>(imgargs[1]);

        if (i[0].channels() != i[1].channels()) {
            std::cerr << "The images have a different number of channels: "
                      << i[0].channels() << " and " << i[1].channels() << "."
                      << " Please use the --layers argument inside --img to specify the channel(s) to compare." << std::endl;
            return 2;
        }

        if (i[0].type() != i[1].type()) {
            std::cerr << "The images differ in their datatype: "
                      << to_string(i[0].type()) << " and " << to_string(i[1].type())
                      << ". Cannot compare them." << std::endl;
            return 3;
        }

        if (i[0].size() != i[1].size()) {
            std::cerr << "The images have different sizes. Cannot compare." << std::endl;
            return 4;
        }
    }

    // collect and merge masks with AND
    std::vector<std::string> maskImgArgs;
    for (auto const& opt : options["MASKIMG"])
        maskImgArgs.push_back(opt.arg);
    imagefusion::Image mask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, i[0].channels(), !options["MASKRANGE"].empty());

    // combine mask for valid / invalid ranges
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

    bool useNodataValue = options["USENODATA"].back().prop() == "ENABLE";
    std::array<imagefusion::IntervalSet, 2> validSets = {baseValidSet, baseValidSet};
    if (useNodataValue) {
        for (int i : {0, 1}) {
            if (!hasValidRanges)
                validSets[i] += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

            if (gi[i].hasNodataValue()) {
                imagefusion::Interval nodataInt = imagefusion::Interval::closed(gi[i].getNodataValue(), gi[i].getNodataValue());
                validSets[i] -= nodataInt;
            }
        }
    }

    if (hasValidRanges || useNodataValue) {
        mask = helpers::processSetMask(std::move(mask), i[0], validSets[0]);
        if (!singleImgMode)
            mask = helpers::processSetMask(std::move(mask), i[1], validSets[1]);
    }

    // get valid range max for stats normalization
    double normMax = imagefusion::getImageRangeMax(i[0].type());
    if (!baseValidSet.empty()) {
        imagefusion::Interval const& last  = *(--baseValidSet.end());
        if (last.upper() > 0) // prevent division by zero
            normMax = last.upper();
    }

    // select best geo info, prefer one with geotransform, then gcps, then anything
    imagefusion::GeoInfo gi_good;
    if (gi[0].hasGeotransform() || singleImgMode)
        gi_good = gi[0];
    else if (gi[1].hasGeotransform())
        gi_good = gi[1];
    else if (gi[0].hasGCPs())
        gi_good = gi[0];
    else if (gi[1].hasGCPs())
        gi_good = gi[1];
    else if (gi[0].hasNodataValue() || !gi[0].metadata.empty())
        gi_good = gi[0];
    else if (gi[1].hasNodataValue() || !gi[1].metadata.empty())
        gi_good = gi[1];
    gi_good.nodataValues.clear();
    gi_good.colorTable.clear();

    // write out mask
    if (!mask.empty() && !options["OUTMASK"].empty()) {
        std::string& fname = options["OUTMASK"].back().arg;
        mask.write(fname, gi_good);
    }

    bool withLegend = options["LEGENDS"].back().prop() == "ENABLE";
    bool withGrid   = options["GRIDS"].back().prop()   == "ENABLE";
    if (!options["OUTHISTBOTH"].empty() || !options["OUTHIST1"].empty() || !options["OUTHIST2"].empty()) {
        int nbins = Parse::Int(options["HISTBINS"].back().arg);
        imagefusion::Size plotSize = Parse::Size(options["HISTSIZE"].back().arg);
        bool logarithmic = !options["HISTLOG"].empty();
        imagefusion::Interval range = findRange(options["HISTRANGE"].back().arg, "--hist-range", i[0], i[1], mask);

        // make one single-channel histogram for each channel
        std::vector<imagefusion::Image> i0_layers = i[0].split();
        std::vector<imagefusion::Image> i1_layers;
        std::vector<imagefusion::Image> mask_layers;
        if (!singleImgMode)
            i1_layers = i[1].split();
        if (mask.channels() > 1)
            mask_layers = mask.split();
        for (unsigned int c = 0; c < i0_layers.size(); ++c) {
            std::string filename_chan = i0_layers.size() > 1 ? "_" + std::to_string(c) : "";
            imagefusion::ConstImage const& i0 = i0_layers[c];
            imagefusion::ConstImage const& i1 = singleImgMode ? i[1] : i1_layers[c];
            imagefusion::ConstImage const& m  = mask.channels() > 1 ? mask_layers[c] : mask;

            // get histograms
            std::pair<std::vector<double>, std::vector<unsigned int>> bins_hist1;
            std::pair<std::vector<double>, std::vector<unsigned int>> bins_hist2;

            if (!options["OUTHISTBOTH"].empty() || !options["OUTHIST1"].empty())
                bins_hist1 = computeHist(i0, nbins, range, m);
            if (!options["OUTHISTBOTH"].empty() || !options["OUTHIST2"].empty())
                bins_hist2 = computeHist(i1, nbins, range, m);
            std::vector<double> const& bins = bins_hist1.first;
            std::vector<unsigned int> const& hist1 = bins_hist1.second;
            std::vector<unsigned int> const& hist2 = bins_hist2.second;

            // plot and output
            if (!options["OUTHISTBOTH"].empty()) {
                std::string filename = options["OUTHISTBOTH"].back().arg;
                auto base_ext = splitToFileBaseAndExtension(filename);
                std::string outfilename = base_ext.first + filename_chan + base_ext.second;

                if (base_ext.second == ".csv" || base_ext.second == ".txt") {
                    std::ofstream out(outfilename);
                    out << std::setw(10) << "center_val" << ", "
                        << std::setw(10) << "count1" << ", "
                        << std::setw(10) << "count2" << std::endl;
                    for (unsigned int i = 0; i < hist1.size() && i < hist2.size() && i < bins.size(); ++i)
                        out << std::setw(10) << bins[i] << ", "
                            << std::setw(10) << hist1[i] << ", "
                            << std::setw(10) << hist2[i] << std::endl;
                }
                else {
                    imagefusion::Image histPlot = plotHist(hist1, hist2, bins, range, i0.basetype(), plotSize, withLegend, logarithmic, withGrid, /*gray*/ false, imgargs[0], imgargs[1]);
                    histPlot.write(outfilename);
                }
            }

            if (!options["OUTHIST1"].empty()) {
                std::string filename = options["OUTHIST1"].back().arg;
                auto base_ext = splitToFileBaseAndExtension(filename);
                std::string outfilename = base_ext.first + filename_chan + base_ext.second;

                if (base_ext.second == ".csv" || base_ext.second == ".txt") {
                    std::ofstream out(outfilename);
                    out << std::setw(10) << "center_val" << ", "
                        << std::setw(10) << "count" << std::endl;
                    for (unsigned int i = 0; i < hist1.size() && i < bins.size(); ++i)
                        out << std::setw(10) << bins[i] << ", "
                            << std::setw(10) << hist1[i] << std::endl;
                }
                else {
                    std::vector<unsigned int> empty(nbins);
                    imagefusion::Image histPlot = plotHist(hist1, empty, bins, range, i0.basetype(), plotSize, withLegend, logarithmic, withGrid, /*gray*/ false, imgargs[0], "");
                    histPlot.write(outfilename);
                }
            }

            if (!options["OUTHIST2"].empty()) {
                std::string filename = options["OUTHIST2"].back().arg;
                auto base_ext = splitToFileBaseAndExtension(filename);
                std::string outfilename = base_ext.first + filename_chan + base_ext.second;

                if (base_ext.second == ".csv" || base_ext.second == ".txt") {
                    std::ofstream out(outfilename);
                    out << std::setw(10) << "center_val" << ", "
                        << std::setw(10) << "count" << std::endl;
                    for (unsigned int i = 0; i < hist2.size() && i < bins.size(); ++i)
                        out << std::setw(10) << bins[i] << ", "
                            << std::setw(10) << hist2[i] << std::endl;
                }
                else {
                    std::vector<unsigned int> empty(nbins);
                    imagefusion::Image histPlot = plotHist(empty, hist2, bins, range, i1.basetype(), plotSize, withLegend, logarithmic, withGrid, /*gray*/ false, "", imgargs[1]);
                    histPlot.write(outfilename);
                }
            }
        }
    }

    if (singleImgMode) {
        // print stats
        std::vector<Stats> allStats = computeStats(i[0], mask);
        for (unsigned int c = 0; c < allStats.size(); ++c) {
            Stats const& stats = allStats.at(c);
            if (allStats.size() > 1)
                std::cout << "Statistics for channel " << c << ":" << std::endl;

            std::cout << "Valid pixels:      " << i[0].width() << " x " << i[0].height() << " - " << (i[0].size().area() - stats.validPixels) << " = " << stats.validPixels << std::endl
                      << "Valid value range: " << validSets[0] << (options["MASKIMG"].empty() ? " (and no mask images)" : " (further restricted by mask images)") << std::endl;
            if (stats.validPixels > 0)
                std::cout << "Min:               " << stats.min    << " / " << normMax << " = " << (stats.min    / normMax)
                                                   << " occurs " << (stats.minCount == 1 ? "just" : std::to_string(stats.minCount) + " times, first") << " at (" << stats.minLoc.x << ", " << stats.minLoc.y << ")" << std::endl
                          << "Max:               " << stats.max    << " / " << normMax << " = " << (stats.max    / normMax)
                                                   << " occurs " << (stats.maxCount == 1 ? "just" : std::to_string(stats.maxCount) + " times, first") << " at (" << stats.maxLoc.x << ", " << stats.maxLoc.y << ")" << std::endl
                          << "Mean:              " << stats.mean   << " / " << normMax
                                                   << " = " << (stats.mean / normMax) << std::endl
                          << "Std. dev.:         " << stats.stddev << " / " << normMax << " = " << (stats.stddev / normMax) << std::endl << std::endl;
        }

        // print pixels
        for (auto const& opt : options["AT"]) {
            imagefusion::Interval coordInt = Parse::Interval(opt.arg);
            int x = std::round(coordInt.lower());
            int y = std::round(coordInt.upper());
            if (x >= i[0].width() || y >= i[0].height()) {
                std::cerr << "Cannot print pixel values at (" << x << ", " << y << "), since it is out of bounds. "
                          << "The image size is: " << i[0].size() << ". Ignoring this point." << std::endl;
                continue;
            }

            printPixel(i[0], x, y);
        }
        return 0;
    }

    // calculate absdiff and set masked out values to 0
    assert(imgargs.size() == 2);
    imagefusion::Image diff = i[0].absdiff(i[1]);
    if (!mask.empty())
        diff.set(0, mask.bitwise_not());


    if (!options["OUTDIFF"].empty()) {
        std::string& fname = options["OUTDIFF"].back().arg;
        try {
            diff.write(fname, gi_good);
        }
        catch (imagefusion::runtime_error&) {
        }
    }

    if (!options["OUTDIFFBIN"].empty()) {
        imagefusion::Image diffbin = diff.createMultiChannelMaskFromRange({imagefusion::Interval::closed(1, std::numeric_limits<double>::infinity())});
        std::string& fname = options["OUTDIFFBIN"].back().arg;
        diffbin.write(fname, gi_good);
    }

    if (!options["OUTDIFFSCALED"].empty()) {
        imagefusion::Image diffscaled{diff.size(), diff.type()};
        cv::normalize(diff.cvMat(), diffscaled.cvMat(), 0, 255, cv::NORM_MINMAX, CV_8U);
        std::string& fname = options["OUTDIFFSCALED"].back().arg;
        diffscaled.write(fname, gi_good);
    }

    if (!options["OUTSCATTER"].empty()) {
        std::string fn1 = Parse::ImageFileName(imgargs[0]);
        std::string fn2 = Parse::ImageFileName(imgargs[1]);
        int size = Parse::Int(options["SCATTERSIZE"].back().arg);
        imagefusion::Interval range = findRange("auto", "", i[0], i[1], mask);

        // make one single-channel scatter plot for each channel
        std::vector<imagefusion::Image> i0_layers = i[0].split();
        std::vector<imagefusion::Image> i1_layers = i[1].split();
        std::vector<imagefusion::Image> mask_layers;
        if (mask.channels() > 1)
            mask_layers = mask.split();
        for (unsigned int c = 0; c < i0_layers.size(); ++c) {
            std::string filename_chan = i0_layers.size() > 1 ? "_" + std::to_string(c) : "";
            imagefusion::ConstImage const& i0 = i0_layers[c];
            imagefusion::ConstImage const& i1 = i1_layers[c];
            imagefusion::ConstImage const& m = mask.channels() > 1 ? mask_layers[c] : mask;

            try {
                imagefusion::Image scatter = plotScatter(i0, i1, m, range, size, /*drawFrame*/ true, withGrid, withLegend, fn1, fn2);

                std::string filename = options["OUTSCATTER"].back().arg;
                auto base_ext = splitToFileBaseAndExtension(filename);
                std::string outfilename = base_ext.first + filename_chan + base_ext.second;
                scatter.write(outfilename);
            }
            catch(...) {
            }
        }
    }

    if (!options["OUTHISTDIFF"].empty()) {
        int nbins = Parse::Int(options["HISTBINS"].back().arg);
        imagefusion::Size plotSize = Parse::Size(options["HISTSIZE"].back().arg);
        bool logarithmic = !options["HISTLOG"].empty();
        imagefusion::Interval range = findRange(options["HISTDIFFRANGE"].back().arg, "--hist-diff-range", diff, {}, mask);

        // make one single-channel histogram for each channel
        std::vector<imagefusion::Image> diff_layers = diff.split();
        std::vector<imagefusion::Image> mask_layers;
        if (mask.channels() > 1)
            mask_layers = mask.split();
        for (unsigned int c = 0; c < diff_layers.size(); ++c) {
            std::string filename_chan = diff_layers.size() > 1 ? "_" + std::to_string(c) : "";
            imagefusion::ConstImage const& d = diff_layers[c];
            imagefusion::ConstImage const& m = mask.channels() > 1 ? mask_layers[c] : mask;

            auto bins_hist = computeHist(d, nbins, range, m);
            std::vector<double> const& bins = bins_hist.first;
            std::vector<unsigned int> const& hist = bins_hist.second;

            std::string filename = options["OUTHISTDIFF"].back().arg;
            auto base_ext = splitToFileBaseAndExtension(filename);
            std::string outfilename = base_ext.first + filename_chan + base_ext.second;

            if (base_ext.second == ".csv" || base_ext.second == ".txt") {
                std::ofstream out(outfilename);
                out << std::setw(10) << "center_val,"
                    << std::setw(10) << "count" << std::endl;
                for (unsigned int i = 0; i < hist.size(); ++i)
                    out << std::setw(10) << bins[i] << ","
                        << std::setw(10) << hist[i] << std::endl;
            }
            else {
                std::vector<unsigned int> empty(nbins);
                imagefusion::Image histPlot = plotHist(hist, empty, bins, range, diff.basetype(), plotSize, withLegend, logarithmic, withGrid, /*gray*/ true, "difference");
                histPlot.write(outfilename);
            }
        }
    }

    // print stats
    std::vector<Stats> allStats = computeStats(diff, mask);
    for (unsigned int c = 0; c < allStats.size(); ++c) {
        Stats const& stats = allStats.at(c);
        if (allStats.size() > 1)
            std::cout << "Statistics for channel " << c << ":" << std::endl;

        std::cout << "Valid pixels:             " << diff.width() << " x " << diff.height() << " - " << (diff.size().area() - stats.validPixels) << " = " << stats.validPixels << std::endl;
        if (validSets[0] != validSets[1])
            std::cout << "Valid value range (img 1):" << validSets[0] << (options["MASKIMG"].empty() ? " (and no mask images)" : " (further restricted by mask images)") << std::endl
                      << "Valid value range (img 2):" << validSets[1] << (options["MASKIMG"].empty() ? " (and no mask images)" : " (further restricted by mask images)") << std::endl;
        std::cout << "Valid value range:        " << validSets[0] << (options["MASKIMG"].empty() ? " (and no mask images)" : " (further restricted by mask images)") << std::endl;
        if (stats.validPixels > 0)
            std::cout << "Average number of errors: " << stats.nonzeros << " / " << (stats.validPixels)
                                                      << " = " << (stats.nonzeros / stats.validPixels) << std::endl
                      << "AAD or MAD or mean error: " << stats.aad    << " / " << stats.validPixels << " / " << normMax
                                                      << " = " << (stats.aad / stats.validPixels) << " / " << normMax
                                                      << " = " << (stats.aad / stats.validPixels / normMax) << std::endl
                      << "RMSE:                     " << stats.rmse   << " / " << stats.validPixels << " / " << normMax
                                                      << " = " << (stats.rmse / stats.validPixels) << " / " << normMax
                                                      << " = " << (stats.rmse / stats.validPixels / normMax) << std::endl
                      << "Min error:                " << stats.min    << " / " << normMax << " = " << (stats.min    / normMax)
                                                      << " occurs " << (stats.minCount == 1 ? "just" : std::to_string(stats.minCount) + " times, first") << " at (" << stats.minLoc.x << ", " << stats.minLoc.y << ")" << std::endl
                      << "Max error:                " << stats.max    << " / " << normMax << " = " << (stats.max    / normMax)
                                                      << " occurs " << (stats.maxCount == 1 ? "just" : std::to_string(stats.maxCount) + " times, first") << " at (" << stats.maxLoc.x << ", " << stats.maxLoc.y << ")" << std::endl
                      << "Error std. dev.:          " << stats.stddev << " / " << normMax << " = " << (stats.stddev / normMax) << std::endl << std::endl;
    }

    // print pixels
    if (!options["AT"].empty()) {
        std::string fn1 = Parse::ImageFileName(imgargs[0]);
        std::string fn2 = Parse::ImageFileName(imgargs[1]);
        std::cout << "Printing Pixels, where i0 is " << fn1 << " and i1 is " << fn2 << ":" << std::endl;
    }
    for (auto const& opt : options["AT"]) {
        imagefusion::Interval coordInt = Parse::Interval(opt.arg);
        int x = std::round(coordInt.lower());
        int y = std::round(coordInt.upper());
        if (x >= i[0].width() || y >= i[0].height()) {
            std::cerr << "Cannot print pixel values at (" << x << ", " << y << "), since it is out of bounds. "
                      << "The image size is: " << i[0].size() << ". Ignoring this point." << std::endl;
            continue;
        }

        printPixel(i[0], x, y, "i0 ", "  ");
        printPixel(i[1], x, y, "i1 ", "  ");
        printPixel(diff, x, y, "diff ", "");
    }

    return 0;
}

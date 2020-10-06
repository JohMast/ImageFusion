#include <iostream>
#include <iomanip>
#include <string>

#include <filesystem>

#include "optionparser.h"
#include "geoinfo.h"
#include "image.h"
#include "fileformat.h"

#include "imggeocrop.h"
#include "utils_common.h"


const char* usageImage =
    "  -i <img>, --img=<img> \tImage to crop. At least two images are required. The -i or --img can be omitted.\v"
    "<img> can be a file path. If pre-cropping or using only a subset of channels / layers "
    "is desired, <img> must have the form '-f <file> [--crop-pix=<rect>] [--crop-proj=<rect>] [-l <num-list>] [--disable-use-color-table]', "
    "where the arguments can have an arbitrary order, but only one crop option is allowed. "
    "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"
    "\t  -f <file>,     --file=<file> \tSpecifies the image file path. GDAL subdataset paths are also valid, but have to be quoted.\n"
    "\t  -l <num-list>, --layers=<num-list> \tOptional. Specifies the bands or subdatasets, that will be read. Hereby a 0 means the first band/subdataset.\v"
    "<num-list> must have the format '(<num> [[,]<num> ...])' or just '<num>'.\n"
    "\t  --crop-pix=<rect> \tOptional. Specifies the crop window in pixels, where the image will be read. This restricts the intersection window further.\n"
    "\t  --crop-proj=<rect> \tOptional. Similar as --crop-pix, but specifies the crop window in projection space (metre).\n"
    "\t<rect> requires either all of the following arguments:\v"
    "  -c (<num> <num>), --center=(<num> <num>) x and y center\v"
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
    "          --img='-f \"test image.tif\" --crop-pix=(-x 1 -y 2 -w 300 -h 200) -l (0 2)'\v"
    "          --img='-f \"test image.tif\" --crop-pix=(-x=(100 300) -y=(50 100))'\v"
    "          --img='-f \"test image.tif\" -l 0'\v"
    "          --img='-f (HDF4_EOS:EOS_GRID:\"path/MOD09GA.hdf\":MODIS_Grid_500m_2D:sur_refl_b01_1)'\n";

const char* usageLongLatRect =
    "  -c <ll-rect>, --crop-longlat=<ll-rect> \tThe extents specified in longitude / latitude limit the cutset further.\n"
    "  --crop-longlat-full=<ll-rect> \tThe rectangle specified in <ll-rect> limits the cutset further, but should be fully included in the resulting image.\n"
    "\t<ll-rect> requires a combination of some of the following arguments:\n"
    "\t  --center=(<lat/long>) \tSpecifies the center location. To define the extents, specify either --width and --height or a --corner additionally.\n"
    "\t  --corner=<lat/long> \tSpecifies one corner for cropping. Use this option once to specify the top left corner in combination with --center "
    "or --width and --height to define the extents. Or just use it exactly twice to specify opposing corners, which also defines the extents.\n"
    "\t  -h <num>, --height=<num> \tSpecifies the height in projection space unit (usually metre).\n"
    "\t  -w <num>, --width=<num> \tSpecifies the width in projection space unit (usually metre).\n"
    "\tExamples: ... --crop-longlat=(--corner=(0d 0' 0.01\"E, 50d 0' 0.00\"N) --corner=(13d 3'14.66\"E, 40d 0' 0.00\"N)) ... \v"
    "          ... --crop-longlat=(--center=(7d 4' 15.84\"E, 45d N) -w 10000 -h 5000) ... \v"
    "          ... --crop-longlat=(--corner=(0d 0' 0.01\"E, 50d 0' 0.00\"N) -w 10000 -h 5000) ... \v"
    "          ... --crop-longlat=(--corner=(0d 0' 0.01\"E, 50d 0' 0.00\"N) --center=(7d 4' 15.84\"E, 45d N)) ... \v"
    "          ... --crop-longlat=\"--corner=(0d 0' 0.01\\\"E, 50d 0' 0.00\\\"N) --center=(7d 4' 15.84\\\"E, 45d N)\" ... \n";

const char* usageType =
    "  -t <type>, --out-type=<type> \tThis will be used as output type and can be useful to convert an unsigned integer to a signed or vice versa. "
    "Note, the valid value location are checked before conversion, but set to the nodata value afterwards. "
    "Values will saturate, when they do not fit into the new range. No scaling will be done (currently)."
    "<type> should be one of:\v"
    "uint8 (or Byte), uint16, int16, int32, float32 (or Single or just float) or float64 (or Double).\n";

const char* usageDataImage =
    "  --d <img>, --data-img=<img> \tcan be used exactly like --img, but it is meant for images that contain data. "
    "When resampling, data images will be using nearest neighbor method, while usual images may be using a more advanced interpolation method. "
    "Also saturation, masking and type conversion will not be applied on data images.\n";

using Descriptor = imagefusion::option::Descriptor;
using ArgChecker = imagefusion::option::ArgChecker;
using Parse      = imagefusion::option::Parse;
std::vector<Descriptor> usage{
    Descriptor::text("Usage: imggeocrop -i <img> -i <img> [options]\n"
                     "   or: imggeocrop <img> <img> [options]\n\n"
                     "The order of the options can be arbitrary, but at least two images are required for cropping. Remember to protect whitespace by quoting with '...', \"...\" or (...) or by escaping.\n"
                     "Options:"),
    {"CROPLONGLAT","",        "c", "crop-longlat",        ArgChecker::NonEmpty,       "  -c <ll-rect>, --crop-longlat=<ll-rect> \tThe extents specified in longitude / latitude limit the cutset further.\n"},
    {"CROPLONGLAT","FULL",    "c", "crop-longlat-full",   ArgChecker::NonEmpty,       usageLongLatRect},
    {"IMAGE",      "DISABLE", "d", "data-img",            argCheckImageProjCrop,      usageDataImage},
    {"SATURATE",   "DISABLE", "",  "disable-saturate",    ArgChecker::None,           "  --disable-saturate \tThis will leave all values as they are. Default.\n"},
    {"USENODATA",  "DISABLE", "",  "disable-use-nodata",  ArgChecker::None,           "  --disable-use-nodata \tThis will not use the nodata value as invalid range for masking.\n"},
    {"SATURATE",   "ENABLE",  "",  "enable-saturate",     ArgChecker::None,           "  --enable-saturate \tThis will set valid negative values to 0.\n"},
    {"USENODATA",  "ENABLE",  "",  "enable-use-nodata",   ArgChecker::None,           "  --enable-use-nodata \tThis will use the nodata value as invalid range for masking. Default.\n"},
    {"HELP",       "",        "h", "help",                ArgChecker::None,           "  -h, --help  \tPrint usage and exit.\n"},
    {"HELPFORMAT", "",        "",  "help-formats",        ArgChecker::None,           "  --help-formats  \tPrint all available file formats that can be used with --out-format and exit.\n"},
    {"IMAGE",      "",        "i", "img",                 argCheckImageProjCrop,      usageImage},
    {"MASKRANGE",  "INVALID", "",  "mask-invalid-ranges", ArgChecker::IntervalSet,    helpers::usageInvalidRanges},
    {"MASKRANGE",  "VALID",   "",  "mask-valid-ranges",   ArgChecker::IntervalSet,    helpers::usageValidRanges},
    Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file "
                     "are specified in the same way as on the command line. You can use newlines "
                     "between options and line comments with # (use \\# to get a non-comment #). "
                     "The specified options in the file replace the --option-file=<file> argument "
                     "before they are parsed.\n"),
    {"FORMAT",     "",        "f", "out-format",          ArgChecker::NonEmpty,       "  -f <fmt>, --out-format=<fmt>  \tUse the specified image file format, like GTiff, as output. See also --help-formats.\n"},
    {"LIKE",       "",        "l", "out-like",            ArgChecker::File,           "  -l <img>, --out-like=<img>  \tUse for output type and format like the specified image.\n"},
    {"OUTPOSTFIX", "",        "",  "out-postfix",         ArgChecker::Optional,       "  --out-postfix=<string> \tThis will be appended to the output filenames.\n"},
    {"OUTPREFIX",  "",        "",  "out-prefix",          ArgChecker::Optional,       "  --out-prefix=<string> \tThis will be prepended to the output filenames. By default this is 'cropped_'.\n"},
    {"TYPE",       "",        "t", "out-type",            ArgChecker::Type,           usageType},
    {"NODATAVAL",  "",        "",  "set-nodata-val",      ArgChecker::Float,          "  --set-nodata-val=<float> \tSets the nodata value to the specified value.\n"},
    {"GEOEXTIMG",  "",        "",  "use-extents-from",    argCheckImageProjCrop,      "  --use-extents-from=<img> \tThe extents from the image limit the cutset further.\n"},
    Descriptor::breakTable(),
    Descriptor::text("\nExamples:\n"
                     "  \timggeocrop -i 'file 1.tif'  -i file2.tif\v"
                     "will rescale the coarser resolution image (if the resolutions are different) and crop one or both of the specified images and output the rescaled cropped images with a prefix 'cropped_'.\v\v"
                     "imggeocrop 'file 1.tif'  file2.tif --out-prefix='' --out-postfix='_cropped'\v"
                     "does the same as above, but with the postfix '_cropped' and no prefix.\v\v")
};

int main(int argc, char* argv[]) {
    using std::swap;

    // parse arguments with accepting options after non-option arguments, like ./imgcompare file1.tif file2.tif --out-diff=diff.tif
    std::string defaults = "--out-prefix=cropped_ --disable-saturate --enable-use-nodata";
    imagefusion::option::OptionParser options(usage);
    options.acceptsOptAfterNonOpts = true;
    options.parse(defaults).parse(argc, argv);

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

    std::vector<std::string>& imgargs = options.nonOptionArgs;
    std::vector<bool> isDataImg(imgargs.size() + options["IMAGE"].size());
    for (auto& o : options["IMAGE"]) {
        imgargs.push_back(o.arg);
        isDataImg.at(imgargs.size() - 1) = o.prop() == "DISABLE";
    }

    std::vector<std::string> extimgargs;
    for (auto& o : options["GEOEXTIMG"])
        extimgargs.push_back(o.arg);

    std::vector<std::string> longLatArgs;
    std::vector<bool> longLatFull;
    for (auto const& o : options["CROPLONGLAT"]) {
        longLatArgs.push_back(o.arg);
        longLatFull.push_back(o.prop() == "FULL");
    }

    // process geo infos and find intersection
    ProcessedGI pGI = getAndProcessGeoInfo<Parse>(imgargs, extimgargs, longLatArgs, longLatFull);
    if (!pGI.haveGI && (!extimgargs.empty() || !longLatArgs.empty())) {
        std::string err;
        if (!extimgargs.empty())
            err += "--" + options["GEOEXTIMG"].front().name;
        if (!err.empty())
            err += " and ";
        if (!longLatArgs.empty())
            err += "--" + options["CROPLONGLAT"].front().name;
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "At least one image does not have geo information, but you specified " + err + ", which does not make sense."));
    }

    // nodata value
    double newNodataVal = std::numeric_limits<double>::quiet_NaN();
    if (!options["NODATAVAL"].empty())
        newNodataVal = Parse::Float(options["NODATAVAL"].back().arg);
    bool doSetNodataVal = !std::isnan(newNodataVal);
    bool useNodataValue = options["USENODATA"].back().prop() == "ENABLE";

    // combine valid / invalid ranges
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

    // output filename options
    auto prepost = helpers::getPrefixAndPostfix(options["OUTPREFIX"], options["OUTPOSTFIX"], "cropped_", "output prefix");
    std::string prefix  = prepost.first;
    std::string postfix = prepost.second;

    // output type and format
    imagefusion::FileFormat outformat = imagefusion::FileFormat::unsupported;
    imagefusion::Type outtype = imagefusion::Type::invalid;
    for (auto& opt : options.input) {
        if (opt.spec() == "TYPE")
            outtype = imagefusion::getBaseType(Parse::Type(opt.arg));
        else if (opt.spec() == "FORMAT") {
            outformat = imagefusion::FileFormat(opt.arg);
            if (outformat == imagefusion::FileFormat::unsupported)
                std::cerr << "The image file format " << opt.arg << " is not supported on your platform." << std::endl;
        }
        else if (opt.spec() == "LIKE") {
            outformat = imagefusion::FileFormat::fromFile(opt.arg);
            imagefusion::GeoInfo outgi{opt.arg};
            outtype = outgi.baseType;
        }
    }
    if (outtype == imagefusion::Type::int8)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Sorry, signed 8-bit integer is not supported for I/O, since all GDAL image format drivers will interpret this as unsigned 8-bit integer."));
    bool doConvert = outtype != imagefusion::Type::invalid;

    // saturate?
    imagefusion::IntervalSet saturationSet;
    bool doSaturate = options["SATURATE"].back().prop() == "ENABLE";
    if (doSaturate) {
        saturationSet += imagefusion::Interval::open(-std::numeric_limits<double>::infinity(), 0.0);
        if (!options["MASKRANGE"].empty())
            saturationSet &= baseValidSet;
        doSaturate = !saturationSet.empty();
    }

    // process all images
    for (unsigned int idx = 0; idx < pGI.gis.size(); ++idx) {
        imagefusion::GeoInfo& gi = pGI.gis.at(idx);
        std::string filename = Parse::ImageFileName(imgargs.at(idx));
        imagefusion::Image i;
        imagefusion::Image mask;

        // exclude nodata value from valid range
        imagefusion::IntervalSet imgValidSet = baseValidSet;
        if (useNodataValue) {
            if (!hasValidRanges)
                imgValidSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

            if (gi.hasNodataValue()) {
                imagefusion::Interval nodataInt = imagefusion::Interval::closed(gi.getNodataValue(), gi.getNodataValue());
                imgValidSet -= nodataInt;
            }
        }

        if (!pGI.haveGI) {
            i = Parse::Image(imgargs[idx]);

            // make mask
            if ((hasValidRanges || useNodataValue) && !isDataImg.at(idx)) {
                std::cout << "Valid ranges for masking: " << imgValidSet << std::endl;
                mask = i.createMultiChannelMaskFromSet({imgValidSet});

                // set all invalid pixels to nodata value
                if (useNodataValue && gi.hasNodataValue())
                    i.set(gi.getNodataValue(), mask.bitwise_not());
            }
        }
        else {
            // find target rectangle in image coordinates and read only this image part for performance reasons
            imagefusion::CoordRectangle targetRect(pGI.targetGI.geotrans.offsetX, pGI.targetGI.geotrans.offsetY,
                                                   pGI.targetGI.geotrans.XToX * pGI.targetGI.width(), pGI.targetGI.geotrans.YToY * pGI.targetGI.height());
            constexpr unsigned int numPoints = 33;
            std::vector<imagefusion::Coordinate> boundariesTarget = imagefusion::detail::makeRectBoundaryCoords(targetRect, numPoints);
            std::vector<imagefusion::Coordinate> boundariesImg = pGI.targetGI.projToImg(boundariesTarget, gi);

            imagefusion::CoordRectangle restrictCoordRect = imagefusion::detail::getRectFromBoundaryCoords(boundariesImg);

            imagefusion::Rectangle restrictRect(std::floor(restrictCoordRect.x + 1e-8),  // one pixel oversize
                                                std::floor(restrictCoordRect.y + 1e-8),
                                                0, 0); // dummies
            restrictRect.width = std::ceil(restrictCoordRect.br().x - 1e-8) - restrictRect.x;
            restrictRect.height = std::ceil(restrictCoordRect.br().y - 1e-8) - restrictRect.y;
            restrictRect &= imagefusion::Rectangle(0, 0, gi.width(), gi.height());

            // read image part
            i = imagefusion::Image{filename, Parse::ImageLayers(imgargs[idx]), restrictRect, /*flipH*/ false, /*flipV*/ false, Parse::ImageIgnoreColorTable(imgargs[idx])};
            gi.geotrans.translateImage(restrictRect.x, restrictRect.y);

            // make mask
            if ((hasValidRanges || useNodataValue) && !isDataImg.at(idx)) {
                std::cout << "Valid ranges for masking: " << imgValidSet << std::endl;
                mask = i.createMultiChannelMaskFromSet({imgValidSet});

                // set all invalid pixels to nodata value before warping
                if (useNodataValue && gi.hasNodataValue())
                    i.set(gi.getNodataValue(), mask.bitwise_not());
            }

            // reproject
            auto interpMethod = isDataImg.at(idx) ? imagefusion::InterpMethod::nearest
                                                  : imagefusion::InterpMethod::bilinear;
            imagefusion::GeoInfo targetGI = pGI.targetGI;
            targetGI.clearNodataValues();
            i = i.warp(gi, targetGI, interpMethod);
            if (!mask.empty())
                mask = mask.warp(gi, targetGI, imagefusion::InterpMethod::nearest);
            gi.geotrans = targetGI.geotrans;
            gi.geotransSRS = targetGI.geotransSRS;
        }

        if (!isDataImg.at(idx)) {
            // convert type
            if (doConvert && outtype != i.basetype())
                i = i.convertTo(outtype);

            // set nodata value and invalid pixels to nodata value
            double rangeMin = imagefusion::getImageRangeMin(i.type());
            double rangeMax = imagefusion::getImageRangeMax(i.type());
            bool printNewline = false;
            if (doSetNodataVal) {
                if ((newNodataVal < rangeMin || newNodataVal > rangeMax) && imagefusion::isIntegerType(i.type())) {
                    std::cout << "New nodata value (" << newNodataVal << ") does not fit into the image data range (["
                              << imagefusion::getImageRangeMin(i.type()) << ", " << imagefusion::getImageRangeMax(i.type()) << "]). ";
                    printNewline = true;
                }
                else
                    gi.setNodataValue(newNodataVal);
            }

            if (gi.hasNodataValue() && (gi.getNodataValue() < rangeMin || gi.getNodataValue() > rangeMax) && imagefusion::isIntegerType(i.type())) {
                std::cout << "Original nodata value (" << gi.getNodataValue() << ") does not fit into the image data range (["
                          << imagefusion::getImageRangeMin(i.type()) << ", " << imagefusion::getImageRangeMax(i.type()) << "]). ";
                gi.clearNodataValues();
                printNewline = true;
            }

            if (!gi.hasNodataValue()) {
                double ndv = helpers::findAppropriateNodataValue(i, mask);
                if (std::isnan(ndv)) {
                    std::cout << "The nodata value could not be set to a specific value, since all possible values exist in the image. ";
                    if (!mask.empty())
                        std::cout << "Therefore a separate mask file will be output.";
                }
                else {
                    gi.setNodataValue(ndv);
                    std::cout << "Changed nodata value to " << gi.getNodataValue() << ".";
                }
                printNewline = true;
            }
            if (printNewline)
                std::cout << std::endl;

            if (!mask.empty() && gi.hasNodataValue())
                i.set(gi.getNodataValue(), mask.bitwise_not());

            // apply saturation only to valid locations
            if (doSaturate && imagefusion::isSignedType(i.basetype())) {
                imagefusion::Image satmask = i.createMultiChannelMaskFromSet({saturationSet});
                if (!mask.empty())
                    satmask = mask.bitwise_and(std::move(satmask));
                i.set(0, satmask);
            }
        }

        // output file with prefix and postfix
        if (outformat == imagefusion::FileFormat::unsupported)
            outformat = imagefusion::FileFormat::fromFile(filename);
        try {
            // avoid overwriting when using the same file multiple times with different layers (e. g. visible bands and quality layers from an HDF file)
            std::string postfixWithLayers = postfix;
            std::vector<int> layers = Parse::ImageLayers(imgargs.at(idx));
            for (int l : layers)
                postfixWithLayers = "_b" + std::to_string(l) + postfixWithLayers;

            std::string outfilename = helpers::outputImageFile(i, gi, filename, prefix, postfixWithLayers, outformat);
            std::cout << "Wrote file " << outfilename << "." << std::endl;

            // maybe output mask (if choosing a nodata value failed)
            if (!mask.empty() && !gi.hasNodataValue()) {
                std::string maskfilename = helpers::outputImageFile(mask, gi, outfilename, "mask_", "", outformat);
                std::cout << "Wrote mask file to " << maskfilename << " with 255 being valid values." << std::endl;
            }
        }
        catch (imagefusion::runtime_error&) {
            std::cout << "Could not write the output of processed " << filename << ", sorry. Going on with the next one." << std::endl;
        }
    }

    return 0;
}

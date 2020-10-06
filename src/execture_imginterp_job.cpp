#include <Rcpp.h>
#include <iostream>
#include <iomanip>
#include <string>

#include <filesystem>

#include "optionparser.h"
#include "geoinfo.h"
#include "multiresimages.h"
#include "utils_common.h"
#include "fileformat.h"

#include "interpolation.h"
#include "customopts.h"

using namespace Rcpp;

using  std::vector;


using Descriptor = imagefusion::option::Descriptor;
std::vector<Descriptor> usage{
  
  Descriptor::text("Usage: imginterp -i <img> -i <img> -i <img> [options]\n"
                     "   or: imginterp --option-file=<file> [options]\n"
                     "   or: imginterp \t-i <img> [--help] [--disable-output-masks] [--disable-use-nodata] [--enable-output-masks] [--enable-use-nodata] [--help] [--img] [--mask-img] "
                     "[--interp-ranges] [--mask-invalid-ranges] [--mask-valid-ranges] [--no-interp-ranges] [--out-mask-postfix] [--out-mask-prefix] [--out-postfix] [--out-prefix]  "
                     "[--ql-fmask] [--ql-img] [--ql-modis]\n"),
                     Descriptor::breakTable(),
                     
                     Descriptor::text("This utility is developed to perform simple interpolation on a given time series of remote sensing images."
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
                                        {"PSOUT",         "ENABLE",  "",  "enable-output-pixelstate", ArgChecker::None,                 "usageEnablePixelstate"},
                                        {"PRIOCLOUDS",    "ENABLE",  "",  "enable-prioritize-interp", ArgChecker::None,                 "  --enable-prioritize-interp  \tWhen a pixel location is marked as invalid and as interpolate, handle as invalid location and do not interpolate. Default."},
                                        {"PRIOCLOUDS",    "DISABLE", "",  "enable-prioritize-invalid",ArgChecker::None,                 "  --enable-prioritize-invalid \tWhen a pixel location is marked as invalid and as interpolate, handle as location to interpolate."},
                                        {"USENODATA",     "ENABLE",  "",  "enable-use-nodata",        ArgChecker::None,                 "  --enable-use-nodata  \tThis will use the nodata value as invalid range for masking. Default.\n"},
                                        {"HELP",          "",        "h", "help",                     ArgChecker::None,                 "  -h, --help  \tPrint usage and exit.\n"},
                                        {"IMAGE",         "",        "i", "img",                      ArgChecker::None, "usageImage"},
                                        {"INTERPRANGE",   "VALID",   "",  "interp-ranges",            ArgChecker::None,          "usageInterpRanges"},
                                        {"LIMIT",         "",        "l", "limit-days",               ArgChecker::None,                  "  -l <num>, --limit-days=<num>  \tLimit the maximum numbers of days from the interpolating day that will be considered. So using e. g. a 3 will only consider images that are 3 days apart from the interpolation day. Default 5.\n"},
                                        {"MASKIMG",       "",        "m", "mask-img",                 ArgChecker::None,  "usageMaskFile"},
                                        {"MASKRANGE",     "INVALID", "",  "mask-invalid-ranges",      ArgChecker::None,          "helpers::usageInvalidRanges"},
                                        {"MASKRANGE",     "VALID",   "",  "mask-valid-ranges",        ArgChecker::None,          "helpers::usageValidRanges"},
                                        {"INTERPRANGE",   "INVALID", "",  "no-interp-ranges",         ArgChecker::None,          "usageNoInterpRanges"},
                                        Descriptor::text("  --option-file=<file> \tRead options from a file. The options in this file are specified in the same way as on the command line. You can use newlines between options "
                                                           "and line comments with # (use \\# to get a non-comment #). The specified options in the file replace the --option-file=<file> argument before they are parsed.\n"),
                                                           {"OUTPSPOSTFIX",  "",        "",  "out-pixelstate-postfix",   ArgChecker::None,             "  --out-pixelstate-postfix=<string> \tThis will be appended to the output filenames (including prefix and postfix) to form the pixel state bitfield filenames. Only used if pixel state output is enabled.\n"},
                                                           {"OUTPSPREFIX",   "",        "",  "out-pixelstate-prefix",    ArgChecker::None,             "  --out-pixelstate-prefix=<string> \tThis will be prepended to the output filenames (including prefix and postfix) to form the pixel state bitfield filenames. Only used if pixelstate output is enabled. By default this is 'ps_'.\n"},
                                                           {"OUTPOSTFIX",    "",        "",  "out-postfix",              ArgChecker::None,             "  --out-postfix=<string> \tThis will be appended to the output filenames.\n"},
                                                           {"OUTPREFIX",     "",        "",  "out-prefix",               ArgChecker::None,             "  --out-prefix=<string> \tThis will be prepended to the output filenames. By default this is 'interpolated_'.\n"},
                                                           {"STATS",         "",       "s",  "stats",                    ArgChecker::Optional,             "  -s, --stats, -s <out>, --stats=<out> \tEnable stats (cloud pixels before and after, etc.) and output into the given file. If no file is specified it is output to stdout.\n"},
                                                           Descriptor::breakTable(),
                                                           Descriptor::text("\nExamples 1:\n"
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
                                                                              
                                                                              "\nExamples 2:\n"
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


//===========================================imginterp=================================
// [[Rcpp::export]]
void execute_imginterp_job_cpp(
    CharacterVector input_filenames, 
    IntegerVector input_dates, 
    const std::string& input_string,
    const std::string& MASKIMG_options,
    const std::string& MASKRANGE_options){
  
  //So it begins
  // collect arguments for images, quality layers and masks
  imagefusion::option::OptionParser options(usage);
  
  //Default Arguments
  // parse arguments with accepting options after non-option arguments, like ./interpolator file1.tif file2.tif file2.tif --out-diff=diff.tif
  std::string defaultArgs = "--enable-use-nodata --disable-output-pixelstate --disable-interp-invalid --enable-prioritize-invalid --out-prefix='interpolated_' --out-pixelstate-prefix='ps_' --limit-days=5";
  options.acceptsOptAfterNonOpts = true;
  options.parse(defaultArgs).parse(input_string);

  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  }//end function
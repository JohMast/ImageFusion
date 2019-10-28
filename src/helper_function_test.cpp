#include <Rcpp.h>
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
#include "utils_common.h"

using namespace Rcpp;
using namespace imagefusion;

using  std::vector;
using Descriptor = imagefusion::option::Descriptor;
using ArgChecker = imagefusion::option::ArgChecker;
using Parse      = imagefusion::option::Parse;


std::vector<Descriptor> usage{
  // first element usually just introduces the help
  Descriptor::text("Usage: program [options]\n\n"
                     "Options:"),
                     // then the regular options follow
                     // group specifier, property,  short, long option,      checkArg function,     help table entry
                     {  "FILTER",        "DISABLE", "",    "disable-filter", ArgChecker::None,      "\t--disable-filter  \tDisable filtering of similar..."},
                     {  "FILTER",        "ENABLE",  "",    "enable-filter",  ArgChecker::None,      "\t--enable-filter  \tEnable filtering of similar..."  },
                     {  "PREDAREA",      "",        "pa",  "pred-area",      ArgChecker::Rectangle, "  -p <rect>, -a <rect>, \t--pred-area=<rect>  \tSpecify prediction area to..." },
                     {"MASKRANGE",     "HIGHINVALID", "",  "mask-high-res-invalid-ranges", ArgChecker::IntervalSet, "  --mask-high-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the high resolution images.\n"},
                     {"MASKRANGE",     "HIGHVALID",   "",  "mask-high-res-valid-ranges",   ArgChecker::IntervalSet, "  --mask-high-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the high resolution images.\n"},
                     {"MASKRANGE",     "INVALID",     "",  "mask-invalid-ranges",          ArgChecker::IntervalSet, helpers::usageInvalidRanges},
                     {"MASKRANGE",     "LOWINVALID",  "",  "mask-low-res-invalid-ranges",  ArgChecker::IntervalSet, "  --mask-low-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the low resolution images.\n"},
                     {"MASKRANGE",     "LOWVALID",    "",  "mask-low-res-valid-ranges",    ArgChecker::IntervalSet, "  --mask-low-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the low resolution images.\n"},
                     {"MASKRANGE",     "VALID",       "",  "mask-valid-ranges",            ArgChecker::IntervalSet, helpers::usageValidRanges},
                     // insert the predefined option-file Descriptor
                     Descriptor::optfile() // (for alphabetic order put this before prediction area option)
};
std::vector<Descriptor> *usage_ptr = &usage;

//' @export
// [[Rcpp::export]]

void testoptions(){

  auto options = imagefusion::option::OptionParser::parse(usage,"--mask-valid-ranges=\"[0, 10000]\"");
  auto baseValidSets = helpers::parseAndCombineRanges<Parse>(options["MASKRANGE"]);
  auto predValidSets = baseValidSets;

  }

// 
// void testranges(){
//   std::vector test_range = "[0, 10000]";
//   //auto baseValidSets = helpers::parseAndCombineRanges<Parse>(options["MASKRANGE"]);
//   //auto baseValidSets = helpers::parseAndCombineRanges<Parse>();
//   auto baseValidSets = helpers::parseAndCombineRanges<Parse>(test_range);
//   if (!baseValidSets.high.empty()) {
//     imagefusion::Interval const& first = *baseValidSets.high.begin();
//     imagefusion::Interval const& last  = *(--baseValidSets.high.end());
//     double rangeMin = first.lower();
//     double rangeMax = last.upper();
//     std::cout << "Data range: [" << rangeMin << ", " << rangeMax << "]" << std::endl;
//     //estarfmOpts.setDataRange(rangeMin, rangeMax);
//   }
//   
//   
//   }
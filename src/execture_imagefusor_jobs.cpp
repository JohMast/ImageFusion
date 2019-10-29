#include <Rcpp.h>
#include "Starfm.h"
#include "Estarfm.h"
#include "fitfc.h"
#include "spstfm.h"
#include "utils_common.h"
#include "MultiResImages.h"
using namespace Rcpp;

using  std::vector;
using Parse  = imagefusion::option::Parse;

//' @export
// [[Rcpp::export]]
void execute_estarfm_job_cpp(CharacterVector input_filenames, //character vector length n_i
                         CharacterVector input_resolutions, //character vector length n_i
                         IntegerVector input_dates, //character vector length n_i
                         IntegerVector pred_dates, //vector of length n_o
                         CharacterVector pred_filenames,   //vector of length n_o
                         IntegerVector pred_area, //vector of x1 y1 width height) 
                         int winsize,   //windowsize
                         int date1,
                         int date3,
                         bool use_local_tol,
                         bool use_quality_weighted_regression,
                         bool output_masks,
                         bool use_nodata_value,
                         double uncertainty_factor,
                         double number_classes,
                         double data_range_min,
                         double data_range_max,
                         const std::string& hightag,  //string identifying the high res images
                         const std::string& lowtag,    //string identifying the low res images
                         const std::string& MASKIMG_options,
                         const std::string& MASKRANGE_options
                           )
  {

  using namespace imagefusion;
  //Step 1: Prepare the Inputs
  //(This function assumes that the inputs are all in correct length and type,
  //any checks, assertions, conversions, and error handling
  //are performed within the R wrapper function)

  Rectangle pred_rectangle = Rectangle{pred_area[0],pred_area[1],pred_area[2],pred_area[3]};
  //Step 2: Prepare the fusion
  //Get the Geoinformation from the first of the images
  GeoInfo gi{as<std::string>(input_filenames[1])};
  //Loop over all the input filenames and load them into a MultiResImages object
  //for every filename, set the corresponding resolution tag
  //and date
  auto mri = std::make_shared<MultiResImages>();
  int n_inputs = input_filenames.size();
  for(int i=0; i< n_inputs;++i){
    mri->set(as<std::string>(input_resolutions[i]),
             input_dates[i],
                  Image(as<std::string>(input_filenames[i])));
  }
  
  //Set the desired Options
  EstarfmOptions o;
  o.setHighResTag(hightag);
  o.setLowResTag(lowtag);
  o.setDate1(date1);
  o.setDate3(date3);
  o.setPredictionArea(pred_rectangle);
  o.setWinSize(winsize);
  o.setNumberClasses(number_classes);
  o.setUncertaintyFactor(uncertainty_factor);
  o.setUseLocalTol(use_local_tol);
  o.setDataRange(data_range_min,data_range_max);
  o.setUseQualityWeightedRegression(use_quality_weighted_regression);
  
  
  
  

  //Step 3: Create the Fusor
  //Create the Fusor
  EstarfmFusor esf;
  esf.srcImages(mri); //Pass Images
  esf.processOptions(o);   //Pass Options
  
  //Step 4: Handle the Masking for the Input Pairs
  //We create a single base mask "pairMask" derived from applying the ranges(if any are given) to the
  //Images for Date1 and Date3
  //This is implemented effectively in the c++ code, which is why we simply wrap around its option parser system
  //and pass it strings which contain the options we would like, as if on command line
  using Descriptor = imagefusion::option::Descriptor;
  using ArgChecker = imagefusion::option::ArgChecker;
  using Parse      = imagefusion::option::Parse;
  std::vector<Descriptor> usage{
    Descriptor::text(""),
                       {"MASKIMG",       "",            "m", "mask-img",                     ArgChecker::Mask,        helpers::usageMaskFile},
                       {"MASKRANGE",     "HIGHINVALID", "",  "mask-high-res-invalid-ranges", ArgChecker::IntervalSet, "  --mask-high-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the high resolution images.\n"},
                       {"MASKRANGE",     "HIGHVALID",   "",  "mask-high-res-valid-ranges",   ArgChecker::IntervalSet, "  --mask-high-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the high resolution images.\n"},
                       {"MASKRANGE",     "INVALID",     "",  "mask-invalid-ranges",          ArgChecker::IntervalSet, helpers::usageInvalidRanges},
                       {"MASKRANGE",     "LOWINVALID",  "",  "mask-low-res-invalid-ranges",  ArgChecker::IntervalSet, "  --mask-low-res-invalid-ranges=<range-list> \tThis is the same as --mask-invalid-ranges, but is applied only for the low resolution images.\n"},
                       {"MASKRANGE",     "LOWVALID",    "",  "mask-low-res-valid-ranges",    ArgChecker::IntervalSet, "  --mask-low-res-valid-ranges=<range-list> \tThis is the same as --mask-valid-ranges, but is applied only for the low resolution images.\n"},
                       {"MASKRANGE",     "VALID",       "",  "mask-valid-ranges",            ArgChecker::IntervalSet, helpers::usageValidRanges},
                       Descriptor::optfile() // 
  };

  
  //MASK
  //First, parse the MASKIMG options, if given
  auto maskoptions = imagefusion::option::OptionParser::parse(usage,MASKIMG_options);
  std::vector<std::string> maskImgArgs;
  for (auto const& opt : maskoptions["MASKIMG"])
    maskImgArgs.push_back(opt.arg);
  //Get the Mask if one was given in the options, otherwise create a base mask from scratch
  //NOTE: TAKES THE NUMBER OF CHANNELS FROM THE TEMPLATE
  imagefusion::Image baseMask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, gi.channels, !maskoptions["MASKRANGE"].empty());
  
  //RANGE
  auto rangeoptions = imagefusion::option::OptionParser::parse(usage,MASKRANGE_options);
  auto baseValidSets = helpers::parseAndCombineRanges<Parse>(rangeoptions["MASKRANGE"]);
  auto pairValidSets = baseValidSets;
  
  //adjust ranges by including the nodata value, if desired
  if (use_nodata_value) { //Only in case we want to include the nodatavalue
    if (!pairValidSets.hasHigh)//If there are NO ranges given for the highrez
      //set high limits to entire datarange
      pairValidSets.high += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    if (!pairValidSets.hasLow)//If there are NO ranges given for the lowrez
      //set low limits to entire datarange
      pairValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    //Now ranges are given.
    pairValidSets.hasHigh = pairValidSets.hasLow = true;
    
    //Then we get the NoDataValue from the GeoInfo (if there is one), make an interval of it, and subtract it from the
    //high rez ranges#TO DO: ADJUST SO IT ACTUALLY TAKES THE CORRECT FILE
    GeoInfo const& giHigh{as<std::string>(input_filenames[1])};
    if (giHigh.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHigh.getNodataValue(), giHigh.getNodataValue());
      pairValidSets.high -= nodataInt;
    }
    //Then we get the NoDataValue from the GeoInfo (if there is one), make an interval of it, and subtract it from the
    //low rez ranges #TO DO: ADJUST SO IT ACTUALLY TAKES THE CORRECT FILE
    GeoInfo const& giLow{as<std::string>(input_filenames[1])};
    if (giLow.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giLow.getNodataValue(), giLow.getNodataValue());
      pairValidSets.low -= nodataInt;
    }
  }
  
  
  
  
  
  imagefusion::Image pairMask = baseMask;
  if (pairValidSets.hasHigh) //If there are any ranges given for the highrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date1), pairValidSets.high);
  if (pairValidSets.hasLow) //If there are any ranges given for the lowrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date1), pairValidSets.low);
  
  
  
  
  
  
  
  
  
  
  
  
  //Predict for desired Dates
  int n_outputs = pred_dates.size();
  for(int i=0; i< n_outputs;++i){
    //Get the destination filename
    std::string pred_filename=as<std::string>(pred_filenames[i]);
    
    //Make the Mask
    //We use the basic ranges
    auto predValidSets = baseValidSets;
    //TO DO: INSERT OPTION FOR USING NODATAVALUE
    if (use_nodata_value) {
      if (!predValidSets.hasLow)
        predValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
      predValidSets.hasLow = true;
    //TO DO: ADJUST SO IT ACTUALLY TAKES THE CORRECT FILE ATM takes just the basic geoinfo
      if (gi.hasNodataValue()) { 
        imagefusion::Interval nodataInt = imagefusion::Interval::closed(gi.getNodataValue(), gi.getNodataValue());
        predValidSets.low -= nodataInt;
      }
    }
    //For every prediction, we start out with the pairMask, which we obtained earlier through a combination of Date1 and Date3 masks 
    imagefusion::Image predMask = pairMask;
    
    if (predValidSets.hasLow)//If there are any ranges given for the lowrez :
      //Adjust the mask by also applying those ranges.
      predMask = helpers::processSetMask(std::move(predMask), mri->get(lowtag, pred_dates[i]), predValidSets.low);
    
    //Predict using the new mask we have made
    esf.predict(pred_dates[i],predMask);
    //Write to destination
    esf.outputImage().write(pred_filename);
    
    
    
    if (output_masks){
      imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
      std::string outmaskfilename = helpers::outputImageFile(predMask, gi, "Mask", pred_filename, "", outformat, date1, pred_dates[i], date3);}

    //Add the Geoinformation of the TEMPLATE to the written file
    if (gi.hasGeotransform()) {
      gi.geotrans.translateImage(pred_rectangle.x, pred_rectangle.y);
      if (pred_rectangle.width != 0)
        gi.size.width = pred_rectangle.width;
      if (pred_rectangle.height != 0)
        gi.size.height = pred_rectangle.height;
      gi.addTo(pred_filename);
    }
    
  }
}










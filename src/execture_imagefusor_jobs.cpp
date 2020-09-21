#include <Rcpp.h>
#include "Starfm.h"
#include "Estarfm.h"
#include "fitfc.h"
#include "spstfm.h"
#include "utils_common.h"
#include "MultiResImages.h"
#include "GeoInfo.h"
#include <filesystem>
#ifdef WITH_OMP
#include "Parallelizer.h"
#include "ParallelizerOptions.h"
#endif /* WITH_OMP */

using namespace Rcpp;

using  std::vector;

//===========================================estarfm=================================
// [[Rcpp::export]]
void execute_estarfm_job_cpp(CharacterVector input_filenames, 
                             CharacterVector input_resolutions, 
                             IntegerVector input_dates, 
                             IntegerVector pred_dates, 
                             CharacterVector pred_filenames,   
                             IntegerVector pred_area, 
                             int winsize,   
                             int date1,
                             int date3,
                             int n_cores,
                             bool use_local_tol,
                             bool use_quality_weighted_regression,
                             bool output_masks,
                             bool use_nodata_value,
                             bool verbose,
                             double uncertainty_factor,
                             double number_classes,
                             double data_range_min,
                             double data_range_max,
                             const std::string& hightag,  
                             const std::string& lowtag,   
                             const std::string& MASKIMG_options,
                             const std::string& MASKRANGE_options
)
{
#ifndef WITH_OMP
  if(n_cores>1){
    Rcout <<"Sorry, if you want to use Parallelizer, you need to install OpenMP first."<<std::endl;
  }
#endif 
  
  using namespace imagefusion;
  //Step 1: Prepare the Input
  //create a prediction area rectangle
  Rectangle pred_rectangle = Rectangle{pred_area[0],pred_area[1],pred_area[2],pred_area[3]};
  
  // find the gi the first pair high image 
  Rcpp::LogicalVector is_high(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_high[i] = (input_resolutions[i] == hightag);}
  Rcpp::CharacterVector filenames_high = input_filenames[is_high];
  std::string high_template_filename = Rcpp::as<std::vector<std::string> >(filenames_high)[0];
 //GeoInfo const& giHighPair1 {high_template_filename};
  GeoInfo giHighPair1{high_template_filename};
  if(verbose){Rcout <<"Getting High Resolution Geoinformation from File: "<<high_template_filename<<std::endl;}
  
  // find the gi the first pair low image 
  Rcpp::LogicalVector is_low(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_low[i] = (input_resolutions[i] == lowtag);}
  Rcpp::CharacterVector filenames_low = input_filenames[is_low];
  std::string low_template_filename = Rcpp::as<std::vector<std::string> >(filenames_low)[0];
  if(verbose){Rcout <<"Getting Low Resolution Geoinformation from File: "<<low_template_filename<<std::endl;}
  GeoInfo  giLowPair1{low_template_filename};
  

  
  
  
  //Step 2: Load the images and set the options
  
  //Load the images into a mri
  auto mri = std::make_shared<MultiResImages>();
  int n_inputs = input_filenames.size();
  for(int i=0; i< n_inputs;++i){
    mri->set(as<std::string>(input_resolutions[i]),
             input_dates[i],
                        Image(as<std::string>(input_filenames[i])));
  }
  
  //Pass the desired Options
  //required arguments
  EstarfmOptions o;
  o.setHighResTag(hightag);
  o.setLowResTag(lowtag);
  o.setDate1(date1);
  o.setDate3(date3);
  //optional arguments
  o.setPredictionArea(pred_rectangle);
  o.setWinSize(winsize);
  o.setNumberClasses(number_classes);
  o.setUncertaintyFactor(uncertainty_factor);
  o.setUseLocalTol(use_local_tol);
  o.setDataRange(data_range_min,data_range_max);
  o.setUseQualityWeightedRegression(use_quality_weighted_regression);
  
  
  
  //Step 3: Create the Fusor and pass the options
  //Create the Fusor
#ifdef WITH_OMP
    ParallelizerOptions<EstarfmOptions> po;
    po.setNumberOfThreads(n_cores);
    po.setAlgOptions(o);
    Parallelizer<EstarfmFusor> esf;
    esf.srcImages(mri);
    esf.processOptions(po);
#else /* WITH_OMP not defined */
    EstarfmFusor esf;
    esf.srcImages(mri); 
    esf.processOptions(o);
#endif /* WITH_OMP */
  

  
  
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
  //Gets the Mask if one was given in the options, otherwise create a base mask
  // from scratch. Uses first pair high image  as template.
  imagefusion::Image baseMask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, giHighPair1.channels, !maskoptions["MASKRANGE"].empty());
  
  //get ranges
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
    
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    //make an interval of it, and subtract it from the high rez ranges
    if (giHighPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
      pairValidSets.high -= nodataInt;
    }
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    // make an interval of it, and subtract it from the low rez ranges
    if (giLowPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giLowPair1.getNodataValue(), giLowPair1.getNodataValue());
      pairValidSets.low -= nodataInt;
    }
  }
  
  imagefusion::Image pairMask = baseMask;
  if (pairValidSets.hasHigh) //If there are any ranges given for the highrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date1), pairValidSets.high);
  pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date3), pairValidSets.high);
  if (pairValidSets.hasLow) //If there are any ranges given for the lowrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date1), pairValidSets.low);
  pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date3), pairValidSets.low);
  
  
  //Step 5: Predictions
  //Predict for desired Dates
  int n_outputs = pred_dates.size();
  for(int i=0; i< n_outputs;++i){
    //Get the destination filename
    std::string pred_filename=as<std::string>(pred_filenames[i]);
    
    //Make the Mask
    //We use the basic ranges
    auto predValidSets = baseValidSets;
    
    if (use_nodata_value) {
      if (!predValidSets.hasLow)
        predValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
      predValidSets.hasLow = true;
      if (giHighPair1.hasNodataValue()) { 
        imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
        predValidSets.low -= nodataInt;
      }
    }
    //For every prediction, we start out with the pairMask, which we obtained earlier through a combination of Date1 and Date3 masks 
    imagefusion::Image predMask = pairMask;
    
    //Apply the low res ranges, if there are any 
    //(pred will always be a high res image, so we only need the low ranges)
    if (predValidSets.hasLow)
      //Adjust the mask by also applying those ranges.
      predMask = helpers::processSetMask(std::move(predMask), mri->get(lowtag, pred_dates[i]), predValidSets.low);
    
    //Predict using the new mask we have made
      if(verbose){Rcout  <<"Predicting for date"<< pred_dates[i]<< " using both pairs from dates " << date1 << " and " << date3 << "." << std::endl;}
      esf.predict(pred_dates[i],predMask);  
      esf.outputImage().write(pred_filename);
    
    //Write the masks if desired
    if (output_masks){
      imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
      std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i], date3);}
    
    //Add the Geoinformation of the template to the written file
    // and adjust it, if we have used a pred area
    GeoInfo giTemplate {giHighPair1};
    if (giTemplate.hasGeotransform()) {
      giTemplate.geotrans.translateImage(pred_rectangle.x, pred_rectangle.y);
      if (pred_rectangle.width != 0)
        giTemplate.size.width = pred_rectangle.width;
      if (pred_rectangle.height != 0)
        giTemplate.size.height = pred_rectangle.height;
      giTemplate.addTo(pred_filename);
    }
  }
}





//===========================================starfm=================================

// [[Rcpp::export]]
void execute_starfm_job_cpp(CharacterVector input_filenames, 
                            CharacterVector input_resolutions, 
                            IntegerVector input_dates, 
                            IntegerVector pred_dates, 
                            CharacterVector pred_filenames,   
                            IntegerVector pred_area, 
                            int winsize,
                            int date1,
                            int date3,
                            int n_cores,
                            bool output_masks,
                            bool use_nodata_value,
                            bool use_strict_filtering,
                            bool use_temp_diff_for_weights,
                            bool do_copy_on_zero_diff,
                            bool double_pair_mode,
                            bool verbose,
                            double number_classes,
                            double logscale_factor,
                            double spectral_uncertainty,
                            double temporal_uncertainty,
                            const std::string& hightag,  
                            const std::string& lowtag,   
                            const std::string& MASKIMG_options,
                            const std::string& MASKRANGE_options
){
  

#ifndef WITH_OMP
  if(n_cores>1){
    Rcout <<"Sorry, if you want to use Parallelizer, you need to install OpenMP first."<<std::endl;
  }
#endif 
  using namespace imagefusion;
  //Step 1: Prepare the Input
  //create a prediction area rectangle
  Rectangle pred_rectangle = Rectangle{pred_area[0],pred_area[1],pred_area[2],pred_area[3]};
  
  // find the gi the first pair high image 
  Rcpp::LogicalVector is_high(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_high[i] = (input_resolutions[i] == hightag);}
  Rcpp::CharacterVector filenames_high = input_filenames[is_high];
  std::string high_template_filename = Rcpp::as<std::vector<std::string> >(filenames_high)[0];
  GeoInfo giHighPair1 {high_template_filename};
  if(verbose){Rcout <<"Getting High Resolution Geoinformation from File: "<<high_template_filename<<std::endl;}
  
  // find the gi the first pair low image 
  Rcpp::LogicalVector is_low(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_low[i] = (input_resolutions[i] == lowtag);}
  Rcpp::CharacterVector filenames_low = input_filenames[is_low];
  std::string low_template_filename = Rcpp::as<std::vector<std::string> >(filenames_low)[0];
  if(verbose){Rcout <<"Getting Low Resolution Geoinformation from File: "<<low_template_filename<<std::endl;}
  GeoInfo giLowPair1 {low_template_filename};
  
  // a copy of the high image gi, which we later use for the output (and might modify a bit)
  GeoInfo giTemplate {giHighPair1};
  
  
  
  //Step 2: Load the images and set the options
  //Load the images into a mri
  auto mri = std::make_shared<MultiResImages>();
  int n_inputs = input_filenames.size();
  for(int i=0; i< n_inputs;++i){
    mri->set(as<std::string>(input_resolutions[i]),
             input_dates[i],
                        Image(as<std::string>(input_filenames[i])));
  }
  
  //Pass the desired Options
  //required arguments
  StarfmOptions o;
  o.setHighResTag(hightag);
  o.setLowResTag(lowtag);
  if(double_pair_mode){
    o.setDoublePairDates(date1,date3);
  }else{
    o.setSinglePairDate(date1);
  }
  //optional arguments
  o.setWinSize(winsize);
  o.setPredictionArea(pred_rectangle);
  o.setLogScaleFactor(logscale_factor);
  o.setSpectralUncertainty(spectral_uncertainty);
  o.setTemporalUncertainty(temporal_uncertainty);
  o.setUseStrictFiltering(use_strict_filtering);
  o.setDoCopyOnZeroDiff(do_copy_on_zero_diff);
  o.setNumberClasses(number_classes);
  
  
  imagefusion::StarfmOptions::TempDiffWeighting tempDiffSetting = imagefusion::StarfmOptions::TempDiffWeighting::on_double_pair;
  if (use_temp_diff_for_weights){
    tempDiffSetting = imagefusion::StarfmOptions::TempDiffWeighting::enable;
  }else{
    tempDiffSetting = imagefusion::StarfmOptions::TempDiffWeighting::disable;
  }
  o.setUseTempDiffForWeights(tempDiffSetting);
  
    
  //Step 3: Create the Fusor
  //Create the Fusor

  //create a parallelizer options object if desired
#ifdef WITH_OMP
    ParallelizerOptions<StarfmOptions> po;
    po.setNumberOfThreads(n_cores);
    po.setAlgOptions(o);
    Parallelizer<StarfmFusor> sf;
    sf.srcImages(mri);
    sf.processOptions(po);
#else /* WITH_OMP not defined */
    StarfmFusor sf;
    sf.srcImages(mri); 
    sf.processOptions(o);
#endif /* WITH_OMP */
  

  

  
  
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
  //Gets the Mask if one was given in the options, otherwise create a base mask
  // from scratch. Uses first pair high image  as template.
  imagefusion::Image baseMask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, giHighPair1.channels, !maskoptions["MASKRANGE"].empty());
  
  //get ranges
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
    
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    //make an interval of it, and subtract it from the high rez ranges
    if (giHighPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
      pairValidSets.high -= nodataInt;
    }
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    // make an interval of it, and subtract it from the low rez ranges
    if (giLowPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giLowPair1.getNodataValue(), giLowPair1.getNodataValue());
      pairValidSets.low -= nodataInt;
    }
  }
  
  imagefusion::Image pairMask = baseMask;
  if (pairValidSets.hasHigh) //If there are any ranges given for the highrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date1), pairValidSets.high);
  if(double_pair_mode){pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date3), pairValidSets.high);}
  if (pairValidSets.hasLow) //If there are any ranges given for the lowrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date1), pairValidSets.low);
  if(double_pair_mode){pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date3), pairValidSets.low);}
  
  
  
  
  //Step 5: Predictions
  //Predict for desired Dates
  int n_outputs = pred_dates.size();
  for(int i=0; i< n_outputs;++i){
    //Get the destination filename
    std::string pred_filename=as<std::string>(pred_filenames[i]);
    
    //Make the Mask
    //We use the basic ranges
    auto predValidSets = baseValidSets;
    
    if (use_nodata_value) {
      if (!predValidSets.hasLow)
        predValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
      predValidSets.hasLow = true;
      if (giHighPair1.hasNodataValue()) { 
        imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
        predValidSets.low -= nodataInt;
      }
    }
    //For every prediction, we start out with the pairMask, which we obtained earlier through a combination of Date1 and Date3 masks 
    imagefusion::Image predMask = pairMask;
    
    //Apply the low res ranges, if there are any 
    //(pred will always be a high res image, so we only need the low ranges)
    if (predValidSets.hasLow)
      //Adjust the mask by also applying those ranges.
      predMask = helpers::processSetMask(std::move(predMask), mri->get(lowtag, pred_dates[i]), predValidSets.low);
    
    //Predict using the new mask we have made
    //If we have a parallelised fusor object, use that

      //OPTIONAL START
      if(verbose){Rcout  << "Predicting for date " << pred_dates[i];}
      if (o.isDoublePairModeConfigured())
        if(verbose){Rcout  << " using both pairs from dates " << date1 << " and " << date3 << "." << std::endl;}
      else {
        if(verbose){Rcout  << " using a single pair from date " << date1 << "." << std::endl;}
      }
      
      //OPTIONAL END
      sf.predict(pred_dates[i],predMask);  
      sf.outputImage().write(pred_filename);
    
    //Write the masks if desired
    if (output_masks){
      imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
      if(double_pair_mode){
        std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i], date3);
    }else{
      std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i], date1);
      }
    }
    //Add the Geoinformation of the template to the written file
    // and adjust it, if we have used a pred area
    GeoInfo giTemplate {giHighPair1};
    if (giTemplate.hasGeotransform()) {
      giTemplate.geotrans.translateImage(pred_rectangle.x, pred_rectangle.y);
      if (pred_rectangle.width != 0)
        giTemplate.size.width = pred_rectangle.width;
      if (pred_rectangle.height != 0)
        giTemplate.size.height = pred_rectangle.height;
      giTemplate.addTo(pred_filename);
    }
  }
}



//===========================================fitfc=================================

// [[Rcpp::export]]
void execute_fitfc_job_cpp(CharacterVector input_filenames, 
                            CharacterVector input_resolutions, 
                            IntegerVector input_dates, 
                            IntegerVector pred_dates, 
                            CharacterVector pred_filenames,   
                            IntegerVector pred_area, 
                            int winsize,
                            int date1,
                            int n_cores,
                            int n_neighbors,
                            bool output_masks,
                            bool use_nodata_value,
                            bool verbose,
                            double resolution_factor, 
                            const std::string& hightag,  
                            const std::string& lowtag,   
                            const std::string& MASKIMG_options,
                            const std::string& MASKRANGE_options
){
  
#ifndef WITH_OMP
  if(n_cores>1){
    Rcout <<"Sorry, if you want to use Parallelizer, you need to install OpenMP first."<<std::endl;
  }
#endif 
  using namespace imagefusion;
  //Step 1: Prepare the Input
  //create a prediction area rectangle
  Rectangle pred_rectangle = Rectangle{pred_area[0],pred_area[1],pred_area[2],pred_area[3]};
  
  
  
  // find the gi the first pair high image 
  Rcpp::LogicalVector is_high(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_high[i] = (input_resolutions[i] == hightag);}
  Rcpp::CharacterVector filenames_high = input_filenames[is_high];
  std::string high_template_filename = Rcpp::as<std::vector<std::string> >(filenames_high)[0];
  GeoInfo giHighPair1 {high_template_filename};
  if(verbose){Rcout <<"Getting High Resolution Geoinformation from File: "<<high_template_filename<<std::endl;}
  
  // find the gi the first pair low image 
  Rcpp::LogicalVector is_low(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_low[i] = (input_resolutions[i] == lowtag);}
  Rcpp::CharacterVector filenames_low = input_filenames[is_low];
  std::string low_template_filename = Rcpp::as<std::vector<std::string> >(filenames_low)[0];
  if(verbose){Rcout <<"Getting Low Resolution Geoinformation from File: "<<low_template_filename<<std::endl;}
  GeoInfo  giLowPair1 {low_template_filename};
  
  // a copy of the high image gi, which we later use for the output (and might modify a bit)
  GeoInfo giTemplate {giHighPair1};
  
  
  
  //Step 2: Load the images and set the options
  //Load the images into a mri
  auto mri = std::make_shared<MultiResImages>();
  int n_inputs = input_filenames.size();
  for(int i=0; i< n_inputs;++i){
    mri->set(as<std::string>(input_resolutions[i]),
             input_dates[i],
                        Image(as<std::string>(input_filenames[i])));
  }
  
  //Pass the desired Options
  //required arguments
  FitFCOptions o;
  o.setHighResTag(hightag);
  o.setLowResTag(lowtag);
  o.setPairDate(date1);
  
  //optional arguments
  o.setWinSize(winsize);
  o.setPredictionArea(pred_rectangle);
  o.setNumberNeighbors(n_neighbors);
  o.setResolutionFactor(resolution_factor);

    
  
  //Step 3: Create the Fusor
  //create a parallelizer options object if desired
#ifdef WITH_OMP
  ParallelizerOptions<FitFCOptions> po;
  po.setNumberOfThreads(n_cores);
  po.setAlgOptions(o);
  Parallelizer<StarfmFusor> ffc;
  ffc.srcImages(mri);
  ffc.processOptions(po);
#else /* WITH_OMP not defined */    
  //Create the Fusor
  FitFCFusor ffc;
  ffc.srcImages(mri); 
  ffc.processOptions(o);
#endif /* WITH_OMP */


  
  
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
  //Gets the Mask if one was given in the options, otherwise create a base mask
  // from scratch. Uses first pair high image  as template.
  imagefusion::Image baseMask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, giHighPair1.channels, !maskoptions["MASKRANGE"].empty());
  
  //get ranges
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
    
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    //make an interval of it, and subtract it from the high rez ranges
    if (giHighPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
      pairValidSets.high -= nodataInt;
    }
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    // make an interval of it, and subtract it from the low rez ranges
    if (giLowPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giLowPair1.getNodataValue(), giLowPair1.getNodataValue());
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

  
  
  
  //Step 5: Predictions
  //Predict for desired Dates
  int n_outputs = pred_dates.size();
  for(int i=0; i< n_outputs;++i){
    //Get the destination filename
    std::string pred_filename=as<std::string>(pred_filenames[i]);
    
    //Make the Mask
    //We use the basic ranges
    auto predValidSets = baseValidSets;
    
    if (use_nodata_value) {
      if (!predValidSets.hasLow)
        predValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
      predValidSets.hasLow = true;
      if (giHighPair1.hasNodataValue()) { 
        imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
        predValidSets.low -= nodataInt;
      }
    }
    //For every prediction, we start out with the pairMask, which we obtained earlier through a combination of Date1 and Date3 masks 
    imagefusion::Image predMask = pairMask;
    
    //Apply the low res ranges, if there are any 
    //(pred will always be a high res image, so we only need the low ranges)
    if (predValidSets.hasLow)
      //Adjust the mask by also applying those ranges.
      predMask = helpers::processSetMask(std::move(predMask), mri->get(lowtag, pred_dates[i]), predValidSets.low);
    
    //Predict using the new mask we have made
    //If we have a parallelised fusor object, use that
      if(verbose){Rcout  << "Predicting for date " << pred_dates[i];}
      if(verbose){Rcout  << " using pair from date " << date1<< std::endl;}
      ffc.predict(pred_dates[i],predMask);  
      ffc.outputImage().write(pred_filename);
    
    //Write the masks if desired
    if (output_masks){
      imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
      std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i]);}
    
    //Add the Geoinformation of the template to the written file
    // and adjust it, if we have used a pred area
    GeoInfo giTemplate {giHighPair1};
    if (giTemplate.hasGeotransform()) {
      giTemplate.geotrans.translateImage(pred_rectangle.x, pred_rectangle.y);
      if (pred_rectangle.width != 0)
        giTemplate.size.width = pred_rectangle.width;
      if (pred_rectangle.height != 0)
        giTemplate.size.height = pred_rectangle.height;
      giTemplate.addTo(pred_filename);
    }
  }
}


//===========================================spstfm=================================
// [[Rcpp::export]]
void execute_spstfm_job_cpp(CharacterVector input_filenames, 
                             CharacterVector input_resolutions, 
                             IntegerVector input_dates, 
                             IntegerVector pred_dates, 
                             CharacterVector pred_filenames,   
                             IntegerVector pred_area, 
                             int date1,
                             int date3,
                             int n_cores,
                             int dict_size,
                             int n_training_samples,
                             int patch_size,
                             int patch_overlap,
                             int min_train_iter,
                             int max_train_iter,
                             bool output_masks,
                             bool use_nodata_value,
                             bool random_sampling,
                             bool verbose,
                             const std::string& hightag,  
                             const std::string& lowtag,   
                             const std::string& MASKIMG_options,
                             const std::string& MASKRANGE_options,
                             const std::string& LOADDICT_options,
                             const std::string& SAVEDICT_options,
                             const std::string& REUSE_options
                             
)
{
#ifndef WITH_OMP
  if(n_cores>1){
    Rcout <<"Sorry, if you want to use Parallelizer, you need to install OpenMP first."<<std::endl;
  }
#endif 
  
  
  using namespace imagefusion;
  //Step 1: Prepare the Input
  //create a prediction area rectangle
  Rectangle pred_rectangle = Rectangle{pred_area[0],pred_area[1],pred_area[2],pred_area[3]};
  
  // check spelling of reuse dictionary option
  std::string reuseOpt = REUSE_options;
  if (reuseOpt != "improve" && reuseOpt != "clear" && reuseOpt != "use") {
    std::cerr << "For --dict-reuse you must either give 'improve', 'clear' or 'use'. You gave " << reuseOpt << "." << std::endl;
  }
  
  // find the gi the first pair high image 
  Rcpp::LogicalVector is_high(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_high[i] = (input_resolutions[i] == hightag);}
  Rcpp::CharacterVector filenames_high = input_filenames[is_high];
  std::string high_template_filename = Rcpp::as<std::vector<std::string> >(filenames_high)[0];
  GeoInfo  giHighPair1 {high_template_filename};
  if(verbose){Rcout <<"Getting High Resolution Geoinformation from File: "<<high_template_filename<<std::endl;}
  
  // find the gi the first pair low image 
  Rcpp::LogicalVector is_low(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_low[i] = (input_resolutions[i] == lowtag);}
  Rcpp::CharacterVector filenames_low = input_filenames[is_low];
  std::string low_template_filename = Rcpp::as<std::vector<std::string> >(filenames_low)[0];
  if(verbose){Rcout <<"Getting Low Resolution Geoinformation from File: "<<low_template_filename<<std::endl;}
  GeoInfo giLowPair1 {low_template_filename};
  
  
  
  
  
  //Step 2: Load the images and set the options
  
  //Load the images into a mri
  auto mri = std::make_shared<MultiResImages>();
  int n_inputs = input_filenames.size();
  for(int i=0; i< n_inputs;++i){
    mri->set(as<std::string>(input_resolutions[i]),
             input_dates[i],
                        Image(as<std::string>(input_filenames[i])));
  }
  
  //Pass the desired Options
  //required arguments
  imagefusion::SpstfmOptions o;
  o.setHighResTag(hightag);
  o.setLowResTag(lowtag);
  o.setDate1(date1);
  o.setDate3(date3);
  //optional arguments
  o.setPredictionArea(pred_rectangle);
  o.setDictSize(dict_size);
  o.setNumberTrainingSamples(n_training_samples);
  o.setPatchSize(patch_size);
  o.setPatchOverlap(patch_overlap);
  o.setMinTrainIter(min_train_iter);
  o.setMaxTrainIter(max_train_iter);
  if(random_sampling){
    o.setSamplingStrategy(imagefusion::SpstfmOptions::SamplingStrategy::random);
  }else{
    o.setSamplingStrategy(imagefusion::SpstfmOptions::SamplingStrategy::variance);
  }
  

  
  //Step 3: Create the Fusor
  //Create the Fusor
  SpstfmFusor spsf;
  spsf.srcImages(mri); 

  
  
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
    {"SAVEDICT",      "",            "s", "save-dict",                    ArgChecker::NonEmpty,    "  -s <outfile>, --save-dict=<outfile> \tSave the dictionary after the last training to a file. This can be used later on with --load-dict=outfile and, if you do not want to improve it, --dict-reuse=use.\n"},
    {"LOADDICT",      "",            "l", "load-dict",                    ArgChecker::NonEmpty,    "  -l <file>, --load-dict=<file> \tLoad dictionary from a file, which has been written with --save-dict before. You can give the filename you specified with --save-dict, even if there have been generated numbers in the filename (which happens in case of multi-channel images). Do not specify multiple dictionaries. Only the last will be used otherwise.\n"},
    
    Descriptor::optfile() // 
  };
  
  //LOAD DICT
  // load dictionary from file
  if (!LOADDICT_options.empty()) {
    std::string dictPath = LOADDICT_options;
    unsigned int chans = mri->getAny().channels();
    if (chans == 1) {
      if (!std::filesystem::exists(dictPath)) {
        std::cerr << "Could not find the dictionary file " << dictPath << " to load a single-channel dictionary." << std::endl;
      }
      
      arma::mat dict;
      bool loadOK = dict.load(dictPath);
      if (loadOK) {
        if(verbose){Rcout  << "Using dictionary from " << dictPath << "." << std::endl;}
        spsf.setDictionary(dict);
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
        }
        
        arma::mat dict;
        bool loadOK = dict.load(infilename);
        if (loadOK) {
          if(verbose){Rcout  << "Using dictionary from " << infilename << " for channel " << c << "." << std::endl;}
          spsf.setDictionary(dict, c);
        }
        else {
          std::cerr << "Could not load dictionary from " << infilename << " although the file exists. Defect file? Ignoring option --load-dict completely." << std::endl;
          // remove eventual dictionaries set to previous channels
          for (unsigned int i = 0; i < chans; ++i)
            spsf.setDictionary(dict, i);
          break;
        }
      }
    }
  }
  
  
  //MASK
  //First, parse the MASKIMG options, if given
  auto maskoptions = imagefusion::option::OptionParser::parse(usage,MASKIMG_options);
  std::vector<std::string> maskImgArgs;
  for (auto const& opt : maskoptions["MASKIMG"])
    maskImgArgs.push_back(opt.arg);
  //Gets the Mask if one was given in the options, otherwise create a base mask
  // from scratch. Uses first pair high image  as template.
  imagefusion::Image baseMask = helpers::parseAndCombineMaskImages<Parse>(maskImgArgs, giHighPair1.channels, !maskoptions["MASKRANGE"].empty());
  
  //get ranges
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
    
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    //make an interval of it, and subtract it from the high rez ranges
    if (giHighPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
      pairValidSets.high -= nodataInt;
    }
    //Then we get the NoDataValue from the GeoInfo (if there is one),
    // make an interval of it, and subtract it from the low rez ranges
    if (giLowPair1.hasNodataValue()) {
      imagefusion::Interval nodataInt = imagefusion::Interval::closed(giLowPair1.getNodataValue(), giLowPair1.getNodataValue());
      pairValidSets.low -= nodataInt;
    }
  }
  
  imagefusion::Image pairMask = baseMask;
  if (pairValidSets.hasHigh) //If there are any ranges given for the highrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date1), pairValidSets.high);
  pairMask = helpers::processSetMask(std::move(pairMask), mri->get(hightag, date3), pairValidSets.high);
  if (pairValidSets.hasLow) //If there are any ranges given for the lowrez :
    //apply the ranges and update the mask accordingly
    pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date1), pairValidSets.low);
  pairMask = helpers::processSetMask(std::move(pairMask), mri->get(lowtag,  date3), pairValidSets.low);
  
  
  //Step 6: Training
  // train dictionary (if there is one from a previous time series, improve it)
  if(verbose){Rcout  << "Training with dates " << date1 << " and " << date3 << std::endl;}
  if (reuseOpt == "improve")
    o.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::improve);
  else if (reuseOpt == "clear")
    o.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::clear);
  else // (reuseOpt == "use")
    o.setDictionaryReuse(imagefusion::SpstfmOptions::ExistingDictionaryHandling::use);
  
  spsf.processOptions(o);
  spsf.train(pairMask);
  
  
  //Step 6: Predictions
  //Predict for desired Dates
  int n_outputs = pred_dates.size();
  for(int i=0; i< n_outputs;++i){
    //Get the destination filename
    std::string pred_filename=as<std::string>(pred_filenames[i]);
    
    //Make the Mask
    //We use the basic ranges
    auto predValidSets = baseValidSets;
    
    if (use_nodata_value) {
      if (!predValidSets.hasLow)
        predValidSets.low  += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
      predValidSets.hasLow = true;
      if (giHighPair1.hasNodataValue()) { 
        imagefusion::Interval nodataInt = imagefusion::Interval::closed(giHighPair1.getNodataValue(), giHighPair1.getNodataValue());
        predValidSets.low -= nodataInt;
      }
    }
    //For every prediction, we start out with the pairMask, which we obtained earlier through a combination of Date1 and Date3 masks 
    imagefusion::Image predMask = pairMask;
    
    //Apply the low res ranges, if there are any 
    //(pred will always be a high res image, so we only need the low ranges)
    if (predValidSets.hasLow)
      //Adjust the mask by also applying those ranges.
      predMask = helpers::processSetMask(std::move(predMask), mri->get(lowtag, pred_dates[i]), predValidSets.low);
    if(verbose){Rcout  <<"Predicting for date"<< pred_dates[i]<< " using both pairs from dates " << date1 << " and " << date3 << "." << std::endl;}
    
    spsf.predict(pred_dates[i],predMask);  
    spsf.outputImage().write(pred_filename);
    
    //Write the masks if desired
    if (output_masks){
      imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
      std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i], date3);}
    
    //Add the Geoinformation of the template to the written file
    // and adjust it, if we have used a pred area
    GeoInfo giTemplate {giHighPair1};
    if (giTemplate.hasGeotransform()) {
      giTemplate.geotrans.translateImage(pred_rectangle.x, pred_rectangle.y);
      if (pred_rectangle.width != 0)
        giTemplate.size.width = pred_rectangle.width;
      if (pred_rectangle.height != 0)
        giTemplate.size.height = pred_rectangle.height;
      giTemplate.addTo(pred_filename);
    }
  }
  // save dictionary to file
  if (!SAVEDICT_options.empty()) {
    bool success = true;
    std::string dictPath = SAVEDICT_options;
    unsigned int chans = mri->getAny().channels();
    if (chans == 1) {
      success = spsf.getDictionary().save(dictPath);
      if (success)
        if(verbose){Rcout  << "Saved dictionary to " << dictPath << "." << std::endl;}
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
        bool saved = spsf.getDictionary(c).save(outfilename);
        if (saved)
          if(verbose){Rcout  << "Saved dictionary for channel " << c << " to " << outfilename << "." << std::endl;}
        success &= saved;
      }
      
      if (success)
        if(verbose){Rcout  << "For loading the dictionaries later on, you can still use --load-dict=" << dictPath << "." << std::endl;}
    }
  }
  
}





#include <Rcpp.h>
#include "Starfm.h"
#include "Estarfm.h"
#include "fitfc.h"
#include "spstfm.h"
#include "utils_common.h"
#include "MultiResImages.h"

#ifdef WITH_OMP
#include "Parallelizer.h"
#include "ParallelizerOptions.h"
#endif /* WITH_OMP */

using namespace Rcpp;

using  std::vector;

//TO DO:
//ADD VERBOSE OPTION
//CLEAN UP CODE INTO SENSIBLE CHAPTERS
//ADD PARALLELISATION

//===========================================starfm=================================
//' @export
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
                             bool use_parallelisation,
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
  if(use_parallelisation){
#ifndef WITH_OMP
    std::cout<<"Sorry, if you want to use Parallelizer, you need to install OpenMP first."<<std::endl;
    use_parallelisation = false;
#endif
  }
  
  
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
  GeoInfo const& giHighPair1 {high_template_filename};
  //std::cout<<"Getting High Resolution Geoinformation from File: "<<high_template_filename<<std::endl;
  
  // find the gi the first pair low image 
  Rcpp::LogicalVector is_low(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_low[i] = (input_resolutions[i] == lowtag);}
  Rcpp::CharacterVector filenames_low = input_filenames[is_low];
  std::string low_template_filename = Rcpp::as<std::vector<std::string> >(filenames_low)[0];
  //std::cout<<"Getting Low Resolution Geoinformation from File: "<<low_template_filename<<std::endl;
  GeoInfo const& giLowPair1 {low_template_filename};
  
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
  
  //create a parallelizer options object if desired
#ifdef WITH_OMP
  if(use_parallelisation){
    ParallelizerOptions<EstarfmOptions> po;
    po.setNumberOfThreads(n_cores);
    po.setAlgOptions(o);
  }
#endif /* WITH_OMP */
  
  
  //Step 3: Create the Fusor
  //Create the Fusor
  EstarfmFusor esf;
  esf.srcImages(mri); 
  esf.processOptions(o);
#ifdef WITH_OMP
  if(use_parallelisation){
    Parallelizer<StarfmFusor> pesf;
    pesf.srcImages(mri);
    pesf.processOptions(po);
  }
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
    //If we have a parallelised fusor object, use that
    if(use_parallelisation){
#ifdef WITH_OMP
      pesf.predict(pred_dates[i],predMask);
      pesf.outputImage().write(pred_filename);
#endif /* Otherwise, use the standard fusor object*/
    }else{
      esf.predict(pred_dates[i],predMask);  
      esf.outputImage().write(pred_filename);
    }
    
    //Write the masks if desired
    if (output_masks){
      imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
      std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i], date3);}
    
    //Add the Geoinformation of the template to the written file
    // and adjust it, if we have used a pred area
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

//' @export
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
                            bool use_parallelisation,
                            bool use_strict_filtering,
                            bool use_temp_diff_for_weights,
                            bool do_copy_on_zero_diff,
                            bool double_pair_mode,
                            double number_classes,
                            double data_range_min,
                            double data_range_max,
                            double logscale_factor,
                            double spectral_uncertainty,
                            double temporal_uncertainty,
                            const std::string& hightag,  
                            const std::string& lowtag,   
                            const std::string& MASKIMG_options,
                            const std::string& MASKRANGE_options
){
  
  if(use_parallelisation){
#ifndef WITH_OMP
    std::cout<<"Sorry, if you want to use Parallelizer, you need to install OpenMP first."<<std::endl;
    use_parallelisation = false;
#endif
  }
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
  GeoInfo const& giHighPair1 {high_template_filename};
  //std::cout<<"Getting High Resolution Geoinformation from File: "<<high_template_filename<<std::endl;
  
  // find the gi the first pair low image 
  Rcpp::LogicalVector is_low(input_filenames.size());
  for( int i=0; i<input_filenames.size(); i++){
    is_low[i] = (input_resolutions[i] == lowtag);}
  Rcpp::CharacterVector filenames_low = input_filenames[is_low];
  std::string low_template_filename = Rcpp::as<std::vector<std::string> >(filenames_low)[0];
  //std::cout<<"Getting Low Resolution Geoinformation from File: "<<low_template_filename<<std::endl;
  GeoInfo const& giLowPair1 {low_template_filename};
  
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
  o.setDoublePairDates(date1,date3);
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
    
    
    //create a parallelizer options object if desired
#ifdef WITH_OMP
    if(use_parallelisation){
      ParallelizerOptions<EstarfmOptions> po;
      po.setNumberOfThreads(n_cores);
      po.setAlgOptions(o);
    }
#endif /* WITH_OMP */
    
    
    //Step 3: Create the Fusor
    //Create the Fusor
    StarfmFusor sf;
    sf.srcImages(mri); 
    sf.processOptions(o);
    
#ifdef WITH_OMP
    if(use_parallelisation){
      Parallelizer<StarfmFusor> psf;
      psf.srcImages(mri);
      psf.processOptions(po);
    }
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
      //If we have a parallelised fusor object, use that
      if(use_parallelisation){
#ifdef WITH_OMP
        psf.predict(pred_dates[i],predMask);
        psf.outputImage().write(pred_filename);
#endif /* Otherwise, use the standard fusor object*/
      }else{
        sf.predict(pred_dates[i],predMask);  
        sf.outputImage().write(pred_filename);
      }
      
      //Write the masks if desired
      if (output_masks){
        imagefusion::FileFormat outformat = imagefusion::FileFormat::fromFile(pred_filename);
        std::string outmaskfilename = helpers::outputImageFile(predMask, giHighPair1, "MaskImage", pred_filename, "MaskImage", outformat, date1, pred_dates[i], date3);}
      
      //Add the Geoinformation of the template to the written file
      // and adjust it, if we have used a pred area
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


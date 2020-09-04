#' Execute a single self-contained self-contained time-series imagefusion job using STARFM
#' @description A wrapper function for \code{execute_starfm_job_cpp}. Intended to execute a single job, that is a number of predictions based on the same input pair(s). It ensures that all of the arguments passed are of the correct type and creates sensible defaults. 
#'
#' @param input_filenames  A string vector containing the filenames of the input images
#' @param input_resolutions A string vector containing the resolution-tags (corresponding to the arguments \code{hightag} and \code{lowtag}, which are by default "high" and "low") of the input images.
#' @param input_dates An integer vector containing the dates of the input images.
#' @param pred_dates An integer vector containing the dates for which images should be predicted.
#' @param pred_filenames A string vector containing the filenames for the predicted images. Must match \code{pred_dates} in length and order. Must include an extension relating to one of the \href{https://gdal.org/drivers/raster/index.html}{drivers supported by GDAL}, such as ".tif".
#' @param pred_area (Optional) An integer vector containing parameters in image coordinates for a bounding box which specifies the prediction area. The prediction will only be done in this area. (x_min, y_min, width, height). By default will use the entire area of the first input image.
#' @param winsize (Optional) Window size of the rectangle around the current pixel. Default is 51.
#' @param date1 (Optional) Set the date of the first input image pair. By default, will use the pair with the lowest date value.
#' @param date3 (Optional) Set the date of the second input image pair. By default, will use the pair with the highest date value. Disregarded if using double pair mode.
#' @param n_cores (Optional) Set the number of cores to use when using parallelisation. Default is 1.
#' @param logscale_factor (Optional) When using a positive scale, the logistic weighting formula is used, which reduces the influence of spectral and temporal differences. Default is 0, i. e. logistic formula not used
#' @param spectral_uncertainty (Optional) This spectral uncertainty value will influence the spectral difference value. Default is 1 for 8 bit images (INT1U and INT1S), 50 otherwise.
#' @param temporal_uncertainty (Optional) This spectral uncertainty value will influence the spectral difference value. Default is 1 for 8 bit images (INT1U and INT1S), 50 otherwise.
#' @param number_classes  (Optional) The number of classes used for similarity. Note all channels of a pixel are considered for similarity. So this value holds for each channel, e. g. with 3 channels there are n^3 classes. Default: 40.
#' @param hightag (Optional) A string which is used in \code{input_resolutions} to describe the high-resolution images. Default is "high".
#' @param lowtag (Optional) A string which is used in \code{input_resolutions} to describe the low-resolution images.  Default is "low".
#' @param MASKIMG_options (Optional) A string containing information for a mask image (8-bit, boolean, i. e. consists of 0 and 255). "For all input images the pixel values at the locations where the mask is 0 is replaced by the mean value." Example: \code{--mask-img=some_image.png}
#' @param MASKRANGE_options (Optional) Specify one or more intervals for valid values. Locations with invalid values will be masked out. Ranges should be given in the format '[<float>,<float>]', '(<float>,<float>)', '[<float>,<float>' or '<float>,<float>]'. There are a couple of options:' \itemize{
##'  \item{"--mask-valid-ranges"}{ Intervals which are marked as valid. Valid ranges can excluded from invalid ranges or vice versa, depending on the order of options.}
##'  \item{"--mask-invalid-ranges"}{ Intervals which are marked as invalid. Invalid intervals can be excluded from valid ranges or vice versa, depending on the order of options.}
##'  \item{"--mask-high-res-valid-ranges"}{ This is the same as --mask-valid-ranges, but is applied only for the high resolution images.}
##'  \item{"--mask-high-res-invalid-ranges"}{ This is the same as --mask-invalid-ranges, but is applied only for the high resolution images.}
##'  \item{"--mask-low-res-valid-ranges"}{ This is the same as --mask-valid-ranges, but is applied only for the low resolution images.}
##'  \item{"--mask-low-res-invalid-ranges"}{ This is the same as --mask-invalid-ranges, but is applied only for the low resolution images.}
##' }
#' @param output_masks  (Optional) Write mask images to disk? Default is "false".
#' @param use_nodata_value (Optional) Use the nodata value as invalid range for masking? Default is "true".
#' @param use_parallelisation (Optional) Use parallelisation when possible? Default is "false".
#' @param use_strict_filtering (Optional) Use strict filtering, which means that candidate pixels will be accepted only if they have less temporal *and* spectral difference than the central pixel (like in the paper). Default is "false".
#' @param double_pair_mode (Optional) Use two dates \code{date1} and \code{date3} for prediction, instead of just \code{date1} for all predictions? Default is "true" if *all* the pred dates are in between input pairs, and "false" otherwise. Note: It may be desirable to predict in double-pair mode where possible, as in the following example: \code{[(7) 10 12 (13) 14] } , where we may wish to predict 10 and 12 in double pair mode, but can only predict 14 in single-pair mode. Do achieve this it is necessary to split the task into different jobs.
#' @param use_temp_diff_for_weights (Optional) Use temporal difference in the candidates weight (like in the paper)? Default is to use temporal weighting in double pair mode, and to not use it in single pair mode.
#' @param do_copy_on_zero_diff (Optional) Predict for all pixels, even for pixels with zero temporal or spectral difference (behaviour of the reference implementation). Default is "false".
#' @references Gao, Feng, et al. "On the blending of the Landsat and MODIS surface reflectance: Predicting daily Landsat surface reflectance." IEEE Transactions on Geoscience and Remote sensing 44.8 (2006): 2207-2218.
#' @return Nothing. Output files are written to disk. The Geoinformation for the output images is adopted from the first input pair images.
#' @export
#' @importFrom raster stack dataType
#' @importFrom assertthat assert_that 
#' @author Christof Kaufmann (C++)
#' @author Johannes Mast (R)
#' @details Executes the STARFM algorithm to create a number of synthetic high-resolution images from either two pairs (double pair mode) or one pair (single pair mode) of matching high- and low-resolution images. Assumes that the input images already have matching size. See the original paper for details (TO DO: INSERT CHANGES WITH REGARDS TO THE ORIGINAL PAPER). \itemize{
##'  \item{  For the weighting (10) states: \eqn{C = S T D}  but we use  \eqn{C = (S+1)(T+1)D}, according to the reference implementation. With \code{logscale_factor}, the weighting formula can be changed to \eqn{C = ln{(Sb+2)}ln{(Tb+1)D}}}
##'  \item{ In addition to the temporal uncertainty \eqn{\sigma_t} (see \code{temporal_uncertainty}) and the spectral uncertainty\eqn{ \sigma_s} (see \code{spectral_uncertainty}) there will be used a *combined uncertainty* \eqn{\sigma_c := \sqrt{\sigma_t^2 + \sigma_s^2} }. This will be used in the candidate weighting: If \eqn{(S + 1) \, (T + 1) < \sigma_c }, then \eqn{C = 1 } instead of the formula above.}
##'  \item{Considering candidate weighting again, there is an option \code{use_tempdiff_for_weights} to not use the temporal difference for the weighting (also not for the combined uncertainty check above), i. e. T = 0 then. This is also the default behaviour.}{ }
##'  \item{The basic assumption of the original paper that with zero spectral or temporal difference the central pixel will be chosen is wrong since there might be multiple pixels with zero difference within one window. Also due to the addition of 1 to the spectral and temporal differences, the weight will not increase so dramatical from a zero difference. However, these assumptions can be enforced with \code{do_copy_on_zero_diff}, which is the default behaviour.}{ }
##'  \item{The paper states that a good candidate should satisfy (15) and (16). This can be set with use_strict_filtering, which is by default used. However the other behaviour, that a candidate should fulfill (15) or (16), as in the reference implementation, can be also be selected with that option.}
##'  \item{The paper uses max in (15) and (16), which would choose the largest spectral and temporal difference from all input pairs (only one or two are possible). Since this should filter out bad candidates, we believe this is a mistake and should be min instead of max, like it is done in the reference implementation. So this implementation uses min here.}
##' }
#' @examples Sorry, maybe later
#' @family {fusion_algorithms}
#' 
starfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area,winsize,date1,date3,n_cores, logscale_factor,spectral_uncertainty, temporal_uncertainty, number_classes,hightag,lowtag,MASKIMG_options,MASKRANGE_options,output_masks,use_nodata_value,use_parallelisation,use_strict_filtering,double_pair_mode,use_temp_diff_for_weights,do_copy_on_zero_diff,verbose=T) {
  library(assertthat)
  library(raster)
  
  ##### A: Check all the Optional Inputs #####
  #These are variables which are optional 
  # or can easily infered from the required inputs
  
  #### pred_area ####
  # check pred area.
  # if not provided by user, set pred area to max image size of first image
  # TO DO: MAKE SURE THAT THE SPECIFIED IMAGE COORDINATES FIT INTO THE BOUNDING BOX
  # TO DO: ADD AN OPTION TO USE GEOCOORDINATES
  #Use the first image as a template #TO DO: MAY MAKE MORE SENSE TO USE THE FIRST LOW-ONLY IMAGE AS TEMPLATE
  template <- raster::stack(input_filenames[1])
  #If a bbox was provided, check it for plausibility and pass it on 
  if(!missing(pred_area)){
    assert_that(
      length(pred_area)==4,
      class(pred_area)=="numeric",
      pred_area[1]+pred_area[3]<=template@ncols,
      pred_area[2]+pred_area[4]<=template@nrows
    )
    pred_area_c <- pred_area
    
  }else{
    print("No Prediction Area specified. Predicting entire extent of:")
    print(template[[1]]@file@name)
    pred_area_c <- c(0,0,template@ncols,template@nrows)
    #pred_area_c <- template@extent[c(1,3,2,4)]  #Geocoordinates
  }
  
  #### winsize ####
  if(!missing(winsize)){
    assert_that(class(winsize)=="numeric")
    winsize_c <- winsize
  }else{
    winsize_c <- 51
  }
  
  
  #### output_masks ####
  if(!missing(output_masks)){
    assert_that(class(output_masks)=="logical")
    output_masks_c <- output_masks
  }else{
    output_masks_c <- FALSE
  } 
  
  #### use_nodata_value ####
  if(!missing(use_nodata_value)){
    assert_that(class(use_nodata_value)=="logical")
    use_nodata_value_c <- use_nodata_value
  }else{
    use_nodata_value_c <- TRUE
  } 
  
  #### use_parallelisation ####
  if(!missing(use_parallelisation)){
    assert_that(class(use_parallelisation)=="logical")
    use_parallelisation_c <- use_parallelisation
  }else{
    use_parallelisation_c <- FALSE
  }   
  
  #### use_strict_filtering ####
  if(!missing(use_strict_filtering)){
    assert_that(class(use_strict_filtering)=="logical")
    use_strict_filtering_c <- use_strict_filtering
  }else{
    use_strict_filtering_c <- FALSE
  } 
  

  
  
  #### do_copy_on_zero_diff ####
  if(!missing(do_copy_on_zero_diff)){
    assert_that(class(do_copy_on_zero_diff)=="logical")
    do_copy_on_zero_diff_c <- do_copy_on_zero_diff
  }else{
    do_copy_on_zero_diff_c <- FALSE
  }
  
  
  #### number_classes ####
  if(!missing(number_classes)){
    assert_that(class(number_classes)=="numeric")
    number_classes_c <- number_classes
  }else{
    number_classes_c <- 40
  }
  
  #### logscalefactor ####
  if(!missing(logscale_factor)){
    assert_that(class(logscale_factor)=="numeric")
    logscale_factor_c <- logscale_factor
  }else{
    logscale_factor_c <- 0
  }
  
  #### spectral_uncertainty ####
  if(!missing(spectral_uncertainty)){
    assert_that(class(spectral_uncertainty)=="numeric")
    spectral_uncertainty_c <- spectral_uncertainty
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U" | template_dataType=="INT1S"){
      spectral_uncertainty_c <- 1
    }else{
      spectral_uncertainty_c <- 50
      }
  }
  #### temporal_uncertainty ####
  if(!missing(temporal_uncertainty)){
    assert_that(class(temporal_uncertainty)=="numeric")
    temporal_uncertainty_c <- temporal_uncertainty
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U" | template_dataType=="INT1S"){
      temporal_uncertainty_c <- 1
    }else{
      temporal_uncertainty_c <- 50
    }
  }
  
  #### hightag and lowtag####
  if(!missing(hightag)){
    assert_that(class(hightag)=="character")
    hightag_c <- hightag
  }else{
    hightag_c <- "high"
  }
  
  if(!missing(lowtag)){
    assert_that(class(lowtag)=="character")
    lowtag_c <- lowtag
  }else{
    lowtag_c <- "low"
  }
  
  
  # #### double_pair_mode ####
  if(!missing(double_pair_mode)){
    assert_that(class(double_pair_mode)=="logical")
    double_pair_mode_c <- double_pair_mode
  }else{
    #Check if our pred dates are between two pair dates
    high_dates <- input_dates[input_resolutions==hightag_c]
    low_dates <- input_dates[input_resolutions==lowtag_c]
    pair_dates <- as.numeric(names(table(c(unique(high_dates),unique(low_dates))))[which(table(c(unique(high_dates),unique(low_dates)))>=2)])
    if(length(pair_dates)>=2 & any(!pred_dates>max(pair_dates)) & any(!pred_dates<min(pair_dates))){
      double_pair_mode_c = TRUE
    }else{
      double_pair_mode_c = FALSE
    }
  }
  #### use_temp_diff_for_weights ####
  if(!missing(use_temp_diff_for_weights)){
    assert_that(class(use_temp_diff_for_weights)=="logical")
    use_temp_diff_for_weights_c <- use_temp_diff_for_weights
  }else{
    use_temp_diff_for_weights_c <- double_pair_mode_c
  } 
  
  #### maskimg options #### 
  if(!missing(MASKIMG_options)){
    assert_that(class(MASKIMG_options)=="character")
    MASKIMG_options_c <- MASKIMG_options
  }else{
    MASKIMG_options_c <- ""
  }
  
  #### maskrange options####
  if(!missing(MASKRANGE_options)){
    assert_that(class(MASKRANGE_options)=="character")
    MASKRANGE_options_c <- MASKRANGE_options
  }else{
    MASKRANGE_options_c <- ""
  }
  
  
  #### date1 and date3 ####
  #Get the High and Low Dates and Pair Dates for finding the first and last pair
  high_dates <- input_dates[input_resolutions==hightag_c]
  low_dates <- input_dates[input_resolutions==lowtag_c]
  pair_dates <- as.numeric(names(table(c(unique(high_dates),unique(low_dates))))[which(table(c(unique(high_dates),unique(low_dates)))>=2)])
  
  if(!missing(date1)){
    assert_that(class(date1)=="numeric")
    date1_c <- date1
  }else{
    date1_c <- pair_dates[1]
  }
  
  if(!missing(date3)){
    assert_that(class(date3)=="numeric")
    date3_c <- date3
  }else{
    date3_c <- pair_dates[length(pair_dates)]
  }
  
  #### n_cores ####
  if(!missing(n_cores)){
    assert_that(class(n_cores)=="numeric",
                n_cores<=parallel::detectCores())
    n_cores_c <- n_cores
  }else{
    n_cores_c <- 1
  }
  
  ##### B: Check all the required Inputs #####
  #These are variables which are always provided by the user
  #Here, we simply make sure they are of the correct types and matching length
  
  #Some basic Assertions about the types of the inputs
  assert_that(
    class(input_filenames)=="character",
    class(input_dates)=="numeric",
    class(input_resolutions)=="character",
    class(pred_dates)=="numeric",
    class(pred_filenames)=="character"
  )
  
  #Some basic Assertions about the length of the inputs
  assert_that(
    !double_pair_mode_c | length(input_filenames)>=5, # at least 5 files in double pair mode?
    double_pair_mode_c | length(input_filenames)>=3, # at least 3 files in single pair mode?
    length(input_resolutions)==length(input_filenames),
    length(input_dates)==length(input_filenames),
    length(unique(input_resolutions))==2,   
    length(pred_dates)==length(pred_dates)
  )
  
  #Get the High and Low Dates and Pair Dates just for checking
  high_dates <- input_dates[input_resolutions==hightag_c]
  low_dates <- input_dates[input_resolutions==lowtag_c]
  pair_dates <- as.numeric(names(table(c(unique(high_dates),unique(low_dates))))[which(table(c(unique(high_dates),unique(low_dates)))>=2)])
  
  
  
  #Some further Assertions

  assert_that(
   !double_pair_mode_c | length(pair_dates)>=2,             #At least two pairs in doublepair mode?
   double_pair_mode_c | length(pair_dates)>=1,             #At least one pair in singlepair mode?
   !double_pair_mode_c | any(!pred_dates>max(pair_dates)),  #Pred Dates within the Interval in doublepair mode?
   !double_pair_mode_c | any(!pred_dates<min(pair_dates)),   #Pred Dates within the Interval in doublepair mode?
   all(input_resolutions %in% c(hightag_c, lowtag_c)) #Resolutions given consistent with hightag and lowtag
  )
  
  
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  
  
  ##### C: Call the CPP function #####
  if(verbose){
    #And print the used parameters 
    print("Input Filenames: ")
    print(input_filenames_c)
    print("Input Resolutions: ")
    print(input_resolutions_c)
    print("Input Dates: ")
    print(input_dates_c)
    print("Prediction Filenames: ")
    print(pred_filenames_c)
    print("Prediction Dates: ")
    print(pred_dates_c)
     if(double_pair_mode_c){
       print("Predicting between Pairs on Dates:")
       print(paste(date1_c,date3_c))
     }else{
      print("Predicting from Pair on Date:")
      print(paste(date1_c))
     }
    print("Prediction Area: ")
    print(pred_area_c)
    print("Number Classes: ")
    print(number_classes_c)
    if (!grepl("^\\s*$", MASKIMG_options_c)){
      print("MASKIMG Options: ")
      print(MASKIMG_options_c)
    }
    if (!grepl("^\\s*$", MASKRANGE_options_c)){
      print("MASKRANGE Options: ")
      print(MASKRANGE_options_c)
    }
    if(use_parallelisation_c){
      print(paste("USING PARALLELISATION WITH ", n_cores_c," CORES"))
    }
  }
  
  #___________________________________________________________________________#
  #Call the cpp fusion function with the checked inputs
  execute_starfm_job_cpp(input_filenames = input_filenames_c,
                                      input_resolutions = input_resolutions_c,
                                      input_dates = input_dates_c,
                                      pred_dates = pred_dates_c,
                                      pred_filenames = pred_filenames_c,
                                      pred_area = pred_area_c,
                                      winsize = winsize_c,
                                      date1 = date1_c,
                                      date3 = date3_c,
                                      n_cores = n_cores_c,
                                      output_masks = output_masks_c,
                                      use_nodata_value = use_nodata_value_c,
                                      use_parallelisation = use_parallelisation_c,
                                      use_strict_filtering = use_strict_filtering_c,
                                      use_temp_diff_for_weights = use_temp_diff_for_weights_c,
                                      do_copy_on_zero_diff = do_copy_on_zero_diff_c,
                                      double_pair_mode = double_pair_mode_c,
                                      number_classes = number_classes_c,
                                      logscale_factor = logscale_factor_c,
                                      spectral_uncertainty = spectral_uncertainty_c,
                                      temporal_uncertainty = temporal_uncertainty_c,
                                      hightag = hightag_c,
                                      lowtag = lowtag_c,
                                      MASKIMG_options = MASKIMG_options_c,
                                      MASKRANGE_options = MASKRANGE_options_c,
                                      verbose=verbose
  )
  #___________________________________________________________________________#
  
}
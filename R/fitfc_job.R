
#' fitfc_job
#' @description A wrapper function for \code{execute_fitfc_job_cpp}. Intended to execute a single job, that is a number of predictions based on the same input pair(s). It ensures that all of the arguments passed are of the correct type and creates sensible defaults. 
#'
#' @param input_filenames  A string vector containing the filenames of the input images
#' @param input_resolutions A string vector containing the resolution-tags \code{hightag} and \code{lowtag} of the input images
#' @param input_dates An integer vector containing the dates of the input images.
#' @param pred_dates A string vector containing the dates for which images should be predicted.
#' @param pred_filenames A string vector containing the filenames for the predicted images. Must match \code{pred_dates} in length and order.
#' @param pred_area (Optional) An integer vector containing parameters in image coordinates for a bounding box which specifies the prediction area. The prediction will only be done in this area. (x_min, y_min, width, height). By default will use the entire area of the first input image.
#' @param winsize (Optional) Window size of the rectangle around the current pixel. Default is 51.
#' @param date1 (Optional) Set the date of the first input image pair. By default, will use the pair with the lowest date value.
#' @param n_cores (Optional) Set the number of cores to use when using parallelisation. Default is 1.
#' @param n_neighbors (Optional) The number of near pixels (including the center) to use in the filtering step (spatial filtering and residual compensation). Default is 10.
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
#' @param resolution_factor (Optional) Scale factor with which the low resolution image has been upscaled. This will be used for cubic interpolation of the residuals. Setting it to 1 will disable it. Default: 30.
#' @references Wang, Qunming, and Peter M. Atkinson. "Spatio-temporal fusion for daily Sentinel-2 images." Remote Sensing of Environment 204 (2018): 31-42.
#' @return Nothing. Output files are written to disk. The Geoinformation for the output images is adopted from the first input pair images.
#' @export
#'
#' @author Johannes Mast
#' @details Executes the FITFC Algorithm. If more than one pair is given, will perform prediction for the pred dates twice, once for each of the input pairs.
#' @examples Sorry, maybe later
#' 
#' 
fitfc_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area,winsize,date1,date3,n_cores,n_neighbors,hightag,lowtag,MASKIMG_options,MASKRANGE_options,output_masks,use_nodata_value,use_parallelisation,resolution_factor,verbose=T
){
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
  

  #### resolution_factor ####
  if(!missing(resolution_factor)){
    assert_that(class(resolution_factor)=="numeric")
    resolution_factor_c <- resolution_factor
  }else{
    resolution_factor_c <- 30
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
  #Note: If only one pair was given, Date1 and Date3 should be identical.
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
  
  #### n_neighbors ####
  if(!missing(n_neighbors)){
    assert_that(class(n_neighbors)=="numeric")
    n_neighbors_c <- n_neighbors
  }else{
    n_neighbors_c <- 10
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
    length(input_filenames)>=3, # at least 3 files in single pair mode?
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
  #2DO ADJUST FOR SINGLE PAIR MODE
  assert_that(
    length(pair_dates)>=1,             #At least one pair in singlepair mode?
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
    print("Predicting from Pair on Date:")
    print(paste(date1_c))
    print("Prediction Area: ")
    print(pred_area_c)
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
  #If we are in singlepair mode (only one pair specified)
  if(date1_c==date3_c){
  #Call the cpp fusion function once from date 1 with the checked inputs
  ImageFusion::execute_fitfc_job_cpp(input_filenames = input_filenames_c,
                                      input_resolutions = input_resolutions_c,
                                      input_dates = input_dates_c,
                                      pred_dates = pred_dates_c,
                                      pred_filenames = pred_filenames_c,
                                      pred_area = pred_area_c,
                                      winsize = winsize_c,
                                      date1 = date1_c,
                                      n_cores = n_cores_c,
                                      n_neighbors = n_neighbors_c,
                                      output_masks = output_masks_c,
                                      use_nodata_value = use_nodata_value_c,
                                      use_parallelisation = use_parallelisation_c,
                                      resolution_factor = resolution_factor_c,
                                      hightag = hightag_c,
                                      lowtag = lowtag_c,
                                      MASKIMG_options = MASKIMG_options_c,
                                      MASKRANGE_options = MASKRANGE_options_c
  )
  }
  #If we are in "doublepair" mode (two pairs specified)
  if(date1_c!=date3_c){
    #Call the cpp function twice, once based on each input pair
    #modify output names a bit to make them unique for each input pair
    pred_filenames_c1 <- paste(paste(tools::file_path_sans_ext(pred_filenames_c),"from_pair",date1_c,sep="_"),tools::file_ext(pred_filenames_c),sep=".")
    #executre job from date1
    ImageFusion::execute_fitfc_job_cpp(input_filenames = input_filenames_c,
                                       input_resolutions = input_resolutions_c,
                                       input_dates = input_dates_c,
                                       pred_dates = pred_dates_c,
                                       pred_filenames = pred_filenames_c1,
                                       pred_area = pred_area_c,
                                       winsize = winsize_c,
                                       date1 = date1_c,
                                       n_cores = n_cores_c,
                                       n_neighbors = n_neighbors_c,
                                       output_masks = output_masks_c,
                                       use_nodata_value = use_nodata_value_c,
                                       use_parallelisation = use_parallelisation_c,
                                       resolution_factor = resolution_factor_c,
                                       hightag = hightag_c,
                                       lowtag = lowtag_c,
                                       MASKIMG_options = MASKIMG_options_c,
                                       MASKRANGE_options = MASKRANGE_options_c
    )
    #modify output names a bit to make them unique for each input pair
    pred_filenames_c3 <- paste(paste(tools::file_path_sans_ext(pred_filenames_c),"from_pair",date3_c,sep="_"),tools::file_ext(pred_filenames_c),sep=".")
    #execture job fromdate 3
    ImageFusion::execute_fitfc_job_cpp(input_filenames = input_filenames_c,
                                       input_resolutions = input_resolutions_c,
                                       input_dates = input_dates_c,
                                       pred_dates = pred_dates_c,
                                       pred_filenames = pred_filenames_c3,
                                       pred_area = pred_area_c,
                                       winsize = winsize_c,
                                       date1 = date3_c,
                                       n_cores = n_cores_c,
                                       n_neighbors = n_neighbors_c,
                                       output_masks = output_masks_c,
                                       use_nodata_value = use_nodata_value_c,
                                       use_parallelisation = use_parallelisation_c,
                                       resolution_factor = resolution_factor_c,
                                       hightag = hightag_c,
                                       lowtag = lowtag_c,
                                       MASKIMG_options = MASKIMG_options_c,
                                       MASKRANGE_options = MASKRANGE_options_c
    )
    
    
  }
  
  #
  
  
  
  #___________________________________________________________________________#
  
}
#' Execute a single self-contained self-contained time-series imagefusion job using SPSTFM.
#' @description A wrapper function for \code{execute_spstfm_job_cpp}. Intended to execute a single job, that is a number of predictions based on the same input pairs. It ensures that all of the arguments passed are of the correct type and creates sensible defaults. 
#'
#' @param input_filenames A string vector containing the filenames of the input images
#' @param input_resolutions A string vector containing the resolution-tags (corresponding to the arguments \code{hightag} and \code{lowtag}, which are by default "high" and "low") of the input images.
#' @param input_dates An integer vector containing the dates of the input images.
#' @param pred_filenames A string vector containing the filenames for the predicted images. Must match \code{pred_dates} in length and order. Must include an extension relating to one of the \href{https://gdal.org/drivers/raster/index.html}{drivers supported by GDAL}, such as ".tif".
#' @param pred_dates An integer vector  containing the dates for which images should be predicted.
#' @param pred_area (Optional) An integer vector containing parameters in image coordinates for a bounding box which specifies the prediction area. The prediction will only be done in this area. (x_min, y_min, width, height). By default will use the entire area of the first input image.
#' @param winsize (Optional) Window size of the rectangle around the current pixel. Default is 51.
#' @param date1 (Optional) Set the date of the first input image pair. By default, will use the pair with the lowest date value.
#' @param date3 (Optional) Set the date of the second input image pair. By default, will use the pair with the highest date value.
#' @param n_cores (Optional) Set the number of cores to use when using parallelisation. Note that parallelisation is currently not implemented for spstfm, and only listed here for consistency. Default is 1.
#' @param dict_size (Optional) Dictionary size, i. e. the number of atoms. Default: 256.
#' @param n_training_samples (Optional) The number of samples used for training (training data size). Default: 2000.
#' @param patch_size (Optional) Size of a patch in pixel. Default: 7.
#' @param patch_overlap (Optional) Overlap on each side of a patch in pixel. Default: 2.
#' @param min_train_iter (Optional) Minimum number of iterations during the training. Default: 10.
#' @param max_train_iter (Optional) Can be 0 if the minimum is also 0. Then no training will be done, even if there is no dictionary yet (it will still be initialized). Default: 20
#' @param MASKIMG_options (Optional) A string containing information for a mask image (8-bit, boolean, i. e. consists of 0 and 255). "For all input images the pixel values at the locations where the mask is 0 is replaced by the mean value." Example: \code{--mask-img=some_image.png}
#' @param MASKRANGE_options (Optional) Specify one or more intervals for valid values. Locations with invalid values will be masked out. Ranges should be given in the format \code{'[<float>,<float>]'}, \code{'(<float>,<float>)'}, \code{'[<float>,<float>'} or \code{'<float>,<float>]'}. There are a couple of options:' \itemize{
##'  \item{"--mask-valid-ranges"}{ Intervals which are marked as valid. Valid ranges can excluded from invalid ranges or vice versa, depending on the order of options.}
##'  \item{"--mask-invalid-ranges"}{ Intervals which are marked as invalid. Invalid intervals can be excluded from valid ranges or vice versa, depending on the order of options.}
##'  \item{"--mask-high-res-valid-ranges"}{ This is the same as --mask-valid-ranges, but is applied only for the high resolution images.}
##'  \item{"--mask-high-res-invalid-ranges"}{ This is the same as --mask-invalid-ranges, but is applied only for the high resolution images.}
##'  \item{"--mask-low-res-valid-ranges"}{ This is the same as --mask-valid-ranges, but is applied only for the low resolution images.}
##'  \item{"--mask-low-res-invalid-ranges"}{ This is the same as --mask-invalid-ranges, but is applied only for the low resolution images.}
##' }
##' @param LOADDICT_options (Optional) A string containing the path to a previously saved dictionary file, which is to be loaded (see \code{REUSE_OPTIONS}).
##' @param SAVEDICT_options (Optional) A string containing the path to a file to save the dictionary to after training. 
##' @param REUSE_options (Optional) For a dictionary loaded from file, what to do with the existing dictionary: \itemize{
##' \item{"clear"} { clear an existing dictionary before training}
##' \item{"improve"} {improve an existing dictionary (default)}
##' \item{"use"} { use an existing dictionary without further training}
##' }
#' @param hightag (Optional) A string which is used in \code{input_resolutions} to describe the high-resolution images. Default is "high".
#' @param lowtag (Optional) A string which is used in \code{input_resolutions} to describe the low-resolution images.  Default is "low".
#' @param output_masks (Optional) Write mask images to disk? Default is "false".
#' @param use_nodata_value (Optional) Use the nodata value as invalid range for masking? Default is "true".
#' @param random_sampling (Optional) Use random samples for training data instead of the samples with the most variance? Default is "false".
#' @param verbose (Optional) Print progress updates to console? Default is "true".
#' @references Huang, B., & Song, H. (2012). Spatiotemporal reflectance fusion via sparse representation. IEEE Transactions on Geoscience and Remote Sensing, 50(10), 3707-3716.
#' @return Nothing. Output files are written to disk. The Geoinformation for the output images is adopted from the first input pair images.
#' @export
#' @importFrom raster stack
#' @importFrom assertthat assert_that 
#'
#' @author Christof Kaufmann (C++)
#' @author Johannes Mast (R)
#' @details Executes the SPSTFM algorithm to create a number of synthetic high-resolution images from two pairs of matching high- and low-resolution images.  Assumes that the input images already have matching size. For a detailed explanation how SPSTFM works there is the original paper and the thesis, which yielded this implementation. The latter explains also all available options and shows some test results. However, the default options should give good results.
#' @examples #
#' # Load required libraries
#' library(ImageFusion)
#' library(raster)
#' # Get filesnames of high resolution images
#' landsat <- list.files(
#'   system.file("landsat/filled",
#'               package = "ImageFusion"),
#'   ".tif",
#'   recursive = TRUE,
#'   full.names = TRUE
#' )
#' 
#' # Get filesnames of low resolution images
#' modis <- list.files(
#'   system.file("modis",
#'               package = "ImageFusion"),
#'   ".tif",
#'   recursive = TRUE,
#'   full.names = TRUE
#' )
#' 
#' #Select the first two landsat images 
#' landsat_sel <- landsat[1:2]
#' #Select some corresponding modis images
#' modis_sel <- modis[1:10]
#' # Create output directory
#' if(!dir.exists("Outputs")) dir.create("Outputs", recursive = TRUE)
#' #Run the job, fusing two images
#' spstfm_job(input_filenames = c(landsat_sel,modis_sel),
#'            input_resolutions = c("high","high",
#'                                  "low","low","low",
#'                                  "low","low","low",
#'                                  "low","low","low",
#'                                  "low"),
#'            input_dates = c(68,77,68,69,70,71,72,73,74,75,76,77),
#'            pred_dates = c(74),
#'            pred_filenames = c("Outputs/spstfm_74.tif"),
#'            n_training_samples = 5000
#'            )
#' 
#' # remove the output directory
#' unlink("Outputs",recursive = TRUE)
#' @family {fusion_algorithms}


spstfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area,winsize,date1,date3,n_cores,dict_size,
                       n_training_samples,patch_size,patch_overlap,min_train_iter,max_train_iter,
                       hightag,lowtag,MASKIMG_options,MASKRANGE_options,LOADDICT_options,SAVEDICT_options,REUSE_options,output_masks,use_nodata_value,random_sampling,verbose=TRUE
) {

  
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
  
  #### random_sampling ####
  if(!missing(random_sampling)){
    assert_that(class(random_sampling)=="logical")
    random_sampling_c <- random_sampling
  }else{
    random_sampling_c <- FALSE
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
  
  #### dictionary options####
  if(!missing(LOADDICT_options)){
    assert_that(class(LOADDICT_options)=="character")
    LOADDICT_options_c <- LOADDICT_options
  }else{
    LOADDICT_options_c <- ""
  }
  
  if(!missing(SAVEDICT_options)){
    assert_that(class(SAVEDICT_options)=="character")
    SAVEDICT_options_c <- SAVEDICT_options
  }else{
    SAVEDICT_options_c <- ""
  }
  
  if(!missing(REUSE_options)){
    assert_that(class(REUSE_options)=="character")
    REUSE_options_c <- REUSE_options
  }else{
    REUSE_options_c <- "improve"
  }
  #### date1 and date3 ####
  #Get the High and Low Dates and Pair Dates for finding the first and last pair
  high_dates <- input_dates[input_resolutions==hightag_c]
  low_dates <- input_dates[input_resolutions==lowtag_c]
  pair_dates <- as.numeric(names(table(c(unique(high_dates),unique(low_dates))))[which(table(c(unique(high_dates),unique(low_dates)))>=2)])
  
  if(!missing(date1)){
    assert_that(class(date1)=="numeric"|class(date1)=="integer")
    date1_c <- date1
  }else{
    date1_c <- pair_dates[1]
  }
  
  if(!missing(date3)){
    assert_that(class(date3)=="numeric"|class(date3)=="integer")
    date3_c <- date3
  }else{
    date3_c <- pair_dates[length(pair_dates)]
  }
  
  #### n_cores ####
  if(!missing(n_cores)){
    assert_that(class(n_cores)=="numeric"|class(n_cores)=="integer",
                n_cores<=parallel::detectCores())
    n_cores_c <- n_cores
  }else{
    n_cores_c <- 1
  }
  
  #### dict_size ####
  if(!missing(dict_size)){
    assert_that(class(dict_size)=="numeric")
    dict_size_c <- dict_size
  }else{
    dict_size_c <- 256
  }
  
  #### n_training_samples ####
  if(!missing(n_training_samples)){
    assert_that(class(n_training_samples)=="numeric"|class(n_training_samples)=="integer")
    n_training_samples_c <- n_training_samples
  }else{
    n_training_samples_c <- 2000
  }
  
  #### patch_size ####
  if(!missing(patch_size)){
    assert_that(class(patch_size)=="numeric")
    patch_size_c <- patch_size
  }else{
    patch_size_c <- 7
  }
  
  
  #### patch_overlap ####
  if(!missing(patch_overlap)){
    assert_that(class(patch_overlap)=="numeric"|class(patch_overlap)=="integer")
    patch_overlap_c <- patch_overlap
  }else{
    patch_overlap_c <- 2
  }
  #### min and max train iter ####
  if(!missing(min_train_iter)){
    assert_that(class(min_train_iter)=="numeric"|class(min_train_iter)=="integer")
    min_train_iter_c <- min_train_iter
  }else{
    min_train_iter_c <- 10
  }
  if(!missing(max_train_iter)){
    assert_that(class(max_train_iter)=="numeric"|class(max_train_iter)=="integer")
    max_train_iter_c <- max_train_iter
  }else{
    max_train_iter_c <- 20
  }
  
  #___________________________________________________________________________#
  
  ##### B: Check all the required Inputs #####
  #These are variables which are always provided by the user
  #Here, we simply make sure they are of the correct types and matching length
  
  #Some basic Assertions about the types of the inputs
  assert_that(
    class(input_filenames)=="character",
    class(input_dates)=="numeric"|class(input_dates)=="integer",
    class(input_resolutions)=="character",
    class(pred_dates)=="numeric"|class(pred_dates)=="integer",
    class(pred_filenames)=="character"
  )
  
  #Some basic Assertions about the length of the inputs
  assert_that(
    length(input_filenames)>=5,
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
    length(pair_dates)>=2,             #At least two pairs?
    any(!pred_dates>max(pair_dates)),  #Pred Dates within the Interval?
    any(!pred_dates<min(pair_dates)),   #Pred Dates within the Interval?
    all(input_resolutions %in% c(hightag_c, lowtag_c)) #Resolutions given consistent with hightag and lowtag
  )
  
  
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  #___________________________________________________________________________#
  
  
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
    print("Predicting between Pairs on Dates:")
    print(paste(date1_c,date3_c))
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
    if(n_cores_c>1){
      print(paste("PARALLELISATION WITH ", n_cores_c," CORES ATTEMPTED. PARALLELISATION IS NOT SUPPORTED IN SPSTFM"))
    }
  }
  
  #Call the cpp fusion function with the checked inputs
  execute_spstfm_job_cpp(input_filenames = input_filenames_c,
                                       input_resolutions = input_resolutions_c,
                                       input_dates = input_dates_c,
                                       pred_dates = pred_dates_c,
                                       pred_filenames = pred_filenames_c,
                                       pred_area = pred_area_c,
                                       date1 = date1_c,
                                       date3 = date3_c,
                                       n_cores = n_cores_c,
                                        dict_size = dict_size_c,
                                        n_training_samples = n_training_samples_c,
                                        patch_size = patch_size_c,
                                        patch_overlap = patch_overlap_c,
                                        min_train_iter = min_train_iter_c,
                                        max_train_iter = max_train_iter_c,
                                       output_masks = output_masks_c,
                                       use_nodata_value = use_nodata_value_c,
                                        random_sampling = random_sampling_c,
                                      verbose=verbose,
                                       hightag=hightag_c,
                                       lowtag=lowtag_c,
                                       MASKIMG_options= MASKIMG_options_c,
                                       MASKRANGE_options = MASKRANGE_options_c,
                                        LOADDICT_options = LOADDICT_options_c,
                                        SAVEDICT_options = SAVEDICT_options_c,
                                        REUSE_options = REUSE_options_c
  )
  #___________________________________________________________________________#
  
}
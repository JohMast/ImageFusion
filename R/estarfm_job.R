


#' estarfm_job
#' @description A wrapper function for \code{execute_estarfm_job_cpp}. It ensures that all of the arguments passed are of the correct type and creates sensible defaults. 
#'
#' @param input_filenames A string vector containing the filenames of the input images
#' @param input_resolutions A string vector containing the resolution-tags \code{hightag} and \code{lowtag} of the input images
#' @param input_dates An integer vector containing the dates of the input images.
#' @param pred_filenames A string vector containing the filenames for the predicted images. Must match \code{pred_dates} in length and order.
#' @param pred_dates A string vector containing the dates for which images should be predicted.
#' @param pred_area (Optional) An integer vector containing parameters in image coordinates for a bounding box which specifies the prediction area. The prediction will only be done in this area. (x_min, y_min, width, height). By default will use the entire area of the first input image.
#' @param winsize (Optional) Window size of the rectangle around the current pixel. Default is 51.
#' @param data_range_min (Optional) When predicting pixel values ESTARFM can exceed the values that appear in the image. To prevent from writing invalid values (out of a known data range) you can set bounds. By default, the value range will be limited by the natural data range (e. g. -32767 for INT2S).
#' @param data_range_max (Optional) When predicting pixel values ESTARFM can exceed the values that appear in the image. To prevent from writing invalid values (out of a known data range) you can set bounds. By default, the value range will be limited by the natural data range (e. g.  32767 for INT2S).
#' @param use_local_tol (Optional) This enables the usage of local tolerances to find similar pixels instead of using the global tolerance.  When searching similar pixels, a tolerance of \eqn{2\sigma/m} is used. This options sets whether is calculated only from the local window region around the central pixel or from the whole image. Default is "false".
#' @param uncertainty_factor (Optional) Sets the uncertainty factor. This is multiplied to the upper limit of the high resolution range. The range can be given by a mask range. Default: 0.002 (i. e. 0.2)
#' @param number_classes (Optional) The number of classes used for similarity. Note all channels of a pixel are considered for similarity. So this value holds for each channel, e. g. with 3 channels there are n^3 classes. Default: c-th root of 64, where c is the number of channels.
#' @param hightag (Optional) A string which is used in \code{input_resolutions} to describe the high-resolution images. Default is "high".
#' @param lowtag (Optional) A string which is used in \code{input_resolutions} to describe the low-resolution images.  Default is "low".
#' @references Zhu, X., Chen, J., Gao, F., Chen, X., & Masek, J. G. (2010). An enhanced spatial and temporal adaptive reflectance fusion model for complex heterogeneous regions. Remote Sensing of Environment, 114(11), 2610-2623.
#' @return Nothing. Output files are written to disk.
#' @export
#'
#' @author Johannes Mast
#' @details Executes the estarfm algorithm to create a number of synthetic high-resolution images from two pairs of matching high- and low-resolution images.  Assumes that the input images already have matching size. See the original paper for details (Note: There is a difference to the algorithm as described in the paper though. The regression for $ R $ is now done with all candidates of one window. This complies to the reference implementation, but not to the paper, since there the regression is done only for the candidates that belong to one single coarse pixel. However, the coarse grid is not known at prediction and not necessarily trivial to find out (e. g. in case of higher order interpolation).)
#' @examples Sorry, maybe later


estarfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area,winsize,data_range_min, data_range_max, uncertainty_factor,number_classes,hightag,lowtag
                        ) {
  library(assertthat)
  

  #### A: Check all the Optional Inputs ####
  #These are variables which are optional 
  # or can easily infered from the required inputs
  

  ### pred_area ###
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

  ### winsize ###
  if(!missing(winsize)){
    assert_that(class(winsize)=="numeric")
    winsize_c <- winsize
  }else{
    winsize_c <- 51
  }
  ### use_local_tol ###
  if(!missing(use_local_tol)){
    assert_that(class(use_local_tol)=="logical")
    use_local_tol_c <- use_local_tol
  }else{
    use_local_tol_c <- FALSE
  }
  
  ### uncertainty_factor ###
  if(!missing(uncertainty_factor)){
    assert_that(class(uncertainty_factor)=="numeric")
    uncertainty_factor_c <- uncertainty_factor
  }else{
    uncertainty_factor_c <- 0.002
  }
  
  
  ### number_classes ###
  if(!missing(number_classes)){
    assert_that(class(number_classes)=="numeric")
    number_classes_c <- number_classes
  }else{
    number_classes_c <- 64^(1/length(template@layers))
  }
  
  ### data_range_min ###
  if(!missing(data_range_min)){
    assert_that(class(data_range_min)=="numeric")
    data_range_min_c <- data_range_min
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U"){data_range_min_c <- 0}
    if(template_dataType=="INT1S"){data_range_min_c <- -127}
    if(template_dataType=="INT2U"){data_range_min_c <- 0}
    if(template_dataType=="INT2S"){data_range_min_c <-  -32767}
    if(template_dataType=="FLT4S"){data_range_min_c <- -3.4e+38}
    if(template_dataType=="FLT8S"){data_range_min_c <- -1.7e+308}
    }
    
    
  ### data_range_min ###
  if(!missing(data_range_max)){
    assert_that(class(data_range_max)=="numeric")
    data_range_max_c <- data_range_max
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U"){data_range_max_c <- 255}
    if(template_dataType=="INT1S"){data_range_max_c <- 127}
    if(template_dataType=="INT2U"){data_range_max_c <- 65534}
    if(template_dataType=="INT2S"){data_range_max_c <- 32767}
    if(template_dataType=="FLT4S"){data_range_max_c <- 3.4e+38}
    if(template_dataType=="FLT8S"){data_range_max_c <- 1.7e+308 }
  } 
  
  ### hightag and lowtag###
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
  #___________________________________________________________________________#
  
  #### B: Check all the required Inputs ####
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
    length(input_filenames)>=5,
    length(input_resolutions)==length(input_filenames),
    length(input_dates)==length(input_filenames),
    length(unique(input_resolutions))==2,   
    length(pred_dates)==length(pred_dates)
  )
  
  #Get the High and Low Dates and Pair Dates just for checking
  high_dates <- input_dates[input_resolutions==hightag_c]
  low_dates <- input_dates[input_resolutions==lowtag_c]
  pair_dates <- which(table(c(unique(high_dates),unique(low_dates)))>=2)
  
  #Some further Assertions
  assert_that(
    length(pair_dates)>=2,             #At least two pairs?
    any(!pred_dates>max(pair_dates)),  #Pred Dates within the Interval?
    any(!pred_dates<min(pair_dates)),   #Pred Dates within the Interval?
    all(test_resolutions %in% c(hightag_c, lowtag_c)) #Resolutions given consistent with hightag and lowtag
      )
  
  
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  #___________________________________________________________________________#
  
  
  #### C: Call the CPP function ####
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
  print("Prediction Area: ")
  print(pred_area_c)
  print("Number Classes: ")
  print(number_classes_c)
  print("Data_Range: ")
  print(paste(data_range_min_c, data_range_max_c))
  
  #Call the cpp fusion function with the checked inputs
  ImageFusion::execute_estarfm_job_cpp(input_filenames = input_filenames_c,
                                   input_resolutions = input_resolutions_c,
                                   input_dates = input_dates_c,
                                   pred_dates = pred_dates_c,
                                   pred_filenames = pred_filenames_c,
                                   pred_area = pred_area_c,
                                   winsize = winsize_c,
                                   use_local_tol = use_local_tol_c,
                                   uncertainty_factor = uncertainty_factor_c,
                                   number_classes = number_classes_c,
                                   data_range_min = data_range_min_c,
                                   data_range_max = data_range_max_c,
                                   hightag=hightag_c,
                                   lowtag=lowtag_c
                                  )
  #___________________________________________________________________________#
  
}
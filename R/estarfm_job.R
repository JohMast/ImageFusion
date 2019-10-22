

library(assertthat)
#' Title
#' @description A wrapper function for \code{execute_estarfm_job_cpp}. It ensures that all of the arguments passed are of the correct type and creates sensible defaults. 
#'
#' @param input_filenames A string vector containing the filenames of the input images
#' @param input_resolutions A string vector containing the resolution-tags \code{hightag} and \code{lowtag} of the input images
#' @param input_dates An integer vector containing the dates of the input images.
#' @param pred_dates A string vector containing the dates for which images should be predicted.
#' @param pred_filenames A string vector containing the filenames for the predicted images. Must match pred_dates in length and order.
#' @param pred_area (Optional) An integer vector containing the bounding box coordinates for the area to predict (x_min, y_min, x_max, y_max). By default will use the entire area of the first input image.
#' @param hightag (Optional) A string which is used in \code{input_resolutions} to describe the high-resolution images. Default is "high".
#' @param lowtag (Optional) A string which is used in \code{input_resolutions} to describe the low-resolution images.  Default is "low".
#'
#' @return Nothing. Output files are written to disk.
#' @export
#'
#' @author Johannes Mast
#' @details Executes the estarfm algorithm to create a number of synthetic high-resolution images from two pairs of matching high- and low-resolution images.  Assumes that the input images already have matching size.
#' @examples Sorry, maybe later
#'
#' 
#'   
estarfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area,hightag,lowtag
                        ) {
  
  #### A: Check all the required Inputs ####
  #These are variables which are always provided by the user
  #Here, we simply make sure they are of the correct types and matching length
  
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  
  #### B: Check all the Optional Inputs ####
  #These are variables which are optional 
  # or can easily infered from the required inputs
  

  ### pred_area ###
  # check pred area.
  # if not provided by user, set pred area to max image size of first image
  # Open Question: Is the cropping window required in image- or geo-coordinates? Here we assume geo.
  if(!missing(pred_area)){
    assert_that(length(pred_area)==4,
                class(pred_area)=="numeric")
    pred_area_c <- pred_area
  }else{
    print("No Prediction Area specified. Predicting entire extent of:")
    print(input_filenames_c[1])
    template <- raster::raster(input_filenames_c[1])
    pred_area_c <- template@extent[c(1,3,2,4)]
  }

  ### hightag and lowtag###
  if(!missing(hightag)){
    assert_that(class(pred_area)=="character")
    hightag_c <- hightag
  }else{
    hightag_c <- "high"
  }
  if(!missing(lowtag)){
    assert_that(class(pred_area)=="character")
    lowtag_c <- lowtag
  }else{
    lowtag_c <- "low"
  }

  #Call the cpp fusion function with the checked inputs
  ImageFusion::execute_estarfm_job_cpp(input_filenames = input_filenames_c,
                                   input_resolutions = input_resolutions_c,
                                   input_dates = input_dates_c,
                                   pred_dates = pred_dates_c,
                                   pred_filenames = pred_filenames_c,
                                   pred_area = pred_area_c,
                                   hightag=hightag_c,
                                   lowtag=lowtag_c
                                  )
  
  
}
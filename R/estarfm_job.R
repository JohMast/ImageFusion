
estarfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area
                        ) {
  #Check all inputs for plausibility
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  
  
  #### TO DO: SET OPTIONAL PARAMETERS IF NOT GIVEN ####
  # pred_area
  # set pred area to max image size
  # Open Question: Is the cropping window required in image- or geo-coordinates? Here we assume geo.
  if(!missing(pred_area)){
    pred_area_c <- pred_area
  }else{
    print(input_filenames_c[1])
    template <- raster::raster(input_filenames_c[1])
    pred_area_c <- template@extent[c(1,3,2,4)]
  }

  
  #Call the cpp fusion function with the checked inputs
  ImageFusion::execute_estarfm_job_cpp(input_filenames = input_filenames_c,
                                   input_resolutions = input_resolutions_c,
                                   input_dates = input_dates_c,
                                   pred_dates = pred_dates_c,
                                   pred_filenames = pred_filenames_c,
                                   pred_area = pred_area_c
                                  )
  
  
}
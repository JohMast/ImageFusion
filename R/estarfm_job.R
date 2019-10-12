

#' Title
#'
#' @return Nothinh
#' @export
#'
#' @examples
#' 
#' 

estarfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area
                        ) {
  #Check all inputs for plausibility
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  pred_area_c <- pred_area
  
  

  
  #Call the cpp fusion function with the checked inputs
  ImageFusion::execute_estarfm_job(input_filenames = input_filenames_c,
                                   input_resolutions = input_resolutions_c,
                                   input_dates = input_dates_c,
                                   pred_dates = pred_dates_c,
                                   pred_filenames = pred_filenames_c,
                                   pred_area = pred_area_c
                                  )
  
  
}
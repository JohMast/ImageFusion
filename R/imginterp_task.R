#' imginterp_task
#'
#' @return
#' @export
#'
#' @examples Sorry, maybe later
imginterp_task <- function(){

  
  
  in_files <- list.files(path = "../../Triglav_Processed_with_Gaps/",pattern = ".tif",full.names = T)
  dates <- 1:length(in_files)*2
  tags <- c("high","low","high","high","low","high","low","high","low")
  layers <- c(1,2,3,5)
  limit_days <- 9
  
  interp_invalid <- T
  interp_ranges <- NULL
  no_interp_ranges <- NULL
  print_stats=F
  stats_filename <- NULL
  
  string_imgargs <- paste0(" --img='-f ",in_files," -d ",dates)
  if(!is.null(tags)){
    string_imgargs <- paste0(string_imgargs," -t ",tags)
  }
  if(!is.null(layers)){
    string_layers <- paste(layers,collapse = ",")
    string_imgargs <- paste0(string_imgargs," -l ",string_layers)
  }
  
  
  #End each image string with a '
  string_imgargs <- paste0(string_imgargs,"'")
  string_imgargs
  string_args <- paste0(string_imgargs,collapse="")
  string_args <- paste0(string_args, " --limit-days=",limit_days)

  #If interp-ranges are given, add them, otherwise use a faux range that means no values will be interpolated
  if(!is.null(interp_ranges)){
    string_args <- paste0(string_args, " --interp-ranges=",interp_ranges)
  }else{
    string_args <- paste0(string_args, " --interp-ranges=[inf,inf]")
  }
  #If no-interp-ranges are given, add them
  if(!is.null(no_interp_ranges)){
    string_args <- paste0(string_args, " --no-interp-ranges=",no_interp_ranges)
    }
  #IF desired, also interpolate missing values
  if(interp_invalid){
    string_args <- paste0(string_args, " --enable-interp-invalid ")
  }
  #IF desired, also output stats to file or console
  if(!is.null(stats_filename)){
    string_args <- paste0(string_args, " --stats=", stats_filename)
  }else if(print_stats){
    string_args <- paste0(string_args, " --stats")
  }
  
  string_args
  ImageFusion:::execute_imginterp_job_cpp(input_string = string_args,verbose=T)
  library(raster)
  t <- stack("interpolated_2020100_191-28.tif")
  t
  plotRGB(t,stretch="lin")

  
}


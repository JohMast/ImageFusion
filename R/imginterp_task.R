#' Perform time-series image interpolation
#' @description A utility function of the ImageFusion Package, which performs simple interpolation on a given time series of remote sensing images
#'
#' @param filenames A character vector of the filenames of the images to interpolate.
#' @param dates An integer vector of the dates associated with the \code{filenames}. Must match \code{filenames} in length and order.
#' @param tags (Optional) A character vector of the resolution tags associated with the \code{filenames}. Interpolation will be done in groups based on these tags. By default, no grouping will be done.
#' @param limit_days (Optional) Integer. Limit the maximum numbers of days from the interpolating day that will be considered. So using e. g. a 3 will only consider images that are 3 days apart from the interpolation day. Default is 5.
#' @param layers A list of numeric (specifically a num-list, see below at \code{<num-list>}). Specifies the channels or layers, that will be read. Hereby a 0 means the first channel. Should be either matching \code{filenames} in length and order or be of length 1, in which case the selection will be applied to all images.
#' @param verbose (Optional) Logical. Output intermediate progress reports? Default is "true".
#' @param interp_invalid (Optional) Logical. Interpolate invalid locations? Default is "true".
#' @param interp_ranges (Optional) Character (specifically a range-list, see below at \code{<range-list>}). Specify one or more intervals for values that should be interpolated. By default is empty.
#' @param no_interp_ranges (Optional) Character (specifically a range-list, see below at \code{<range-list>}). Specify one or more intervals for values that should not be interpolated. No-interp-ranges can be excluded from interp-ranges. By default is empty.
#' @param stats_filename (Optional) Character, a filename. Enable stats (cloud pixels before and after, etc.) and output into the given file. By default, no stats will be output.
#' @param print_stats (Optional) Logical. If no \code{stats_filename} was provided: Print stats to console instead? Default is "false".
#' @param out_prefix (Optional) Character. This string will be prepended to the output filenames and can also be used to specify an output filepath.  By default is empty.
#' @param out_postfix (Optional) Character. This string will be appended to the output filenames. Default is "interpolated_".
#' @param output_pixelstate (Optional) Logical. If "true" output the pixelstate. The pixelstates are 8 bit wide. Default is "false".
#' @param use_nodata (Optional) Logical. If "true" nodata will be used as invalid range for masking. Default is "true".
#' @param prioritize_invalid (Optional) Logical. When a pixel location is marked as invalid and as interpolate, handle as invalid location and do not interpolate. Default is "false".
#' @param out_pixelstate_prefix (Optional, only used if \code{output_pixelstate} was set to "true".) Character. A string which will be prepended to the output filenames (including out_prefix and out_postfix). 
#' @param out_pixelstate_postfix (Optional, only used if \code{output_pixelstate} was set to "true".) Character. A string which will be appended to the output filenames (including out_prefix and out_postfix). Default is "ps_".
#' @param ql_filenames (Optional) A character vector of the filenames of the images use as quality layers. By default, no quality layer will be used.
#' @param ql_dates (Optional, only used if any \code{ql_filenames} were provided.) An integer vector of the dates associated with the \code{ql_filenames}. Must match \code{ql_filenames} in length and order.
#' @param ql_tags (Optional, only used if any \code{ql_filenames} were provided.) A character vector of the resolution tags associated with the \code{ql_filenames}.
#' @param ql_layers (Optional, only used if any \code{ql_filenames} were provided.) A list of numeric vectors (see below at \code{<num-list>}). Specifies the channels or layers, that will be read. Hereby a 0 means the first channel. Should be either matching \code{ql_filenames} in length and order or be of length 1, in which case the selection will be applied to all ql_images.
#' @param ql_interp_ranges (Optional, only used if any \code{ql_filenames} were provided.) A list of character (specifically a list of range-lists, see below at \code{<range-list>}). Specifies the ranges of the shifted value that should mark the location for interpolation. Should be either matching \code{ql_filenames} in length and order or be of length 1, in which case the ranges will be applied to all ql_images.
#' @param ql_non_interp_ranges (Optional, only used if any \code{ql_filenames} were provided.) A list of character (specifically a list of range-lists, see below at \code{<range-list>}). Specifies the ranges of the shifted value that should mark the location for interpolation. Should be either matching \code{ql_filenames} in length and order or be of length 1, in which case the ranges will be applied to all ql_images.
#' @param ql_bits  (Optional, only used if any \code{ql_filenames} were provided.) A list of numeric (see below at \code{<num-list>}). Specifies the bits to use. The selected bits will be extracted. from the quality layer image and then shifted to the least significant positions. If unspecified, all bits will be used. Should be either matching \code{ql_filenames} in length and order or be of length 1, in which case the selection will be applied to all ql_images.
#' @param mask_filenames  (Optional) A character vector of the filenames of the images use as Mask image (8-bit, boolean, i. e. consists of 0 and 255). By default, no mask image will be used.
#' @param mask_layers (Optional, only used if any \code{mask_filenames} were provided.) A list of numeric vectors(see below at \code{<num-list>}). Specifies the channels or layers, that will be read. Hereby a 0 means the first channel. Should be either matching \code{mask_filenames} in length and order or be of length 1, in which case the selection will be applied to all mask_images.Should be either matching \code{mask_filenames} in length and order or be of length 1, in which case the selection will be applied to all mask_images.
#' @param mask_valid_ranges (Optional) A list of character (specifically a list of range-lists, see below at \code{<range-list>}). Specifies the ranges of the shifted value that should mark the location as valid (true; 255).  Should be either matching \code{mask_filenames} in length and order or be of length 1, in which case the ranges will be applied to all ql_images.
#' @param mask_bits  (Optional, only used if any \code{mask_filenames} were provided.) A list of numeric (see below at \code{<num-list>}). Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted from the quality layer image and then shifted to the least significant positions. By default all bits will be used.
#' @param mask_invalid_ranges  A list of character (specifically a list of range-lists, see below at \code{<range-list>}). Specifies the ranges of the shifted value that should mark the location as invalid (false; 0). Should be either matching \code{mask_filenames} in length and order or be of length 1, in which case the ranges will be applied to all ql_images.
#' @param invalid_ranges (Optional) Character (specifically a range-list, see below at \code{<range-list>}). Specify one or more intervals for invalid values. These will be masked out. 
#' @param valid_ranges  (Optional) Character (specifically a range-list, see below at \code{<range-list>}). Specify one or more intervals for valid values. Locations with invalid values will be masked out.
#' @return Nothing, files are written to disk.
#' @export
#' @details This utility is developed to perform simple interpolation on a given time series of remote sensing images.
#'  This utility can also perform cloud masking on satellite images with the quality layer provided using \code{ql} options. 
#'  The quality layer can be a bit field image (ex. State_1km: Reflectance Data State QA layer from MODIS) or state image which 
#'  provides the state of the pixel (ex. quality layer from FMASK). When a single image with a date and a quality layer with the 
#'  same date is provided, this utility will fill the cloud (or whatever is specified) locations with the nodata value and output
#'  the modified image. If multiple images with dates are provided with quality layers, this utility will try to interpolate the
#'  bad locations linearly. When there is not enough data, the non-interpolated locations will be set to the nodata value. Note, nodata locations will not be interpolated by default.
#'  
#'  
#'  \strong{Pixelstates}
#'  
#' Using \code{output_pixelstate} the pixelstates can be produces as an additional output. The pixelstates are 8 bit wide.
#' bit 6 indicates that it was a location to interpolate before,bit 7 indicates that it is a clear pixel afterwards.
#' This results in the following states (other bits are 0):
#' 
#' \tabular{rrrr}{
#'   \strong{value} \tab \strong{b7} \tab \strong{b6} \tab \strong{meaning} \cr
#'     0\tab 0 \tab 0 \tab  Was nodata before and still is\cr
#'       64 \tab 0 \tab 1 \tab Could not be interpolated and is set to nodata\cr
#'         192 \tab 1 \tab 1 \tab Is interpolated \cr
#'           128 \tab 1 \tab 0 \tab Was clear before and still is. 
#'             }
#' \strong{Specific Formats}
#' 
#' Some arguments require inputs in a specific format.
#' \itemize{
#' \item{starfm: STARFM stands for spatial and temporal adaptive reflectance fusion model. It requires a relatively low amount of computation time for prediction. Supports singlepair and doublepair modes. See \link[ImageFusion]{starfm_job}.}
#' \item{estarfm: ESTARFM stands for enhanced spatial and temporal adaptive reflectance fusion model so it claims to be an enhanced STARFM. It can yield better results in some situations. Only supports doublepair mode. See \link[ImageFusion]{estarfm_job}.}
#' \item{fitfc: Fit-FC is a three-step method consisting of regression model fitting (RM fitting), spatial filtering (SF) and residual compensation (RC). It requires a relatively low amount of computation time for prediction. Supports singlepair or a pseudo-doublepair mode(For predictions between two pair dates, predictions will be done twice, once for each of the pair dates). See \link[ImageFusion]{fitfc_job}. This is the default algorithm.}
#' } 
#'  \itemize{
#'  \item \code{<num-list>} must be lists of integer vectors. Example: \code{list(c(1,3,3,7),c(4,2))} 
#'  \item \code{<range> must} be strings. Either be a single number or have the format \code{'[<float>,<float>]'}, \code{'(<float>,<float>)'}, \code{'[<float>,<float>'} or \code{'<float>,<float>]'} where the comma and round brackets are optional, but square brackets are here actual characters. Especially for half-open intervals do not use unbalanced parentheses or escape them (maybe with two '\\'). Additional whitespace can be added anywhere. Example: \code{"(125,175)"}
#'  \item \code{<range-list>} must be strings that combine ranges in the form \code{ '<range> [<range> ...]'}, where the brackets mean that further intervals are optional. The different ranges are related as union. Example: \code{"(125,175) [225,275]"}
#'  }
#' @author  Christof Kaufmann (C++), Johannes Mast (R)
#' @examples 
#' # Load required libraries
#' library(ImageFusion)
#' library(raster)
#' # Get filesnames of images with gaps
#' landsat_with_gaps <- list.files(system.file("landsat/unfilled",
#'                                             package = "ImageFusion"),
#'                                 ".tif",
#'                                 recursive = TRUE,
#'                                 full.names = TRUE)
#' # Create output Directory
#' if(!dir.exists("Outputs")) dir.create("Outputs", recursive = TRUE)
#' # Interpolate into output directory
#' imginterp_task(filenames = landsat_with_gaps,
#'               dates = c(68,77,93,100),limit_days = 15,
#'                invalid_ranges = "[-inf,-1]",
#'                out_prefix = "Outputs/")
#' # Get filenames of interpolated images
#' landsat_without_gaps <- list.files("Outputs",pattern = ".tif$",full.names = TRUE)
#' # Count the number of NAs before and after the interpolation
#' sum(is.na(getValues(stack(landsat_with_gaps[2]))))
#' sum(is.na(getValues(stack(landsat_without_gaps[2]))))
#' # remove the output directory
#' unlink("Outputs",recursive = TRUE)
#' 
#' 

 # \itemize{
 # \item{<num-list> must be lists of integer vectors. Example: \code{list(c(1,3,3,7),c(4,2))} }
 # \item <range> must be strings. Either be a single number or have the format \code{'[<float>,<float>]'}, \code{'(<float>,<float>)'}, \code{'[<float>,<float>'} or \code{'<float>,<float>]'} where the comma and round brackets are optional, but square brackets are here actual characters. Especially for half-open intervals do not use unbalanced parentheses or escape them (maybe with two '\\'). Additional whitespace can be added anywhere. Example: \code{"(125,175)"}
 # \item <range-list> must be strings that combine ranges in the form \code{ '<range> [<range> ...]'}, where the brackets mean that further intervals are optional. The different ranges are related as union. Example: \code{"(125,175) [225,275]"}
 # }
imginterp_task <- function(filenames,
                           dates,
                           tags=NULL,
                           layers=NULL,
                           limit_days=5,
                           output_pixelstate=FALSE,
                           use_nodata=TRUE,
                           interp_invalid=TRUE,
                           prioritize_invalid=FALSE,
                           interp_ranges=NULL,
                           no_interp_ranges=NULL,
                           valid_ranges=NULL,
                           invalid_ranges=NULL,
                           out_prefix=NULL,
                           out_postfix=NULL,
                           out_pixelstate_prefix=NULL,
                           out_pixelstate_postfix=NULL,
                           stats_filename=NULL,
                           print_stats=FALSE,
                           ql_filenames=NULL,
                           ql_dates=NULL,
                           ql_tags=NULL,
                           ql_layers=NULL,
                           ql_bits=NULL,
                           ql_interp_ranges=NULL,
                           ql_non_interp_ranges=NULL,
                           mask_filenames=NULL,
                           mask_layers=NULL,
                           mask_bits=NULL,
                           mask_valid_ranges=NULL,
                           mask_invalid_ranges=NULL,
                           verbose=FALSE){

  #combine the filenames and dates into image strings
  string_imgargs <- paste0(" --img='-f ",filenames," -d ",dates)
  string_imgargs <- paste0(" --img='-f ",'"',filenames,'"'," -d ",dates)
  if(!is.null(tags)){
    string_imgargs <- paste0(string_imgargs," -t ",tags)
  }
  #Add arguments for layers to the image strings if any were given
  if(!is.null(layers)){
    if(length(layers)==1){
      layerstring <- rep(layers,length(filenames))
      layerstring <- lapply(layerstring,FUN =  paste,collapse=",")
      layerstring <- do.call(c,layerstring)
    }else if(length(layers)==length(filenames)){
      layerstring <- lapply(layers,FUN =  paste,collapse=",")
      layerstring <- do.call(c,layerstring)
    }else{
      print("layers must be a list of length 1 or the same length as filenames.")
    }
    paste0(string_imgargs," -l ",layerstring)
  }
  
  #End each image string with a '
  string_imgargs <- paste0(string_imgargs,"'")
  #combine the image strings to one string
  string_args <- paste0(string_imgargs,collapse="")
  
  #If quality layer filenames were given, add them (with a similar syntax to the imgargs)
  if(!is.null(ql_filenames)){
    #combine the filenames and dates into image strings
    string_ql_imgargs <- paste0(" --ql='-f ",ql_filenames)
    if(!is.null(ql_dates)){
      string_ql_imgargs <- paste0(string_ql_imgargs," -d ",ql_dates)
    }
    if(!is.null(ql_tags)){
      string_ql_imgargs <- paste0(string_ql_imgargs," -t ",ql_tags)
    }
    #Add arguments for ql_layers to the ql_image strings if any were given
    if(!is.null(ql_layers)){
      if(length(ql_layers)==1){
        layerstring <- rep(ql_layers,length(ql_filenames))
        layerstring <- lapply(layerstring,FUN =  paste,collapse=",")
        layerstring <- do.call(c,layerstring)
      }else if(length(ql_layers)==length(ql_filenames)){
        layerstring <- lapply(ql_layers,FUN =  paste,collapse=",")
        layerstring <- do.call(c,layerstring)
      }else{
        print("ql_layers must be a list of length 1 or the same length as ql_filenames.")
      }
      paste0(string_ql_imgargs," -l ",layerstring)
    }
    #Add arguments for ql_bits to the ql_image strings if any were given
    if(!is.null(ql_bits)){
      if(length(ql_bits)==1){
        bitstring <- rep(ql_bits,length(ql_filenames))
        bitstring <- lapply(bitstring,FUN =  paste,collapse=",")
        bitstring <- do.call(c,bitstring)
      }else if(length(ql_bits)==length(ql_filenames)){
        bitstring <- lapply(ql_bits,FUN =  paste,collapse=",")
        bitstring <- do.call(c,bitstring)
      }else{
        print("ql_bits must be a list of length 1 or the same length as ql_filenames.")
      }
      paste0(string_ql_imgargs," -b ",bitstring)
    }
    #Add arguments for ql_interp_ranges to the ql_image strings if any were given
    if(!is.null(ql_interp_ranges)){
      if(length(ql_interp_ranges)==1){
        interprangestring <- rep(ql_interp_ranges,length(ql_filenames))
        interprangestring <- do.call(c,interprangestring)
      }else if(length(ql_interp_ranges)==length(ql_filenames)){
        interprangestring <- do.call(c,ql_interp_ranges)
      }else{
        print("ql_layers must be a list of length 1 or the same length as ql_filenames.")
      }
      string_ql_imgargs <- paste0(string_ql_imgargs,"  --interp-ranges= ",interprangestring)
    }
    #Add arguments for ql_non-interp_ranges to the ql_image strings if any were given
    if(!is.null(ql_non_interp_ranges)){
      if(length(ql_non_interp_ranges)==1){
        noninterprangestring <- rep(ql_non_interp_ranges,length(ql_filenames))
        noninterprangestring <- do.call(c,noninterprangestring)
      }else if(length(ql_non_interp_ranges)==length(ql_filenames)){
        noninterprangestring <- do.call(c,ql_non_interp_ranges)
      }else{
        print("ql_layers must be a list of length 1 or the same length as ql_filenames.")
      }
      string_ql_imgargs <- paste0(string_ql_imgargs,"  --non-interp-ranges= ",noninterprangestring)
    }
    #End each ql image string with a '
    string_ql_imgargs <- paste0(string_ql_imgargs,"'")
    #combine the ql image strings to one string
    string_qlargs <- paste0(string_ql_imgargs,collapse="")
    #add them to the string args
    string_args <- paste0(string_args, string_qlargs)
  }
  
  
  

  
  #Ifmask image filenames were given, add them (with a similar syntax to the imgargs)
  if(!is.null(mask_filenames)){
    #combine the filenames and dates into image strings
    string_mask_imgargs <- paste0(" --m='-f ",mask_filenames)

    #Add arguments for mask_layers to the mask_image strings if any were given
    if(!is.null(mask_layers)){
      if(length(mask_layers)==1){
        layerstring <- rep(mask_layers,length(mask_filenames))
        layerstring <- lapply(layerstring,FUN =  paste,collapse=",")
        layerstring <- do.call(c,layerstring)
      }else if(length(mask_layers)==length(mask_filenames)){
        layerstring <- lapply(mask_layers,FUN =  paste,collapse=",")
        layerstring <- do.call(c,layerstring)
      }else{
        print("mask_layers must be a list of length 1 or the same length as mask_filenames.")
      }
      paste0(string_mask_imgargs," -l ",layerstring)
    }
    #Add arguments for mask_bits to the mask_image strings if any were given
    if(!is.null(mask_bits)){
      if(length(mask_bits)==1){
        bitstring <- rep(mask_bits,length(mask_filenames))
        bitstring <- lapply(bitstring,FUN =  paste,collapse=",")
        bitstring <- do.call(c,bitstring)
      }else if(length(mask_bits)==length(mask_filenames)){
        bitstring <- lapply(mask_bits,FUN =  paste,collapse=",")
        bitstring <- do.call(c,bitstring)
      }else{
        print("mask_bits must be a list of length 1 or the same length as mask_filenames.")
      }
      paste0(string_mask_imgargs," -b ",bitstring)
    }
    #Add arguments for mask_valid_ranges to the mask_image strings if any were given
    if(!is.null(mask_valid_ranges)){
      if(length(mask_valid_ranges)==1){
        validrangestring <- rep(mask_valid_ranges,length(mask_filenames))
        validrangestring <- do.call(c,validrangestring)
      }else if(length(mask_valid_ranges)==length(mask_filenames)){
        validrangestring <- do.call(c,mask_valid_ranges)
      }else{
        print("mask_layers must be a list of length 1 or the same length as mask_filenames.")
      }
      string_mask_imgargs <- paste0(string_mask_imgargs,"  --valid-ranges= ",validrangestring)
    }
    #Add arguments for mask_non-interp_ranges to the mask_image strings if any were given
    if(!is.null(mask_invalid_ranges)){
      if(length(mask_invalid_ranges)==1){
        ivalidrangestring <- rep(mask_invalid_ranges,length(mask_filenames))
        ivalidrangestring <- do.call(c,ivalidrangestring)
      }else if(length(mask_invalid_ranges)==length(mask_filenames)){
        ivalidrangestring <- do.call(c,mask_invalid_ranges)
      }else{
        print("mask_layers must be a list of length 1 or the same length as mask_filenames.")
      }
      string_mask_imgargs <- paste0(string_mask_imgargs,"  --invalid-ranges= ",ivalidrangestring)
    }
    #End each mask image string with a '
    string_mask_imgargs <- paste0(string_mask_imgargs,"'")
    #combine the mask image strings to one string
    string_maskargs <- paste0(string_mask_imgargs,collapse="")
    #add them to the string args
    string_args <- paste0(string_args, string_maskargs)
  }
  
  
  
  
  
  
  
  
  
  
  #Add string for the day limit
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
  #If valid-ranges are given, add them, otherwise use a faux range that means no values will be interpolated
  if(!is.null(valid_ranges)){
    string_args <- paste0(string_args, " --mask-valid-ranges=",valid_ranges)
  }
  #If invalid-ranges are given, add them
  if(!is.null(invalid_ranges)){
    string_args <- paste0(string_args, " --mask-invalid-ranges=",invalid_ranges)
  }
  
  #IF desired, use nodata
  if(use_nodata){
    string_args <- paste0(string_args, " --enable-use-nodata  ")
  }else{
    string_args <- paste0(string_args, " --disable-use-nodata  ")
  }
  #IF desired, also interpolate missing values
  if(interp_invalid){
    string_args <- paste0(string_args, " --enable-interp-invalid ")
  }else{
    string_args <- paste0(string_args, " --disable-interp-invalid ")
  }
  #IF desired, also prioritize missing values
  if(prioritize_invalid){
    string_args <- paste0(string_args, " --enable-prioritize-invalid ")
  }else{
    string_args <- paste0(string_args, " --enable-prioritize-interp ")
    
  }
  #IF desired, output pixelstate
  if(output_pixelstate){
    string_args <- paste0(string_args, " --enable-output-pixelstate")
  }else{
    string_args <- paste0(string_args, " --disable-output-pixelstate")
  }
  #If outprefix is given, add it
  if(!is.null(out_prefix)){
    string_args <- paste0(string_args, " --out-prefix=",out_prefix)
  }
  #If outpostfix is given, add it
  if(!is.null(out_postfix)){
    string_args <- paste0(string_args, " --out-postfix=",out_postfix)
  }
  #If out_pixelstate_pprefix is given, add it
  if(!is.null(out_pixelstate_prefix)){
    string_args <- paste0(string_args, " --out-pixelstate-prefix=",out_prefix)
  }
  #If out_pixelstate_postfix is given, add it
  if(!is.null(out_pixelstate_postfix)){
    string_args <- paste0(string_args, " --out-pixelstate-postfix=",out_postfix)
  }
  #IF desired, also output stats to file or console
  if(!is.null(stats_filename)){
    string_args <- paste0(string_args, " --stats=", stats_filename)
  }else if(print_stats){
    string_args <- paste0(string_args, " --stats")
  }
  #Print the string one last time
  if(verbose){print(string_args)}
  #Run the job
  execute_imginterp_job_cpp(input_string = string_args,verbose=verbose)
}


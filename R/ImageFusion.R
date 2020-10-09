#' Imagefusion: A package for fusion of images
#'
#' @description Implementation of Time-Series fusion.
#' @section Outline:
#' \code{ImageFusion} provides implementations of the following time-series fusion algorithms:
#' \itemize{
#' \item ESTARFM
#' \item FITFC
#' \item SPSTFM
#' \item STARFM
#' } 
#' More algorithms will be added over time.
#' Also provides \link{imginterp_task}, a utility function for the linear interpolation of masked or missing values
#' @section Implementation:
#' The algorithms are implemented in C++ via 'GDAL', 'opencv' and 'Boost' and work from input images on disk without prior loading into R.
#' @section Usage:
#' Use the \link{imagefusion_task} function to set up a complete time-series fusion task. Use the algorithm-specific functions \link{fitfc_job} \link{estarfm_job} \link{spstfm_job} and \link{starfm_job} to execute individual jobs.
#'
#' @docType package
#' @name ImageFusion
#' @author Christof Kaufmann (C++)
#' @author Johannes Mast (R)
#' @useDynLib packagename, .registration = TRUE
#' @md
NULL
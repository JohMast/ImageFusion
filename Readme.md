## Introduction
Temporal fusion of raster image time-Series. R interface for the ImageFusion framework, which provides implementation of the FITFC, ESTARFM, SPSTFM and STARFM algorithms in C++. 


<<<<<<< HEAD
### Background

=======
###Background
>>>>>>> bd361cbbc0c546e69af7bb3ef4c244db75878d76
When performing time-series analysis of remote-sensing data, one often faces the choice between the different sensors which produce freely available imagery. 
* *High-Resolution:* Images with a high spatial resolution but low temporal resolution (Sentinel, Landsat)
* *Low-Resolution:* Images with a low spatial resolution and high temporal resolution (MODIS)
This is a rough categorization, and the capabilities and number of platforms is great and ever increasing. This provides great opportunities but also the challenge on how to best combine different sources.

<<<<<<< HEAD
### Principle

=======
###Principle
>>>>>>> bd361cbbc0c546e69af7bb3ef4c244db75878d76
Fusion is a process by which we can combine information from two temporally overlapping time series to create a single time series with the temporal resolution of the *Low-Resolution* inputs and the spatial resolution of the *High-Resolution* inputs, resulting in an output time series with high spatial *and* high temporal resolution.

The basic principle is to seek **pair-dates** on which images for both time series are available, and finding the relation between both images on that date. The relation is then applied to those dates for which only *Low_Resolution* images are available. The details vary depending on the fusion algorithm. A variety of algorithms exist, and the ImageFusion framework is intended to be continuously extended by users and scientists. 

<<<<<<< HEAD
So far, the following algorithms are implemented :
=======
So far, the following algorithms are implemented:
>>>>>>> bd361cbbc0c546e69af7bb3ef4c244db75878d76

* FITFC
* ESTARFM
* SPSTFM
* STARFM

## Installation
<<<<<<< HEAD

=======
>>>>>>> bd361cbbc0c546e69af7bb3ef4c244db75878d76
The development version can be installed from GitHub:

```r
devtools::install_github("JohMast/ImageFusion")
```

## Usage

<<<<<<< HEAD
### ImageFusion functions

=======
###ImageFusion functions
>>>>>>> bd361cbbc0c546e69af7bb3ef4c244db75878d76
The core of the package are functions for the algorithms ESTARFM, FITFC, SPSTFM and STARFM. 

Individual *jobs* can be executed using the `estarfm_job`, `fitfc_job`, `spstfm_job` and `starfm_job` functions. These jobs require one or two "pair dates"" on which both high and low resolution images are available and offer the fusion of images for all dates for which only low resolution images are available.

For the creation of long time series of many pair dates, the function `imagefusion_task`is available. This function automatically detects pair dates and splits the time-series *task* into as many *jobs* as is supported by the inputs. 

ImageFusion also provides an additional utility function `imginterp_task` which allows for the linear interpolation of missing or masked values. We recommend to use this before the fusion to replace any missing or masked values, which usually results in a much better fusion result.

### File Handling
<<<<<<< HEAD

=======
>>>>>>> bd361cbbc0c546e69af7bb3ef4c244db75878d76
All functions in `ImageFusion`operate from disk to disk - Input images are read using [GDAL](https://gdal.org/drivers/raster/index.html) and output images are written into an output directory on disk. Note that the input images must be matching in spatial extent and resolution. We recommend to use [getSpatialData](https://github.com/16EAGLE/getSpatialData) package for automatized data download and the [raster](https://cran.r-project.org/web/packages/raster/index.html) package or [GDAL](https://gdal.org) for the preprocessing.

## Performance

Imagefusion is expensive and can take a long time for long time series using large images. `ImageFusion` is intended to allow the processing to be done in a reasonable time. The core algorithms are implemented in C++, exposed to R via [Rcpp](http://rcpp.org/). If [OPENMP](https://www.openmp.org/) support is available, it can be used for parallelisation. 

## Outlook

Additional packages are in development for the 
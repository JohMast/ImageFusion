# ImageFusion

<img src=".img/logo.png" align="right" src="doc/images/logo.png">
## Introduction
Temporal fusion of raster image time-Series. R interface for the imagefusion framework, which provides implementation of the FITFC, ESTARFM, SPSTFM and STARFM algorithms in C++. 


### Background

When performing time-series analysis of remote-sensing data, one often faces the choice between the different sensors which produce freely available imagery. 
* *High-Resolution:* Images with a high spatial resolution but low temporal resolution (Sentinel, Landsat)
* *Low-Resolution:* Images with a low spatial resolution and high temporal resolution (MODIS)
This is a rough categorization, and the capabilities and number of platforms is great and ever increasing. This provides great opportunities but also the challenge on how to best combine different sources.

### Principle

Fusion is a process by which we can combine information from two temporally overlapping time series to create a single time series with the temporal resolution of the *Low-Resolution* inputs and the spatial resolution of the *High-Resolution* inputs, resulting in an output time series with high spatial *and* high temporal resolution.

The basic principle is to seek *pair-dates* on which images for both time series are available, and finding the relation between both images on that date. The relation is then applied to those dates for which only *Low_Resolution* images are available. The details vary depending on the fusion algorithm. A variety of algorithms exist, and the ImageFusion framework is intended to be continuously extended by users and scientists. 

So far, the following algorithms are implemented:

* FITFC
* ESTARFM
* SPSTFM
* STARFM

## Installation

The development version can be installed from GitHub:

```r
devtools::install_github("JohMast/ImageFusion")
```

## Usage


### Jobs

The core of the package are functions for the algorithms ESTARFM, FITFC, SPSTFM and STARFM. 

Individual *jobs* can be executed using the `estarfm_job`, `fitfc_job`, `spstfm_job` and `starfm_job` functions. These jobs require one or two *pair dates* on which both high and low resolution images are available. The *pair dates* support an interval and the job offer the fusion of images for all dates within this interval for which only low resolution images are available.

Some algorithms also offer a *singlepair mode* which allows fusion based on only a single pair. The FITFC algorithm operates exclusively in singlepair mode.
```r
estarfm_job(input_filenames = list_of_input_filenames,
            input_resolutions = c("high","high","low","low","low","low",
              "low","low", "low","low","low","low"),
            input_dates = c(68,77,68,69,70,71,72,73,74,75,76,77),
            pred_dates = c(72,74))
```
Here we pass all image filenames as a single input vector. The tags (whether they are high or low resolution) is a parallel vector of equal length and order. A third such vector passes the date for each image. Note how we have two dates (68 and 77) for which we have both "high" and "low" available. These dates will be used as pair dates.

### Tasks

For the creation of long time series of many pair dates, the function `imagefusion_task`is available. This function automatically detects pair dates and splits the time-series *task* into as many *jobs* as is supported by the inputs. 
```r
imagefusion_task(filenames_high = list_of_high_resolution_filenames,
                 dates_high = c(68,77,93,100),
                 filenames_low = list_of_low_resolution_filenames,
                 dates_low = 68:93,
                 dates_pred = c(65,70,75,80,85,90,95))
```
For the creation of a task, we pass the high and low filenames and dates seperately. Note that here we have multiple high-resolution images (68, 77, 93, 100) and multiple pair dates (68, 77, 93). The task will therefore be split up into different jobs, one for each interval.

### Utilities

ImageFusion also provides an additional utility function `imginterp_task` which allows for the linear interpolation of missing or masked values. We recommend to use this before the fusion to replace any missing or masked values, which usually results in a much better fusion result.

```r
imginterp_task(filenames = list_of_input_images,dates = c(68,77,93,100))
```

### File Handling

All functions in `ImageFusion`operate from disk to disk - Input images are read using [GDAL](https://gdal.org/drivers/raster/index.html) and output images are written into an output directory on disk. Note that the input images must be matching in spatial extent and resolution. We recommend to use [getSpatialData](https://github.com/16EAGLE/getSpatialData) package for automatized data download and the [raster](https://cran.r-project.org/web/packages/raster/index.html) package or [GDAL](https://gdal.org) for the preprocessing.

## Performance

Fusion is expensive and can take a long time for long time series using large images. `ImageFusion` is intended to allow the processing to be done in a reasonable time. The core algorithms are implemented in C++, exposed to R via [Rcpp](http://rcpp.org/). If [OPENMP](https://www.openmp.org/) support is available, it can be used for parallelization. 

## Outlook

Additional packages are in development for the preprocessing before the fusion and the visualization and analysis of the fused time series. Check [our website](http://remote-sensing.org/) for latest developments.
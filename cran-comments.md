Dear CRAN-Team,

This is our resubmission of our ImageFusion package. 

We have done our best to thoroughly test our code and follow all best practices.
Thank you for your support.

Johannes Mast (Package Maintainer)

## Changes

In Response to the comments by David Ripley, we have made the following changes:

* DESCRIPTION: Added C++17 as a System Requirement
* Changed the System Requirements from Debian format to the project names (e.g. libgdal -> GDAL) .

* configure: We are not able to perform tests on macos. Thus, we follow the example of the R package opencv whicha ccording to its [cran checks](https://cran.r-project.org/web/checks/check_results_opencv.html) runs fine across many different platforms. We adopt the code used in its configure file to dynamically create the variables PKG_LIBS_OPENCV and OPENCV_FLAG for use in Makevars. There, they replace the previously hardcoded paths and libraries.
* 2 DO: Include jeroen ooms as copyright holder for that code
* Rbuildignore: Added the configure.log file to the list of ignored files (logs opencv libraries)

## Test environments
* local windows 10, R 4.0.3
* local windows 10, R under development (unstable) (2021-02-17 r80023)
* local ubuntu 20.04, R 4.0.2
* win-builder  R version 4.0.4 (2021-02-15)
* win-builder  R Under development (unstable) (2021-02-27 r80059)

## R CMD check results

### ERRORS

There were no ERRORs.

### WARNINGS

There were no WARNINGs.

### NOTES

#### NOTE 1:
checking installed package size ... NOTE
    installed size is 61.3Mb
    sub-directories of 1Mb or more:
      libs   59.2Mb
      modis   1.4Mb
      
The great size mostly due to our inclusion of the gdal library,
which is required by the c++ library we are building on. 
Therefore, the library is not easily substituted.

Other packages handling raster files,
such as [sf](https://cran.r-project.org/web/packages/sf/index.html) or [gdalcubes](https://cran.r-project.org/web/packages/gdalcubes/index.html),
are also affected by this issue.
We appreciate any ideas on how to solve this note,
as we have no influence over the size of the GDAL library.

## Downstream dependencies

There are no downstream dependencies yet,
although our intent is to produce some in the future.
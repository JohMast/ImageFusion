Dear CRAN-Team,

This is our resubmission of our ImageFusion package. 

We have done our best to thoroughly test our code and follow all best practices.
Thank you for your support.

Johannes Mast (Package Maintainer)

## Changes

In Response to the comments by David Ripley, we have made the following changes:

* DESCRIPTION: Added C++17 as a System Requirement
* Changed the System Requirements from Debian format to the project names (e.g. libgdal -> GDAL).
* configure.ac & configure: At present, we are not able to perform tests on macOS. Thus, we follow the example of the R package opencv which according to its [cran checks](https://cran.r-project.org/web/checks/check_results_opencv.html) runs fine across many different platforms. We adopt the code used in its configure file to dynamically create the variables PKG_LIBS_OPENCV and OPENCV_FLAG for use in Makevars. There, they replace the previously hardcoded paths and libraries.
* configure.ac & configure: Maintaining openmp capability on macOS is not [trivial](https://thecoatlessprofessor.com/programming/cpp/openmp-in-r-on-os-x/). As our capability to test on macOS is very limited, we follow the example of [RcppArmadillo](https://github.com/RcppCore/RcppArmadillo) and the recommendation of this [stackoverflow post](https://stackoverflow.com/q/46723854). Instead of hardcoding -fopenmp as before, the updated configure process creates the flags OPENMP_FLAG (SHLIB_OPENMP_CXXFLAGS) for use in Makevars, as suggested by the [R Extensions Manual](https://cran.r-project.org/doc/manuals/r-release/R-exts.html#OpenMP-support). 
* Makevars.in: See changes above.
* Makevars.win: See changes above.
* inst/Copyrights: To acknowledge the original sources which we base the above modifications on, we have credited the contribution of the original authors.
* src/: Also, we now consistently use _OPENMP to only activate openmp-based parallelization if openmp was detected. We previously used a custom variable WITH_OMP that was required by in our wrapped imagefusion code. For simplicity and portability, we now make this change.


We have made further changes:

* DESCRIPTION: Increased Version number to 0.0.2
* NEWS.md: Updated news according to the changes.
* Rbuildignore: Added the configure.log file to the list of ignored files (This file logs opencv libraries).
* R/estarfm_job.R: Fixed a bug resulting from a typo.
* configure.ac: Updated the PROJ and GDAL sections to follow the example of the gdalcubes package which locates GDAL on all [checked](https://cran.r-project.org/web/checks/check_results_gdalcubes.html) platforms.

## Test environments
* local windows 10, R 4.0.3
* local windows 10, R under development (unstable) (2021-02-17 r80023)
* local ubuntu 20.04, R 4.0.2
* win-builder  R version 4.0.5 (2021-03-31)
* win-builder  R version 4.1.0 alpha (2021-05-01´ r80245)

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
Dear CRAN-Team,

This is our resubmission of our ImageFusion package. 

We have done our best to thoroughly test our code and follow all best practices.
Thank you for your support.

Johannes Mast (Package Maintainer)

## Changes

In response to the comments by Uwe Ligges, we have made the following changes:

To address general issues on platforms using clang:
* DESCRIPTION: Added C++17 as a System Requirement.
* configure.ac: Set C++ before include AC_LANG().

To address issues of macOS portability:
configure.ac: Adapted the configuration process of PROJ from the recent version 
of rgdal to comply with the most recent requirements of PROJ.
* Makevars.in: See changes above.
* Makevars.win: See changes above.

* src/include/filesystem.h: Now wraps around GDAL filesystem functions (path stem and extension) instead of relying on the std::filesystem which may not be provided on certain macOS. This is adapted from the r package gdalcubes by Marius Appel.
* src/include/filesystem.cpp: See changes above.
* src/image.cpp: See changes above.
* src/utils/helpers/utils_common.cpp: See changes above.
* src/utils/imginterp/interpolation.h: See changes above.
* inst/Copyrights: Added rgdal author as original copyright holder for the GDAL and PROJ configure sections. Added Marius Appel as the source for the filesystem gdal wrapper.


We have made further changes:
* removed superfluous files of the imagefusion library to reduce the overall size of the package slightly.
* Makevars.in: Increased Version number to 0.0.3.
* Makevars.win: Increased Version number to 0.0.3.
* DESCRIPTION: Increased Version number to 0.0.3.
* NEWS.md: Updated news according to the changes.


## Test environments
* local windows 10, R 4.0.3
* local windows 10, R under development (unstable) (2021-02-17 r80023)
* local ubuntu 20.04, R 4.0.2
* win-builder  R version 4.0.5 (2021-03-31)
* win-builder  R version 4.1.0 RC (2021-05-06´ r80288)

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
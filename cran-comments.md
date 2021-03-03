Dear CRAN-Team,

This is our resubmission of our ImageFusion package. 

We have done our best to thoroughly test our code and follow all best practices.
Thank you for your support.

Johannes Mast (Package Maintainer)

## Changes
In response to the e mail conversation with Julia Haider, we have made the following changes:

* DESCRIPTION: Changed the spellings of "time-Series" to "time series" and "c++" to "C++".
* LICENSE.note: Added Hochschule Bochum explicitely as the copyright holder of the imagefusion C++ code.
* R/fitfc_job.R:  Changed example to write to tempdir instead of the current directory.
* R/starfm_job.R:  Changed example to write to tempdir instead of the current directory.
* R/estarfm_job.R: Changed example to write to tempdir instead of the current directory.
* R/imagefusion_task.R: Changed example to write to tempdir instead of the current directory. Accordingly, changed how the default output directory is chosen by the functions.
* R/imginterp_task.R: Changed example to write to tempdir instead of the current directory. Accordingly, changed how the default output directory is chosen by the functions.

We have made further changes:
* Removed superflous comments across various functions.


## Previous Changes
In response to the e mail conversation with Uwe Ligges, we have previously made the following changes:
* LICENSE: Omitted superfluous newlines.
* LICENSE.note: Omitted superfluous newlines.
* DESCRIPTION: Added Hochschule Bochum as the copyright holder of the imagefusion source code.
* DESCRIPTION: Added reference to file inst/Copyrights.
* R/imagefusion_task.R: Added references to the underlying algorithms to the documentation.
* inst/Copyrights: Created text file listing copyrights.

We have made further changes:
* Readme.md: Fixed a typo.
* DESCRIPTION: Added en-US as the language of the package. Ensured that American spelling is used consistently.


## Test environments
* local windows 10, R 4.0.3
* local windows 10, R under development (unstable) (2021-02-17 r80023)
* local ubuntu 20.04, R 4.0.2
* win-builder  R version 4.0.4
* win-builder  R Under development (unstable) (2021-02-27 r80043)

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
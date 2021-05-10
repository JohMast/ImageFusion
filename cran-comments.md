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
* inst/Copyrights: Added rgdal author as original copyright holder for the GDAL and PROJ configure sections.

We have made further changes:
* removed superfluous files of the imagefusion library to reduce the overall size of the package slightly.
* Makevars.in: Increased Version number to 0.0.3
* Makevars.win: Increased Version number to 0.0.3
* DESCRIPTION: Increased Version number to 0.0.3
* NEWS.md: Updated news according to the changes.


## Test environments
* local windows 10, R 4.0.3
* local windows 10, R under development (unstable) (2021-02-17 r80023)
* local ubuntu 20.04, R 4.0.2
* win-builder  R version 4.0.5 (2021-03-31)
* win-builder  R version 4.1.0 beta (2021-05-06´ r80268)


## Note
We are currently unable to test on macOS, as we lack access to a macOS machine and the rhub builder does not appear to supply the system requirements. Nevertheless, we can suspect certain issues which may arise:
The imagefusion c++ library, which is wrapped by this R package, relies for its file handling on std::filesystem, which may not be available on older macOS systems, as described in this thread:
[stackoverflow](https://stackoverflow.com/questions/49577343/filesystem-with-c17-doesnt-work-on-my-mac-os-x-high-sierra)


As an alternative, we consider a dedicated, header-only helper library:
[filesystem](https://github.com/gulrak/filesystem)


However, this library contains pragmas that suppress diagnostics, and result in additional WARNINGS.
Since, to our understanding, the filesystem library is supplied since macOS 10.15 onward (that is, since October 2019), this may not be an issue. If it is, we will appreciate any guidance on this matter.

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

#### Note 2:
Non-standard file/directory found at top level:
  'tmp'
  
We are unsure what causes this error, as we are not able to reproduce it on any of our local machines. 

## Downstream dependencies

There are no downstream dependencies yet,
although our intent is to produce some in the future.
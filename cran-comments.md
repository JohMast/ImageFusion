Dear CRAN-Team,

This is my first submission of our ImageFusion package. 

We have done our best to thoroughly test our code and follow all best practices.
Thank you for your support.

Johannes Mast (Package Maintainer)

## Test environments
* local windows 10, R 4.0.2
* local ubuntu 20.04, R 4.0.2
* win-builder  R 4.0.3
* win-builder  R Under development (unstable) (2020-10-15 r79342)

## R CMD check results

### ERRORS

There were no ERRORs.

### WARNINGS

There were no WARNINGs.

### NOTES
#### NOTE 1:
This is my first submission.

#### NOTE 2:
checking installed package size ... NOTE
    installed size is 103.6Mb
    sub-directories of 1Mb or more:
      gdal    3.8Mb
      libs   83.1Mb
      modis   1.4Mb
      proj    5.0Mb
      share   9.7Mb
      
The great size mostly due to our inclusion of the gdal library, which is required by the c++ library we are building on. Therefore, the library is not easily substituted.

Other packages handling raster files, such as [sf](https://cran.r-project.org/web/packages/sf/index.html) or [gdalcubes](https://cran.r-project.org/web/packages/gdalcubes/index.html), also suffer from 
this.
Do you have any ideas on how to solve this note, as we have no influence over the size of the GDAL library.

#### NOTE 3:
checking compiled code ... NOTE
File 'ImageFusion/libs/i386/ImageFusion.dll':
  Found '_ZSt4cerr', possibly from 'std::cerr' (C++)

The c++ library we are building on utilizes std::cout and std::cerr.
In the wrapper functions for the c++ library, we use Rcout and Rcerr provided
by Rcpp. We do not use Rcout for the c++ code, even if it would be as trivial as
replacing all instances of cout with Rcout.
Our aim is to wrap around this c++ code without making any changes to the c++ code,
as we want to keep the library and our R package synchronized. 
We hope our reasoning is correct and understandable. 
We implement a solution discussed in [this thread](http://lists.r-forge.r-project.org/pipermail/rcpp-devel/2018-October/010162.html), which does not remove the NOTE, but adresses the issue.
We will also be very thankful for any other suggestions on how to best handle this issue.

## Downstream dependencies

There are no downstreams dependencies yet, although our intent is to produce some in the future.
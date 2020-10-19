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
We hope to solve this problem and, in time, provide a suitable alternative.

    
** running examples for arch 'i386' ... [24s] NOTE
Examples with CPU (user + system) or elapsed time > 10s
           user system elapsed
spstfm_job 10.4   0.03   10.43


The SPSTFM algorithm is fairly slow compared to the other algorithms implemented.
We considered reduce the example, but prefer to keep it as similar as possible to the examples
of the other functions(estarfm_job, starfm_job, fitfc_job), using the same format and data. This will allow the user to compare the algorithms more fairly.

  
  
## Downstream dependencies

There are no downstreams dependencies yet, although our intent is to produce some in the future.
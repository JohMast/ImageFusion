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
We hope to solve this problem and, in time, provide a suitable alternative.

#### NOTE 3:
** running examples for arch 'i386' ... [24s] NOTE
Examples with CPU (user + system) or elapsed time > 10s
           user system elapsed
spstfm_job 10.4   0.03   10.43


The SPSTFM algorithm is fairly slow compared to the other algorithms implemented.
We considered reduce the example, but prefer to keep it as similar as possible to the examples
of the other functions(estarfm_job, starfm_job, fitfc_job), using the same format and data. This will allow the user to compare the algorithms more fairly.

#### NOTE 4:
checking compiled code ... NOTE
File 'ImageFusion/libs/i386/ImageFusion.dll':
  Found '_ZSt4cerr', possibly from 'std::cerr' (C++)
    Objects: 'execture_imagefusor_jobs.o', 'src/geoinfo.o',
      'src/spstfm_impl.o', 'src/spstfm.o', 'src/image.o',
      'src/fitfc.o', 'src/optionparser.o',
      'utils/helpers/utils_common.o'
  Found '_ZSt4cout', possibly from 'std::cout' (C++)
    Objects: 'execture_imagefusor_jobs.o', 'execture_imginterp_job.o',
      'src/staarch.o', 'src/spstfm_impl.o', 'src/spstfm.o',
      'src/image.o', 'utils/helpers/utils_common.o'
File 'ImageFusion/libs/x64/ImageFusion.dll':
  Found '_ZSt4cerr', possibly from 'std::cerr' (C++)
    Objects: 'execture_imagefusor_jobs.o', 'src/geoinfo.o',
      'src/spstfm_impl.o', 'src/spstfm.o', 'src/image.o',
      'src/fitfc.o', 'src/optionparser.o',
      'utils/helpers/utils_common.o'
  Found '_ZSt4cout', possibly from 'std::cout' (C++)
    Objects: 'execture_imagefusor_jobs.o', 'execture_imginterp_job.o',
      'src/staarch.o', 'src/spstfm_impl.o', 'src/spstfm.o',
      'src/image.o', 'utils/helpers/utils_common.o'

The c++ library we are building on utilizes std::cout and std::cerr.
In the wrapper functions for the c++ library, we use Rcout and Rcerr provided
by Rcpp. We do not use Rcout for the c++ code, even if it would be as trivial as
replacing all instances of cout with Rcout.
Our aim is to wrap around this c++ code without making any changes to the c++ code,
as we want to keep the library and our R package synchronized. 
We hope our reasoning is correct and understandable. We will also be very thankful
for any suggestions on how to best handle this situation.
## Downstream dependencies

There are no downstreams dependencies yet, although our intent is to produce some in the future.
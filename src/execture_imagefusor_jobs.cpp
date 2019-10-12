#include <Rcpp.h>
#include "Starfm.h"
#include "Estarfm.h"
#include "fitfc.h"
#include "spstfm.h"

using namespace Rcpp;
using namespace imagefusion;
using  std::vector;


// [[Rcpp::export]]
void execute_estarfm_job(CharacterVector input_filenames,
                         CharacterVector input_resolutions,
                         IntegerVector input_dates) {
  EstarfmFusor esf;

}



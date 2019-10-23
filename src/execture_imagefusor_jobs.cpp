#include <Rcpp.h>
#include "Starfm.h"
#include "Estarfm.h"
#include "fitfc.h"
#include "spstfm.h"

using namespace Rcpp;

using  std::vector;


//' @export
// [[Rcpp::export]]
void execute_estarfm_job_cpp(CharacterVector input_filenames, //character vector length n_i
                         CharacterVector input_resolutions, //character vector length n_i
                         IntegerVector input_dates, //character vector length n_i
                         IntegerVector pred_dates, //vector of length n_o
                         CharacterVector pred_filenames,   //vector of length n_o
                         IntegerVector pred_area, //vector of x1 y1 x2 y2) 
                         const std::string& hightag,  //string identifying the high res images
                         const std::string& lowtag  //string identifying the low res images
                           )
  {

  using namespace imagefusion;
  //Step 1: Prepare the Inputs
  //(This function assumes that the inputs are all in correct length and type,
  //any checks, assertions, conversions, and error handling
  //are performed within the R wrapper function)

  Rectangle pred_rectangle = Rectangle{pred_area[0],pred_area[1],pred_area[2],pred_area[3]};
  //Step 2: Prepare the fusion
  //Get the Geoinformation from the first of the images
  GeoInfo gi{as<std::string>(input_filenames[1])};

  //Loop over all the input filenames and load them into a MultiResImages object
  //for every filename, set the corresponding resolution tag
  //and date
  auto mri = std::make_shared<MultiResImages>();
  int n_inputs = input_filenames.size();
  for(int i=0; i< n_inputs;++i){
    mri->set(as<std::string>(input_resolutions[i]),
             input_dates[i],
                  Image(as<std::string>(input_filenames[i])));
  }
  
  //Set the desired Options
  EstarfmOptions o;
  o.setHighResTag(hightag);
  o.setLowResTag(lowtag);
  o.setDate1(1);
  o.setDate3(3);
  o.setPredictionArea(pred_rectangle);
  
  
  
  
  
  
  

  //Step 3: Fusion
  //Create the Fusor
  EstarfmFusor esf;
  esf.srcImages(mri); //Pass Images
  esf.processOptions(o);   //Pass Options
  
  //Predict for desired Dates
  int n_outputs = pred_dates.size();
  for(int i=0; i< n_outputs;++i){
    //Get the destination filename
    std::string pred_filename=as<std::string>(pred_filenames[i]);
    //Predict
    esf.predict(pred_dates[i]);
    //Write to destination
    esf.outputImage().write(pred_filename);
    
    //Add the Geoinformation to the written file
    if (gi.hasGeotransform()) {
      gi.geotrans.translateImage(pred_rectangle.x, pred_rectangle.y);
      if (pred_rectangle.width != 0)
        gi.size.width = pred_rectangle.width;
      if (pred_rectangle.height != 0)
        gi.size.height = pred_rectangle.height;
      gi.addTo(pred_filename);
    }
    
  }
}











starfm_job <- function(input_filenames,input_resolutions,input_dates,pred_dates,pred_filenames,pred_area,winsize,date1,date3,n_cores,data_range_min, data_range_max, logscale_factor,spectral_uncertainty, temporal_uncertainty, number_classes,hightag,lowtag,MASKIMG_options,MASKRANGE_options,use_local_tol,use_quality_weighted_regression,output_masks,use_nodata_value,use_parallelisation,use_strict_filtering,use_temp_diff_for_weights 
) {
  library(assertthat)
  library(raster)
  
  ##### A: Check all the Optional Inputs #####
  #These are variables which are optional 
  # or can easily infered from the required inputs
  
  #### pred_area ####
  # check pred area.
  # if not provided by user, set pred area to max image size of first image
  # TO DO: MAKE SURE THAT THE SPECIFIED IMAGE COORDINATES FIT INTO THE BOUNDING BOX
  # TO DO: ADD AN OPTION TO USE GEOCOORDINATES
  #Use the first image as a template #TO DO: MAY MAKE MORE SENSE TO USE THE FIRST LOW-ONLY IMAGE AS TEMPLATE
  template <- raster::stack(input_filenames[1])
  #If a bbox was provided, check it for plausibility and pass it on 
  if(!missing(pred_area)){
    assert_that(
      length(pred_area)==4,
      class(pred_area)=="numeric",
      pred_area[1]+pred_area[3]<=template@ncols,
      pred_area[2]+pred_area[4]<=template@nrows
    )
    pred_area_c <- pred_area
    
  }else{
    print("No Prediction Area specified. Predicting entire extent of:")
    print(template[[1]]@file@name)
    pred_area_c <- c(0,0,template@ncols,template@nrows)
    #pred_area_c <- template@extent[c(1,3,2,4)]  #Geocoordinates
  }
  
  #### winsize ####
  if(!missing(winsize)){
    assert_that(class(winsize)=="numeric")
    winsize_c <- winsize
  }else{
    winsize_c <- 51
  }
  
  
  #### output_masks ####
  if(!missing(output_masks)){
    assert_that(class(output_masks)=="logical")
    output_masks_c <- output_masks
  }else{
    output_masks_c <- FALSE
  } 
  
  #### use_nodata_value ####
  if(!missing(use_nodata_value)){
    assert_that(class(use_nodata_value)=="logical")
    use_nodata_value_c <- use_nodata_value
  }else{
    use_nodata_value_c <- TRUE
  } 
  
  #### use_parallelisation ####
  if(!missing(use_parallelisation)){
    assert_that(class(use_parallelisation)=="logical")
    use_parallelisation_c <- use_parallelisation
  }else{
    use_parallelisation_c <- FALSE
  }   
  
  #### use_strict_filtering ####
  if(!missing(use_strict_filtering)){
    assert_that(class(use_strict_filtering)=="logical")
    use_strict_filtering_c <- use_strict_filtering
  }else{
    use_strict_filtering_c <- FALSE
  } 
  
  #### use_temp_diff_for_weights ####
  if(!missing(use_temp_diff_for_weights)){
    assert_that(class(use_temp_diff_for_weights)=="logical")
    use_temp_diff_for_weights_c <- use_temp_diff_for_weights
  }else{
    use_temp_diff_for_weights_c <- TRUE
  } 
  #### number_classes ####
  if(!missing(number_classes)){
    assert_that(class(number_classes)=="numeric")
    number_classes_c <- number_classes
  }else{
    number_classes_c <- 40
  }
  

  #### data_range_min ####
  if(!missing(data_range_min)){
    assert_that(class(data_range_min)=="numeric")
    data_range_min_c <- data_range_min
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U"){data_range_min_c <- 0}
    if(template_dataType=="INT1S"){data_range_min_c <- -127}
    if(template_dataType=="INT2U"){data_range_min_c <- 0}
    if(template_dataType=="INT2S"){data_range_min_c <-  -32767}
    if(template_dataType=="FLT4S"){data_range_min_c <- -3.4e+38}
    if(template_dataType=="FLT8S"){data_range_min_c <- -1.7e+308}
  }
  
  
  #### data_range_max ####
  if(!missing(data_range_max)){
    assert_that(class(data_range_max)=="numeric")
    data_range_max_c <- data_range_max
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U"){data_range_max_c <- 255}
    if(template_dataType=="INT1S"){data_range_max_c <- 127}
    if(template_dataType=="INT2U"){data_range_max_c <- 65534}
    if(template_dataType=="INT2S"){data_range_max_c <- 32767}
    if(template_dataType=="FLT4S"){data_range_max_c <- 3.4e+38}
    if(template_dataType=="FLT8S"){data_range_max_c <- 1.7e+308 }
  } 
  
  #### logscalefactor ####
  if(!missing(logscale_factor)){
    assert_that(class(logscale_factor)=="numeric")
    logscale_factor_c <- logscale_factor
  }else{
    logscale_factor <- 0
  }
  
  #### spectral_uncertainty ####
  if(!missing(spectral_uncertainty)){
    assert_that(class(spectral_uncertainty)=="numeric")
    spectral_uncertainty_c <- spectral_uncertainty
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U" | template_dataType=="INT1S"){
      spectral_uncertainty_c <- 1
    }else{
      spectral_uncertainty_c <- 50
      }
  }
  #### temporal_uncertainty ####
  if(!missing(temporal_uncertainty)){
    assert_that(class(temporal_uncertainty)=="numeric")
    temporal_uncertainty_c <- temporal_uncertainty
  }else{
    template_dataType <- dataType(template[[1]])
    if(template_dataType=="INT1U" | template_dataType=="INT1S"){
      temporal_uncertainty_c <- 1
    }else{
      temporal_uncertainty_c <- 50
    }
  }
  
  #### hightag and lowtag####
  if(!missing(hightag)){
    assert_that(class(hightag)=="character")
    hightag_c <- hightag
  }else{
    hightag_c <- "high"
  }
  
  if(!missing(lowtag)){
    assert_that(class(lowtag)=="character")
    lowtag_c <- lowtag
  }else{
    lowtag_c <- "low"
  }
  
  #### maskimg options #### 
  if(!missing(MASKIMG_options)){
    assert_that(class(MASKIMG_options)=="character")
    MASKIMG_options_c <- MASKIMG_options
  }else{
    MASKIMG_options_c <- ""
  }
  
  #### maskrange options####
  if(!missing(MASKRANGE_options)){
    assert_that(class(MASKRANGE_options)=="character")
    MASKRANGE_options_c <- MASKRANGE_options
  }else{
    MASKRANGE_options_c <- ""
  }
  
  
  #### date1 and date3 ####
  #Get the High and Low Dates and Pair Dates for finding the first and last pair
  high_dates <- input_dates[input_resolutions==hightag_c]
  low_dates <- input_dates[input_resolutions==lowtag_c]
  pair_dates <- which(table(c(unique(high_dates),unique(low_dates)))>=2)
  
  if(!missing(date1)){
    assert_that(class(date1)=="numeric")
    date1_c <- date1
  }else{
    date1_c <- pair_dates[1]
  }
  
  if(!missing(date3)){
    assert_that(class(date3)=="numeric")
    date3_c <- date3
  }else{
    date3_c <- pair_dates[length(pair_dates)]
  }
  
  #### n_cores ####
  if(!missing(n_cores)){
    assert_that(class(n_cores)=="numeric",
                n_cores<=parallel::detectCores())
    n_cores_c <- n_cores
  }else{
    n_cores_c <- 1
  }
  
  ##### B: Check all the required Inputs #####
  #These are variables which are always provided by the user
  #Here, we simply make sure they are of the correct types and matching length
  
  #Some basic Assertions about the types of the inputs
  assert_that(
    class(input_filenames)=="character",
    class(input_dates)=="numeric",
    class(input_resolutions)=="character",
    class(pred_dates)=="numeric",
    class(pred_filenames)=="character"
  )
  
  #Some basic Assertions about the length of the inputs
  assert_that(
    length(input_filenames)>=5,
    length(input_resolutions)==length(input_filenames),
    length(input_dates)==length(input_filenames),
    length(unique(input_resolutions))==2,   
    length(pred_dates)==length(pred_dates)
  )
  
  #Get the High and Low Dates and Pair Dates just for checking
  high_dates <- input_dates[input_resolutions==hightag_c]
  low_dates <- input_dates[input_resolutions==lowtag_c]
  pair_dates <- which(table(c(unique(high_dates),unique(low_dates)))>=2)
  
  
  
  #Some further Assertions
  assert_that(
    length(pair_dates)>=2,             #At least two pairs?
    any(!pred_dates>max(pair_dates)),  #Pred Dates within the Interval?
    any(!pred_dates<min(pair_dates)),   #Pred Dates within the Interval?
    all(input_resolutions %in% c(hightag_c, lowtag_c)) #Resolutions given consistent with hightag and lowtag
  )
  
  
  input_filenames_c <- input_filenames
  input_resolutions_c <- input_resolutions
  input_dates_c <- input_dates
  pred_dates_c <- pred_dates
  pred_filenames_c <- pred_filenames
  
  
  ##### C: Call the CPP function #####
  #And print the used parameters 
  print("Input Filenames: ")
  print(input_filenames_c)
  print("Input Resolutions: ")
  print(input_resolutions_c)
  print("Input Dates: ")
  print(input_dates_c)
  print("Prediction Filenames: ")
  print(pred_filenames_c)
  print("Prediction Dates: ")
  print(pred_dates_c)
  print("Predicting between Pairs on Dates:")
  print(paste(date1_c,date3_c))
  print("Prediction Area: ")
  print(pred_area_c)
  print("Number Classes: ")
  print(number_classes_c)
  print("Data_Range: ")
  print(paste(data_range_min_c, data_range_max_c))
  if (!grepl("^\\s*$", MASKIMG_options_c)){
    print("MASKIMG Options: ")
    print(MASKIMG_options_c)
  }
  if (!grepl("^\\s*$", MASKRANGE_options_c)){
    print("MASKRANGE Options: ")
    print(MASKRANGE_options_c)
  }
  if(use_parallelisation_c){
    print(paste("USING PARALLELISATION WITH ", n_cores_c," CORES"))
  }
  
  
  #___________________________________________________________________________#
  #Call the cpp fusion function with the checked inputs
  ImageFusion::execute_starfm_job_cpp(input_filenames = input_filenames_c,
                                      input_resolutions = input_resolutions_c,
                                      input_dates = input_dates_c,
                                      pred_dates = pred_dates_c,
                                      pred_filenames = pred_filenames_c,
                                      pred_area = pred_area_c,
                                      winsize = winsize_c,
                                      date1 = date1_c,
                                      date3 = date3_c,
                                      n_cores = n_cores_c,
                                      output_masks = output_masks_c,
                                      use_nodata_value = use_nodata_value_c,
                                      use_parallelisation = use_parallelisation_c,
                                      use_strict_filtering = use_strict_filtering_c,
                                      use_temp_diff_for_weights = use_temp_diff_for_weights_c,
                                      number_classes = number_classes_c,
                                      data_range_min = data_range_min_c,
                                      data_range_max = data_range_max_c,
                                      logscale_factor = logscale_factor_c,
                                      spectral_uncertainty = spectral_uncertainty_c,
                                      temporal_uncertainty = temporal_uncertainty_c,
                                      hightag = hightag_c,
                                      lowtag = lowtag_c,
                                      MASKIMG_options = MASKIMG_options_c,
                                      MASKRANGE_options = MASKRANGE_options_c
  )
  #___________________________________________________________________________#
  
}
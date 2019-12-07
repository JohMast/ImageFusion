pkgbuild::compile_dll()

roxygen2::roxygenise()

library(magrittr)
setwd("../Testground/")
if(!dir.exists("TestOutputs")){dir.create("TestOutputs")}





filenames_high <- list.files("TestImages/Landsat_MODIS_Fusion/Landsat_2015/",pattern = ".tif",full.names = T)
filenames_low <- list.files("TestImages/Landsat_MODIS_Fusion/MODIS_2015/",pattern = ".tif",full.names = T)
#drop a couple images for testing purposes
filenames_low <- filenames_low[-c(84,123,158)]

dates_high <- filenames_high %>%
  sapply(substr,64,71) %>%
  as.POSIXct(format="%Y%m%d") %>% 
  strftime(format = "%j") %>% 
  as.numeric()%>% 
  "+"(1000)
dates_low <- filenames_low %>% 
  sapply(substr,52,54) %>% 
  as.numeric()%>% 
  "+"(1000)

dates_pred <- c(50,51,100,107,123,125,126,128,155,158,200,201,202,280,281,282,283,355)%>% 
  "+"(1000)

####Test basic functionality###

?ImageFusion::estarfm_job()
?ImageFusion::starfm_job()
?ImageFusion::fitfc_job()
?ImageFusion::spstfm_job
library(raster)
raster::raster("TestImages/test/images/real-set2/L_2016158_NIR.tif")

ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="starfm",
                              singlepair_mode = "ignore",
                              out_dir = file.path("TestOutputs","ignore"),
                              pred_area = c(400,    400, 25,  25),
                              verbose = T,
                              output_overview = T)

ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="estarfm",
                              singlepair_mode = "ignore",
                              out_dir = file.path("TestOutputs","ignore"),
                              pred_area = c(400,    400, 25,  25),
                              verbose = T,
                              output_overview = T)

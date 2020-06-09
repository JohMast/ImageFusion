#pkgbuild::compile_dll()
#7053eb080acfbcad49d3dcad661c7eba4ac9ac93
#roxygen2::roxygenise()
library(rgdal)
library(magrittr)
#setwd("../Testground/")
outdir <- "../TesOut"
if(!dir.exists(outdir)){dir.create(outdir)}





filenames_high <- list.files("../RabbiTS/Data/Landsat_MODIS_Fusion/Landsat_2015/",pattern = ".tif",full.names = T)
filenames_low <- list.files("../RabbiTS/Data/Landsat_MODIS_Fusion/MODIS_2015/",pattern = ".tif",full.names = T)
#drop a couple images for testing purposes
filenames_low <- filenames_low[-c(84,123,158)]

dates_high <- filenames_high %>%
  basename() %>% 
  sapply(substr,18,25) %>%
  as.POSIXct(format="%Y%m%d") %>% 
  strftime(format = "%j") %>% 
  as.numeric()%>% 
  "+"(1000)
dates_low <- filenames_low %>% 
  basename() %>% 
  sapply(substr,8,10) %>% 
  as.numeric()%>% 
  "+"(1000)

dates_pred <- c(50,51,100,107,123,125,126,128,135,155,158,200,201,202,280,281,282,283,355)%>% 
  "+"(1000)

####Test basic functionality###


?ImageFusion::starfm_job
?ImageFusion::spstfm_job

####Test fitfc ####
#Basic fitfc
#uses doublepair mode as default, meaning that even though the outliers could be predicted, they will not
?ImageFusion::fitfc_job()
fitfc_1 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = c(1125,1126),
                              method="fitfc",
                              out_dir = file.path(outdir,"fitfc","basic"),
                              verbose = T,
                              output_overview = T)
#fitfc with some additional arguments, like restriction to pred_area (much faster)
fitfc_2 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="fitfc",
                              out_dir = file.path(outdir,"fitfc","arguments"),
                              pred_area = c(400,    400, 25,  25),
                              n_neighbors=5,
                              verbose = T,
                              output_overview = T)
#fitfc with singlepair ignore
fitfc_3 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="fitfc",
                              out_dir = file.path(outdir,"fitfc","ignore"),
                              pred_area = c(400,    400, 25,  25),
                              verbose = T,
                              output_overview = T)

#fitfc with singlepair mixed
fitfc_4 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="fitfc",
                              singlepair_mode = "mixed",
                              out_dir = file.path(outdir,"fitfc","mixed"),
                              pred_area = c(400,    400, 25,  25),
                              verbose = T,
                              output_overview = T)
#fitfc with singlepair all
fitfc_5 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="fitfc",
                              singlepair_mode = "all",
                              out_dir = file.path(outdir,"fitfc","all"),
                              pred_area = c(400,    400, 25,  25),
                              verbose = T,
                              output_overview = T)

####Test estarfm ####
?ImageFusion::estarfm_job()
#estarfm with some additional arguments
estarfm_1 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = c(1125,1126),
                              method="estarfm",
                              out_dir = file.path(outdir,"estarfm","arguments"),
                              pred_area = c(400,    400, 25,  25),
                              use_local_tol=T,
                              verbose = T,
                              output_overview = T)

#estarfm with masking additional arguments
estarfm_2 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="estarfm",
                              out_dir = file.path(outdir,"estarfm","masking"),
                              pred_area = c(400,    400, 50,  50),
                              MASKRANGE_options="--mask-low-res-invalid-ranges=(125,175)  --mask-high-res-invalid-ranges=[225,275]",
                              verbose = T,
                              output_overview = T,high_date_prediction_mode = "ignore")

#estarfm with copying high additional arguments
estarfm_3 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="estarfm",
                              out_dir = file.path(outdir,"estarfm","copy"),
                              pred_area = c(400,    400, 50,  50),
                              MASKRANGE_options="--mask-low-res-invalid-ranges=(125,175)  --mask-high-res-invalid-ranges=[225,275]",
                              verbose = T,
                              output_overview = T,high_date_prediction_mode = "copy"
                              )
#estarfm with force high additional arguments
estarfm_4 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="estarfm",
                              out_dir = file.path(outdir,"estarfm","force"),
                              pred_area = c(400,    400, 50,  50),
                              MASKRANGE_options="--mask-low-res-invalid-ranges=(125,175)  --mask-high-res-invalid-ranges=[225,275]",
                              verbose = T,
                              output_overview = T,high_date_prediction_mode = "force"
)
#estarfm with masking and large image
estarfm_5 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                                           filenames_low = filenames_low,
                                           dates_high = dates_high,
                                           dates_low = dates_low,
                                           dates_pred = 1126,
                                           method="estarfm",
                                           out_dir = file.path(outdir,"estarfm","large_mask"),
                                           MASKRANGE_options="--mask-low-res-valid-ranges=(0,1000)",
                                           verbose = T,
                                           output_overview = T,high_date_prediction_mode = "ignore")

#estarfm without masking and large image
estarfm_6 <- ImageFusion::imagefusion_task(filenames_high = filenames_high,
                                           filenames_low = filenames_low,
                                           dates_high = dates_high,
                                           dates_low = dates_low,
                                           dates_pred = 1126,
                                           method="estarfm",
                                           out_dir = file.path(outdir,"estarfm","large_nomask"),
                                           verbose = T,
                                           output_overview = T,high_date_prediction_mode = "ignore")

ImageFusion::execute_estarfm_job_cpp()


####test starfm ####
#test basic starfm, ignoring outliers
ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="starfm",
                              singlepair_mode = "ignore",
                              out_dir = file.path(outdir,"starfm","ignore"),
                              verbose = T,
                              output_overview = T)

#test basic starfm, mixed outliers
ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="starfm",
                              singlepair_mode = "mixed",
                              out_dir = file.path(outdir,"starfm","mixed"),
                              verbose = T,
                              output_overview = T)
#test starfm maskranges
ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = dates_pred,
                              method="starfm",
                              singlepair_mode = "mixed",
                              out_dir = file.path(outdir,"starfm","maskrange"),
                              MASKRANGE_options="--mask-low-res-valid-ranges=(0,10000)",
                              pred_area = c(2,    2, 200,  200),
                              verbose = T,
                              output_overview = T)

####test spstfm ####
#test basic spstfm for one date, no saving
ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = 1126,
                              method="spstfm",
                              out_dir = file.path(outdir,"spstfm","nosave"),spstfm_mode = "none",
                              verbose = T,
                              output_overview = T)

#test basic spstfm for one date, no saving
ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = 1126,
                              method="spstfm",
                              out_dir = file.path(outdir,"spstfm","ignore"),spstfm_mode = "iterative",
                              verbose = T,
                              output_overview = T)

#test basic spstfm for one date, no saving
ImageFusion::imagefusion_task(filenames_high = filenames_high,
                              filenames_low = filenames_low,
                              dates_high = dates_high,
                              dates_low = dates_low,
                              dates_pred = c(1125,1126),
                              method="spstfm",
                              out_dir = file.path(outdir,"spstfm","ignore"),spstfm_mode = "",REUSE_options="use",
                              verbose = T,
                              output_overview = T)


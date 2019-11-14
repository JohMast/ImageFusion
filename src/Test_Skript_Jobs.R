pkgbuild::compile_dll()

roxygen2::roxygenise()


###VIEW DOCUMENTATION####
?ImageFusion::estarfm_job()
?ImageFusion::starfm_job()
?ImageFusion::fitfc_job()
?ImageFusion::spstfm_job
library(raster)
raster::raster("src/test/images/real-set2/L_2016158_NIR.tif")



####TEST ESTARFM MINIMAL FOR ONE DATE####
ImageFusion::estarfm_job(input_filenames = c("src/test/images/real-set1/L_2016_220.tif",
                                             "src/test/images/real-set1/L_2016_243.tif",
                                             "src/test/images/real-set1/M_2016_220-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_236-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_243-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames = c("../Estarfm_Test_Multiband.tif"),
                         winsize=5)

####TEST ESTARFM DATARANGE####
ImageFusion::estarfm_job(input_filenames = c("src/test/images/real-set1/L_2016_220.tif",
                                             "src/test/images/real-set1/L_2016_243.tif",
                                             "src/test/images/real-set1/M_2016_220-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_236-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_243-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(5,8,5,7,8),
                         pred_dates = c(7),
                         pred_filenames = c("../Estarfm_Test_DataRanges.tif"),
                         winsize=5,
                         data_range_min = 1000,
                         data_range_max = 5000)

####TEST ESTARFM WITH GEOREFERENCED DATA (1 BAND) ####
ImageFusion::estarfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
            input_resolutions = c("high","high","low","low","low"),
            input_dates = c(1,3,1,2,3),
            pred_dates = c(2),
            pred_filenames =  c("../Estarfm_Test_Singleband.tif"),
            pred_area = c(800,    800, 400,  400),winsize=21)

####TEST ESTARFM WITH GEOREFERENCED DATA (1 BAND) AND NO MASK RANGES ####
ImageFusion::estarfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Estarfm_Test_EmptyMaskRanges.tif"),
                         pred_area = c(800,    800, 400,  400),
                         winsize=21,
                         output_masks = T)

####TEST ESTARFM WITH GEOREFERENCED DATA (1 BAND) AND MASK RANGES ####
ImageFusion::estarfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Estarfm_Test_MaskRanges.tif"),
                         pred_area = c(800,    800, 400,  400),
                         winsize=21,
                         MASKRANGE_options = "--mask-low-res-invalid-ranges=(0,1000)  --mask-high-res-invalid-ranges=[0,10000]",
                         output_masks = T)


####TEST ESTARFM WITH PARALLELISATION ####
ImageFusion::estarfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Estarfm_Test_Singleband.tif"),
                         pred_area = c(800,    800, 400,  400),
                         use_parallelisation = T,n_cores=1)




####TEST STARFM MINIMAL FOR ONE DATE####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set1/L_2016_220.tif",
                                             "src/test/images/real-set1/L_2016_243.tif",
                                             "src/test/images/real-set1/M_2016_220-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_236-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_243-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames = c("../Starfm_Test_Multiband.tif"),
                         winsize=5)


####TEST STARFM DATARANGE####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set1/L_2016_220.tif",
                                             "src/test/images/real-set1/L_2016_243.tif",
                                             "src/test/images/real-set1/M_2016_220-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_236-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_243-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames = c("../Starfm_Test_DataRanges.tif"),
                         winsize=5,
                         data_range_min = 1000,
                         data_range_max = 5000)

####TEST STARFM WITH GEOREFERENCED DATA (1 BAND) ####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Starfm_Test_Singleband.tif"),
                         pred_area = c(800,    800, 400,  400),winsize=21)
####TEST STARFM IN SINGLE PAIR MODE (1 BAND) WITH 1 PAIR INPUT####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","low","low","low"),
                        input_dates = c(3,3,5,8),
                        pred_dates = c(5,8),
                        pred_filenames =  c("../Starfm_Test_Singleband_Singlepair2.tif",
                                            "../Starfm_Test_Singleband_Singlepair3.tif"),
                        pred_area = c(800,    800, 400,  400),winsize=21)

####TEST STARFM IN SINGLE PAIR MODE (1 BAND) WITH 2 PAIR INPUTS BUT FORCED SINGLEPAIR####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/L_2016254_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","high","low","low","low"),
                        input_dates = c(11,13,11,12,13),
                        pred_dates = c(12,13),
                        pred_filenames =  c("../Starfm_Test_Singleband_ForcedSinglepair2.tif",
                                            "../Starfm_Test_Singleband_ForcedSinglepair3.tif"),
                        pred_area = c(800,    800, 400,  400),winsize=21,double_pair_mode = F)

####TEST STARFM WITH GEOREFERENCED DATA (1 BAND) AND NO MASK RANGES ####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Starfm_Test_EmptyMaskRanges.tif"),
                         pred_area = c(800,    800, 400,  400),
                         winsize=21,
                         output_masks = T)

####TEST STARFM WITH GEOREFERENCED DATA (1 BAND) AND MASK RANGES ####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Starfm_Test_MaskRanges.tif"),
                         pred_area = c(800,    800, 400,  400),
                         winsize=21,
                         MASKRANGE_options = "--mask-low-res-invalid-ranges=(0,1000)  --mask-high-res-invalid-ranges=[0,10000]",
                         output_masks = T)


####TEST STARFM WITH PARALLELISATION ####
ImageFusion::starfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Starfm_Test_Singleband.tif"),
                         pred_area = c(800,    800, 400,  400),
                         use_parallelisation = T,n_cores=1)



####TEST FITFC MINIMAL FOR ONE DATE####
ImageFusion::fitfc_job(input_filenames = c("src/test/images/real-set1/L_2016_220.tif",
                                             "src/test/images/real-set1/L_2016_243.tif",
                                             "src/test/images/real-set1/M_2016_220-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_236-bilinear.tif",
                                             "src/test/images/real-set1/M_2016_243-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames = c("../Fitfc_Test_Multiband.tif"),
                         winsize=51)

####TEST FITFC WITH GEOREFERENCED DATA (1 BAND) ####
ImageFusion::fitfc_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/L_2016254_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","high","low","low","low"),
                        input_dates = c(11,16,11,14,16),
                        pred_dates = c(14),
                        pred_filenames =  c("../Fitfc_Test_Singleband.tif"),
                        pred_area = c(800,    800, 400,  400),winsize=51)

####TEST FITFC WITH GEOREFERENCED DATA (1 BAND) AND MASK RANGES ####
ImageFusion::fitfc_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-nearestneighbor.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-nearestneighbor.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-nearestneighbor.tif"),
                         input_resolutions = c("high","low","low","low"),
                         input_dates = c(1,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../fitfc_Test_MaskRanges.tif"),
                         #pred_area = c(800,    800, 400,  400),
                         winsize=21,
                         MASKRANGE_options = "--mask-low-res-invalid-ranges=(0,1000)  --mask-high-res-invalid-ranges=[0,10000]",resolution_factor = 10,
                         output_masks = T)

raster::raster("src/test/images/real-set2/L_2016158_NIR.tif")
####TEST SPSTFM WITH GEOREFERENCED DATA (1 BAND) ####
ImageFusion::spstfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                             "src/test/images/real-set2/L_2016254_NIR.tif",
                                             "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                             "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                         input_resolutions = c("high","high","low","low","low"),
                         input_dates = c(1,3,1,2,3),
                         pred_dates = c(2),
                         pred_filenames =  c("../Spstfm_Test_Singleband.tif"),
                         pred_area = c(800,    800, 100,  100))

####TEST SPSTFM WITH GEOREFERENCED DATA. ATTEMPT PARALLELISATION AND RANDOM SAMPLING. (1 BAND) ####
ImageFusion::spstfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/L_2016254_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","high","low","low","low"),
                        input_dates = c(1,3,1,2,3),
                        pred_dates = c(2),
                        pred_filenames =  c("../Spstfm_Test_Singleband_randomsampling.tif"),
                        pred_area = c(800,    800, 100,  100),use_parallelisation = T,random_sampling = T)

####TEST SPSTFM SAVING DICT (1 BAND) ####
ImageFusion::spstfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/L_2016254_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","high","low","low","low"),
                        input_dates = c(1,3,1,2,3),
                        pred_dates = c(2),
                        pred_filenames =  c("../Spstfm_Test_Singleband_savedict.tif"),
                        pred_area = c(800,    800, 100,  100),SAVEDICT_options = "../Spstfm_dict_test")

####TEST SPSTFM LOADING DICT (1 BAND) ####
ImageFusion::spstfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/L_2016254_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","high","low","low","low"),
                        input_dates = c(1,3,1,2,3),
                        pred_dates = c(2),
                        pred_filenames =  c("../Spstfm_Test_Singleband_improvedict.tif"),
                        pred_area = c(900,    900, 100,  100),LOADDICT_options = "../Spstfm_dict_test",
                        SAVEDICT_options =  "../Spstfm_dict_test_improved",REUSE_options = "improve")

####TEST SPSTFM WITH NO TRAINING(1 BAND) ####
ImageFusion::spstfm_job(input_filenames = c("src/test/images/real-set2/L_2016158_NIR.tif",
                                            "src/test/images/real-set2/L_2016254_NIR.tif",
                                            "src/test/images/real-set2/M_2016158_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016238_NIR-bilinear.tif",
                                            "src/test/images/real-set2/M_2016254_NIR-bilinear.tif"),
                        input_resolutions = c("high","high","low","low","low"),
                        input_dates = c(1,3,1,2,3),
                        pred_dates = c(2),
                        pred_filenames =  c("../Spstfm_Test_Singleband_notraining.tif"),
                        pred_area = c(900,    800, 100,  100),min_train_iter = 0, max_train_iter = 0, LOADDICT_options = "../Spstfm_dict_test",
                        SAVEDICT_options =  "../Spstfm_dict_test_improved",REUSE_options = "improve")


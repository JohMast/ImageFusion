// Generated by using Rcpp::compileAttributes() -> do not edit by hand
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <RcppArmadillo.h>
#include <Rcpp.h>

using namespace Rcpp;

// execute_estarfm_job_cpp
void execute_estarfm_job_cpp(CharacterVector input_filenames, CharacterVector input_resolutions, IntegerVector input_dates, IntegerVector pred_dates, CharacterVector pred_filenames, IntegerVector pred_area, int winsize, int date1, int date3, int n_cores, bool use_local_tol, bool use_quality_weighted_regression, bool output_masks, bool use_nodata_value, bool verbose, double uncertainty_factor, double number_classes, double data_range_min, double data_range_max, const std::string& hightag, const std::string& lowtag, const std::string& MASKIMG_options, const std::string& MASKRANGE_options);
RcppExport SEXP _ImageFusion_execute_estarfm_job_cpp(SEXP input_filenamesSEXP, SEXP input_resolutionsSEXP, SEXP input_datesSEXP, SEXP pred_datesSEXP, SEXP pred_filenamesSEXP, SEXP pred_areaSEXP, SEXP winsizeSEXP, SEXP date1SEXP, SEXP date3SEXP, SEXP n_coresSEXP, SEXP use_local_tolSEXP, SEXP use_quality_weighted_regressionSEXP, SEXP output_masksSEXP, SEXP use_nodata_valueSEXP, SEXP verboseSEXP, SEXP uncertainty_factorSEXP, SEXP number_classesSEXP, SEXP data_range_minSEXP, SEXP data_range_maxSEXP, SEXP hightagSEXP, SEXP lowtagSEXP, SEXP MASKIMG_optionsSEXP, SEXP MASKRANGE_optionsSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< CharacterVector >::type input_filenames(input_filenamesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type input_resolutions(input_resolutionsSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type input_dates(input_datesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_dates(pred_datesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type pred_filenames(pred_filenamesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_area(pred_areaSEXP);
    Rcpp::traits::input_parameter< int >::type winsize(winsizeSEXP);
    Rcpp::traits::input_parameter< int >::type date1(date1SEXP);
    Rcpp::traits::input_parameter< int >::type date3(date3SEXP);
    Rcpp::traits::input_parameter< int >::type n_cores(n_coresSEXP);
    Rcpp::traits::input_parameter< bool >::type use_local_tol(use_local_tolSEXP);
    Rcpp::traits::input_parameter< bool >::type use_quality_weighted_regression(use_quality_weighted_regressionSEXP);
    Rcpp::traits::input_parameter< bool >::type output_masks(output_masksSEXP);
    Rcpp::traits::input_parameter< bool >::type use_nodata_value(use_nodata_valueSEXP);
    Rcpp::traits::input_parameter< bool >::type verbose(verboseSEXP);
    Rcpp::traits::input_parameter< double >::type uncertainty_factor(uncertainty_factorSEXP);
    Rcpp::traits::input_parameter< double >::type number_classes(number_classesSEXP);
    Rcpp::traits::input_parameter< double >::type data_range_min(data_range_minSEXP);
    Rcpp::traits::input_parameter< double >::type data_range_max(data_range_maxSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type hightag(hightagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type lowtag(lowtagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKIMG_options(MASKIMG_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKRANGE_options(MASKRANGE_optionsSEXP);
    execute_estarfm_job_cpp(input_filenames, input_resolutions, input_dates, pred_dates, pred_filenames, pred_area, winsize, date1, date3, n_cores, use_local_tol, use_quality_weighted_regression, output_masks, use_nodata_value, verbose, uncertainty_factor, number_classes, data_range_min, data_range_max, hightag, lowtag, MASKIMG_options, MASKRANGE_options);
    return R_NilValue;
END_RCPP
}
// execute_starfm_job_cpp
void execute_starfm_job_cpp(CharacterVector input_filenames, CharacterVector input_resolutions, IntegerVector input_dates, IntegerVector pred_dates, CharacterVector pred_filenames, IntegerVector pred_area, int winsize, int date1, int date3, int n_cores, bool output_masks, bool use_nodata_value, bool use_strict_filtering, bool use_temp_diff_for_weights, bool do_copy_on_zero_diff, bool double_pair_mode, bool verbose, double number_classes, double logscale_factor, double spectral_uncertainty, double temporal_uncertainty, const std::string& hightag, const std::string& lowtag, const std::string& MASKIMG_options, const std::string& MASKRANGE_options);
RcppExport SEXP _ImageFusion_execute_starfm_job_cpp(SEXP input_filenamesSEXP, SEXP input_resolutionsSEXP, SEXP input_datesSEXP, SEXP pred_datesSEXP, SEXP pred_filenamesSEXP, SEXP pred_areaSEXP, SEXP winsizeSEXP, SEXP date1SEXP, SEXP date3SEXP, SEXP n_coresSEXP, SEXP output_masksSEXP, SEXP use_nodata_valueSEXP, SEXP use_strict_filteringSEXP, SEXP use_temp_diff_for_weightsSEXP, SEXP do_copy_on_zero_diffSEXP, SEXP double_pair_modeSEXP, SEXP verboseSEXP, SEXP number_classesSEXP, SEXP logscale_factorSEXP, SEXP spectral_uncertaintySEXP, SEXP temporal_uncertaintySEXP, SEXP hightagSEXP, SEXP lowtagSEXP, SEXP MASKIMG_optionsSEXP, SEXP MASKRANGE_optionsSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< CharacterVector >::type input_filenames(input_filenamesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type input_resolutions(input_resolutionsSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type input_dates(input_datesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_dates(pred_datesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type pred_filenames(pred_filenamesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_area(pred_areaSEXP);
    Rcpp::traits::input_parameter< int >::type winsize(winsizeSEXP);
    Rcpp::traits::input_parameter< int >::type date1(date1SEXP);
    Rcpp::traits::input_parameter< int >::type date3(date3SEXP);
    Rcpp::traits::input_parameter< int >::type n_cores(n_coresSEXP);
    Rcpp::traits::input_parameter< bool >::type output_masks(output_masksSEXP);
    Rcpp::traits::input_parameter< bool >::type use_nodata_value(use_nodata_valueSEXP);
    Rcpp::traits::input_parameter< bool >::type use_strict_filtering(use_strict_filteringSEXP);
    Rcpp::traits::input_parameter< bool >::type use_temp_diff_for_weights(use_temp_diff_for_weightsSEXP);
    Rcpp::traits::input_parameter< bool >::type do_copy_on_zero_diff(do_copy_on_zero_diffSEXP);
    Rcpp::traits::input_parameter< bool >::type double_pair_mode(double_pair_modeSEXP);
    Rcpp::traits::input_parameter< bool >::type verbose(verboseSEXP);
    Rcpp::traits::input_parameter< double >::type number_classes(number_classesSEXP);
    Rcpp::traits::input_parameter< double >::type logscale_factor(logscale_factorSEXP);
    Rcpp::traits::input_parameter< double >::type spectral_uncertainty(spectral_uncertaintySEXP);
    Rcpp::traits::input_parameter< double >::type temporal_uncertainty(temporal_uncertaintySEXP);
    Rcpp::traits::input_parameter< const std::string& >::type hightag(hightagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type lowtag(lowtagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKIMG_options(MASKIMG_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKRANGE_options(MASKRANGE_optionsSEXP);
    execute_starfm_job_cpp(input_filenames, input_resolutions, input_dates, pred_dates, pred_filenames, pred_area, winsize, date1, date3, n_cores, output_masks, use_nodata_value, use_strict_filtering, use_temp_diff_for_weights, do_copy_on_zero_diff, double_pair_mode, verbose, number_classes, logscale_factor, spectral_uncertainty, temporal_uncertainty, hightag, lowtag, MASKIMG_options, MASKRANGE_options);
    return R_NilValue;
END_RCPP
}
// execute_fitfc_job_cpp
void execute_fitfc_job_cpp(CharacterVector input_filenames, CharacterVector input_resolutions, IntegerVector input_dates, IntegerVector pred_dates, CharacterVector pred_filenames, IntegerVector pred_area, int winsize, int date1, int n_cores, int n_neighbors, bool output_masks, bool use_nodata_value, bool verbose, double resolution_factor, const std::string& hightag, const std::string& lowtag, const std::string& MASKIMG_options, const std::string& MASKRANGE_options);
RcppExport SEXP _ImageFusion_execute_fitfc_job_cpp(SEXP input_filenamesSEXP, SEXP input_resolutionsSEXP, SEXP input_datesSEXP, SEXP pred_datesSEXP, SEXP pred_filenamesSEXP, SEXP pred_areaSEXP, SEXP winsizeSEXP, SEXP date1SEXP, SEXP n_coresSEXP, SEXP n_neighborsSEXP, SEXP output_masksSEXP, SEXP use_nodata_valueSEXP, SEXP verboseSEXP, SEXP resolution_factorSEXP, SEXP hightagSEXP, SEXP lowtagSEXP, SEXP MASKIMG_optionsSEXP, SEXP MASKRANGE_optionsSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< CharacterVector >::type input_filenames(input_filenamesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type input_resolutions(input_resolutionsSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type input_dates(input_datesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_dates(pred_datesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type pred_filenames(pred_filenamesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_area(pred_areaSEXP);
    Rcpp::traits::input_parameter< int >::type winsize(winsizeSEXP);
    Rcpp::traits::input_parameter< int >::type date1(date1SEXP);
    Rcpp::traits::input_parameter< int >::type n_cores(n_coresSEXP);
    Rcpp::traits::input_parameter< int >::type n_neighbors(n_neighborsSEXP);
    Rcpp::traits::input_parameter< bool >::type output_masks(output_masksSEXP);
    Rcpp::traits::input_parameter< bool >::type use_nodata_value(use_nodata_valueSEXP);
    Rcpp::traits::input_parameter< bool >::type verbose(verboseSEXP);
    Rcpp::traits::input_parameter< double >::type resolution_factor(resolution_factorSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type hightag(hightagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type lowtag(lowtagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKIMG_options(MASKIMG_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKRANGE_options(MASKRANGE_optionsSEXP);
    execute_fitfc_job_cpp(input_filenames, input_resolutions, input_dates, pred_dates, pred_filenames, pred_area, winsize, date1, n_cores, n_neighbors, output_masks, use_nodata_value, verbose, resolution_factor, hightag, lowtag, MASKIMG_options, MASKRANGE_options);
    return R_NilValue;
END_RCPP
}
// execute_spstfm_job_cpp
void execute_spstfm_job_cpp(CharacterVector input_filenames, CharacterVector input_resolutions, IntegerVector input_dates, IntegerVector pred_dates, CharacterVector pred_filenames, IntegerVector pred_area, int date1, int date3, int n_cores, int dict_size, int n_training_samples, int patch_size, int patch_overlap, int min_train_iter, int max_train_iter, bool output_masks, bool use_nodata_value, bool random_sampling, bool verbose, const std::string& hightag, const std::string& lowtag, const std::string& MASKIMG_options, const std::string& MASKRANGE_options, const std::string& LOADDICT_options, const std::string& SAVEDICT_options, const std::string& REUSE_options);
RcppExport SEXP _ImageFusion_execute_spstfm_job_cpp(SEXP input_filenamesSEXP, SEXP input_resolutionsSEXP, SEXP input_datesSEXP, SEXP pred_datesSEXP, SEXP pred_filenamesSEXP, SEXP pred_areaSEXP, SEXP date1SEXP, SEXP date3SEXP, SEXP n_coresSEXP, SEXP dict_sizeSEXP, SEXP n_training_samplesSEXP, SEXP patch_sizeSEXP, SEXP patch_overlapSEXP, SEXP min_train_iterSEXP, SEXP max_train_iterSEXP, SEXP output_masksSEXP, SEXP use_nodata_valueSEXP, SEXP random_samplingSEXP, SEXP verboseSEXP, SEXP hightagSEXP, SEXP lowtagSEXP, SEXP MASKIMG_optionsSEXP, SEXP MASKRANGE_optionsSEXP, SEXP LOADDICT_optionsSEXP, SEXP SAVEDICT_optionsSEXP, SEXP REUSE_optionsSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< CharacterVector >::type input_filenames(input_filenamesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type input_resolutions(input_resolutionsSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type input_dates(input_datesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_dates(pred_datesSEXP);
    Rcpp::traits::input_parameter< CharacterVector >::type pred_filenames(pred_filenamesSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type pred_area(pred_areaSEXP);
    Rcpp::traits::input_parameter< int >::type date1(date1SEXP);
    Rcpp::traits::input_parameter< int >::type date3(date3SEXP);
    Rcpp::traits::input_parameter< int >::type n_cores(n_coresSEXP);
    Rcpp::traits::input_parameter< int >::type dict_size(dict_sizeSEXP);
    Rcpp::traits::input_parameter< int >::type n_training_samples(n_training_samplesSEXP);
    Rcpp::traits::input_parameter< int >::type patch_size(patch_sizeSEXP);
    Rcpp::traits::input_parameter< int >::type patch_overlap(patch_overlapSEXP);
    Rcpp::traits::input_parameter< int >::type min_train_iter(min_train_iterSEXP);
    Rcpp::traits::input_parameter< int >::type max_train_iter(max_train_iterSEXP);
    Rcpp::traits::input_parameter< bool >::type output_masks(output_masksSEXP);
    Rcpp::traits::input_parameter< bool >::type use_nodata_value(use_nodata_valueSEXP);
    Rcpp::traits::input_parameter< bool >::type random_sampling(random_samplingSEXP);
    Rcpp::traits::input_parameter< bool >::type verbose(verboseSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type hightag(hightagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type lowtag(lowtagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKIMG_options(MASKIMG_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKRANGE_options(MASKRANGE_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type LOADDICT_options(LOADDICT_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type SAVEDICT_options(SAVEDICT_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type REUSE_options(REUSE_optionsSEXP);
    execute_spstfm_job_cpp(input_filenames, input_resolutions, input_dates, pred_dates, pred_filenames, pred_area, date1, date3, n_cores, dict_size, n_training_samples, patch_size, patch_overlap, min_train_iter, max_train_iter, output_masks, use_nodata_value, random_sampling, verbose, hightag, lowtag, MASKIMG_options, MASKRANGE_options, LOADDICT_options, SAVEDICT_options, REUSE_options);
    return R_NilValue;
END_RCPP
}
// execute_imginterp_job_cpp
void execute_imginterp_job_cpp(const std::string& input_string);
RcppExport SEXP _ImageFusion_execute_imginterp_job_cpp(SEXP input_stringSEXP) {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< const std::string& >::type input_string(input_stringSEXP);
    execute_imginterp_job_cpp(input_string);
    return R_NilValue;
END_RCPP
}

static const R_CallMethodDef CallEntries[] = {
    {"_ImageFusion_execute_estarfm_job_cpp", (DL_FUNC) &_ImageFusion_execute_estarfm_job_cpp, 23},
    {"_ImageFusion_execute_starfm_job_cpp", (DL_FUNC) &_ImageFusion_execute_starfm_job_cpp, 25},
    {"_ImageFusion_execute_fitfc_job_cpp", (DL_FUNC) &_ImageFusion_execute_fitfc_job_cpp, 18},
    {"_ImageFusion_execute_spstfm_job_cpp", (DL_FUNC) &_ImageFusion_execute_spstfm_job_cpp, 26},
    {"_ImageFusion_execute_imginterp_job_cpp", (DL_FUNC) &_ImageFusion_execute_imginterp_job_cpp, 1},
    {NULL, NULL, 0}
};

RcppExport void R_init_ImageFusion(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}

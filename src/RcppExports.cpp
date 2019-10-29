// Generated by using Rcpp::compileAttributes() -> do not edit by hand
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <RcppArmadillo.h>
#include <Rcpp.h>

using namespace Rcpp;

// execute_estarfm_job_cpp
void execute_estarfm_job_cpp(CharacterVector input_filenames, CharacterVector input_resolutions, IntegerVector input_dates, IntegerVector pred_dates, CharacterVector pred_filenames, IntegerVector pred_area, int winsize, int date1, int date3, bool use_local_tol, bool use_quality_weighted_regression, bool output_masks, bool use_nodata_value, double uncertainty_factor, double number_classes, double data_range_min, double data_range_max, const std::string& hightag, const std::string& lowtag, const std::string& MASKIMG_options, const std::string& MASKRANGE_options);
RcppExport SEXP _ImageFusion_execute_estarfm_job_cpp(SEXP input_filenamesSEXP, SEXP input_resolutionsSEXP, SEXP input_datesSEXP, SEXP pred_datesSEXP, SEXP pred_filenamesSEXP, SEXP pred_areaSEXP, SEXP winsizeSEXP, SEXP date1SEXP, SEXP date3SEXP, SEXP use_local_tolSEXP, SEXP use_quality_weighted_regressionSEXP, SEXP output_masksSEXP, SEXP use_nodata_valueSEXP, SEXP uncertainty_factorSEXP, SEXP number_classesSEXP, SEXP data_range_minSEXP, SEXP data_range_maxSEXP, SEXP hightagSEXP, SEXP lowtagSEXP, SEXP MASKIMG_optionsSEXP, SEXP MASKRANGE_optionsSEXP) {
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
    Rcpp::traits::input_parameter< bool >::type use_local_tol(use_local_tolSEXP);
    Rcpp::traits::input_parameter< bool >::type use_quality_weighted_regression(use_quality_weighted_regressionSEXP);
    Rcpp::traits::input_parameter< bool >::type output_masks(output_masksSEXP);
    Rcpp::traits::input_parameter< bool >::type use_nodata_value(use_nodata_valueSEXP);
    Rcpp::traits::input_parameter< double >::type uncertainty_factor(uncertainty_factorSEXP);
    Rcpp::traits::input_parameter< double >::type number_classes(number_classesSEXP);
    Rcpp::traits::input_parameter< double >::type data_range_min(data_range_minSEXP);
    Rcpp::traits::input_parameter< double >::type data_range_max(data_range_maxSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type hightag(hightagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type lowtag(lowtagSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKIMG_options(MASKIMG_optionsSEXP);
    Rcpp::traits::input_parameter< const std::string& >::type MASKRANGE_options(MASKRANGE_optionsSEXP);
    execute_estarfm_job_cpp(input_filenames, input_resolutions, input_dates, pred_dates, pred_filenames, pred_area, winsize, date1, date3, use_local_tol, use_quality_weighted_regression, output_masks, use_nodata_value, uncertainty_factor, number_classes, data_range_min, data_range_max, hightag, lowtag, MASKIMG_options, MASKRANGE_options);
    return R_NilValue;
END_RCPP
}
// testoptions
void testoptions();
RcppExport SEXP _ImageFusion_testoptions() {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    testoptions();
    return R_NilValue;
END_RCPP
}
// rcpp_hello_world
List rcpp_hello_world();
RcppExport SEXP _ImageFusion_rcpp_hello_world() {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    rcpp_result_gen = Rcpp::wrap(rcpp_hello_world());
    return rcpp_result_gen;
END_RCPP
}

static const R_CallMethodDef CallEntries[] = {
    {"_ImageFusion_execute_estarfm_job_cpp", (DL_FUNC) &_ImageFusion_execute_estarfm_job_cpp, 21},
    {"_ImageFusion_testoptions", (DL_FUNC) &_ImageFusion_testoptions, 0},
    {"_ImageFusion_rcpp_hello_world", (DL_FUNC) &_ImageFusion_rcpp_hello_world, 0},
    {NULL, NULL, 0}
};

RcppExport void R_init_ImageFusion(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}

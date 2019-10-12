
#ifdef WITH_OMP
#include "ParallelizerOptions.h"
#include "Parallelizer.h"
#endif /* WITH_OMP */
#include "fitfc.h"

#include <chrono>
#include <iostream>
#include <boost/test/unit_test.hpp>

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(FitFC_Suite)


// test RegressionMapper
BOOST_AUTO_TEST_CASE(regr) {
    Image absdiff;
    int nnz;

    FitFCOptions o;
    o.setWinSize(3);

    Image h1, l1, l2, h2_exp;


    // Test: l2 = 2 * l1 without noise, so h2 = 2 * h1 and residual is 0
    l1.cvMat() = (cv::Mat_<double>(3,3) << 1, 2, 3, 4, 5, 6, 7, 8, 9); // arbitrary numbers
    h1.cvMat() = (cv::Mat_<double>(3,3) << 1, 3, 5, 7, 9, 1, 2, 3, 4); // arbitrary numbers
    l2.cvMat()     = 2 * l1.cvMat();
    h2_exp.cvMat() = 2 * h1.cvMat();

    auto frm_and_r = CallBaseTypeFunctor::run(fitfc_impl_detail::RegressionMapper{o, h1, l1, l2, Image{}},
                                              h1.type());
    Image& h2_pred = frm_and_r.first;
    Image& res = frm_and_r.second;

    absdiff = h2_exp.absdiff(h2_pred);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);

    nnz = cv::countNonZero(res.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);


    // Test: l2 = 3 * l1 + 2 without noise, so h2 = 3 * h1 + 2 and residual is 0
    l1.cvMat() = (cv::Mat_<double>(3,3) << 1, 2, 3, 4, 5, 6, 7, 8, 9); // arbitrary numbers
    h1.cvMat() = (cv::Mat_<double>(3,3) << 1, 3, 5, 7, 9, 1, 2, 3, 4); // arbitrary numbers
    l2.cvMat()     = 3 * l1.cvMat() + 2;
    h2_exp.cvMat() = 3 * h1.cvMat() + 2;

    frm_and_r = CallBaseTypeFunctor::run(fitfc_impl_detail::RegressionMapper{o, h1, l1, l2, Image{}},
                                         h1.type());
    absdiff = h2_exp.absdiff(h2_pred);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);

    nnz = cv::countNonZero(res.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);



    // Test: l2 = 3 * l1 + 2 with 2 samples with symmetrical noise of strength 1, so h2 = 3 * h1 + 2 and residual is 1 and -1 for these points
    //       for (2, 0) and (2, 1) there is just the -1 in the window, so ignore these
    l1.cvMat() = (cv::Mat_<double>(3,3) << 1, 1, 3, 4, 5, 6, 7, 8, 9); // arbitrary numbers, first 2 equal!
    h1.cvMat() = (cv::Mat_<double>(3,3) << 1, 3, 5, 7, 9, 1, 2, 3, 4); // arbitrary numbers
    l2.cvMat()     = 3 * l1.cvMat() + 2;
    h2_exp.cvMat() = 3 * h1.cvMat() + 2;
    l2.at<double>(0, 0) += 1;
    l2.at<double>(1, 0) -= 1;

    frm_and_r = CallBaseTypeFunctor::run(fitfc_impl_detail::RegressionMapper{o, h1, l1, l2, Image{}},
                                         h1.type());
//    std::cout << "frm:\n" << h2_pred.cvMat() << std::endl;
//    std::cout << "r:\n" << res.cvMat() << std::endl;

    absdiff = h2_exp.absdiff(h2_pred);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 2); // ignore (2, 0) and (2, 1)

    nnz = cv::countNonZero(res.cvMat());
    BOOST_CHECK_EQUAL(nnz, 2 + 2); // noise +1, -1 and ignore (2, 0) and (2, 1)
    BOOST_CHECK_EQUAL(res.at<double>(0, 0),  1.0);
    BOOST_CHECK_EQUAL(res.at<double>(1, 0), -1.0);
}



// test FilterStep with only using a single pixel of FRM and r
BOOST_AUTO_TEST_CASE(filter_single_pixel) {
    constexpr unsigned int size = 3;
    FitFCOptions o;
    o.setWinSize(size);

    Image r{size, size, Type::float64x1};
    Image dw{size, size, Type::float64x1};

    Image mask{size, size, Type::uint8x1};
    mask.set(255); // all valid

    Image h2_pred{1, 1, Type::float64x1};
    h2_pred.set(0); // just for readability, when outputting the prediction

    Image h1;
    h1.cvMat() = (cv::Mat_<double>(3,3) << 1, 3, 5,
                                           7, 9, 1,
                                           2, 3, 4); // arbitrary numbers
    Image frm;
    frm.cvMat() = (cv::Mat_<double>(3,3) << 1, 2, 3,
                                            4, 5, 6,
                                            7, 8, 9); // arbitrary numbers
    BOOST_REQUIRE(h1.width()  == size && h1.height()  == size);
    BOOST_REQUIRE(frm.width() == size && frm.height() == size);

    // center
    unsigned int x = 1;
    unsigned int y = 1;

    // Test: take just the center pixel of frm
    o.setNumberNeighbors(1);
    dw.set(1);
    r.set(0);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_EQUAL(h2_pred.at<double>(0, 0), frm.at<double>(x, y));


    // Test: take center pixel of frm and add r
    o.setNumberNeighbors(1);
    dw.set(1);
    r.set(4);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_EQUAL(h2_pred.at<double>(0, 0), frm.at<double>(x, y) + r.at<double>(x, y));


    // Test: select all pixels, but only weight the center one non-zero
    o.setNumberNeighbors(9);
    dw.set(0);
    dw.at<double>(x, y) = 1;
    r.set(3);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_EQUAL(h2_pred.at<double>(0, 0), frm.at<double>(x, y) + r.at<double>(x, y));


    // Test: select all pixels, but only weight the center one non-zero
    o.setNumberNeighbors(9);
    dw.set(0);
    dw.at<double>(x, y) = 1;
    r.set(3);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_EQUAL(h2_pred.at<double>(0, 0), frm.at<double>(x, y) + r.at<double>(x, y));


    // Test: select all pixels, but only mask the center one non-zero
    o.setNumberNeighbors(9);
    dw.set(1);
    mask.set(0);
    mask.setBoolAt(x, y, 0, true);
    r.set(3);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_EQUAL(h2_pred.at<double>(0, 0), frm.at<double>(x, y) + r.at<double>(x, y));
    mask.set(255);

//    std::cout << h2_pred.cvMat() << std::endl;
}



// test FilterStep single channel images
BOOST_AUTO_TEST_CASE(filter_single_channel) {
    constexpr unsigned int size = 3;
    FitFCOptions o;
    o.setWinSize(size);

    Image r{size, size, Type::float64x1};
    Image dw{size, size, Type::float64x1};

    Image mask{size, size, Type::uint8x1};
    mask.set(255);

    Image h2_pred{1, 1, Type::float64x1};
    h2_pred.set(0);

    Image h1;
    h1.cvMat() = (cv::Mat_<double>(3,3) <<  1,  2,  3,
                                           11, 12, 13,
                                           21, 22, 23); // rows have similar numbers
    Image frm;
    frm.cvMat() = (cv::Mat_<double>(3,3) <<  1,  2,  3,
                                             4,  5,  9,
                                             7,  2,  9); // arbitrary numbers, but rows have not the most similar numbers
    BOOST_REQUIRE(h1.width()  == size && h1.height()  == size);
    BOOST_REQUIRE(frm.width() == size && frm.height() == size);

    unsigned int x = 1;
    unsigned int y = 1;

    // Test: average 3 values
    o.setNumberNeighbors(3);
    dw.set(1);
    r.set(0);
    double avg = (frm.at<double>(x-1, y) + frm.at<double>(x, y) + frm.at<double>(x+1, y)) / 3;

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0),  avg, 1e-12);


    // Test: average 3 values with added r
    o.setNumberNeighbors(3);
    dw.set(1);
    r.cvMat() = (cv::Mat_<double>(3,3) <<  0,  0,  0,
                                          -1,  3,  1,
                                           0,  0,  0);
    double sum = frm.at<double>(x-1, y) + r.at<double>(x-1, y)
               + frm.at<double>(x, y)   + r.at<double>(x, y)
               + frm.at<double>(x+1, y) + r.at<double>(x+1, y);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0), sum / 3, 1e-12);


    // Test: weighted average 3 values with added r
    o.setNumberNeighbors(3);
    dw.cvMat() = (cv::Mat_<double>(3,3) << 0,   0,   0,
                                           0.5, 1,   0.25,
                                           0,   0,   0);
    r.cvMat() = (cv::Mat_<double>(3,3) <<  0,   0,   0,
                                           2,  15,  11,
                                           0,   0,   0);
    double sum_weights = dw.at<double>(x-1, y) + dw.at<double>(x, y) + dw.at<double>(x+1, y);
    double weighted_sum = dw.at<double>(x-1, y) * (frm.at<double>(x-1, y) + r.at<double>(x-1, y))
                        + dw.at<double>(x, y)   * (frm.at<double>(x, y)   + r.at<double>(x, y))
                        + dw.at<double>(x+1, y) * (frm.at<double>(x+1, y) + r.at<double>(x+1, y));

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0), weighted_sum / sum_weights, 1e-12);

//    std::cout << h2_pred.cvMat() << std::endl;
}



// test FilterStep multi channel images
BOOST_AUTO_TEST_CASE(filter_multi_channel) {
    constexpr unsigned int size = 3;
    FitFCOptions o;
    o.setWinSize(size);

    Image r{size, size, Type::float64x2};
    Image dw{size, size, Type::float64x1};

    Image mask{size, size, Type::uint8x1};
    mask.set(255);

    Image h2_pred{1, 1, Type::float64x2};
    h2_pred.set(0);

    Image h1;
    h1.cvMat() = (cv::Mat_<cv::Vec2d>(3,3) <<
                  cv::Vec2d{ 1, 70}, cv::Vec2d{12, 10}, cv::Vec2d{30, 80},
                  cv::Vec2d{ 5, 34}, cv::Vec2d{ 5, 16}, cv::Vec2d{30, 16},
                  cv::Vec2d{23, 19}, cv::Vec2d{ 4, 22}, cv::Vec2d{16,  5}); // center column has least euclidean distance
    Image frm;
    frm.cvMat() = (cv::Mat_<cv::Vec2d>(3,3) <<
                   cv::Vec2d{ 1, 70}, cv::Vec2d{ 2, 10}, cv::Vec2d{30, 80},
                   cv::Vec2d{ 5, 34}, cv::Vec2d{ 6, 16}, cv::Vec2d{30, 16},
                   cv::Vec2d{23, 19}, cv::Vec2d{ 4, 22}, cv::Vec2d{16,  5}); // only center column used, values arbitrary
    BOOST_REQUIRE(h1.width()  == size && h1.height()  == size);
    BOOST_REQUIRE(frm.width() == size && frm.height() == size);

    unsigned int x = 1;
    unsigned int y = 1;

    // Test: average 3 pixels
    o.setNumberNeighbors(3);
    dw.set(1);
    r.set(0);
    cv::Vec2d avg = (frm.at<cv::Vec2d>(x, y-1) + frm.at<cv::Vec2d>(x, y) + frm.at<cv::Vec2d>(x, y+1)) / 3;

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0, 0),  avg[0], 1e-12);
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0, 1),  avg[1], 1e-12);


    // Test: average 3 values with added r
    o.setNumberNeighbors(3);
    dw.set(1);
    r.cvMat() = (cv::Mat_<cv::Vec2d>(3,3) <<
                       cv::Vec2d{ 1, 70}, cv::Vec2d{ 2, 10}, cv::Vec2d{30, 80},
                       cv::Vec2d{ 5, 34}, cv::Vec2d{-6, 16}, cv::Vec2d{30, 16},
                       cv::Vec2d{23, 19}, cv::Vec2d{ 4, 22}, cv::Vec2d{16,  5}); // only center column used, values arbitrary
    cv::Vec2d sum = frm.at<cv::Vec2d>(x, y-1) + r.at<cv::Vec2d>(x, y-1)
                  + frm.at<cv::Vec2d>(x, y)   + r.at<cv::Vec2d>(x, y)
                  + frm.at<cv::Vec2d>(x, y+1) + r.at<cv::Vec2d>(x, y+1);

    CallBaseTypeFunctor::run(fitfc_impl_detail::FilterStep{
                                 o, x, y, h1, frm, r, mask, dw, h2_pred},
                                 h2_pred.type());
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0, 0), sum[0] / 3, 1e-12);
    BOOST_CHECK_CLOSE_FRACTION(h2_pred.at<double>(0, 0, 1), sum[1] / 3, 1e-12);

//    std::cout << h2_pred.cvMat() << std::endl;
}



// check that using a prediction area gives the same result as cropping a full prediction (if cubic interpolation is not used)
BOOST_AUTO_TEST_CASE(compare_serial_pararallel_cropped) {
    std::string highTag = "high";
    std::string lowTag = "low";
    auto mri = std::make_shared<MultiResImages>();
//    mri->set(highTag, 1, Image("../test_resources/images/real-set1/L_2016_220.tif"));
//    mri->set(lowTag,  1, Image("../test_resources/images/real-set1/M_2016_220-nearestneighbor.tif"));
//    mri->set(lowTag,  2, Image("../test_resources/images/real-set1/M_2016_236-nearestneighbor.tif"));
    mri->set(highTag, 1, Image("../test_resources/images/artificial-set2/h1.tif"));
    mri->set(lowTag,  1, Image("../test_resources/images/artificial-set2/l1.tif"));
    mri->set(lowTag,  2, Image("../test_resources/images/artificial-set2/l2.tif"));
    int width  = mri->get(highTag, 1).width();
    int height = mri->get(highTag, 1).height();

    FitFCOptions o;
    o.setPairDate(1);
//    o.setPredictionArea({0, 0, width, height}); // tests setting no prediction area for FitFCFusor
    o.setWinSize(51);
    o.setNumberNeighbors(10);
    o.setResolutionFactor(1); // turn off cubic interpolation
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    Image absdiff;
    int nnz;

    // predict full
    Image result_serial;
    {
        FitFCFusor fitfc;
        fitfc.srcImages(mri);
        fitfc.processOptions(o);
        fitfc.predict(2);
        result_serial = fitfc.outputImage();
    }

    // predict cropped version
    Rectangle crop{50, 50, width-105, height-110};
    Image result_cropped;
    {
        FitFCFusor fitfc;
        fitfc.srcImages(mri);
        o.setPredictionArea(crop);
        fitfc.processOptions(o);
        fitfc.predict(2);
        result_cropped = fitfc.outputImage();
    }

    // check if serial and cropped version give the same results
    absdiff = result_serial.constSharedCopy(crop).absdiff(result_cropped);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);
//    std::cout << "Absdiff:\n" << absdiff.cvMat() << std::endl;
}


// test some exception cases
BOOST_AUTO_TEST_CASE(exceptions) {
    std::string highTag = "high";
    std::string lowTag = "low";
    auto mri = std::make_shared<MultiResImages>();
    mri->set(highTag, 1, Image{1, 100, Type::uint8x1});
    mri->set(lowTag,  1, Image{1, 100, Type::uint8x1});
    mri->set(lowTag,  2, Image{1, 100, Type::uint8x1});
    int width  = mri->get(highTag, 1).width();
    int height = mri->get(highTag, 1).height();

    FitFCOptions o;
    o.setPairDate(1);
    o.setPredictionArea({0, 0, width, height});
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    // test that a resolution factor larger than the image size causes an exception
    o.setResolutionFactor(10);
    FitFCFusor fitfc;
    fitfc.srcImages(mri);
    fitfc.processOptions(o);
    BOOST_CHECK_THROW(fitfc.predict(2), size_error);

    // test that a resolution factor of 1 causes no exception
    o.setResolutionFactor(1);
    fitfc.processOptions(o);
    BOOST_CHECK_NO_THROW(fitfc.predict(2));
}


// test that cubic_filter for more than 4 channels does not throw an OpenCV exception
BOOST_AUTO_TEST_CASE(cubic_filter_test_5_channels) {
    Image i{"../test_resources/images/formats/float64x5.tif"};
    BOOST_CHECK_EQUAL(i.type(), Type::float64x5);
    imagefusion::Size s = i.size();
    imagefusion::Type t = i.type();
//    std::cout << i.cvMat() << std::endl;

    Image filtered;
    BOOST_CHECK_NO_THROW(filtered = fitfc_impl_detail::cubic_filter(std::move(i), 2));
//    std::cout << filtered.cvMat() << std::endl;
    BOOST_CHECK_EQUAL(filtered.size(), s);
    BOOST_CHECK_EQUAL(filtered.type(), t);
}


BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

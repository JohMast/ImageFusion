
#ifdef WITH_OMP
#include "ParallelizerOptions.h"
#include "Parallelizer.h"
#endif /* WITH_OMP */
#include "Estarfm.h"

#include <chrono>
#include <iostream>

#include <boost/test/unit_test.hpp>

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(Estarfm_Suite)


// check that serial fusion gives the same as parallel and
// check that using a prediction area gives the same result as cropping a full prediction
BOOST_AUTO_TEST_CASE(compare_serial_pararallel_cropped) {
    std::string highTag = "high";
    std::string lowTag  = "low";
    auto mri = std::make_shared<MultiResImages>();
    mri->set(lowTag,  1, Image("../test_resources/images/artificial-set2/l1.tif"));
    mri->set(highTag, 1, Image("../test_resources/images/artificial-set2/h1.tif"));
    mri->set(lowTag,  2, Image("../test_resources/images/artificial-set2/l2.tif"));
    mri->set(lowTag,  3, Image("../test_resources/images/artificial-set2/l3.tif"));
    mri->set(highTag, 3, Image("../test_resources/images/artificial-set2/h3.tif"));
    int width  = mri->get(highTag, 1).width();
    int height = mri->get(highTag, 1).height();

    EstarfmOptions o;
    o.setDate1(1);
    o.setDate3(3);
    o.setNumberClasses(40);
//    o.setPredictionArea({0, 0, width, height}); // tests setting no prediction area for EstarfmFusor
    o.setWinSize(51);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    Image absdiff;
    int nnz;

    // predict serial
    Image result_serial;
    {
        EstarfmFusor estarfm;
        estarfm.srcImages(mri);
        estarfm.processOptions(o);
        estarfm.predict(2);
        result_serial = estarfm.outputImage();
    }

    #ifdef WITH_OMP
    // predict parallel
    Image result_parallel;
    {
        // set options for Parallelizer
        ParallelizerOptions<EstarfmOptions> p_opt;
        p_opt.setNumberOfThreads(2);
        p_opt.setPredictionArea(o.getPredictionArea());
        p_opt.setAlgOptions(o);

        // execute Starfm in parallel
        Parallelizer<EstarfmFusor> p;
        p.srcImages(mri);
        p.processOptions(p_opt);
        p.predict(2);
        result_parallel = p.outputImage();
    }

    // check if serial and parallel version give the same results
    absdiff = result_serial.absdiff(result_parallel);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);
    #endif /* WITH_OMP */

    // predict cropped version
    Rectangle crop{30, 40, width-50, height-60};
    Image result_cropped;
    {
        EstarfmFusor estarfm;
        estarfm.srcImages(mri);
        o.setPredictionArea(crop);
        estarfm.processOptions(o);
        estarfm.predict(2);
        result_cropped = estarfm.outputImage();
    }

    // check if serial and cropped version give the same results
    absdiff = result_serial.constSharedCopy(crop).absdiff(result_cropped);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);
}


BOOST_AUTO_TEST_CASE(regression) {
    {   // test y = -x should give 1, because negative is not allowed
        std::vector<int> x{ 1,  2,  4,  8};
        std::vector<int> y{-1, -2, -4, -8};
        constexpr double expected = 1;
        BOOST_CHECK_EQUAL(estarfm_impl_detail::regress(x, y), expected);
    }

    {   // test y = 6 x should give 1, because greater 5 is not allowed
        std::vector<int> x{ 1,  2,  4,  8};
        std::vector<int> y{ 6, 12, 24, 48};
        constexpr double expected = 1;
        BOOST_CHECK_EQUAL(estarfm_impl_detail::regress(x, y), expected);
    }

    {   // test y = 3 x
        std::vector<int> x{1, 2,  4,  8};
        std::vector<int> y{3, 6, 12, 24};
        constexpr double expected = 3;
        BOOST_CHECK_LE(std::abs(estarfm_impl_detail::regress(x, y) - expected), 1e-10);
    }

    {   // test y = 3 x + 5
        std::vector<int> x{1, 2,   4,  8};
        std::vector<int> y{8, 11, 17, 29};
        constexpr double expected = 3;
        BOOST_CHECK_LE(std::abs(estarfm_impl_detail::regress(x, y) - expected), 1e-10);
    }

    {   // test y = 5
        std::vector<int> x{1, 2, 4, 8};
        std::vector<int> y{5, 5, 5, 5};
        constexpr double expected = 0;
        BOOST_CHECK_LE(std::abs(estarfm_impl_detail::regress(x, y) - expected), 1e-10);
    }

    {
        std::vector<int> x{1, 1,  1,  1};
        std::vector<int> y{3, 6, 12, 24};
        constexpr double expected = 1;
        BOOST_CHECK_LE(std::abs(estarfm_impl_detail::regress(x, y) - expected), 1e-10);
    }

    {   // test bad quality. Least squares give y = 62.5 + 3.5714 * x, but quality is so bad, that it should return 1.
        std::vector<double> x{  1, 1.5,   2,  2.5,  3, 3.5,   4};
        std::vector<double> y{200, 100, 100, -350, 50, 300, 100};
        constexpr double expected = 1;
        BOOST_CHECK_LE(std::abs(estarfm_impl_detail::regress(x, y) - expected), 1e-10);
    }

    {   // test bad quality. Least squares give y = 62.5 + 3.5714 * x, but quality is so bad, that it should return a bit larger than 1 with smoothing option.
        std::vector<double> x{  1, 1.5,   2,  2.5,  3, 3.5,   4};
        std::vector<double> y{200, 100, 100, -350, 50, 300, 100};
        constexpr double expected = 1;
        BOOST_CHECK_GE(estarfm_impl_detail::regress(x, y, true), expected);
    }

    {   // test non-perfect quality. Least squares give y = 3 x, but with smoothing it should be a bit less than 3.
        std::vector<int> x{1,   2,    4,    8};
        std::vector<int> y{3+1, 6-1, 12+1, 24-1};
        constexpr double expected = 3;
        BOOST_CHECK_LE(estarfm_impl_detail::regress(x, y, true), expected);
    }
}


BOOST_AUTO_TEST_CASE(correlation_test) {
    // test from https://harrisgeospatial.com/docs/CORRELATE.html
    std::vector<int> x{65, 63, 67, 64, 68, 62, 70, 66, 68, 67, 69, 71};
    std::vector<int> y{68, 66, 68, 65, 69, 66, 68, 65, 71, 67, 68, 70};
    BOOST_CHECK_LE(std::abs(estarfm_impl_detail::correlate(x, y) - 0.702652), 1e-5);
}


// check fusion with a five channel image (it should not throw an exception)
BOOST_AUTO_TEST_CASE(fuse_5_chan_img) {
    std::string highTag = "high";
    std::string lowTag  = "low";
    auto mri = std::make_shared<MultiResImages>();
    mri->set(lowTag,  1, Image("../test_resources/images/artificial-set2/l1.tif", {0,0,0,0,0}));
    mri->set(highTag, 1, Image("../test_resources/images/artificial-set2/h1.tif", {0,0,0,0,0}));
    mri->set(lowTag,  2, Image("../test_resources/images/artificial-set2/l2.tif", {0,0,0,0,0}));
    mri->set(lowTag,  3, Image("../test_resources/images/artificial-set2/l3.tif", {0,0,0,0,0}));
    mri->set(highTag, 3, Image("../test_resources/images/artificial-set2/h3.tif", {0,0,0,0,0}));
    int width  = mri->get(highTag, 1).width();
    int height = mri->get(highTag, 1).height();
    BOOST_CHECK_EQUAL(mri->get(highTag, 1).channels(), 5);

    EstarfmOptions o;
    o.setDate1(1);
    o.setDate3(3);
    o.setPredictionArea({0, 0, width, height});
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    // predict serial
    EstarfmFusor estarfm;
    estarfm.srcImages(mri);
    estarfm.processOptions(o);
    BOOST_CHECK_NO_THROW(estarfm.predict(2));
}

BOOST_AUTO_TEST_SUITE_END()

}

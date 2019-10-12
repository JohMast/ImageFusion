
#ifdef WITH_OMP
#include "ParallelizerOptions.h"
#include "Parallelizer.h"
#endif /* WITH_OMP */
#include "Starfm.h"

#include <chrono>
#include <iostream>
#include <boost/test/unit_test.hpp>

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(Starfm_Suite)


// check that serial fusion gives the same as parallel and
// check that using a prediction area gives the same result as cropping a full prediction
BOOST_AUTO_TEST_CASE(compare_serial_pararallel_cropped) {
    std::string highTag = "high";
    std::string lowTag = "low";
    auto mri = std::make_shared<MultiResImages>();
    mri->set(lowTag,  1, Image("../test_resources/images/artificial-set2/l1.tif"));
    mri->set(highTag, 1, Image("../test_resources/images/artificial-set2/h1.tif"));
    mri->set(lowTag,  2, Image("../test_resources/images/artificial-set2/l2.tif"));
    int width  = mri->get(highTag, 1).width();
    int height = mri->get(highTag, 1).height();

    StarfmOptions o;
    o.setSinglePairDate(1);
    o.setTemporalUncertainty(50);
    o.setSpectralUncertainty(50);
    o.setNumberClasses(40);
//    o.setPredictionArea({0, 0, width, height}); // tests setting no prediction area for StarfmFusor
    o.setWinSize(51);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    Image absdiff;
    int nnz;

    // predict serial
    Image result_serial;
    {
        StarfmFusor starfm;
        starfm.srcImages(mri);
        starfm.processOptions(o);
        starfm.predict(2);
        result_serial = starfm.outputImage();
    }

    #ifdef WITH_OMP
    // predict parallel
    Image result_parallel;
    {
        // set options for Parallelizer
        ParallelizerOptions<StarfmOptions> p_opt;
        p_opt.setNumberOfThreads(2);
//        p_opt.setPredictionArea(o.getPredictionArea()); // tests setting no prediction area for Parallelizer
        p_opt.setAlgOptions(o);

        // execute Starfm in parallel
        Parallelizer<StarfmFusor> p;
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
        StarfmFusor starfm;
        starfm.srcImages(mri);
        o.setPredictionArea(crop);
        starfm.processOptions(o);
        starfm.predict(2);
        result_cropped = starfm.outputImage();
    }

    // check if serial and cropped version give the same results
    absdiff = result_serial.constSharedCopy(crop).absdiff(result_cropped);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);
}


// check that l1 == l2 gives h1 and that l1 == h1 gives l2
BOOST_AUTO_TEST_CASE(zero_spectral_or_temporal_diff) {
    Image h1("../test_resources/images/artificial-set2/h1.tif");
    Image l2("../test_resources/images/artificial-set2/l2.tif");

    std::string highTag = "high";
    std::string lowTag = "low";
    auto mri = std::make_shared<MultiResImages>();
    mri->set(highTag, 1, h1);
    mri->set(lowTag,  2, l2);
    int width  = h1.width();
    int height = h1.height();

    StarfmOptions o;
    o.setSinglePairDate(1);
    o.setTemporalUncertainty(50);
    o.setSpectralUncertainty(50);
    o.setNumberClasses(40);
    o.setPredictionArea({0, 0, width, height});
    o.setWinSize(51);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);
    o.setDoCopyOnZeroDiff(true);

    StarfmFusor starfm;
    starfm.srcImages(mri);
    starfm.processOptions(o);

    ConstImage result;
    Image absdiff;
    int nnz;

    // predict with zero spectral difference
    mri->set(lowTag, 1, h1);
    starfm.predict(2);
    result = starfm.outputImage().constSharedCopy();
    absdiff = result.absdiff(l2);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);

    // predict with zero temporal difference
    mri->set(lowTag, 1, l2);
    starfm.predict(2);
    result = starfm.outputImage().constSharedCopy();
    absdiff = result.absdiff(h1);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK_EQUAL(nnz, 0);

    // anti-check
    absdiff = result.absdiff(l2);
    nnz = cv::countNonZero(absdiff.cvMat());
    BOOST_CHECK(nnz != 0);
}

struct artificial_set2 {
    std::vector<Image> h1_vec;
    std::vector<Image> l1_vec;
    std::vector<Image> l2_vec;
    std::vector<Image> h4_vec;
    std::vector<Image> l4_vec;
    Image h1_multi;
    Image l1_multi;
    Image l2_multi;
    Image h4_multi;
    Image l4_multi;

    std::vector<Image> mask_vec;
    Image mask_single;
    Image mask_multi;

    artificial_set2() {
        // get images
        h1_vec.push_back(Image("../test_resources/images/artificial-set2/h1.tif", {}, Rectangle{0, 10, 80, 70}));
        l1_vec.push_back(Image("../test_resources/images/artificial-set2/l1.tif", {}, Rectangle{0, 10, 80, 70}));
        l2_vec.push_back(Image("../test_resources/images/artificial-set2/l2.tif", {}, Rectangle{0, 10, 80, 70}));
        h4_vec.push_back(Image("../test_resources/images/artificial-set2/h4.tif", {}, Rectangle{0, 10, 80, 70}));
        l4_vec.push_back(Image("../test_resources/images/artificial-set2/l4.tif", {}, Rectangle{0, 10, 80, 70}));

        h1_vec.push_back(Image("../test_resources/images/artificial-set2/h1.tif", {}, Rectangle{40, 20, 80, 70}));
        l1_vec.push_back(Image("../test_resources/images/artificial-set2/l1.tif", {}, Rectangle{40, 20, 80, 70}));
        l2_vec.push_back(Image("../test_resources/images/artificial-set2/l2.tif", {}, Rectangle{40, 20, 80, 70}));
        h4_vec.push_back(Image("../test_resources/images/artificial-set2/h4.tif", {}, Rectangle{40, 20, 80, 70}));
        l4_vec.push_back(Image("../test_resources/images/artificial-set2/l4.tif", {}, Rectangle{40, 20, 80, 70}));

        h1_vec.push_back(Image("../test_resources/images/artificial-set2/h1.tif", {}, Rectangle{60, 70, 80, 70}));
        l1_vec.push_back(Image("../test_resources/images/artificial-set2/l1.tif", {}, Rectangle{60, 70, 80, 70}));
        l2_vec.push_back(Image("../test_resources/images/artificial-set2/l2.tif", {}, Rectangle{60, 70, 80, 70}));
        h4_vec.push_back(Image("../test_resources/images/artificial-set2/h4.tif", {}, Rectangle{60, 70, 80, 70}));
        l4_vec.push_back(Image("../test_resources/images/artificial-set2/l4.tif", {}, Rectangle{60, 70, 80, 70}));

        h1_multi.merge(h1_vec);
        l1_multi.merge(l1_vec);
        l2_multi.merge(l2_vec);
        h4_multi.merge(h4_vec);
        l4_multi.merge(l4_vec);

//        h1_multi.write("h1_multi.tif");
//        l1_multi.write("l1_multi.tif");
//        l2_multi.write("l2_multi.tif");
//        h4_multi.write("h4_multi.tif");
//        l4_multi.write("l4_multi.tif");

        // make masks
        mask_vec.push_back(Image("../test_resources/images/artificial-set2/h2.tif",        {}, Rectangle{ 10,  70, 80, 70}).createSingleChannelMaskFromRange({Interval::closed(800, 1200)}).bitwise_not());
        mask_vec.push_back(Image("../test_resources/images/artificial-set1/shapes_h1.png", {}, Rectangle{180, 180, 80, 70}).createSingleChannelMaskFromRange({Interval::closed(  0,  250)}).bitwise_not());
        mask_vec.push_back(Image("../test_resources/images/artificial-set1/shapes_h3.png", {}, Rectangle{  0,  10, 80, 70}).createSingleChannelMaskFromRange({Interval::closed(  0,  250)}).bitwise_not());
        mask_single = mask_vec.front();
        mask_multi.merge(mask_vec);
//        mask_multi.write("mask_multi.tif");
    }
};

// check the result of a multi-channel fusion gives the same as separate single-channel fusions, yet for single pair mode
BOOST_FIXTURE_TEST_CASE(single_pair_multi_channel, artificial_set2) {
    // prepare multi res images objects
    std::string highTag = "high";
    std::string lowTag = "low";

    auto mri_multi = std::make_shared<MultiResImages>();
    mri_multi->set(highTag, 1, h1_multi);
    mri_multi->set(lowTag,  1, l1_multi);
    mri_multi->set(lowTag,  2, l2_multi);

    std::vector<std::shared_ptr<MultiResImages>> mri_vec;
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        auto mri = std::make_shared<MultiResImages>();
        mri->set(highTag, 1, h1_vec.at(i));
        mri->set(lowTag,  1, l1_vec.at(i));
        mri->set(lowTag,  2, l2_vec.at(i));
        mri_vec.push_back(std::move(mri));
    }

    // prepare prediction
    Rectangle predArea{0, 0, 75, 65};
    StarfmOptions o;
    o.setSinglePairDate(1);
    o.setTemporalUncertainty(50);
    o.setSpectralUncertainty(50);
    o.setNumberClasses(40);
    o.setPredictionArea(predArea);
    o.setWinSize(51);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    StarfmFusor starfm_multi;
    starfm_multi.srcImages(mri_multi);
    starfm_multi.processOptions(o);

    // make a multi-channel prediction without mask
    starfm_multi.predict(2);
    ConstImage result_multi = starfm_multi.outputImage().constSharedCopy();
    auto result_multi_split = result_multi.split();

    // make separate single-channel predictions without mask and compare
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        StarfmFusor starfm;
        starfm.srcImages(mri_vec.at(i));
        starfm.processOptions(o);
        starfm.predict(2);

        ConstImage result = starfm.outputImage().constSharedCopy();
        Image absdiff = result.absdiff(result_multi_split.at(i));
        int nnz = cv::countNonZero(absdiff.cvMat());
        BOOST_CHECK_EQUAL(nnz, 0);
    }



    // make a multi-channel prediction with single-channel mask
    starfm_multi.predict(2, mask_single);
    result_multi = starfm_multi.outputImage().constSharedCopy();
    result_multi_split = result_multi.split();

    // make separate single-channel predictions with single-channel mask and compare
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        StarfmFusor starfm;
        starfm.srcImages(mri_vec.at(i));
        starfm.processOptions(o);
        starfm.predict(2, mask_single);

        ConstImage result = starfm.outputImage().constSharedCopy();
        Image absdiff = result.absdiff(result_multi_split.at(i));
        absdiff.set(0, mask_single.constSharedCopy(predArea).bitwise_not());
        int nnz = cv::countNonZero(absdiff.cvMat());
        BOOST_CHECK_EQUAL(nnz, 0);
    }



    // make a multi-channel prediction with multi-channel mask
    starfm_multi.predict(2, mask_multi);
    result_multi = starfm_multi.outputImage().constSharedCopy();
    result_multi_split = result_multi.split();

    // make separate single-channel predictions with separate single-channel mask and compare
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        StarfmFusor starfm;
        starfm.srcImages(mri_vec.at(i));
        starfm.processOptions(o);
        starfm.predict(2, mask_vec.at(i));

        ConstImage result = starfm.outputImage().constSharedCopy();
        Image absdiff = result.absdiff(result_multi_split.at(i));
        absdiff.set(0, mask_vec.at(i).constSharedCopy(predArea).bitwise_not());
        int nnz = cv::countNonZero(absdiff.cvMat());
        BOOST_CHECK_EQUAL(nnz, 0);
    }
}


// check the result of a multi-channel fusion gives the same as separate single-channel fusions, even for double pair mode
BOOST_FIXTURE_TEST_CASE(double_pair_multi_channel, artificial_set2) {
    // prepare multi res images objects
    std::string highTag = "high";
    std::string lowTag = "low";

    auto mri_multi = std::make_shared<MultiResImages>();
    mri_multi->set(highTag, 1, h1_multi);
    mri_multi->set(lowTag,  1, l1_multi);
    mri_multi->set(lowTag,  2, l2_multi);
    mri_multi->set(highTag, 4, h4_multi);
    mri_multi->set(lowTag,  4, l4_multi);

    std::vector<std::shared_ptr<MultiResImages>> mri_vec;
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        auto mri = std::make_shared<MultiResImages>();
        mri->set(highTag, 1, h1_vec.at(i));
        mri->set(lowTag,  1, l1_vec.at(i));
        mri->set(lowTag,  2, l2_vec.at(i));
        mri->set(highTag, 4, h4_vec.at(i));
        mri->set(lowTag,  4, l4_vec.at(i));
        mri_vec.push_back(std::move(mri));
    }

    // prepare prediction
    StarfmOptions o;
    o.setDoublePairDates(1, 4);
    o.setTemporalUncertainty(50);
    o.setSpectralUncertainty(50);
    o.setNumberClasses(40);
    o.setPredictionArea({0, 0, 80, 70});
    o.setWinSize(51);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    StarfmFusor starfm_multi;
    starfm_multi.srcImages(mri_multi);
    starfm_multi.processOptions(o);

    // make a multi-channel prediction without mask
    starfm_multi.predict(2);
    ConstImage result_multi = starfm_multi.outputImage().constSharedCopy();
    auto result_multi_split = result_multi.split();

    // make separate single-channel predictions without mask and compare
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        StarfmFusor starfm;
        starfm.srcImages(mri_vec.at(i));
        starfm.processOptions(o);
        starfm.predict(2);

        ConstImage result = starfm.outputImage().constSharedCopy();
        Image absdiff = result.absdiff(result_multi_split.at(i));
        int nnz = cv::countNonZero(absdiff.cvMat());
        BOOST_CHECK_EQUAL(nnz, 0);
    }



    // make a multi-channel prediction with single-channel mask
    starfm_multi.predict(2, mask_single);
    result_multi = starfm_multi.outputImage().constSharedCopy();
    result_multi_split = result_multi.split();

    // make separate single-channel predictions with single-channel mask and compare
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        StarfmFusor starfm;
        starfm.srcImages(mri_vec.at(i));
        starfm.processOptions(o);
        starfm.predict(2, mask_single);

        ConstImage result = starfm.outputImage().constSharedCopy();
        Image absdiff = result.absdiff(result_multi_split.at(i));
        absdiff.set(0, mask_single.bitwise_not());
        int nnz = cv::countNonZero(absdiff.cvMat());
        BOOST_CHECK_EQUAL(nnz, 0);
    }



    // make a multi-channel prediction with multi-channel mask
    starfm_multi.predict(2, mask_multi);
    result_multi = starfm_multi.outputImage().constSharedCopy();
    result_multi_split = result_multi.split();

    // make separate single-channel predictions with separate single-channel mask and compare
    for (unsigned int i = 0; i < h1_vec.size(); ++i) {
        StarfmFusor starfm;
        starfm.srcImages(mri_vec.at(i));
        starfm.processOptions(o);
        starfm.predict(2, mask_vec.at(i));

        ConstImage result = starfm.outputImage().constSharedCopy();
        Image absdiff = result.absdiff(result_multi_split.at(i));
        absdiff.set(0, mask_vec.at(i).bitwise_not());
        int nnz = cv::countNonZero(absdiff.cvMat());
        BOOST_CHECK_EQUAL(nnz, 0);
    }
}


// check that double pair is superior than single pair
BOOST_AUTO_TEST_CASE(single_pair_vs_double_pair) {
    // prepare multi res images objects
    std::string highTag = "high";
    std::string lowTag = "low";

    auto mri = std::make_shared<MultiResImages>();
    mri->set(highTag, 1, Image("../test_resources/images/artificial-set2/h1.tif"));
    mri->set(lowTag,  1, Image("../test_resources/images/artificial-set2/l1.tif"));
    mri->set(highTag, 2, Image("../test_resources/images/artificial-set2/h2.tif"));
    mri->set(lowTag,  2, Image("../test_resources/images/artificial-set2/l2.tif"));
    mri->set(highTag, 4, Image("../test_resources/images/artificial-set2/h4.tif"));
    mri->set(lowTag,  4, Image("../test_resources/images/artificial-set2/l4.tif"));
    ConstImage const& ref = mri->get(highTag, 2);
    Size s = mri->getAny().size();

    // prepare prediction
    StarfmOptions o;
    o.setTemporalUncertainty(50);
    o.setSpectralUncertainty(50);
    o.setNumberClasses(40);
    o.setPredictionArea({0, 0, s.width, s.height});
    o.setWinSize(51);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    StarfmFusor starfm;
    starfm.srcImages(mri);

    // make a single-pair prediction from date 1
    o.setSinglePairDate(1);
    starfm.processOptions(o);
    starfm.predict(2);
    ConstImage resultFrom1 = starfm.outputImage().constSharedCopy();
    double aadErrorFrom1 = cv::norm(resultFrom1.cvMat(), ref.cvMat(), cv::NORM_L1);


    // make a single-pair prediction from date 4
    o.setSinglePairDate(4);
    starfm.processOptions(o);
    starfm.predict(2);
    ConstImage resultFrom4 = starfm.outputImage().constSharedCopy();
    double aadErrorFrom4 = cv::norm(resultFrom4.cvMat(), ref.cvMat(), cv::NORM_L1);

    // make a double-pair prediction
    o.setDoublePairDates(1, 4);
    starfm.processOptions(o);
    starfm.predict(2);
    ConstImage resultFrom1And4 = starfm.outputImage().constSharedCopy();
    double aadErrorFrom1And4 = cv::norm(resultFrom1And4.cvMat(), ref.cvMat(), cv::NORM_L1);

    BOOST_CHECK_LT(aadErrorFrom1And4, aadErrorFrom1);
    BOOST_CHECK_LT(aadErrorFrom1And4, aadErrorFrom4);
}


BOOST_AUTO_TEST_CASE(fuse_5_chan_img) {
    std::string highTag = "high";
    std::string lowTag = "low";
    auto mri = std::make_shared<MultiResImages>();
    mri->set(lowTag,  1, Image("../test_resources/images/artificial-set2/l1.tif", {0,0,0,0,0}));
    mri->set(highTag, 1, Image("../test_resources/images/artificial-set2/h1.tif", {0,0,0,0,0}));
    mri->set(lowTag,  2, Image("../test_resources/images/artificial-set2/l2.tif", {0,0,0,0,0}));
    mri->set(lowTag,  3, Image("../test_resources/images/artificial-set2/l3.tif", {0,0,0,0,0}));
    mri->set(highTag, 3, Image("../test_resources/images/artificial-set2/h3.tif", {0,0,0,0,0}));
    BOOST_CHECK_EQUAL(mri->get(highTag, 1).channels(), 5);

    StarfmOptions o;
    o.setDoublePairDates(1, 3);
    o.setDoCopyOnZeroDiff(true); // test code path with OpenCV operations == 0 and * 0.5
    o.setTemporalUncertainty(50);
    o.setSpectralUncertainty(50);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);

    StarfmFusor starfm;
    starfm.srcImages(mri);
    starfm.processOptions(o);
    BOOST_CHECK_NO_THROW(starfm.predict(2));
}


BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

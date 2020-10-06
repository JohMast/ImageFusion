#include "interpolation.h"

#include "image.h"
#include "geoinfo.h"
#include "multiresimages.h"
#include "exceptions.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(imginterp_suite)

using namespace imagefusion;



// test interpolation with three single-channel images, center one will be interpolated, no mask
// this tests:
//  * linear interpolation
//  * usage of QL image to confirm that clear pixels will not be interpolated
//  * constant extrapolation in case of no valid values on one side and bahavior with no valid value at all
BOOST_AUTO_TEST_CASE(single_chan_image)
{
    Image i1, i2, i3, ex, intd, ps;
    Image q1, q2, q3;
    MultiResImages imgs, qls, masks;
    std::string tag = "a";
    InterpStats stats;

    // simple test
    i1.cvMat() = (cv::Mat_<uint8_t>(1, 3) <<   0,  10,  50);
    ex.cvMat() = (cv::Mat_<uint8_t>(1, 3) <<  10,  30, 100);
    i3.cvMat() = (cv::Mat_<uint8_t>(1, 3) <<  20,  50, 150);
    i2 = i1;
    imgs.set(tag, 1, Image{i1.sharedCopy()});
    imgs.set(tag, 2, Image{i2.sharedCopy()});
    imgs.set(tag, 3, Image{i3.sharedCopy()});

    q2.cvMat() = (cv::Mat_<uint8_t>(1, 3) << 255, 255, 255);
    qls.set(tag, 2, Image{q2.sharedCopy()});

    auto i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    BOOST_CHECK(intd.type() == ex.type());
    BOOST_CHECK_EQUAL(intd.size(), ex.size());
    BOOST_CHECK_EQUAL(ps.channels(), ex.channels());
    BOOST_CHECK_EQUAL(ps.basetype(), Type::uint8);
    BOOST_CHECK_EQUAL(ps.size(), ex.size());
    for (int x = 0; x < intd.width(); ++x) {
        BOOST_CHECK_EQUAL(ex.at<uint8_t>(x, 0), intd.at<uint8_t>(x, 0));
        BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, 0), static_cast<uint8_t>(PixelState::interpolated));
    }

    BOOST_CHECK_EQUAL(stats.date, 2);
    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 0);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 3);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);


    // do not interpolate at 0
    q2.at<uint8_t>(0, 0) = 0;
    ex.at<uint8_t>(0, 0) = i2.at<uint8_t>(0, 0);

    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    for (int x = 0; x < intd.width(); ++x) {
        BOOST_CHECK_EQUAL(ex.at<uint8_t>(x, 0), intd.at<uint8_t>(x, 0));
        BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, 0), x == 0 ? static_cast<uint8_t>(PixelState::clear)
                                                          : static_cast<uint8_t>(PixelState::interpolated));
    }

    BOOST_CHECK_EQUAL(stats.date, 2);
    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 0);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 2);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);


    // try to interpolate all positions, but at 0 only i1 is clear, at 1 no value is clear, at 2 only i3 is clear.
    q1.cvMat() = (cv::Mat_<uint8_t>(1, 3) <<   0, 255, 255);
    q2.cvMat() = (cv::Mat_<uint8_t>(1, 3) << 255, 255, 255);
    q3.cvMat() = (cv::Mat_<uint8_t>(1, 3) << 255, 255,   0);
    qls.set(tag, 1, Image{q1.sharedCopy()});
    qls.set(tag, 2, Image{q2.sharedCopy()});
    qls.set(tag, 3, Image{q3.sharedCopy()});

    ex.at<uint8_t>(0, 0) = i1.at<uint8_t>(0, 0);
    ex.at<uint8_t>(1, 0) = i2.at<uint8_t>(1, 0); // no valid value, use original value
    ex.at<uint8_t>(2, 0) = i3.at<uint8_t>(2, 0);

    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    for (int x = 0; x < intd.width(); ++x) {
        BOOST_CHECK_EQUAL(ex.at<uint8_t>(x, 0), intd.at<uint8_t>(x, 0));
        BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, 0), x == 1 ? static_cast<uint8_t>(PixelState::noninterpolated)
                                                          : static_cast<uint8_t>(PixelState::interpolated));
    }

    BOOST_CHECK_EQUAL(stats.date, 2);
    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 0);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 3);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 1);
//    std::cout << intd.cvMat() << std::endl;
}



// test interpolation with five single-channel images, single-channel mask
// center and boundary dates will be interpolated / extrapolated. This tests
//  * interpolation formula
//  * correct interpretation of mask (255 -> valid) and cloud values (255 -> invalid/cloud)
//  * finding correct dates
//  * no going out of bounds even for boundary date interpolation
BOOST_AUTO_TEST_CASE(single_chan_image_with_single_channel_mask)
{
    Image i0, i1, i2, i3, i4, ex, intd, ps;
    MultiResImages imgs, qls, masks;
    std::string tag = "a";
    constexpr int8_t Q = -50;  // cloud value
    constexpr int8_t M = -100; // mask value
    InterpStats stats;

    // test basic usage                        0    1    2    3    4    5    6    7    8    9
    i0.cvMat() = (cv::Mat_<int8_t>(1, 10) <<   0,  10,   Q,   M,  99,   Q,  10,   M, 101, 101);
    i1.cvMat() = (cv::Mat_<int8_t>(1, 10) <<   Q,   M,   Q,   M,  17,  10,   M,   0,  80,  80);
    ex.cvMat() = (cv::Mat_<int8_t>(1, 10) <<  15,   5,  11,  13,  17,  20,  30,  50,  55,   M); // this M will be replaced later in expected image
    i3.cvMat() = (cv::Mat_<int8_t>(1, 10) <<   Q,   M,  11,   Q,   Q,   M,  40, 100,  30,  30);
    i4.cvMat() = (cv::Mat_<int8_t>(1, 10) <<  30,   0,   M,  13,   M,  40,   Q,   M, 102, 102);
    i2 = i1;
    i2.at<int8_t>(9, 0) = 33;
    imgs.set(tag, 0, Image{i0.sharedCopy()});
    imgs.set(tag, 1, Image{i1.sharedCopy()});
    imgs.set(tag, 2, Image{i2.sharedCopy()});
    imgs.set(tag, 3, Image{i3.sharedCopy()});
    imgs.set(tag, 4, Image{i4.sharedCopy()});

    qls.set(tag, 0, i0.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));
    qls.set(tag, 1, i1.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));
    qls.set(tag, 2, i2.createSingleChannelMaskFromRange({Interval::closed(-128, 127)}, false));
    qls.set(tag, 3, i3.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));
    qls.set(tag, 4, i4.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));

    IntervalSet maskSet; // [-128, M) u (M, 127]
    maskSet += Interval::right_open(-128, M);
    maskSet += Interval::left_open(M, 127);
    masks.set(tag, 0, i0.createSingleChannelMaskFromSet({maskSet}));
    masks.set(tag, 1, i1.createSingleChannelMaskFromSet({maskSet}));
    masks.set(tag, 2, ex.createSingleChannelMaskFromSet({maskSet})); // set just one invalid location at x = 9
    masks.set(tag, 3, i3.createSingleChannelMaskFromSet({maskSet}));
    masks.set(tag, 4, i4.createSingleChannelMaskFromSet({maskSet}));

    // predict for date 2
    ex.at<int8_t>(9, 0) = i2.at<int8_t>(9, 0);
    auto i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    BOOST_CHECK(intd.type() == ex.type());
    BOOST_CHECK_EQUAL(intd.size(), ex.size());
    BOOST_CHECK_EQUAL(ps.channels(), ex.channels());
    BOOST_CHECK_EQUAL(ps.basetype(), Type::uint8);
    BOOST_CHECK_EQUAL(ps.size(), ex.size());
    for (int x = 0; x < intd.width(); ++x) {
        BOOST_CHECK_EQUAL(ex.at<int8_t>(x, 0), intd.at<int8_t>(x, 0));
        BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, 0), x == 9 ? static_cast<uint8_t>(PixelState::nodata)
                                                          : static_cast<uint8_t>(PixelState::interpolated));
    }

    BOOST_CHECK_EQUAL(stats.date, 2);
    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 1);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 9);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);


    // predict for date 0, original values:    0,  10,   Q,   M,  99,   Q,  10,   M, 101, 101
    ex.cvMat() = (cv::Mat_<int8_t>(1, 10) <<   0,  10,  11,   M,  99,  10,  10,   M, 101, 101);
    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 0, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    for (int x = 0; x < intd.width(); ++x) {
        BOOST_CHECK_EQUAL(ex.at<int8_t>(x, 0), intd.at<int8_t>(x, 0));
        BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, 0), i0.at<int8_t>(x, 0, 0) == Q ? static_cast<uint8_t>(PixelState::interpolated)
                                                                               : i0.at<int8_t>(x, 0, 0) == M ? static_cast<uint8_t>(PixelState::nodata)
                                                                                                             : static_cast<uint8_t>(PixelState::clear));
    }

    BOOST_CHECK_EQUAL(stats.date, 0);
    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 2);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 2);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);


    // predict for date 4, original values:   30,   0,   M,  13,   M,  40,   Q,   M, 102, 102
    ex.cvMat() = (cv::Mat_<int8_t>(1, 10) <<  30,   0,   M,  13,   M,  40,  40,   M, 102, 102);
    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 4, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    for (int x = 0; x < intd.width(); ++x) {
        BOOST_CHECK_EQUAL(ex.at<int8_t>(x, 0), intd.at<int8_t>(x, 0));
        BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, 0), i4.at<int8_t>(x, 0, 0) == Q ? static_cast<uint8_t>(PixelState::interpolated)
                                                 : i4.at<int8_t>(x, 0, 0) == M ? static_cast<uint8_t>(PixelState::nodata)
                                                                               : static_cast<uint8_t>(PixelState::clear));
    }

    BOOST_CHECK_EQUAL(stats.date, 4);
    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 3);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 1);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);
//    std::cout << intd.cvMat() << std::endl;
}



// test interpolation with multi-channel image, mixed single-channel and multi-channel masks
BOOST_AUTO_TEST_CASE(multi_chan_image_with_mixed_channel_masks)
{
    Image i0, i1, i2, i3, i4, ex, intd, ps;
    MultiResImages imgs, qls, masks;
    std::string tag = "a";
    constexpr int16_t Q = -50;  // cloud value
    constexpr int16_t M = -100; // mask value
    InterpStats stats;

    // test basic usage                                    0a   0b |             1a   1b |             2a   2b |             3a   3b |             4a   4b |             5a   5b
    i0.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(   0,  10), cv::Vec2s(   Q,   Q), cv::Vec2s(   M,  88), cv::Vec2s(  33,   M), cv::Vec2s(  35, 100), cv::Vec2s(  35, 110)); // multi-channel mask
    i1.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(   Q,   Q), cv::Vec2s(   M,  -1), cv::Vec2s(   Q,   Q), cv::Vec2s(   Q,   Q), cv::Vec2s(   M,  -1), cv::Vec2s(   M,  -1)); // single-channel mask, -1 means: inherits invalid from other channels
    ex.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(  15,  25), cv::Vec2s(  99,  11), cv::Vec2s(  -2,  88), cv::Vec2s(  33,  40), cv::Vec2s(  25,  90), cv::Vec2s(   M, 115)); // -2 and M will be replaced later in expected image
    i3.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(   Q,   Q), cv::Vec2s(  99,  11), cv::Vec2s(   Q,   Q), cv::Vec2s(   M,  40), cv::Vec2s(  20,   M), cv::Vec2s(  20,   M)); // multi-channel mask
    i4.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(  30,  40), cv::Vec2s(   0,   0), cv::Vec2s(  -1,   M), cv::Vec2s(   Q,   Q), cv::Vec2s(   0,  80), cv::Vec2s(   0, 120)); // single-channel mask, -1 means: inherits invalid from other channels
    i2 = i1;
    i2.at<int16_t>(2, 0, 0) = 111; // replace -2 by 111, will not be modified
    i2.at<int16_t>(5, 0, 0) = 110; // replace M by 110, will not be modified
    imgs.set(tag, 0, Image{i0.sharedCopy()});
    imgs.set(tag, 1, Image{i1.sharedCopy()});
    imgs.set(tag, 2, Image{i2.sharedCopy()});
    imgs.set(tag, 3, Image{i3.sharedCopy()});
    imgs.set(tag, 4, Image{i4.sharedCopy()});

    qls.set(tag, 0, i0.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));
    qls.set(tag, 1, i1.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));
    qls.set(tag, 2, i2.createSingleChannelMaskFromRange({Interval::closed(-128, 127)}, false));
    qls.set(tag, 3, i3.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));
    qls.set(tag, 4, i4.createSingleChannelMaskFromRange({Interval::closed(Q, Q)}, false));

    IntervalSet maskSet; // [-128, M) u (M, 127]
    maskSet += Interval::right_open(-128, M);
    maskSet += Interval::left_open(M, 127);
    masks.set(tag, 0, i0.createMultiChannelMaskFromSet({maskSet}));
    masks.set(tag, 1, i1.createSingleChannelMaskFromSet({maskSet}));
    masks.set(tag, 2, ex.createMultiChannelMaskFromSet({maskSet})); // set just one invalid location at x = 5, channel 0
    masks.set(tag, 3, i3.createMultiChannelMaskFromSet({maskSet}));
    masks.set(tag, 4, i4.createSingleChannelMaskFromSet({maskSet}));

    // predict for date 2
    Image exOrig = ex; // will be later used for repeating this test
    ex.at<int16_t>(2, 0, 0) = i2.at<int16_t>(2, 0, 0);
    ex.at<int16_t>(5, 0, 0) = i2.at<int16_t>(5, 0, 0);
    auto i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    BOOST_CHECK(intd.type() == ex.type());
    BOOST_CHECK_EQUAL(intd.size(), ex.size());
    BOOST_CHECK_EQUAL(ps.channels(), ex.channels());
    BOOST_CHECK_EQUAL(ps.basetype(), Type::uint8);
    BOOST_CHECK_EQUAL(ps.size(), ex.size());
    for (int x = 0; x < intd.width(); ++x) {
        for (unsigned int c = 0; c < intd.channels(); ++c) {
            BOOST_CHECK_EQUAL(ex.at<int16_t>(x, 0, c), intd.at<int16_t>(x, 0, c));
            BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, c), x == 2 && c == 0 ? static_cast<uint8_t>(PixelState::noninterpolated)
                                                     : x == 5 && c == 0 ? static_cast<uint8_t>(PixelState::nodata)
                                                                        : static_cast<uint8_t>(PixelState::interpolated));
        }
    }

    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 1);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 11);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 1);


    // predict for date 0, original values:                 0,  10,               Q,   Q,               M,  88,              33,   M,              35, 100,              35, 110
    ex.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(   0,  10), cv::Vec2s(  99,  11), cv::Vec2s(   M,  88), cv::Vec2s(  33,   M), cv::Vec2s(  35, 100), cv::Vec2s(  35, 110));
    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 0, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    for (int x = 0; x < intd.width(); ++x) {
        for (unsigned int c = 0; c < intd.channels(); ++c) {
            BOOST_CHECK_EQUAL(ex.at<int16_t>(x, 0, c), intd.at<int16_t>(x, 0, c));
            BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, c), i0.at<int16_t>(x, 0, c) == Q ? static_cast<uint8_t>(PixelState::interpolated)
                                                     : i0.at<int16_t>(x, 0, c) == M ? static_cast<uint8_t>(PixelState::nodata)
                                                                                   : static_cast<uint8_t>(PixelState::clear));
        }
    }

    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 2);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 2);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);


    // predict for date 4, original values:                30,  40,               0,   0,              -1,   M,               Q,   Q,               0,  80,               0, 120
    ex.cvMat() = (cv::Mat_<cv::Vec2s>(1, 6) << cv::Vec2s(  30,  40), cv::Vec2s(   0,   0), cv::Vec2s(  -1,   M), cv::Vec2s(  33,  40), cv::Vec2s(   0,  80), cv::Vec2s(   0, 120));
    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 4, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    for (int x = 0; x < intd.width(); ++x) {
        for (unsigned int c = 0; c < intd.channels(); ++c) {
            BOOST_CHECK_EQUAL(ex.at<int16_t>(x, 0, c), intd.at<int16_t>(x, 0, c));
            BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, c), x == 3 ? static_cast<uint8_t>(PixelState::interpolated)
                                                     : x == 2 ? static_cast<uint8_t>(PixelState::nodata)
                                                              : static_cast<uint8_t>(PixelState::clear));
        }
    }

    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 2);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 2);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);
//    std::cout << intd.cvMat() << std::endl;

//    for (int i = 0; i < 5; ++i) {
//        std::cout << "img  " << i << ": " << imgs.get(tag, i).cvMat() << std::endl;
//        std::cout << "ql   " << i << ": " << qls.get(tag, i).cvMat() << std::endl;
//        std::cout << "mask " << i << ": " << masks.get(tag, i).cvMat() << std::endl;
//        std::cout << std::endl;
//        imgs.get(tag, i).write("i" + std::to_string(i) + ".tif");
//        qls.get(tag, i).write("q" + std::to_string(i) + ".tif");
//        masks.get(tag, i).write("m" + std::to_string(i) + ".tif");
//    }

    // now test doPreferCloudsOverNodata, by marking all values invalid for date 2
    masks.set(tag, 2, qls.get(tag, 2).bitwise_not()); // all invalid

    // predict for date 2, test with doPreferCloudsOverNodata set to true ==> all values will be interpolated, even 5a
    ex = exOrig;
    ex.at<int16_t>(2, 0, 0) = i2.at<int16_t>(2, 0, 0);
    ex.at<int16_t>(5, 0, 0) = 25; // interpolation overrides invalid value
    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ true}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    BOOST_CHECK(intd.type() == ex.type());
    BOOST_CHECK_EQUAL(intd.size(), ex.size());
    BOOST_CHECK_EQUAL(ps.channels(), ex.channels());
    BOOST_CHECK_EQUAL(ps.basetype(), Type::uint8);
    BOOST_CHECK_EQUAL(ps.size(), ex.size());
    for (int x = 0; x < intd.width(); ++x) {
        for (unsigned int c = 0; c < intd.channels(); ++c) {
            BOOST_CHECK_EQUAL(ex.at<int16_t>(x, 0, c), intd.at<int16_t>(x, 0, c));
            BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, c), x == 2 && c == 0 ? static_cast<uint8_t>(PixelState::noninterpolated)
                                                                        : static_cast<uint8_t>(PixelState::interpolated));
        }
    }

    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, 0);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 12);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 1);

    // predict for date 2, test with doPreferCloudsOverNodata set to false ==> no value will be interpolated at all
    ex.at<int16_t>(2, 0, 0) = i2.at<int16_t>(2, 0, 0);
    ex.at<int16_t>(5, 0, 0) = i2.at<int16_t>(5, 0, 0);
    i_m_s = CallBaseTypeFunctor::run(Interpolator{imgs, qls, masks, tag, 2, /*doPreferCloudsOverNodata*/ false}, i1.type());
    intd = std::get<0>(i_m_s);
    ps = std::get<1>(i_m_s);
    stats = std::get<2>(i_m_s);
    BOOST_CHECK(intd.type() == ex.type());
    BOOST_CHECK_EQUAL(intd.size(), ex.size());
    BOOST_CHECK_EQUAL(ps.channels(), ex.channels());
    BOOST_CHECK_EQUAL(ps.basetype(), Type::uint8);
    BOOST_CHECK_EQUAL(ps.size(), ex.size());
    for (int x = 0; x < intd.width(); ++x) {
        for (unsigned int c = 0; c < intd.channels(); ++c) {
            BOOST_CHECK_EQUAL(i2.at<int16_t>(x, 0, c), intd.at<int16_t>(x, 0, c)); // intd stays i2, since nothing is interpolated
            BOOST_CHECK_EQUAL(ps.at<uint8_t>(x, 0, c), static_cast<uint8_t>(PixelState::nodata));
        }
    }

    BOOST_CHECK_EQUAL(stats.sz, ex.size());
    BOOST_CHECK_EQUAL(stats.nChans, ex.channels());
    BOOST_CHECK_EQUAL(stats.nNoData, stats.sz.area() * stats.nChans);
    BOOST_CHECK_EQUAL(stats.nInterpBefore, 0);
    BOOST_CHECK_EQUAL(stats.nInterpAfter, 0);
}

BOOST_AUTO_TEST_SUITE_END()

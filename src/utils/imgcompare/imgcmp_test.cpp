#include "imgcmp.h"

#include <boost/test/unit_test.hpp>

#include <functional>

BOOST_AUTO_TEST_SUITE(imgcompare_suite)

using namespace imagefusion;

// test stats with int8, uint8 and float images
BOOST_AUTO_TEST_CASE(stats)
{
    int i;

    Image img_int8{9, 9, Type::int8x1};
    Image mask{    9, 9, Type::uint8x1};
    i = -40;
    for (auto it = img_int8.begin<int8_t>(0), it_end = img_int8.end<int8_t>(0); it != it_end; ++it) {
        *it = i++;
        mask.setBoolAt(it.getX(), it.getY(), 0, i > 0);
    }

    Stats s_int8 = computeStats(img_int8).front();
    BOOST_CHECK_EQUAL(s_int8.min,     -40);
    BOOST_CHECK_EQUAL(s_int8.max,      40);
    BOOST_CHECK_EQUAL(s_int8.mean,      0);
    BOOST_CHECK_EQUAL(s_int8.nonzeros, 80);

    // mask true for img_int8 >= 0
    Stats s_int8_m = computeStats(img_int8, mask).front();
    BOOST_CHECK_EQUAL(s_int8_m.min,     0);
    BOOST_CHECK_EQUAL(s_int8_m.max,    40);
    BOOST_CHECK_EQUAL(s_int8_m.mean,   20);


    Image img_uint16{9, 9, Type::uint16x1};
    i = 40;
    for (auto it = img_uint16.begin<uint16_t>(0), it_end = img_uint16.end<uint16_t>(0); it != it_end; ++it)
        *it = i++;

    Stats s_uint16 = computeStats(img_uint16).front();
    BOOST_CHECK_EQUAL(s_uint16.min,    40);
    BOOST_CHECK_EQUAL(s_uint16.max,    40+80);
    BOOST_CHECK_EQUAL(s_uint16.mean,   40+40);
    BOOST_CHECK_EQUAL(s_uint16.nonzeros, 81);

    // mask true for img_int8 >= 80
    Stats s_uint16_m = computeStats(img_uint16, mask).front();
    BOOST_CHECK_EQUAL(s_uint16_m.min,  80);
    BOOST_CHECK_EQUAL(s_uint16_m.max,  40+80);
    BOOST_CHECK_EQUAL(s_uint16_m.mean, 100);


    Image img_float32{9, 9, Type::float32x1};
    i = 40;
    for (auto it = img_float32.begin<float>(0), it_end = img_float32.end<float>(0); it != it_end; ++it)
        *it = i++ / 13.;

    Stats s_float32 = computeStats(img_float32).front();
    BOOST_CHECK_CLOSE(s_float32.min,    40/13.,        1e-5);
    BOOST_CHECK_CLOSE(s_float32.max,    (40+80) / 13., 1e-5);
    BOOST_CHECK_CLOSE(s_float32.mean,   (40+40) / 13., 1e-5);
    BOOST_CHECK_EQUAL(s_float32.nonzeros, 81);

    // mask true for img_int8 >= 80/13
    Stats s_float32_m = computeStats(img_float32, mask).front();
    BOOST_CHECK_CLOSE(s_float32_m.min,  80/13.,        1e-5);
    BOOST_CHECK_CLOSE(s_float32_m.max,  (40+80) / 13., 1e-5);
    BOOST_CHECK_CLOSE(s_float32_m.mean,  100 / 13.,    1e-5);
}


BOOST_AUTO_TEST_CASE(fileBaseAndExtension)
{
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("bla.test").first,         std::string("bla"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("bla.test").second,        std::string(".test"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("path.d/bla.test").first,  std::string("bla"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("path.d/bla.test").second, std::string(".test"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("bla.blupp.test").first,   std::string("bla.blupp"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("bla.blupp.test").second,  std::string(".test"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("path.d/bla").first,       std::string("bla"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("path.d/bla").second,      std::string(""));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("path.d/.bla").first,      std::string(".bla"));
    BOOST_CHECK_EQUAL(splitToFileBaseAndExtension("path.d/.bla").second,     std::string(""));
}


BOOST_AUTO_TEST_CASE(scatterplots_uint8)
{
    auto x_list = {   1, 2,    4, 5,
                   0, 1, 2, 3, 4, 5, 6,
                   0, 1, 2, 3, 4, 5, 6,
                   0, 1, 2, 3, 4, 5, 6,
                      1, 2, 3, 4, 5,
                         2, 3, 4,
                            3};

    auto y_list = {   6, 6,    6, 6,
                   5, 5, 5, 5, 5, 5, 5,
                   4, 4, 4, 4, 4, 4, 4,
                   3, 3, 3, 3, 3, 3, 3,
                      2, 2, 2, 2, 2,
                         1, 1, 1,
                            0};

    unsigned int size = std::distance(x_list.begin(), x_list.end());
    assert(size == 34);
    Image img1(1, size, Type::uint8x1);
    Image img2(1, size, Type::uint8x1);
    auto it = img1.begin<uint8_t>(0);
    for (int x : x_list)
        *it++ = x;

    it = img2.begin<uint8_t>(0);
    for (int y : y_list)
        *it++ = y;

    Interval range = findRange("auto", "", img1, img2);
    Image plot = plotScatter(img1, img2, {}, range, -100, false);

    auto x_it = x_list.begin();
    auto y_it = y_list.begin();
    for (unsigned int i = 0; i < size; ++i) {
        int x = *x_it++;
        int y = *y_it++;
        if (plot.boolAt(x, plot.height() - 1 - y, 0))
            BOOST_ERROR("Check plot failed at " + to_string(Point(x,y)));
    }

    // test that tiny plot does not crash
    BOOST_CHECK_NO_THROW(plotScatter(img1, img2, {}, range, -100, true, true));


    // visualize oversize plot with circular scatter dots
//    Image plotviz = plotScatter(img1, img2, {}, range, 100, true);
//    cv::namedWindow("scatter", cv::WINDOW_NORMAL);
//    cv::imshow("scatter", plotviz.cvMat());
//    plotviz.write("scatplot.tif");
//    cv::waitKey(0);
}


BOOST_AUTO_TEST_CASE(scatterplots_uint16)
{
    using std::to_string;
    using imagefusion::to_string;

    Image img1{10, 9, Type::uint16x1};
    Image img2{10, 9, Type::uint16x1};
    Image mask{10, 9, Type::uint8x1};

    uint16_t i = 50;
    int min = i;
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            img1.at<uint16_t>(x, y) = i;
            img2.at<uint16_t>(x, y) = i + 11;
            mask.setBoolAt(x, y, 0, i < 90 || i >= 110 );

            // set one pixel in each corner
            if (i == 86) {
                img1.at<uint16_t>(x, y) = 50;
                img2.at<uint16_t>(x, y) = 50;
            }
            if (i == 87) {
                img1.at<uint16_t>(x, y) = 150;
                img2.at<uint16_t>(x, y) = 150;
            }
            if (i == 88) {
                img1.at<uint16_t>(x, y) = 50;
                img2.at<uint16_t>(x, y) = 150;
            }
            if (i == 89) {
                img1.at<uint16_t>(x, y) = 150;
                img2.at<uint16_t>(x, y) = 50;
            }
            ++i;
        }
    }
    int max = img2.at<uint16_t>(img2.width() - 1, img2.height() - 1);

    // natural size is 100 = 150 - 50
    Interval range = findRange("auto", "", img1, img2);
    for (int16_t plotSize : {-1, 50, 77, 100}) {
        Image plot = plotScatter(img1, img2, Image{}, range, plotSize, false);

        plotSize = plot.width();
        if (plotSize == 100) {
            cv::Scalar pointsSet = plotSize * plotSize - countNonZero(plot.cvMat());
            BOOST_CHECK_EQUAL(pointsSet[0], img1.width() * img1.height());
        }

        auto transform = [min, max, plotSize] (int val) {
            return static_cast<uint16_t>(std::round((double)(val - min) / (max - min) * (plotSize - 1)));
        };

        for (int y = 0; y < img1.height(); ++y) {
            for (int x = 0; x < img1.width(); ++x) {
                uint16_t v1 = img1.at<uint16_t>(x, y);
                uint16_t v2 = img2.at<uint16_t>(x, y);
//                std::cout << "(" << v1 << ", " << v2 << ") --> (" << transform(v1) << ", " << transform(v2) << ")" << std::endl;
                if (plot.boolAt(transform(v1), plotSize - 1 - transform(v2), 0))
                    BOOST_ERROR("check plot failed at " + to_string(Point(x,y)) + ", where value is: " + to_string(v1));
            }
        }

//        cv::namedWindow("scatter", cv::WINDOW_NORMAL);
//        cv::imshow("scatter", plot.cvMat());
//        cv::waitKey(0);
    }

    // test plot with mask
    range = findRange("auto", "", img1, img2, mask);
    Image plot = plotScatter(img1, img2, mask, range, -1, false);
    int16_t plotSize = plot.width();
    cv::Scalar pointsSet = plotSize * plotSize - countNonZero(plot.cvMat());
    BOOST_CHECK_EQUAL(pointsSet[0], img1.width() * img1.height() - 20);

    auto transform = [min, max, plotSize] (int val) {
        return static_cast<uint16_t>(std::round((double)(val - min) / (max - min) * (plotSize - 1)));
    };

    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            uint16_t v1 = img1.at<uint16_t>(x, y);
            uint16_t v2 = img2.at<uint16_t>(x, y);
            if (v1 < 90 || v1 >= 110)
                if (plot.boolAt(transform(v1), plotSize - 1 - transform(v2), 0))
                    BOOST_ERROR("check plot with mask failed at " + to_string(Point(x,y)) + ", where value is: " + to_string(v1));
        }
    }

    // with frame
//    range = findRange("auto", "", img1, img2);
//    Image plotviz = plotScatter(img1, img2, Image{}, range, -1, true, true, true, "firstImageFile.tif", "secondImageFile.tif");
//    range = findRange("auto", "", img1, img2, mask);
//    Image plotviz = plotScatter(img1, img2, mask, range, -1, true, true);

//    // test fractional bits to move a line... result: CV_AA is required, which does not look nice for lines with thickness 1
//    Image plotviz{100,100,Type::uint8x3};
//    plotviz.set(255);
//    for (int i = 1; i < 17; ++i) {
//        int frac = i;
//        double d = i / 16.;
//        int shift = static_cast<int>(std::round(d * (1<<frac)));
//        std::cout << "frac: " << frac << ", rel shift: " << d << ", abs shift: " << shift << ", reange: " << (1<<frac) << std::endl;
//        cv::line(plotviz.cvMat(), cv::Point(5*i, 5*(i-1)), cv::Point(5*i, 5*i - 1), cv::Scalar::all(128));
//        cv::line(plotviz.cvMat(), cv::Point((5*i << frac) + shift, 5*i << frac), cv::Point((5*i << frac) + shift, 5*(i+1) << frac), cv::Scalar::all(128), 1, CV_AA, frac);
//    }
//    cv::line(plotviz.cvMat(), cv::Point(5, 5*2+2), cv::Point(5, 5*3), cv::Scalar::all(128), 1, CV_AA);

//    cv::namedWindow("scatter", cv::WINDOW_NORMAL);
//    cv::imshow("scatter", plotviz.cvMat());
//    plotviz.write("scatplot.tif");
//    cv::waitKey(0);
}


BOOST_AUTO_TEST_CASE(scatterplots_int16)
{
    Image img1{100, 100, Type::int16x1};
    Image img2{100, 100, Type::int16x1};
    int i = -30000;
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            img1.at<int16_t>(x, y) = i;
            img2.at<int16_t>(x, y) = -i;
            i += 6;
        }
    }

    // results in axis [-32768, 32767], and a diagonal line from (-32768, 32767) to (32767, -32768)
    constexpr int size = 550;
    Interval range = findRange("auto", "", img1, img2);
    Image plot = plotScatter(img1, img2, {}, range, size, false);
    for (int xy = 0; xy < size; ++xy)
        if (plot.boolAt(xy, xy, 0))
            BOOST_ERROR("check int16 scatter plot failed at " + to_string(Point(xy,xy)));

//    Image plotviz = plotScatter(img1, img2, {}, range, -size, true, true, true, "firstImageFile.tif", "secondImageFile.tif");
//    cv::namedWindow("scatter", cv::WINDOW_NORMAL);
//    cv::imshow("scatter", plotviz.cvMat());
//    plotviz.write("scatplot.tif");
//    cv::waitKey(0);
}

BOOST_AUTO_TEST_CASE(scatterplots_float)
{
    Image img1{100, 100, Type::float32x1};
    Image img2{100, 100, Type::float32x1};
    int i = 0;
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            float val = i / 2. / 9999.;
            img1.at<float>(x, y) = val;
            img2.at<float>(x, y) = val + 0.05;
            ++i;
        }
    }

    // results in axis [0, 0.55], and a diagonal line from (0, 0.05) to (0.5, 0.55)
    constexpr int size = 550;
    Interval range = findRange("auto", "", img1, img2);
    Image plot = plotScatter(img1, img2, {}, range, size, false);
    for (int xy = 0; xy < size - 50; ++xy)
        if (plot.boolAt(xy, size - 1 - xy - 50, 0))
            BOOST_ERROR("check float scatter plot failed at " + to_string(Point(xy, size - 1 - xy - 50)));

//    Image plotviz = plotScatter(img1, img2, {}, range, -size, true, false, true, "firstImageFile.tif", "secondImageFile.tif");
//    cv::namedWindow("scatter", cv::WINDOW_NORMAL);
//    cv::imshow("scatter", plotviz.cvMat());
//    plotviz.write("scatplot.tif");
//    cv::waitKey(0);
}

BOOST_AUTO_TEST_CASE(linearticks)
{
    std::vector<double> ticks;
    std::vector<double> expected;

    ticks = makeLinTicks(0, 10, 10);
    expected = {0, 2, 4, 6, 8, 10};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(0, 10, 11);
    expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(0, 100, 10);
    expected = {0, 20, 40, 60, 80, 100};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(100, 200, 10);
    expected = {100, 120, 140, 160, 180, 200};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(-200, -100, 10);
    expected = {-200, -180, -160, -140, -120, -100};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(-299.23, -100.1, 10);
    expected = {-280, -260, -240, -220, -200, -180, -160, -140, -120};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(-299.23, -100.1, 4);
    expected = {-250, -200, -150};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(10001, 99999, 2);
    expected = {50000};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());

    ticks = makeLinTicks(10001.1, 10002.5, 6);
    expected = {10001.25, 10001.5, 10001.75, 10002, 10002.25, 10002.5};
    BOOST_CHECK_EQUAL_COLLECTIONS(ticks.begin(), ticks.end(), expected.begin(), expected.end());
//    for (double t : ticks)
//        std::cout << t << '\t';
//    std::cout << std::endl;
}


BOOST_AUTO_TEST_CASE(logticks)
{
    std::vector<double> ticks;
    std::vector<double> expected;

    ticks = makeLogTicks(0.1, 10);
    expected = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    BOOST_REQUIRE_EQUAL(ticks.size(), expected.size());
    for (unsigned int i = 0; i < ticks.size(); ++i)
        BOOST_CHECK_CLOSE_FRACTION(ticks[i], expected[i], 1e-14);

    ticks = makeLogTicks(0.5, 5);
    expected = {0.5, 0.6, 0.7, 0.8, 0.9, 1, 2, 3, 4, 5};
    BOOST_REQUIRE_EQUAL(ticks.size(), expected.size());
    for (unsigned int i = 0; i < ticks.size(); ++i)
        BOOST_CHECK_CLOSE_FRACTION(ticks[i], expected[i], 1e-14);
//    for (double t : ticks)
//        std::cout << t << '\t';
//    std::cout << std::endl;
}


// test findRange function with auto and user specified ranges
BOOST_AUTO_TEST_CASE(get_range_interval)
{
    Image i0(2, 1, Type::uint8x1);
    Image i1(2, 1, Type::uint8x1);
    Interval range;

    // test auto range
    i0.at<uint8_t>(0, 0) = 0;
    i0.at<uint8_t>(1, 0) = 255;
    range = findRange("auto", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 0);
    BOOST_CHECK_EQUAL(range.upper(), 255);

    i0.at<uint8_t>(0, 0) = 10;
    i0.at<uint8_t>(1, 0) = 11;
    range = findRange("auto", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 10);
    BOOST_CHECK_EQUAL(range.upper(), 11);

    i1.at<uint8_t>(0, 0) = 11;
    i1.at<uint8_t>(1, 0) = 12;
    range = findRange("auto", "", i0, i1);
    BOOST_CHECK_EQUAL(range.lower(), 10);
    BOOST_CHECK_EQUAL(range.upper(), 12);

    i0 = Image(2, 1, Type::uint16x1);
    i1 = Image(2, 1, Type::uint16x1);
    i0.at<uint16_t>(0, 0) = 11;
    i0.at<uint16_t>(1, 0) = 35012;
    i1.at<uint16_t>(0, 0) = 100;
    i1.at<uint16_t>(1, 0) = 101;
    range = findRange("auto", "", i0, i1);
    BOOST_CHECK_EQUAL(range.lower(), 11);
    BOOST_CHECK_EQUAL(range.upper(), 35012);

    i0.at<uint16_t>(0, 0) = 100;
    i0.at<uint16_t>(1, 0) = 101;
    i1.at<uint16_t>(0, 0) = 11;
    i1.at<uint16_t>(1, 0) = 35012;
    range = findRange("auto", "", i0, i1);
    BOOST_CHECK_EQUAL(range.lower(), 11);
    BOOST_CHECK_EQUAL(range.upper(), 35012);

    // test user range with integer image
    range = findRange("[10.5, 25.25]", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 11);
    BOOST_CHECK_EQUAL(range.upper(), 25);

    range = findRange("(10.5, 25.25)", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 11);
    BOOST_CHECK_EQUAL(range.upper(), 25);

    range = findRange("(10, 26)", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 11);
    BOOST_CHECK_EQUAL(range.upper(), 25);

    range = findRange("[11, 25]", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 11);
    BOOST_CHECK_EQUAL(range.upper(), 25);

    range = findRange("(-0.5, 255.5)", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 0);
    BOOST_CHECK_EQUAL(range.upper(), 255);

    // test user range with float image
    i0 = Image(2, 1, Type::float32x1);
    range = findRange("[10.5,25.25]", "", i0);
    BOOST_CHECK_EQUAL(range.lower(), 10.5);
    BOOST_CHECK_EQUAL(range.upper(), 25.25);

    // test with 5 channels
    i0 = Image(2, 1, Type::uint16x5);
    i0.at<uint16_t>(0, 0, 0) = 11;    i0.at<uint16_t>(1, 0, 0) = 150;
    i0.at<uint16_t>(0, 0, 1) = 10;    i0.at<uint16_t>(1, 0, 1) = 250;
    i0.at<uint16_t>(0, 0, 2) = 12;    i0.at<uint16_t>(1, 0, 2) = 120;
    i0.at<uint16_t>(0, 0, 3) = 13;    i0.at<uint16_t>(1, 0, 3) = 110;
    i0.at<uint16_t>(0, 0, 4) = 20;    i0.at<uint16_t>(1, 0, 4) = 3551;
    Image m(2, 1, Type::uint8x5);
    m.setBoolAt(0, 0, 0, false);    m.setBoolAt(1, 0, 0, false);
    m.setBoolAt(0, 0, 1, false);    m.setBoolAt(1, 0, 1, true);
    m.setBoolAt(0, 0, 2, true);     m.setBoolAt(1, 0, 2, false);
    m.setBoolAt(0, 0, 3, true);     m.setBoolAt(1, 0, 3, true);
    m.setBoolAt(0, 0, 4, true);     m.setBoolAt(1, 0, 4, false);
    range = findRange("auto", "", i0, {}, m);
    BOOST_CHECK_EQUAL(range.lower(), 12);
    BOOST_CHECK_EQUAL(range.upper(), 250);
}


// test auto range histogram with integer values
BOOST_AUTO_TEST_CASE(auto_range_hists_int)
{
    Image img{ 3, 1, Type::uint16x1};
    Image mask{3, 1, Type::uint8x1};

    img.at<uint16_t>(0, 0) = 10;
    img.at<uint16_t>(1, 0) = 20;
    img.at<uint16_t>(2, 0) = 20000;
    mask.setBoolAt(0, 0, 0, true);
    mask.setBoolAt(1, 0, 0, true);
    mask.setBoolAt(2, 0, 0, false);

    unsigned int nbins = 1;
    Interval range = findRange("auto", "", img, {}, mask);
    auto bins_hist = computeHist(img, nbins, range, mask);
    std::vector<double>&       bins = bins_hist.first;
    std::vector<unsigned int>& hist = bins_hist.second;
    BOOST_CHECK_EQUAL(bins.size(), nbins);
    BOOST_CHECK_EQUAL(bins.front(), 15);
    BOOST_CHECK_EQUAL(hist.size(), nbins);
    BOOST_CHECK_EQUAL(hist.front(), 2);
}


// test that histogram works with integer values
BOOST_AUTO_TEST_CASE(hists_int)
{
    Image img1{16, 16, Type::uint8x1};
    Image img2{16, 16, Type::uint8x1};
    Image mask{16, 16, Type::uint8x1};

    uint8_t i = 0;
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            img1.at<uint8_t>(x, y) = static_cast<uint8_t>(-i*(i-255.) * 255. / 16256);
            img2.at<uint8_t>(x, y) = i;
            mask.setBoolAt(x, y, 0, i < 20 || i > 30 );
            ++i;
        }
    }

    // simple: each bin gets 1 value, except where the mask is 0
    unsigned int nbins = 256;
    Interval range = Interval::closed(0, 255);
    auto bins_hist2 = computeHist(img2, nbins, range, mask);
    std::vector<unsigned int>& hist2 = bins_hist2.second;
    BOOST_CHECK_EQUAL(hist2.size(), nbins);
    for (unsigned int b = 0; b < nbins; ++b) {
        if (b < 20 || b > 30)
            BOOST_CHECK_EQUAL(hist2[b], 1);
        else
            BOOST_CHECK_EQUAL(hist2[b], 0);
    }

    // non-integer bin width, compare with OpenCV result
    nbins = 10;
    auto bins_hist1 = CallBaseTypeFunctor::run(HistFunctor{img1, nbins, range, mask}, img1.type());
    std::vector<unsigned int>& hist1 = bins_hist1.second;

    std::vector<unsigned int> myHist(nbins + 1);
    double binwidth = (range.upper() - range.lower()) / nbins;
    for (int y = 0; y < img1.height(); ++y)
        for (int x = 0; x < img1.width(); ++x)
            if (mask.boolAt(x, y, 0))
                myHist.at(img1.at<uint8_t>(x, y) / binwidth)++;
    unsigned int last = myHist.back();
    myHist.pop_back();
    myHist.back() += last;

    BOOST_CHECK_EQUAL_COLLECTIONS(hist1.begin(), hist1.end(), myHist.begin(), myHist.end());
}


// test that the histogram works with floating point values
BOOST_AUTO_TEST_CASE(hists_float)
{
    constexpr int width  = 100;
    constexpr int height = 100;
    Image img{width, height, Type::float32x1};

    // set images to values in [0, 1]
    double i = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            img.at<float>(x, y) = i / (width * height - 1); // starts with 0, ends with 1
            i += 1;
        }
    }

    unsigned int nbins = 1;
    Interval range = Interval::closed(0, 1);
    // check that a histogram uses the full range (also including 1!)
    auto bins_hist2 = computeHist(img, nbins, range);
    std::vector<unsigned int>& hist2 = bins_hist2.second;
    BOOST_CHECK_EQUAL(hist2.size(), nbins);
    BOOST_CHECK_EQUAL(hist2.front(), width * height);

    nbins = 10; // non-integer bin width
    bins_hist2 = computeHist(img, nbins, range);
    BOOST_CHECK_EQUAL(hist2.size(), nbins);
    for (int b = 0; b < 10; ++b)
        BOOST_CHECK_EQUAL(hist2.at(b), width * height / nbins);
}


BOOST_AUTO_TEST_CASE(histplots_uint8)
{
    Image img1{16, 16, Type::uint8x1};
    Image img2{16, 16, Type::uint8x1};
    Image mask{16, 16, Type::uint8x1};

    uint8_t i = 0;
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            img1.at<uint8_t>(x, y) = static_cast<uint8_t>(-i*(i-255.) * 255. / 16256);
            img2.at<uint8_t>(x, y) = i;
            mask.setBoolAt(x, y, 0, i*i < 20 || i*i > 30 );
            ++i;
        }
    }

    unsigned int nbins = 16;
    Interval range = Interval::closed(0, 255);
    auto bins_hist1 = computeHist(img1, nbins, range, mask);
    auto bins_hist2 = computeHist(img2, nbins, range, mask);
    auto bins_empty = computeHist({},   nbins, range, mask);
    std::vector<double>& bins = bins_hist1.first;
    std::vector<unsigned int>& hist1 = bins_hist1.second;
    std::vector<unsigned int>& hist2 = bins_hist2.second;
    std::vector<unsigned int>& empty = bins_empty.second;
    Size plotSize1{256, 62 + 1}; // 62 is the maximum value of hist1
    Size plotSize2{256, 16 + 1}; // 16 is the maximum value of hist2

    Image plot1 = plotHist(hist1, empty, bins, range, img1.basetype(), plotSize1, /*legend*/ true, /*log*/ false, /*grid*/ false, /*gray*/ false, "", "", false);
    Image plot2 = plotHist(empty, hist2, bins, range, img2.basetype(), plotSize2, /*legend*/ true, /*log*/ false, /*grid*/ false, /*gray*/ false, "", "", false);
    int binwidth = plotSize1.width / nbins;
    int offset = binwidth / 2;
    // check that the plots have a black pixel (becuase of the black frame around the bar) at the specific hist values
    for (unsigned int i = 0; i < nbins; ++i) {
        int val = hist1[i];
        BOOST_CHECK(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 0) == 0);
        BOOST_CHECK(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 1) == 0);
        BOOST_CHECK(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 2) == 0);
    }
    for (unsigned int i = 0; i < nbins; ++i) {
        int val = hist2[i];
        BOOST_CHECK(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 0) == 0);
        BOOST_CHECK(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 1) == 0);
        BOOST_CHECK(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 2) == 0);
    }

//    plot1 = plotHist(hist1, hist2, img1.basetype(), plotSize1 + Size{0, 100}, /*legend*/ true, /*log*/ false, /*grid*/ true, /*gray*/ false, "hist1", "hist2");
//    cv::namedWindow("hist1", cv::WINDOW_NORMAL);
//    cv::imshow("hist1", plot1.cvMat()); // note, that blue and red are swapped!
//    plot1.write("hist.tif");
//    cv::waitKey(0);
}


BOOST_AUTO_TEST_CASE(histplots_int16)
{
    Image img1{256, 256, Type::int16x1};
    Image img2{256, 256, Type::int16x1};

    int i = std::numeric_limits<int16_t>::min();
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            img1.at<int16_t>(x, y) = static_cast<int16_t>(-i/256 * i/128);
            img2.at<int16_t>(x, y) = i;
            ++i;
        }
    }

    unsigned int nbins = 64;
    Interval range = Interval::closed(-32768, 32767);
    auto bins_hist1 = computeHist(img1, nbins, range);
    auto bins_hist2 = computeHist(img2, nbins, range);
    auto bins_empty = computeHist({},   nbins, range);
    std::vector<double>& bins = bins_hist1.first;
    std::vector<unsigned int>& hist1 = bins_hist1.second;
    std::vector<unsigned int>& hist2 = bins_hist2.second;
    std::vector<unsigned int>& empty = bins_empty.second;

    Size plotSize1{256, 11264/10 + 1}; // 11264 is the maximum value of hist1
    Size plotSize2{256, 1024 + 1};     // 1024 is the maximum value of hist2

    Image plot1 = plotHist(hist1, empty, bins, range, img1.basetype(), plotSize1, /*legend*/ true, /*log*/ false, /*grid*/ false, /*gray*/ false, "", "", false);
    Image plot2 = plotHist(empty, hist2, bins, range, img2.basetype(), plotSize2, /*legend*/ true, /*log*/ false, /*grid*/ false, /*gray*/ false, "", "", false);
    int binwidth = plotSize1.width / nbins;
    int offset = binwidth / 2;
    // check that the plots have a black pixel (because of the black frame around the bar) at the specific hist values
    double factor = (plotSize1.height - 1) / 11264.;
    for (unsigned int i = 0; i < nbins; ++i) {
        int val = static_cast<int>(std::round(hist1[i] * factor));
        BOOST_CHECK_EQUAL((int)(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 0)), 0);
        BOOST_CHECK_EQUAL((int)(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 1)), 0);
        BOOST_CHECK_EQUAL((int)(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 2)), 0);
    }
    for (unsigned int i = 0; i < nbins; ++i) {
        int val = hist2[i];
        BOOST_CHECK_EQUAL((int)(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 0)), 0);
        BOOST_CHECK_EQUAL((int)(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 1)), 0);
        BOOST_CHECK_EQUAL((int)(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 2)), 0);
    }

//    Image plot = plotHist(hist1, hist2, bins, range, img1.basetype(), Size{1000, 500}, /*legend*/ true, /*log*/ false, /*grid*/ true, /*gray*/ false, "hist1", "hist2");
//    cv::namedWindow("hist", cv::WINDOW_NORMAL);
//    cv::imshow("hist", plot1.cvMat()); // note, that blue and red are swapped!
//    cv::imwrite("histcv.png", plot1.cvMat());
//    plot.write("histboth.tif");
//    cv::waitKey(0);
}


BOOST_AUTO_TEST_CASE(histplots_float)
{
    constexpr int width  = 100;
    constexpr int height = 100;
    Image img1{width, height, Type::float32x1};
    Image img2{width, height, Type::float32x1};

    double i = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double val = i / (width * height - 1);
            img1.at<float>(x, y) = val * val;
            img2.at<float>(x, y) = val;
            i += 1;
        }
    }

    unsigned int nbins = 40;
    Interval range = Interval::closed(0, 1);
    auto bins_hist1 = computeHist(img1, nbins, range);
    auto bins_hist2 = computeHist(img2, nbins, range);
    auto bins_empty = computeHist({},   nbins, range);
    std::vector<double>& bins = bins_hist1.first;
    std::vector<unsigned int>& hist1 = bins_hist1.second;
    std::vector<unsigned int>& hist2 = bins_hist2.second;
    std::vector<unsigned int>& empty = bins_empty.second;

    Size plotSize1{400, 1581 + 1}; // 1581 is the maximum value of hist1
    Size plotSize2{400,  250 + 1}; // 250 is the maximum value of hist2

    Image plot1 = plotHist(hist1, empty, bins, range, img1.basetype(), plotSize1, /*legend*/ true, /*log*/ false, /*grid*/ false, /*gray*/ false, "", "", false);
    Image plot2 = plotHist(empty, hist2, bins, range, img2.basetype(), plotSize2, /*legend*/ true, /*log*/ false, /*grid*/ false, /*gray*/ false, "", "", false);
    int binwidth = plotSize1.width / nbins;
    int offset = binwidth / 2;
    // check that the plots have a black pixel (becuase of the black frame around the bar) at the specific hist values
    for (unsigned int i = 0; i < nbins; ++i) {
        int val = hist1[i];
        BOOST_CHECK(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 0) == 0);
        BOOST_CHECK(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 1) == 0);
        BOOST_CHECK(plot1.at<uint8_t>(offset + binwidth * i, plotSize1.height - 1 - val, 2) == 0);
    }
    for (unsigned int i = 0; i < nbins; ++i) {
        int val = hist2[i];
        BOOST_CHECK(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 0) == 0);
        BOOST_CHECK(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 1) == 0);
        BOOST_CHECK(plot2.at<uint8_t>(offset + binwidth * i, plotSize2.height - 1 - val, 2) == 0);
    }

//    Image plot = plotHist(hist1, hist2, img1.basetype(), Size{1000, 500}, /*legend*/ true, /*log*/ false, /*grid*/ true, /*gray*/ false, "hist1", "hist2");
//    cv::namedWindow("hist", cv::WINDOW_NORMAL);
//    cv::imshow("hist", plot.cvMat()); // note, that blue and red are swapped!
//    plot1.write("hist.tif");
//    cv::waitKey(0);
}


/*               11 1111
 *   0123 4567 8901 2345
 *      +---------+
 * 0 1111 1111 1111 1111
 * 1 1111 1111 1111 1111
 * 2+1111 1101 1111 1111
 * 3|1111 1111 1111 1111
 *  |
 * 4|1111 1111 1110 1111
 * 5|1111 1111 1111 1111
 * 6|1111 1111 1111 1111
 * 7|1110 1111 1111 1111
 *  |
 * 8|1111 1111 1111 1111
 * 9|1111 1111 1111 1111
 *10|1111 1111 1111 1111
 *11|1111 1111 1111 1111
 *  |
 *12|1111 1111 1111 1111
 *13+1111 0111 1111 1111
 *14 1111 1111 1111 1111
 *15 1111 1111 1111 1111
 */
BOOST_AUTO_TEST_CASE(find_border)
{
    Image img{16, 16, Type::uint8x1};
    img.set(255);
    img.setBoolAt(6,  2, 0, false);
    img.setBoolAt(11, 4, 0, false);
    img.setBoolAt(3,  7, 0, false);
    img.setBoolAt(4, 13, 0, false);
    auto r = detectBorderCropBounds(img);
    BOOST_CHECK_EQUAL(r.x, 3);
    BOOST_CHECK_EQUAL(r.y, 2);
    BOOST_CHECK_EQUAL(r.width,  11 - 3 + 1);
    BOOST_CHECK_EQUAL(r.height, 13 - 2 + 1);
}


BOOST_AUTO_TEST_CASE(string_differences)
{
    auto diffs = findStringDiffs("000common1234shared", "commonABCsharedD");
    //                           TTTFFFFFFTTTTFFFFFF    FFFFFFTTTFFFFFFT
    std::vector<bool> diff0{true, true, true,                          // 000
                            false, false, false, false, false, false,  // common
                            true, true, true, true,                    // 1234
                            false, false, false, false, false, false}; // shared
    std::vector<bool> diff1{false, false, false, false, false, false,  // common
                            true, true, true,                          // ABC
                            false, false, false, false, false, false,  // shared
                            true};                                     // D

    BOOST_CHECK_EQUAL_COLLECTIONS(diffs[0].begin(), diffs[0].end(), diff0.begin(), diff0.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(diffs[1].begin(), diffs[1].end(), diff1.begin(), diff1.end());
}

BOOST_AUTO_TEST_CASE(abbreviated_string_candidates)
{
    // candidates for "ab12cd" where the order is influenced by the preserved "12"
    std::string s = "ab12cd";
    std::vector<bool> pres{false, false, true, true, false, false};

    CandidateFactory cf{s, pres};
    std::vector<std::string> candidates;
    while (cf.hasNext())
        candidates.push_back(cf.getNext());

    std::vector<std::string> expected{
        "...b12cd", // 0 preserved, 1 non-preserved
        "a...12cd", // 0 preserved, 1 non-preserved
        "ab12...d", // 0 preserved, 1 non-preserved
        "ab12c...", // 0 preserved, 1 non-preserved
        "...12cd",  // 0 preserved, 2 non-preserved
        "ab12...",  // 0 preserved, 2 non-preserved
        "ab...2cd", // 1 preserved, 0 non-preserved
        "ab1...cd", // 1 preserved, 0 non-preserved
        "a...2cd",  // 1 preserved, 1 non-preserved
        "ab1...d",  // 1 preserved, 1 non-preserved
        "...2cd",   // 1 preserved, 2 non-preserved
        "ab1...",   // 1 preserved, 2 non-preserved
        "ab...cd",  // 2 preserved, 0 non-preserved
        "a...cd",   // 2 preserved, 1 non-preserved
        "ab...d",   // 2 preserved, 1 non-preserved
        "...cd",    // 2 preserved, 2 non-preserved
        "a...d",    // 2 preserved, 2 non-preserved
        "ab...",    // 2 preserved, 2 non-preserved
        "...d",     // 2 preserved, 3 non-preserved
        "a...",     // 2 preserved, 3 non-preserved
        "..."       // 2 preserved, 4 non-preserved
    };

    BOOST_CHECK_EQUAL_COLLECTIONS(candidates.begin(), candidates.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(string_shortening)
{
    auto isShortEnough = [] (std::string const& s) { return s.size() < 13; };
    auto s = shorten("fineImageFile.tif", "verycoarseImageFile.tif", isShortEnough);
    BOOST_CHECK_EQUAL(s[0], "fin...le.tif");
    BOOST_CHECK_EQUAL(s[1], "veryco...tif");

    s = shorten("aaaaaaabbbbbbb", "bbbbbbbaaaaaaa", isShortEnough);
    BOOST_CHECK_EQUAL(s[0], "aaa...bbbbbb");
    BOOST_CHECK_EQUAL(s[1], "bbb...aaaaaa");

    s = shorten("bbbbbbbaaaaaaa", "aaaaaaabbbbbbb", isShortEnough);
    BOOST_CHECK_EQUAL(s[0], "bbb...aaaaaa");
    BOOST_CHECK_EQUAL(s[1], "aaa...bbbbbb");

    s = shorten("000common1234shared", "commonABCsharedD", isShortEnough);
    BOOST_CHECK_EQUAL(s[0], "000...shared");
    BOOST_CHECK_EQUAL(s[1], "com...haredD");

    s = shorten("sitting", "kitten", isShortEnough);
    BOOST_CHECK_EQUAL(s[0], "sitting");
    BOOST_CHECK_EQUAL(s[1], "kitten");

    s = shorten("123MM456iiiiii789", "abcMMdefiiiiiighi", isShortEnough);
    BOOST_CHECK_EQUAL(s[0], "123MM4...789");
    BOOST_CHECK_EQUAL(s[1], "abcMMd...ghi");

}

BOOST_AUTO_TEST_SUITE_END()

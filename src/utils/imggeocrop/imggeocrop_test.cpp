#include "imggeocrop.h"

#include "Image.h"
#include "GeoInfo.h"
#include "exceptions.h"
#include "../helpers/utils_common.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(imgcrop_suite)

using namespace imagefusion;


// test parsing user crops with --crop-pix and --crop-proj
BOOST_AUTO_TEST_CASE(parse_crop)
{
    std::string dummy = "-f ../test_resources/images/formats/uint16x4.tif "; // to have a valid argument, a valid image is required
    GeoInfo gi("../test_resources/images/test_info_image.tif");
    CoordRectangle refRect{379545, 5963595, 389265 - 379545, 5973315 - 5963595}; // from gdalinfo
    CoordRectangle parsedRect;

    parsedRect = parseUserCrop<option::Parse>(dummy + "", gi);
    BOOST_CHECK_EQUAL(parsedRect, CoordRectangle{});

    parsedRect = parseUserCrop<option::Parse>(dummy + "--crop-pix=(-x 1 -y 2 -w 3 -h 2)", gi);
    BOOST_CHECK_EQUAL(parsedRect, gi.geotrans.imgToProj(CoordRectangle{1, 2, 3, 2}));

    parsedRect = parseUserCrop<option::Parse>(dummy + "--crop-proj=(-x 379545 -y 5963595 -w 3 -h 2)", gi);
    BOOST_CHECK_EQUAL(parsedRect, (CoordRectangle{379545, 5963595, 3, 2}));

    BOOST_CHECK_THROW(parseUserCrop<option::Parse>(dummy + "--crop-pix=(-x 1 -y 2 -w 3 -h 2)" + " --crop-pix=(-x 1 -y 2 -w 3 -h 2)", gi), invalid_argument_error);
    BOOST_CHECK_THROW(parseUserCrop<option::Parse>(dummy + "--crop-pix=(-x 1 -y 2 -w 3 -h 2)" + " --crop-proj=(-x 1 -y 2 -w 3 -h 2)", gi), invalid_argument_error);
    BOOST_CHECK_THROW(parseUserCrop<option::Parse>(dummy + "--crop-proj=(-x 1 -y 2 -w 3 -h 2)" + " --crop-proj=(-x 1 -y 2 -w 3 -h 2)", gi), invalid_argument_error);
}


// test parsing latitude / longitude extents
BOOST_AUTO_TEST_CASE(geo_extents)
{
    GeoInfo gi("../test_resources/images/test_info_image.tif");
    CoordRectangle refRect{379545, 5963595, 389265 - 379545, 5973315 - 5963595}; // from gdalinfo

    Coordinate offset   = gi.geotrans.imgToProj(Coordinate(0, gi.width()));
    Coordinate opposing = gi.geotrans.imgToProj(Coordinate(gi.height(), 0));
    CoordRectangle projRect{offset.x, offset.y, opposing.x - offset.x, opposing.y - offset.y};
    BOOST_CHECK_EQUAL(refRect, projRect);

    // describe the whole image (coordinates from gdalinfo)
    std::string twoCorners   = "--corner=(13d10' 0.94\"E, 53d53'39.37\"N) --corner=(13d19' 5.80\"E, 53d48'32.79\"N)";
    std::string centerWH     = "--center=(13d14'33.65\"E, 53d51' 6.17\"N) -w " + std::to_string(refRect.width) + " -h " + std::to_string(refRect.height);
    std::string cornerWH     = "--corner=(13d10' 0.94\"E, 53d53'39.37\"N) -w " + std::to_string(refRect.width) + " -h " + std::to_string(refRect.height);
    std::string cornerCenter = "--corner=(13d10' 0.94\"E, 53d53'39.37\"N) --center=(13d14'33.65\"E, 53d51' 6.17\"N)";

    for (std::string arg : {twoCorners, centerWH, cornerWH, cornerCenter}) {
        CoordRectangle parsedRect = parseAndConvertToProjSpace(arg, gi);
//        std::cout << parsedRect << std::endl;
        parsedRect.x = std::round(parsedRect.x);
        parsedRect.y = std::round(parsedRect.y);
        parsedRect.width = std::round(parsedRect.width);
        parsedRect.height = std::round(parsedRect.height);
        BOOST_CHECK_EQUAL(refRect, parsedRect);
    }
}



// test parsing latitude / longitude extents
BOOST_AUTO_TEST_CASE(geo_extents_intersection)
{
    // test image has a size of 324 x 324
    GeoInfo gi("../test_resources/images/test_info_image.tif");

    std::string twoCorners = "--corner=(13d10' 0.94\"E, 53d53'39.37\"N) --corner=(13d19' 5.80\"E, 53d48'32.79\"N)"; // full image in lat/long from gdalinfo
    CoordRectangle fullRect = parseAndConvertToProjSpace(twoCorners, gi);

    Coordinate c1 = gi.geotrans.imgToProj({1, 1});
    Coordinate c2 = gi.geotrans.imgToProj({323, 323});
    CoordRectangle smallRect{std::min(c1.x, c2.x), std::min(c1.y, c2.y),
                             std::abs(c1.x - c2.x), std::abs(c1.y - c2.y)};

    BOOST_CHECK_EQUAL(smallRect & fullRect, smallRect);
}



// test remapping
BOOST_AUTO_TEST_CASE(remap_test_line)
{
    Image i(2, 2, Type::float64x1);
    for (int x = 0; x < i.width(); ++x) {
        i.at<double>(x, 0) = x + 1;
        i.at<double>(x, 1) = x + 1;
    }

    Image ref;
    ref.cvMat() = (cv::Mat_<double>(3, 5) << 1, 1.1, 1.5, 1.9, 2,
                                             1, 1.1, 1.5, 1.9, 2);

    GeoInfo gi;
    gi.geotransSRS.SetWellKnownGeogCS("WGS84");
    gi.geotrans.scaleImage(2, 2);
    GeoInfo giRef = gi;
    giRef.size = ref.size();
    giRef.geotrans.scaleImage(2./5., 1);

    // Check that the expected values are matched
    auto scaled = i.warp(gi, giRef, InterpMethod::bilinear);
    for (int x = 0; x < ref.width(); ++x)
        BOOST_CHECK_CLOSE_FRACTION(ref.at<double>(x, 0), scaled.at<double>(x, 0), 1e-8);
}



BOOST_AUTO_TEST_CASE(remap_test_square)
{
    /* high data:
     * 0  2  4  6  8
     *10 12 14 16 18
     *20 22 24 26 28
     *30 32 34 36 38
     *40 42 44 46 48
     */
    Image imgHigh(5, 5, Type::uint8x1);
    for (int y = 0; y < imgHigh.height(); ++y)
        for (int x = 0; x < imgHigh.width(); ++x)
            imgHigh.at<uint8_t>(x, y) = 2*x + imgHigh.width() * 2*y;

    GeoInfo giHigh;
    giHigh.geotransSRS.SetWellKnownGeogCS("WGS84");
    giHigh.geotrans.offsetX = 1;
    giHigh.geotrans.offsetY = 2;
    giHigh.geotrans.XToX = 2;
    giHigh.geotrans.YToY = 2;

    /* low data:
     * 0 10 20
     *30 40 50
     *60 70 80
     */
    Image imgLow(3, 3, Type::uint8x1);
    for (int y = 0; y < imgLow.height(); ++y)
        for (int x = 0; x < imgLow.width(); ++x)
            imgLow.at<uint8_t>(x, y) = 10*x + imgLow.width() * 10*y;

    GeoInfo giLow;
    giLow.geotransSRS.SetWellKnownGeogCS("WGS84");
    giLow.geotrans.offsetX = 11;
    giLow.geotrans.offsetY = 11;
    giLow.geotrans.XToX = -3;
    giLow.geotrans.YToY = -3;

    GeoInfo giRef;
    giRef.geotransSRS.SetWellKnownGeogCS("WGS84");
    giRef.geotrans.offsetX = 2;
    giRef.geotrans.offsetY = 2;
    giRef.geotrans.XToX = 2;
    giRef.geotrans.YToY = 2;
    giRef.size = Size{4, 4};

    // high image cropped by one half pixel from left and right
    ConstImage scaledHigh = imgHigh.warp(giHigh, giRef, InterpMethod::bilinear);
    BOOST_CHECK_EQUAL(scaledHigh.size(), giRef.size);
    for (int y = 0; y < scaledHigh.height(); ++y) {
        for (int x = 0; x < scaledHigh.width(); ++x) {
            double vLeft  = imgHigh.doubleAt(x, y, 0);
            double vRight = imgHigh.doubleAt(x + 1, y, 0);
            BOOST_CHECK_EQUAL(scaledHigh.at<uint8_t>(x, y),
                              cv::saturate_cast<uint8_t>(0.5 * vLeft + 0.5 * vRight));
        }
    }

    ConstImage scaledLow = imgLow.warp(giLow, giRef, InterpMethod::bilinear);
    BOOST_CHECK_EQUAL(scaledLow.size(), giRef.size);
    cv::Mat val(1, 1, CV_8UC1);
    auto conv = [] (double x) { return (x + 0.5) * 2 / 3 - 0.5; };
    for (int y = 0; y < scaledLow.height(); ++y) {
        for (int x = 0; x < scaledLow.width(); ++x) {
            // get bilinear interpolation at from the correct source location
            double srcX = imgLow.width()  - 1 - conv(x);
            double srcY = imgLow.height() - 1 - conv(y);
            cv::getRectSubPix(imgLow.cvMat(), val.size(), cv::Point2f(srcX, srcY), val);

            // getRectSubPix seems to be much more precise than remap (and similar),
            // thus a difference of 1 is permitted
            BOOST_CHECK_LE(std::abs((int)(val.at<uint8_t>(0, 0)) - (int)(scaledLow.at<uint8_t>(x, y))), 1);
        }
    }
}



BOOST_AUTO_TEST_CASE(test_new_nodata_value)
{
    double nodata;
    Size sz{10, 10};
    Image i_int8(sz, Type::int8x1);
    Image i_uint8(sz, Type::uint8x1);
    Image i_int16(sz, Type::int16x1);
    Image i_uint16(sz, Type::uint16x1);

    // all have values 1, ..., 100
    for (int y = 0; y < sz.height; ++y) {
        for (int x = 0; x < sz.width; ++x) {
            int val = 1 + x + y * sz.width;
            i_int8.at<int8_t>(x, y) = val;
            i_uint8.at<uint8_t>(x, y) = val;
            i_int16.at<int16_t>(x, y) = val;
            i_uint16.at<uint16_t>(x, y) = val;
        }
    }
    nodata = helpers::findAppropriateNodataValue(i_int8, {});
    BOOST_CHECK_EQUAL(nodata, -99);

    nodata = helpers::findAppropriateNodataValue(i_uint8, {});
    BOOST_CHECK_EQUAL(nodata, getImageRangeMax(i_uint8.type()));

    nodata = helpers::findAppropriateNodataValue(i_int16, {});
    BOOST_CHECK_EQUAL(nodata, -9999);

    nodata = helpers::findAppropriateNodataValue(i_uint16, {});
    BOOST_CHECK_EQUAL(nodata, getImageRangeMax(i_uint16.type()));

    // occupy default value
    i_int8.at<int8_t>(0, 0) = -99;
    nodata = helpers::findAppropriateNodataValue(i_int8, {});
    BOOST_CHECK_EQUAL(nodata, getImageRangeMin(i_int8.type())); // -128

    i_uint8.at<uint8_t>(0, 0) = getImageRangeMax(i_uint8.type());
    nodata = helpers::findAppropriateNodataValue(i_uint8, {});
    BOOST_CHECK_EQUAL(nodata, getImageRangeMax(i_uint8.type()) - 1);

    i_int16.at<int16_t>(0, 0) = -9999;
    nodata = helpers::findAppropriateNodataValue(i_int16, {});
    BOOST_CHECK_EQUAL(nodata, getImageRangeMin(i_int16.type())); // -32768

    i_uint16.at<uint16_t>(0, 0) = getImageRangeMax(i_uint16.type());
    nodata = helpers::findAppropriateNodataValue(i_uint16, {});
    BOOST_CHECK_EQUAL(nodata, getImageRangeMax(i_uint16.type()) - 1);
}




BOOST_AUTO_TEST_SUITE_END()

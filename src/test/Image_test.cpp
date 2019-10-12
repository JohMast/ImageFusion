#include "Image.h"
#include "MultiResImages.h"
#include "GeoInfo.h"
#include "helpers_test.h"

#include <memory>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <iomanip>

#include <boost/test/unit_test.hpp>

#include <gdal.h>
#include <gdal_priv.h>

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(Image_Suite)


// test to construct images with different types and check properties like width, height and channels
BOOST_AUTO_TEST_CASE(construction)
{
    // check default constructor... some functions rely on these properties
    Image def{};
    BOOST_CHECK_EQUAL(def.channels(), 1);
    BOOST_CHECK_EQUAL(def.height(),   0);
    BOOST_CHECK_EQUAL(def.width(),    0);
    BOOST_CHECK_EQUAL(def.type(),     Type::uint8x1);
    BOOST_CHECK(def.begin<uint8_t>()  == def.end<uint8_t>());
    BOOST_CHECK(def.begin<uint8_t>(0) == def.end<uint8_t>(0));
    BOOST_CHECK(def.empty());

    Image ic1{5, 6, Type::uint8};
    Image ic2{Size{5, 8}, Type::uint8x2};
    Image ic3{7, 8, Type::uint8x3};
    Image ic10{70, 80, Type::uint8x10};

    BOOST_CHECK_EQUAL(ic1.channels(), 1);
    BOOST_CHECK_EQUAL(ic2.channels(), 2);
    BOOST_CHECK_EQUAL(ic3.channels(), 3);
    BOOST_CHECK_EQUAL(ic10.channels(), 10);
    BOOST_CHECK_EQUAL(ic1.width(),    5);
    BOOST_CHECK_EQUAL(ic1.height(),   6);
    BOOST_CHECK_EQUAL(ic2.width(),    5);
    BOOST_CHECK_EQUAL(ic2.height(),   8);
    BOOST_CHECK_EQUAL(ic3.width(),    7);
    BOOST_CHECK_EQUAL(ic3.height(),   8);
    BOOST_CHECK_EQUAL(ic10.width(),    70);
    BOOST_CHECK_EQUAL(ic10.height(),   80);

    Size s1 = ic1.size();
    Size s2 = ic2.size();
    Size s3 = ic3.size();
    BOOST_CHECK_EQUAL(s1.width,    5);
    BOOST_CHECK_EQUAL(s1.height,   6);
    BOOST_CHECK_EQUAL(s2.width,    5);
    BOOST_CHECK_EQUAL(s2.height,   8);
    BOOST_CHECK_EQUAL(s3.width,    7);
    BOOST_CHECK_EQUAL(s3.height,   8);
}


BOOST_AUTO_TEST_CASE(copy_move_swap)
{
    Image ic1{5, 6, Type::uint8x1};
    Image ic10{70, 80, Type::uint8x10};
    Image shared1{ic1.sharedCopy()};
    Image shared10{ic10.sharedCopy()};

//    std::cout << "swap: " << std::endl;
    using std::swap;
    swap(ic1, ic10);
    BOOST_CHECK_EQUAL(ic1.type(), Type::uint8x10);
    BOOST_CHECK_EQUAL(ic10.type(), Type::uint8x1);
    BOOST_CHECK(shared1.isSharedWith(ic10));
    BOOST_CHECK(shared10.isSharedWith(ic1));

//    std::cout << "move: " << std::endl;
    Image ic2 = std::move(ic1);
    BOOST_CHECK_EQUAL(ic2.type(), Type::uint8x10);
    BOOST_CHECK(shared10.isSharedWith(ic2));

//    std::cout << "copy: " << std::endl;
    Image ic4 = ic2;
    BOOST_CHECK_EQUAL(ic2.type(), Type::uint8x10);
    BOOST_CHECK_EQUAL(ic4.type(), Type::uint8x10);
    BOOST_CHECK(!ic2.isSharedWith(ic4));

//    std::cout << "copy assign: " << std::endl;
    ic1 = ic4;
    BOOST_CHECK_EQUAL(ic1.type(), Type::uint8x10);
    BOOST_CHECK_EQUAL(ic4.type(), Type::uint8x10);
    BOOST_CHECK(!ic1.isSharedWith(ic4));

//    std::cout << "move assign: " << std::endl;
    ic1 = std::move(ic10);
    BOOST_CHECK_EQUAL(ic1.type(), Type::uint8x1);
    BOOST_CHECK(shared1.isSharedWith(ic1));
}


BOOST_AUTO_TEST_CASE(conversion_constructors_and_assignment)
{
    Image i{5, 6, Type::uint8x1};
    Image const& ci = i;
    BOOST_CHECK(i.isSharedWith(ci));

    // non-const source
    // constructors
    ConstImage i_const_shared  = i.constSharedCopy();
    ConstImage i_shared        = i.sharedCopy();
    Image      i_nonConst_shared{i.sharedCopy()};
    ConstImage i_clone1        = i.clone();
    ConstImage i_clone2        = i;

    BOOST_CHECK(i.isSharedWith(i_const_shared));
    BOOST_CHECK(i.isSharedWith(i_shared));
    BOOST_CHECK(i.isSharedWith(i_nonConst_shared));
    BOOST_CHECK(!i.isSharedWith(i_clone1));
    BOOST_CHECK(!i.isSharedWith(i_clone2));

    // assignment
    i_shared = i.sharedCopy();
    i_clone1 = i.clone();
    i_clone2 = i;

    BOOST_CHECK(i.isSharedWith(i_shared));
    BOOST_CHECK(!i.isSharedWith(i_clone1));
    BOOST_CHECK(!i.isSharedWith(i_clone2));

    // ref
    ConstImage&       i_ref    = i;
    ConstImage const& i_cref   = i;
    BOOST_CHECK(i.isSharedWith(i_ref));
    BOOST_CHECK(i.isSharedWith(i_cref));

    // const source
    // constructors
    ConstImage        ci_shared = ci.sharedCopy();
    ConstImage        ci_clone1 = ci.clone();
    ConstImage        ci_clone2 = ci;

    BOOST_CHECK(i.isSharedWith(ci_shared));
    BOOST_CHECK(!i.isSharedWith(ci_clone1));
    BOOST_CHECK(!i.isSharedWith(ci_clone2));

    // assignment
    ci_shared = ci.sharedCopy();
    ci_clone1 = ci.clone();
    ci_clone2 = ci;

    BOOST_CHECK(i.isSharedWith(ci_shared));
    BOOST_CHECK(!i.isSharedWith(ci_clone1));
    BOOST_CHECK(!i.isSharedWith(ci_clone2));

    // ref
    ConstImage const& ci_cref   = ci;
    BOOST_CHECK(i.isSharedWith(ci_cref));
}


std::string testFun1(Image&& /*i*/) {
    return "rref";
}

std::string testFun1(ConstImage const& /*i*/) {
    return "cref";
}

ConstImage testFun2(ConstImage i) { // calling this with an rvalue-ref on ConstImage, will make a sharedCopy
    return i;
}

BOOST_AUTO_TEST_CASE(rvalue_arguments)
{
    constexpr uint8_t some_val = 42;

    std::shared_ptr<MultiResImages> imgs{new MultiResImages{}};
//    auto imgs = std::make_shared<MultiResImages>();
    imgs->set("", 0, Image{1, 1, Type::uint8x1});

    MultiResImages* imgs_raw = new MultiResImages{};
    imgs_raw->set("", 0, Image{1, 1, Type::uint8x1});
    imgs_raw->getAny().at<uint8_t>(0,0) = some_val;
    std::shared_ptr<const MultiResImages> const_imgs{imgs_raw};

    // check that even calling testFun1 with non-const sharedCopy choses testFun1(ConstImage const&) version, since Image(cv::Mat) is explicit
//    std::cout << "Call testFun1 with imgs: " << testFun1(imgs->getAny().sharedCopy()) << std::endl;
    BOOST_CHECK_EQUAL(testFun1(imgs->getAny().sharedCopy()), "cref");
//    std::cout << "Call testFun1 with const_imgs: " << testFun1(const_imgs->getAny().sharedCopy()) << std::endl;
    BOOST_CHECK_EQUAL(testFun1(const_imgs->getAny().sharedCopy()), "cref");

//    std::cout << "Call absdiff with imgs" << std::endl;
//    imgs->getAny().sharedCopy().absdiff(imgs->getAny().sharedCopy()); does not compile :-)
//    std::cout << "Call absdiff with const_imgs" << std::endl;
    BOOST_CHECK_NO_THROW(const_imgs->getAny().sharedCopy().absdiff(const_imgs->getAny().sharedCopy()));

    // check that memory will not be swapped away (only the img pointer in the unnamed sharedCopy object will)
    ConstImage sharedCopy = testFun2(const_imgs->getAny().sharedCopy());
    BOOST_CHECK_EQUAL(const_imgs->getAny().at<uint8_t>(0,0), some_val);
    BOOST_CHECK(const_imgs->getAny().isSharedWith(sharedCopy));
}


// test read and write with all data types in tiff format
BOOST_AUTO_TEST_CASE(read_write_types_in_tiff)
{
    // test exception for auto detect, but no file extension
    Image temp;
    BOOST_CHECK_THROW(temp.write("filename-with-no-extension"), file_format_error);

    // test for each image type up to 5 channels to read-write-read a TIFF image
    std::vector<Type> types{
        Type::uint8,   Type::uint8x2,   Type::uint8x3,   Type::uint8x4,   Type::uint8x5,
        Type::int16,   Type::int16x2,   Type::int16x3,   Type::int16x4,   Type::int16x5,
        Type::uint16,  Type::uint16x2,  Type::uint16x3,  Type::uint16x4,  Type::uint16x5,
        Type::int32,   Type::int32x2,   Type::int32x3,   Type::int32x4,   Type::int32x5,
        Type::float32, Type::float32x2, Type::float32x3, Type::float32x4, Type::float32x5,
        Type::float64, Type::float64x2, Type::float64x3, Type::float64x4, Type::float64x5
    };
    for (Type t : types) {
        // test read
        std::string filename = to_string(t) + ".tif";
//        if (getChannels(t) == 5) { // generate some simple images
//            Image i{5, 6, t};
////            i.set({10, 20, 30, 40, 50});
//            for (int y = 0; y < i.height(); ++y) {
//                for (int x = 0; x < i.width(); ++x) {
//                    for (unsigned int channel = 0; channel < i.channels(); ++channel) {
//                        switch(i.basetype()) {
//                        case Type::uint8:
//                            i.at<uint8_t>(x, y, channel) = 10 * (channel + 1) * y + x;

//                        case Type::int8:
//                            i.at<int8_t>(x, y, channel) = 10 * (channel + 1) * y + x;

//                        case Type::uint16:
//                            i.at<uint16_t>(x, y, channel) = 10 * (channel + 1) * y + x;

//                        case Type::int16:
//                            i.at<int16_t>(x, y, channel) = 10 * (channel + 1) * y + x;

//                        case Type::int32:
//                            i.at<int32_t>(x, y, channel) = 10 * (channel + 1) * y + x;

//                        case Type::float32:
//                            i.at<float>(x, y, channel) = 10 * (channel + 1) * y + x;

//                        default: //Type::float64:
//                            i.at<double>(x, y, channel) = 10 * (channel + 1) * y + x;
//                        }
//                    }
//                }
//            }
//            i.write("../test_resources/images/formats/" + filename);
//        }
        Image i{"../test_resources/images/formats/" + filename};
        BOOST_CHECK_EQUAL(i.type(), t);

        // test write
        i.write("../test_resources/images/formats/out_" + filename);
        i.read("../test_resources/images/formats/out_" + filename);
        BOOST_CHECK_EQUAL(i.type(), t);
    }
}


// test reading of subdatasets file and combining different resolution layers
BOOST_AUTO_TEST_CASE(subdatasets)
{
    std::string filename = "test.nc";
    if (!createMultiImageFile(filename))
        return;

    // test that loading all subdatasets is invalid because of different data types
    BOOST_CHECK_THROW((Image{filename}), image_type_error);

    // test that subdataset 1 has the values it should have
    {
        Image i;
        BOOST_CHECK_NO_THROW((i = Image{filename, std::vector<int>{0}}));
        BOOST_CHECK_EQUAL(i.size(), Size(5, 5));
        BOOST_CHECK_EQUAL(i.type(), Type::uint8x1);
        for (int y = 0; y < i.height(); ++y)
            for (int x = 0; x < i.width(); ++x)
                BOOST_CHECK_EQUAL(i.at<uint8_t>(x, y), x + y * 5 + 100);
    }

    // test that the combination subdataset 1+2 (both uint8) is valid
    {
        Image i;
        BOOST_CHECK_NO_THROW((i = Image{filename, std::vector<int>{0, 1}}));
        BOOST_CHECK_EQUAL(i.size(), Size(5, 5));
        BOOST_CHECK_EQUAL(i.type(), Type::uint8x2);
        for (int y = 0; y < i.height(); ++y)
            for (int x = 0; x < i.width(); ++x)
                for (unsigned int c = 0; c < i.channels(); ++c)
                    if (c == 0)
                        BOOST_CHECK_EQUAL(i.at<uint8_t>(x, y, c), x + y * 5 + 100);
                    else
                        BOOST_CHECK_EQUAL(i.at<uint8_t>(x, y, c), x + y * 5 + 200);
    }

    // test that the combination subdataset 3+4 (both uint16) is valid
    {
        Image i;
        BOOST_CHECK_NO_THROW((i = Image{filename, std::vector<int>{2, 3}}));
        BOOST_CHECK_EQUAL(i.size(), Size(5, 5));
        BOOST_CHECK_EQUAL(i.type(), Type::uint16x2);
        for (int y = 0; y < i.height(); ++y)
            for (int x = 0; x < i.width(); ++x)
                for (unsigned int c = 0; c < i.channels(); ++c)
                    if (c == 0)
                        BOOST_CHECK_EQUAL(i.at<uint16_t>(x, y, c), x + y * 5 + 3000);
                    else
                        BOOST_CHECK_EQUAL(i.at<uint16_t>(x, y, c), x + y * 5 + 4000);
    }

    // test that the combination subdataset 1+3 (uint8, uint16) is invalid
    BOOST_CHECK_THROW((Image{filename, std::vector<int>{0, 2}}), image_type_error);

    // test reading a subdataset with special GDAL filename
    {
        Image img_num{filename, std::vector<int>{0}};
        Image img_name;
        BOOST_CHECK_NO_THROW(img_name = Image{"NETCDF:\"" + filename + "\":Band1"});
        BOOST_CHECK_EQUAL(img_num.size(), img_name.size());
        BOOST_CHECK_EQUAL(img_num.type(), img_name.type());
        for (int y = 0; y < img_num.height() && y < img_name.height(); ++y)
            for (int x = 0; x < img_num.width() && x < img_name.width(); ++x)
                BOOST_CHECK_EQUAL(img_num.at<uint8_t>(x, y), img_name.at<uint8_t>(x, y));
    }
}


// test 1-bit mask files, RGBA indexed mask file, gray-alpha indexed file, rgb indexed file, rgba indexed file
// test that conversion works (`conv`) and that reading without conversion works (`index`)
BOOST_AUTO_TEST_CASE(read_colortable_images)
{
    Image conv;
    Image index;
    // test 1-bit (black/white) mask file. It is all white, except on diagonal, where it is black
    BOOST_CHECK_NO_THROW(conv = Image{"../test_resources/images/formats/uint8x1_1bit_colortable.png"});
    BOOST_CHECK_NO_THROW((index = Image{"../test_resources/images/formats/uint8x1_1bit_colortable.png", /*channels*/ {}, /*crop*/ Rectangle{}, /*flipH*/ false, /*flipV*/ false, /*ignoreColorTable*/ true}));
    BOOST_CHECK_EQUAL(conv.size(), Size(5, 6));
    BOOST_CHECK_EQUAL(conv.type(), Type::uint8x1);
    BOOST_CHECK_EQUAL(index.size(), Size(5, 6));
    BOOST_CHECK_EQUAL(index.type(), Type::uint8x1);
    for (int y = 0; y < conv.height(); ++y) {
        for (int x = 0; x < conv.width(); ++x) {
            BOOST_CHECK_EQUAL(conv.at<uint8_t>(x, y), x == y ? 0 : 255); // 255 everywhere except on diagonal
            BOOST_CHECK_EQUAL(index.at<uint8_t>(x, y), x == y ? 0 : 1); // 255 everywhere except on diagonal
        }
    }

    // test same image as 1-bit mask file, but with a full color table. It is all white, except on diagonal, where it is black
    BOOST_CHECK_NO_THROW(conv = Image{"../test_resources/images/formats/uint8x1_colortable.png"});
    BOOST_CHECK_NO_THROW((index = Image{"../test_resources/images/formats/uint8x1_colortable.png", /*channels*/ {}, /*crop*/ Rectangle{}, /*flipH*/ false, /*flipV*/ false, /*ignoreColorTable*/ true}));
    BOOST_CHECK_EQUAL(conv.size(), Size(5, 6));
    BOOST_CHECK_EQUAL(conv.type(), Type::uint8x1);
    BOOST_CHECK_EQUAL(index.size(), Size(5, 6));
    BOOST_CHECK_EQUAL(index.type(), Type::uint8x1);
    uint8_t black = index.at<uint8_t>(0, 0);
    uint8_t white = index.at<uint8_t>(1, 0);
    for (int y = 0; y < conv.height(); ++y) {
        for (int x = 0; x < conv.width(); ++x) {
            BOOST_CHECK_EQUAL(conv.at<uint8_t>(x, y), x == y ? 0 : 255); // 255 everywhere except on diagonal
            BOOST_CHECK_EQUAL(index.at<uint8_t>(x, y), x == y ? black : white); // white everywhere except on diagonal
        }
    }

    // test Gray-Alpha indexed file. C0: 5*x + 40*y, C1: 255 - 5*x - 40*y
    BOOST_CHECK_NO_THROW(conv = Image{"../test_resources/images/formats/uint8x2_colortable.png"});
    BOOST_CHECK_NO_THROW((index = Image{"../test_resources/images/formats/uint8x2_colortable.png", /*channels*/ {}, /*crop*/ Rectangle{}, /*flipH*/ false, /*flipV*/ false, /*ignoreColorTable*/ true}));
    BOOST_CHECK_EQUAL(conv.size(), Size(6, 5));
    BOOST_CHECK_EQUAL(conv.type(), Type::uint8x2);
    BOOST_CHECK_EQUAL(index.size(), Size(6, 5));
    BOOST_CHECK_EQUAL(index.type(), Type::uint8x1);
    for (int y = 0; y < conv.height(); ++y)
        for (int x = 0; x < conv.width(); ++x)
            for (unsigned int c = 0; c < conv.channels(); ++c)
                BOOST_CHECK_EQUAL(conv.at<uint8_t>(x, y, c), c == 0 ? 5*x + 40*y :
                                                           /*c == 1*/ 255 - 5*x - 40*y);

    // test RGB indexed file. C0: 5*x + 40*y, C1: 255 - 5*x - 40*y, C2: 40*x + 5*y
    BOOST_CHECK_NO_THROW(conv = Image{"../test_resources/images/formats/uint8x3_colortable.png"});
    BOOST_CHECK_NO_THROW((index = Image{"../test_resources/images/formats/uint8x3_colortable.png", /*channels*/ {}, /*crop*/ Rectangle{}, /*flipH*/ false, /*flipV*/ false, /*ignoreColorTable*/ true}));
    BOOST_CHECK_EQUAL(conv.size(), Size(6, 5));
    BOOST_CHECK_EQUAL(conv.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(index.size(), Size(6, 5));
    BOOST_CHECK_EQUAL(index.type(), Type::uint8x1);
    for (int y = 0; y < conv.height(); ++y)
        for (int x = 0; x < conv.width(); ++x)
            for (unsigned int c = 0; c < conv.channels(); ++c)
                BOOST_CHECK_EQUAL(conv.at<uint8_t>(x, y, c), c == 0 ? 5*x + 40*y :
                                                             c == 1 ? 255 - 5*x - 40*y :
                                                           /*c == 2*/ 40*x + 5*y);

    // test RGB-Alpha indexed file. C0: x + 10*y, C1: x + 20*y, C2: x + 30*y, C3: x + 40*y
    BOOST_CHECK_NO_THROW(conv = Image{"../test_resources/images/formats/uint8x4_colortable.png"});
    BOOST_CHECK_NO_THROW((index = Image{"../test_resources/images/formats/uint8x4_colortable.png", /*channels*/ {}, /*crop*/ Rectangle{}, /*flipH*/ false, /*flipV*/ false, /*ignoreColorTable*/ true}));
    BOOST_CHECK_EQUAL(conv.size(), Size(5, 6));
    BOOST_CHECK_EQUAL(conv.type(), Type::uint8x4);
    BOOST_CHECK_EQUAL(index.size(), Size(5, 6));
    BOOST_CHECK_EQUAL(index.type(), Type::uint8x1);
    for (int y = 0; y < conv.height(); ++y)
        for (int x = 0; x < conv.width(); ++x)
            for (unsigned int c = 0; c < conv.channels(); ++c)
                BOOST_CHECK_EQUAL(conv.at<uint8_t>(x, y, c), c == 0 ? x + 10*y :
                                                             c == 1 ? x + 20*y :
                                                             c == 2 ? x + 30*y :
                                                           /*c == 3*/ x + 40*y);
}

// convert uint8x2 and uint8x4 to indexed images for testing in read_colortable_images
/*
BOOST_AUTO_TEST_CASE(write_colortable_images)
{
    // convert uint8x2 to indexed gray-alpha image
    Image i{"../test_resources/images/formats/uint8x2.tif"};
    Image indexed{i.size(), Type::uint8x1};

    GDALColorTable gt;
    for (int i = 0; i <= 255/5; ++i) {
        GDALColorEntry ce{short(i*5), short(i*5), short(i*5), short(255 - i*5)};
        gt.SetColorEntry(i, &ce);
    }

    for (int y = 0; y < i.height(); ++y)
        for (int x = 0; x < i.width(); ++x)
            indexed.at<uint8_t>(x, y) = x + 8*y;

    // write with GDAL API
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("PNG");
    GDALDataset* poSrcDS = const_cast<GDALDataset*>(indexed.asGDALDataset());
    poSrcDS->GetRasterBand(1)->SetColorTable(&gt);
    GDALDataset* poDstDS = driver->CreateCopy("uint8x2_colortable.png", poSrcDS, FALSE, nullptr, nullptr, nullptr);
    GDALClose(poSrcDS);
    GDALClose(poDstDS);

    // convert uint8x4 to indexed RGB-alpha image
    i = Image{"../test_resources/images/formats/uint8x4.tif"};
    indexed = Image{i.size(), Type::uint8x1};

    gt = GDALColorTable{};
    for (int i = 0; i <= 5; ++i) {
        for (int j = 0; j <= 4; ++j) {
            GDALColorEntry ce{short(i*10 + j), short(i*20 + j), short(i*30 + j), short(i*40 + j)};
            gt.SetColorEntry(j + 5*i, &ce);
        }
    }

    for (int y = 0; y < i.height(); ++y)
        for (int x = 0; x < i.width(); ++x)
            indexed.at<uint8_t>(x, y) = x + 5*y;

    // write with GDAL API
    poSrcDS = const_cast<GDALDataset*>(indexed.asGDALDataset());
    poSrcDS->GetRasterBand(1)->SetColorTable(&gt);
    poDstDS = driver->CreateCopy("uint8x4_colortable.png", poSrcDS, FALSE, nullptr, nullptr, nullptr);
    GDALClose(poSrcDS);
    GDALClose(poDstDS);
}
*/

// test Image::doubleAt and Image::setValueAt
BOOST_AUTO_TEST_CASE(set_and_get_as_double)
{
    // test write-read a cropped image
    Image u8{ 5, 6, Type::uint8x1};
    Image s8{ 5, 6, Type::int8x1};
    Image u16{5, 6, Type::uint16x1};
    Image s16{5, 6, Type::int16x1};
    Image s32{5, 6, Type::int32x1};
    Image f32{5, 6, Type::float32x1};
    Image f64{5, 6, Type::float64x1};
    for (int y = 0; y < u8.height(); ++y) {
        for (int x = 0; x < u8.width(); ++x) {
            u8.at<uint8_t>(x, y) = 10 * y + x;
            BOOST_CHECK_EQUAL(     10 * y + x, u8.doubleAt(x, y, 0));
            u8.setValueAt(x, y, 0, 9 * y + x);
            BOOST_CHECK_EQUAL(     9 * y + x, u8.doubleAt(x, y, 0));

            s8.at<int8_t>(x, y) = -10 * y + x;
            BOOST_CHECK_EQUAL(    -10 * y + x, s8.doubleAt(x, y, 0));
            s8.setValueAt(x, y, 0, -9 * y + x);
            BOOST_CHECK_EQUAL(     -9 * y + x, s8.doubleAt(x, y, 0));

            u16.at<uint16_t>(x, y) = 100 * y + x;
            BOOST_CHECK_EQUAL(       100 * y + x, u16.doubleAt(x, y, 0));
            u16.setValueAt(x, y, 0, 99 * y + x);
            BOOST_CHECK_EQUAL(      99 * y + x, u16.doubleAt(x, y, 0));

            s16.at<int16_t>(x, y) = -100 * y + x;
            BOOST_CHECK_EQUAL(      -100 * y + x, s16.doubleAt(x, y, 0));
            s16.setValueAt(x, y, 0, -99 * y + x);
            BOOST_CHECK_EQUAL(      -99 * y + x, s16.doubleAt(x, y, 0));

            s32.at<int32_t>(x, y) = 10000 * y + x;
            BOOST_CHECK_EQUAL(      10000 * y + x, s32.doubleAt(x, y, 0));
            s32.setValueAt(x, y, 0, -9999 * y + x);
            BOOST_CHECK_EQUAL(      -9999 * y + x, s32.doubleAt(x, y, 0));

            f32.at<float>(x, y) = 0.5 * y + x;
            BOOST_CHECK_EQUAL(    0.5 * y + x, f32.doubleAt(x, y, 0));
            f32.setValueAt(x, y, 0, 0.25 * y + x);
            BOOST_CHECK_EQUAL(      0.25 * y + x, f32.doubleAt(x, y, 0));

            f64.at<double>(x, y) = 0.0625 * y + x;
            BOOST_CHECK_EQUAL(     0.0625 * y + x, f64.doubleAt(x, y, 0));
            f64.setValueAt(x, y, 0, 0.125 * y + x);
            BOOST_CHECK_EQUAL(      0.125 * y + x, f64.doubleAt(x, y, 0));
        }
    }
}


// test write and read a cropped image
BOOST_AUTO_TEST_CASE(read_write_cropped)
{
    // test write-read a cropped image
    Image cropped{5, 6, Type::uint8x5};
    for (int y = 0; y < cropped.height(); ++y)
        for (int x = 0; x < cropped.width(); ++x)
            for (unsigned int c = 0; c < cropped.channels(); ++c)
                cropped.at<uint8_t>(x, y, c) = 40 * c + 10 * y + x;
    cropped.crop(Rectangle{1,1,2,2});
    cropped.write("../test_resources/images/formats/cropped.tif");

    Image same{"../test_resources/images/formats/cropped.tif"};
    BOOST_CHECK_EQUAL(same.channels(),               5);
    BOOST_CHECK_EQUAL(same.height(),                 2);
    BOOST_CHECK_EQUAL(same.width(),                  2);
    BOOST_CHECK_EQUAL(same.getOriginalSize().height, 2);
    BOOST_CHECK_EQUAL(same.width(),                  2);
    BOOST_CHECK_EQUAL(same.getOriginalSize().width,  2);
    BOOST_CHECK_EQUAL(same.type(),                   Type::uint8x5);
    for (int y = 0; y < cropped.height(); ++y)
        for (int x = 0; x < cropped.width(); ++x)
            for (unsigned int c = 0; c < cropped.channels(); ++c)
                BOOST_CHECK_EQUAL(cropped.at<uint8_t>(x, y, c), same.at<uint8_t>(x, y, c));
}


// test read options: crop rectangle, layer selection, flipping
BOOST_AUTO_TEST_CASE(read_options)
{
    // use files with known values:
    // uint8x5 has 5 channels, with values 10 * (c + 1) * y + x
    // uint8x4 has 4 channels, with values 10 * (c + 1) * y + x
    // uint8x3 has 3 channels, the first channel has values 40 * y + 5 * x
    std::string file5c = "../test_resources/images/formats/uint8x5.tif";
    std::string file4c = "../test_resources/images/formats/uint8x4.tif";
    std::string file3c = "../test_resources/images/formats/uint8x3.tif";
    int height = 6;
    int width  = 5;
    Image temp;

    // read only specified channels
    temp.read(file4c, std::vector<int>{1,3});
    BOOST_CHECK_EQUAL(temp.width(),  width);
    BOOST_CHECK_EQUAL(temp.height(), height);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x2>::array_type>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel[1], 40 * y + x);
        }
    }

    // read only a specified region
    temp.read(file4c, {}, Rectangle{1,2,3,2});
    BOOST_CHECK_EQUAL(temp.width(),  3);
    BOOST_CHECK_EQUAL(temp.height(), 2);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x4>::array_type>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * (y+2) + x+1);
            BOOST_CHECK_EQUAL(pixel[1], 20 * (y+2) + x+1);
            BOOST_CHECK_EQUAL(pixel[2], 30 * (y+2) + x+1);
            BOOST_CHECK_EQUAL(pixel[3], 40 * (y+2) + x+1);
        }
    }

    // read only specified channels in a specified region
    temp.read(file4c, std::vector<int>{1,3}, Rectangle{1,2,3,2});
    BOOST_CHECK_EQUAL(temp.width(),  3);
    BOOST_CHECK_EQUAL(temp.height(), 2);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x2>::array_type>(x, y);
            //                          10 * (y+2) + x+1);
            BOOST_CHECK_EQUAL(pixel[0], 20 * (y+2) + x+1);
            //                          30 * (y+2) + x+1);
            BOOST_CHECK_EQUAL(pixel[1], 40 * (y+2) + x+1);
        }
    }

    // read multiple times the same channel specified channels in a specified region
    temp.read(file3c, std::vector<int>{0,0,0,0}, Rectangle{1,2,3,2});
    BOOST_CHECK_EQUAL(temp.width(),    3);
    BOOST_CHECK_EQUAL(temp.height(),   2);
    BOOST_CHECK_EQUAL(temp.channels(), 4);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x4>::array_type>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 40 * (y+2) + 5*(x+1));
            BOOST_CHECK_EQUAL(pixel[1], 40 * (y+2) + 5*(x+1));
            BOOST_CHECK_EQUAL(pixel[2], 40 * (y+2) + 5*(x+1));
            BOOST_CHECK_EQUAL(pixel[3], 40 * (y+2) + 5*(x+1));
        }
    }

    // read the image horizontally flipped
    temp.read(file4c, std::vector<int>{}, Rectangle{}, true, false);
    BOOST_CHECK_EQUAL(temp.width(),  width);
    BOOST_CHECK_EQUAL(temp.height(), height);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x4>::array_type>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * y + width - 1 - x);
            BOOST_CHECK_EQUAL(pixel[1], 20 * y + width - 1 - x);
            BOOST_CHECK_EQUAL(pixel[2], 30 * y + width - 1 - x);
            BOOST_CHECK_EQUAL(pixel[3], 40 * y + width - 1 - x);
        }
    }

    // read the image vertically flipped
    temp.read(file4c, std::vector<int>{}, Rectangle{}, false, true);
    BOOST_CHECK_EQUAL(temp.width(),  width);
    BOOST_CHECK_EQUAL(temp.height(), height);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x4>::array_type>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * (height - 1 - y) + x);
            BOOST_CHECK_EQUAL(pixel[1], 20 * (height - 1 - y) + x);
            BOOST_CHECK_EQUAL(pixel[2], 30 * (height - 1 - y) + x);
            BOOST_CHECK_EQUAL(pixel[3], 40 * (height - 1 - y) + x);
        }
    }

    // read the image horizontally and vertically flipped
    temp.read(file4c, std::vector<int>{}, Rectangle{}, true, true);
    BOOST_CHECK_EQUAL(temp.width(),  width);
    BOOST_CHECK_EQUAL(temp.height(), height);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<DataType<Type::uint8x4>::array_type>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * (height - 1 - y) + width - 1 - x);
            BOOST_CHECK_EQUAL(pixel[1], 20 * (height - 1 - y) + width - 1 - x);
            BOOST_CHECK_EQUAL(pixel[2], 30 * (height - 1 - y) + width - 1 - x);
            BOOST_CHECK_EQUAL(pixel[3], 40 * (height - 1 - y) + width - 1 - x);
        }
    }

    // read only a specified region and channel flipped
    /*  0  1  2  3  4
     * 10 11 12_13_14
     * 20 21/22 23 24\  ----\ 34 33 32
     * 30 31\32_33_34/  ----/ 24 23 22
     * 40 41 42 43 44
     * 50 51 52 53 54
     */
    temp.read(file4c, std::vector<int>{0}, Rectangle{2,2,3,2}, true, true);
    BOOST_CHECK_EQUAL(temp.width(),  3);
    BOOST_CHECK_EQUAL(temp.height(), 2);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<uint8_t>(x, y);
            BOOST_CHECK_EQUAL(pixel, 10 * (2 + 2 - 1 - y) + (2 + 3) - 1 - x);
            //                           (r.y+r.h- 1 - y) + r.x+r.w - 1 - x
        }
    }

    temp.read(file5c, std::vector<int>{4}, Rectangle{2,2,3,2}, true, true);
    BOOST_CHECK_EQUAL(temp.width(),  3);
    BOOST_CHECK_EQUAL(temp.height(), 2);
    for (int y = 0; y < temp.height(); ++y) {
        for (int x = 0; x < temp.width(); ++x) {
            auto pixel = temp.at<uint8_t>(x, y);
            BOOST_CHECK_EQUAL(pixel, 10 * 5 * (2 + 2 - 1 - y) + (2 + 3) - 1 - x);
            //                            c *(r.y+r.h- 1 - y) + r.x+r.w - 1 - x
        }
    }
}


// test exceptions for read
BOOST_AUTO_TEST_CASE(read_write_exceptions)
{
    Image temp;
    std::string file3c = "../test_resources/images/formats/uint8x3.tif";
    BOOST_CHECK_THROW(temp.read("not-existing-file"), runtime_error);
    BOOST_CHECK_THROW(temp.read(file3c, std::vector<int>{3}), image_type_error);
    BOOST_CHECK_THROW(temp.read(file3c, {}, Rectangle{-1,0,1,1}), size_error);
    BOOST_CHECK_THROW(temp.read(file3c, {}, Rectangle{0,-1,1,1}), size_error);
    BOOST_CHECK_THROW(temp.read(file3c, {}, Rectangle{0,0,-1,1}), size_error);
    BOOST_CHECK_THROW(temp.read(file3c, {}, Rectangle{0,0,1,-1}), size_error);
    BOOST_CHECK_THROW(temp.read(file3c, {}, Rectangle{4,0,3,2}),  size_error);
    BOOST_CHECK_THROW(temp.read(file3c, {}, Rectangle{0,5,3,2}),  size_error);
}


// currently testing only PNG and JPG
BOOST_AUTO_TEST_CASE(read_write_image_format_drivers)
{
    Image test;

    // test 8-bit png
    test = Image{10, 20, Type::uint8x3};
    for (unsigned int x = 0; x < static_cast<unsigned int>(test.width()); ++x)
        for (unsigned int y = 0; y < static_cast<unsigned int>(test.height()); ++y)
            for (unsigned int c = 0; c < test.channels(); ++c)
                test.at<uint8_t>(x, y, c) = 10 * y + x + 20 * c;
    test.write("../test_resources/images/driver8.png");
    test = Image("../test_resources/images/driver8.png");
    BOOST_CHECK_EQUAL(test.size(), (Size{10, 20}));
    BOOST_CHECK_EQUAL(test.type(), Type::uint8x3);
    for (unsigned int x = 0; x < static_cast<unsigned int>(test.width()); ++x)
        for (unsigned int y = 0; y < static_cast<unsigned int>(test.height()); ++y)
            for (unsigned int c = 0; c < test.channels(); ++c)
                BOOST_CHECK_EQUAL(test.at<uint8_t>(x, y, c), 10 * y + x + 20 * c);


    // test 16-bit png
    test = Image{11, 18, Type::uint16x3};
    for (unsigned int x = 0; x < static_cast<unsigned int>(test.width()); ++x)
        for (unsigned int y = 0; y < static_cast<unsigned int>(test.height()); ++y)
            for (unsigned int c = 0; c < test.channels(); ++c)
                test.at<uint16_t>(x, y, c) = 2500 * y + 200 * x + 10000 * c;
    test.write("../test_resources/images/driver16.png");
    test = Image("../test_resources/images/driver16.png");
    BOOST_CHECK_EQUAL(test.size(), (Size{11, 18}));
    BOOST_CHECK_EQUAL(test.type(), Type::uint16x3);
    for (unsigned int x = 0; x < static_cast<unsigned int>(test.width()); ++x)
        for (unsigned int y = 0; y < static_cast<unsigned int>(test.height()); ++y)
            for (unsigned int c = 0; c < test.channels(); ++c)
                BOOST_CHECK_EQUAL(test.at<uint16_t>(x, y, c), 2500 * y + 200 * x + 10000 * c);


    // test jpg (values are not exactly matched)
    test = Image{9, 20, Type::uint8x3};
    for (unsigned int x = 0; x < static_cast<unsigned int>(test.width()); ++x)
        for (unsigned int y = 0; y < static_cast<unsigned int>(test.height()); ++y)
            for (unsigned int c = 0; c < test.channels(); ++c)
                test.at<uint8_t>(x, y, c) = 10 * y + x + 20 * c;
    test.write("../test_resources/images/driver.jpg");
    test = Image("../test_resources/images/driver.jpg");
    BOOST_CHECK_EQUAL(test.size(), (Size{9, 20}));
    BOOST_CHECK_EQUAL(test.type(), Type::uint8x3);
    for (unsigned int x = 0; x < static_cast<unsigned int>(test.width()); ++x)
        for (unsigned int y = 0; y < static_cast<unsigned int>(test.height()); ++y)
            for (unsigned int c = 0; c < test.channels(); ++c)
                BOOST_CHECK_LE(std::abs(static_cast<int>(test.at<uint8_t>(x, y, c)) - static_cast<int>(10 * y + x + 20 * c)), /* <= */ 3);

}


// test access on single-channel and multi-channel images
BOOST_AUTO_TEST_CASE(at_access)
{
    // test at for single channel image
    Image ic1{5, 6, Type::uint8x1};
    for (int x = 0; x < ic1.width(); ++x)
        for (int y = 0; y < ic1.height(); ++y)
            ic1.at<uint8_t>(x, y) = 10 * y + x;

    for (int x = 0; x < ic1.width(); ++x)
        for (int y = 0; y < ic1.height(); ++y)
            BOOST_CHECK_EQUAL(ic1.at<uint8_t>(x, y), 10 * y + x);

    for (int x = 0; x < ic1.width(); ++x)
        for (int y = 0; y < ic1.height(); ++y)
            ic1.at<uint8_t>(x, y, 0) = 20 * y - x;

    for (int x = 0; x < ic1.width(); ++x)
        for (int y = 0; y < ic1.height(); ++y)
            BOOST_CHECK_EQUAL(ic1.at<uint8_t>(x, y, 0), (uint8_t)(20 * y - x));

    // test at for multi channel image
    Image ic5{7, 8, Type::uint8x5};
    ConstImage cic5 = ic5.sharedCopy();

    for (int y = 0; y < ic5.height(); ++y) {
        for (int x = 0; x < ic5.width(); ++x) {
            ic5.at<std::array<uint8_t,5>>(x, y) = {
                static_cast<uint8_t>(10 * y + x),
                static_cast<uint8_t>(15 * y + x),
                static_cast<uint8_t>(20 * y + x),
                static_cast<uint8_t>(25 * y + x),
                static_cast<uint8_t>(30 * y + x)};
        }
    }

    for (int y = 0; y < ic5.height(); ++y) {
        for (int x = 0; x < ic5.width(); ++x) {
            auto pixel = ic5.at<std::array<uint8_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * y + x);
            BOOST_CHECK_EQUAL(pixel[1], 15 * y + x);
            BOOST_CHECK_EQUAL(pixel[2], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel[3], 25 * y + x);
            BOOST_CHECK_EQUAL(pixel[4], 30 * y + x);

            pixel = cic5.at<std::array<uint8_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * y + x);
            BOOST_CHECK_EQUAL(pixel[1], 15 * y + x);
            BOOST_CHECK_EQUAL(pixel[2], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel[3], 25 * y + x);
            BOOST_CHECK_EQUAL(pixel[4], 30 * y + x);
        }
    }

    for (int x = 0; x < ic5.width(); ++x) {
        for (int y = 0; y < ic5.height(); ++y) {
            ic5.at<uint8_t>(x, y, 1) = 35 * y + x;
        }
    }

    for (int x = 0; x < ic5.width(); ++x) {
        for (int y = 0; y < ic5.height(); ++y) {
            uint8_t value;
            value = ic5.at<uint8_t>(x, y, 0);
            BOOST_CHECK_EQUAL(value, 10 * y + x);

            value = ic5.at<uint8_t>(x, y, 1);
            BOOST_CHECK_EQUAL(value, 35 * y + x);

            value = ic5.at<uint8_t>(x, y, 2);
            BOOST_CHECK_EQUAL(value, 20 * y + x);

            value = ic5.at<uint8_t>(x, y, 3);
            BOOST_CHECK_EQUAL(value, 25 * y + x);

            value = ic5.at<uint8_t>(x, y, 4);
            BOOST_CHECK_EQUAL(value, 30 * y + x);

            value = cic5.at<uint8_t>(x, y, 0);
            BOOST_CHECK_EQUAL(value, 10 * y + x);

            value = cic5.at<uint8_t>(x, y, 1);
            BOOST_CHECK_EQUAL(value, 35 * y + x);

            value = cic5.at<uint8_t>(x, y, 2);
            BOOST_CHECK_EQUAL(value, 20 * y + x);

            value = cic5.at<uint8_t>(x, y, 3);
            BOOST_CHECK_EQUAL(value, 25 * y + x);

            value = cic5.at<uint8_t>(x, y, 4);
            BOOST_CHECK_EQUAL(value, 30 * y + x);
        }
    }

    for (int x = 0; x < ic5.width(); ++x) {
        for (int y = 0; y < ic5.height(); ++y) {
            auto pixel = ic5.at<std::array<uint8_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * y + x);
            BOOST_CHECK_EQUAL(pixel[1], 35 * y + x);
            BOOST_CHECK_EQUAL(pixel[2], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel[3], 25 * y + x);
            BOOST_CHECK_EQUAL(pixel[4], 30 * y + x);
        }
    }
}


// test split and merge
BOOST_AUTO_TEST_CASE(split_merge)
{
    Image i1{5, 6, Type::uint8x1};
    Image i2{5, 6, Type::uint8x2};
    Image i3{5, 6, Type::uint8x3};
    Image i5{5, 6, Type::uint8x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            i1.at<std::array<uint8_t,1>>(x, y) = {(uint8_t)(10 * y + x)};
            i2.at<std::array<uint8_t,2>>(x, y) = {(uint8_t)(20 * y + x),
                                                  (uint8_t)(25 * y + x)};
            i3.at<std::array<uint8_t,3>>(x, y) = {(uint8_t)(30 * y + x),
                                                  (uint8_t)(33 * y + x),
                                                  (uint8_t)(37 * y + x)};
            i5.at<std::array<uint8_t,5>>(x, y) = {(uint8_t)(50 * y + x), // this overflows, but we compare also to overflowed value
                                                  (uint8_t)(52 * y + x),
                                                  (uint8_t)(54 * y + x),
                                                  (uint8_t)(56 * y + x),
                                                  (uint8_t)(58 * y + x)};
        }
    }

    Image merged;
    merged.merge({i1.constSharedCopy(), i2.constSharedCopy(), i3.constSharedCopy(), i5.constSharedCopy()});
    BOOST_CHECK_EQUAL(merged.channels(), 11);
    for (int y = 0; y < merged.height(); ++y) {
        for (int x = 0; x < merged.width(); ++x) {
            BOOST_CHECK((merged.at<std::array<uint8_t,11>>(x, y) == std::array<uint8_t,11>{
                (uint8_t)(10 * y + x),
                (uint8_t)(20 * y + x),
                (uint8_t)(25 * y + x),
                (uint8_t)(30 * y + x),
                (uint8_t)(33 * y + x),
                (uint8_t)(37 * y + x),
                (uint8_t)(50 * y + x),
                (uint8_t)(52 * y + x),
                (uint8_t)(54 * y + x),
                (uint8_t)(56 * y + x),
                (uint8_t)(58 * y + x)
            }));
        }
    }

    std::vector<Image> singleChannelImages = i5.split();
    BOOST_CHECK_EQUAL(singleChannelImages.size(), 5);
    for (int y = 0; y < merged.height(); ++y) {
        for (int x = 0; x < merged.width(); ++x) {
            BOOST_CHECK_EQUAL((singleChannelImages.at(0).at<uint8_t>(x, y)), (uint8_t)(50 * y + x));
            BOOST_CHECK_EQUAL((singleChannelImages.at(1).at<uint8_t>(x, y)), (uint8_t)(52 * y + x));
            BOOST_CHECK_EQUAL((singleChannelImages.at(2).at<uint8_t>(x, y)), (uint8_t)(54 * y + x));
            BOOST_CHECK_EQUAL((singleChannelImages.at(3).at<uint8_t>(x, y)), (uint8_t)(56 * y + x));
            BOOST_CHECK_EQUAL((singleChannelImages.at(4).at<uint8_t>(x, y)), (uint8_t)(58 * y + x));
        }
    }

    singleChannelImages = i3.split({2, 0});
    BOOST_CHECK_EQUAL(singleChannelImages.size(), 2);
    for (int y = 0; y < merged.height(); ++y) {
        for (int x = 0; x < merged.width(); ++x) {
            BOOST_CHECK_EQUAL((singleChannelImages.at(0).at<uint8_t>(x, y)), (uint8_t)(37 * y + x)); // channel 2
            BOOST_CHECK_EQUAL((singleChannelImages.at(1).at<uint8_t>(x, y)), (uint8_t)(30 * y + x)); // channel 0
        }
    }

    BOOST_CHECK_THROW(i3.split({3}), image_type_error);
}

// test operations on single-channel and multi-channel images
BOOST_AUTO_TEST_CASE(set_single_channel)
{
    Image i1{5, 6, Type::uint8x1};
    Image i2{5, 6, Type::uint8x1};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            i1.at<uint8_t>(x, y) = 10 * y + x;
            i2.at<uint8_t>(x, y) = 10 * x + y;
        }
    }

    Image mask{5, 6, Type::uint8x1};
    mask.set(0);
    mask.setBoolAt(1,1, 0, true);
    mask.setBoolAt(2,1, 0, true);
    mask.setBoolAt(1,2, 0, true);
    mask.setBoolAt(2,2, 0, true);

    // set i2 for (x,y) in [1,2]x[1,2] to 5
    i2.set(5, mask);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            if ((x == 1 || x == 2) && (y == 1 || y == 2))
                BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 5);
            else
                BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 10 * x + y);

    // set i2 for (x,y) in [1,2]x[1,2] or (x,y) = (3,4) to i1
    mask.setBoolAt(3,4, 0, true);
    i2.copyValuesFrom(i1, mask);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            if (((x == 1 || x == 2) && (y == 1 || y == 2)) || (x == 3 && y == 4))
                BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 10 * y + x);
            else
                BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 10 * x + y);

    // set i2 to 5
    i2.set(5);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 5);

    // set a cropped image to 6
    i2.crop(Rectangle{1,1,2,3});
    i2.set(6);
    i2.uncrop();
    BOOST_CHECK_EQUAL(i2.width(),  5);
    BOOST_CHECK_EQUAL(i2.height(), 6);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            if ((x == 1 || x == 2) && (y == 1 || y == 2 || y == 3))
                BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 6);
            else
                BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 5);


    // set i2 to i1
    i2.copyValuesFrom(i1);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            BOOST_CHECK_EQUAL(i2.at<uint8_t>(x, y), 10 * y + x);
}

BOOST_AUTO_TEST_CASE(set_multi_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image i1{5, 6, Type::uint16x5};
    Image i2{5, 6, Type::uint16x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<uint16_t>(x, y, c) = 10 * y + x + offsets[c];
                i2.at<uint16_t>(x, y, c) = 10 * x + y + offsets[c];
            }
        }
    }

    Image mask_single{5, 6, Type::uint8x1};
    mask_single.set(0);
    mask_single.setBoolAt(1,1, 0, true);
    mask_single.setBoolAt(2,1, 0, true);
    mask_single.setBoolAt(1,2, 0, true);
    mask_single.setBoolAt(2,2, 0, true);

    // set i2 for (x,y) in [1,2]x[1,2] to 5
    i2.set(5, mask_single);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                if ((x == 1 || x == 2) && (y == 1 || y == 2))
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 5);
                else
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * x + y + offsets[c]);

    // set i2 for (x,y) in [1,2]x[1,2] or (x,y) = (3,4) to i1
    mask_single.setBoolAt(3,4, 0, true);
    i2.copyValuesFrom(i1, mask_single);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                if (((x == 1 || x == 2) && (y == 1 || y == 2)) || (x == 3 && y == 4))
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * y + x + offsets[c]);
                else
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * x + y + offsets[c]);

    // set i2 to 5
    i2.set(5);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 5);

    // set i2 to i1
    i2.copyValuesFrom(i1);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * y + x + offsets[c]);


    Image mask_multi{5, 6, Type::uint8x5};
    mask_multi.set(0);
    mask_multi.setBoolAt(1,1,0, true);
    mask_multi.setBoolAt(2,1,0, true);
    mask_multi.setBoolAt(1,2,0, true);
    mask_multi.setBoolAt(2,2,0, true);
    mask_multi.setBoolAt(3,4,1, true);
    mask_multi.setBoolAt(0,1,2, true);
    mask_multi.setBoolAt(0,2,2, true);
    mask_multi.setBoolAt(1,2,2, true);
    mask_multi.setBoolAt(0,0,4, true);

    for (int y = 0; y < i1.height(); ++y)
        for (int x = 0; x < i1.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                i2.at<uint16_t>(x, y, c) = 10 * x + y + offsets[c];


    // set i2 for (y,x) in mask_multi to 5
    i2.set(5, mask_multi);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                if (mask_multi.boolAt(x, y, c))
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 5);
                else
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * x + y + offsets[c]);

    // set i2 for mask_multi to i1
    i2.copyValuesFrom(i1, mask_multi);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                if (mask_multi.boolAt(x, y, c))
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * y + x + offsets[c]);
                else
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * x + y + offsets[c]);
}


BOOST_AUTO_TEST_CASE(multi_set_multi_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image i1{5, 6, Type::uint16x5};
    Image i2{5, 6, Type::uint16x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<uint16_t>(x, y, c) = 10 * y + x + offsets[c];
                i2.at<uint16_t>(x, y, c) = 10 * x + y + offsets[c];
            }
        }
    }

    Image mask_single{5, 6, Type::uint8x1};
    mask_single.set(0);
    mask_single.setBoolAt(1,1, 0, true);
    mask_single.setBoolAt(2,1, 0, true);
    mask_single.setBoolAt(1,2, 0, true);
    mask_single.setBoolAt(2,2, 0, true);

    // set i2 for (x,y) in [1,2]x[1,2] to (5, 6, 7, 8, 9)
    std::vector<double> vals{5, 6, 7, 8, 9};
    i2.set(vals, mask_single);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                if ((x == 1 || x == 2) && (y == 1 || y == 2))
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), vals.at(c));
                else
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * x + y + offsets[c]);

    // set i2 to (5, 6, 7, 8, 9)
    i2.set(vals);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), vals.at(c));


    Image mask_multi{5, 6, Type::uint8x5};
    mask_multi.set(0);
    mask_multi.setBoolAt(1,1,0, true);
    mask_multi.setBoolAt(2,1,0, true);
    mask_multi.setBoolAt(1,2,0, true);
    mask_multi.setBoolAt(2,2,0, true);
    mask_multi.setBoolAt(3,4,1, true);
    mask_multi.setBoolAt(0,1,2, true);
    mask_multi.setBoolAt(0,2,2, true);
    mask_multi.setBoolAt(1,2,2, true);
    mask_multi.setBoolAt(0,0,4, true);

    for (int y = 0; y < i1.height(); ++y)
        for (int x = 0; x < i1.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                i2.at<uint16_t>(x, y, c) = 10 * x + y + offsets[c];


    // set i2 for (y,x) in mask_multi to (5, 6, 7, 8, 9)
    i2.set(vals, mask_multi);
    for (int y = 0; y < i2.height(); ++y)
        for (int x = 0; x < i2.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                if (mask_multi.boolAt(x, y, c))
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), vals.at(c));
                else
                    BOOST_CHECK_EQUAL(i2.at<uint16_t>(x, y, c), 10 * x + y + offsets[c]);
}


BOOST_AUTO_TEST_CASE(abs_single_channel)
{
    Image i1s{5, 6, Type::int32x1};
    Image i2s{5, 6, Type::int32x1};
    Image i1u{5, 6, Type::uint8x1};
    Image i2u{5, 6, Type::uint8x1};
    for (int y = 0; y < i1s.height(); ++y) {
        for (int x = 0; x < i1s.width(); ++x) {
            i1s.at<int32_t>(x, y) = -2 + y;
            i2s.at<int32_t>(x, y) = -3 + x;
            i1u.at<uint8_t>(x, y) = y;
            i2u.at<uint8_t>(x, y) = x;
        }
    }

    // copy abs
    Image i1sClone = i1s.abs();
    Image i1uClone = i1u.abs();
    for (int y = 0; y < i1sClone.height(); ++y) {
        for (int x = 0; x < i1sClone.width(); ++x) {
            BOOST_CHECK_EQUAL(i1sClone.at<int32_t>(x, y), std::abs(-2 + y));
            BOOST_CHECK_EQUAL(i1uClone.at<uint8_t>(x, y), std::abs(y));
        }
    }

    // move self
    Image clone = i2s.clone();
    ConstImage shared = clone.sharedCopy();
    clone = std::move(clone).abs();
    for (int y = 0; y < clone.height(); ++y)
        for (int x = 0; x < clone.width(); ++x)
            BOOST_CHECK_EQUAL(clone.at<int32_t>(x, y), std::abs(-3 + x));
    // check that memory location is the original one from clone
    BOOST_CHECK(shared.isSharedWith(clone));

    clone = i2u.clone();
    shared = clone.sharedCopy();
    clone = std::move(clone).abs();
    for (int y = 0; y < clone.height(); ++y)
        for (int x = 0; x < clone.width(); ++x)
            BOOST_CHECK_EQUAL(clone.at<uint8_t>(x, y), std::abs(x));
    // check that memory location is the original one from clone
    BOOST_CHECK(shared.isSharedWith(clone));

    // test absdiff
    Image absdiff = i1u.absdiff(i2u); // max value: 255
    Image absdiff_manual = i1u.subtract(i2u, Type::int8x1).abs(); // max value: 127, abs cannot change type
    for (int y = 0; y < absdiff.height(); ++y) {
        for (int x = 0; x < absdiff.width(); ++x) {
            BOOST_CHECK_EQUAL(absdiff.at<uint8_t>(x, y),       std::abs(y - x));
            BOOST_CHECK_EQUAL(absdiff_manual.at<int8_t>(x, y), std::abs(y - x));
        }
    }

    // check |-128| = 127 for int8
    Image i3s{1, 1, Type::int8x1};
    i3s.at<int8_t>(0, 0) = -128;
    auto i3sabs = i3s.abs();
    BOOST_CHECK_EQUAL(i3sabs.at<int8_t>(0, 0), 127);
}

BOOST_AUTO_TEST_CASE(abs_three_channel)
{
    constexpr std::array<int,3> offsets = {10, 20, 30};

    Image i1{5, 6, Type::float64x3};
    Image i2{5, 6, Type::float64x3};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<double>(x, y, c) = y + offsets[c] / 10;
                i2.at<double>(x, y, c) = x - offsets[c];
            }
        }
    }

    // copy abs
    Image i2abs = i2.abs();
    for (int y = 0; y < i2abs.height(); ++y)
        for (int x = 0; x < i2abs.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i2abs.at<double>(x, y, c), std::abs(x - offsets[c]));

    // move self
    Image clone = i2.clone();
    ConstImage shared = clone.sharedCopy();
    clone = std::move(clone).abs();
    for (int y = 0; y < clone.height(); ++y)
        for (int x = 0; x < clone.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(clone.at<double>(x, y, c), std::abs(x - offsets[c]));
    // check that memory location is the original one from clone
    BOOST_CHECK(shared.isSharedWith(clone));

    // test absdiff
    Image absdiff = i2.absdiff(i1);
    Image absdiff_manual = i2.subtract(i1).abs();
    for (int y = 0; y < absdiff.height(); ++y) {
        for (int x = 0; x < absdiff.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                BOOST_CHECK_EQUAL(absdiff.at<double>(x, y, c),        std::abs(y - x + offsets[c]/10 + offsets[c]));
                BOOST_CHECK_EQUAL(absdiff_manual.at<double>(x, y, c), std::abs(y - x + offsets[c]/10 + offsets[c]));
            }
        }
    }

    // check |-128| = 127 for int8
    Image i3s{1, 1, Type::int8x3};
    i3s.set(-128);
    auto i3sabs = i3s.abs();
    for (unsigned int i = 0; i < i3sabs.channels(); ++i)
        BOOST_CHECK_EQUAL(i3sabs.at<int8_t>(0, 0, i), 127);
}

BOOST_AUTO_TEST_CASE(abs_five_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image i1{5, 6, Type::float64x5};
    Image i2{5, 6, Type::float64x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<double>(x, y, c) = y + offsets[c] / 10;
                i2.at<double>(x, y, c) = x - offsets[c];
            }
        }
    }

    // copy abs
    Image i2abs = i2.abs();
    for (int y = 0; y < i2abs.height(); ++y)
        for (int x = 0; x < i2abs.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i2abs.at<double>(x, y, c), std::abs(x - offsets[c]));

    // move self
    Image clone = i2.clone();
    ConstImage shared = clone.sharedCopy();
    clone = std::move(clone).abs();
    for (int y = 0; y < clone.height(); ++y)
        for (int x = 0; x < clone.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(clone.at<double>(x, y, c), std::abs(x - offsets[c]));
    // check that memory location is the original one from clone
    BOOST_CHECK(shared.isSharedWith(clone));

    // test absdiff
    Image absdiff = i2.absdiff(i1);
    Image absdiff_manual = i2.subtract(i1).abs();
    for (int y = 0; y < absdiff.height(); ++y) {
        for (int x = 0; x < absdiff.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                BOOST_CHECK_EQUAL(absdiff.at<double>(x, y, c),        std::abs(y - x + offsets[c]/10 + offsets[c]));
                BOOST_CHECK_EQUAL(absdiff_manual.at<double>(x, y, c), std::abs(y - x + offsets[c]/10 + offsets[c]));
            }
        }
    }

    // check |-128| = 127 for int8
    Image i3s{1, 1, Type::int8x5};
    i3s.set(-128);
    auto i3sabs = i3s.abs();
    for (unsigned int i = 0; i < i3sabs.channels(); ++i)
        BOOST_CHECK_EQUAL(i3sabs.at<int8_t>(0, 0, i), 127);
}

BOOST_AUTO_TEST_CASE(add_single_channel)
{
    Image clone;
    Image i1{5, 6, Type::uint8x1};
    Image i2{5, 6, Type::uint8x1};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            i1.at<uint8_t>(x, y) = 10 * y + x;
            i2.at<uint8_t>(x, y) = 10 * x + y;
        }
    }

    // copy add
    Image i_sum = i1.add(i2);
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            BOOST_CHECK_EQUAL(i_sum.at<uint8_t>(x, y), 11 * (y + x));

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous add was done with a copy indeed.
    Image i_sum16 = i1.add(i2, Type::uint16x1);
    for (int y = 0; y < i_sum16.height(); ++y)
        for (int x = 0; x < i_sum16.width(); ++x)
            BOOST_CHECK_EQUAL(i_sum16.at<uint16_t>(x, y), 11 * (y + x));

    // move self
    ConstImage shared = i_sum.sharedCopy();
    i_sum = std::move(i_sum).add(i2);
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            BOOST_CHECK_EQUAL(i_sum.at<uint8_t>(x, y), 21 * x + 12 * y);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_sum));

    // move other
    clone = i1.clone();
    shared = clone.sharedCopy();
    i_sum = i_sum.add(std::move(clone));
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            BOOST_CHECK_EQUAL(i_sum.at<uint8_t>(x, y), 22 * (y + x));
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_sum));

    // add only a cropped region
    i_sum.crop(Rectangle{1,1,2,3});
    i1.crop(Rectangle{1,1,2,3});
    i2.crop(Rectangle{1,1,2,3});
    i_sum = std::move(i_sum).add(i2).add(i1);
    i_sum.uncrop();
    BOOST_CHECK_EQUAL(i_sum.width(),  5);
    BOOST_CHECK_EQUAL(i_sum.height(), 6);
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            if ((x == 1 || x == 2) && (y == 1 || y == 2 || y == 3))
                BOOST_CHECK_EQUAL(i_sum.at<uint8_t>(x, y), 33 * (y + x));
            else
                BOOST_CHECK_EQUAL(i_sum.at<uint8_t>(x, y), 22 * (y + x));
}

BOOST_AUTO_TEST_CASE(add_multi_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image clone;
    Image i1{5, 6, Type::float32x5};
    Image i2{5, 6, Type::float32x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<float>(x, y, c) = 10 * y + x + offsets[c];
                i2.at<float>(x, y, c) = 10 * x + y + offsets[c];
            }
        }
    }

    // copy add
    Image i_sum = i1.add(i2);
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_sum.at<float>(x, y, c), 11 * (y + x) + 2 * offsets[c]);

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous add was done with a copy indeed.
    Image i_sum16 = i1.add(i2, Type::uint16x1);
    for (int y = 0; y < i_sum16.height(); ++y)
        for (int x = 0; x < i_sum16.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_sum16.at<uint16_t>(x, y, c), 11 * (y + x) + 2 * offsets[c]);

    // move self
    ConstImage shared = i_sum.sharedCopy();
    i_sum = std::move(i_sum).add(i2);
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_sum.at<float>(x, y, c), 21 * x + 12 * y + 3 * offsets[c]);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_sum));

    // move other
    clone = i1.clone();
    shared = clone.sharedCopy();
    i_sum = i_sum.add(std::move(clone));
    for (int y = 0; y < i_sum.height(); ++y)
        for (int x = 0; x < i_sum.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_sum.at<float>(x, y, c), 22 * (y + x) + 4 * offsets[c]);
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_sum));
}

BOOST_AUTO_TEST_CASE(subtract_single_channel)
{
    Image clone;
    Image i1{5, 6, Type::int8x1};
    Image i2{5, 6, Type::int8x1};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            i1.at<int8_t>(x, y) = 10 * y + x;
            i2.at<int8_t>(x, y) = 10 * x + y;
        }
    }

    // copy subtract
    Image i_diff = i1.subtract(i2);
    for (int y = 0; y < i_diff.height(); ++y)
        for (int x = 0; x < i_diff.width(); ++x)
            BOOST_CHECK_EQUAL(i_diff.at<int8_t>(x, y), 9 * y - 9 * x);

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous subtract was done with a copy indeed.
    Image i_diff16 = i1.subtract(i2, Type::int16x1);
    for (int y = 0; y < i_diff16.height(); ++y)
        for (int x = 0; x < i_diff16.width(); ++x)
            BOOST_CHECK_EQUAL(i_diff16.at<int16_t>(x, y), 9 * y - 9 * x);

    // move self
    ConstImage shared = i_diff.sharedCopy();
    i_diff = std::move(i_diff).subtract(i1);
    for (int y = 0; y < i_diff.height(); ++y)
        for (int x = 0; x < i_diff.width(); ++x)
            BOOST_CHECK_EQUAL(i_diff.at<int8_t>(x, y), -10 * x - y);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_diff));

    // move other
    clone = i1.clone();
    shared = clone.sharedCopy();
    i_diff = i_diff.subtract(std::move(clone));
    for (int y = 0; y < i_diff.height(); ++y)
        for (int x = 0; x < i_diff.width(); ++x)
            BOOST_CHECK_EQUAL(i_diff.at<int8_t>(x, y), -11 * (y + x));
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_diff));
}

BOOST_AUTO_TEST_CASE(subtract_multi_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image clone;
    Image i1{5, 6, Type::float32x5};
    Image i2{5, 6, Type::float32x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<float>(x, y, c) = 10 * y + x + offsets[c];
                i2.at<float>(x, y, c) = 10 * x + y - offsets[c];
            }
        }
    }

    // copy subtract
    Image i_diff = i1.subtract(i2);
    for (int y = 0; y < i_diff.height(); ++y)
        for (int x = 0; x < i_diff.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_diff.at<float>(x, y, c), 9 * y - 9 * x + 2 * offsets[c]);

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous subtract was done with a copy indeed.
    Image i_diff16 = i1.subtract(i2, Type::int16x1);
    for (int y = 0; y < i_diff16.height(); ++y)
        for (int x = 0; x < i_diff16.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_diff16.at<int16_t>(x, y, c), 9 * y - 9 * x + 2 * offsets[c]);

    // move self
    ConstImage shared = i_diff.sharedCopy();
    i_diff = std::move(i_diff).subtract(i1);
    for (int y = 0; y < i_diff.height(); ++y)
        for (int x = 0; x < i_diff.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_diff.at<float>(x, y, c), -10 * x - y + offsets[c]);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_diff));

    // move other
    clone = i1.clone();
    shared = clone.sharedCopy();
    i_diff = i_diff.subtract(std::move(clone));
    for (int y = 0; y < i_diff.height(); ++y)
        for (int x = 0; x < i_diff.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_diff.at<float>(x, y, c), -11 * (y + x));
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_diff));
}

BOOST_AUTO_TEST_CASE(multiply_single_channel)
{
    Image clone;
    Image i1{5, 6, Type::int32x1};
    Image i2{5, 6, Type::int32x1};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            i1.at<int32_t>(x, y) = y;
            i2.at<int32_t>(x, y) = x;
        }
    }

    // copy multiply
    Image i_prod = i1.multiply(i2);
    for (int y = 0; y < i_prod.height(); ++y)
        for (int x = 0; x < i_prod.width(); ++x)
            BOOST_CHECK_EQUAL(i_prod.at<int32_t>(x, y), y * x);

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous multiply was done with a copy indeed.
    Image i_prod16 = i1.multiply(i2, Type::int16x1);
    for (int y = 0; y < i_prod16.height(); ++y)
        for (int x = 0; x < i_prod16.width(); ++x)
            BOOST_CHECK_EQUAL(i_prod16.at<int16_t>(x, y), y * x);

    // move self
    ConstImage shared = i_prod.sharedCopy();
    i_prod = std::move(i_prod).multiply(i1);
    for (int y = 0; y < i_prod.height(); ++y)
        for (int x = 0; x < i_prod.width(); ++x)
            BOOST_CHECK_EQUAL(i_prod.at<int32_t>(x, y), x * y*y);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_prod));

    // move other
    clone = i2.clone();
    shared = clone.sharedCopy();
    i_prod = i_prod.multiply(std::move(clone));
    for (int y = 0; y < i_prod.height(); ++y)
        for (int x = 0; x < i_prod.width(); ++x)
            BOOST_CHECK_EQUAL(i_prod.at<int32_t>(x, y), x*x * y*y);
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_prod));
}

BOOST_AUTO_TEST_CASE(multiply_multi_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image clone;
    Image i1{5, 6, Type::float64x5};
    Image i2{5, 6, Type::float64x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<double>(x, y, c) = y + offsets[c];
                i2.at<double>(x, y, c) = x - offsets[c];
            }
        }
    }

    // copy multiply
    Image i_prod = i1.multiply(i2);
    for (int y = 0; y < i_prod.height(); ++y)
        for (int x = 0; x < i_prod.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_prod.at<double>(x, y, c), x * y + (x - y) * offsets[c] - offsets[c]*offsets[c]);

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous multiply was done with a copy indeed.
    Image i_prod16 = i1.multiply(i2, Type::int16x1);
    for (int y = 0; y < i_prod16.height(); ++y)
        for (int x = 0; x < i_prod16.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_prod16.at<int16_t>(x, y, c), x * y + (x - y) * offsets[c] - offsets[c]*offsets[c]);

    // move self
    ConstImage shared = i_prod.sharedCopy();
    i_prod = std::move(i_prod).multiply(i1);
    for (int y = 0; y < i_prod.height(); ++y)
        for (int x = 0; x < i_prod.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_prod.at<double>(x, y, c), std::pow(y + offsets[c], 2) * (x - offsets[c]));
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_prod));

    // move other
    clone = i1.clone();
    shared = clone.sharedCopy();
    i_prod = i_prod.multiply(std::move(clone));
    for (int y = 0; y < i_prod.height(); ++y)
        for (int x = 0; x < i_prod.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_prod.at<double>(x, y, c), std::pow(y + offsets[c], 3) * (x - offsets[c]));
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_prod));
}

BOOST_AUTO_TEST_CASE(divide_single_channel)
{
    Image clone;
    Image i1_int{5, 6, Type::int32x1};
    Image i2_int{5, 6, Type::int32x1};
    Image i1_float{5, 6, Type::float32x1};
    Image i2_float{5, 6, Type::float32x1};
    for (int y = 0; y < i1_int.height(); ++y) {
        for (int x = 0; x < i1_int.width(); ++x) {
            i1_int.at<int32_t>(x, y) = y;
            i2_int.at<int32_t>(x, y) = x;
            i1_float.at<float>(x, y) = y;
            i2_float.at<float>(x, y) = x;
        }
    }

#ifdef __builtin_flt_rounds
#if FLT_ROUNDS != 1 /* std::float_round_style::round_to_nearest */
    #pragma STDC FENV_ACCESS ON
    std::fesetround(FE_TONEAREST);
    #pragma STDC FENV_ACCESS OFF
#endif /*FLT_ROUNDS */
#else
    std::fesetround(FE_TONEAREST);
#endif

    // copy divide
    Image int_quot = i1_int.divide(i2_int);
    Image float_quot = i1_float.divide(i2_float);
    for (int y = 0; y < int_quot.height(); ++y) {
        for (int x = 0; x < int_quot.width(); ++x) {
            BOOST_CHECK_EQUAL(int_quot.at<int32_t>(x, y), x != 0 ? nearbyint((float)y / x) : 0); // note special arithmetics
            BOOST_CHECK_CLOSE_FRACTION(float_quot.at<float>(x, y), x != 0 ? (float)y / x : 0, 1e-15);
        }
    }

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous divide was done with a copy indeed.
    Image int_to_float_quot = i1_int.divide(i2_int, Type::float32x1);
    Image float_to_int_quot = i1_float.divide(i2_float, Type::int16x1);
    for (int y = 0; y < int_to_float_quot.height(); ++y) {
        for (int x = 0; x < int_to_float_quot.width(); ++x) {
            BOOST_CHECK_CLOSE_FRACTION(int_to_float_quot.at<float>(x, y), x != 0 ? (float)y / x : 0, 1e-15); // note operands are integer, result is float with correct value!
            BOOST_CHECK_EQUAL(float_to_int_quot.at<int16_t>(x, y), x != 0 ? nearbyint((float)y / x) : 0);
        }
    }

    // move self
    ConstImage shared = int_quot.sharedCopy();
    int_quot = std::move(int_quot).divide(i1_int);
    for (int y = 0; y < int_quot.height(); ++y)
        for (int x = 0; x < int_quot.width(); ++x)
            BOOST_CHECK_EQUAL(int_quot.at<int32_t>(x, y), x != 0 && y != 0 ? nearbyint((float)(nearbyint((float)y / x)) / y) : 0);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(int_quot));

    // move other
    clone = i2_int.clone();
    shared = clone.sharedCopy();
    int_quot = int_quot.divide(std::move(clone));
    for (int y = 0; y < int_quot.height(); ++y)
        for (int x = 0; x < int_quot.width(); ++x)
            BOOST_CHECK_EQUAL(int_quot.at<int32_t>(x, y), x != 0 && y != 0 ? nearbyint((float)(nearbyint((float)(nearbyint((float)y / x)) / y)) / x) : 0);
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(int_quot));
}

BOOST_AUTO_TEST_CASE(divide_multi_channel)
{
    constexpr std::array<int,5> offsets = {10, 20, 30, 40, 50};

    Image clone;
    Image i1{5, 6, Type::float64x5};
    Image i2{5, 6, Type::float64x5};
    for (int y = 0; y < i1.height(); ++y) {
        for (int x = 0; x < i1.width(); ++x) {
            for (unsigned int c = 0; c < offsets.size(); ++c) {
                i1.at<double>(x, y, c) = y + offsets[c];
                i2.at<double>(x, y, c) = x - offsets[c];
                // may not be 0, since then the divisor had to be checked for 0 in the results, since here x/0 := 0
                BOOST_CHECK_NE(i1.at<double>(x, y, c), 0);
                BOOST_CHECK_NE(i2.at<double>(x, y, c), 0);
            }
        }
    }

    // copy divide
    Image i_quot = i1.divide(i2);
    for (int y = 0; y < i_quot.height(); ++y)
        for (int x = 0; x < i_quot.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_CLOSE_FRACTION(i_quot.at<double>(x, y, c), (double)(y + offsets[c]) / (x - offsets[c]), 1e-15);

    // different type (always copy)
    // using variables i1 and i2 again, proves that previous divide was done with a copy indeed.
    Image i_quot16 = i1.divide(i2, Type::int16x1);
    for (int y = 0; y < i_quot16.height(); ++y)
        for (int x = 0; x < i_quot16.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_EQUAL(i_quot16.at<int16_t>(x, y, c), nearbyint((double)(y + offsets[c]) / (x - offsets[c])));

    // move self
    ConstImage shared = i_quot.sharedCopy();
    i_quot = std::move(i_quot).divide(i1);
    for (int y = 0; y < i_quot.height(); ++y)
        for (int x = 0; x < i_quot.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_CLOSE_FRACTION(i_quot.at<double>(x, y, c), 1. / (x - offsets[c]), 1e-15);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_quot));

    // move other
    clone = i1.clone();
    shared = clone.sharedCopy();
    i_quot = i_quot.divide(std::move(clone));
    for (int y = 0; y < i_quot.height(); ++y)
        for (int x = 0; x < i_quot.width(); ++x)
            for (unsigned int c = 0; c < offsets.size(); ++c)
                BOOST_CHECK_CLOSE_FRACTION(i_quot.at<double>(x, y, c), 1. / (y + offsets[c]) / (x - offsets[c]), 1e-15);
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_quot));
}

// test mean method and meanStdDev method with masks
/*
 * Octave test code:
 * A = magic(4);
 * B = (1:4)'; B = [B B B B];
 * si = [2 3 4 6 7 8 10 11 12 15];
 * mi1 = [1:5 8 12:16];
 * mi2 = [1:3 5:7 9:11 13:15]
 * mean(A(:))
 * mean(B(:))
 * mean(A(si))
 * mean(B(si))
 * mean(A(mi1))
 * mean(A(mi2))
 * mean(B(mi1))
 * mean(B(mi2))
 * std(A(:), 1)
 * std(B(:), 1)
 * std(A(:))
 * std(B(:))
 * std(A(si), 1)
 * std(B(si), 1)
 * std(A(si))
 * std(B(si))
 * std(A(mi1), 1)
 * std(B(mi2), 1)
 * std(A(mi1))
 * std(B(mi2))
 */
BOOST_AUTO_TEST_CASE(meanStdDev_2chans)
{
    /*    channel 1           channel 2
     * img:
     * magic(4) matrix:
     * 16   2   3  13       1   1   1   1
     *  5  11  10   8       2   2   2   2
     *  9   7   6  12       3   3   3   3
     *  4  14  15   1       4   4   4   4
     *
     * single-channel mask:
     *  0   0   0   0
     *  1   0   1   1
     *  1   1   1   1
     *  1   1   1   0
     *
     * multi-channel mask:
     *  1   1   0   1       1   1   1   1
     *  1   0   0   1       1   1   1   1
     *  1   0   0   1       1   1   1   1
     *  1   1   1   1       0   0   0   0
     */

    Image i{4, 4, Type::uint8x2};
    i.at<uint8_t>(0, 0, 0) = 16; i.at<uint8_t>(1, 0, 0) = 2;  i.at<uint8_t>(2, 0, 0) = 3;  i.at<uint8_t>(3, 0, 0) = 13;
    i.at<uint8_t>(0, 1, 0) = 5;  i.at<uint8_t>(1, 1, 0) = 11; i.at<uint8_t>(2, 1, 0) = 10; i.at<uint8_t>(3, 1, 0) = 8;
    i.at<uint8_t>(0, 2, 0) = 9;  i.at<uint8_t>(1, 2, 0) = 7;  i.at<uint8_t>(2, 2, 0) = 6;  i.at<uint8_t>(3, 2, 0) = 12;
    i.at<uint8_t>(0, 3, 0) = 4;  i.at<uint8_t>(1, 3, 0) = 14; i.at<uint8_t>(2, 3, 0) = 15; i.at<uint8_t>(3, 3, 0) = 1;

    i.at<uint8_t>(0, 0, 1) = 1;  i.at<uint8_t>(1, 0, 1) = 1;  i.at<uint8_t>(2, 0, 1) = 1;  i.at<uint8_t>(3, 0, 1) = 1;
    i.at<uint8_t>(0, 1, 1) = 2;  i.at<uint8_t>(1, 1, 1) = 2;  i.at<uint8_t>(2, 1, 1) = 2;  i.at<uint8_t>(3, 1, 1) = 2;
    i.at<uint8_t>(0, 2, 1) = 3;  i.at<uint8_t>(1, 2, 1) = 3;  i.at<uint8_t>(2, 2, 1) = 3;  i.at<uint8_t>(3, 2, 1) = 3;
    i.at<uint8_t>(0, 3, 1) = 4;  i.at<uint8_t>(1, 3, 1) = 4;  i.at<uint8_t>(2, 3, 1) = 4;  i.at<uint8_t>(3, 3, 1) = 4;

    Image s{4, 4, Type::uint8x1};
    s.setBoolAt(0, 0, 0, false); s.setBoolAt(1, 0, 0, false); s.setBoolAt(2, 0, 0, false); s.setBoolAt(3, 0, 0, false);
    s.setBoolAt(0, 1, 0, true);  s.setBoolAt(1, 1, 0, false); s.setBoolAt(2, 1, 0, true);  s.setBoolAt(3, 1, 0, true);
    s.setBoolAt(0, 2, 0, true);  s.setBoolAt(1, 2, 0, true);  s.setBoolAt(2, 2, 0, true);  s.setBoolAt(3, 2, 0, true);
    s.setBoolAt(0, 3, 0, true);  s.setBoolAt(1, 3, 0, true);  s.setBoolAt(2, 3, 0, true);  s.setBoolAt(3, 3, 0, false);

    Image m{4, 4, Type::uint8x2};
    m.setBoolAt(0, 0, 0, true);  m.setBoolAt(1, 0, 0, true);  m.setBoolAt(2, 0, 0, false); m.setBoolAt(3, 0, 0, true);
    m.setBoolAt(0, 1, 0, true);  m.setBoolAt(1, 1, 0, false); m.setBoolAt(2, 1, 0, false); m.setBoolAt(3, 1, 0, true);
    m.setBoolAt(0, 2, 0, true);  m.setBoolAt(1, 2, 0, false); m.setBoolAt(2, 2, 0, false); m.setBoolAt(3, 2, 0, true);
    m.setBoolAt(0, 3, 0, true);  m.setBoolAt(1, 3, 0, true);  m.setBoolAt(2, 3, 0, true);  m.setBoolAt(3, 3, 0, true);

    m.setBoolAt(0, 0, 1, true);  m.setBoolAt(1, 0, 1, true);  m.setBoolAt(2, 0, 1, true);  m.setBoolAt(3, 0, 1, true);
    m.setBoolAt(0, 1, 1, true);  m.setBoolAt(1, 1, 1, true);  m.setBoolAt(2, 1, 1, true);  m.setBoolAt(3, 1, 1, true);
    m.setBoolAt(0, 2, 1, true);  m.setBoolAt(1, 2, 1, true);  m.setBoolAt(2, 2, 1, true);  m.setBoolAt(3, 2, 1, true);
    m.setBoolAt(0, 3, 1, false); m.setBoolAt(1, 3, 1, false); m.setBoolAt(2, 3, 1, false); m.setBoolAt(3, 3, 1, false);


    // test ConstImage::mean()
    auto mean = i.mean();
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 8.5); // mean of any row or col: 34 / 4  = 8.5
    BOOST_CHECK_EQUAL(mean.back(),  2.5); // mean of any col: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5

    mean = i.mean(s);
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 90 / 10);
    BOOST_CHECK_EQUAL(mean.back(),  30 / 10);

    mean = i.mean(m);
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 99 / 11);
    BOOST_CHECK_EQUAL(mean.back(),  2); // mean of any col: (1 + 2 + 3) / 3 = 6 / 3 = 2


    // test ConstImage::meanStdDev()
    constexpr bool sampleCorrection = true;
    auto meanStdDev = i.meanStdDev();
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 8.5); // mean of any row or col: 34 / 4  = 8.5
    BOOST_CHECK_EQUAL(mean.back(),  2.5); // mean of any col: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5

    auto stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 4.60977222864644), 1e-13); // from octave
    BOOST_CHECK_LE(std::abs(stdDev.back()  - 1.11803398874989), 1e-13); // from octave


    meanStdDev = i.meanStdDev(Image{}, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 8.5); // mean of any row or col: 34 / 4  = 8.5
    BOOST_CHECK_EQUAL(mean.back(),  2.5); // mean of any col: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 4.76095228569523), 1e-13); // from octave
    BOOST_CHECK_LE(std::abs(stdDev.back()  - 1.15470053837925), 1e-13); // from octave


    meanStdDev = i.meanStdDev(s);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 90 / 10);
    BOOST_CHECK_EQUAL(mean.back(),  30 / 10);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 3.54964786985977),  1e-13); // from octave
    BOOST_CHECK_LE(std::abs(stdDev.back()  - 0.774596669241483), 1e-13); // from octave


    meanStdDev = i.meanStdDev(s, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 90 / 10);
    BOOST_CHECK_EQUAL(mean.back(),  30 / 10);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 3.74165738677394),  1e-13); // from octave
    BOOST_CHECK_LE(std::abs(stdDev.back()  - 0.816496580927726), 1e-13); // from octave


    meanStdDev = i.meanStdDev(m);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 99 / 11);
    BOOST_CHECK_EQUAL(mean.back(),  2); // mean of any col: (1 + 2 + 3) / 3 = 6 / 3 = 2


    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 5.13455318052470),  1e-13); // from octave
    BOOST_CHECK_LE(std::abs(stdDev.back()  - 0.816496580927726), 1e-13); // from octave


    meanStdDev = i.meanStdDev(m, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 99 / 11);
    BOOST_CHECK_EQUAL(mean.back(),  2); // mean of any col: (1 + 2 + 3) / 3 = 6 / 3 = 2


    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 5.38516480713450),  1e-13); // from octave
    BOOST_CHECK_LE(std::abs(stdDev.back()  - 0.852802865422442), 1e-13); // from octave
}

BOOST_AUTO_TEST_CASE(meanStdDev_5chans)
{
    /*    channel 0           channel 1           channel 2           channel 3           channel 4
     * img:
     * magic(4) matrix:                         copy of channel 1:  copy of channel 1:  copy of channel 1:
     * 16   2   3  13       1   1   1   1       1   1   1   1       1   1   1   1       1   1   1   1
     *  5  11  10   8       2   2   2   2       2   2   2   2       2   2   2   2       2   2   2   2
     *  9   7   6  12       3   3   3   3       3   3   3   3       3   3   3   3       3   3   3   3
     *  4  14  15   1       4   4   4   4       4   4   4   4       4   4   4   4       4   4   4   4
     *
     * single-channel mask:
     *  0   0   0   0
     *  1   0   1   1
     *  1   1   1   1
     *  1   1   1   0
     *
     * multi-channel mask:
     *  1   1   0   1       1   1   1   1       1   1   1   1       1   1   1   1       1   1   1   1
     *  1   0   0   1       1   1   1   1       1   1   1   1       1   1   1   1       1   1   1   1
     *  1   0   0   1       1   1   1   1       1   1   1   1       1   1   1   1       1   1   1   1
     *  1   1   1   1       0   0   0   0       0   0   0   0       0   0   0   0       0   0   0   0
     */

    Image i{4, 4, Type::uint8x5};
    i.at<uint8_t>(0, 0, 0) = 16; i.at<uint8_t>(1, 0, 0) = 2;  i.at<uint8_t>(2, 0, 0) = 3;  i.at<uint8_t>(3, 0, 0) = 13;
    i.at<uint8_t>(0, 1, 0) = 5;  i.at<uint8_t>(1, 1, 0) = 11; i.at<uint8_t>(2, 1, 0) = 10; i.at<uint8_t>(3, 1, 0) = 8;
    i.at<uint8_t>(0, 2, 0) = 9;  i.at<uint8_t>(1, 2, 0) = 7;  i.at<uint8_t>(2, 2, 0) = 6;  i.at<uint8_t>(3, 2, 0) = 12;
    i.at<uint8_t>(0, 3, 0) = 4;  i.at<uint8_t>(1, 3, 0) = 14; i.at<uint8_t>(2, 3, 0) = 15; i.at<uint8_t>(3, 3, 0) = 1;

    for (int y = 0; y < i.height(); ++y)
        for (int x = 0; x < i.width(); ++x)
            for (unsigned int c = 1; c < i.channels(); ++c)
                i.at<uint8_t>(x, y, c) = y + 1;

    Image s{4, 4, Type::uint8x1};
    s.setBoolAt(0, 0, 0, false); s.setBoolAt(1, 0, 0, false); s.setBoolAt(2, 0, 0, false); s.setBoolAt(3, 0, 0, false);
    s.setBoolAt(0, 1, 0, true);  s.setBoolAt(1, 1, 0, false); s.setBoolAt(2, 1, 0, true);  s.setBoolAt(3, 1, 0, true);
    s.setBoolAt(0, 2, 0, true);  s.setBoolAt(1, 2, 0, true);  s.setBoolAt(2, 2, 0, true);  s.setBoolAt(3, 2, 0, true);
    s.setBoolAt(0, 3, 0, true);  s.setBoolAt(1, 3, 0, true);  s.setBoolAt(2, 3, 0, true);  s.setBoolAt(3, 3, 0, false);

    Image m{4, 4, Type::uint8x5};
    m.setBoolAt(0, 0, 0, true);  m.setBoolAt(1, 0, 0, true);  m.setBoolAt(2, 0, 0, false); m.setBoolAt(3, 0, 0, true);
    m.setBoolAt(0, 1, 0, true);  m.setBoolAt(1, 1, 0, false); m.setBoolAt(2, 1, 0, false); m.setBoolAt(3, 1, 0, true);
    m.setBoolAt(0, 2, 0, true);  m.setBoolAt(1, 2, 0, false); m.setBoolAt(2, 2, 0, false); m.setBoolAt(3, 2, 0, true);
    m.setBoolAt(0, 3, 0, true);  m.setBoolAt(1, 3, 0, true);  m.setBoolAt(2, 3, 0, true);  m.setBoolAt(3, 3, 0, true);

    for (int y = 0; y < m.height(); ++y)
        for (int x = 0; x < m.width(); ++x)
            for (unsigned int c = 1; c < m.channels(); ++c)
                m.setBoolAt(x, y, c, y < 3);

    // test ConstImage::mean()
    auto mean = i.mean();
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 8.5); // mean of any row or col: 34 / 4  = 8.5
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  2.5); // mean of any col: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5

    mean = i.mean(s);
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 90 / 10);
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  30 / 10);

    mean = i.mean(m);
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 99 / 11);
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  2); // mean of any col: (1 + 2 + 3) / 3 = 6 / 3 = 2


    // test ConstImage::meanStdDev()
    constexpr bool sampleCorrection = true;
    auto meanStdDev = i.meanStdDev();
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 8.5); // mean of any row or col: 34 / 4  = 8.5
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i), 2.5); // mean of any col: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5

    auto stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(), 5);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 4.60977222864644), 1e-13); // from octave
    for (std::size_t i = 1; i < stdDev.size(); ++i)
        BOOST_CHECK_LE(std::abs(stdDev.at(i)  - 1.11803398874989), 1e-13); // from octave


    meanStdDev = i.meanStdDev(Image{}, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 8.5); // mean of any row or col: 34 / 4  = 8.5
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  2.5); // mean of any col: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(), 5);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 4.76095228569523), 1e-13); // from octave
    for (std::size_t i = 1; i < stdDev.size(); ++i)
        BOOST_CHECK_LE(std::abs(stdDev.at(i)  - 1.15470053837925), 1e-13); // from octave


    meanStdDev = i.meanStdDev(s);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 90 / 10);
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  30 / 10);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(), 5);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 3.54964786985977),  1e-13); // from octave
    for (std::size_t i = 1; i < stdDev.size(); ++i)
        BOOST_CHECK_LE(std::abs(stdDev.at(i)  - 0.774596669241483), 1e-13); // from octave


    meanStdDev = i.meanStdDev(s, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 90 / 10);
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  30 / 10);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(), 5);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 3.74165738677394),  1e-13); // from octave
    for (std::size_t i = 1; i < stdDev.size(); ++i)
        BOOST_CHECK_LE(std::abs(stdDev.at(i)  - 0.816496580927726), 1e-13); // from octave


    meanStdDev = i.meanStdDev(m);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 99 / 11);
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  2); // mean of any col: (1 + 2 + 3) / 3 = 6 / 3 = 2


    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(), 5);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 5.13455318052470),  1e-13); // from octave
    for (std::size_t i = 1; i < stdDev.size(); ++i)
        BOOST_CHECK_LE(std::abs(stdDev.at(i)  - 0.816496580927726), 1e-13); // from octave


    meanStdDev = i.meanStdDev(m, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(), 5);
    BOOST_CHECK_EQUAL(mean.front(), 99 / 11);
    for (std::size_t i = 1; i < mean.size(); ++i)
        BOOST_CHECK_EQUAL(mean.at(i),  2); // mean of any col: (1 + 2 + 3) / 3 = 6 / 3 = 2


    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(), 5);
    BOOST_CHECK_LE(std::abs(stdDev.front() - 5.38516480713450),  1e-13); // from octave
    for (std::size_t i = 1; i < stdDev.size(); ++i)
        BOOST_CHECK_LE(std::abs(stdDev.at(i)  - 0.852802865422442), 1e-13); // from octave
}


// test corner cases for mean method and meanStdDev
BOOST_AUTO_TEST_CASE(meanStdDev_extrem)
{
    /* channel 1   channel 2
     * img:
     *  1   2      5   6
     *  3   4      7   8
     *
     * single-channel mask 1:
     *  1   0
     *  0   0
     *
     * single-channel mask 2:
     *  1   0
     *  1   0
     *
     * multi-channel mask:
     *  1   0      0   0
     *  1   0      0   0
     */

    Image i{2, 2, Type::uint8x2};
    i.at<uint8_t>(0, 0, 0) = 1;   i.at<uint8_t>(1, 0, 0) = 2;
    i.at<uint8_t>(0, 1, 0) = 3;   i.at<uint8_t>(1, 1, 0) = 4;

    i.at<uint8_t>(0, 0, 1) = 5;   i.at<uint8_t>(1, 0, 1) = 6;
    i.at<uint8_t>(0, 1, 1) = 7;   i.at<uint8_t>(1, 1, 1) = 8;

    Image s1{2, 2, Type::uint8x1};
    s1.setBoolAt(0, 0, 0, true);  s1.setBoolAt(1, 0, 0, false);
    s1.setBoolAt(0, 1, 0, false); s1.setBoolAt(1, 1, 0, false);

    Image s2{2, 2, Type::uint8x1};
    s2.setBoolAt(0, 0, 0, true);  s2.setBoolAt(1, 0, 0, false);
    s2.setBoolAt(0, 1, 0, true);  s2.setBoolAt(1, 1, 0, false);

    Image m{2, 2, Type::uint8x2};
    m.setBoolAt(0, 0, 0, true);   m.setBoolAt(1, 0, 0, false);
    m.setBoolAt(0, 1, 0, true);   m.setBoolAt(1, 1, 0, false);

    m.setBoolAt(0, 0, 1, false);  m.setBoolAt(1, 0, 1, false);
    m.setBoolAt(0, 1, 1, false);  m.setBoolAt(1, 1, 1, false);


    // test ConstImage::mean()
    auto mean = i.mean(s1);
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 1);
    BOOST_CHECK_EQUAL(mean.back(),  5);

    mean = i.mean(s2);
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 2);
    BOOST_CHECK_EQUAL(mean.back(),  6);

    mean = i.mean(m);
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 2);
    BOOST_CHECK_EQUAL(mean.back(),  0);


    // test ConstImage::meanStdDev()
    constexpr bool sampleCorrection = true;
    auto meanStdDev = i.meanStdDev(s1);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 1);
    BOOST_CHECK_EQUAL(mean.back(),  5);

    auto stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_EQUAL(stdDev.front(), 0);
    BOOST_CHECK_EQUAL(stdDev.back(),  0);


    meanStdDev = i.meanStdDev(s1, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 1);
    BOOST_CHECK_EQUAL(mean.back(),  5);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_EQUAL(stdDev.front(), 0);
    BOOST_CHECK_EQUAL(stdDev.back(),  0);


    meanStdDev = i.meanStdDev(s2);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 2);
    BOOST_CHECK_EQUAL(mean.back(),  6);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_EQUAL(stdDev.front(), 1);
    BOOST_CHECK_EQUAL(stdDev.back(),  1);


    meanStdDev = i.meanStdDev(s2, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 2);
    BOOST_CHECK_EQUAL(mean.back(),  6);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_EQUAL(stdDev.front(), std::sqrt(2));
    BOOST_CHECK_EQUAL(stdDev.back(),  std::sqrt(2));


    meanStdDev = i.meanStdDev(m);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 2);
    BOOST_CHECK_EQUAL(mean.back(),  0);

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_EQUAL(stdDev.front(), 1);
    BOOST_CHECK_EQUAL(stdDev.back(),  0);


    meanStdDev = i.meanStdDev(m, sampleCorrection);
    mean = meanStdDev.first;
    BOOST_CHECK_EQUAL(mean.size(),  2);
    BOOST_CHECK_EQUAL(mean.front(), 2);
    BOOST_CHECK_EQUAL(mean.back(),  0); // no valid location

    stdDev = meanStdDev.second;
    BOOST_CHECK_EQUAL(stdDev.size(),  2);
    BOOST_CHECK_EQUAL(stdDev.front(), std::sqrt(2));
    BOOST_CHECK_EQUAL(stdDev.back(),  0); // no valid location
}


// test corner cases for minMaxLocations method
BOOST_AUTO_TEST_CASE(minmax_extrem)
{
    /* channel 1   channel 2   channel 3   channel 4   channel 5
     * img:
     *  1   2       5   6       9  10      13  14      20  18
     *  3   4       7   8      11  12      15  16      19  17
     *
     * single-channel mask 1:
     *  1   0
     *  0   0
     *
     * single-channel mask 2:
     *  1   0
     *  1   0
     *
     * multi-channel mask:
     *  1   0       0   0       1   1       0   0       0   1
     *  1   0       0   0       0   0       0   1       0   0
     */

    Image i{2, 2, Type::uint8x5};
    i.at<uint8_t>(0, 0, 0) = 1;   i.at<uint8_t>(1, 0, 0) = 2;
    i.at<uint8_t>(0, 1, 0) = 3;   i.at<uint8_t>(1, 1, 0) = 4;

    i.at<uint8_t>(0, 0, 1) = 5;   i.at<uint8_t>(1, 0, 1) = 6;
    i.at<uint8_t>(0, 1, 1) = 7;   i.at<uint8_t>(1, 1, 1) = 8;

    i.at<uint8_t>(0, 0, 2) = 9;   i.at<uint8_t>(1, 0, 2) = 10;
    i.at<uint8_t>(0, 1, 2) = 11;  i.at<uint8_t>(1, 1, 2) = 12;

    i.at<uint8_t>(0, 0, 3) = 13;  i.at<uint8_t>(1, 0, 3) = 14;
    i.at<uint8_t>(0, 1, 3) = 15;  i.at<uint8_t>(1, 1, 3) = 16;

    i.at<uint8_t>(0, 0, 4) = 20;  i.at<uint8_t>(1, 0, 4) = 18;
    i.at<uint8_t>(0, 1, 4) = 19;  i.at<uint8_t>(1, 1, 4) = 17;

    Image s1{2, 2, Type::uint8x1};
    s1.setBoolAt(0, 0, 0, true);  s1.setBoolAt(1, 0, 0, false);
    s1.setBoolAt(0, 1, 0, false); s1.setBoolAt(1, 1, 0, false);

    Image s2{2, 2, Type::uint8x1};
    s2.setBoolAt(0, 0, 0, true);  s2.setBoolAt(1, 0, 0, false);
    s2.setBoolAt(0, 1, 0, true);  s2.setBoolAt(1, 1, 0, false);

    Image m{2, 2, Type::uint8x5};
    m.setBoolAt(0, 0, 0, true);   m.setBoolAt(1, 0, 0, false);
    m.setBoolAt(0, 1, 0, true);   m.setBoolAt(1, 1, 0, false);

    m.setBoolAt(0, 0, 1, false);  m.setBoolAt(1, 0, 1, false);
    m.setBoolAt(0, 1, 1, false);  m.setBoolAt(1, 1, 1, false);

    m.setBoolAt(0, 0, 2, true);   m.setBoolAt(1, 0, 2, true);
    m.setBoolAt(0, 1, 2, false);  m.setBoolAt(1, 1, 2, false);

    m.setBoolAt(0, 0, 3, false);  m.setBoolAt(1, 0, 3, false);
    m.setBoolAt(0, 1, 3, false);  m.setBoolAt(1, 1, 3, true);

    m.setBoolAt(0, 0, 4, false);  m.setBoolAt(1, 0, 4, true);
    m.setBoolAt(0, 1, 4, false);  m.setBoolAt(1, 1, 4, false);


    // test ConstImage::minMaxLocation()
    using pair_t = std::pair<ValueWithLocation, ValueWithLocation>;
    auto minMaxLocation = i.minMaxLocations();
    std::array<pair_t, 5> exp{
        pair_t{{1.0,  {0, 0}}, {4.0,  {1, 1}}},
        pair_t{{5.0,  {0, 0}}, {8.0,  {1, 1}}},
        pair_t{{9.0,  {0, 0}}, {12.0, {1, 1}}},
        pair_t{{13.0, {0, 0}}, {16.0, {1, 1}}},
        pair_t{{17.0, {1, 1}}, {20.0, {0, 0}}}
    };

    BOOST_CHECK_EQUAL(minMaxLocation.size(),  5);
    for (unsigned int i = 0; i < exp.size(); ++i) {
        BOOST_CHECK(exp.at(i) == minMaxLocation.at(i));
    }

    // test ConstImage::minMaxLocation() with single channel mask
    minMaxLocation = i.minMaxLocations(s1);
    std::array<pair_t, 5> exp_s1{
        pair_t{{1.0,  {0, 0}}, {1.0,  {0, 0}}},
        pair_t{{5.0,  {0, 0}}, {5.0,  {0, 0}}},
        pair_t{{9.0,  {0, 0}}, {9.0,  {0, 0}}},
        pair_t{{13.0, {0, 0}}, {13.0, {0, 0}}},
        pair_t{{20.0, {0, 0}}, {20.0, {0, 0}}}
    };

    BOOST_CHECK_EQUAL(minMaxLocation.size(),  5);
    for (unsigned int i = 0; i < exp_s1.size(); ++i) {
        BOOST_CHECK(exp_s1.at(i) == minMaxLocation.at(i));
    }


    minMaxLocation = i.minMaxLocations(s2);
    std::array<pair_t, 5> exp_s2{
        pair_t{{1.0,  {0, 0}}, {3.0,  {0, 1}}},
        pair_t{{5.0,  {0, 0}}, {7.0,  {0, 1}}},
        pair_t{{9.0,  {0, 0}}, {11.0, {0, 1}}},
        pair_t{{13.0, {0, 0}}, {15.0, {0, 1}}},
        pair_t{{19.0, {0, 1}}, {20.0, {0, 0}}}
    };

    BOOST_CHECK_EQUAL(minMaxLocation.size(),  5);
    for (unsigned int i = 0; i < exp_s2.size(); ++i) {
        BOOST_CHECK(exp_s2.at(i) == minMaxLocation.at(i));
    }

    // test ConstImage::minMaxLocation() with multi channel mask
    minMaxLocation = i.minMaxLocations(m);
    std::array<pair_t, 5> exp_m{
        pair_t{{1.0,  {0, 0}}, {3.0,  {0, 1}}},
        pair_t{{0.0,  {-1, -1}}, {0.0,  {-1, -1}}}, // no valid location
        pair_t{{9.0,  {0, 0}}, {10.0, {1, 0}}},
        pair_t{{16.0, {1, 1}}, {16.0, {1, 1}}},
        pair_t{{18.0, {1, 0}}, {18.0, {1, 0}}}
    };

    BOOST_CHECK_EQUAL(minMaxLocation.size(),  5);
    for (unsigned int i = 0; i < exp_m.size(); ++i) {
        BOOST_CHECK(exp_m.at(i) == minMaxLocation.at(i));
//        std::cout << "min: "   << minMaxLocation.at(i).first.val
//                  << " at "    << minMaxLocation.at(i).first.p
//                  << ", max: " << minMaxLocation.at(i).second.val
//                  << " at "    << minMaxLocation.at(i).second.p
//                  << std::endl;
    }
}


// test conversion of single-channel images of different type
BOOST_AUTO_TEST_CASE(conversion)
{
    // uint8 to uint16
    Image ic1_8{5, 6, Type::uint8};
    for (int x = 0; x < ic1_8.width(); ++x)
        for (int y = 0; y < ic1_8.height(); ++y)
            ic1_8.at<uint8_t>(x, y) = 10 * y + x;

    Image ic1_16 = ic1_8.convertTo(Type::uint16);
    for (int x = 0; x < ic1_16.width(); ++x)
        for (int y = 0; y < ic1_16.height(); ++y)
            BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), 10 * y + x);

    // uint16 to uint8
    Image ic2_16{5, 6, Type::uint16};
    for (int x = 0; x < ic2_16.width(); ++x)
        for (int y = 0; y < ic2_16.height(); ++y)
            ic2_16.at<uint16_t>(x, y) = 10 * y + x;

    Image ic2_8 = ic2_16.convertTo(Type::uint8);
    for (int x = 0; x < ic2_8.width(); ++x)
        for (int y = 0; y < ic2_8.height(); ++y)
            BOOST_CHECK_EQUAL(ic2_8.at<uint8_t>(x, y), 10 * y + x);

    // uint16 to double
    Image ic3_16{5, 6, Type::uint16};
    for (int x = 0; x < ic3_16.width(); ++x)
        for (int y = 0; y < ic3_16.height(); ++y)
            ic3_16.at<uint16_t>(x, y) = 10 * y + x;

    Image ic3_d = ic2_16.convertTo(Type::float64);
    for (int x = 0; x < ic3_d.width(); ++x)
        for (int y = 0; y < ic3_d.height(); ++y)
            BOOST_CHECK_EQUAL(ic3_d.at<double>(x, y), 10 * y + x);

    // double to uint16
    Image ic4_d{5, 6, Type::float64};
    for (int x = 0; x < ic4_d.width(); ++x)
        for (int y = 0; y < ic4_d.height(); ++y)
            ic4_d.at<double>(x, y) = 10.1 * y + x;

    Image ic4_16 = ic2_16.convertTo(Type::uint16);
    for (int x = 0; x < ic4_16.width(); ++x)
        for (int y = 0; y < ic4_16.height(); ++y)
            BOOST_CHECK_EQUAL(ic4_16.at<uint16_t>(x, y), 10 * y + x);
}

// build this test case only in Boost 1.59+, since from there on it is possible to disable a test case by default
// we do not want to run this tests by default, since it prints a lot of stuff
// run with --run_test=Image_Suite/minmax_color_space_values
#if BOOST_VERSION / 100 % 1000 > 58 //|| 1
#if BOOST_VERSION / 100 % 1000 > 58
// usage text test outputs, disabled by default, run with --run_test=Image_Suite/minmax_color_space_values
BOOST_AUTO_TEST_CASE(minmax_color_space_values, * boost::unit_test::disabled())
#else /* Boost version <= 1.58 */
BOOST_AUTO_TEST_CASE(minmax_color_space_values)
#endif
{
    double Lmin =  1e100;
    double u1min =  1e100;
    double v1min =  1e100;
    double u2min =  1e100;
    double v2min =  1e100;
    double amin =  1e100;
    double bmin =  1e100;

    double Lmin_R,  Lmin_G,  Lmin_B,
           u1min_R, u1min_G, u1min_B,
           v1min_R, v1min_G, v1min_B,
           u2min_R, u2min_G, u2min_B,
           v2min_R, v2min_G, v2min_B,
           amin_R,  amin_G,  amin_B,
           bmin_R,  bmin_G,  bmin_B;

    double Lmax = -1e100;
    double u1max = -1e100;
    double v1max = -1e100;
    double u2max = -1e100;
    double v2max = -1e100;
    double amax = -1e100;
    double bmax = -1e100;

    double Lmax_R,  Lmax_G,  Lmax_B,
           u1max_R, u1max_G, u1max_B,
           v1max_R, v1max_G, v1max_B,
           u2max_R, u2max_G, u2max_B,
           v2max_R, v2max_G, v2max_B,
           amax_R,  amax_G,  amax_B,
           bmax_R,  bmax_G,  bmax_B;

    double Xmin =  1e100;
    double Ymin =  1e100;
    double Zmin =  1e100;

    double Xmax = -1e100;
    double Ymax = -1e100;
    double Zmax = -1e100;

    double Xmin_R, Xmin_G, Xmin_B,
           Ymin_R, Ymin_G, Ymin_B,
           Zmin_R, Zmin_G, Zmin_B,
           Xmax_R, Xmax_G, Xmax_B,
           Ymax_R, Ymax_G, Ymax_B,
           Zmax_R, Zmax_G, Zmax_B;


    constexpr double delta = 6. / 29.;
    auto f = [](double t) { return t > std::pow(delta, 3)
                                   ? std::cbrt(t)
                                   : t / (3 * delta * delta) + 4. / 29.; };
    constexpr double Rstart = 0.114;
    constexpr double Rend   = 0.116;
    constexpr double Gstart = 0;
    constexpr double Gend   = 1;
    constexpr double Bstart = 0;
    constexpr double Bend   = 1;
    constexpr double Rstep = (Rend - Rstart) / 1;
    constexpr double Gstep = (Gend - Gstart) / 1;
    constexpr double Bstep = (Bend - Bstart) / 1;
    for (double B = Bstart; B <= Bend; B += Bstep) {
        std::cout << B << " ";
        std::cout.flush();
        for (double G = Gstart; G <= Gend; G += Gstep) {
            for (double R = Rstart; R <= Rend; R += Rstep) {
                double X = 0.4124532201441615 * R + 0.3575795812935000 * G + 0.1804225899705355 * B;
                double Y = 0.2126711213412183 * R + 0.7151592053107827 * G + 0.0721687767761325 * B;
                double Z = 0.0193338164619908 * R + 0.1191935402066255 * G + 0.9502269222897068 * B;
//                double X = 0.412453 * R + 0.357580 * G + 0.180423 * B;
//                double Y = 0.212671 * R + 0.715160 * G + 0.072169 * B;
//                double Z = 0.019334 * R + 0.119193 * G + 0.950227 * B;
                X /= 0.950455391408197;
                Y /= 0.999999103428133;
                Z /= 1.088754278958323;

                double L = 116 * f(Y) - 16;
                double a = 500 * (f(X) - f(Y));
                double b = 200 * (f(Y) - f(Z));
                double u1 = 13 * L * (4 * X / (X + 15 * Y + 3 * Z) - 0.2009);
                double v1 = 13 * L * (9 * Y / (X + 15 * Y + 3 * Z) - 0.461);
                double u2 = 13 * L * (4 * X / (X + 15 * Y + 3 * Z) - 0.19793943);
                double v2 = 13 * L * (9 * Y / (X + 15 * Y + 3 * Z) - 0.46831096);

                if (Xmin > X) {
                    Xmin = X;
                    Xmin_R = R;
                    Xmin_G = G;
                    Xmin_B = B;
                }
                if (Xmax < X) {
                    Xmax = X;
                    Xmax_R = R;
                    Xmax_G = G;
                    Xmax_B = B;
                }
                if (Ymin > Y) {
                    Ymin = Y;
                    Ymin_R = R;
                    Ymin_G = G;
                    Ymin_B = B;
                }
                if (Ymax < Y) {
                    Ymax = Y;
                    Ymax_R = R;
                    Ymax_G = G;
                    Ymax_B = B;
                }
                if (Zmin > Z) {
                    Zmin = Z;
                    Zmin_R = R;
                    Zmin_G = G;
                    Zmin_B = B;
                }
                if (Zmax < Z) {
                    Zmax = Z;
                    Zmax_R = R;
                    Zmax_G = G;
                    Zmax_B = B;
                }

                if (Lmin > L) {
                    Lmin = L;
                    Lmin_R = R;
                    Lmin_G = G;
                    Lmin_B = B;
                }
                if (Lmax < L) {
                    Lmax = L;
                    Lmax_R = R;
                    Lmax_G = G;
                    Lmax_B = B;
                }
                if (u1min > u1) {
                    u1min = u1;
                    u1min_R = R;
                    u1min_G = G;
                    u1min_B = B;
                }
                if (u1max < u1) {
                    u1max = u1;
                    u1max_R = R;
                    u1max_G = G;
                    u1max_B = B;
                }
                if (u2min > u2) {
                    u2min = u2;
                    u2min_R = R;
                    u2min_G = G;
                    u2min_B = B;
                }
                if (u2max < u2) {
                    u2max = u2;
                    u2max_R = R;
                    u2max_G = G;
                    u2max_B = B;
                }
                if (v1min > v1) {
                    v1min = v1;
                    v1min_R = R;
                    v1min_G = G;
                    v1min_B = B;
                }
                if (v1max < v1) {
                    v1max = v1;
                    v1max_R = R;
                    v1max_G = G;
                    v1max_B = B;
                }
                if (v2min > v2) {
                    v2min = v2;
                    v2min_R = R;
                    v2min_G = G;
                    v2min_B = B;
                }
                if (v2max < v2) {
                    v2max = v2;
                    v2max_R = R;
                    v2max_G = G;
                    v2max_B = B;
                }
                if (amin > a) {
                    amin = a;
                    amin_R = R;
                    amin_G = G;
                    amin_B = B;
                }
                if (amax < a) {
                    amax = a;
                    amax_R = R;
                    amax_G = G;
                    amax_B = B;
                }
                if (bmin > b) {
                    bmin = b;
                    bmin_R = R;
                    bmin_G = G;
                    bmin_B = B;
                }
                if (bmax < b) {
                    bmax = b;
                    bmax_R = R;
                    bmax_G = G;
                    bmax_B = B;
                }
            }
        }
    }
    std::cout << std::endl;

    std::cout << "min X:  " << std::setw(4) << Xmin  /*    0     */ << "\t at: [" << std::setw(4) << Xmin_R  << '\t' << std::setw(4) << Xmin_G  << '\t' << std::setw(4) << Xmin_B  <<  /* 0        0 0 */ "]" << std::endl;
    std::cout << "max X:  " << std::setw(4) << Xmax  /*    1     */ << "\t at: [" << std::setw(4) << Xmax_R  << '\t' << std::setw(4) << Xmax_G  << '\t' << std::setw(4) << Xmax_B  <<  /* 1        1 1 */ "]" << std::endl;
    std::cout << "min Y:  " << std::setw(4) << Ymin  /*    0     */ << "\t at: [" << std::setw(4) << Ymin_R  << '\t' << std::setw(4) << Ymin_G  << '\t' << std::setw(4) << Ymin_B  <<  /* 0        0 0 */ "]" << std::endl;
    std::cout << "max Y:  " << std::setw(4) << Ymax  /*    1     */ << "\t at: [" << std::setw(4) << Ymax_R  << '\t' << std::setw(4) << Ymax_G  << '\t' << std::setw(4) << Ymax_B  <<  /* 1        1 1 */ "]" << std::endl;
    std::cout << "min Z:  " << std::setw(4) << Zmin  /*    0     */ << "\t at: [" << std::setw(4) << Zmin_R  << '\t' << std::setw(4) << Zmin_G  << '\t' << std::setw(4) << Zmin_B  <<  /* 0        0 0 */ "]" << std::endl;
    std::cout << "max Z:  " << std::setw(4) << Zmax  /*    1     */ << "\t at: [" << std::setw(4) << Zmax_R  << '\t' << std::setw(4) << Zmax_G  << '\t' << std::setw(4) << Zmax_B  <<  /* 1        1 1 */ "]" << std::endl;
    std::cout << "min L:  " << std::setw(4) << Lmin  /*    0     */ << "\t at: [" << std::setw(4) << Lmin_R  << '\t' << std::setw(4) << Lmin_G  << '\t' << std::setw(4) << Lmin_B  <<  /* 0        0 0 */ "]" << std::endl;
    std::cout << "max L:  " << std::setw(4) << Lmax  /*  100     */ << "\t at: [" << std::setw(4) << Lmax_R  << '\t' << std::setw(4) << Lmax_G  << '\t' << std::setw(4) << Lmax_B  <<  /* 1        1 1 */ "]" << std::endl;
    std::cout << "min u1: " << std::setw(4) << u1min /* -78.9986 */ << "\t at: [" << std::setw(4) << u1min_R << '\t' << std::setw(4) << u1min_G << '\t' << std::setw(4) << u1min_B <<  /* 0        1 0 */ "] (u_n: 0.2009)" << std::endl;
    std::cout << "max u1: " << std::setw(4) << u1max /* 187.66   */ << "\t at: [" << std::setw(4) << u1max_R << '\t' << std::setw(4) << u1max_G << '\t' << std::setw(4) << u1max_B <<  /* 1        0 0 */ "] (u_n: 0.2009)" << std::endl;
    std::cout << "min u2: " << std::setw(4) << u2min /* -75.622  */ << "\t at: [" << std::setw(4) << u2min_R << '\t' << std::setw(4) << u2min_G << '\t' << std::setw(4) << u2min_B <<  /* 0        1 0 */ "] (u_n: 0.19793943)" << std::endl;
    std::cout << "max u2: " << std::setw(4) << u2max /* 189.709  */ << "\t at: [" << std::setw(4) << u2max_R << '\t' << std::setw(4) << u2max_G << '\t' << std::setw(4) << u2max_B <<  /* 1        0 0 */ "] (u_n: 0.19793943)" << std::endl;
    std::cout << "min v1: " << std::setw(4) << v1min /* -125.544 */ << "\t at: [" << std::setw(4) << v1min_R << '\t' << std::setw(4) << v1min_G << '\t' << std::setw(4) << v1min_B <<  /* 0.115464 0 1 */ "] (v_n: 0.461)" << std::endl;
    std::cout << "max v1: " << std::setw(4) << v1max /* 116.356  */ << "\t at: [" << std::setw(4) << v1max_R << '\t' << std::setw(4) << v1max_G << '\t' << std::setw(4) << v1max_B <<  /* 0        1 0 */ "] (v_n: 0.461)" << std::endl;
    std::cout << "min v2: " << std::setw(4) << v2min /* -129.113 */ << "\t at: [" << std::setw(4) << v2min_R << '\t' << std::setw(4) << v2min_G << '\t' << std::setw(4) << v2min_B <<  /* 0.131    0 1 */ "] (v_n: 0.46831096)" << std::endl;
    std::cout << "max v2: " << std::setw(4) << v2max /* 108.018  */ << "\t at: [" << std::setw(4) << v2max_R << '\t' << std::setw(4) << v2max_G << '\t' << std::setw(4) << v2max_B <<  /* 0        1 0 */ "] (v_n: 0.46831096)" << std::endl;
    std::cout << "min a:  " << std::setw(4) << amin  /* -86.1813 */ << "\t at: [" << std::setw(4) << amin_R  << '\t' << std::setw(4) << amin_G  << '\t' << std::setw(4) << amin_B  <<  /* 0        1 0 */ "]" << std::endl;
    std::cout << "max a:  " << std::setw(4) << amax  /* 98.2352  */ << "\t at: [" << std::setw(4) << amax_R  << '\t' << std::setw(4) << amax_G  << '\t' << std::setw(4) << amax_B  <<  /* 1        0 1 */ "]" << std::endl;
    std::cout << "min b:  " << std::setw(4) << bmin  /* -107.862 */ << "\t at: [" << std::setw(4) << bmin_R  << '\t' << std::setw(4) << bmin_G  << '\t' << std::setw(4) << bmin_B  <<  /* 0        0 1 */ "]" << std::endl;
    std::cout << "max b:  " << std::setw(4) << bmax  /* 94.4758  */ << "\t at: [" << std::setw(4) << bmax_R  << '\t' << std::setw(4) << bmax_G  << '\t' << std::setw(4) << bmax_B  <<  /* 1        1 0 */ "]" << std::endl;
}
#endif /* Boost version > 1.58 */


BOOST_AUTO_TEST_CASE(color_conversion_gray) {
    constexpr int size = 10;
    // uint8_t Images can be converted with OpenCV, but double Images not
    Image rgb_u8{size, size, Type::uint8x3};
    Image rgb_f64{size, size, Type::float64x3}; // with same values as uint8
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            rgb_u8.at<uint8_t>(x, y, 0) = static_cast<uint8_t>(x + y);
            rgb_u8.at<uint8_t>(x, y, 1) = static_cast<uint8_t>(x + size*y);
            rgb_u8.at<uint8_t>(x, y, 2) = static_cast<uint8_t>(size * x + y);

            rgb_f64.at<double>(x, y, 0) = rgb_u8.at<uint8_t>(x, y, 0);
            rgb_f64.at<double>(x, y, 1) = rgb_u8.at<uint8_t>(x, y, 1);
            rgb_f64.at<double>(x, y, 2) = rgb_u8.at<uint8_t>(x, y, 2);
        }
    }

    Image gray_u8  = rgb_u8.convertColor(ColorMapping::RGB_to_Gray);
    Image gray_f64 = rgb_f64.convertColor(ColorMapping::RGB_to_Gray);

    // try with OpenCV
    Image gray_cv8;
    cv::cvtColor(rgb_u8.cvMat(), gray_cv8.cvMat(), CV_RGB2GRAY);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            BOOST_CHECK_EQUAL(std::round(gray_f64.at<double>(x, y)),
                              static_cast<int>(gray_u8.at<uint8_t>(x, y)));
            BOOST_CHECK_EQUAL(static_cast<int>(gray_cv8.at<uint8_t>(x, y)),
                              static_cast<int>(gray_u8.at<uint8_t>(x, y)));
        }
    }

    // swaw red and blue
    gray_u8 = rgb_u8.convertColor(ColorMapping::RGB_to_Gray, Type::invalid, {2, 1, 0});
    cv::cvtColor(rgb_u8.cvMat(), gray_cv8.cvMat(), CV_BGR2GRAY);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            BOOST_CHECK_EQUAL(static_cast<int>(gray_cv8.at<uint8_t>(x, y)),
                              static_cast<int>(gray_u8.at<uint8_t>(x, y)));
}


BOOST_AUTO_TEST_CASE(color_conversion_forth_and_back) {
    // size for 16 * 16 * 16 pixel
    constexpr int width  = 16 * 4;
    constexpr int height = 16 * 4;
    Image rgb_f{width, height, Type::float32x3};
    Image rgb_s{width, height, Type::int16x3};
    Image rgb_u{width, height, Type::uint16x3};
    auto it_f     = rgb_f.begin<std::array<float,3>>();
    auto it_s     = rgb_s.begin<std::array<int16_t,3>>();
    auto it_u     = rgb_u.begin<std::array<uint16_t,3>>();
#ifndef NDEBUG
    auto it_end_f = rgb_f.end<std::array<float,3>>();
    auto it_end_s = rgb_s.end<std::array<int16_t,3>>();
    auto it_end_u = rgb_u.end<std::array<uint16_t,3>>();
#endif
    for (int ri = 0; ri < 16; ++ri) {
        for (int gi = 0; gi < 16; ++gi) {
            for (int bi = 0; bi < 16; ++bi) {
                float rf = ri / 15.;
                float gf = gi / 15.;
                float bf = bi / 15.;
                assert(it_f != it_end_f);
                std::array<float,3>& px_f = *it_f;
                px_f[0] = rf;
                px_f[1] = gf;
                px_f[2] = bf;
                ++it_f;

                int16_t rs = static_cast<int16_t>(std::round(std::numeric_limits<int16_t>::max() * rf));
                int16_t gs = static_cast<int16_t>(std::round(std::numeric_limits<int16_t>::max() * gf));
                int16_t bs = static_cast<int16_t>(std::round(std::numeric_limits<int16_t>::max() * bf));
                assert(it_s != it_end_s);
                std::array<int16_t,3>&  px_s = *it_s;
                px_s[0] = rs;
                px_s[1] = gs;
                px_s[2] = bs;
                ++it_s;

                uint16_t ru = static_cast<uint16_t>(std::round(std::numeric_limits<uint16_t>::max() * rf));
                uint16_t gu = static_cast<uint16_t>(std::round(std::numeric_limits<uint16_t>::max() * gf));
                uint16_t bu = static_cast<uint16_t>(std::round(std::numeric_limits<uint16_t>::max() * bf));
                assert(it_u != it_end_u);
                std::array<uint16_t,3>& px_u = *it_u;
                px_u[0] = ru;
                px_u[1] = gu;
                px_u[2] = bu;
                ++it_u;
            }
        }
    }

    Image converted_f, converted_s, converted_u;
    Image backConverted_f, backConverted_s, backConverted_u, backConverted_fs, backConverted_fu;

    using CM = ColorMapping;
    auto reverse = [] (CM cm)
    {
        return cm == CM::RGB_to_YCbCr ? CM::YCbCr_to_RGB
             : cm == CM::RGB_to_XYZ   ? CM::XYZ_to_RGB
             : cm == CM::RGB_to_Lab   ? CM::Lab_to_RGB
             : cm == CM::RGB_to_Luv   ? CM::Luv_to_RGB
             : cm == CM::RGB_to_HSV   ? CM::HSV_to_RGB
                                      : CM::HLS_to_RGB;
    };


    for (CM cm : {CM::RGB_to_YCbCr, CM::RGB_to_XYZ, CM::RGB_to_Lab, CM::RGB_to_Luv, CM::RGB_to_HSV, CM::RGB_to_HLS}) {
        CM rcm = reverse(cm);
        converted_f     = rgb_f.convertColor(cm);
        backConverted_f = converted_f.convertColor(rcm);
        BOOST_REQUIRE_EQUAL(backConverted_f.channels(), 3);
        BOOST_REQUIRE_EQUAL(backConverted_f.width(),    width);
        BOOST_REQUIRE_EQUAL(backConverted_f.height(),   height);
        BOOST_REQUIRE(backConverted_f.type() == Type::float32x3);

        converted_s     = rgb_s.convertColor(cm);
        backConverted_s = converted_s.convertColor(rcm);
        BOOST_REQUIRE_EQUAL(backConverted_s.channels(), 3);
        BOOST_REQUIRE_EQUAL(backConverted_s.width(),    width);
        BOOST_REQUIRE_EQUAL(backConverted_s.height(),   height);
        BOOST_REQUIRE(backConverted_s.type() == Type::int16x3);

        converted_u     = rgb_u.convertColor(cm);
        backConverted_u = converted_u.convertColor(rcm);
        BOOST_REQUIRE_EQUAL(backConverted_u.channels(), 3);
        BOOST_REQUIRE_EQUAL(backConverted_u.width(),    width);
        BOOST_REQUIRE_EQUAL(backConverted_u.height(),   height);
        BOOST_REQUIRE(backConverted_u.type() == Type::uint16x3);

        converted_f      = rgb_s.convertColor(cm, Type::float32);
        backConverted_fs = converted_f.convertColor(rcm, Type::int16);
        BOOST_REQUIRE_EQUAL(backConverted_fs.channels(), 3);
        BOOST_REQUIRE_EQUAL(backConverted_fs.width(),    width);
        BOOST_REQUIRE_EQUAL(backConverted_fs.height(),   height);
        BOOST_REQUIRE(backConverted_fs.type() == Type::int16x3);

        converted_f      = rgb_u.convertColor(cm, Type::float32);
        backConverted_fu = converted_f.convertColor(rcm, Type::uint16);
        BOOST_REQUIRE_EQUAL(backConverted_fu.channels(), 3);
        BOOST_REQUIRE_EQUAL(backConverted_fu.width(),    width);
        BOOST_REQUIRE_EQUAL(backConverted_fu.height(),   height);
        BOOST_REQUIRE(backConverted_fu.type() == Type::uint16x3);

        std::array<int, 3> err_f{0};
        std::array<int, 3> err_s{0};
        std::array<int, 3> err_u{0};
        std::array<int, 3> err_fs{0};
        std::array<int, 3> err_fu{0};
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < 3; ++c) {
                    float    bc_f  = backConverted_f.at<float>(x, y, c);
                    float    img_f = rgb_f.at<float>(x, y, c);
                    int16_t  bc_s  = backConverted_s.at<int16_t>(x, y, c);
                    int16_t  bc_fs = backConverted_fs.at<int16_t>(x, y, c);
                    int16_t  img_s = rgb_s.at<int16_t>(x, y, c);
                    uint16_t bc_u  = backConverted_u.at<uint16_t>(x, y, c);
                    uint16_t bc_fu = backConverted_fu.at<uint16_t>(x, y, c);
                    uint16_t img_u = rgb_u.at<uint16_t>(x, y, c);
//                    if (x == 1 && y == 0) {
//                        std::cout << "x: " << x << ", y: " << y << ", c: " << c
//                                  << ", float: "     << bc_f << ", orig: " << img_f
//                                  << ", signed: "    << bc_s << ", orig: " << img_s
//                                  << ", unsigned : " << bc_u << ", orig: " << img_u
//                                  << std::endl;
//                    }
                    if (std::abs(bc_f - img_f) > 1e-6f) {
                        ++err_f[c];
                    }
                    if (std::abs(bc_s - img_s) > 4) {
                        ++err_s[c];
                    }
                    if (std::abs(static_cast<int>(bc_u) - static_cast<int>(img_u)) > 4) {
                        ++err_u[c];
                    }
                    if (bc_fs != img_s) {
                        ++err_fs[c];
                    }
                    if (bc_fu != img_u) {
                        ++err_fu[c];
                    }
                }
            }
        }
        bool doReportError = false;
        for (int c = 0; c < 3; ++c)
            if (err_f[c] != 0 || err_s[c] != 0 || err_u[c] != 0 || err_fs[c] != 0 || err_fu[c] != 0)
                doReportError = true;
        if (doReportError) {
            std::string msg = "In color conversion " + to_string(cm) + " have been errors. Errors per channel:\n";
            std::array<std::string, 5> err_names
                {"Float deviation too large", "Signed deviation too large",  "Unsigned deviation too large",  "Cross-signed not equal",  "Cross-unsigned not equal"};
            std::array<std::array<int, 3>*, 5> pa{&err_f, &err_s, &err_u, &err_fs, &err_fu};
            for (unsigned int i = 0; i < pa.size(); ++i) {
                msg += err_names[i] + ": ";
                auto const& a = *pa[i];
                for (int e : a)
                    msg += std::to_string(e) + ", ";
                msg.pop_back();
                msg.pop_back();
                msg += "\n";
            }

            BOOST_ERROR(msg);
        }
    }
}


// test bitwise operations on a multi channel image
BOOST_AUTO_TEST_CASE(bitwise_multi_channel)
{
    constexpr std::array<int,6> borders = {-1, 2, 5, 8, 11, 14};

    Image clone;
    ConstImage shared;
    Image m1{14, 14, Type::uint8x5};
    Image m2{14, 14, Type::uint8x5};
    for (int y = 0; y < m1.height(); ++y) {
        for (int x = 0; x < m1.width(); ++x) {
            for (unsigned int c = 0; c < borders.size() - 1; ++c) {
                m1.setBoolAt(x, y, c, x > borders[c] && x < borders[c+1]);
                m2.setBoolAt(x, y, c, y > borders[c] && y < borders[c+1]);
            }
        }
    }
    /*    +--------------+    +--------------+
     * m1:|rr gg bb aa ??| m2:|rrrrrrrrrrrrrr|
     *    |rr gg bb aa ??|    |rrrrrrrrrrrrrr|
     *    |rr gg bb aa ??|    |              |
     *    |rr gg bb aa ??|    |gggggggggggggg|
     *    |rr gg bb aa ??|    |gggggggggggggg|
     *    |rr gg bb aa ??|    |              |
     *    |rr gg bb aa ??|    |bbbbbbbbbbbbbb|
     *    |rr gg bb aa ??|    |bbbbbbbbbbbbbb|
     *    |rr gg bb aa ??|    |              |
     *    |rr gg bb aa ??|    |aaaaaaaaaaaaaa|
     *    |rr gg bb aa ??|    |aaaaaaaaaaaaaa|
     *    |rr gg bb aa ??|    |              |
     *    |rr gg bb aa ??|    |??????????????|
     *    |rr gg bb aa ??|    |??????????????|
     *    +--------------+    +--------------+
     */

    // copy and
    Image i_and = m1.bitwise_and(m2);
    for (int y = 0; y < i_and.height(); ++y)
        for (int x = 0; x < i_and.width(); ++x)
            for (unsigned int c = 0; c < i_and.channels(); ++c)
                BOOST_CHECK_EQUAL(i_and.boolAt(x, y, c), x > borders[c] && x < borders[c+1] && y > borders[c] && y < borders[c+1]);

    // move self and
    shared = i_and.sharedCopy();
    i_and = std::move(i_and).bitwise_and(m2);
    for (int y = 0; y < i_and.height(); ++y)
        for (int x = 0; x < i_and.width(); ++x)
            for (unsigned int c = 0; c < i_and.channels(); ++c)
                BOOST_CHECK_EQUAL(i_and.boolAt(x, y, c), x > borders[c] && x < borders[c+1] && y > borders[c] && y < borders[c+1]);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_and));

    // move other and
    clone = m1.clone();
    shared = clone.sharedCopy();
    i_and = i_and.bitwise_and(std::move(clone));
    for (int y = 0; y < i_and.height(); ++y)
        for (int x = 0; x < i_and.width(); ++x)
            for (unsigned int c = 0; c < i_and.channels(); ++c)
                BOOST_CHECK_EQUAL(i_and.boolAt(x, y, c), x > borders[c] && x < borders[c+1] && y > borders[c] && y < borders[c+1]);
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_and));



    // copy or
    Image i_or = m1.bitwise_or(m2);
    for (int y = 0; y < i_or.height(); ++y)
        for (int x = 0; x < i_or.width(); ++x)
            for (unsigned int c = 0; c < i_or.channels(); ++c)
                BOOST_CHECK_EQUAL(i_or.boolAt(x, y, c), (x > borders[c] && x < borders[c+1]) || (y > borders[c] && y < borders[c+1]));

    // move self or
    shared = i_or.sharedCopy();
    i_or = std::move(i_or).bitwise_or(m2);
    for (int y = 0; y < i_or.height(); ++y)
        for (int x = 0; x < i_or.width(); ++x)
            for (unsigned int c = 0; c < i_or.channels(); ++c)
                BOOST_CHECK_EQUAL(i_or.boolAt(x, y, c), (x > borders[c] && x < borders[c+1]) || (y > borders[c] && y < borders[c+1]));
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_or));

    // move other or
    clone = m1.clone();
    shared = clone.sharedCopy();
    i_or = i_or.bitwise_or(std::move(clone));
    for (int y = 0; y < i_or.height(); ++y)
        for (int x = 0; x < i_or.width(); ++x)
            for (unsigned int c = 0; c < i_or.channels(); ++c)
                BOOST_CHECK_EQUAL(i_or.boolAt(x, y, c), (x > borders[c] && x < borders[c+1]) || (y > borders[c] && y < borders[c+1]));
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_or));



    // copy xor
    Image i_xor = m1.bitwise_xor(m2);
    for (int y = 0; y < i_xor.height(); ++y)
        for (int x = 0; x < i_xor.width(); ++x)
            for (unsigned int c = 0; c < i_xor.channels(); ++c)
                BOOST_CHECK_EQUAL(i_xor.boolAt(x, y, c), (x > borders[c] && x < borders[c+1]) ^ (y > borders[c] && y < borders[c+1]));

    // move self xor
    shared = i_xor.sharedCopy();
    i_xor = std::move(i_xor).bitwise_xor(m2); // m1 ^ m2 ^ m2 == m1
    for (int y = 0; y < i_xor.height(); ++y)
        for (int x = 0; x < i_xor.width(); ++x)
            for (unsigned int c = 0; c < i_xor.channels(); ++c)
                BOOST_CHECK_EQUAL(i_xor.boolAt(x, y, c), x > borders[c] && x < borders[c+1]);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_xor));

    // move other xor
    clone = m1.clone();
    shared = clone.sharedCopy();
    i_xor = i_xor.bitwise_xor(std::move(clone)); // m1 ^ m1 == 0
    for (int y = 0; y < i_xor.height(); ++y)
        for (int x = 0; x < i_xor.width(); ++x)
            for (unsigned int c = 0; c < i_xor.channels(); ++c)
                BOOST_CHECK_EQUAL(i_xor.boolAt(x, y, c), false);
    // check that memory location is the one from clone
    BOOST_CHECK(shared.isSharedWith(i_xor));



    // copy not
    Image i_not = m1.bitwise_not();
    for (int y = 0; y < i_not.height(); ++y)
        for (int x = 0; x < i_not.width(); ++x)
            for (unsigned int c = 0; c < i_not.channels(); ++c)
                BOOST_CHECK_EQUAL(!i_not.boolAt(x, y, c), x > borders[c] && x < borders[c+1]);

    // move self not
    shared = i_not.sharedCopy();
    i_not = std::move(i_not).bitwise_not(); // !!m1 == m1
    for (int y = 0; y < i_not.height(); ++y)
        for (int x = 0; x < i_not.width(); ++x)
            for (unsigned int c = 0; c < i_not.channels(); ++c)
                BOOST_CHECK_EQUAL(i_not.boolAt(x, y, c), x > borders[c] && x < borders[c+1]);
    // check that memory location did not change
    BOOST_CHECK(shared.isSharedWith(i_not));
}


// test createSingleChannelMaskFromRange and createMultiChannelMaskFromRange
BOOST_AUTO_TEST_CASE(create_masks_from_range)
{
    constexpr int width  = 6;
    constexpr int height = 5;
    Image i1{width, height, Type::uint16x5};
    for (int y = 0; y < i1.height(); ++y)
        for (int x = 0; x < i1.width(); ++x)
            i1.at<std::array<uint16_t,5>>(x, y) = {
                    (uint16_t)x,
                    (uint16_t)y,
                    (uint16_t)(x + y),
                    (uint16_t)(i1.width() - 1 - x),
                    (uint16_t)(i1.height() - 1 - y)};

    Interval singleClosed = Interval::closed(1.5, 3); // [2, 3]
    Interval singleOpen   = Interval::open(-0.5, 3.5);  // [0, 3]

    std::vector<Interval> multi{
        Interval::closed(0, 3),
        Interval::closed(1, 4),
        Interval::closed(3, 3),
        Interval::closed(4, 6),
        Interval::closed(4, 6)
    };

    // single channel from single bound, bitwise and
    Image single_bound_single_mask1 = i1.createSingleChannelMaskFromRange({singleClosed});
    Image single_bound_single_mask5 = i1.createSingleChannelMaskFromRange({singleClosed, singleOpen, singleClosed, singleOpen, singleClosed});
    BOOST_CHECK(single_bound_single_mask1.type() == Type::uint8x1);
    BOOST_CHECK(single_bound_single_mask5.type() == Type::uint8x1);
    for (int y = 0; y < single_bound_single_mask1.height(); ++y) {
        for (int x = 0; x < single_bound_single_mask1.width(); ++x) {
            double l = singleClosed.lower();
            double u = singleClosed.upper();
            BOOST_CHECK_EQUAL(single_bound_single_mask1.boolAt(x, y, 0),
                              x              >= l && x              <= u
                           && y              >= l && y              <= u
                           && x + y          >= l && x + y          <= u
                           && width - 1 - x  >= l && width - 1 - x  <= u
                           && height - 1 - y >= l && height - 1 - y <= u);
            BOOST_CHECK_EQUAL(single_bound_single_mask5.boolAt(x, y, 0),
                              x              >= l && x              <= u
                           && y              >= 0 && y              <= 3
                           && x + y          >= l && x + y          <= u
                           && width - 1 - x  >= 0 && width - 1 - x  <= 3
                           && height - 1 - y >= l && height - 1 - y <= u);
        }
    }

    // single channel from single bound, bitwise or
    single_bound_single_mask1 = i1.createSingleChannelMaskFromRange({singleClosed}, false);
    single_bound_single_mask5 = i1.createSingleChannelMaskFromRange({singleClosed, singleOpen, singleClosed, singleOpen, singleClosed}, false);
    BOOST_CHECK(single_bound_single_mask1.type() == Type::uint8x1);
    BOOST_CHECK(single_bound_single_mask5.type() == Type::uint8x1);
    for (int y = 0; y < single_bound_single_mask1.height(); ++y) {
        for (int x = 0; x < single_bound_single_mask1.width(); ++x) {
            double l = singleClosed.lower();
            double u = singleClosed.upper();
            BOOST_CHECK_EQUAL(single_bound_single_mask1.boolAt(x, y, 0),
                              (x              >= l && x              <= u)
                           || (y              >= l && y              <= u)
                           || (x + y          >= l && x + y          <= u)
                           || (width - 1 - x  >= l && width - 1 - x  <= u)
                           || (height - 1 - y >= l && height - 1 - y <= u));
            BOOST_CHECK_EQUAL(single_bound_single_mask5.boolAt(x, y, 0),
                              (x              >= l && x              <= u)
                           || (y              >= 0 && y              <= 3)
                           || (x + y          >= l && x + y          <= u)
                           || (width - 1 - x  >= 0 && width - 1 - x  <= 3)
                           || (height - 1 - y >= l && height - 1 - y <= u));
        }
    }

    // single channel from multiple bounds, bitwise and
    Image multiple_bound_single_mask = i1.createSingleChannelMaskFromRange(multi);
    BOOST_CHECK(multiple_bound_single_mask.type() == Type::uint8x1);
    for (int y = 0; y < multiple_bound_single_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_single_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_single_mask.boolAt(x, y, 0),
                              x              >= multi[0].lower() && x              <= multi[0].upper()
                           && y              >= multi[1].lower() && y              <= multi[1].upper()
                           && x + y          >= multi[2].lower() && x + y          <= multi[2].upper()
                           && width - 1 - x  >= multi[3].lower() && width - 1 - x  <= multi[3].upper()
                           && height - 1 - y >= multi[4].lower() && height - 1 - y <= multi[4].upper());
        }
    }

    // single channel from multiple bounds, bitwise or
    multiple_bound_single_mask = i1.createSingleChannelMaskFromRange(multi, false);
    BOOST_CHECK(multiple_bound_single_mask.type() == Type::uint8x1);
    for (int y = 0; y < multiple_bound_single_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_single_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_single_mask.boolAt(x, y, 0),
                              (x              >= multi[0].lower() && x              <= multi[0].upper())
                           || (y              >= multi[1].lower() && y              <= multi[1].upper())
                           || (x + y          >= multi[2].lower() && x + y          <= multi[2].upper())
                           || (width - 1 - x  >= multi[3].lower() && width - 1 - x  <= multi[3].upper())
                           || (height - 1 - y >= multi[4].lower() && height - 1 - y <= multi[4].upper()));
        }
    }

    // multi channel from single bound
    Image single_bound_multi_mask1 = i1.createMultiChannelMaskFromRange({singleClosed});
    Image single_bound_multi_mask5 = i1.createMultiChannelMaskFromRange({singleClosed, singleOpen, singleClosed, singleOpen, singleClosed});
    BOOST_CHECK(single_bound_multi_mask1.type() == Type::uint8x5);
    BOOST_CHECK(single_bound_multi_mask5.type() == Type::uint8x5);
    for (int y = 0; y < single_bound_multi_mask1.height(); ++y) {
        for (int x = 0; x < single_bound_multi_mask1.width(); ++x) {
            double l = singleClosed.lower();
            double u = singleClosed.upper();
            BOOST_CHECK_EQUAL(single_bound_multi_mask1.boolAt(x, y, 0), x              >= l && x              <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask1.boolAt(x, y, 1), y              >= l && y              <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask1.boolAt(x, y, 2), x + y          >= l && x + y          <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask1.boolAt(x, y, 3), width - 1 - x  >= l && width - 1 - x  <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask1.boolAt(x, y, 4), height - 1 - y >= l && height - 1 - y <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask5.boolAt(x, y, 0), x              >= l && x              <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask5.boolAt(x, y, 1), y              >= 0 && y              <= 3);
            BOOST_CHECK_EQUAL(single_bound_multi_mask5.boolAt(x, y, 2), x + y          >= l && x + y          <= u);
            BOOST_CHECK_EQUAL(single_bound_multi_mask5.boolAt(x, y, 3), width - 1 - x  >= 0 && width - 1 - x  <= 3);
            BOOST_CHECK_EQUAL(single_bound_multi_mask5.boolAt(x, y, 4), height - 1 - y >= l && height - 1 - y <= u);
        }
    }

    // multi channel from multi bound
    Image multiple_bound_multi_mask = i1.createMultiChannelMaskFromRange(multi);
    BOOST_CHECK(multiple_bound_multi_mask.type() == Type::uint8x5);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x              >= multi[0].lower() && x              <= multi[0].upper());
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 1), y              >= multi[1].lower() && y              <= multi[1].upper());
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), x + y          >= multi[2].lower() && x + y          <= multi[2].upper());
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 3), width - 1 - x  >= multi[3].lower() && width - 1 - x  <= multi[3].upper());
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 4), height - 1 - y >= multi[4].lower() && height - 1 - y <= multi[4].upper());
        }
    }

    // test exceptions
    std::vector<Interval> zeroRanges;
    BOOST_CHECK_THROW(i1.createMultiChannelMaskFromRange(zeroRanges), image_type_error);
    BOOST_CHECK_THROW(i1.createSingleChannelMaskFromRange(zeroRanges), image_type_error);
    std::vector<Interval> twoRanges{
        Interval::closed(0, 3),
        Interval::closed(3, 3),
    };
    BOOST_CHECK_THROW(i1.createMultiChannelMaskFromRange(twoRanges), image_type_error);
    BOOST_CHECK_THROW(i1.createSingleChannelMaskFromRange(twoRanges), image_type_error);
}


BOOST_AUTO_TEST_CASE(create_masks_from_set)
{
    Image uint8x1_img{20, 1, Type::uint8x1};
    Image uint8x2_img{20, 1, Type::uint8x2};
    Image uint8x5_img{20, 1, Type::uint8x5};
//    Image float32x1_img{20, 1, Type::float32x1}; // test float when open bounds are supported
//    Image float32x2_img{20, 1, Type::float32x2};
    for (int x = 0; x < uint8x1_img.width(); ++x) {
        uint8x1_img.at<uint8_t>(x, 0) = x;
        uint8x2_img.at<std::array<uint8_t,2>>(x, 0) = {
                (uint8_t)x,
                (uint8_t)(uint8x2_img.width() - 1 - x)};
        uint8x5_img.at<std::array<uint8_t,5>>(x, 0) = {
                (uint8_t)(1*x),
                (uint8_t)(2*x),
                (uint8_t)(3*x),
                (uint8_t)(4*x),
                (uint8_t)(5*x)};
//        float32x1_img.at<float>(x, 0) = x / 100.;
//        float32x2_img.at<std::array<float,2>>(x, 0) = {
//                (float)(x / 100.),
//                (float)((float32x2_img.width() - 1 - x) / 100.)};
    }

    // single set: (2,5] ∪ [14.5,17.5) ∪ (30, 40] ∪ [45, inf) for int image equivalent to [3,5] ∪ [15,17] ∪ [31, 40] ∪ [45, inf)
    //
    // two-chan-img, channel 0:   0  1 (2  3  4  5] 6  7  8  9 10 11 12 13 14[15 16 17 18)19
    // two-chan-img, channel 1:  19(18 17 16 15]14 13 12 11 10  9  8  7  6 [5  4  3  2) 1  0
    // mask 0:                    0  0  0  1  1  1  0  0  0  0  0  0  0  0  0  1  1  1  0  0
    // mask 1:                    0  0  1  1  1  0  0  0  0  0  0  0  0  0  1  1  1  0  0  0
    //                            __________________________________________________________
    // mask 0 & 1:                0  0  0  1  1  0  0  0  0  0  0  0  0  0  0  1  1  0  0  0
    // mask 0 | 1:                0  0  1  1  1  1  0  0  0  0  0  0  0  0  1  1  1  1  0  0
    //
    // five-chan-img, channel 0:  0  1  2 [3  4  5] 6  7  8  9 10 11 12 13 14[15 16 17]18 19
    // five-chan-img, channel 1:  0  2 [4] 6  8 10 12 14[16]18 20 22 24 26 28 30[32 34 36 38]
    // five-chan-img, channel 2:  0 [3] 6  9 12[15]18 21 24 27 30[33 36 39]42[45 48 51 54 57]
    // five-chan-img, channel 3:  0 [4] 8 12[16]20 24 28[32 36 40]44[48 52 56 60 64 68 72 78]
    // five-chan-img, channel 4:  0 [5]10[15]20 25 30[35 40 45 50 55 60 65 70 75 80 85 90 95]
    // mask 0:                    0  0  0  1  1  1  0  0  0  0  0  0  0  0  0  1  1  1  0  0
    // mask 1:                    0  0  1  0  0  0  0  0  1  0  0  0  0  0  0  0  1  1  1  1
    // mask 2:                    0  1  0  0  0  1  0  0  0  0  0  1  1  1  0  1  1  1  1  1
    // mask 3:                    0  1  0  0  1  0  0  0  1  1  1  0  1  1  1  1  1  1  1  1
    // mask 4:                    0  1  0  1  0  0  0  1  1  1  1  1  1  1  1  1  1  1  1  1
    //                            __________________________________________________________
    // mask 0 & 1 & 2 & 3 & 4:    0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  1  1  0  0
    // mask 0 | 1 | 2 | 3 | 4:    0  1  1  1  1  1  0  1  1  1  1  1  1  1  1  1  1  1  1  1
    constexpr double inf = std::numeric_limits<double>::infinity();
    IntervalSet single_int;
    single_int += Interval::left_open(2,3);        // +(2,3]
    single_int += Interval::closed(3,4);           // +[3,4]
    single_int += Interval::left_open(4.5,6);      // +(4.5,6]
    single_int -= Interval::left_open(5,6);        // -(5,6]
    single_int += Interval::right_open(14.5,17.5); // +[14.5, 17.5)
    single_int += Interval::open(30, inf);         // +(30, inf)
    single_int -= Interval::open(40, 45);          // -(40, 45)
                                                   // = (2,5] ∪ [14.5,17.5) ∪ (30, 40] ∪ [45, inf) for int image equivalent to [3,5] ∪ [15,17] ∪ [31, 40] ∪ [45, inf)

    Image uint8x1_mask_and         = uint8x1_img.createSingleChannelMaskFromSet({single_int});
    Image uint8x2a_single_mask_and = uint8x2_img.createSingleChannelMaskFromSet({single_int});
    Image uint8x2b_single_mask_and = uint8x2_img.createSingleChannelMaskFromSet({single_int, single_int});
    Image uint8x5a_single_mask_and = uint8x5_img.createSingleChannelMaskFromSet({single_int});
    Image uint8x5b_single_mask_and = uint8x5_img.createSingleChannelMaskFromSet({single_int, single_int, single_int, single_int, single_int});
    Image uint8x1_mask_or          = uint8x1_img.createSingleChannelMaskFromSet({single_int}, false);
    Image uint8x2a_single_mask_or  = uint8x2_img.createSingleChannelMaskFromSet({single_int}, false);
    Image uint8x2b_single_mask_or  = uint8x2_img.createSingleChannelMaskFromSet({single_int, single_int}, false);
    Image uint8x5a_single_mask_or  = uint8x5_img.createSingleChannelMaskFromSet({single_int}, false);
    Image uint8x5b_single_mask_or  = uint8x5_img.createSingleChannelMaskFromSet({single_int, single_int, single_int, single_int, single_int}, false);
    Image uint8x2_multi_mask       = uint8x2_img.createMultiChannelMaskFromSet({single_int});
    Image uint8x5_multi_mask       = uint8x5_img.createMultiChannelMaskFromSet({single_int});
    for (int x = 0; x < uint8x1_img.width(); ++x) {
        BOOST_CHECK_EQUAL(uint8x1_mask_and.boolAt(x, 0, 0), (x > 2 && x <= 5) || (x >= 15 && x < 18));
        BOOST_CHECK_EQUAL(uint8x2a_single_mask_and.boolAt(x, 0, 0), x == 3 || x == 4 || x == 15 || x == 16);
        BOOST_CHECK_EQUAL(uint8x2b_single_mask_and.boolAt(x, 0, 0), x == 3 || x == 4 || x == 15 || x == 16);
        BOOST_CHECK_EQUAL(uint8x5a_single_mask_and.boolAt(x, 0, 0), x == 16 || x == 17);
        BOOST_CHECK_EQUAL(uint8x5b_single_mask_and.boolAt(x, 0, 0), x == 16 || x == 17);
        BOOST_CHECK_EQUAL(uint8x1_mask_or.boolAt(x, 0, 0), (x > 2 && x <= 5) || (x >= 15 && x < 18));
        BOOST_CHECK_EQUAL(uint8x2a_single_mask_or.boolAt(x, 0, 0), (x >= 2 && x <= 5) || (x >= 14 && x <= 17));
        BOOST_CHECK_EQUAL(uint8x2b_single_mask_or.boolAt(x, 0, 0), (x >= 2 && x <= 5) || (x >= 14 && x <= 17));
        BOOST_CHECK_EQUAL(uint8x5a_single_mask_or.boolAt(x, 0, 0), x != 0 && x != 6);
        BOOST_CHECK_EQUAL(uint8x5b_single_mask_or.boolAt(x, 0, 0), x != 0 && x != 6);
        BOOST_CHECK_EQUAL(uint8x2_multi_mask.boolAt(x, 0, 0), (x > 2 && x <= 5) || (x >= 15 && x < 18));
        BOOST_CHECK_EQUAL(uint8x2_multi_mask.boolAt(x, 0, 1), (x >= 2 && x <= 4) || (x >= 14 && x <= 16));
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 0), (x > 2 && x <= 5) || (x >= 15 && x < 18));
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 1), x == 2 || x == 8 || x >= 16);
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 2), x == 1 || x == 5 || (x >= 11 && x != 14));
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 3), x == 1 || x == 4 || (x >= 8 && x != 11));
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 4), x == 1 || x == 3 || x >= 7);
    }

    // multi set 0: (2,5] ∪ (15,18] U (30, 40] U [45, inf)
    // multi set 1: [0,3) ∪ (3,14] ∪ [16,19)
    // two-chan-img, channel 0:  0  1 (2  3  4  5] 6  7  8  9 10 11 12 13 14[15 16 17 18)19
    // two-chan-img, channel 1:(19 18 17 16]15[14 13 12 11 10  9  8  7  6  5  4 (3) 2  1  0]
    // mask 0:                   0  0  0  1  1  1  0  0  0  0  0  0  0  0  0  1  1  1  0  0
    // mask 1:                   0  1  1  1  0  1  1  1  1  1  1  1  1  1  1  1  0  1  1  1
    //                           __________________________________________________________
    // mask 0 & 1:               0  0  0  1  0  1  0  0  0  0  0  0  0  0  0  1  0  1  0  0
    // mask 0 | 1:               0  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
    IntervalSet second_int;
    second_int += Interval::open(-0.5,19); // +[0,19)
    second_int -= Interval(3);             // -3
    second_int -= Interval::open(14,16);   // -(14,16)
                                           // = [0,3) ∪ (3,14] ∪ [16,19)

    uint8x2a_single_mask_and = uint8x2_img.createSingleChannelMaskFromSet({single_int, second_int});
    uint8x2a_single_mask_or  = uint8x2_img.createSingleChannelMaskFromSet({single_int, second_int}, false);
    uint8x2_multi_mask       = uint8x2_img.createMultiChannelMaskFromSet({single_int, second_int});
    for (int x = 0; x < uint8x1_img.width(); ++x) {
        BOOST_CHECK_EQUAL(uint8x2a_single_mask_and.boolAt(x, 0, 0), x == 3 || x == 5 || x == 15 || x == 17);
        BOOST_CHECK_EQUAL(uint8x2a_single_mask_or.boolAt(x, 0, 0), x != 0);
        BOOST_CHECK_EQUAL(uint8x2_multi_mask.boolAt(x, 0, 0), (x > 2 && x <= 5) || (x >= 15 && x < 18));
        BOOST_CHECK_EQUAL(uint8x2_multi_mask.boolAt(x, 0, 1), x != 0 && x != 4 && x != 16);
    }

    // multi set 0: (2,5] ∪ (15,18] U (30, 40] U [45, inf)
    // multi set 1: [0,3) ∪ (3,14] ∪ [16,19)
    // multi set 2: [0,30)
    // multi set 3: [44,60)
    // multi set 4: (20,70]
    // five-chan-img, channel 0:  0  1  2 [3  4  5] 6  7  8  9 10 11 12 13 14[15 16 17]18 19
    // five-chan-img, channel 1: [0  2  4  6  8 10 12 14 16 18]20 22 24 26 28 30 32 34 36 38
    // five-chan-img, channel 2: [0  3  6  9 12 15 18 21 24 27]30 33 36 39 42 45 48 51 54 57
    // five-chan-img, channel 3:  0  4  8 12 16 20 24 28 32 36 40[44 48 52 56]60 64 68 72 78
    // five-chan-img, channel 4:  0  5 10 15 20[25 30 35 40 45 50 55 60 65 70]75 80 85 90 95
    // mask 0:                    0  0  0  1  1  1  0  0  0  0  0  0  0  0  0  1  1  1  0  0
    // mask 1:                    1  1  1  1  1  1  1  1  1  1  0  0  0  0  0  0  0  0  0  0
    // mask 2:                    1  1  1  1  1  1  1  1  1  1  0  0  0  0  0  0  0  0  0  0
    // mask 3:                    0  0  0  0  0  0  0  0  0  0  0  1  1  1  1  0  0  0  0  0
    // mask 4:                    0  0  0  0  0  1  1  1  1  1  1  1  1  1  1  0  0  0  0  0
    //                            __________________________________________________________
    // mask 0 & 1 & 2 & 3 & 4:    0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0
    // mask 0 | 1 | 2 | 3 | 4:    1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  0  0
    IntervalSet third_int;
    third_int += Interval::right_open(0, 30);
    IntervalSet forth_int;
    forth_int += Interval::right_open(44, 60);
    IntervalSet fifth_int;
    fifth_int += Interval::left_open(20, 70);

    uint8x5a_single_mask_and = uint8x5_img.createSingleChannelMaskFromSet({single_int, second_int, third_int, forth_int, fifth_int});
    uint8x5a_single_mask_or  = uint8x5_img.createSingleChannelMaskFromSet({single_int, second_int, third_int, forth_int, fifth_int}, false);
    uint8x5_multi_mask       = uint8x5_img.createMultiChannelMaskFromSet({single_int, second_int, third_int, forth_int, fifth_int});
    for (int x = 0; x < uint8x1_img.width(); ++x) {
        BOOST_CHECK_EQUAL(uint8x5a_single_mask_and.boolAt(x, 0, 0), false);
        BOOST_CHECK_EQUAL(uint8x5a_single_mask_or.boolAt(x, 0, 0), x <= 17);
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 0), (x > 2 && x <= 5) || (x >= 15 && x < 18));
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 1), x <= 9);
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 2), x <= 9);
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 3), x >= 11 && x <= 14);
        BOOST_CHECK_EQUAL(uint8x5_multi_mask.boolAt(x, 0, 4), x >= 5 && x <= 14);
    }

    // test exceptions
    std::vector<IntervalSet> zeroRanges;
    BOOST_CHECK_THROW(uint8x2_img.createMultiChannelMaskFromSet(zeroRanges), image_type_error);
    BOOST_CHECK_THROW(uint8x5_img.createMultiChannelMaskFromSet(zeroRanges), image_type_error);
    BOOST_CHECK_THROW(uint8x2_img.createMultiChannelMaskFromSet({single_int, single_int, single_int}), image_type_error);
    BOOST_CHECK_THROW(uint8x5_img.createMultiChannelMaskFromSet({single_int, single_int, single_int}), image_type_error);
}

// test limit cases of createSingleChannelMaskFromRange and createMultiChannelMaskFromRange
BOOST_AUTO_TEST_CASE(create_masks_limit_cases)
{
    Image multiple_bound_multi_mask;
    Image i_int{5, 6, Type::uint16x3};
    Image i_float{5, 6, Type::float32x3};
    Image i_double{5, 6, Type::float64x3};
    for (int y = 0; y < i_int.height(); ++y) {
        for (int x = 0; x < i_int.width(); ++x) {
            i_int.at<std::array<uint16_t,3>>(x, y) = {
                    (uint16_t)x,
                    (uint16_t)(i_int.width() - 1 - x),
                    (uint16_t)y};
            i_float.at<std::array<float,3>>(x, y) = {
                    (float)x,
                    // choose some values that would not fit into a 32 bit integer type, to confirm that comparisons are done in float or double
                    (float)(std::numeric_limits<uint32_t>::max()) + 1e10f * (i_float.width() - 1 - x),
                    (float)y};
            i_double.at<std::array<double,3>>(x, y) = {
                    (double)x,
                    // choose some values that would not fit into float type, to confirm that comparisons are done in double
                    (double)(std::numeric_limits<float>::max()) + 1e30 * (i_double.width() - 1 - x),
                    (double)y};
        }
    }

    std::vector<double> multiLow{ 0, 3, 1};
    std::vector<double> multiHigh{3, 3, 4};
    std::vector<Interval> multi;
    bool allTrue;
    bool allFalse;

    // check bounds for integer
    // should work, even when OpenCV's inRange is directly used
    multiLow[1]  = std::numeric_limits<int32_t>::min();
    multiHigh[1] = std::numeric_limits<int32_t>::max();
    multi = std::vector<Interval>{Interval::closed(multiLow[0], multiHigh[0]), Interval::closed(multiLow[1], multiHigh[1]), Interval::closed(multiLow[2], multiHigh[2])};
    allTrue = true;
    multiple_bound_multi_mask = i_int.createMultiChannelMaskFromRange(multi);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x >= multiLow[0] && x <= multiHigh[0]);
            allTrue &= multiple_bound_multi_mask.boolAt(x, y, 1);
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), y >= multiLow[2] && y <= multiHigh[2]);
        }
    }
    BOOST_CHECK(allTrue);

    // should work, since we limit the values to int32 range in get*MaskFromRange
    multiLow[1]  = -std::numeric_limits<double>::infinity();
    multiHigh[1] = (double)(std::numeric_limits<int32_t>::max()) + 1;
    multi = std::vector<Interval>{Interval::closed(multiLow[0], multiHigh[0]), Interval::closed(multiLow[1], multiHigh[1]), Interval::closed(multiLow[2], multiHigh[2])};
    allTrue = true;
    multiple_bound_multi_mask = i_int.createMultiChannelMaskFromRange(multi);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x >= multiLow[0] && x <= multiHigh[0]);
            allTrue &= multiple_bound_multi_mask.boolAt(x, y, 1);
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), y >= multiLow[2] && y <= multiHigh[2]);
        }
    }
    BOOST_CHECK(allTrue);

    // check bounds for float
    // everything is accepted, lowest, max, and infinity all of float or double
    multiLow[1]  = std::numeric_limits<double>::lowest();
//    multiLow[1]  = std::numeric_limits<float>::lowest();
//    multiHigh[1] = std::numeric_limits<float>::max();
//    multiLow[1]  = -std::numeric_limits<float>::infinity();
//    multiHigh[1] =  std::numeric_limits<float>::infinity();
//    multiLow[1]  = -std::numeric_limits<double>::infinity();
    multiHigh[1] =  std::numeric_limits<double>::infinity();
    multi = std::vector<Interval>{Interval::closed(multiLow[0], multiHigh[0]), Interval::closed(multiLow[1], multiHigh[1]), Interval::closed(multiLow[2], multiHigh[2])};
    allTrue = true;
    multiple_bound_multi_mask = i_float.createMultiChannelMaskFromRange(multi);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x >= multiLow[0] && x <= multiHigh[0]);
            allTrue &= multiple_bound_multi_mask.boolAt(x, y, 1);
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), y >= multiLow[2] && y <= multiHigh[2]);
        }
    }
    BOOST_CHECK(allTrue);

    // check bounds for double
    // everything is accepted, lowest, max, and infinity of float or double
    multiLow[1]  = std::numeric_limits<double>::min();
    multiHigh[1] = std::numeric_limits<double>::max();
    multiLow[1]  = -std::numeric_limits<double>::infinity();
    multiHigh[1] =  std::numeric_limits<double>::infinity();
    multi = std::vector<Interval>{Interval::closed(multiLow[0], multiHigh[0]), Interval::closed(multiLow[1], multiHigh[1]), Interval::closed(multiLow[2], multiHigh[2])};
    allTrue = true;
    multiple_bound_multi_mask = i_double.createMultiChannelMaskFromRange(multi);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x >= multiLow[0] && x <= multiHigh[0]);
            allTrue &= multiple_bound_multi_mask.boolAt(x, y, 1);
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), y >= multiLow[2] && y <= multiHigh[2]);
        }
    }
    BOOST_CHECK(allTrue);

    // check NaN for double and int
    // no value is between NaN
    multiLow[1]  = std::numeric_limits<double>::quiet_NaN();
    multiHigh[1] = std::numeric_limits<double>::quiet_NaN();
    multi = std::vector<Interval>{Interval::closed(multiLow[0], multiHigh[0]), Interval::closed(multiLow[1], multiHigh[1]), Interval::closed(multiLow[2], multiHigh[2])};
    allFalse = true;
    multiple_bound_multi_mask = i_double.createMultiChannelMaskFromRange(multi);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x >= multiLow[0] && x <= multiHigh[0]);
            allFalse &= multiple_bound_multi_mask.boolAt(x, y, 1) == false;
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), y >= multiLow[2] && y <= multiHigh[2]);
        }
    }
    BOOST_CHECK(allFalse);

    allFalse = true;
    allTrue = true;
    multiple_bound_multi_mask = i_int.createMultiChannelMaskFromRange(multi);
    for (int y = 0; y < multiple_bound_multi_mask.height(); ++y) {
        for (int x = 0; x < multiple_bound_multi_mask.width(); ++x) {
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 0), x >= multiLow[0] && x <= multiHigh[0]);
            allFalse &= multiple_bound_multi_mask.boolAt(x, y, 1) == false;
            allTrue  &= multiple_bound_multi_mask.boolAt(x, y, 1) == true;
            BOOST_CHECK_EQUAL(multiple_bound_multi_mask.boolAt(x, y, 2), y >= multiLow[2] && y <= multiHigh[2]);
        }
    }
    BOOST_CHECK(allFalse);
    BOOST_CHECK(!allTrue);
}

// test copyValuesFrom copies the values but leaves the memory independent
BOOST_AUTO_TEST_CASE(copyValuesFrom) {
    // single channel case
    // make two images
    Image i1c1_16{5, 6, Type::uint16x1};
    Image i2c1_16{5, 6, Type::uint16x1};

    // set the values in the first one
    for (int x = 0; x < i1c1_16.width(); ++x)
        for (int y = 0; y < i1c1_16.height(); ++y)
            i1c1_16.at<uint16_t>(x, y) = 10 * y + x;

    // copy to the second
    i2c1_16.copyValuesFrom(i1c1_16);
    for (int x = 0; x < i2c1_16.width(); ++x)
        for (int y = 0; y < i2c1_16.height(); ++y)
            BOOST_CHECK_EQUAL(i2c1_16.at<uint16_t>(x, y), 10 * y + x);

    // change the second
    for (int x = 0; x < i2c1_16.width(); ++x)
        for (int y = 0; y < i2c1_16.height(); ++y)
            i2c1_16.at<uint16_t>(x, y) = 20 * y + x;

    // check independency
    for (int x = 0; x < i2c1_16.width(); ++x) {
        for (int y = 0; y < i2c1_16.height(); ++y) {
            BOOST_CHECK_EQUAL(i1c1_16.at<uint16_t>(x, y), 10 * y + x);
            BOOST_CHECK_EQUAL(i2c1_16.at<uint16_t>(x, y), 20 * y + x);
        }
    }



    // multi channel case
    // make two images
    Image i1c5_16{5, 6, Type::uint16x5};
    Image i2c5_16{5, 6, Type::uint16x5};

    // set the values in the first one
    for (int x = 0; x < i1c5_16.width(); ++x) {
        for (int y = 0; y < i1c5_16.height(); ++y) {
            i1c5_16.at<std::array<uint16_t,5>>(x, y) = {
                    static_cast<uint16_t>(10 * y + x),
                    static_cast<uint16_t>(15 * y + x),
                    static_cast<uint16_t>(20 * y + x),
                    static_cast<uint16_t>(25 * y + x),
                    static_cast<uint16_t>(30 * y + x)};
        }
    }

    // copy to the second
    i2c5_16.copyValuesFrom(i1c5_16);
    for (int x = 0; x < i2c5_16.width(); ++x) {
        for (int y = 0; y < i2c5_16.height(); ++y) {
            auto pixel = i2c5_16.at<std::array<uint16_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * y + x);
            BOOST_CHECK_EQUAL(pixel[1], 15 * y + x);
            BOOST_CHECK_EQUAL(pixel[2], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel[3], 25 * y + x);
            BOOST_CHECK_EQUAL(pixel[4], 30 * y + x);
        }
    }

    // change the second
    for (int x = 0; x < i2c5_16.width(); ++x) {
        for (int y = 0; y < i2c5_16.height(); ++y) {
            i2c5_16.at<std::array<uint16_t,5>>(x, y) = {
                    static_cast<uint16_t>(11 * y + x),
                    static_cast<uint16_t>(16 * y + x),
                    static_cast<uint16_t>(21 * y + x),
                    static_cast<uint16_t>(26 * y + x),
                    static_cast<uint16_t>(31 * y + x)};
        }
    }

    // check independency
    for (int x = 0; x < i2c5_16.width(); ++x) {
        for (int y = 0; y < i2c5_16.height(); ++y) {
            auto pixel1 = i1c5_16.at<std::array<uint16_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel1[0], 10 * y + x);
            BOOST_CHECK_EQUAL(pixel1[1], 15 * y + x);
            BOOST_CHECK_EQUAL(pixel1[2], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel1[3], 25 * y + x);
            BOOST_CHECK_EQUAL(pixel1[4], 30 * y + x);
            auto pixel2 = i2c5_16.at<std::array<uint16_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel2[0], 11 * y + x);
            BOOST_CHECK_EQUAL(pixel2[1], 16 * y + x);
            BOOST_CHECK_EQUAL(pixel2[2], 21 * y + x);
            BOOST_CHECK_EQUAL(pixel2[3], 26 * y + x);
            BOOST_CHECK_EQUAL(pixel2[4], 31 * y + x);
        }
    }
}


// test that a cloned image has the same values until one is changed (independent memory)
BOOST_AUTO_TEST_CASE(clone_independency) {
    // make image and set some values
    constexpr int total_height = 5;
    constexpr int total_width  = 6;
    Image i1_8x1{total_width, total_height, Type::uint8x1};
    for (int x = 0; x < i1_8x1.width(); ++x)
        for (int y = 0; y < i1_8x1.height(); ++y)
            i1_8x1.at<uint8_t>(x, y) = 10 * y + x;

    // clone it and verify, that it has the same properties and values
    Image i2_8x1 = i1_8x1.clone();
    BOOST_CHECK_EQUAL(i1_8x1.width(),    i2_8x1.width());
    BOOST_CHECK_EQUAL(i1_8x1.height(),   i2_8x1.height());
    BOOST_CHECK_EQUAL(i1_8x1.channels(), i2_8x1.channels());
    BOOST_CHECK      (i1_8x1.type()   == i2_8x1.type());
    for (int x = 0; x < i2_8x1.width(); ++x)
        for (int y = 0; y < i2_8x1.height(); ++y)
            BOOST_CHECK_EQUAL(i2_8x1.at<uint8_t>(x, y), 10 * y + x);

    // change the cloned image
    for (int x = 0; x < i2_8x1.width(); ++x)
        for (int y = 0; y < i2_8x1.height(); ++y)
            i2_8x1.at<uint8_t>(x, y) = 20 * y + x;

    // verify that the changed values in the cloned image and did not change in the original image
    for (int x = 0; x < i2_8x1.width(); ++x) {
        for (int y = 0; y < i2_8x1.height(); ++y) {
            BOOST_CHECK_EQUAL(i2_8x1.at<uint8_t>(x, y), 20 * y + x);
            BOOST_CHECK_EQUAL(i1_8x1.at<uint8_t>(x, y), 10 * y + x);
        }
    }


    // for a multichannel image only verify, that the values for all channels were cloned
    Image i1_8x5{total_width, total_height, Type::uint8x5};
    for (int x = 0; x < i1_8x5.width(); ++x)
        for (int y = 0; y < i1_8x5.height(); ++y)
            i1_8x5.at<std::array<uint8_t,5>>(x, y) = {
                    static_cast<uint8_t>(10 * y + x),
                    static_cast<uint8_t>(15 * y + x),
                    static_cast<uint8_t>(20 * y + x),
                    static_cast<uint8_t>(25 * y + x),
                    static_cast<uint8_t>(30 * y + x)};

    Image i2_8x5 = i1_8x5.clone();
    BOOST_CHECK_EQUAL(i1_8x5.width(),    i2_8x5.width());
    BOOST_CHECK_EQUAL(i1_8x5.height(),   i2_8x5.height());
    BOOST_CHECK_EQUAL(i1_8x5.channels(), i2_8x5.channels());
    BOOST_CHECK      (i1_8x5.type()   == i2_8x5.type());
    for (int x = 0; x < i2_8x5.width(); ++x) {
        for (int y = 0; y < i2_8x5.height(); ++y) {
            auto pixel = i2_8x5.at<std::array<uint8_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * y + x);
            BOOST_CHECK_EQUAL(pixel[1], 15 * y + x);
            BOOST_CHECK_EQUAL(pixel[2], 20 * y + x);
            BOOST_CHECK_EQUAL(pixel[3], 25 * y + x);
            BOOST_CHECK_EQUAL(pixel[4], 30 * y + x);
        }
    }


    // check that getCropWindow gives the original size for an uncropped image
    Rectangle nocrop = i1_8x1.getCropWindow();
    BOOST_CHECK_EQUAL(nocrop.height, total_height);
    BOOST_CHECK_EQUAL(nocrop.width,  total_width);
    BOOST_CHECK_EQUAL(nocrop.x,      0);
    BOOST_CHECK_EQUAL(nocrop.y,      0);

    // check that cropped images clone the full memory, not only the cropped region
    constexpr int x_off  = 1;
    constexpr int y_off  = 2;
    constexpr int width  = 3;
    constexpr int height = 2;
    i1_8x1.crop(Rectangle{x_off, y_off, width, height});
    Image i3_8x1 = i1_8x1.clone();
    BOOST_CHECK_EQUAL(i3_8x1.width(),  width);
    BOOST_CHECK_EQUAL(i3_8x1.height(), height);

    Rectangle crop = i3_8x1.getCropWindow();
    BOOST_CHECK_EQUAL(crop.height, height);
    BOOST_CHECK_EQUAL(crop.width,  width);
    BOOST_CHECK_EQUAL(crop.x,      x_off);
    BOOST_CHECK_EQUAL(crop.y,      y_off);

    Size size = i3_8x1.getOriginalSize();
    BOOST_CHECK_EQUAL(size.height, total_height);
    BOOST_CHECK_EQUAL(size.width,  total_width);

    i3_8x1.uncrop();
    BOOST_CHECK_EQUAL(i3_8x1.height(), total_height);
    BOOST_CHECK_EQUAL(i3_8x1.width(),  total_width);

    // check that making a cropped clone changes the original size and correct values
    Image croppedClone = i1_8x5.clone(Rectangle{x_off, y_off, width, height});
    size = croppedClone.getOriginalSize();
    BOOST_CHECK_EQUAL(size.height, height);
    BOOST_CHECK_EQUAL(size.width,  width);
    for (int x = 0; x < croppedClone.width(); ++x) {
        for (int y = 0; y < croppedClone.height(); ++y) {
            auto pixel = croppedClone.at<std::array<uint8_t,5>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 10 * (y + y_off) + x + x_off);
            BOOST_CHECK_EQUAL(pixel[1], 15 * (y + y_off) + x + x_off);
            BOOST_CHECK_EQUAL(pixel[2], 20 * (y + y_off) + x + x_off);
            BOOST_CHECK_EQUAL(pixel[3], 25 * (y + y_off) + x + x_off);
            BOOST_CHECK_EQUAL(pixel[4], 30 * (y + y_off) + x + x_off);
        }
    }
}

// test shared copy. copied image should be dependent on the original image! A shared copy of a cropped image should have same size and original size.
BOOST_AUTO_TEST_CASE(copy_dependency) {
    // make image and set some values
    Image i1_8{5, 6, Type::uint8x1};
    for (int x = 0; x < i1_8.width(); ++x)
        for (int y = 0; y < i1_8.height(); ++y)
            i1_8.at<uint8_t>(x, y) = 10 * y + x;

    // make a shared copy and verify, that it has the same properties and values
    Image i2_8{i1_8.sharedCopy()};
    BOOST_CHECK_EQUAL(i1_8.width(),    i2_8.width());
    BOOST_CHECK_EQUAL(i1_8.height(),   i2_8.height());
    BOOST_CHECK_EQUAL(i1_8.channels(), i2_8.channels());
    BOOST_CHECK      (i1_8.type()   == i2_8.type());
    BOOST_CHECK      (i1_8.isSharedWith(i2_8));
    for (int x = 0; x < i2_8.width(); ++x)
        for (int y = 0; y < i2_8.height(); ++y)
            BOOST_CHECK_EQUAL(i2_8.at<uint8_t>(x, y), 10 * y + x);

    // change the copied image
    for (int x = 0; x < i2_8.width(); ++x)
        for (int y = 0; y < i2_8.height(); ++y)
            i2_8.at<uint8_t>(x, y) = 20 * y + x;

    // verify that the changed values in the copied and in the original image
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x, y), 20 * y + x);
            BOOST_CHECK_EQUAL(i2_8.at<uint8_t>(x, y), 20 * y + x);
        }
    }

    // check that a shared copy of a cropped image has the same size and original size.
    i1_8.crop(Rectangle{ /* x */ 1, /* y */ 2, /* width */ 1, /* height */ 2});
    ConstImage i3_8 = i1_8.sharedCopy();
    BOOST_CHECK      (i1_8.isSharedWith(i3_8));
    BOOST_CHECK      (i2_8.isSharedWith(i3_8));
    BOOST_CHECK_EQUAL(i1_8.width(),  i3_8.width());
    BOOST_CHECK_EQUAL(i1_8.height(), i3_8.height());
    Size orig1 = i1_8.getOriginalSize();
    Size orig3 = i3_8.getOriginalSize();
    BOOST_CHECK_EQUAL(orig1.width,  orig3.width);
    BOOST_CHECK_EQUAL(orig1.height, orig3.height);

//    std::cout << "Crop window: " << i1_8.getCropWindow() << std::endl;
//    std::cout << "Orig Size: " << i1_8.getOriginalSize() << std::endl;
//    std::cout << "Offset: " << Point{i1_8.getCropWindow().x, i1_8.getCropWindow().y} << std::endl;
}


// test nested crop with a shared copy.
BOOST_AUTO_TEST_CASE(nested_crop) {
    // make image and copy
    constexpr int total_height = 5;
    constexpr int total_width  = 6;
    Image i1_8{total_width, total_height, Type::uint8x1};
    Image i2_8{i1_8.sharedCopy()};

    // now crop the copied image
    // original x indices: 0|1 2 3|4 5, y indices: 0 1|2 3|4
    //  cropped x indices:  |0 1 2|   , y indices:    |0 1|
    constexpr int x_off1  = 1;
    constexpr int y_off1  = 2;
    constexpr int width1  = 3;
    constexpr int height1 = 2;
    i2_8.crop(Rectangle{x_off1, y_off1, width1, height1});
    BOOST_CHECK_EQUAL(i2_8.width(),  width1);
    BOOST_CHECK_EQUAL(i2_8.height(), height1);

    Rectangle crop1 = i2_8.getCropWindow();
    BOOST_CHECK_EQUAL(crop1.height, height1);
    BOOST_CHECK_EQUAL(crop1.width,  width1);
    BOOST_CHECK_EQUAL(crop1.x,      x_off1);
    BOOST_CHECK_EQUAL(crop1.y,      y_off1);

    Size size1 = i2_8.getOriginalSize();
    BOOST_CHECK_EQUAL(size1.height, total_height);
    BOOST_CHECK_EQUAL(size1.width,  total_width);

    // verify that the memory is still dependent
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            i2_8.at<uint8_t>(x, y) = 10 * y + x;
            int x_with_off = x + x_off1;
            int y_with_off = y + y_off1;
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x_with_off, y_with_off), 10 * y + x);
        }
    }

    // now crop the cropped image
    //    original x indices: 0 1|2 3|4 5, y indices: 0 1 2|3|4
    //     cropped x indices:   0|1 2|   , y indices:     0|1|
    // new cropped x indices:    |0 1|   , y indices:      |0|
    constexpr int x_off2  = 1;
    constexpr int y_off2  = 1;
    constexpr int width2  = 2;
    constexpr int height2 = 1;
    i2_8.crop(Rectangle{x_off2, y_off2, width2, height2});
    BOOST_CHECK_EQUAL(i2_8.width(),  width2);
    BOOST_CHECK_EQUAL(i2_8.height(), height2);

    Rectangle crop2 = i2_8.getCropWindow();
    BOOST_CHECK_EQUAL(crop2.height, height2);
    BOOST_CHECK_EQUAL(crop2.width,  width2);
    BOOST_CHECK_EQUAL(crop2.x,      x_off1 + x_off2);
    BOOST_CHECK_EQUAL(crop2.y,      y_off1 + y_off2);

    Size size2 = i2_8.getOriginalSize();
    BOOST_CHECK_EQUAL(size2.height, total_height);
    BOOST_CHECK_EQUAL(size2.width,  total_width);

    // verify that the memory is still dependent
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            i2_8.at<uint8_t>(x, y) = 20 * y + x;
            int x_with_off = x + x_off1 + x_off2;
            int y_with_off = y + y_off1 + y_off2;
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x_with_off, y_with_off), 20 * y + x);
        }
    }

    // test uncrop
    i2_8.uncrop();
    BOOST_CHECK_EQUAL(i2_8.width(),  total_width);
    BOOST_CHECK_EQUAL(i2_8.height(), total_height);
}


// test that making a zero sized image throws an exception
BOOST_AUTO_TEST_CASE(zero_size_crop) {
    constexpr int total_height = 5;
    constexpr int total_width  = 6;
    Image img{total_width, total_height, Type::uint8x1};

    // test unsupported zero size construction
    BOOST_CHECK_THROW((Image{0, 0, Type::uint8}), size_error);

    // test crop with height 0 or width 0 throws exception and does not change the image
    BOOST_CHECK_THROW(img.crop(Rectangle{0, 0, /* width */ 0, total_height}), size_error);
    BOOST_CHECK_THROW(img.crop(Rectangle{0, 0, total_width, /* height */ 0}), size_error);
    BOOST_CHECK_EQUAL(img.width(),  total_width);
    BOOST_CHECK_EQUAL(img.height(), total_height);

    // now crop to width 1 and height 1 and reduce borders
    // top left
    img.crop(Rectangle{0, 0, /* width */ 1, /* height */ 1});
    BOOST_CHECK_THROW(img.moveCropWindow(-1, 0), size_error);
    BOOST_CHECK_THROW(img.moveCropWindow(0, -1), size_error);
    // bottom right
    img.uncrop();
    img.crop(Rectangle{total_width-1, total_height-1, /* width */ 1, /* height */ 1});
    BOOST_CHECK_THROW(img.moveCropWindow(1, 0), size_error);
    BOOST_CHECK_THROW(img.moveCropWindow(0, 1), size_error);
    // reduce directly
    BOOST_CHECK_THROW(img.adjustCropBorders(-1, 0, 0, 0), size_error);
    BOOST_CHECK_THROW(img.adjustCropBorders(0, -1, 0, 0), size_error);
    BOOST_CHECK_THROW(img.adjustCropBorders(0, 0, -1, 0), size_error);
    BOOST_CHECK_THROW(img.adjustCropBorders(0, 0, 0, -1), size_error);
}


// test crop with a too large window.
BOOST_AUTO_TEST_CASE(oversize_crop) {
    // make image and copy
    Image i1_8{6, 5, Type::uint8x1};
    Image i2_8{i1_8.sharedCopy()};

    // crop the copied image and check, that it limited the width and height
    // original x indices: 0|1 2 3 4 5|, y indices: 0 1|2 3 4|
    //  cropped x indices:  |0 1 2 3 4|, y indices:    |0 1 2|
    constexpr int x_off  = 1;
    constexpr int y_off  = 2;
    constexpr int width  = 100;
    constexpr int height = 100;
    BOOST_REQUIRE_NO_THROW(i2_8.crop(Rectangle{x_off, y_off, width, height}));
    BOOST_CHECK_EQUAL(i2_8.width(),  i1_8.width()  - x_off);
    BOOST_CHECK_EQUAL(i2_8.height(), i1_8.height() - y_off);

    // verify that the memory is still dependent
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            i2_8.at<uint8_t>(x, y) = 10 * y + x;
            int x_with_off = x + x_off;
            int y_with_off = y + y_off;
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x_with_off, y_with_off), 10 * y + x);
        }
    }

    // adjust the crop borders
    //    original x indices:|0 1 2 3 4|5, y indices: 0|1 2|3 4
    //     cropped x indices:|  0 1 2 3|4, y indices:  |  0|1 2
    // new cropped x indices:|0 1 2 3 4| , y indices:  |0 1|
    constexpr int extendTop    =  1;
    constexpr int extendBottom = -2;
    constexpr int extendLeft   =  1;
    constexpr int extendRight  = -1;
    i2_8.adjustCropBorders(extendTop, extendBottom, extendLeft, extendRight);
    BOOST_CHECK_EQUAL(i2_8.width(),  i1_8.width()  - x_off + extendLeft   + extendRight);
    BOOST_CHECK_EQUAL(i2_8.height(), i1_8.height() - y_off + extendBottom + extendTop);

    // verify that the memory is still dependent
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            i2_8.at<uint8_t>(x, y) = 20 * y + x;
            int x_with_off = x + x_off - extendLeft;
            int y_with_off = y + y_off - extendTop;
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x_with_off, y_with_off), 20 * y + x);
        }
    }

    // adjust the crop borders too much
    //    original x indices:|0 1 2 3|4 5, y indices:|0|1 2 3 4
    //     cropped x indices:|  0 1 2|3 4, y indices:| |  0 1 2
    //     cropped x indices:|0 1 2 3|4  , y indices:| |0 1
    // new cropped x indices:|0 1 2 3|   , y indices:|0|
    i2_8.adjustCropBorders(extendTop, extendBottom, extendLeft, extendRight);
    BOOST_CHECK_EQUAL(i2_8.width(),  4);
    BOOST_CHECK_EQUAL(i2_8.height(), 1);

    // verify that the memory is still dependent
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            i2_8.at<uint8_t>(x, y) = 30 * y + x;
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x, y), 30 * y + x);
        }
    }

    // negative offset, should be set to zero and adjust the size accordingly
    i2_8.uncrop();
    BOOST_REQUIRE_NO_THROW(i2_8.crop(Rectangle{-1, -2, 2, 4}));
    BOOST_CHECK_EQUAL(i2_8.width(),  1);

    BOOST_CHECK_EQUAL(i2_8.height(), 2);
    // verify that the memory is still dependent
    for (int x = 0; x < i2_8.width(); ++x) {
        for (int y = 0; y < i2_8.height(); ++y) {
            i2_8.at<uint8_t>(x, y) = 10 * y + x;
            BOOST_CHECK_EQUAL(i1_8.at<uint8_t>(x, y), 10 * y + x);
        }
    }

}


// test sub pixel crop correctness
BOOST_AUTO_TEST_CASE(sub_pixel_crop) {
    constexpr int total_width  = 6;
    constexpr int total_height = 5;
    const Coordinate offsetx{ 0.4, 0  };
    const Coordinate offsety{ 0  , 0.4};
    const Coordinate offsetxy{0.4, 0.3};
    const Size size{total_width - 1, total_height - 1};
    Image i_orig;

    // test with integer type
    i_orig = Image{total_width, total_height, Type::uint8x3};
    for (int x = 0; x < i_orig.width(); ++x)
        for (int y = 0; y < i_orig.height(); ++y)
            i_orig.at<std::array<uint8_t,3>>(x, y) = {
                    static_cast<uint8_t>(10 * y + 2 * x),
                    static_cast<uint8_t>(20 * y + 2 * x),
                    static_cast<uint8_t>(30 * y + 2 * x)};

    Image i_x  = i_orig.clone(offsetx,  size);
    Image i_y  = i_orig.clone(offsety,  size);
    Image i_xy = i_orig.clone(offsetxy, size);
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            auto pixel_tl = i_orig.at<std::array<uint8_t,3>>(x,   y);
            auto pixel_tr = i_orig.at<std::array<uint8_t,3>>(x+1, y);
            auto pixel_bl = i_orig.at<std::array<uint8_t,3>>(x,   y+1);
            auto pixel_br = i_orig.at<std::array<uint8_t,3>>(x+1, y+1);

            auto pixel_x  = i_x.at<std::array<uint8_t,3>>(x, y);
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_x[0]), static_cast<int>(std::round(pixel_tl[0] * 0.6 + pixel_tr[0] * 0.4)));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_x[1]), static_cast<int>(std::round(pixel_tl[1] * 0.6 + pixel_tr[1] * 0.4)));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_x[2]), static_cast<int>(std::round(pixel_tl[2] * 0.6 + pixel_tr[2] * 0.4)));

            auto pixel_y  = i_y.at<std::array<uint8_t,3>>(x, y);
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_y[0]), static_cast<int>(std::round(pixel_tl[0] * 0.6 + pixel_bl[0] * 0.4)));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_y[1]), static_cast<int>(std::round(pixel_tl[1] * 0.6 + pixel_bl[1] * 0.4)));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_y[2]), static_cast<int>(std::round(pixel_tl[2] * 0.6 + pixel_bl[2] * 0.4)));

            auto pixel_xy = i_xy.at<std::array<uint8_t,3>>(x, y);
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_xy[0]), static_cast<int>(std::round(pixel_tl[0] * 0.6 * 0.7 + pixel_tr[0] * 0.4 * 0.7 + pixel_bl[0] * 0.6 * 0.3 + pixel_br[0] * 0.4 * 0.3)));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_xy[1]), static_cast<int>(std::round(pixel_tl[1] * 0.6 * 0.7 + pixel_tr[1] * 0.4 * 0.7 + pixel_bl[1] * 0.6 * 0.3 + pixel_br[1] * 0.4 * 0.3)));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel_xy[2]), static_cast<int>(std::round(pixel_tl[2] * 0.6 * 0.7 + pixel_tr[2] * 0.4 * 0.7 + pixel_bl[2] * 0.6 * 0.3 + pixel_br[2] * 0.4 * 0.3)));
        }
    }

    // test with floating point type
    i_orig = Image{total_width, total_height, Type::float32x3};
    for (int x = 0; x < i_orig.width(); ++x)
        for (int y = 0; y < i_orig.height(); ++y)
            i_orig.at<std::array<float,3>>(x, y) = {
                    static_cast<float>(0.101 * y + 0.11 * x),
                    static_cast<float>(0.201 * y + 0.11 * x),
                    static_cast<float>(0.301 * y + 0.11 * x)};

    i_x  = i_orig.clone(offsetx,  size);
    i_y  = i_orig.clone(offsety,  size);
    i_xy = i_orig.clone(offsetxy, size);
    for (int x = 0; x < size.width; ++x) {
        for (int y = 0; y < size.height; ++y) {
            auto pixel_tl = i_orig.at<std::array<float,3>>(x,   y);
            auto pixel_tr = i_orig.at<std::array<float,3>>(x+1, y);
            auto pixel_bl = i_orig.at<std::array<float,3>>(x,   y+1);
            auto pixel_br = i_orig.at<std::array<float,3>>(x+1, y+1);

            auto pixel_x  = i_x.at<std::array<float,3>>(x, y);
            BOOST_CHECK_CLOSE_FRACTION(pixel_x[0], pixel_tl[0] * 0.6 + pixel_tr[0] * 0.4, 1e-7);
            BOOST_CHECK_CLOSE_FRACTION(pixel_x[1], pixel_tl[1] * 0.6 + pixel_tr[1] * 0.4, 1e-7);
            BOOST_CHECK_CLOSE_FRACTION(pixel_x[2], pixel_tl[2] * 0.6 + pixel_tr[2] * 0.4, 1e-7);

            auto pixel_y  = i_y.at<std::array<float,3>>(x, y);
            BOOST_CHECK_CLOSE_FRACTION(pixel_y[0], pixel_tl[0] * 0.6 + pixel_bl[0] * 0.4, 1e-7);
            BOOST_CHECK_CLOSE_FRACTION(pixel_y[1], pixel_tl[1] * 0.6 + pixel_bl[1] * 0.4, 1e-7);
            BOOST_CHECK_CLOSE_FRACTION(pixel_y[2], pixel_tl[2] * 0.6 + pixel_bl[2] * 0.4, 1e-7);

            auto pixel_xy = i_xy.at<std::array<float,3>>(x, y);
            BOOST_CHECK_CLOSE_FRACTION(pixel_xy[0], pixel_tl[0] * 0.6 * 0.7 + pixel_tr[0] * 0.4 * 0.7 + pixel_bl[0] * 0.6 * 0.3 + pixel_br[0] * 0.4 * 0.3, 1e-7);
            BOOST_CHECK_CLOSE_FRACTION(pixel_xy[1], pixel_tl[1] * 0.6 * 0.7 + pixel_tr[1] * 0.4 * 0.7 + pixel_bl[1] * 0.6 * 0.3 + pixel_br[1] * 0.4 * 0.3, 1e-7);
            BOOST_CHECK_CLOSE_FRACTION(pixel_xy[2], pixel_tl[2] * 0.6 * 0.7 + pixel_tr[2] * 0.4 * 0.7 + pixel_bl[2] * 0.6 * 0.3 + pixel_br[2] * 0.4 * 0.3, 1e-7);
        }
    }

}


// test iterators
BOOST_AUTO_TEST_CASE(iterators) {
    constexpr int x_off  = 1;
    constexpr int y_off  = 2;
    constexpr int width  = 3;
    constexpr int height = 2;
    int i;

    // test single channel image
    Image ic1_16{5, 6, Type::uint16x1};

    // write something with channel iterators, check reading x and y
    i = 0;
    for (auto it1 = ic1_16.begin<uint16_t>(0), it1_end = ic1_16.end<uint16_t>(0); it1 != it1_end; ++it1) {
        BOOST_CHECK_EQUAL(it1.getX(),     i % ic1_16.width());
        BOOST_CHECK_EQUAL(it1.getPos().x, i % ic1_16.width());
        BOOST_CHECK_EQUAL(it1.getY(),     i / ic1_16.width());
        BOOST_CHECK_EQUAL(it1.getPos().y, i / ic1_16.width());
        *it1 = 2 * i++;
    }

    // verify
    for (int x = 0; x < ic1_16.width(); ++x)
        for (int y = 0; y < ic1_16.height(); ++y)
            BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), 2 * (x + y * ic1_16.width()));

    // set x and y of channel iterator and write something to [2,3]x[1,2]
    {
        auto it1 = ic1_16.begin<uint16_t>(0);
        it1.setPos(Point{2,1});
        *it1++ = 42;
        *it1++ = 42;
        it1.setX(2);
        it1.setY(2);
        *it1++ = 42;
        *it1++ = 42;
    }

    // verify
    for (int y = 0; y < ic1_16.height(); ++y)
        for (int x = 0; x < ic1_16.width(); ++x)
            if ((x == 2 || x == 3) && (y == 1 || y == 2))
                BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), 42);
            else
                BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), 2 * (x + y * ic1_16.width()));

    // write something with pixel iterators
    i = 0;
    for (auto it1 = ic1_16.begin<uint16_t>(), it1_end = ic1_16.end<uint16_t>(); it1 != it1_end; ++it1)
        *it1 = i++;

    // verify
    for (int x = 0; x < ic1_16.width(); ++x)
        for (int y = 0; y < ic1_16.height(); ++y)
            BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), x + y * ic1_16.width());

    // do the same with a cropped image
    ic1_16.crop(Rectangle{x_off, y_off, width, height});
    // channel iterator
    i = 0;
    for (auto it1 = ic1_16.begin<uint16_t>(0), it1_end = ic1_16.end<uint16_t>(0); it1 != it1_end; ++it1)
        *it1 = 2 * i++;

    for (int x = 0; x < ic1_16.width(); ++x)
        for (int y = 0; y < ic1_16.height(); ++y)
            BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), 2 * (x + y * ic1_16.width()));
    // pixel iterator
    i = 0;
    for (auto it1 = ic1_16.begin<uint16_t>(), it1_end = ic1_16.end<uint16_t>(); it1 != it1_end; ++it1)
        *it1 = i++;

    for (int x = 0; x < ic1_16.width(); ++x)
        for (int y = 0; y < ic1_16.height(); ++y)
            BOOST_CHECK_EQUAL(ic1_16.at<uint16_t>(x, y), x + y * ic1_16.width());

    // check that begin + w*h == end
    BOOST_CHECK(ic1_16.begin<uint16_t>()  + width * height == ic1_16.end<uint16_t>());
    BOOST_CHECK(ic1_16.begin<uint16_t>(0) + width * height == ic1_16.end<uint16_t>(0));

    // check that begin + w*h -1 == end-1
    BOOST_CHECK(ic1_16.begin<uint16_t>()  + width * height - 1 == ic1_16.end<uint16_t>()  - 1);
    BOOST_CHECK(ic1_16.begin<uint16_t>(0) + width * height - 1 == ic1_16.end<uint16_t>(0) - 1);

    // check that end-begin is w*h
    BOOST_CHECK_EQUAL(std::distance(ic1_16.begin<uint16_t>(),  ic1_16.end<uint16_t>()),  width * height);
    BOOST_CHECK_EQUAL(std::distance(ic1_16.begin<uint16_t>(0), ic1_16.end<uint16_t>(0)), width * height);
    BOOST_CHECK_EQUAL(ic1_16.end<uint16_t>()  - ic1_16.begin<uint16_t>(),  width * height);
    BOOST_CHECK_EQUAL(ic1_16.end<uint16_t>(0) - ic1_16.begin<uint16_t>(0), width * height);

    // check that (begin+1)-begin is 1
    BOOST_CHECK_EQUAL(std::distance(ic1_16.begin<uint16_t>(),  ic1_16.begin<uint16_t>()  + 1), 1);
    BOOST_CHECK_EQUAL(std::distance(ic1_16.begin<uint16_t>(0), ic1_16.begin<uint16_t>(0) + 1), 1);



    // test the same with a multi channel image
    Image ic3_16{5, 6, Type::uint16x3};

    // write something with channel iterators
    constexpr int red_off   = 0;
    constexpr int green_off = 50;
    constexpr int blue_off  = 100;
    i = 0;
    for (auto it1 = ic3_16.begin<uint16_t>(0), it1_end = ic3_16.end<uint16_t>(0); it1 != it1_end; ++it1) {
        BOOST_CHECK_EQUAL(it1.getX(), i % ic3_16.width());
        BOOST_CHECK_EQUAL(it1.getY(), i / ic3_16.width());
        *it1 = i++ + red_off;
    }
    i = 0;
    for (auto it1 = ic3_16.begin<uint16_t>(1), it1_end = ic3_16.end<uint16_t>(1); it1 != it1_end; ++it1)
        *it1 = i++ + green_off;
    i = 0;
    for (auto it1 = ic3_16.begin<uint16_t>(2), it1_end = ic3_16.end<uint16_t>(2); it1 != it1_end; ++it1)
        *it1 = i++ + blue_off;

    // verify
    for (int x = 0; x < ic3_16.width(); ++x) {
        for (int y = 0; y < ic3_16.height(); ++y) {
            auto pixel = ic3_16.at<std::array<uint16_t,3>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], x + y * ic3_16.width() + red_off);
            BOOST_CHECK_EQUAL(pixel[1], x + y * ic3_16.width() + green_off);
            BOOST_CHECK_EQUAL(pixel[2], x + y * ic3_16.width() + blue_off);
        }
    }

    // set x and y of channel iterator and write something to [2,3]x[1,2]
    {
        auto it = ic3_16.begin<uint16_t>(1);
        it.setX(2);
        it.setY(1);
        *it++ = 42;
        *it++ = 42;
        it.setX(2);
        it.setY(2);
        *it++ = 42;
        *it++ = 42;
    }

    // verify
    for (int y = 0; y < ic3_16.height(); ++y)
        for (int x = 0; x < ic3_16.width(); ++x)
            if ((x == 2 || x == 3) && (y == 1 || y == 2))
                BOOST_CHECK_EQUAL(ic3_16.at<uint16_t>(x, y, 1), 42);
            else
                BOOST_CHECK_EQUAL(ic3_16.at<uint16_t>(x, y, 1), x + y * ic3_16.width() + green_off);
    // write something with pixel iterators
    i = 0;
    for (auto it = ic3_16.begin<std::array<uint16_t,3>>(), it_end = ic3_16.end<std::array<uint16_t,3>>(); it != it_end; ++it) {
        (*it)[0] = 2*i + red_off;
        (*it)[1] = 2*i + green_off;
        (*it)[2] = 2*i + blue_off;
        ++i;
    }

    // verify
    for (int x = 0; x < ic3_16.width(); ++x) {
        for (int y = 0; y < ic3_16.height(); ++y) {
            auto pixel = ic3_16.at<std::array<uint16_t,3>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 2 * (x + y * ic3_16.width()) + red_off);
            BOOST_CHECK_EQUAL(pixel[1], 2 * (x + y * ic3_16.width()) + green_off);
            BOOST_CHECK_EQUAL(pixel[2], 2 * (x + y * ic3_16.width()) + blue_off);
        }
    }

    // do the same with a cropped image
    ic3_16.crop(Rectangle{x_off, y_off, width, height});
    // channel iterators
    i = 0;
    for (auto it1 = ic3_16.begin<uint16_t>(0), it1_end = ic3_16.end<uint16_t>(0); it1 != it1_end; ++it1)
        *it1 = i++ + red_off;
    i = 0;
    for (auto it1 = ic3_16.begin<uint16_t>(1), it1_end = ic3_16.end<uint16_t>(1); it1 != it1_end; ++it1)
        *it1 = i++ + green_off;
    i = 0;
    for (auto it1 = ic3_16.begin<uint16_t>(2), it1_end = ic3_16.end<uint16_t>(2); it1 != it1_end; ++it1)
        *it1 = i++ + blue_off;

    for (int x = 0; x < ic3_16.width(); ++x) {
        for (int y = 0; y < ic3_16.height(); ++y) {
            auto pixel = ic3_16.at<std::array<uint16_t,3>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], x + y * ic3_16.width() + red_off);
            BOOST_CHECK_EQUAL(pixel[1], x + y * ic3_16.width() + green_off);
            BOOST_CHECK_EQUAL(pixel[2], x + y * ic3_16.width() + blue_off);
        }
    }
    // pixel iterators
    i = 0;
    for (auto it = ic3_16.begin<std::array<uint16_t,3>>(), it_end = ic3_16.end<std::array<uint16_t,3>>(); it != it_end; ++it) {
        (*it)[0] = 2*i + red_off;
        (*it)[1] = 2*i + green_off;
        (*it)[2] = 2*i + blue_off;
        ++i;
    }

    for (int x = 0; x < ic3_16.width(); ++x) {
        for (int y = 0; y < ic3_16.height(); ++y) {
            auto pixel = ic3_16.at<std::array<uint16_t,3>>(x, y);
            BOOST_CHECK_EQUAL(pixel[0], 2 * (x + y * ic3_16.width()) + red_off);
            BOOST_CHECK_EQUAL(pixel[1], 2 * (x + y * ic3_16.width()) + green_off);
            BOOST_CHECK_EQUAL(pixel[2], 2 * (x + y * ic3_16.width()) + blue_off);
        }
    }

    // check that begin + w*h == end
    BOOST_CHECK(ic3_16.begin<uint16_t>()  + width * height == ic3_16.end<uint16_t>());
    BOOST_CHECK(ic3_16.begin<uint16_t>(0) + width * height == ic3_16.end<uint16_t>(0));
    BOOST_CHECK(ic3_16.begin<uint16_t>(1) + width * height == ic3_16.end<uint16_t>(1));
    BOOST_CHECK(ic3_16.begin<uint16_t>(2) + width * height == ic3_16.end<uint16_t>(2));

    // check that begin + w*h -1 == end-1
    BOOST_CHECK(ic3_16.begin<uint16_t>()  + width * height - 1 == ic3_16.end<uint16_t>()  - 1);
    BOOST_CHECK(ic3_16.begin<uint16_t>(0) + width * height - 1 == ic3_16.end<uint16_t>(0) - 1);
    BOOST_CHECK(ic3_16.begin<uint16_t>(1) + width * height - 1 == ic3_16.end<uint16_t>(1) - 1);
    BOOST_CHECK(ic3_16.begin<uint16_t>(2) + width * height - 1 == ic3_16.end<uint16_t>(2) - 1);

    // check that end-begin is w*h
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(),  ic3_16.end<uint16_t>()),  width * height);
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(0), ic3_16.end<uint16_t>(0)), width * height);
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(1), ic3_16.end<uint16_t>(1)), width * height);
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(2), ic3_16.end<uint16_t>(2)), width * height);
    BOOST_CHECK_EQUAL(ic3_16.end<uint16_t>()  - ic3_16.begin<uint16_t>(),  width * height);
    BOOST_CHECK_EQUAL(ic3_16.end<uint16_t>(0) - ic3_16.begin<uint16_t>(0), width * height);
    BOOST_CHECK_EQUAL(ic3_16.end<uint16_t>(1) - ic3_16.begin<uint16_t>(1), width * height);
    BOOST_CHECK_EQUAL(ic3_16.end<uint16_t>(2) - ic3_16.begin<uint16_t>(2), width * height);

    // check that (begin+1)-begin is 1
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(),  ic3_16.begin<uint16_t>()  + 1), 1);
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(0), ic3_16.begin<uint16_t>(0) + 1), 1);
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(1), ic3_16.begin<uint16_t>(1) + 1), 1);
    BOOST_CHECK_EQUAL(std::distance(ic3_16.begin<uint16_t>(2), ic3_16.begin<uint16_t>(2) + 1), 1);
}


// test const iterators
BOOST_AUTO_TEST_CASE(iterator_constness) {
    // make image and set some values
    Image img{5, 6, Type::uint16x1};
    Image const& const_img = img;
    int i = 0;
    for (auto it = img.begin<uint16_t>(), it_end = img.end<uint16_t>(); it != it_end; ++it)
        *it = i++;

    // use const iterators to verify
    i = 0;
    for (auto it = const_img.begin<uint16_t>(), it_end = const_img.end<uint16_t>(); it != it_end; ++it)
        BOOST_CHECK_EQUAL(*it, i++);

    i = 0;
    for (auto it = const_img.begin<uint16_t>(0), it_end = const_img.end<uint16_t>(0); it != it_end; ++it)
        BOOST_CHECK_EQUAL(*it, i++);
}


// test warping
BOOST_AUTO_TEST_CASE(warp_single_channel) {
    /* stripe data:
     * 1   2   4   8  16  32  64 128  64  32  16   8   4   2   1
     * 1   2   4   8  16  32  64 128  64  32  16   8   4   2   1
     */
    Image stripe(15, 2, Type::uint8x1);
    for (int x = 0; x < (stripe.width() + 1) / 2; ++x) {
        stripe.at<uint8_t>(x, 0) = std::pow(2, x);
        stripe.at<uint8_t>(x, 1) = std::pow(2, x);
        stripe.at<uint8_t>(stripe.width() - 1 - x, 0) = std::pow(2, x);
        stripe.at<uint8_t>(stripe.width() - 1 - x, 1) = std::pow(2, x);
    }

    GeoInfo giSrc;
    giSrc.geotransSRS.SetWellKnownGeogCS("WGS84");
    giSrc.geotrans.offsetX = 0;
    giSrc.geotrans.offsetY = 0;
    giSrc.geotrans.XToX = 2;
    giSrc.geotrans.YToY = 2;

    GeoInfo giDst = giSrc;

    Image warped;
    // simple identity warp
    giDst.size = Size{0, 0};
    warped = stripe.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), stripe.channels());
    BOOST_CHECK_EQUAL(warped.type(), stripe.type());
    BOOST_REQUIRE_EQUAL(warped.size(), stripe.size());
    for (int y = 0; y < stripe.height(); ++y)
        for (int x = 0; x < stripe.width(); ++x)
            BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), stripe.at<uint8_t>(x, y));

    // set size
    giDst.size = Size{6, 1};
    warped = stripe.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), stripe.channels());
    BOOST_CHECK_EQUAL(warped.type(), stripe.type());
    BOOST_REQUIRE_EQUAL(warped.size(), giDst.size);
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), stripe.at<uint8_t>(x, y));

    // translation warp by 1 pixel
    giDst.geotrans.offsetX = 2;
    giDst.size = Size{0, 0};
    warped = stripe.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), stripe.channels());
    BOOST_CHECK_EQUAL(warped.type(), stripe.type());
    BOOST_REQUIRE_EQUAL(warped.height(), stripe.height());
    BOOST_REQUIRE_EQUAL(warped.width(), stripe.width() - 1);
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), stripe.at<uint8_t>(x + 1, y));

    // translation warp by 1 pixel with nodata value
    giDst.geotrans.offsetX = 2;
    giSrc.setNodataValue(16);
    giDst.setNodataValue(255);
    giDst.size = Size{0, 0};
    warped = stripe.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), stripe.channels());
    BOOST_CHECK_EQUAL(warped.type(), stripe.type());
    BOOST_REQUIRE_EQUAL(warped.height(), stripe.height());
    BOOST_REQUIRE_EQUAL(warped.width(), stripe.width() - 1);
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            if (stripe.at<uint8_t>(x + 1, y) != 16)
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), stripe.at<uint8_t>(x + 1, y));
            else
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), 255);

    // translation warp by 0.25 pixel with nodata value
    giDst.size = Size{14, 2};
    giDst.geotrans.offsetX = 0.5;
    giSrc.setNodataValue(16);
    giDst.setNodataValue(255);
    warped = stripe.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), stripe.channels());
    BOOST_CHECK_EQUAL(warped.type(), stripe.type());
    BOOST_REQUIRE_EQUAL(warped.size(), giDst.size);
    Image exp{giDst.size, Type::float64x1};
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            if (stripe.at<uint8_t>(x, y) == 16) {
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), 255);
                exp.at<double>(x, y) = 255;
            }
            else if (stripe.at<uint8_t>(x + 1, y) == 16) {
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), stripe.at<uint8_t>(x, y));
                exp.at<double>(x, y) = stripe.at<uint8_t>(x, y);
            }
            else {
                double expected = 0.75 * stripe.at<uint8_t>(x, y) + 0.25 * stripe.at<uint8_t>(x + 1, y);
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), std::round(expected)); // precision is bad: a diff of 2 occurs!
                exp.at<double>(x, y) = expected;
            }
//    std::cout << "Source:\n" << stripe.cvMat() << std::endl << std::endl;
//    std::cout << "Expected:\n" << exp.cvMat() << std::endl << std::endl;
//    std::cout << "Warped:\n" << warped.cvMat() << std::endl << std::endl;


    // translation warp by 0.75 pixel with nodata value
    giDst.size = Size{14, 2};
    giDst.geotrans.offsetX = 1.5;
    giSrc.setNodataValue(16);
    giDst.setNodataValue(255);
    warped = stripe.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), stripe.channels());
    BOOST_CHECK_EQUAL(warped.type(), stripe.type());
    BOOST_REQUIRE_EQUAL(warped.size(), giDst.size);
    exp = Image{giDst.size, Type::float64x1};
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            if (stripe.at<uint8_t>(x, y) == 16) {
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), stripe.at<uint8_t>(x + 1, y));
                exp.at<double>(x, y) = stripe.at<uint8_t>(x + 1, y);
            }
            else if (stripe.at<uint8_t>(x + 1, y) == 16) {
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), 255);
                exp.at<double>(x, y) = 255;
            }
            else {
                double expected = 0.25 * stripe.at<uint8_t>(x, y) + 0.75 * stripe.at<uint8_t>(x + 1, y);
                BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y), std::round(expected)); // precision is bad: a diff of 2 occurs!
                exp.at<double>(x, y) = expected;
            }
//    std::cout << "Source:\n" << stripe.cvMat() << std::endl << std::endl;
//    std::cout << "Expected:\n" << exp.cvMat() << std::endl << std::endl;
//    std::cout << "Warped:\n" << warped.cvMat() << std::endl << std::endl;
}

BOOST_AUTO_TEST_CASE(warp_multi_channel) {
    /* simple multi-channel image
     * chan 0:  chan 1:
     *  0 100    0 100
     *  0 100    0 100
     */
    Image img(2, 2, Type::uint8x2);
    img.at<uint8_t>(0, 0, 0) = 0;
    img.at<uint8_t>(0, 0, 1) = 0;
    img.at<uint8_t>(0, 1, 0) = 0;
    img.at<uint8_t>(0, 1, 1) = 0;
    img.at<uint8_t>(1, 0, 0) = 100;
    img.at<uint8_t>(1, 0, 1) = 100;
    img.at<uint8_t>(1, 1, 0) = 100;
    img.at<uint8_t>(1, 1, 1) = 100;

    GeoInfo giSrc;
    giSrc.geotransSRS.SetWellKnownGeogCS("WGS84");
    giSrc.geotrans.offsetX = 0;
    giSrc.geotrans.offsetY = 0;
    giSrc.geotrans.XToX = 2;
    giSrc.geotrans.YToY = 2;
    giSrc.setNodataValue(0);

    GeoInfo giDst = giSrc;
    giDst.setNodataValue(255);

    Image warped;
    Size singlePix{1,1};

    // simple full size identity warp
    giDst.geotrans.offsetX = 0;
    giDst.geotrans.offsetY = 0;
    giDst.size = Size{0, 0};
    warped = img.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), img.size());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            for (unsigned int c = 0; c < img.channels(); ++c)
                if (img.at<uint8_t>(x, y, c) == 0)
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), 255);
                else
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), img.at<uint8_t>(x, y, c));

    // simple 1 pixel warp, shift 0.25 pixels
    giDst.geotrans.offsetX = 0.5;
    giDst.geotrans.offsetY = 0.5;
    giDst.size = singlePix;
    warped = img.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), singlePix);
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 0), 255); // although the coordinate is within the nodata value pixel the other neighbor 100 is chosen. At least it does not interpolate with the nodata value
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 1), 255); // although the coordinate is within the nodata value pixel the other neighbor 100 is chosen. At least it does not interpolate with the nodata value

    // simple 1 pixel warp, shift 0.75 pixels
    giDst.geotrans.offsetX = 1.5;
    giDst.geotrans.offsetY = 1.5;
    giDst.size = singlePix;
    warped = img.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), singlePix);
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 0), 100);
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 1), 100);

    /* advanced multi-channel image
     * chan 0:  chan 1:
     *  0 100    1 100
     *  0 100    1 100
     */
    img.at<uint8_t>(0, 0, 1) = 1;
    img.at<uint8_t>(0, 1, 1) = 1;

    // advanced full size identity warp
    giDst.geotrans.offsetX = 0;
    giDst.geotrans.offsetY = 0;
    giDst.size = Size{0, 0};
    warped = img.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), img.size());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            for (unsigned int c = 0; c < img.channels(); ++c)
                if (img.at<uint8_t>(x, y, c) == 0)
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), 255);
                else
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), img.at<uint8_t>(x, y, c));
//    std::cout << "source:\n" << img.cvMat() << std::endl << std::endl;
//    std::cout << "warped:\n" << warped.cvMat() << std::endl << std::endl;

    // advanced 1 pixel warp, shift 0.25 pixels
    giDst.geotrans.offsetX = 0.5;
    giDst.geotrans.offsetY = 0.5;
    giDst.size = singlePix;
    warped = img.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), singlePix);
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 0), 255); // although the coordinate is within the nodata value pixel the other neighbor 100 is chosen. At least it does not interpolate with the nodata value
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 1), 26);
//    std::cout << "warped:" << warped.cvMat() << std::endl << std::endl;

    // advanced 1 pixel warp, shift 0.75 pixels
    giDst.geotrans.offsetX = 1.5;
    giDst.geotrans.offsetY = 1.5;
    giDst.size = singlePix;
    warped = img.warp(giSrc, giDst);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), singlePix);
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 0), 100);
    BOOST_CHECK_EQUAL(warped.at<uint8_t>(0, 0, 1), 75);
//    std::cout << "warped:" << warped.cvMat() << std::endl << std::endl;
}


// this tests warping with different nodata values per channel
BOOST_AUTO_TEST_CASE(warp_multi_channel_different_nodata_values) {
    /* simple multi-channel image
     * chan 0:  chan 1:
     *  0 100    0 100
     *  0 100    0 100
     */
    Image img(2, 2, Type::uint8x2);
    img.at<uint8_t>(0, 0, 0) = 0;
    img.at<uint8_t>(0, 0, 1) = 0;
    img.at<uint8_t>(0, 1, 0) = 0;
    img.at<uint8_t>(0, 1, 1) = 0;
    img.at<uint8_t>(1, 0, 0) = 100;
    img.at<uint8_t>(1, 0, 1) = 100;
    img.at<uint8_t>(1, 1, 0) = 100;
    img.at<uint8_t>(1, 1, 1) = 100;

    GeoInfo giSrc;
    giSrc.geotransSRS.SetWellKnownGeogCS("WGS84");
    giSrc.geotrans.offsetX = 0;
    giSrc.geotrans.offsetY = 0;
    giSrc.geotrans.XToX = 2;
    giSrc.geotrans.YToY = 2;
    giSrc.setNodataValue(0, 0);
    giSrc.setNodataValue(100, 1);

    GeoInfo giDst = giSrc;
    giDst.setNodataValue(255, 0);
    giDst.setNodataValue(255, 1);

    Image warped;
    Size singlePix{1,1};

    // simple full size identity warp
    giDst.geotrans.offsetX = 0;
    giDst.geotrans.offsetY = 0;
    giDst.size = Size{0, 0};
    warped = img.warp(giSrc, giDst, InterpMethod::nearest);

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), img.size());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            for (unsigned int c = 0; c < img.channels(); ++c)
                if (img.at<uint8_t>(x, y, c) == giSrc.getNodataValue(c))
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), 255);
                else
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), img.at<uint8_t>(x, y, c));
//    std::cout << "warped:\n" << warped.cvMat() << std::endl << std::endl;

    warped = img.warp(giSrc, giDst); // bilinear with workaround

    BOOST_CHECK_EQUAL(warped.channels(), img.channels());
    BOOST_CHECK_EQUAL(warped.type(), img.type());
    BOOST_REQUIRE_EQUAL(warped.size(), img.size());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            for (unsigned int c = 0; c < img.channels(); ++c)
                if (img.at<uint8_t>(x, y, c) == giSrc.getNodataValue(c))
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), 255);
                else
                    BOOST_CHECK_EQUAL(warped.at<uint8_t>(x, y, c), img.at<uint8_t>(x, y, c));
//    std::cout << "warped:\n" << warped.cvMat() << std::endl << std::endl;
}


BOOST_AUTO_TEST_CASE(warp_single_pixel_different_types) {
    GeoInfo giSrc;
    giSrc.geotransSRS.SetProjCS("UTM 17 (WGS84) in northern hemisphere.");
    giSrc.geotransSRS.SetWellKnownGeogCS("WGS84");
    giSrc.geotransSRS.SetUTM(17, TRUE);
    giSrc.geotrans.offsetX = 0;
    giSrc.geotrans.offsetY = 0;
    giSrc.geotrans.XToX = 2;
    giSrc.geotrans.YToY = 2;
    GeoInfo giDst = giSrc;

    Size singlePix{1,1};
    giDst.size = singlePix;
    /* simple single-channel image
     *  0     0     0
     *  0     0 x 100
     *  0     0     0
     */
    std::vector<Type> types{Type::uint8x1, Type::uint16x1, Type::int16x1, Type::int32x1, Type::float32x1, Type::float64x1};
    for (auto type : types) {
        Image img(3, 3, Type::uint8x1);
        img.set(0);
        img.at<uint8_t>(2, 1) = 100;
        img = img.convertTo(type);

        for (double offs = 0; offs <= 4.01; offs += 0.1) {
            giDst.geotrans.offsetX = offs;
            giDst.geotrans.offsetY = 2;
            Image warped = img.warp(giSrc, giDst);
            double factor = std::max((offs - 2), 0.0) / 2;
            double exp = factor * 100;
//            std::cout << "type: " << type << ", offset: " << offs << ", warped: " << warped.cvMat() << ", expected: " << exp << std::endl;
            BOOST_CHECK_LE(std::abs(warped.doubleAt(0, 0, 0) - exp), 1e-12);
        }
    }
}

// test identity warp with warping one pixel around the actual image with different nodata values
BOOST_AUTO_TEST_CASE(warp_out_of_bounds) {
    GeoInfo giSrc;
    giSrc.geotransSRS.SetProjCS("UTM 17 (WGS84) in northern hemisphere.");
    giSrc.geotransSRS.SetWellKnownGeogCS("WGS84");
    giSrc.geotransSRS.SetUTM(17, TRUE);
    giSrc.geotrans.offsetX = 0;
    giSrc.geotrans.offsetY = 0;
    giSrc.geotrans.XToX = 2;
    giSrc.geotrans.YToY = 2;
    GeoInfo giDst = giSrc;
    giDst.geotrans.offsetX = -2;
    giDst.geotrans.offsetY = -2;

    Size largerRegion{5,5};
    giDst.size = largerRegion;
    /* simple single-channel image
     *  X   X   X   X   X
     *  X   0   0   0   X
     *  X   0   0 100   X
     *  X   0   0   0   X
     *  X   X   X   X   X
     */
    Image img(3, 3, Type::uint16x1);
    img.set(0);
    img.at<uint16_t>(2, 1) = 100;
//    std::cout << "img:\n" << img.cvMat() << std::endl;

    // test different nodata values: 200 --> 255
    giSrc.setNodataValue(200);
    giDst.setNodataValue(255);
    Image warped = img.warp(giSrc, giDst);
//    std::cout << "warped with 200 --> 255:\n" << warped.cvMat() << std::endl << std::endl;
    BOOST_CHECK_EQUAL(warped.size(), largerRegion);
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            if (x >= 1 && x <= img.width() && y >= 1 && y <= img.height())
                BOOST_CHECK_EQUAL(warped.at<uint16_t>(x, y), img.at<uint16_t>(x-1, y-1));
            else
                BOOST_CHECK_EQUAL(warped.at<uint16_t>(x, y), 255);

    // test source nodata value: 200 --> ??? (==> destination will take source nodata value)
    giDst.clearNodataValues();
    warped = img.warp(giSrc, giDst);
//    std::cout << "warped with 200 --> ???:\n" << warped.cvMat() << std::endl << std::endl;
    BOOST_CHECK_EQUAL(warped.size(), largerRegion);
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            if (x >= 1 && x <= img.width() && y >= 1 && y <= img.height())
                BOOST_CHECK_EQUAL(warped.at<uint16_t>(x, y), img.at<uint16_t>(x-1, y-1));
            else
                BOOST_CHECK_EQUAL(warped.at<uint16_t>(x, y), 200);

    // test destination nodata value: ??? --> 150 (==> destination will take 0 as nodata value)
    giSrc.clearNodataValues();
    giDst.setNodataValue(150);
    warped = img.warp(giSrc, giDst);
//    std::cout << "warped with ??? --> 150:\n" << warped.cvMat() << std::endl << std::endl;
    BOOST_CHECK_EQUAL(warped.size(), largerRegion);
    for (int y = 0; y < warped.height(); ++y)
        for (int x = 0; x < warped.width(); ++x)
            if (x >= 1 && x <= img.width() && y >= 1 && y <= img.height())
                BOOST_CHECK_EQUAL(warped.at<uint16_t>(x, y), img.at<uint16_t>(x-1, y-1));
            else
                BOOST_CHECK_EQUAL(warped.at<uint16_t>(x, y), 0);
}

// make sure that at<bool> fails
// not easily testable, maybe we can use:
// http://stackoverflow.com/questions/17408824/how-to-write-runnable-tests-of-static-assert
//BOOST_AUTO_TEST_CASE(at_bool) {
//    Image img{5, 6, Type::uint8x2};
//    ConstImage cimg{5, 6, Type::uint8x2};

//    // functions
//    img.at<bool>(0,0,0);
//    img.at<bool>(0,0);
//    img.begin<bool>();
//    img.begin<bool>(0);
//    img.end<bool>();
//    img.end<bool>(0);
//    cimg.at<bool>(0,0,0);
//    cimg.at<bool>(0,0);
//    cimg.begin<bool>();
//    cimg.begin<bool>(0);
//    cimg.end<bool>();
//    cimg.end<bool>(0);

//    // types
//    img.at<bool>(0,0);
//    img.at<bool[5]>(0,0);
//    img.at<bool[]>(0,0);
//    img.at<std::array<bool,2>>(0,0);
//    img.at<cv::Scalar_<bool>>(0,0);
//    img.at<cv::Vec<bool,3>>(0,0);
//    img.at<cv::Matx<bool, 5, 7>>(0,0);
//}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

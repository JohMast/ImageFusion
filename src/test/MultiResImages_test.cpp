#include <iostream>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "MultiResImages.h"
#include "Image.h"

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(MultiResImages_Suite)

BOOST_AUTO_TEST_CASE(basic) {
    MultiResImages images;

    BOOST_CHECK_THROW(images.get(/*don't care*/ "high", /*don't care*/ 0), not_found_error);
    BOOST_CHECK_THROW(images.getAny(/*don't care*/ "high"), not_found_error);
    BOOST_CHECK_THROW(images.getAny(/*don't care*/ 0), not_found_error);
    BOOST_CHECK_THROW(images.getAny(), not_found_error);
    BOOST_CHECK_THROW(images.remove(/*don't care*/ "high", /*don't care*/ 0), not_found_error);
    BOOST_CHECK(!images.has(/*don't care*/ "high", /*don't care*/ 0));
    BOOST_CHECK(!images.has(/*don't care*/ "high"));
    BOOST_CHECK(!images.has(/*don't care*/ 0));
    BOOST_CHECK_EQUAL(images.countResolutionTags(), 0);
    BOOST_CHECK_EQUAL(images.count(), 0);
    BOOST_CHECK_EQUAL(images.count(/* don't care */ "high"), 0);
    BOOST_CHECK_EQUAL(images.count(/* don't care */ 0), 0);
    BOOST_CHECK(images.empty());
    BOOST_CHECK_EQUAL(images.getResolutionTags().size(), 0);
    BOOST_CHECK_EQUAL(images.getDates(/* don't care */ "high").size(), 0);

    // images: empty
    images.set("high", 0, Image{});
    images.set("high", 5, Image{});
    // images: high: 0, 5
    BOOST_CHECK_THROW(   images.get("low", /*don't care*/ 0), not_found_error);
    BOOST_CHECK_THROW(   images.getAny("low"), not_found_error);
    BOOST_CHECK_NO_THROW(images.getAny("high"));
    BOOST_CHECK_NO_THROW(images.getAny());
    BOOST_CHECK_NO_THROW(images.getAny(0));
    BOOST_CHECK_NO_THROW(images.getAny(5));
    BOOST_CHECK_THROW(   images.getAny(1), not_found_error);
    BOOST_CHECK_NO_THROW(images.get("high", 0));
    BOOST_CHECK_THROW(   images.get("high", 1), not_found_error);
    BOOST_CHECK_NO_THROW(images.get("high", 5));
    BOOST_CHECK( images.has("high"));
    BOOST_CHECK( images.has(0));
    BOOST_CHECK(!images.has(1));
    BOOST_CHECK(!images.has(2));
    BOOST_CHECK(!images.has(3));
    BOOST_CHECK(!images.has(4));
    BOOST_CHECK( images.has(5));
    BOOST_CHECK(!images.has(6));
    BOOST_CHECK(!images.has("high", 1));
    BOOST_CHECK(!images.has("high", 2));
    BOOST_CHECK(!images.has("high", 3));
    BOOST_CHECK(!images.has("high", 4));
    BOOST_CHECK( images.has("high", 5));
    BOOST_CHECK(!images.has("high", 6));
    BOOST_CHECK_EQUAL(images.countResolutionTags(), 1);
    BOOST_CHECK_EQUAL(images.count(),       2);
    BOOST_CHECK_EQUAL(images.count(0),      1);
    BOOST_CHECK_EQUAL(images.count(1),      0);
    BOOST_CHECK_EQUAL(images.count(3),      0);
    BOOST_CHECK_EQUAL(images.count(5),      1);
    BOOST_CHECK_EQUAL(images.count("high"), 2);
    BOOST_CHECK_EQUAL(images.count("low"),  0);
    BOOST_CHECK(!images.empty());
    BOOST_CHECK_EQUAL(images.getResolutionTags().size(), 1);
    BOOST_CHECK_EQUAL(images.getResolutionTags().at(0), "high");
    BOOST_CHECK_EQUAL(images.getDates("high").size(), 2);
    BOOST_CHECK_EQUAL(images.getDates("high").at(0), 0);
    BOOST_CHECK_EQUAL(images.getDates("high").at(1), 5);


    // images: high: 0, 5
    images.set("low",  -42, Image{});
    images.set("low", 1337, Image{});
    // images: high: 0, 5; low: -42, 1337
    BOOST_CHECK(images.has("high",    0));
    BOOST_CHECK(images.has("high",    5));
    BOOST_CHECK(images.has("low",   -42));
    BOOST_CHECK(images.has("low",  1337));
    BOOST_CHECK_EQUAL(images.getDates("low").size(), 2);
    BOOST_CHECK_EQUAL(images.getDates("low").at(0),  -42);
    BOOST_CHECK_EQUAL(images.getDates("low").at(1), 1337);
    BOOST_CHECK_EQUAL(images.count(),       4);
    BOOST_CHECK_EQUAL(images.count("high"), 2);
    BOOST_CHECK_EQUAL(images.count("low"),  2);
    BOOST_CHECK_NO_THROW(images.getAny(0));
    BOOST_CHECK_NO_THROW(images.getAny(-42));
    BOOST_CHECK_THROW(   images.getAny(1), not_found_error);

    // images: high: 0, 5; low: -42, 1337
    BOOST_CHECK_EQUAL(images.getDates().size(), 4);
    BOOST_CHECK_NO_THROW(images.remove("high", 0));
    // images: high: 5; low: -42, 1337
    BOOST_CHECK_THROW(images.getAny(0), not_found_error);
    BOOST_CHECK(!images.has("high",    0));
    BOOST_CHECK( images.has("high",    5));
    BOOST_CHECK( images.has("low",   -42));
    BOOST_CHECK( images.has("low",  1337));
    BOOST_CHECK_EQUAL(images.getDates("high").size(), 1);
    BOOST_CHECK_EQUAL(images.getDates("high").at(0), 5);
    BOOST_CHECK_EQUAL(images.getDates().size(), 3);
    BOOST_CHECK_EQUAL(images.countResolutionTags(), 2);
    BOOST_CHECK_EQUAL(images.count(),       3);
    BOOST_CHECK_EQUAL(images.count("high"), 1);
    BOOST_CHECK_EQUAL(images.count("low"),  2);

    // images: high: 5; low: -42, 1337
    BOOST_CHECK_NO_THROW(images.remove("high", 5));
    // images: low: -42, 1337
    BOOST_CHECK_EQUAL(images.countResolutionTags(), 1);
    BOOST_CHECK(!images.has("high"));
    BOOST_CHECK( images.has("low"));
    BOOST_CHECK( images.has(1337));
    BOOST_CHECK(!images.has(5));
    BOOST_CHECK_EQUAL(images.getResolutionTags().size(), 1);

    images.set("high", 1337, Image{});
    // images: high: 1337, low: -42, 1337
    BOOST_CHECK_EQUAL(images.count(1337), 2);
    BOOST_CHECK_EQUAL(images.getResolutionTags().size(), 2);
    BOOST_CHECK_EQUAL(images.getResolutionTags(1337).size(), 2);
    BOOST_CHECK_EQUAL(images.getResolutionTags(-42).size(),  1);
    BOOST_CHECK_EQUAL(images.getDates().size(), 2);
    images.remove(1337);
    // images: low: -42
    BOOST_CHECK_EQUAL(images.count(1337), 0);
    BOOST_CHECK_EQUAL(images.count(-42),  1);
    BOOST_CHECK_EQUAL(images.count(),     1);
    BOOST_CHECK_EQUAL(images.getDates().size(), 1);
    BOOST_CHECK_EQUAL(images.getResolutionTags().size(), 1);
    BOOST_CHECK_EQUAL(images.getResolutionTags(1337).size(), 0);
    BOOST_CHECK_EQUAL(images.getResolutionTags(-42).size(),  1);
}


BOOST_AUTO_TEST_CASE(shared_copy_and_clone) {
    // create an image memory and add an image
    MultiResImages images;
    images.set("high", 0, Image{1, 1, Type::uint8x1});
    Image& im = images.get("high", 0);
    im.at<uint8_t>(0, 0) = 41;

    // make clones and shared copies
    MultiResImages cl1 = images.cloneWithClonedImages();
    MultiResImages cl2 = images; // performs a cloneWithClonedImages()
    MultiResImages sc1 = images.cloneWithSharedImageCopies();

    // check wheather they have the image
    BOOST_CHECK(cl1.has("high", 0));
    BOOST_CHECK(cl2.has("high", 0));
    BOOST_CHECK(sc1.has("high", 0));

    // check that the memory of shared copies is dependent and of clones independent
    im.at<uint8_t>(0, 0) = 42;

    Image& im_cl1 = cl1.get("high", 0);
    BOOST_CHECK_EQUAL(im_cl1.at<uint8_t>(0,0), 41);
    BOOST_CHECK(!im.isSharedWith(im_cl1));

    Image& im_cl2 = cl2.get("high", 0);
    BOOST_CHECK_EQUAL(im_cl2.at<uint8_t>(0,0), 41);
    BOOST_CHECK(!im.isSharedWith(im_cl2));

    Image& im_sc1 = sc1.get("high", 0);
    BOOST_CHECK_EQUAL(im_sc1.at<uint8_t>(0,0), 42);
    BOOST_CHECK(im.isSharedWith(im_sc1));

    // check that a new image in sc1 does not appear in sc2 and vice versa
    cl1.set("high", -1, Image{1, 1, Type::uint8});
    BOOST_CHECK( cl1.has("high",      -1));
    BOOST_CHECK(!cl2.has("high",      -1));
    BOOST_CHECK(!sc1.has("high",      -1));
    BOOST_CHECK(!images.has("high",   -1));

    cl2.set("high",  1, Image{1, 1, Type::uint8});
    BOOST_CHECK(!cl1.has("high",       1));
    BOOST_CHECK( cl2.has("high",       1));
    BOOST_CHECK(!sc1.has("high",       1));
    BOOST_CHECK(!images.has("high",    1));

    sc1.set("high",  2, Image{1, 1, Type::uint8});
    BOOST_CHECK(!cl1.has("high",       2));
    BOOST_CHECK(!cl2.has("high",       2));
    BOOST_CHECK( sc1.has("high",       2));
    BOOST_CHECK(!images.has("high",    2));
}


BOOST_AUTO_TEST_CASE(tags) {
    MultiResImages images;

    BOOST_CHECK(!images.has("high"));
    images.set("high", 0, Image{});
    BOOST_CHECK(images.has("high"));

    images.set("low", 0, Image{});

    auto tags = images.getResolutionTags();
    BOOST_CHECK(std::find(tags.begin(), tags.end(), "high") != tags.end());
    BOOST_CHECK(std::find(tags.begin(), tags.end(), "low")  != tags.end());

    images.remove("high");
    tags = images.getResolutionTags();
    BOOST_CHECK(std::find(tags.begin(), tags.end(), "high") == tags.end());
    BOOST_CHECK(std::find(tags.begin(), tags.end(), "low")  != tags.end());
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

#include <iostream>

#include <boost/test/unit_test.hpp>

#include "Image.h"
#include "GeoInfo.h"
#include "helpers_test.h"

namespace imagefusion {


BOOST_AUTO_TEST_SUITE(geoinfo_suite)

BOOST_AUTO_TEST_CASE(copy_move)
{
    std::string input_filename  = "../test_resources/images/test_info_image.tif";

    GeoInfo gi1{input_filename};
    GeoInfo gi2;
    gi2 = GeoInfo{input_filename};
    BOOST_CHECK(gi1 == gi2);

    GeoInfo gi3(std::move(gi2));
    BOOST_CHECK(gi1 == gi3);
}


BOOST_AUTO_TEST_CASE(nodata_marker)
{
    std::string input_filename  = "../test_resources/images/test_info_image.tif";
    std::string output_filename = "../test_resources/images/test_image_nodata.tif";

    GeoInfo gi{input_filename};

    // test no-data value
    gi.setNodataValue(42);
    BOOST_CHECK(gi.hasNodataValue());
    BOOST_CHECK_EQUAL(gi.getNodataValue(), 42);

    // now copy the image and reread the geo infos of it
    Image img{input_filename};
    img.write(output_filename, gi);
    GeoInfo gi_after{output_filename};

    BOOST_CHECK(gi_after.hasNodataValue());
    BOOST_CHECK_EQUAL(gi_after.getNodataValue(), 42);

//    std::cout << "nodata-marker: " << gi_after.getNodataValue() << std::endl;
}

BOOST_AUTO_TEST_CASE(gcp)
{
    std::string input_filename  = "../test_resources/images/test_info_image.tif";
    std::string output_filename = "../test_resources/images/test_image_gcp.tif";

    GeoInfo gi{input_filename};

    // test GCPs, note: GCP spatial reference is copied from geotransform spatial reference which is set in the test image
    gi.addGCP(GeoInfo::GCP{"ignored ID", "ignored info string",
                           /*pixel*/ 1, /*line*/ 2, /*X*/ 234, /*Y*/ 235, /*Z*/ 0});
    BOOST_CHECK_EQUAL(gi.gcps.size(), 1);
    BOOST_CHECK_EQUAL(gi.gcps.front().pixel,   1);
    BOOST_CHECK_EQUAL(gi.gcps.front().line,    2);
    BOOST_CHECK_EQUAL(gi.gcps.front().x,     234);
    BOOST_CHECK_EQUAL(gi.gcps.front().y,     235);
    BOOST_CHECK_EQUAL(gi.gcps.front().z,       0);
    BOOST_CHECK(!gi.hasGCPs()); // at least three GCPs are required

    gi.addGCP(GeoInfo::GCP{});
    gi.addGCP(GeoInfo::GCP{});
    BOOST_CHECK(!gi.hasGCPs()); // SRS also required

    gi.gcpSRS = gi.geotransSRS;
    BOOST_CHECK(gi.gcpSRS.Validate() == OGRERR_NONE);
    BOOST_CHECK(gi.hasGCPs());

    // remove the existing geotransform, since it would otherwise would take precedence over the GCP spatial reference
    gi.geotrans.clear();

    // now copy the image and reread the geo infos of it
    Image img{input_filename};
    img.write(output_filename, gi);
    GeoInfo gi_after{output_filename};

    // check if the GCP survived
    BOOST_CHECK_EQUAL(gi_after.gcps.size(), 3);
    if (!gi_after.gcps.empty()) {
        // check values; do not check strings... they are not working currently
        BOOST_CHECK_EQUAL(gi_after.gcps.front().pixel,   1);
        BOOST_CHECK_EQUAL(gi_after.gcps.front().line,    2);
        BOOST_CHECK_EQUAL(gi_after.gcps.front().x,     234);
        BOOST_CHECK_EQUAL(gi_after.gcps.front().y,     235);
        BOOST_CHECK_EQUAL(gi_after.gcps.front().z,       0);
    }

    BOOST_CHECK(gi_after.gcpSRS.Validate() == OGRERR_NONE);

//    std::cout << "GCPs:" << std::endl;
//    for (auto const& gcp : gi_after.gcps)
//        std::cout << "ID: '" << gcp.id << "', Info: '" << gcp.info << "', "
//                  << "(" << gcp.pixel << ", " << gcp.line << ") --> "
//                  << "(" << gcp.x << ", " << gcp.y  << ", " << gcp.z << ")" << std::endl;

//    std::cout << "GCP-CS:" << std::endl;
//    char* projStr;
//    gi_after.gcpSRS.exportToWkt(&projStr);
//    std::cout << projStr << std::endl;
//    CPLFree(projStr);
}

BOOST_AUTO_TEST_CASE(metadata)
{
    std::string input_filename  = "../test_resources/images/test_info_image.tif";
    std::string output_filename = "../test_resources/images/test_image_meta.tif";

    GeoInfo gi{input_filename};

    // set some meta data domain, keys, values
    gi.setMetadataItem("CUSTOM_DOMAIN", "TEST_KEY1", "TEST_VALUE1");
    gi.setMetadataItem("CUSTOM_DOMAIN", "TEST_KEY2", "TEST_VALUE2");
    auto metaDoms = gi.getMetadataDomains();
    BOOST_CHECK(metaDoms.size() == 3 || metaDoms.size() == 4);
    int metaDomsInputSize = metaDoms.size();
    // GDAL 2.1.x (probably)
    if (metaDoms.size() == 3) {
        std::set<std::string> expected{"", "CUSTOM_DOMAIN", "IMAGE_STRUCTURE"};
        std::set<std::string> testing(metaDoms.begin(), metaDoms.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(testing.begin(), testing.end(), expected.begin(), expected.end());
    }
    // GDAL 2.2.x (probably)
    if (metaDoms.size() == 4) {
        std::set<std::string> expected{"", "CUSTOM_DOMAIN", "DERIVED_SUBDATASETS", "IMAGE_STRUCTURE"};
        std::set<std::string> testing(metaDoms.begin(), metaDoms.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(testing.begin(), testing.end(), expected.begin(), expected.end());
    }

    if (gi.hasMetadataDomain("CUSTOM_DOMAIN")) {
        auto const& items = gi.getMetadataItems("CUSTOM_DOMAIN");

        auto item_it = items.find("TEST_KEY1");
        BOOST_CHECK(item_it != items.end());
        if (item_it != items.end())
            BOOST_CHECK_EQUAL(item_it->second, "TEST_VALUE1");

        item_it = items.find("TEST_KEY2");
        BOOST_CHECK(item_it != items.end());
        if (item_it != items.end())
            BOOST_CHECK_EQUAL(item_it->second, "TEST_VALUE2");

        gi.setMetadataItem("CUSTOM_DOMAIN", "TEST_KEY1", "TEST_VALUE0");
        item_it = items.find("TEST_KEY1");
        BOOST_CHECK(item_it != items.end());
        if (item_it != items.end())
            BOOST_CHECK_EQUAL(item_it->second, "TEST_VALUE0");

        gi.removeMetadataItem("CUSTOM_DOMAIN", "TEST_KEY1");
        item_it = items.find("TEST_KEY1");
        BOOST_CHECK(item_it == items.end());

        gi.removeMetadataDomain("CUSTOM_DOMAIN");
        auto dom_it = gi.metadata.find("CUSTOM_DOMAIN");
        BOOST_CHECK(dom_it == gi.metadata.end());

        // check that removing the only item in a domain, will also remove the domain
        gi.setMetadataItem("CUSTOM_DOMAIN", "TEST_KEY1", "TEST_VALUE1");
        gi.removeMetadataItem("CUSTOM_DOMAIN", "TEST_KEY1");
        dom_it = gi.metadata.find("CUSTOM_DOMAIN");
        BOOST_CHECK(dom_it == gi.metadata.end());

        gi.setMetadataItem("CUSTOM_DOMAIN", "TEST_KEY1", "TEST_VALUE1");
        gi.setMetadataItem("CUSTOM_DOMAIN", "TEST_KEY2", "TEST_VALUE2");
    }

    // now copy the image and reread the geo infos of it
    Image img{input_filename};
    img.write(output_filename, gi);
    img.write("new_write.tif", gi);
    img.write("old_write.tif");
    gi.addTo("old_write.tif");
    GeoInfo gi_after{output_filename};

    // check if we still have the same meta data
    auto metaDoms_after = gi_after.getMetadataDomains();
    BOOST_CHECK_EQUAL(metaDoms_after.size(), metaDomsInputSize);
    // GDAL 2.1.x (probably)
    if (metaDoms_after.size() == 3) {
        std::set<std::string> expected{"", "CUSTOM_DOMAIN", "IMAGE_STRUCTURE"};
        std::set<std::string> testing(metaDoms_after.begin(), metaDoms_after.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(testing.begin(), testing.end(), expected.begin(), expected.end());
    }
    // GDAL 2.2.x (probably)
    if (metaDoms_after.size() == 4) {
        std::set<std::string> expected{"", "CUSTOM_DOMAIN", "DERIVED_SUBDATASETS", "IMAGE_STRUCTURE"};
        std::set<std::string> testing(metaDoms_after.begin(), metaDoms_after.end());
        BOOST_CHECK_EQUAL_COLLECTIONS(testing.begin(), testing.end(), expected.begin(), expected.end());
    }

    if (gi_after.hasMetadataDomain("CUSTOM_DOMAIN")) {
        auto const& items_after = gi_after.getMetadataItems("CUSTOM_DOMAIN");
        auto item_it_after = items_after.find("TEST_KEY1");
        BOOST_CHECK(item_it_after != items_after.end());
        if (item_it_after != items_after.end())
            BOOST_CHECK_EQUAL(item_it_after->second, "TEST_VALUE1");
        item_it_after = items_after.find("TEST_KEY2");
        BOOST_CHECK(item_it_after != items_after.end());
        if (item_it_after != items_after.end())
            BOOST_CHECK_EQUAL(item_it_after->second, "TEST_VALUE2");
    }

//    std::cout << "Metadata:" << std::endl;
//    for (auto const& mp : gi_after.metadata) {
//        std::cout << "Domain: '" << mp.first << "'" << std::endl;
//        for (auto const& item : mp.second) {
//            std::cout << "key: '" << item.first << "', value: '" << item.second << "'" << std::endl;
//        }
//        std::cout << std::endl;
//    }
}

BOOST_AUTO_TEST_CASE(geotransform)
{
    std::string input_filename  = "../test_resources/images/test_info_image.tif";
    std::string output_filename = "../test_resources/images/test_image_geo.tif";

    GeoInfo gi{input_filename};

    // test geo tranform methods on projection space
    gi.geotrans.clear();
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);
    BOOST_CHECK(!gi.hasGeotransform());

    gi.geotrans.translateProjection(/*xorigin*/ 2, /*yorigin*/ 5);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   2);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   5);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);
    BOOST_CHECK(gi.hasGeotransform());

    gi.geotrans.shearXProjection(2);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,  12);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   5);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      2);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);

    gi.geotrans.shearYProjection(1);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,  12);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,  17);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      2);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      3);

    gi.geotrans.scaleProjection(/*xscale*/ 10, /*yscale*/ 20);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX, 120);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY, 340);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,     10);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,     20);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,     20);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,     60);

    gi.geotrans.rotateProjection(90);
    BOOST_CHECK_SMALL(gi.geotrans.offsetX - (-340), 1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.offsetY -   120,  1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.XToX    -  (-20), 1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.YToX    -  (-60), 1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.XToY    -    10,  1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.YToY    -    20,  1e-10);

    // test geo tranform methods on image space
    gi.geotrans.clear();
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);

    gi.geotrans.shearXImage(2);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      2);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);

    gi.geotrans.shearYImage(1);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      3);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      2);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);

    gi.geotrans.scaleImage(/*xscale*/ 10, /*yscale*/ 20);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,     30);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,     40);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,     10);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,     20);

    gi.geotrans.translateImage(/*xorigin*/ 2, /*yorigin*/ 5);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX, 260);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY, 120);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,     30);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,     40);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,     10);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,     20);

    Size sz{10, 20};
    gi.geotrans.flipImage(/*horizontal*/ true, /*vertical*/ false, sz);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX, 560);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY, 220);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,    -30);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,     40);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,    -10);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,     20);

    gi.geotrans.flipImage(/*horizontal*/ false, /*vertical*/ true, sz);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,1360);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY, 620);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,    -30);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,    -40);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,    -10);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,    -20);

    gi.geotrans.flipImage(/*horizontal*/ true, /*vertical*/ true, sz);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX, 260);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY, 120);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,     30);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,     40);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,     10);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,     20);

    gi.geotrans.rotateImage(90);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   260);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   120);
    BOOST_CHECK_SMALL(gi.geotrans.XToX    -   40,  1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.YToX    - (-30), 1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.XToY    -   20,  1e-10);
    BOOST_CHECK_SMALL(gi.geotrans.YToY    - (-10), 1e-10);


    // test direct setting of geo transform
    gi.geotrans.set(/*offsetX*/ 1, /*offsetY*/ 2, /*x_to_x*/ 3, /*y_to_x*/ 4, /*x_to_y*/ 5, /*y_to_y*/ 6);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX, 1);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY, 2);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,    3);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,    4);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,    5);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,    6);

    // test image has already set a geotransform spatial reference coordinate system
    BOOST_CHECK(gi.geotransSRS.Validate() == OGRERR_NONE);

    // now copy the image and reread the geo infos of it
    Image img{input_filename};
    img.write(output_filename, gi);
    GeoInfo gi_after{output_filename};

    // check if geotransform survived
    BOOST_CHECK_EQUAL(gi_after.geotrans.offsetX, 1);
    BOOST_CHECK_EQUAL(gi_after.geotrans.offsetY, 2);
    BOOST_CHECK_EQUAL(gi_after.geotrans.XToX,    3);
    BOOST_CHECK_EQUAL(gi_after.geotrans.YToX,    4);
    BOOST_CHECK_EQUAL(gi_after.geotrans.XToY,    5);
    BOOST_CHECK_EQUAL(gi_after.geotrans.YToY,    6);

    BOOST_CHECK(gi_after.geotransSRS.Validate() == OGRERR_NONE);

//    std::cout << "Geotransform: ";
//    for (int i = 0; i < 6; ++i)
//        std::cout << gi_after.geotrans.values[i] << "; ";
//    std::cout << std::endl;

//    std::cout << "Geo-CS:" << std::endl;
//    char* projStr;
//    gi_after.geotransSRS.exportToWkt(&projStr);
//    std::cout << projStr << std::endl;
//    CPLFree(projStr);

    Rectangle crop{3, 7, 13, 19};
    GeoInfo giWithTransforms;
    giWithTransforms = GeoInfo{input_filename, {}, crop};
    gi = GeoInfo{input_filename};
    gi.geotrans.translateImage(crop.x, crop.y);
    gi.size = crop.size();
    BOOST_CHECK(gi == giWithTransforms);

    giWithTransforms = GeoInfo{input_filename, {}, crop, /*flipH*/ true, /*flipV*/ false};
    gi = GeoInfo{input_filename};
    gi.geotrans.translateImage(crop.x, crop.y);
    gi.size = crop.size();
    gi.geotrans.flipImage(/*flipH*/ true, /*flipV*/ false, gi.size);
    BOOST_CHECK(gi == giWithTransforms);

    giWithTransforms = GeoInfo{input_filename, {}, crop, /*flipH*/ false, /*flipV*/ true};
    gi = GeoInfo{input_filename};
    gi.geotrans.translateImage(crop.x, crop.y);
    gi.size = crop.size();
    gi.geotrans.flipImage(/*flipH*/ false, /*flipV*/ true, gi.size);
    BOOST_CHECK(gi == giWithTransforms);

    giWithTransforms = GeoInfo{input_filename, {}, crop, /*flipH*/ true, /*flipV*/ true};
    gi = GeoInfo{input_filename};
    gi.geotrans.translateImage(crop.x, crop.y);
    gi.size = crop.size();
    gi.geotrans.flipImage(/*flipH*/ true, /*flipV*/ true, gi.size);
    BOOST_CHECK(gi == giWithTransforms);
}

BOOST_AUTO_TEST_CASE(geotransform_evaluation)
{
    GeoInfo gi;
    BOOST_CHECK_EQUAL(gi.geotrans.offsetX,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.offsetY,   0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToX,      1);
    BOOST_CHECK_EQUAL(gi.geotrans.YToX,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.XToY,      0);
    BOOST_CHECK_EQUAL(gi.geotrans.YToY,      1);

    // test shifted identity
    Coordinate c;
    c = gi.geotrans.imgToProj(Coordinate{2, 3});
    BOOST_CHECK_EQUAL(c.x, 2);
    BOOST_CHECK_EQUAL(c.y, 3);

    c = gi.geotrans.projToImg(Coordinate{2, 3});
    BOOST_CHECK_EQUAL(c.x, 2);
    BOOST_CHECK_EQUAL(c.y, 3);

    // test evaluation
    gi.geotrans.offsetX = 1;
    gi.geotrans.offsetY = 2;
    gi.geotrans.XToX = 4;
    gi.geotrans.YToX = 7;
    gi.geotrans.XToY = 3;
    gi.geotrans.YToY = 5;

    /*  / 1 \     / 4  7 \   / x_i\
     * |     | + |        | |      |
     *  \ 2 /     \ 3  5 /   \ y_i/
     */
    c = gi.geotrans.imgToProj(Coordinate{2, 3});
    BOOST_CHECK_EQUAL(c.x, 1 + 4 * 2 + 7 * 3);
    BOOST_CHECK_EQUAL(c.y, 2 + 3 * 2 + 5 * 3);

    /*  /-5  7 \   / x_p - 1 \
     * |        | |           |
     *  \ 3 -4 /   \ y_p - 2 /
     */
    c = gi.geotrans.projToImg(Coordinate{2, 3});
    BOOST_CHECK_EQUAL(c.x, -5 + 7);
    BOOST_CHECK_EQUAL(c.y,  3 - 4);
}

BOOST_AUTO_TEST_CASE(latitude_longitude)
{
    GeoInfo gi("../test_resources/images/test_info_image.tif");

    // top-left and bottom-right projection space coordinates and longitude / latitude from gdalinfo call
    Coordinate p_tl{379545, 5973315};
    Coordinate p_br{389265, 5963595};
    Coordinate ll_tl{13.1669278, 53.8942694}; // 13d10' 0.94"E, 53d53'39.37"N
    Coordinate ll_br{13.3182778, 53.8091083}; // 13d19' 5.80"E, 53d48'32.79"N

    // convert image coordinates and projection space coordinates to longitude / latitude
    Coordinate i_tl{0, 0};
    Coordinate i_br(gi.width(), gi.height());

    auto i2ll_tl = gi.imgToLatLong(i_tl);
    auto i2ll_br = gi.imgToLatLong(i_br);
    auto p2ll_tl = gi.projToLatLong(p_tl);
    auto p2ll_br = gi.projToLatLong(p_br);

    BOOST_CHECK_CLOSE_FRACTION(ll_tl.x, i2ll_tl.x, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(ll_tl.y, i2ll_tl.y, 1e-6);

    BOOST_CHECK_CLOSE_FRACTION(ll_br.x, i2ll_br.x, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(ll_br.y, i2ll_br.y, 1e-6);

    BOOST_CHECK_CLOSE_FRACTION(ll_tl.x, p2ll_tl.x, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(ll_tl.y, p2ll_tl.y, 1e-6);

    BOOST_CHECK_CLOSE_FRACTION(ll_br.x, p2ll_br.x, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(ll_br.y, p2ll_br.y, 1e-6);

    // convert longitude / latitude to image coordinates and projection space coordinates
    auto ll2i_tl = gi.latLongToImg(ll_tl);
    auto ll2i_br = gi.latLongToImg(ll_br);
    auto ll2p_tl = gi.latLongToProj(ll_tl);
    auto ll2p_br = gi.latLongToProj(ll_br);

    BOOST_CHECK_LT(std::abs(   i_tl.x- ll2i_tl.x),1e-2); // rel err to 0 is inf, so use abs err
    BOOST_CHECK_LT(std::abs(   i_tl.y- ll2i_tl.y),1e-2); // rel err to 0 is inf, so use abs err
    BOOST_CHECK_CLOSE_FRACTION(i_br.x, ll2i_br.x, 1e-5);
    BOOST_CHECK_CLOSE_FRACTION(i_br.y, ll2i_br.y, 1e-5);
    BOOST_CHECK_CLOSE_FRACTION(p_tl.x, ll2p_tl.x, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(p_tl.y, ll2p_tl.y, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(p_br.x, ll2p_br.x, 1e-6);
    BOOST_CHECK_CLOSE_FRACTION(p_br.y, ll2p_br.y, 1e-6);
}

BOOST_AUTO_TEST_CASE(exceptions)
{
    BOOST_CHECK_THROW(GeoInfo g{"not-existing-file"}, runtime_error);

    GeoInfo gi;
    BOOST_CHECK_THROW(gi.getMetadataItems("whatever"), not_found_error);
}

BOOST_AUTO_TEST_CASE(size_channels_type)
{
    GeoInfo giDefault;
    BOOST_CHECK_EQUAL(giDefault.channels,    0);
    BOOST_CHECK_EQUAL(giDefault.width(),     0);
    BOOST_CHECK_EQUAL(giDefault.height(),    0);
    BOOST_CHECK_EQUAL(giDefault.size.width,  0);
    BOOST_CHECK_EQUAL(giDefault.size.height, 0);
    BOOST_CHECK_EQUAL(giDefault.baseType,    Type::invalid);

    std::string input_filename  = "../test_resources/images/formats/float32x2.tif";
    GeoInfo gi(input_filename);
    BOOST_CHECK_EQUAL(gi.channels,    2);
    BOOST_CHECK_EQUAL(gi.width(),     6);
    BOOST_CHECK_EQUAL(gi.height(),    5);
    BOOST_CHECK_EQUAL(gi.size.width,  6);
    BOOST_CHECK_EQUAL(gi.size.height, 5);
    BOOST_CHECK_EQUAL(gi.baseType,    Type::float32);

    GeoInfo gi_single(input_filename, std::vector<int>{0});
    BOOST_CHECK_EQUAL(gi_single.channels,    1);
    BOOST_CHECK_EQUAL(gi_single.width(),     6);
    BOOST_CHECK_EQUAL(gi_single.height(),    5);
    BOOST_CHECK_EQUAL(gi_single.size.width,  6);
    BOOST_CHECK_EQUAL(gi_single.size.height, 5);
    BOOST_CHECK_EQUAL(gi_single.baseType,    Type::float32);
}

BOOST_AUTO_TEST_CASE(subdatasets)
{
    std::string filename = "test.nc";
    if (!createMultiImageFile(filename))
        return;

    using md_type = std::map<std::string, std::string>;
    using md_it = md_type::iterator;

    // test that parent (container) file has 0 channels, but 4 subdatasets and metadata item id:parent
    {
        GeoInfo gi{filename};
        BOOST_CHECK_EQUAL(gi.channels, 0);
        BOOST_CHECK_EQUAL(gi.subdatasetsCount(), 4);
        md_type md;
        BOOST_REQUIRE_NO_THROW(md = gi.getMetadataItems(""));
        md_it it = md.find("id");
        if (it != md.end()) // currently not implemented, see https://trac.osgeo.org/gdal/wiki/NetCDF_Improvements#Issueswiththecurrentimplementation1
            BOOST_CHECK_EQUAL(it->second, "parent");
    }

    // test that subdataset 1 has 1 channels, 0 subdatasets and metadata item id:sds1
    {
        GeoInfo gi{filename, std::vector<int>{0}};
        BOOST_CHECK_EQUAL(gi.channels, 1);
        BOOST_CHECK_EQUAL(gi.subdatasetsCount(), 0);
        md_type md;
        BOOST_REQUIRE_NO_THROW(md = gi.getMetadataItems(""));
        md_it it = md.find("Band1#id");
        BOOST_REQUIRE(it != md.end());
        BOOST_CHECK_EQUAL(it->second, "sds1");
    }

    // test that the combination subdataset 1+2 (both uint8) is valid, has 2 channels, 0 subdatasets and no metadata
    {
        GeoInfo gi{filename, std::vector<int>{0, 1}};
        BOOST_CHECK_EQUAL(gi.channels, 2);
        BOOST_CHECK_EQUAL(gi.subdatasetsCount(), 0);
        BOOST_CHECK_EQUAL(gi.baseType, Type::uint8);
        md_type md;
        BOOST_REQUIRE_THROW(md = gi.getMetadataItems(""), runtime_error);
    }

    // test that the combination subdataset 3+4 (both uint16) is valid, has 2 channels, 0 subdatasets and no metadata
    {
        GeoInfo gi{filename, std::vector<int>{2, 3}};
        BOOST_CHECK_EQUAL(gi.channels, 2);
        BOOST_CHECK_EQUAL(gi.subdatasetsCount(), 0);
        BOOST_CHECK_EQUAL(gi.baseType, Type::uint16);
        md_type md;
        BOOST_REQUIRE_THROW(md = gi.getMetadataItems(""), runtime_error);
    }

    // test that the combination subdataset 1+3 (uint8, uint16) is invalid
    try {
        // should either throw or have invalid type
        GeoInfo gi = GeoInfo{filename, std::vector<int>{0, 2}};
        BOOST_CHECK_EQUAL(gi.baseType, Type::invalid);
    }
    catch(runtime_error& e) {
    }

    // test reading a subdataset with special GDAL filename
    {
        GeoInfo gi_num{filename, std::vector<int>{0}};
        GeoInfo gi_name;
        BOOST_CHECK_NO_THROW(gi_name = GeoInfo{"NETCDF:\"" + filename + "\":Band1"});
        BOOST_CHECK(gi_num == gi_name);
    }
}

BOOST_AUTO_TEST_CASE(color_table)
{
    std::string filename = "../test_resources/images/formats/uint8x2_colortable.png";
    GeoInfo gi{filename};
    BOOST_CHECK_EQUAL(gi.size, (Size{6, 5}));
    BOOST_CHECK_EQUAL(gi.channels, 1);               // single color index (should expand to 2 channels)
    BOOST_CHECK_EQUAL(gi.baseType, Type::uint8);
    BOOST_CHECK(!gi.colorTable.empty());
    BOOST_REQUIRE_EQUAL(gi.nodataValues.size(), 1);
    BOOST_CHECK_EQUAL(gi.nodataValues.front(), 51);  // index: 51
    BOOST_REQUIRE_GT(gi.colorTable.size(), gi.nodataValues.front());
    std::array<short, 4>& nodataEntry = gi.colorTable.at(gi.nodataValues.front());
    BOOST_CHECK_EQUAL(nodataEntry.at(0), 255);       // values: "255, 255, 255, 0"
    BOOST_CHECK_EQUAL(nodataEntry.at(1), 255);
    BOOST_CHECK_EQUAL(nodataEntry.at(2), 255);
    BOOST_CHECK_EQUAL(nodataEntry.at(3), 0);
    BOOST_CHECK(gi.compareColorTables(gi));

    // test writing a color table
    // DOES NOT WORK WITH GDAL GTiff: ALPHA CHANNEL WILL BE CHANGED TO 255 FOR ALL INDICES EXCEPT NODATA WHERE IT WILL BE CHANGED TO 0
    //                         PNG: ALPHA CHANNEL OF NODATA WILL BE CHANGED TO 0
    nodataEntry.at(0) = 1;
    nodataEntry.at(1) = 2;
    nodataEntry.at(2) = 3;
    nodataEntry.at(3) = 4;

    Image i{filename, /*channels*/ {}, /*Rectangle*/ {}, /*flipH*/ false, /*flipV*/ false, /*ignoreColorTable*/ true};
    std::string newFilename = "../test_resources/images/test_write_colortable.png";
    i.write(newFilename, gi);

    GeoInfo same{newFilename};
    BOOST_CHECK_EQUAL(same.size, (Size{6, 5}));
    BOOST_CHECK_EQUAL(same.channels, 1);               // single color index (was read with ignoreColorTable)
    BOOST_CHECK_EQUAL(same.baseType, Type::uint8);
    BOOST_CHECK(!same.colorTable.empty());
    BOOST_REQUIRE_EQUAL(same.nodataValues.size(), 1);
    BOOST_CHECK_EQUAL(same.nodataValues.front(), 51);
    BOOST_REQUIRE_GT(same.colorTable.size(), same.nodataValues.front());
    std::array<short, 4> const& sameNodataEntry = same.colorTable.at(same.nodataValues.front());
    BOOST_CHECK_EQUAL(sameNodataEntry.at(0), 1);       // changed values: "1, 2, 3, 4"
    BOOST_CHECK_EQUAL(sameNodataEntry.at(1), 2);
    BOOST_CHECK_EQUAL(sameNodataEntry.at(2), 3);
    BOOST_WARN_EQUAL(sameNodataEntry.at(3), 4); // ENTRY WILL BE CHANGED UNFORTUNATELY FOR PNG AND TIFF
    BOOST_WARN(gi.compareColorTables(same));    // ... this is a known error

    // test failing of writing to invalid image
    i = Image{filename}; // gets by default exanded to uint8x2
    i.write(newFilename, gi); // ignores the color table, since it is not a uint8x1 image

    same = GeoInfo{newFilename};
    BOOST_CHECK_EQUAL(same.size, (Size{6, 5}));
    BOOST_CHECK_EQUAL(same.channels, 2);               // expanded to 2 channels
    BOOST_CHECK_EQUAL(same.baseType, Type::uint8);
    BOOST_CHECK(same.colorTable.empty());
    BOOST_REQUIRE_EQUAL(same.nodataValues.size(), 2);
    BOOST_CHECK_EQUAL(same.nodataValues.front(), 51);  // is index: 51, should be "1, 4"
}


// test the compareColorTables method
BOOST_AUTO_TEST_CASE(compare_color_tables) {
    GeoInfo gi;

    // test empty color tables
    BOOST_CHECK(gi.compareColorTables(gi));
    gi.colorTable.push_back(std::array<short,4>{1, 2, 3, 4});
    BOOST_CHECK(!gi.compareColorTables(GeoInfo{}));
    BOOST_CHECK(GeoInfo{}.compareColorTables(gi));

    // same color tables
    GeoInfo test = gi;
    BOOST_CHECK(gi.compareColorTables(test));

    // compatible color tables
    test.colorTable.push_back(std::array<short,4>{8, 7, 6, 5});
    BOOST_CHECK(gi.compareColorTables(test));

    // incompatible color tables
    gi.colorTable.push_back(std::array<short,4>{8, 7, 6, 0});
    BOOST_CHECK(!gi.compareColorTables(test));
}



// test the case that a sentinel-2 image lying on the boundary of a MODIS tile is intersected and warped correctly
BOOST_AUTO_TEST_CASE(warp_boundary_intersection)
{
    // load 30x30 image with MODIS extents and SRS and values
    // 0...0 1...1 2...2 (each block with the same value is 10x10)
    // ..... ..... .....
    // 0...0 1...1 2...2
    // 3...3 4...4 5...5
    // ..... ..... .....
    // 3...3 4...4 5...5
    // 6...6 7...7 8...8
    // ..... ..... .....
    // 6...6 7...7 8...8
    GeoInfo giBig{"../test_resources/images/geocrop-border/modis-ultra-low-res.tif"};
    Image imgBig{"../test_resources/images/geocrop-border/modis-ultra-low-res.tif"};
    BOOST_CHECK_EQUAL(imgBig.size(), (Size{30,30}));
    BOOST_CHECK_EQUAL(giBig.getNodataValue(), 255);

    // load 10x10 image with Sentinel SRS, but a pixel resolution of 5000
    GeoInfo giSmall{"../test_resources/images/geocrop-border/sentinel-low-res.tif"};
    Image imgSmall{"../test_resources/images/geocrop-border/sentinel-low-res.tif"};
    BOOST_CHECK_EQUAL(imgSmall.size(), (Size{10,10}));
    BOOST_CHECK(!giSmall.hasNodataValue());

    // now move small image to a corner, to the middle of a boundary or to the center and simulate an imggeocrop run
    std::vector<imagefusion::CoordRectangle> cropDummy(2);
    for (int ySrc : {0, 15, 30}) {
        for (int xSrc : {0, 15, 30}) {
//            std::cout << "Test coordinates: xSrc: " << xSrc << ", ySrc: " << ySrc << std::endl;

            // move small (high-res) image to test position
            imagefusion::Coordinate cDst = giBig.imgToProj(Coordinate(xSrc, ySrc), giSmall);
            giSmall.geotrans.offsetX = cDst.x - imgSmall.width() / 2 * giSmall.geotrans.XToX;
            giSmall.geotrans.offsetY = cDst.y - imgSmall.height() / 2 * giSmall.geotrans.YToY;
//            std::string filename = "small" + std::to_string(xSrc) + "_" + std::to_string(ySrc) + ".tif";
//            imgSmall.write(filename);
//            giSmall.addTo(filename);

            // find intersection
            GeoInfo giTarget = giSmall;
            giTarget.intersect(giBig);
            BOOST_CHECK_EQUAL(giTarget.width(),  xSrc == 15 ? 10 : 5);
            BOOST_CHECK_EQUAL(giTarget.height(), ySrc == 15 ? 10 : 5);
//            std::cout << "Intersection corners target projection space: TL: " << giTarget.geotrans.imgToProj({0, 0}) << ", BR: " << giTarget.geotrans.imgToProj(Coordinate(giTarget.width(), giTarget.height())) << std::endl;
//            std::cout << "Intersection target image size: " << giTarget.size << std::endl;

//            // simulate reading only as much as necessary (imgBig has been loaded anyway, but in practice this is not the case)
//            // find target rectangle in source image coordinates and restrict to image part
//            Coordinate tlImg = giTarget.imgToImg(Coordinate{0, 0}, giBig);
//            Coordinate brImg = giTarget.imgToImg(Coordinate{giTarget.size}, giBig);
////            std::cout << "Intersection source image space: TL: " << tlImg << ", BR: " << brImg << std::endl;

//            constexpr double abstol = 1e-8;
//            imagefusion::Point tlPixel{static_cast<int>(std::floor(std::min(tlImg.x, brImg.x) + abstol)), // one pixel oversize
//                                       static_cast<int>(std::floor(std::min(tlImg.y, brImg.y) + abstol))};
//            imagefusion::Point brPixel{static_cast<int>(std::ceil( std::max(tlImg.x, brImg.x) - abstol)),
//                                       static_cast<int>(std::ceil( std::max(tlImg.y, brImg.y) - abstol))};
//            imagefusion::Rectangle targetRect{tlPixel.x, tlPixel.y, brPixel.x - tlPixel.x, brPixel.y - tlPixel.y};
//            imagefusion::Rectangle restrictRect = fixupRect(targetRect, imgBig.size());
////            std::cout << "Target Rect in source image space: " << targetRect << ", restricted rect: " << restrictRect << std::endl;

//            // crop source image part
//            imagefusion::ConstImage cropped = imgBig.sharedCopy(restrictRect);
//            imagefusion::GeoInfo giSource = giBig;
//            giSource.geotrans.translateImage(restrictRect.x, restrictRect.y);

            // warp and check that there is no line with only nodata values
//            imagefusion::Image warped = cropped.warp(giSource, giTarget);
            imagefusion::Image warped = imgBig.warp(giBig, giTarget);
            int expected = xSrc * 3 / (imgBig.width() + 1) + 3 * (ySrc * 3 / (imgBig.height() + 1)); // 0, ..., 8 from fake MODIS image
            bool hasOnlyNodata1 = true;
            bool hasOnlyNodata2 = true;
            for (int x = 0; x < warped.width(); ++x) {
                if (warped.at<uint8_t>(x, 0) != 255) {
                    hasOnlyNodata1 = false;
                    BOOST_CHECK_EQUAL(static_cast<int>(warped.at<uint8_t>(x, 0)), expected);
                }
                if (warped.at<uint8_t>(x, warped.height()-1) != 255) {
                    hasOnlyNodata2 = false;
                    BOOST_CHECK_EQUAL(static_cast<int>(warped.at<uint8_t>(x, warped.height()-1)), expected);
                }
            }
            BOOST_CHECK_MESSAGE(!hasOnlyNodata1, "The top row of the warped image with (xSrc, ySrc) = (" << xSrc << ", " << ySrc << ") has only nodata values. So the crop is too large.");
            BOOST_CHECK_MESSAGE(!hasOnlyNodata2, "The bottom row of the warped image with (xSrc, ySrc) = (" << xSrc << ", " << ySrc << ") has only nodata values. So the crop is too large.");

            hasOnlyNodata1 = true;
            hasOnlyNodata2 = true;
            for (int y = 0; y < warped.height(); ++y) {
                if (warped.at<uint8_t>(0, y) != 255) {
                    hasOnlyNodata1 = false;
                    BOOST_CHECK_EQUAL(static_cast<int>(warped.at<uint8_t>(0, y)), expected);
                }
                if (warped.at<uint8_t>(warped.width()-1, y) != 255) {
                    hasOnlyNodata2 = false;
                    BOOST_CHECK_EQUAL(static_cast<int>(warped.at<uint8_t>(warped.width()-1, y)), expected);
                }
            }
            BOOST_CHECK_MESSAGE(!hasOnlyNodata1, "The left column of the warped image with (xSrc, ySrc) = (" << xSrc << ", " << ySrc << ") has only nodata values. So the crop is too large.");
            BOOST_CHECK_MESSAGE(!hasOnlyNodata2, "The right column of the warped image with (xSrc, ySrc) = (" << xSrc << ", " << ySrc << ") has only nodata values. So the crop is too large.");
//            std::cout << "warped image\n" << warped.cvMat() << std::endl << std::endl;
        }
    }
}


// helper for crop_extents_two_coordinates
GeoInfo makeGeoInfo(Coordinate const& c1, Coordinate const& c2, Size imgsize = {}) {
    GeoInfo gi;

    // set size
    if (imgsize == Size{}) // empty means resolution 1
        imgsize = Size{static_cast<int>(std::abs(c1.x - c2.x)), static_cast<int>(std::abs(c1.y - c2.y))};
    gi.size = imgsize;

    double dx = c2.x - c1.x;
    double dy = c2.y - c1.y;
    gi.geotrans.scaleImage(dx / imgsize.width, dy / imgsize.height);
    gi.geotrans.translateProjection(c1.x, c1.y);
    gi.geotransSRS.SetWellKnownGeogCS("WGS84");
    return gi;
}

// helper for crop_extents_two_coordinates
std::pair<Coordinate,Coordinate>
getCropExtents(GeoInfo ref, GeoInfo const& gi)
{
    assert(ref.geotransSRS.IsSame(&gi.geotransSRS));

    CoordRectangle r = intersectRect(ref, ref.projRect(), gi, gi.projRect());

    // check if there is an intersection
    if (r.area() == 0)
        IF_THROW_EXCEPTION(size_error("The intersection of both images is empty. Cannot find common part to crop that."));

    return {r.tl(), r.br()};
}

// test intersectRect by using the above getCropExtents
BOOST_AUTO_TEST_CASE(crop_extents_two_coordinates)
{
    GeoInfo g;
    Coordinate tl{10, 10};
    Coordinate br{20, 20};

    // reference image: (10, 10) to (20, 20), resolution: 1
    GeoInfo ref{makeGeoInfo(tl, br)};

    // (0, 0) to (30, 30), resolution: 30 (contains reference image)
    g = GeoInfo{makeGeoInfo(Coordinate{0,0}, Coordinate{30,30}, Size{1,1})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, 30);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, 30);

    auto ce = getCropExtents(ref, g);
    BOOST_CHECK(ce.first  == tl);
    BOOST_CHECK(ce.second == br);

    ce = getCropExtents(g, ref); // swapped arguments
    BOOST_CHECK(ce.first  == tl);
    BOOST_CHECK(ce.second == br);

    // (30, 30) to (0, 0), resolution: -30 (contains reference image)
    g = GeoInfo{makeGeoInfo(Coordinate{30,30}, Coordinate{0,0}, Size{1,1})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, -30);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, -30);

    ce = getCropExtents(ref, g);
    BOOST_CHECK(ce.first  == tl);
    BOOST_CHECK(ce.second == br);

    // (14, 14) to (16, 16), resolution: -1x1 (contained in reference image)
    g = GeoInfo{makeGeoInfo(Coordinate{16,14}, Coordinate{14,16})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, -1);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, 1);

    ce = getCropExtents(ref, g);
    BOOST_CHECK((ce.first  == Coordinate{14, 14}));
    BOOST_CHECK((ce.second == Coordinate{16, 16}));

    // (5, 10) to (15, 15), resolution: 1 (intersection with reference image)
    g = GeoInfo{makeGeoInfo(Coordinate{5,10}, Coordinate{15,15})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, 1);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, 1);

    ce = getCropExtents(ref, g);
    BOOST_CHECK(ce.first  == tl);
    BOOST_CHECK((ce.second == Coordinate{15, 15}));

    ce = getCropExtents(g, ref);
    BOOST_CHECK(ce.first  == tl);
    BOOST_CHECK((ce.second == Coordinate{15, 15}));

    // (4, 5) to (19, 17), resolution: -1x1 (intersection with reference image)
    g = GeoInfo{makeGeoInfo(Coordinate{19,17}, Coordinate{4,5})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, -1);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, -1);

    ce = getCropExtents(ref, g);
    BOOST_CHECK(ce.first  == tl);
    BOOST_CHECK((ce.second == Coordinate{19, 17}));

    // (0, 0) to (10, 10), resolution: 2 (touches reference image on top left corner)
    g = GeoInfo{makeGeoInfo(Coordinate{0,0}, Coordinate{10,10}, Size{5,5})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, 2);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, 2);
    BOOST_CHECK_THROW(getCropExtents(ref, g), size_error);

    // (20, 10) to (30, 20), resolution: 1x-1 (touches reference image on right edge)
    g = GeoInfo{makeGeoInfo(Coordinate{20,20}, Coordinate{30,10})};
    BOOST_CHECK_EQUAL(g.geotrans.XToX, 1);
    BOOST_CHECK_EQUAL(g.geotrans.YToY, -1);
    BOOST_CHECK_THROW(getCropExtents(ref, g), size_error);
}




BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

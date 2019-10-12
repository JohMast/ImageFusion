#include "fileformat.h"

#include <boost/test/unit_test.hpp>

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(fileformat_Suite)


BOOST_AUTO_TEST_CASE(test_support) {
    // test some basic file formats
    BOOST_CHECK(FileFormat::isSupported("GTiff"));
    BOOST_CHECK(FileFormat::isSupported("BMP"));
    BOOST_CHECK(FileFormat::isSupported("PNG"));

    // bad test
    BOOST_CHECK(!FileFormat::isSupported("bad format"));
}

BOOST_AUTO_TEST_CASE(test_singleton) {
    BOOST_CHECK_EQUAL(to_string(FileFormat::unsupported), "");
    BOOST_CHECK_EQUAL(FileFormat("blabla"), FileFormat::unsupported);
}

BOOST_AUTO_TEST_CASE(test_all_supported_formats) {
    BOOST_CHECK_NO_THROW(FileFormat::supportedFormats());
}

BOOST_AUTO_TEST_CASE(test_file_ext) {
    BOOST_CHECK_EQUAL(FileFormat::fromFileExtension(".tif"),  FileFormat("GTiff"));
    BOOST_CHECK_EQUAL(FileFormat::fromFileExtension(".tiff"), FileFormat("GTiff"));
    BOOST_CHECK_EQUAL(FileFormat::fromFileExtension(".bmp"),  FileFormat("BMP"));
    BOOST_CHECK_EQUAL(FileFormat::fromFileExtension(".png"),  FileFormat("PNG"));
    BOOST_CHECK_EQUAL(FileFormat::fromFileExtension("PnG"),   FileFormat("PNG"));

    BOOST_CHECK_EQUAL(FileFormat("BMP").fileExtension(), "bmp");
    BOOST_CHECK_EQUAL(FileFormat("GTiff").allFileExtensions(), "tif tiff");
}

BOOST_AUTO_TEST_CASE(test_filename) {
    BOOST_CHECK_EQUAL(FileFormat::fromFile("../test_resources/images/test_info_image.tif"),
                      FileFormat("GTiff"));

    BOOST_CHECK(FileFormat::fromFile("not existing file") == FileFormat::unsupported);
}

BOOST_AUTO_TEST_CASE(test_longName) {
    BOOST_CHECK_EQUAL(FileFormat("GTiff").longName(), "GeoTIFF");
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

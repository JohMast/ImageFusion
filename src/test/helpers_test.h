#pragma once

#include "Image.h"

#include <string>

#include <boost/test/unit_test.hpp>

#include <gdal.h>
#include <gdal_priv.h>

namespace imagefusion {


inline char** makeGDALMemOptions(imagefusion::ConstImage const& i) {
    char szPtrValue[128] = { '\0' };
    int nRet = CPLPrintPointer(szPtrValue,
                               reinterpret_cast<void*>(const_cast<uchar*>(i.cvMat().ptr())),
                               sizeof(szPtrValue));
    szPtrValue[nRet] = 0;

    std::string pxOff = std::to_string(i.cvMat().elemSize());
    std::string lnOff = std::to_string(i.cvMat().ptr(1) - i.cvMat().ptr(0));

    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "DATAPOINTER", szPtrValue);
    papszOptions = CSLSetNameValue(papszOptions, "PIXELOFFSET", pxOff.c_str());
    papszOptions = CSLSetNameValue(papszOptions, "LINEOFFSET",  lnOff.c_str());
    return papszOptions;
}

/* makes an multi-image-file with subdatasets
 * -parent: metadata: dom: "", key: "id", val: "parent"
 *   +sds1: metadata: dom: "", key: "id", val: "sds1"
 *   |      image: 5 x 5, uint8
 *   +sds2: metadata: dom: "", key: "id", val: "sds2"
 *   |      image: 5 x 5, uint8
 *   +sds3: metadata: dom: "", key: "id", val: "sds3"
 *   |      image: 5 x 5, uint16 (type is changed to float32, when writing NetCDF file)
 *   +sds4: metadata: dom: "", key: "id", val: "sds4"
 *   |      image: 5 x 5, uint16 (type is changed to float32, when writing NetCDF file)
 ********* below bands cannot be created with GDAL (sizes are the same for all bands by definition) *********
 *   +sds5: metadata: dom: "", key: "id", val: "sds5"
 *   |      image: 10 x 10, uint8
 *   +sds6: metadata: dom: "", key: "id", val: "sds6"
 *   |      image: 10 x 10, uint16 (type is changed to float32, when writing NetCDF file)
 */
inline bool createMultiImageFile(std::string filename) {
    GDALAllRegister();
    GDALDriver* poNetCDFDriver = GetGDALDriverManager()->GetDriverByName("NetCDF");
    if (!poNetCDFDriver) {
        BOOST_TEST_MESSAGE("failed to load NetCDF driver");
        return false;
    }

    GDALDriver* poMemDriver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!poMemDriver) {
        BOOST_TEST_MESSAGE("failed to load Mem driver");
        return false;
    }

    char** noValidation = nullptr;
//    noValidation = CSLSetNameValue(noValidation, "GDAL_VALIDATE_CREATION_OPTIONS", "NO");
    GDALDataset* poMemDS = poMemDriver->Create("", 5, 5, 0, GDT_Byte, noValidation);
    if (!poMemDS) {
        BOOST_TEST_MESSAGE("failed to create Mem dataset");
        return false;
    }
    if (noValidation)
        CSLDestroy(noValidation);

    Image img1{5, 5,   Type::uint8x1};
    Image img2{5, 5,   Type::uint8x1};
    Image img3{5, 5,   Type::uint16x1};
    Image img4{5, 5,   Type::uint16x1};
//    Image img5{10, 10, Type::uint8x1};
//    Image img6{10, 10, Type::uint16x1};
    for (int y = 0; y < img1.height(); ++y) {
        for (int x = 0; x < img1.width(); ++x) {
            img1.at<uint8_t>(x, y) = x + y * 5 + 100;
            img2.at<uint8_t>(x, y) = x + y * 5 + 200;
            img3.at<uint16_t>(x, y) = x + y * 5 + 3000;
            img4.at<uint16_t>(x, y) = x + y * 5 + 4000;

//            img5.at<uint8_t>(x*2+0, y*2+0) = x*2+0 + (y*2+0) * 5 + 40;
//            img5.at<uint8_t>(x*2+1, y*2+0) = x*2+1 + (y*2+0) * 5 + 40;
//            img5.at<uint8_t>(x*2+0, y*2+1) = x*2+0 + (y*2+1) * 5 + 40;
//            img5.at<uint8_t>(x*2+1, y*2+1) = x*2+1 + (y*2+1) * 5 + 40;

//            img6.at<uint16_t>(x*2+0, y*2+0) = x*2+0 + (y*2+0) * 5 + 6000;
//            img6.at<uint16_t>(x*2+1, y*2+0) = x*2+1 + (y*2+0) * 5 + 6000;
//            img6.at<uint16_t>(x*2+0, y*2+1) = x*2+0 + (y*2+1) * 5 + 6000;
//            img6.at<uint16_t>(x*2+1, y*2+1) = x*2+1 + (y*2+1) * 5 + 6000;
        }
    }

    CPLErr err;
    char** papszMemOptions = nullptr;
    GDALRasterBand* sds;

    papszMemOptions = makeGDALMemOptions(img1);
    err = poMemDS->AddBand(toGDALDepth(img1.basetype()), papszMemOptions);
    CSLDestroy(papszMemOptions);
    if (err != CE_None) {
        GDALClose(poMemDS);
        BOOST_TEST_MESSAGE("add band 1 error: " << err);
        return false;
    }
    sds = poMemDS->GetRasterBand(poMemDS->GetRasterCount());
    sds->SetMetadataItem("id", "sds1", "");

    papszMemOptions = makeGDALMemOptions(img2);
    err = poMemDS->AddBand(toGDALDepth(img2.basetype()), papszMemOptions);
    CSLDestroy(papszMemOptions);
    if (err != CE_None) {
        GDALClose(poMemDS);
        BOOST_TEST_MESSAGE("add band 2 error: " << err);
        return false;
    }
    sds = poMemDS->GetRasterBand(poMemDS->GetRasterCount());
    sds->SetMetadataItem("id", "sds2", "");

    papszMemOptions = makeGDALMemOptions(img3);
    err = poMemDS->AddBand(toGDALDepth(img3.basetype()), papszMemOptions);
    CSLDestroy(papszMemOptions);
    if (err != CE_None) {
        GDALClose(poMemDS);
        BOOST_TEST_MESSAGE("add band 3 error: " << err);
        return false;
    }
    sds = poMemDS->GetRasterBand(poMemDS->GetRasterCount());
    sds->SetMetadataItem("id", "sds3", "");

    papszMemOptions = makeGDALMemOptions(img4);
    err = poMemDS->AddBand(toGDALDepth(img4.basetype()), papszMemOptions);
    CSLDestroy(papszMemOptions);
    if (err != CE_None) {
        GDALClose(poMemDS);
        BOOST_TEST_MESSAGE("add band 4 error: " << err);
        return false;
    }
    sds = poMemDS->GetRasterBand(poMemDS->GetRasterCount());
    sds->SetMetadataItem("id", "sds4", "");

//    gi.setMetadataItem("", "id", "sds5");
//    gi.geotrans.scaleImage(4, 4);
//    papszMemOptions = makeGDALMemOptions(img5);
//    err = poMemDS->AddBand(toGDALDepth(img5.basetype()), papszMemOptions);
//    CSLDestroy(papszMemOptions);
//    if (err != CE_None) {
//        BOOST_TEST_MESSAGE("add band 5 error: " << err);
//        return false;
//    }
//    sds = poMemDS->GetRasterBand(poMemDS->GetRasterCount());
//    sds->SetMetadataItem("id", "sds5", "");

//    gi.setMetadataItem("", "id", "sds6");
//    gi.geotrans.scaleImage(0.5, 0.5);
//    papszMemOptions = makeGDALMemOptions(img6);
//    err = poMemDS->AddBand(toGDALDepth(img6.basetype()), papszMemOptions);
//    CSLDestroy(papszMemOptions);
//    if (err != CE_None) {
//        BOOST_TEST_MESSAGE("add band 6 error: " << err);
//        return false;
//    }
//    sds = poMemDS->GetRasterBand(poMemDS->GetRasterCount());
//    sds->SetMetadataItem("id", "sds6", "");

    char** papszNetCDFOptions = nullptr;
    papszNetCDFOptions = CSLSetNameValue(papszNetCDFOptions, "FORMAT", "NC4"); // required to store uint16
    GDALDataset* poDstDS = poNetCDFDriver->CreateCopy(filename.c_str(), poMemDS, FALSE, papszNetCDFOptions, nullptr, nullptr);
    CSLDestroy(papszNetCDFOptions);
    if (!poDstDS) {
        GDALClose(poMemDS);
        BOOST_TEST_MESSAGE("Could not create container file.");
        return false;
    }
    poDstDS->SetMetadataItem("NC_GLOBAL#id", "parent", ""); // currently not supported, see https://trac.osgeo.org/gdal/wiki/NetCDF_Improvements#Issueswiththecurrentimplementation1
    poDstDS->SetMetadataItem("id", "parent", ""); // currently not supported, see https://trac.osgeo.org/gdal/wiki/NetCDF_Improvements#Issueswiththecurrentimplementation1

    GDALClose(poDstDS);
    GDALClose(poMemDS);
    return true;
}


} /* namespace imagefusion */

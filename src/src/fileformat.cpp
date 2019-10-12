#include "fileformat.h"

#include <gdal.h>
#include <gdal_priv.h>
#include <boost/filesystem.hpp>

namespace imagefusion {

FileFormat const FileFormat::unsupported{};

bool FileFormat::isSupported(std::string const& fmtStr) {
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(fmtStr.c_str());
    return driver != nullptr;
}

std::string FileFormat::fileExtension() const {
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.c_str());
    char const* fileExtension_c_str = driver->GetMetadataItem(GDAL_DMD_EXTENSION);
    if (fileExtension_c_str == nullptr)
        return "";

    return fileExtension_c_str;
}

std::string FileFormat::allFileExtensions() const {
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.c_str());
    char const* fileExtensions_c_str = driver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
    if (fileExtensions_c_str == nullptr)
        return fileExtension();

    return fileExtensions_c_str;
}

std::string FileFormat::longName() const {
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.c_str());
    char const* longName_c_str = driver->GetMetadataItem(GDAL_DMD_LONGNAME);
    if (longName_c_str == nullptr)
        return "";

    return longName_c_str;
}

FileFormat FileFormat::fromFileExtension(std::string fileExt) {
    std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
    if (fileExt.front() == '.')
        fileExt = fileExt.substr(1);

    GDALAllRegister();
    GDALDriverManager* dMng = GetGDALDriverManager();
    int dCount = dMng->GetDriverCount();
    for (int i = 0; i < dCount; ++i) {
        GDALDriver* d = dMng->GetDriver(i);
        char const* fileExtensions_c_str = d->GetMetadataItem(GDAL_DMD_EXTENSIONS);
        if (fileExtensions_c_str == nullptr)
            continue;

        std::string fileExtensions = fileExtensions_c_str;
        std::transform(fileExtensions.begin(), fileExtensions.end(), fileExtensions.begin(), ::tolower);

        std::istringstream stream{fileExtensions};
        std::string singleExt;
        while (stream >> singleExt) {
            if (singleExt == fileExt) {
                FileFormat fileFormat;
                fileFormat.driverName = d->GetDescription();
                return fileFormat;
            }
        }
    }
    return unsupported;
}

std::vector<FileFormat> FileFormat::supportedFormats() {
    std::vector<FileFormat> v;
    GDALAllRegister();
    GDALDriverManager* dMng = GetGDALDriverManager();
    int dCount = dMng->GetDriverCount();
    for (int i = 0; i < dCount; ++i) {
        GDALDriver* d = dMng->GetDriver(i);
        FileFormat f;
        f.driverName = d->GetDescription();
        v.push_back(f);
    }
    return v;
}

FileFormat FileFormat::fromFile(std::string filename) {
    // open file
    GDALAllRegister();
    GDALDataset* gdal_img = (GDALDataset*) GDALOpen(filename.c_str(), GA_ReadOnly);
    if (!gdal_img)
        return unsupported;

    // get driver name
    FileFormat f;
    f.driverName = gdal_img->GetDriverName();
    GDALClose(gdal_img);
    return f;
}


} /* namespace imagefusion */

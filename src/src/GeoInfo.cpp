#include <cstring>
#include <cmath>
#include <cassert>

#include <filesystem>

#include <gdal.h>
#include <gdal_priv.h>
#include <gdalwarper.h>
#include <gdal_utils.h>
#include <cpl_string.h>

#include "GeoInfo.h"
#include "exceptions.h"

namespace {

std::vector<imagefusion::Coordinate> convertProj(imagefusion::GeoInfo const& giSrc, imagefusion::GeoInfo const& giDst, std::vector<imagefusion::Coordinate> const& coords, bool srcImg, bool dstImg) {
    // collect coordinates
    int nSamplePoints = coords.size();
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> z(nSamplePoints, 0.0);
    std::vector<int> succ(nSamplePoints);
    x.reserve(nSamplePoints);
    y.reserve(nSamplePoints);
    for (imagefusion::Coordinate const& c : coords) {
        x.push_back(c.x);
        y.push_back(c.y);
    }

    // build the transformer object
    char* pszSrcWKT = nullptr;
    giSrc.geotransSRS.exportToWkt(&pszSrcWKT);
    if (std::strlen(pszSrcWKT) <= 0) {
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "Please provide a valid source projection reference system for reprojection."));
    }

    char* pszDstWKT = nullptr;
    giDst.geotransSRS.exportToWkt(&pszDstWKT);
    if (std::strlen(pszDstWKT) <= 0) {
        if (pszSrcWKT)
            CPLFree(pszSrcWKT);
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "Please provide a valid destination projection reference system for reprojection."));
    }

    double const* padfSrcGeoTransform = srcImg ? giSrc.geotrans.values.data() : nullptr;
    double const* padfDstGeoTransform = dstImg ? giDst.geotrans.values.data() : nullptr;

    void* pTransformer = GDALCreateGenImgProjTransformer3(pszSrcWKT, padfSrcGeoTransform,
                                                          pszDstWKT, padfDstGeoTransform);

    // transform in place
    constexpr int fromDstToSrc = FALSE;
    GDALGenImgProjTransform(pTransformer, fromDstToSrc, nSamplePoints,
                            x.data(), y.data(), z.data(), succ.data());
    CPLFree(pszSrcWKT);
    CPLFree(pszDstWKT);
    GDALDestroyGenImgProjTransformer(pTransformer);

    // convert to coordinates again
    std::vector<imagefusion::Coordinate> ret;
    ret.reserve(nSamplePoints);
    for (int i = 0; i < nSamplePoints; ++i)
        ret.emplace_back(x.at(i), y.at(i));
    return ret;
}


std::vector<imagefusion::Coordinate> convertLongLat(imagefusion::GeoInfo const& gi, std::vector<imagefusion::Coordinate> const& coords, bool imgCoords /* otherwise proj coords */, bool reverse /* from long lat? */) {
    // collect coordinates
    int nSamplePoints = coords.size();
    std::vector<double> x;
    std::vector<double> y;
    x.reserve(nSamplePoints);
    y.reserve(nSamplePoints);
    for (imagefusion::Coordinate c : coords) {
        if (imgCoords && !reverse)
            c = gi.geotrans.imgToProj(c);
        x.push_back(c.x);
        y.push_back(c.y);
    }

    // prepare long/lat target CRS, see https://gdal.org/tutorials/osr_api_tut.html#coordinate-transformation
    OGRSpatialReference* poLongLat = gi.geotransSRS.CloneGeogCS();
    #if GDAL_VERSION_MAJOR >= 3
        poLongLat->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    #endif

    // build the transformer object
    OGRCoordinateTransformation* poTransform;
    if (reverse) {
        poTransform = OGRCreateCoordinateTransformation(poLongLat,
                                                        const_cast<OGRSpatialReference*>(&gi.geotransSRS));
    }
    else {
        poTransform = OGRCreateCoordinateTransformation(const_cast<OGRSpatialReference*>(&gi.geotransSRS),
                                                        poLongLat);
    }

    if (poTransform == nullptr)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "Cannot make the transformation to or from latitude / longitude. Please provide a valid projection reference system."));

    // transform in place
    if (!poTransform->Transform(nSamplePoints, x.data(), y.data())) {
        OGRCoordinateTransformation::DestroyCT(poTransform);
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                "Could not transform to or from latitude / longitude. Don't know why, sorry! Maybe the points are given in the wrong coordinate space and thus are not in the expected range."));
    }

    OGRCoordinateTransformation::DestroyCT(poTransform);
    OGRSpatialReference::DestroySpatialReference(poLongLat);

    // convert to coordinates again
    std::vector<imagefusion::Coordinate> ret;
    ret.reserve(nSamplePoints);
    if (imgCoords && reverse)
        for (int i = 0; i < nSamplePoints; ++i)
            ret.emplace_back(gi.geotrans.projToImg({x.at(i), y.at(i)}));
    else
        for (int i = 0; i < nSamplePoints; ++i)
            ret.emplace_back(x.at(i), y.at(i));
    return ret;
}

} /* anonymous namespace */



namespace imagefusion {

void GeoInfo::readFrom(std::string const& filename) {
    GDALDataset* img = (GDALDataset*) GDALOpen(filename.c_str(), GA_ReadOnly);
    if (!img)
        IF_THROW_EXCEPTION(runtime_error("Could not open image '" + filename + "' with GDAL to read its GeoInfo. "
                                         "Either the file does not exist or GDAL could not find an appropriate driver to read the image"))
                << boost::errinfo_file_name(filename);

    readFrom(img);
    GDALClose(img);
}

void GeoInfo::readFrom(GDALDataset const* img_arg) {
    if (!img_arg)
        IF_THROW_EXCEPTION(runtime_error("The GDALDataset from which the GeoInfo should be read is nullptr."));

    GDALDataset* img = const_cast<GDALDataset*>(img_arg);

    // filename
    char** fileList = img->GetFileList();
    if (CSLCount(fileList) > 0)
        filename = CSLGetField(fileList, 0);
    if (fileList)
        CSLDestroy(fileList);

    // determine size, channels, baseType, colorTable
    size.width = img->GetRasterXSize();
    size.height = img->GetRasterYSize();
    channels = img->GetRasterCount();
    GDALColorTable* ct = nullptr;
    if (channels > 0) {
        GDALDataType gdal_type = img->GetRasterBand(1)->GetRasterDataType();
        baseType = toBaseType(gdal_type);
        for (int i = 2; i <= channels; ++i) {
            if (img->GetRasterBand(i)->GetRasterDataType() != gdal_type) {
                baseType = Type::invalid;
                break;
            }
        }

        ct = img->GetRasterBand(1)->GetColorTable();
        if (ct) {
            if (channels != 1)
                IF_THROW_EXCEPTION(file_format_error("Color indexed images are only supported for single-index images. This image has " + std::to_string(channels) + " indices."))
                        << boost::errinfo_file_name(filename);
            if (baseType != Type::uint8)
                IF_THROW_EXCEPTION(file_format_error("This is a color indexed image, which is not of type uint8 (or GDT_Byte). I did not know this is possible. Please help me to fix this!"))
                        << boost::errinfo_file_name(filename);
            int ctsize = ct->GetColorEntryCount();
            colorTable.clear();
            colorTable.reserve(ctsize);
            for (int i = 0; i < ctsize; ++i) {
                GDALColorEntry const* ce = ct->GetColorEntry(i);
                colorTable.push_back(std::array<short,4>{ce->c1, ce->c2, ce->c3, ce->c4});
            }
        }
    }
    else
        baseType = Type::invalid;

    // no data values
    clearNodataValues();
    bool hasAnyNodataVals = false;
    for (int i = 1; i <= channels; ++i) {
        int hasNodataVal;
        double nodataVal = img->GetRasterBand(i)->GetNoDataValue(&hasNodataVal);
        if (hasNodataVal) {
            hasAnyNodataVals = true;
            setNodataValue(nodataVal, i-1);
        }
    }
    if (hasAnyNodataVals)
        nodataValues.resize(channels, std::numeric_limits<double>::quiet_NaN());

    // geo transform and projection
    CPLErr errorGeotransform = img->GetGeoTransform(geotrans.values.data());

    const char* geotransProjStr = img->GetProjectionRef();
    if (errorGeotransform == CE_None && std::strlen(geotransProjStr) > 0) {
        geotransSRS.SetFromUserInput(geotransProjStr);
//        geotransSRS.importFromWkt(const_cast<char**>(&geotransProjStr)); // same but const cast required!?
    }


    // ground control points and projection
    int gcp_count = img->GetGCPCount();
    gcps.clear();
    if (gcp_count > 0) {
        GDAL_GCP const* gdal_gcps = img->GetGCPs();
        for (int i = 0; i < gcp_count; ++i) {
            gcps.push_back(GCP{gdal_gcps->pszId,
                               gdal_gcps->pszInfo,
                               gdal_gcps->dfGCPPixel,
                               gdal_gcps->dfGCPLine,
                               gdal_gcps->dfGCPX,
                               gdal_gcps->dfGCPY,
                               gdal_gcps->dfGCPZ});
            gdal_gcps++;
        }

        const char* gcpProjStr = img->GetGCPProjection();
        if (std::strlen(gcpProjStr) > 0)
            gcpSRS.SetFromUserInput(gcpProjStr);
    }

    // remaining meta data
    char** domains = img->GetMetadataDomainList();
    if (domains != nullptr) {
        for (char** dom = domains; *dom != nullptr; ++dom) {
            char** metaitems = img->GetMetadata(*dom);
            if (metaitems != nullptr) {
                for (char** item = metaitems; *item != nullptr; ++item) {
                    char* key;
                    const char* value = CPLParseNameValue(*item, &key);
                    metadata[std::string(*dom)][std::string(key)] = std::string(value);
                    VSIFree(key);
                }
            }
        }
        CSLDestroy(domains);
    }
}

void GeoInfo::addTo(std::string const& filename) const {
    if (!std::filesystem::exists(filename))
        IF_THROW_EXCEPTION(not_found_error("Could not find any file at path " + filename + " to add GeoInfo to it."))
                << boost::errinfo_file_name(filename);

    GDALDataset* img = (GDALDataset*) GDALOpen(filename.c_str(), GA_Update);
    if (!img)
        IF_THROW_EXCEPTION(runtime_error("The corresponding GDAL driver does not support update of metadata for this image type."));

    addTo(img);
    bool shouldHaveWrittenColorTable = !colorTable.empty() && img->GetRasterCount() == 1 && img->GetRasterBand(1)->GetRasterDataType() == GDT_Byte;
    GDALClose(img);

    // check for changed color table and warn
    if (shouldHaveWrittenColorTable) {
        GeoInfo test{filename};
        compareColorTables(test, false);
    }
}

void GeoInfo::addTo(GDALDataset* img) const {
    if (!img)
        IF_THROW_EXCEPTION(runtime_error("The GDALDataset to which the GeoInfo should be added is nullptr."));

    // nodata values
    if (!nodataValues.empty()) {
        std::size_t rastercount = img->GetRasterCount();
        for (std::size_t channel = 0; channel < rastercount; ++channel) {
            std::size_t ndChan = std::min(channel, nodataValues.size() - 1);
            double nodataVal = getNodataValue(ndChan);
            if (!std::isnan(nodataVal))
                img->GetRasterBand(channel+1)->SetNoDataValue(nodataVal);
        }
    }

    // write color table only for uint8x1 images
    // GDAL GTiff driver DOES CHANGE THE ALPHA CHANNEL TO 255 FOR ALL INDICES EXCEPT NODATA WHERE IT WILL BE CHANGED TO 0 ==> cannot write an arbitrary color table
    if (!colorTable.empty() && img->GetRasterCount() == 1 && img->GetRasterBand(1)->GetRasterDataType() == GDT_Byte) {
        GDALColorTable ct;
        for (unsigned int i = 0; i < colorTable.size(); ++i) {
            std::array<short, 4> const& col = colorTable.at(i);
            GDALColorEntry* ce = const_cast<GDALColorEntry*>(reinterpret_cast<GDALColorEntry const*>(&col));
            ct.SetColorEntry(i, ce);
        }
        img->GetRasterBand(1)->SetColorTable(&ct);
    }

    // geo transform (will only be used if it is not an identity transform)
    bool useGeotransform = hasGeotransform();
    if (useGeotransform) {
        img->SetGeoTransform(const_cast<double*>(geotrans.values.data())); // const cast is fine, SetGeoTransform will not change geotransform

        char* projStr;
        geotransSRS.exportToWkt(&projStr);
        if (std::strlen(projStr) > 0)
            img->SetProjection(projStr);
        CPLFree(projStr);
    }

    // ground control points (set only, if not using geotransform, since geotransform is more common)
    bool useGCPs = !gcps.empty() && const_cast<OGRSpatialReference&>(gcpSRS).Validate() == OGRERR_NONE;
    if (useGCPs && !useGeotransform) {
        std::vector<GDAL_GCP> gdal_gcps;
        for (GCP const& gcp : gcps) {
            gdal_gcps.push_back(GDAL_GCP{const_cast<char*>(gcp.id.c_str()),
                                         const_cast<char*>(gcp.info.c_str()),
                                         gcp.pixel,
                                         gcp.line,
                                         gcp.x,
                                         gcp.y,
                                         gcp.z});
        }

        if (!gdal_gcps.empty()) {
            char* projStr;
            gcpSRS.exportToWkt(&projStr);
            img->SetGCPs(gdal_gcps.size(), gdal_gcps.data(), projStr);
            CPLFree(projStr);
        }
    }

    // remaining meta data
    for (auto const& mp : metadata) {
        const char* domain = mp.first.c_str();
        for (auto const& item : mp.second) {
            const char* key = item.first.c_str();
            const char* val = item.second.c_str();
            img->SetMetadataItem(key, val, domain);
        }
    }
}


bool GeoInfo::compareColorTables(GeoInfo const& other, bool quiet) const {
    if (colorTable.empty())
        return true;

    if (!colorTable.empty() && other.colorTable.empty()) {
        if (!quiet)
            std::cerr << "Warning: The color table has been removed by the driver!" << std::endl;
        return false;
    }

    if (colorTable.size() > other.colorTable.size()) {
        if (!quiet)
            std::cerr << "Warning: Not all entries of the color tables were written by the driver!" << std::endl;
        return false;
    }

    // check which channels have been changed and if alpha has been changed to 255 except for nodata index
    std::array<bool, 4> hasChanged = {false, false, false, false};
    bool allNonNodataAlpha255Before = true;
    bool allNonNodataAlpha255After = true;
    for (unsigned int i = 0; i < colorTable.size(); ++i) {
        std::array<short, 4> const& before = colorTable.at(i);
        std::array<short, 4> const& after  = other.colorTable.at(i);

        if (!hasNodataValue() || i != getNodataValue())
            if (before.back() != 255)
                allNonNodataAlpha255Before = false;
        if (!other.hasNodataValue() || i != other.getNodataValue())
            if (after.back() != 255)
                allNonNodataAlpha255After = false;

        for (unsigned int c = 0; c < 4; ++c) {
            hasChanged.at(c) = before.at(c) != after.at(c);

            // if we don't need a warning we can quit directly
            if (quiet && hasChanged.at(c))
                return false;
        }
    }

    bool nodataAlpha0Before = true;
    bool nodataAlpha0After = true;
    if (hasNodataValue() && colorTable.at(getNodataValue()).back() != 0)
        nodataAlpha0Before = false;
    if (other.hasNodataValue() && other.colorTable.at(other.getNodataValue()).back() != 0)
        nodataAlpha0After = false;

    // find and print reasons
    bool areCompatible = true;
    std::string warning = "Warning: The driver changed the color table. ";
    for (unsigned int c = 0; c < 4; ++c) {
        if (hasChanged.at(c)) {
            if (areCompatible)
                warning = "Channel(s) ";
            warning += std::to_string(c) + ", ";
            areCompatible = false;
        }
    }

    if (!areCompatible) {
        warning.pop_back(); warning.pop_back();
        warning += " has/have changed. ";
    }

    if (!nodataAlpha0Before && nodataAlpha0After)
        warning += "The nodata entry's alpha value has been changed to 0. ";

    if (!allNonNodataAlpha255Before && allNonNodataAlpha255After)
        warning += "All non-nodata entries' alpha values have been changed to 255.";

    std::cerr << warning << std::endl;

    return areCompatible;
}


GeoInfo GeoInfo::subdatasetGeoInfo(unsigned int idx) const {
    if (!hasMetadataDomain("SUBDATASETS"))
        IF_THROW_EXCEPTION(file_format_error("The file does not contain any subdatasets. "
                                             "Hence, cannot collect the GeoInfo of subdataset "
                                             + std::to_string(idx)));

    auto const& metaSDS = getMetadataItems("SUBDATASETS");
    unsigned int numSDS = metaSDS.size() / 2; // for each subdataset there is a name and a description item
    if (idx >= numSDS)
        IF_THROW_EXCEPTION(invalid_argument_error("The file contains " + std::to_string(numSDS) + " subdatasets. "
                                                  "You tried to obtain subdataset " + std::to_string(idx) +
                                                  ", which does not exist."));

    std::string const& name = metaSDS.at("SUBDATASET_" + std::to_string(idx + 1) + "_NAME");
    GDALDataset* sds = (GDALDataset*) GDALOpen(name.c_str(), GA_ReadOnly);
    if (!sds)
        IF_THROW_EXCEPTION(runtime_error("GDAL could not open the subdataset with index " + std::to_string(idx) + " and virtual filename " + name));

    GeoInfo gi;
    gi.readFrom(sds);
    return gi;
}


GDALDataset* GeoInfo::openVrtGdalDataset(std::vector<int> const& selChans, InterpMethod interp) const {
    // collect subdataset names
    auto sds = subdatasets();
    if (sds.empty())
        IF_THROW_EXCEPTION(image_type_error("The file does not contain any subdatasets."));

    char** papszSrcDSNames = nullptr;
    int bandCount = selChans.empty() ? sds.size() : selChans.size();
    if (selChans.empty())
        for (unsigned int c = 0; c < sds.size(); ++c)
            papszSrcDSNames = CSLAddString(papszSrcDSNames, sds.at(c).first.c_str());
    else {
        if (selChans.size() == 1) {
            int c = selChans.front();
            if (c >= static_cast<int>(sds.size()) || c < 0)
                IF_THROW_EXCEPTION(image_type_error("You acquired subdataset " + std::to_string(c)
                                                    + " that does not exist. The image only has "
                                                    + std::to_string(sds.size()) + " subdatasets."));
            return (GDALDataset*) GDALOpen(sds.at(c).first.c_str(), GA_ReadOnly);
        }

        for (int c : selChans) {
            if (c >= static_cast<int>(sds.size()) || c < 0)
                IF_THROW_EXCEPTION(image_type_error("You acquired subdataset " + std::to_string(c)
                                                    + " that does not exist. The image only has "
                                                    + std::to_string(sds.size()) + " subdatasets."));
            papszSrcDSNames = CSLAddString(papszSrcDSNames, sds.at(c).first.c_str());
        }
    }

    // set some default options
    char** papszVRTopts = nullptr;
    papszVRTopts = CSLAddString(papszVRTopts, "-resolution");
    papszVRTopts = CSLAddString(papszVRTopts, "highest");
    papszVRTopts = CSLAddString(papszVRTopts, "-separate");
    papszVRTopts = CSLAddString(papszVRTopts, "-r");
    if (interp == InterpMethod::nearest)
        papszVRTopts = CSLAddString(papszVRTopts, "nearest");
    else if (interp == InterpMethod::bilinear)
        papszVRTopts = CSLAddString(papszVRTopts, "bilinear");
    else if (interp == InterpMethod::cubic)
        papszVRTopts = CSLAddString(papszVRTopts, "cubic");
    else // interp == InterpMethod::cubicspline
        papszVRTopts = CSLAddString(papszVRTopts, "cubicspline");
    GDALBuildVRTOptions* vrtOpts = GDALBuildVRTOptionsNew(papszVRTopts, nullptr);
    CSLDestroy(papszVRTopts);

    int bUsageError = FALSE;
    GDALDataset* poImg = (GDALDataset*) GDALBuildVRT(/* pszDest */ nullptr, bandCount, /* pahSrcDS */ nullptr, papszSrcDSNames, vrtOpts, &bUsageError);
    CSLDestroy(papszSrcDSNames);
    GDALBuildVRTOptionsFree(vrtOpts);
    if (bUsageError) {
        GDALClose(poImg);
        IF_THROW_EXCEPTION(runtime_error("Some error occurred while combinding the subdatasets. Possible reasons "
                                         "include they have different data types, projection coordinate systems, etc. "
                                         "Maybe there is another error or warning message from GDAL, which known more."));
    }
    return poImg;
}


GeoInfo::GeoInfo(std::string const& filename, std::vector<int> const& selChans, Rectangle crop, bool flipH, bool flipV)
    : GeoInfo(filename)
{
    if (hasSubdatasets())
        channels = subdatasetsCount();

    if (!selChans.empty()) {
        for (int const& c : selChans)
            if (c >= static_cast<int>(channels) || c < 0)
                IF_THROW_EXCEPTION(image_type_error("You acquired a channel (" + std::to_string(c)
                                                    + ") that does not exist. The image only has "
                                                    + std::to_string(channels) + " channels."))
                        << boost::errinfo_file_name(filename);
        channels = selChans.size();
    }

    if (hasSubdatasets()) {
        GDALDataset* poVrtImg = openVrtGdalDataset(selChans);
        *this = GeoInfo{};
        readFrom(poVrtImg);
        GDALClose(poVrtImg);
    }

    crop.x = std::min(std::max(crop.x, 0), width());
    crop.y = std::min(std::max(crop.y, 0), height());

    if (crop.width == 0)
        crop.width = width() - crop.x;

    if (crop.height == 0)
        crop.height = height() - crop.y;

    size.width  = crop.width;
    size.height = crop.height;

    if (hasGeotransform()) {
        geotrans.translateImage(crop.x, crop.y);
        geotrans.flipImage(flipH, flipV, size);
    }
}


CoordRectangle intersectRect(GeoInfo const& refGI, CoordRectangle const& refRect, GeoInfo const& otherGI, CoordRectangle const& otherRect, unsigned int numPoints) {
    if (refRect.width <= 0 || refRect.height <= 0 || otherRect.width <= 0 || otherRect.height <= 0 )
        return {};
    if (numPoints <= 0)
        return {};

    // collect source boundaries
    std::vector<Coordinate> boundariesRef(4 * numPoints);
    double relstep = 1. / (numPoints - 1);
    for (unsigned int i = 0; i < numPoints; ++i) {
        double t = i * relstep;
        // top
        boundariesRef.at(i)               = Coordinate(refRect.x + t * refRect.width, refRect.y);
        // right
        boundariesRef.at(i + numPoints)   = Coordinate(refRect.x + refRect.width,     refRect.y + t * refRect.height);
        // bottom
        boundariesRef.at(i + 2*numPoints) = Coordinate(refRect.x + t * refRect.width, refRect.y + refRect.height);
        // left
        boundariesRef.at(i + 3*numPoints) = Coordinate(refRect.x,                     refRect.y + t * refRect.height);
    }

    // transform to other projection coordinate space and restrict by other rectangle
    std::vector<Coordinate> boundariesOther = refGI.projToProj(boundariesRef, otherGI);
    Coordinate otherBR = otherRect.br();
    for (Coordinate& c : boundariesOther) {
        // note: this corresponds to a projection onto the otherRect boundary and can give points that are out of refRect!
        c.x = std::min(std::max(c.x, otherRect.x), otherBR.x);
        c.y = std::min(std::max(c.y, otherRect.y), otherBR.y);
    }

    // transform back
    boundariesRef = otherGI.projToProj(boundariesOther, refGI);

    // intersect (enclose all points by a rectangle)
    auto compareX = [] (imagefusion::Coordinate const& c1, imagefusion::Coordinate const& c2) {
        return c1.x < c2.x;
    };
    auto compareY = [] (imagefusion::Coordinate const& c1, imagefusion::Coordinate const& c2) {
        return c1.y < c2.y;
    };
    auto minmaxTopY    = std::minmax_element(std::begin(boundariesRef),               std::begin(boundariesRef) + numPoints,   compareY);
    auto minmaxRightX  = std::minmax_element(std::begin(boundariesRef) + numPoints,   std::begin(boundariesRef) + 2*numPoints, compareX);
    auto minmaxBottomY = std::minmax_element(std::begin(boundariesRef) + 2*numPoints, std::begin(boundariesRef) + 3*numPoints, compareY);
    auto minmaxLeftX   = std::minmax_element(std::begin(boundariesRef) + 3*numPoints, std::begin(boundariesRef) + 4*numPoints, compareX);

    assert(minmaxLeftX.first->x + minmaxLeftX.second->x < minmaxRightX.first->x  + minmaxRightX.second->x);
    assert(minmaxTopY.first->y  + minmaxTopY.second->y  < minmaxBottomY.first->y + minmaxBottomY.second->y);

    double leftX   = minmaxLeftX.first->x;
    double rightX  = minmaxRightX.second->x;
    double topY    = minmaxTopY.first->y;
    double bottomY = minmaxBottomY.second->y;

    // check for empty intersection
    bool isWindowLegal = leftX < rightX && topY < bottomY;
    if (!isWindowLegal)
        return {};

    // the new bounds can be larger than refRect, so take refRect as limit
    CoordRectangle projectedLimitation{leftX, topY, rightX - leftX, bottomY - topY};
    return projectedLimitation & refRect;
}

void GeoInfo::intersect(GeoInfo const& other, unsigned int numCoords, bool shrink) {
    // find intersection
    CoordRectangle inter = intersectRect(*this, projRect(), other, other.projRect(), numCoords);
    if (inter.area() <= 0) {
        size = Size{};
        return;
    }

    setExtents(inter, shrink);
}

void GeoInfo::setExtents(CoordRectangle const& ex, bool shrink) {
    assert(geotrans.XToY == 0 && geotrans.YToX && "Only simple transformations supported.");

    // get top left and bottom right corners
    Coordinate projCornerTL = ex.tl();
    Coordinate projCornerBR = ex.br();
    if (geotrans.XToX < 0)
        std::swap(projCornerTL.x, projCornerBR.x);
    if (geotrans.YToY < 0)
        std::swap(projCornerTL.y, projCornerBR.y);

    Coordinate imgCornerTL = geotrans.projToImg(projCornerTL);
    Coordinate imgCornerBR = geotrans.projToImg(projCornerBR);

    // find new size
    constexpr double abstol = 1e-11;
    double newWidth  = imgCornerBR.x - imgCornerTL.x;
    double newHeight = imgCornerBR.y - imgCornerTL.y;
    if (shrink) {
        newWidth  = std::floor(newWidth  + abstol);
        newHeight = std::floor(newHeight + abstol);
    }
    else {
        newWidth  = std::ceil(newWidth  - abstol);
        newHeight = std::ceil(newHeight - abstol);
    }

    // if one corner did not change, try to preserve it, if all changed take top left
    bool alignLeftX = false; // otherwise to right x
    if (std::abs(imgCornerTL.x) < abstol || std::abs(imgCornerBR.x - width()) >= abstol)
        alignLeftX = true; // default, when both changed
    else
        imgCornerTL.x = imgCornerBR.x - newWidth;

    bool alignTopY = false; // otherwise to bottom y
    if (std::abs(imgCornerTL.y) < abstol || std::abs(imgCornerBR.y - height()) >= abstol)
        alignTopY = true; // default, when both changed
    else
        imgCornerTL.y = imgCornerBR.y - newHeight;

    if (!alignLeftX || !alignTopY)
        projCornerTL = geotrans.imgToProj(imgCornerTL);

    // save extents
    size.width  = newWidth;
    size.height = newHeight;

    geotrans.offsetX = projCornerTL.x;
    geotrans.offsetY = projCornerTL.y;
}

GeoTransform& GeoTransform::clear() {
    offsetX = 0;
    offsetY = 0;
    XToX    = 1;
    YToX    = 0;
    XToY    = 0;
    YToY    = 1;

    return *this;
}


Coordinate GeoInfo::imgToProj(Coordinate const& c_i, GeoInfo const& to) const {
    return imgToProj(std::vector<Coordinate>{c_i}, to).front();
}

Coordinate GeoInfo::projToImg(Coordinate const& c_p, GeoInfo const& to) const {
    return projToImg(std::vector<Coordinate>{c_p}, to).front();
}

Coordinate GeoInfo::imgToImg(Coordinate const& c_i, GeoInfo const& to) const {
    return imgToImg(std::vector<Coordinate>{c_i}, to).front();
}

Coordinate GeoInfo::projToProj(Coordinate const& c_p, GeoInfo const& to) const {
    return projToProj(std::vector<Coordinate>{c_p}, to).front();
}

std::vector<Coordinate> GeoInfo::imgToProj(std::vector<Coordinate> const& c_i, GeoInfo const& to) const {
    return convertProj(*this, to, c_i, true, false);
}

std::vector<Coordinate> GeoInfo::projToImg(std::vector<Coordinate> const& c_p, GeoInfo const& to) const {
    return convertProj(*this, to, c_p, false, true);
}

std::vector<Coordinate> GeoInfo::imgToImg(std::vector<Coordinate> const& c_i, GeoInfo const& to) const {
    return convertProj(*this, to, c_i, true, true);
}

std::vector<Coordinate> GeoInfo::projToProj(std::vector<Coordinate> const& c_p, GeoInfo const& to) const {
    return convertProj(*this, to, c_p, false, false);
}

Coordinate GeoInfo::imgToLongLat(Coordinate const& c_i) const {
    return imgToLongLat(std::vector<Coordinate>{c_i}).front();
}

Coordinate GeoInfo::projToLongLat(Coordinate const& c_p) const {
    return projToLongLat(std::vector<Coordinate>{c_p}).front();
}

Coordinate GeoInfo::longLatToProj(Coordinate const& c_l) const {
    return longLatToProj(std::vector<Coordinate>{c_l}).front();
}

Coordinate GeoInfo::longLatToImg(Coordinate const& c_l) const {
    return longLatToImg(std::vector<Coordinate>{c_l}).front();
}

std::vector<Coordinate> GeoInfo::imgToLongLat(std::vector<Coordinate> const& c_i) const {
    return convertLongLat(*this, c_i, true, false);
}

std::vector<Coordinate> GeoInfo::projToLongLat(std::vector<Coordinate> const& c_p) const {
    return convertLongLat(*this, c_p, false, false);
}

std::vector<Coordinate> GeoInfo::longLatToProj(std::vector<Coordinate> const& c_l) const {
    return convertLongLat(*this, c_l, false, true);
}

std::vector<Coordinate> GeoInfo::longLatToImg(std::vector<Coordinate> const& c_l) const {
    return convertLongLat(*this, c_l, true, true);
}

Coordinate GeoTransform::imgToProj(Coordinate const& c_i) const {
    return Coordinate{
        offsetX + XToX * (c_i.x) + YToX * (c_i.y),
        offsetY + XToY * (c_i.x) + YToY * (c_i.y)
    };
}

Coordinate GeoTransform::projToImg(Coordinate const& c_p) const {
    double const x = c_p.x - offsetX;
    double const y = c_p.y - offsetY;
    double const det = XToX * YToY - XToY * YToX;

    return Coordinate{
       ( YToY * x - YToX * y) / det,
       (-XToY * x + XToX * y) / det
    };
}

CoordRectangle GeoTransform::imgToProj(CoordRectangle const& r_i) const {
    Coordinate projTL = imgToProj(r_i.tl());
    Coordinate projBR = imgToProj(r_i.br());
    return CoordRectangle{std::min(projTL.x,  projBR.x), std::min(projTL.y,  projBR.y),
                          std::abs(projTL.x - projBR.x), std::abs(projTL.y - projBR.y)};
}

CoordRectangle GeoTransform::projToImg(CoordRectangle const& r_p) const {
    Coordinate imgTL = projToImg(r_p.tl());
    Coordinate imgBR = projToImg(r_p.br());
    return CoordRectangle{std::min(imgTL.x,  imgBR.x), std::min(imgTL.y,  imgBR.y),
                          std::abs(imgTL.x - imgBR.x), std::abs(imgTL.y - imgBR.y)};
}

GeoTransform& GeoTransform::scaleProjection(double xscale, double yscale) {
    offsetX *= xscale;
    offsetY *= yscale;
    XToX    *= xscale;
    YToX    *= xscale;
    XToY    *= yscale;
    YToY    *= yscale;

    return *this;
}

GeoTransform& GeoTransform::scaleImage(double xscale, double yscale) {
    XToX *= xscale;
    YToX *= yscale;
    XToY *= xscale;
    YToY *= yscale;

    return *this;
}

GeoTransform& GeoTransform::rotateProjection(double angle) {
    double cos = std::cos(angle * M_PI / 180);
    double sin = std::sin(angle * M_PI / 180);

    auto rotate = [cos,sin] (double xin, double yin) {
        return std::array<double,2>{
            cos * xin - sin * yin, // xout
            sin * xin + cos * yin  // yout
        };
    };

    constexpr unsigned int X = 0;
    constexpr unsigned int Y = 1;
    std::array<double,2> rotated;

    // rotate origin
    rotated = rotate(offsetX, offsetY);
    offsetX = rotated[X];
    offsetY = rotated[Y];

    // rotate first column of A
    rotated = rotate(XToX, XToY);
    XToX = rotated[X];
    XToY = rotated[Y];

    // rotate second column of A
    rotated = rotate(YToX, YToY);
    YToX = rotated[X];
    YToY = rotated[Y];

    return *this;
}

GeoTransform& GeoTransform::rotateImage(double angle) {
    double cos = std::cos(angle * M_PI / 180);
    double sin = std::sin(angle * M_PI / 180);

    // get first column of A
    double temp_XToX =  XToX * cos + YToX * sin;
    double temp_XToY =  XToY * cos + YToY * sin;

    // get second column of A
    double temp_YToX = -XToX * sin + YToX * cos;
    double temp_YToY = -XToY * sin + YToY * cos;

    XToX = temp_XToX;
    XToY = temp_XToY;
    YToX = temp_YToX;
    YToY = temp_YToY;

    return *this;
}

GeoTransform& GeoTransform::flipImage(bool flipH, bool flipV, Size s) {
    translateImage(flipH ? s.width  : 0,
                   flipV ? s.height : 0);
    scaleImage(flipH ? -1 : 1,
               flipV ? -1 : 1);
    return *this;
}

GeoTransform& GeoTransform::shearXProjection(double factor) {
    offsetX += offsetY * factor;
    XToX    += XToY    * factor;
    YToX    += YToY    * factor;

    return *this;
}

GeoTransform& GeoTransform::shearXImage(double factor) {
    YToX += XToX * factor;
    YToY += XToY * factor;

    return *this;
}

GeoTransform& GeoTransform::shearYProjection(double factor) {
    offsetY += offsetX * factor;
    XToY    += XToX    * factor;
    YToY    += YToX    * factor;

    return *this;
}

GeoTransform& GeoTransform::shearYImage(double factor) {
    XToX += YToX * factor;
    XToY += YToY * factor;

    return *this;
}

GeoTransform& GeoTransform::translateProjection(double xoffset, double yoffset) {
    offsetX += xoffset;
    offsetY += yoffset;

    return *this;
}

GeoTransform& GeoTransform::translateImage(double xoffset, double yoffset) {
    offsetX += XToX * xoffset + YToX * yoffset;
    offsetY += XToY * xoffset + YToY * yoffset;

    return *this;
}


void GeoTransform::set(double topLeft_x, double topLeft_y, double x_to_x, double y_to_x, double x_to_y, double y_to_y) {
    offsetX = topLeft_x;
    offsetY = topLeft_y;
    XToX    = x_to_x;
    YToY    = y_to_y;
    YToX    = y_to_x;
    XToY    = x_to_y;
}


bool GeoTransform::isIdentity() const {
    return offsetX == 0
        && offsetY == 0
        && XToX    == 1
        && YToX    == 0
        && XToY    == 0
        && YToY    == 1;
}

bool GeoInfo::hasGeotransform() const {
    return !geotrans.isIdentity() && const_cast<OGRSpatialReference&>(geotransSRS).Validate() == OGRERR_NONE;
}


bool GeoInfo::hasGCPs() const {
    return gcps.size() >= 3 && const_cast<OGRSpatialReference&>(gcpSRS).Validate() == OGRERR_NONE;
}


std::map<std::string, std::string> const& GeoInfo::getMetadataItems(std::string const& domain) const {
    auto dom_it = metadata.find(domain);
    if (dom_it != metadata.end())
        return dom_it->second;

    IF_THROW_EXCEPTION(not_found_error("Could not find the requested metadata domain " + domain + "!"));
}

double GeoInfo::getNodataValue(unsigned int channel) const {
    return nodataValues.at(std::min(static_cast<std::size_t>(channel), nodataValues.size() - 1));
}




} /* namespace imagefusion */

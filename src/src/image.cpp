#include <algorithm>
#include <numeric>
#include <type_traits>
#include <sstream>

#include <filesystem>

#include <gdal.h>
#include <gdal_priv.h>
#include <gdalwarper.h>
#include <gdal_utils.h>
#include <cpl_string.h>

#include "image.h"
#include "geoinfo.h"

namespace {

template <typename Imgtype>
imagefusion::Image merge_impl(std::vector<Imgtype> const& images) {
    std::vector<cv::Mat> cvImages;
    cvImages.reserve(images.size());
    for (auto const& i : images)
        cvImages.push_back(i.cvMat()); // OpenCV shared copy

//    unsigned int totalChannels = std::accumulate(images.begin(), images.end(), 0, [] (Imgtype const& i, unsigned int count) { return count + i.channels(); });
    imagefusion::Image multichannel;
    cv::merge(cvImages, multichannel.cvMat());
    return multichannel;
}


std::string getGDALMemOptionString(imagefusion::ConstImage const& i) {
    char szPtrValue[128] = { '\0' };
    int nRet = CPLPrintPointer(szPtrValue,
                               reinterpret_cast<void*>(const_cast<uchar*>(i.cvMat().ptr())),
                               sizeof(szPtrValue));
    szPtrValue[nRet] = 0;

    std::ostringstream ss;
    ss << "MEM:::DATAPOINTER=" << szPtrValue
       << ",PIXELS="           << i.width()
       << ",LINES="            << i.height()
       << ",BANDS="            << i.channels()
       << ",DATATYPE="         << GDALGetDataTypeName(toGDALDepth(i.type()))
       << ",PIXELOFFSET="      << i.cvMat().elemSize()
       << ",LINEOFFSET="       << i.cvMat().ptr(1) - i.cvMat().ptr(0)
       << ",BANDOFFSET="       << i.cvMat().elemSize1();
    return ss.str();
}

} /* anonymous namespace */

namespace imagefusion {

// Implementation

GDALDataset const* ConstImage::asGDALDataset() const {
    // This reads the OpenCV Mat as GDAL Dataset. See ConstImage::write for more info!
    std::string optStr = getGDALMemOptionString(*this);
    return (GDALDataset*) GDALOpen(optStr.c_str(), GA_ReadOnly);
}


GDALDataset* Image::asGDALDataset() {
    // This reads the OpenCV Mat as GDAL Dataset. See ConstImage::write for more info!
    std::string optStr = getGDALMemOptionString(*this);
    return (GDALDataset*) GDALOpen(optStr.c_str(), GA_Update);
}


void ConstImage::write(std::string const& filename, GeoInfo const& gi, FileFormat format) const {
    GDALAllRegister();
    if (format == FileFormat::unsupported) {
        std::filesystem::path p = filename;
        std::string ext = p.extension().string();
        format = FileFormat::fromFileExtension(ext);
        if (format == FileFormat::unsupported) {
            IF_THROW_EXCEPTION(file_format_error("Cannot auto detect image format for file extension " + ext +
                                                 ". Please specify image format explicitly or read it from an input image file!"))
                    << boost::errinfo_file_name(filename);
        }
    }

    try {
        if (format == FileFormat("GTiff"))
            write(filename, to_string(format), {std::make_pair<std::string,std::string>("COMPRESS", "LZW")}, gi);
        else
            write(filename, to_string(format), {}, gi);
    }
    catch(boost::exception& ex) {
        ex << errinfo_file_format(to_string(format));
        throw;
    }
}


void ConstImage::write(std::string const& filename, std::string const& driverName, std::vector<std::pair<std::string,std::string>> const& options_vec, GeoInfo const& gi) const {
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.c_str());
    if (!driver) {
        IF_THROW_EXCEPTION(file_format_error("GDAL driver for image file format " + driverName + " not available! "
                                             "Maybe you have to recompile GDAL with more dependencies."))
                << boost::errinfo_file_name(filename);
    }

    // To write as png or jpg, we need to use CreateCopy method, from a memory ("MEM") Dataset:
    // https://gis.stackexchange.com/questions/134000/gdal-api-cant-save-image-in-some-formats
    // To avoid making an additional copy, we use the memory of the OpenCV Mat for the GDAL Dataset.
    // For that we need to convert some options (like pointer to image memory) to a string and use it as filename:
    // https://gis.stackexchange.com/questions/196048/how-to-reuse-memory-pointer-of-gdal-memory-driver
    // https://lists.osgeo.org/pipermail/gdal-dev/2016-June/044556.html
    GDALDataset* poSrcDS = const_cast<GDALDataset*>(asGDALDataset());

    // add GeoInfo first time (important for color table)
    gi.addTo(poSrcDS);

    // write to file
    char **options = nullptr;
    for (auto const& p : options_vec)
        options = CSLSetNameValue(options, p.first.c_str(), p.second.c_str());
    GDALDataset* poDstDS = driver->CreateCopy(filename.c_str(), poSrcDS, FALSE, options, nullptr, nullptr);

    // add GeoInfo second time (important for custom metadata domains)
    gi.addTo(poDstDS);
    bool shouldHaveWrittenColorTable = !gi.colorTable.empty() && type() == Type::uint8x1;

    GDALClose(poSrcDS);
    if (options)
        CSLDestroy(options);
    if (!poDstDS) {
        IF_THROW_EXCEPTION(runtime_error("Could not create the output file " + filename + " with driver " + driverName +
                                         " (" + FileFormat(driverName).longName() + ") and type " + to_string(type()) + "."))
                << errinfo_file_format(driverName)
                << errinfo_image_type(type())
                << boost::errinfo_file_name(filename);
    }
    GDALClose(poDstDS);

    // check for changed color table and warn
    if (shouldHaveWrittenColorTable) {
        GeoInfo test{filename};
        gi.compareColorTables(test, false);
    }
}


void Image::read(std::string const& filename,
                 std::vector<int> channels /* {} means all channels */,
                 Rectangle r /* {0, 0, 0, 0} means whole image */,
                 bool flipH, bool flipV, bool ignoreColorTable, InterpMethod interp)
{
    GDALAllRegister();

    // open file
    FileFormat format = FileFormat::fromFile(filename);
    if (format == FileFormat::unsupported)
        IF_THROW_EXCEPTION(runtime_error("Could not open image '" + filename + "' with GDAL to read the image. "
                                         "Either the file does not exist or GDAL could not find an appropriate driver to read the image."))
                << boost::errinfo_file_name(filename);

    // open dataset
    GeoInfo gi{filename};
    unsigned int rastercount = gi.channels;
    GDALDataset* gdal_img = nullptr;
    if (gi.hasSubdatasets()) {
        gdal_img = gi.openVrtGdalDataset(channels, interp);
        rastercount = gdal_img->GetRasterCount();

        GeoInfo gi_sub;
        gi_sub.readFrom(gdal_img);
        if (gi_sub.baseType == Type::invalid) {
            std::string types;
            for (unsigned int i = 1; i <= rastercount; ++i) {
                GDALDataType gdal_type = gdal_img->GetRasterBand(i)->GetRasterDataType();
                types += to_string(toBaseType(gdal_type)) + ", ";
            }
            types.pop_back(); types.pop_back();
            GDALClose(gdal_img);
            IF_THROW_EXCEPTION(image_type_error("The selected subdatasets have different data types: " + types + ". Cannot import image with different types."));
        }

        channels.clear(); // to get all channels in virtual dataset
    }
    else {
        // check acquired channels
        if (!channels.empty()) {
            for (int& c : channels) { // in GDAL channels are called bands and are 1 based
                if (c >= static_cast<int>(rastercount) || c < 0)
                    IF_THROW_EXCEPTION(image_type_error("You acquired a channel (" + std::to_string(c)
                                                        + ") that does not exist. The image only has "
                                                        + std::to_string(rastercount) + " channels."))
                            << boost::errinfo_file_name(filename);
                ++c;
            }
            rastercount = channels.size();
        }

        gdal_img = (GDALDataset*) GDALOpen(filename.c_str(), GA_ReadOnly);
    }

    if (!gdal_img)
        IF_THROW_EXCEPTION(file_format_error("Could not open image " + filename + " for unknown reasons. This might be a bug."))
                << boost::errinfo_file_name(filename);

    // check acquired region
    int cols = gdal_img->GetRasterXSize();
    int rows = gdal_img->GetRasterYSize();

    if (r.width == 0)
        r.width = cols - r.x;

    if (r.height == 0)
        r.height = rows - r.y;

    if (r.x + r.width > cols || r.y + r.height > rows                     // check bounds
            || r.x < 0 || r.y < 0 || r.width < 0 || r.height < 0)         // check negative values
    {
        IF_THROW_EXCEPTION(size_error("The region you acquired " + to_string(r) + " has negative size or "
                                      "goes out of bounds of the image with size ("
                                      + std::to_string(cols) + " x " + std::to_string(rows) + ")."))
                << boost::errinfo_file_name(filename);
    }

    // create image memory
    GDALDataType gdal_type = gdal_img->GetRasterBand(1)->GetRasterDataType();
    int cv_type = CV_MAKETYPE(toCVType(toBaseType(gdal_type)), rastercount);
    img.create(r.height, r.width, cv_type);

    // read
    uint8_t* pData = img.ptr();
    if (flipH)
        pData += (r.width - 1) * img.elemSize();
    if (flipV)
        pData += (r.height - 1) * (img.ptr(1) - img.ptr(0));

    auto ret = gdal_img->RasterIO(
                GF_Read,
                r.x, /* nXOff */
                r.y, /* nYOff */
                r.width, /* nXSize */
                r.height, /* nYSize */
                pData,  /* pData */
                r.width, /* nBufXSize */
                r.height,  /* nBufYSize */
                gdal_type, /* eBufType */
                rastercount, /* nBandCount */
                channels.empty() ? nullptr : channels.data(), /* panBandMap, nullptr means all bands */
                img.elemSize() * (flipH ? -1 : 1),  /* nPixelSpace, how many bytes to the next value of the same channel*/
                (img.ptr(1) - img.ptr(0)) * (flipV ? -1 : 1),  /* nLineSpace */
                img.elemSize1()  /* nBandSpace */
//                NULL  /* psExtraArg */
    );
    if (ret == CE_Failure) {
        GDALClose(gdal_img);
        IF_THROW_EXCEPTION(runtime_error("Could not read from image " + filename +
                                         " of type " + to_string(type()) + "."))
                << errinfo_image_type(type())
                << boost::errinfo_file_name(filename);
    }


    // check for color table (indexed colors) and replace image with converted colors image
    GDALColorTable* ct = gdal_img->GetRasterBand(1)->GetColorTable();
    if (ct && !ignoreColorTable) {
        if (ct->GetPaletteInterpretation() == GPI_CMYK || ct->GetPaletteInterpretation() == GPI_HLS || (ct->GetPaletteInterpretation() == GPI_RGB && rastercount > 1))
            IF_THROW_EXCEPTION(file_format_error("Color indexed images with RGB interpretation are only supported for single-index images. CMYK and HLS are not supported at all."))
                    << boost::errinfo_file_name(filename);
        if (gdal_type != GDT_Byte)
            IF_THROW_EXCEPTION(file_format_error("This is a color indexed image, which is not of type uint8 (or GDT_Byte). I did not know this is possible. Please help me to fix this!"))
                    << boost::errinfo_file_name(filename);

        bool isGray = true;
        bool hasAlpha = false;
        int ctsize = ct->GetColorEntryCount();
        if (ct->GetPaletteInterpretation() == GPI_RGB) {
            // test only the occuring colors to check for the number of required target channels
            for (int y = 0; y < r.height && (isGray || !hasAlpha); ++y) {
                for (int x = 0; x < r.width && (isGray || !hasAlpha); ++x) {
                    int i = at<uint8_t>(x, y, 0);
                    if (i >= ctsize)
                        continue;
                    GDALColorEntry const* ce = ct->GetColorEntry(i);
                    if (ce->c1 > 255 || ce->c2 > 255 || ce->c3 > 255 || ce->c4 > 255)
                        IF_THROW_EXCEPTION(file_format_error("The color index has values that do not fit into 8 bit. I did not know this is possible. Please help me to fix this!"))
                                << boost::errinfo_file_name(filename);
                    if (ce->c1 != ce->c2 || ce->c2 != ce->c3)
                        isGray = false;
                    if (ce->c4 != 255)
                        hasAlpha = true;
                }
            }
        }
        int targetChannels = isGray ? 1 : 3;
        if (hasAlpha)
            ++targetChannels;

        if (ct->GetPaletteInterpretation() == GPI_RGB) {
            std::cout << "Note, interpreted the RGBA color table of " << filename << " as "
                      << (isGray ? "Gray scale (since all used entries are gray) " : "RGB (since there are non-gray colors) ")
                      << (hasAlpha ? "with Alpha channel (since the alpha values vary)." : "without Alpha channel (since the alpha values are all 255).")
                      << " This results in " << targetChannels << " channel(s) in the converted image." << std::endl;
        }

        cv_type = CV_MAKETYPE(toCVType(Type::uint8), targetChannels);

        cv::Mat indexed = img;
//        std::cout << "indexed:\n" << img << std::endl;
        img.create(r.height, r.width, cv_type);
        GDALColorEntry black{0, 0, 0, 255};
        for (int y = 0; y < r.height; ++y) {
            for (int x = 0; x < r.width; ++x) {
                int i = indexed.at<uint8_t>(/*cv order*/ y, x);
                GDALColorEntry const* ce;
                if (i >= ctsize) {
                    std::cout << "WARNING: Cannot convert the indexed values at (" << x << ", " << y << "), because it is too large: "
                              << i << " >= " << ctsize << " = table size. (Maybe you read out of bounds?) Using black instead." << std::endl;
                    ce = &black;
                }
                else
                    ce = ct->GetColorEntry(i);

                switch(targetChannels) {
                case 4:
                    at<uint8_t>(x, y, 3) = cv::saturate_cast<uint8_t>(ce->c4);
                case 3:
                    at<uint8_t>(x, y, 2) = cv::saturate_cast<uint8_t>(ce->c3);
                    at<uint8_t>(x, y, 1) = cv::saturate_cast<uint8_t>(ce->c2);
                    at<uint8_t>(x, y, 0) = cv::saturate_cast<uint8_t>(ce->c1);
                    break;
                case 2:
                    at<uint8_t>(x, y, 1) = cv::saturate_cast<uint8_t>(ce->c4); // gray-alpha
                case 1:
                    at<uint8_t>(x, y, 0) = cv::saturate_cast<uint8_t>(ce->c1);
                }
            }
        }
//        std::cout << "converted:\n" << img << std::endl;
    }

    GDALClose(gdal_img);
}


Rectangle ConstImage::getCropWindow() const {
    cv::Size wholeSize;
    cv::Point offset;
    img.locateROI(wholeSize, offset);
    return Rectangle{offset.x, offset.y, width(), height()};
}


Size ConstImage::getOriginalSize() const {
    cv::Size wholeSize;
    cv::Point offset;
    img.locateROI(wholeSize, offset);
    return Size{wholeSize.width, wholeSize.height};
}


void ConstImage::crop(Rectangle r) {
    ConstImage temp = sharedCopy(r);
    img = std::move(temp.img);
}


void ConstImage::uncrop() {
    Size s = getOriginalSize();
    adjustCropBorders(s.height, s.height, s.width, s.width);
}


void ConstImage::adjustCropBorders(int extendTop, int extendBottom, int extendLeft, int extendRight) {
    // check for resulting zero size
    cv::Size wholeSize;
    cv::Point offset;
    img.locateROI(wholeSize, offset);
    int newLeft   = offset.x - extendLeft;
    int newRight  = offset.x + width() + extendRight;
    int newTop    = offset.y - extendTop;
    int newBottom = offset.y + height() + extendBottom;
    newLeft   = std::max(newLeft, 0);
    newRight  = std::min(newRight, wholeSize.width);
    newTop    = std::max(newTop, 0);
    newBottom = std::min(newBottom, wholeSize.height);
    const int newWidth  = newRight  - newLeft;
    const int newHeight = newBottom - newTop;
    if (newWidth <= 0 || newHeight <= 0)
        IF_THROW_EXCEPTION(size_error("Adjusting crop borders would result in zero size (here, width: " + std::to_string(newWidth)
                                  + ", height: " + std::to_string(newHeight) + "), which is not supported."));

    // non-zero size, do it!
    img.adjustROI(extendTop, extendBottom, extendLeft, extendRight);
}


void Image::copyValuesFrom(ConstImage const& src, ConstImage const& mask) {
    assert(src.width()    == width());
    assert(src.height()   == height());
    assert(src.channels() == channels());
    assert(src.type()     == type());

    if (mask.empty())
        src.img.copyTo(img);
    else
        src.img.copyTo(img, mask.img);
}


Image ConstImage::clone() const {
    if (empty())
        return {};

    cv::Size wholeSize;
    cv::Point offset;
    img.locateROI(wholeSize, offset);

    Image ret;
    if (wholeSize.height == height() && wholeSize.width == width()) {
        // not cropped, just clone
        ret.img = img.clone();
        return ret;
    }

    // this is cropped, so uncrop it first and then clone and recrop
    Image temp;
    temp.img = img;
    temp.uncrop();
    ret.img = temp.img.clone();
    ret.crop(Rectangle{offset.x, offset.y, width(), height()});
    return ret;
}


Image ConstImage::clone(Rectangle const& r) const {
    ConstImage temp = sharedCopy(r);

    Image ret;
    ret.img = temp.img.clone();
    return ret;
}

namespace {
struct BilinearFilterHelper {
    ConstImage const& img_tl;
    ConstImage const& img_tr;
    ConstImage const& img_bl;
    ConstImage const& img_br;
    double dx;
    double dy;
    Size s;
    Type fullType;

    template<Type baseType>
    Image operator()() const {
        using type = typename DataType<baseType>::base_type;
        int chans = getChannels(fullType);
        Image cropped{s, fullType};
        double rounding = isIntegerType(baseType) ? 0.5 : 0;

        double weight_tl = (1 - dx) * (1 - dy);
        double weight_tr =      dx  * (1 - dy);
        double weight_bl = (1 - dx) *      dy ;
        double weight_br =      dx  *      dy ;

        if (dx == 0) {
            // only interpolate y
            for (int y = 0; y < s.height; ++y) {
                for (int x = 0; x < s.width; ++x) {
                    for (int c = 0; c < chans; ++c) {
                        double tl = img_tl.at<type>(x, y, c) * weight_tl;
                        double bl = img_bl.at<type>(x, y, c) * weight_bl;
                        cropped.at<type>(x, y, c) = static_cast<type>(tl + bl + rounding);
                    }
                }
            }
        }
        else if (dy == 0) {
            // only interpolate x
            for (int y = 0; y < s.height; ++y) {
                for (int x = 0; x < s.width; ++x) {
                    for (int c = 0; c < chans; ++c) {
                        double tl = img_tl.at<type>(x, y, c) * weight_tl;
                        double tr = img_tr.at<type>(x, y, c) * weight_tr;
                        cropped.at<type>(x, y, c) = static_cast<type>(tl + tr + rounding);
                    }
                }
            }
        }
        else {
            // interpolate x and y
            for (int y = 0; y < s.height; ++y) {
                for (int x = 0; x < s.width; ++x) {
                    for (int c = 0; c < chans; ++c) {
                        double tl = img_tl.at<type>(x, y, c) * weight_tl;
                        double tr = img_tr.at<type>(x, y, c) * weight_tr;
                        double bl = img_bl.at<type>(x, y, c) * weight_bl;
                        double br = img_br.at<type>(x, y, c) * weight_br;
                        cropped.at<type>(x, y, c) = static_cast<type>(tl + tr + bl + br + rounding);
                    }
                }
            }
        }
        return cropped;
    }
};
}

Image ConstImage::clone(Coordinate const& topleft, Size s) const {
    double dx = topleft.x - std::floor(topleft.x);
    double dy = topleft.y - std::floor(topleft.y);

    if (dx == 0 && dy == 0)
        return clone(Rectangle{static_cast<int>(topleft.x), static_cast<int>(topleft.y), s.width, s.height});

    int x0 = static_cast<int>(std::floor(topleft.x));
    int x1 = static_cast<int>(std::ceil(topleft.x));
    int y0 = static_cast<int>(std::floor(topleft.y));
    int y1 = static_cast<int>(std::ceil(topleft.y));

    ConstImage img_tl = sharedCopy(Rectangle{x0, y0, s.width, s.height});
    ConstImage img_tr = sharedCopy(Rectangle{x1, y0, s.width, s.height});
    ConstImage img_bl = sharedCopy(Rectangle{x0, y1, s.width, s.height});
    ConstImage img_br = sharedCopy(Rectangle{x1, y1, s.width, s.height});

    return CallBaseTypeFunctor::run(BilinearFilterHelper{img_tl, img_tr, img_bl, img_br, dx, dy, s, type()}, basetype());

    // this code uses much more memory than the above functor, since it generates a temporary double image and image-wide operations. It is only slightly faster.
//    cv::Mat temp;
//    cv::addWeighted(img_tl.cvMat(), weight_tl, img_tr.cvMat(), weight_tr, 0, temp, CV_64F);
//    cv::addWeighted(temp, 1, img_bl.cvMat(), weight_bl, 0, temp, CV_64F);
//    cv::addWeighted(temp, 1, img_br.cvMat(), weight_br, 0, temp, CV_64F);

//    Image cropped;
//    temp.convertTo(cropped.cvMat(), toCVType(basetype())); // converting from double to integer type will also round the result
//    return cropped;
}


Image ConstImage::warp(GeoInfo const& from, GeoInfo const& to, InterpMethod method) const {
    if (!from.hasGeotransform() || !to.hasGeotransform())
        IF_THROW_EXCEPTION(invalid_argument_error("For warping you need to specify from which projection system to which should be warped. "
                                                  "One of your arguments does not specify any geotransformation (GCPs are currently ignored)."));

    GDALDataset* poSrcDS = const_cast<GDALDataset*>(asGDALDataset());
    from.addTo(poSrcDS);

    Size sz = to.size;
    if (sz.width <= 0 || sz.height <= 0) {
        // estimate output size from source

        char* projStr;
        to.geotransSRS.exportToWkt(&projStr);
        if (std::strlen(projStr) <= 0)
            IF_THROW_EXCEPTION(logic_error("no reference system"));

        GDALTransformerFunc pfnTransformer = GDALGenImgProjTransform;
        void* pTransformArg = GDALCreateGenImgProjTransformer(poSrcDS,
                                                              poSrcDS->GetProjectionRef(),
                                                              nullptr,
                                                              projStr,
                                                              FALSE, 0.0, 1);

        double geoTransformOut[6];
        int widthOut;
        int heightOut;
        double extentsOut[4]; // xmin, ymin, xmax, ymax in projection space
#ifdef DEBUG
        CPLErr eErr =
#endif
                GDALSuggestedWarpOutput2(poSrcDS,
                                         pfnTransformer,
                                         pTransformArg,
                                         geoTransformOut,
                                         &widthOut,
                                         &heightOut,
                                         extentsOut,
                                         0);
        GDALDestroyGenImgProjTransformer(pTransformArg);
        CPLFree(projStr);
        CPLAssert(eErr == CE_None);

//        std::cout << "suggested... width: " << widthOut << ", height: " << heightOut
//                  << ", extents: (" << extentsOut[0] << ", " << extentsOut[1] << "), (" << extentsOut[2] << ", " << extentsOut[3] << ")" << std::endl;

        Coordinate c1 = to.geotrans.projToImg({extentsOut[0], extentsOut[1]});
        Coordinate c2 = to.geotrans.projToImg({extentsOut[2], extentsOut[3]});
        sz.width  = static_cast<int>(std::ceil(std::max(c1.x, c2.x)));
        sz.height = static_cast<int>(std::ceil(std::max(c1.y, c2.y)));
    }

    Image warped{sz, type()};
    GDALDataset* poDstDS = warped.asGDALDataset();
    to.addTo(poDstDS);

    // Setup warp options.
    GDALWarpOptions *psWarpOptions = GDALCreateWarpOptions();
    if (method == InterpMethod::nearest)
        psWarpOptions->eResampleAlg = GRA_NearestNeighbour; // for data images
    else if (method == InterpMethod::bilinear)
        psWarpOptions->eResampleAlg = GRA_Bilinear;
    else if (method == InterpMethod::cubic)
        psWarpOptions->eResampleAlg = GRA_Cubic;
    else // method == InterpMethod::cubicspline
        psWarpOptions->eResampleAlg = GRA_CubicSpline;

    psWarpOptions->hSrcDS = poSrcDS;
    psWarpOptions->hDstDS = poDstDS;
    psWarpOptions->nBandCount = channels(); // required for nodata values
    psWarpOptions->panSrcBands = (int*) CPLMalloc(sizeof(int) * psWarpOptions->nBandCount);
    psWarpOptions->panDstBands = (int*) CPLMalloc(sizeof(int) * psWarpOptions->nBandCount);
    for (unsigned int i = 0; i < channels(); ++i) {
        psWarpOptions->panSrcBands[i] = i + 1;
        psWarpOptions->panDstBands[i] = i + 1;
    }

    // nodata values
    if (from.hasNodataValue()) {
        psWarpOptions->padfSrcNoDataReal = (double*) CPLMalloc(channels() * sizeof(double));
        psWarpOptions->padfSrcNoDataImag = (double*) CPLMalloc(channels() * sizeof(double));
        psWarpOptions->padfDstNoDataReal = (double*) CPLMalloc(channels() * sizeof(double));
        psWarpOptions->padfDstNoDataImag = (double*) CPLMalloc(channels() * sizeof(double));
        for (unsigned int i = 0; i < channels(); ++i) {
            int hasNodataVal;
            double nodataVal = poSrcDS->GetRasterBand(i+1)->GetNoDataValue(&hasNodataVal);
            psWarpOptions->padfSrcNoDataReal[i] = nodataVal;
            psWarpOptions->padfSrcNoDataImag[i] = 0;

            if (to.hasNodataValue())
                nodataVal = poDstDS->GetRasterBand(i+1)->GetNoDataValue(&hasNodataVal);
            psWarpOptions->padfDstNoDataReal[i] = nodataVal;
            psWarpOptions->padfDstNoDataImag[i] = 0;
        }
    }

    psWarpOptions->papszWarpOptions = nullptr;
    if (channels() == 1)
        psWarpOptions->papszWarpOptions = CSLSetNameValue(psWarpOptions->papszWarpOptions, "UNIFIED_SRC_NODATA", "YES" ); // otherwise no data points will be replaced by the nearest valid neighbor

    psWarpOptions->papszWarpOptions = CSLSetNameValue(psWarpOptions->papszWarpOptions, "INIT_DEST",    "NO_DATA");
    psWarpOptions->papszWarpOptions = CSLSetNameValue(psWarpOptions->papszWarpOptions, "SAMPLE_STEPS", "32"); // increase precision to some 2^x number

    // workaround for a bug in GDAL, which sets the scale factor for an identity warp to 0.5 instead of 1 and then uses anti-aliasing
    if (from.geotransSRS.IsSame(&to.geotransSRS) &&
        from.geotrans.XToX == to.geotrans.XToX &&
        from.geotrans.YToY == to.geotrans.YToY &&
        from.geotrans.XToY == to.geotrans.XToY &&
        from.geotrans.YToX == to.geotrans.YToX)
    {
        psWarpOptions->papszWarpOptions = CSLSetNameValue(psWarpOptions->papszWarpOptions, "XSCALE", "1");
        psWarpOptions->papszWarpOptions = CSLSetNameValue(psWarpOptions->papszWarpOptions, "YSCALE", "1");
    }


    // Establish reprojection transformer.
    char** papszTO = nullptr;
    papszTO = CSLSetNameValue(papszTO, "STRIP_VERT_CS", "YES");
    psWarpOptions->pTransformerArg = GDALCreateGenImgProjTransformer2(poSrcDS, poDstDS, papszTO);
    psWarpOptions->pfnTransformer = GDALGenImgProjTransform;

    // Initialize and execute the warp operation.
    GDALWarpOperation oOperation;
    oOperation.Initialize(psWarpOptions);
    oOperation.ChunkAndWarpImage(0, 0, sz.width, sz.height);
    GDALClose(poDstDS);
    GDALClose(poSrcDS);

    // workaround for a bug in GDAL, which uses the nearest valid neighbor, even if the nearest neighbor is invalid. This happens only if not using nearest neighbor interpolation and UNIFIED_SRC_NODATA is set to NO, i. e. for multi-channel images
    if (channels() > 1 && method != InterpMethod::nearest && from.hasNodataValue()) {
        // make mask from source nodata values
        std::vector<Interval> nodataValIntervals;
        std::vector<double> nodataVals;
        for (unsigned int i = 0; i < channels(); ++i) {
            double d = from.getNodataValue(i);
            nodataVals.push_back(d);
            nodataValIntervals.push_back(Interval::closed(d, d));
        }
        Image mask = createMultiChannelMaskFromRange(nodataValIntervals);

        // warp source nodata mask with nearest neighbor interpolation
        Image warpedMask{sz, mask.type()};
        GDALDataset* poSrcMaskDS = const_cast<GDALDataset*>(mask.asGDALDataset());
        GDALDataset* poDstMaskDS = warpedMask.asGDALDataset();
        from.addTo(poSrcMaskDS);
        to.addTo(poDstMaskDS);

        psWarpOptions->eResampleAlg = GRA_NearestNeighbour;
        psWarpOptions->hSrcDS = poSrcMaskDS;
        psWarpOptions->hDstDS = poDstMaskDS;
        for (unsigned int i = 0; i < channels(); ++i) {
            psWarpOptions->padfSrcNoDataReal[i] = std::numeric_limits<double>::quiet_NaN();;
            psWarpOptions->padfDstNoDataReal[i] = std::numeric_limits<double>::quiet_NaN();;
        }
        oOperation.Initialize(psWarpOptions);
        oOperation.ChunkAndWarpImage(0, 0, sz.width, sz.height);

        GDALClose(poDstMaskDS);
        GDALClose(poSrcMaskDS);

        // set dst nodata values
        if (to.hasNodataValue())
            for (unsigned int i = 0; i < channels(); ++i)
                nodataVals.at(i) = to.getNodataValue(i);
        warped.set(nodataVals, warpedMask);
    }

    GDALDestroyGenImgProjTransformer(psWarpOptions->pTransformerArg);
    GDALDestroyWarpOptions(psWarpOptions);

    return warped;
}

std::vector<Image> ConstImage::split(std::vector<unsigned int> chans) const {
    std::vector<cv::Mat> cvImages;
    if (chans.empty()) {
        // simply split all channels
        cvImages = std::vector<cv::Mat>(channels());
        cv::split(img, cvImages);
    }
    else {
        // from channel - to channel relation
        std::vector<int> fromTo;
        for (unsigned int i = 0; i < chans.size(); ++i) {
            unsigned int ch = chans[i];
            if (ch >= channels())
                IF_THROW_EXCEPTION(image_type_error("You requested to split an image and to use channel "
                                                    + std::to_string(ch) + ", but the image has got only "
                                                    + std::to_string(channels()))) << errinfo_image_type(type());
            fromTo.push_back(ch);
            fromTo.push_back(i);
        }

        // prepare single channel output images
        cvImages = std::vector<cv::Mat>(chans.size());
        for (cv::Mat& m : cvImages)
            m.create(size(), toCVType(basetype()));

        // split
        std::vector<cv::Mat> src{cvMat()};
        cv::mixChannels(src, cvImages, fromTo.data(), chans.size());
    }

    // convert to Image type
    std::vector<Image> channelImages(cvImages.size());
    for (unsigned int i = 0; i < channelImages.size(); ++i)
        channelImages[i].img = std::move(cvImages[i]);

    return channelImages;
}


Image merge(std::vector<Image> const& images) {
    return merge_impl(images);
}

Image merge(std::vector<ConstImage> const& images) {
    return merge_impl(images);
}

namespace {
template<typename OP>
Image SimpleBroadcastOperation(ConstImage const& A, Image B, OP const& op) {
    cv::Mat a = A.cvMat();
    if (a.channels() == 1 && B.channels() > 1) {
        std::vector<cv::Mat> aaa(B.channels(), a);
        cv::merge(aaa, a); // note: broadcasting could be done without temporary memory allocation for better performance
    }
    else if (B.channels() == 1 && a.channels() > 1) {
        std::vector<cv::Mat> bbb(a.channels(), B.cvMat());
        cv::merge(bbb, B.cvMat());
    }

    op(a, B.cvMat(), B.cvMat());
    return B;
}
} /* anonymous namespace */


Image ConstImage::absdiff(Image&& B) const& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::absdiff(mat1, mat2, res);});
}

Image ConstImage::absdiff(ConstImage const& B) const& {
    if (B.channels() > channels()) // for performance only in case of broadcasting
        return absdiff(B.clone()); // B has multiple channels and can be used to save the result
    return B.absdiff(clone());
}

Image Image::absdiff(ConstImage const& B)&& {
    return B.absdiff(std::move(*this));
}


Image ConstImage::abs() const& {
    if (isUnsignedType(type()))
        return clone();

    if (channels() <= 4) {
        Image ret;
        ret.img = cv::abs(img);
        return ret;
    }

    Image ret = clone();
    auto op = [] (auto& v, int, int, int) {
        using type = std::remove_reference_t<decltype(v)>;
        v = cv::saturate_cast<type>(std::abs(v));
    };
    CallBaseTypeFunctorRestrictBaseTypesTo<Type::int8, Type::int16, Type::int32, Type::float32, Type::float64>::run(InplacePointOperationFunctor{ret, {}, op}, ret.basetype());
    return ret;
}


Image Image::abs()&& {
    if (isUnsignedType(type()))
        return std::move(*this);

    if (channels() <= 4) {
        img = cv::abs(img);
    }
    else {
        auto op = [] (auto& v, int, int, int) {
            using type = std::remove_reference_t<decltype(v)>;
            v = cv::saturate_cast<type>(std::abs(v));
        };
        CallBaseTypeFunctorRestrictBaseTypesTo<Type::int8, Type::int16, Type::int32, Type::float32, Type::float64>::run(InplacePointOperationFunctor{*this, {}, op}, basetype());
    }

    return std::move(*this);
}

namespace {

void checkMask(ConstImage const& mask, unsigned int imgChans) {
    if (mask.channels() > 1 && mask.channels() != imgChans)
        IF_THROW_EXCEPTION(image_type_error("Mask has a wrong number of channels. It should be single-channel"
                                            + (imgChans > 1 ? " or multi-channel with " + std::to_string(imgChans) + " channels" : "")
                                            + ". However, it has " + std::to_string(mask.channels()) + " channels."))
                << errinfo_image_type(mask.type());

    if (mask.basetype() != Type::uint8)
        IF_THROW_EXCEPTION(image_type_error("Mask has an invalid type. It should have basetype Type::uint8, "
                                            "but has " + to_string(mask.basetype()) + "."))
                << errinfo_image_type(mask.basetype());
}

template<typename CvOp, typename StdOp>
Image minimum_maximum(Image A, ConstImage const& B, ConstImage const& mask, CvOp const& cvminmax, StdOp const& stdminmax) {
    if (B.empty())
        return A;
    if (A.empty())
        return B.clone();

    // no mask --> use OpenCV with broadcasting
    if (mask.empty())
        return SimpleBroadcastOperation(B, std::move(A), cvminmax);

    // broadcasting minimum or maximum with mask
    if (A.channels() > 1 && B.channels() == 1) {
        auto op = [&B, &stdminmax](auto& v, int const& x, int const& y, int const& /*c*/){
            using type = std::remove_reference_t<decltype(v)>;
            v = stdminmax(v, B.at<type>(x, y, 0));
        };
        CallBaseTypeFunctor::run(InplacePointOperationFunctor{A, mask, op}, A.type());
        return A;
    }


    if (A.channels() == 1 && B.channels() > 1) {
        std::vector<cv::Mat> aaa(B.channels(), A.cvMat());
        cv::merge(aaa, A.cvMat());
    }

    auto op = [&B, &stdminmax](auto& v, int const& x, int const& y, int const& c){
        using type = std::remove_reference_t<decltype(v)>;
        v = stdminmax(v, B.at<type>(x, y, c));
    };
    CallBaseTypeFunctor::run(InplacePointOperationFunctor{A, mask, op}, A.type());
    return A;
}

template<typename CvOp, typename CustomOp>
Image minimum_maximum_pixel(std::vector<double> const& pix, Image A, ConstImage const& mask, CvOp const& cvop, CustomOp const& customop) {
    using std::to_string;
    checkMask(mask, A.channels());

    if (pix.size() != 1 && pix.size() != A.channels())
        IF_THROW_EXCEPTION(invalid_argument_error("The number of pixel values must be one or match number of channels of the image. "
                                                  "Here the number of values is " + to_string(pix.size()) + ", but the image has " + to_string(A.channels()) + "."));

    // no mask --> use OpenCV
    if (mask.empty()) {
        cvop(A.cvMat(), pix, A.cvMat());
        return A;
    }

    // fallback code
    if (pix.size() < A.channels()) {
        double p = pix.front();
        auto op = [p, &customop] (auto& v, int /*x*/, int /*y*/, int /*c*/) {
            using type = std::remove_reference_t<decltype(v)>;
            v = cv::saturate_cast<type>(customop(static_cast<double>(v), p));
        };
        CallBaseTypeFunctor::run(InplacePointOperationFunctor{A, mask, op}, A.type());
    }
    else {
        auto op = [&pix, &customop](auto& v, int, int, int const& c){
            using type = std::remove_reference_t<decltype(v)>;
            v = cv::saturate_cast<type>(customop(static_cast<double>(v), pix.at(c)));
        };
        CallBaseTypeFunctor::run(InplacePointOperationFunctor{A, mask, op}, A.type());
    }

    return A;
}

template<typename CvOp, typename CustomOp>
Image add_subtract_pixel(std::vector<double> const& pix, Image A, ConstImage const& mask, CvOp const& cvop, CustomOp const& customop) {
    if (A.empty())
        return A;

    using std::to_string;
    checkMask(mask, A.channels());

    if (pix.size() != 1 && pix.size() != A.channels())
        IF_THROW_EXCEPTION(invalid_argument_error("The number of pixel values must be one or match number of channels of the image. "
                                                  "Here the number of values is " + to_string(pix.size()) + ", but the image has " + to_string(A.channels()) + "."));

    // try OpenCV operation first, for performance reasons, but it does not accept all types (e. g. it throws on uint8x2)
    if (mask.empty() || mask.channels() == 1) {
        try {
            cvop(A.cvMat(), pix, A.cvMat(), mask.cvMat());
            return A;
        } catch (cv::Exception&) {
            /* empty, fall through */
        }
    }

    // fallback code
    if (pix.size() < A.channels()) {
        double p = pix.front();
        auto op = [p, &customop] (auto& v, int /*x*/, int /*y*/, int /*c*/) {
            using type = std::remove_reference_t<decltype(v)>;
            v = cv::saturate_cast<type>(customop(v, p));
        };
        CallBaseTypeFunctor::run(InplacePointOperationFunctor{A, mask, op}, A.type());
    }
    else {
        auto op = [&pix, &customop](auto& v, int, int, int const& c){
            using type = std::remove_reference_t<decltype(v)>;
            v = cv::saturate_cast<type>(customop(v, pix.at(c)));
        };
        CallBaseTypeFunctor::run(InplacePointOperationFunctor{A, mask, op}, A.type());
    }

    return A;
}

} /* anonymous namespace */

Image ConstImage::minimum(Image&& B, ConstImage const& mask) const& {
    if (mask.empty())
        return std::move(B).minimum(*this, mask);
    return clone().minimum(B, mask); // order must be consistent when using masks
}

Image ConstImage::minimum(ConstImage const& B, ConstImage const& mask) const& {
    if (B.channels() < channels() || !mask.empty()) // for performance only in case of broadcasting and non-empty mask
        return clone().minimum(B, mask);     // A has multiple channels and can be used to save the result
    return B.clone().minimum(*this, mask);   // in case of a non-empty mask it must be A.minimum(B, mask) instead of B.minimum(A, mask)
}

Image Image::minimum(ConstImage const& B, ConstImage const& mask)&& {
    return minimum_maximum(std::move(*this), B, mask,
                           [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::min(mat1, mat2, res);},
                           [] (auto a, auto b) { return std::min(a, b); });
}

Image ConstImage::minimum(std::vector<double> const& pix, ConstImage const& mask) const& {
    return clone().minimum(pix, mask); // call rvalue method
}

Image Image::minimum(std::vector<double> const& pix, ConstImage const& mask)&& {
    return minimum_maximum_pixel(pix, std::move(*this), mask,
                                 [] (cv::Mat const& mat, std::vector<double> const& pix, cv::Mat& res) {
                                     cv::min(mat, pix, res);
                                 },
                                 [] (auto a, auto b) { return std::min(a, b); });
}


Image ConstImage::maximum(Image&& B, ConstImage const& mask) const& {
    if (mask.empty())
        return std::move(B).maximum(*this, mask);
    return clone().maximum(B, mask); // order must be consistent when using masks
}

Image ConstImage::maximum(ConstImage const& B, ConstImage const& mask) const& {
    if (B.channels() < channels() || !mask.empty()) // for performance only in case of broadcasting and non-empty mask
        return clone().maximum(B, mask);     // A has multiple channels and can be used to save the result
    return B.clone().maximum(*this, mask);   // in case of a non-empty mask it must be A.maximum(B, mask) instead of B.maximum(A, mask)
}

Image Image::maximum(ConstImage const& B, ConstImage const& mask)&& {
    return minimum_maximum(std::move(*this), B, mask,
                           [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::max(mat1, mat2, res);},
                           [] (auto a, auto b) { return std::max(a, b); });
}

Image ConstImage::maximum(std::vector<double> const& pix, ConstImage const& mask) const& {
    return clone().maximum(pix, mask); // call rvalue method
}

Image Image::maximum(std::vector<double> const& pix, ConstImage const& mask)&& {
    return minimum_maximum_pixel(pix, std::move(*this), mask,
                                 [] (cv::Mat const& mat, std::vector<double> const& pix, cv::Mat& res) {
                                     cv::max(mat, pix, res);
                                 },
                                 [] (auto a, auto b) { return std::max(a, b); });
}


Image ConstImage::add(ConstImage const& B, Type resultType) const {
    Image ret;
    cv::add(img, B.img, ret.img, cv::noArray(), toCVType(resultType));
    return ret;
}

Image ConstImage::add(Image&& B) const& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::add(mat1, mat2, res);});
}

Image ConstImage::add(ConstImage const& B) const& {
    if (B.channels() > channels()) // for performance only in case of broadcasting
        return add(B.clone());     // B has multiple channels and can be used to save the result
    return B.add(clone());
}

Image Image::add(ConstImage const& B)&& {
    return B.add(std::move(*this));
}

Image ConstImage::add(std::vector<double> const& pix, ConstImage const& mask, Type resultType) const& {
    if (resultType == Type::invalid || getBaseType(resultType) == basetype())
        return clone().add(pix, mask); // call rvalue method
    else // different type
        return convertTo(resultType).add(pix, mask); // call rvalue method
}

Image Image::add(std::vector<double> const& pix, ConstImage const& mask)&& {
    int sameResultType = toCVType(basetype());
    return add_subtract_pixel(pix, std::move(*this), mask,
                              [sameResultType] (cv::Mat const& mat, std::vector<double> const& pix, cv::Mat& res, cv::Mat const& mask) {
                                  cv::add(mat, pix, res, mask, sameResultType);
                              },
                              [] (auto a, auto b) { return a + b; });
}


Image ConstImage::subtract(ConstImage const& B, Type resultType) const {
    Image ret;
    cv::subtract(img, B.img, ret.img, cv::noArray(), toCVType(resultType));
    return ret;
}

Image ConstImage::subtract(Image&& B) const& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [](cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res){cv::subtract(mat1, mat2, res);});
}

Image ConstImage::subtract(ConstImage const& B) const& {
    return subtract(B.clone());
}

Image Image::subtract(ConstImage const& B)&& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(/*mat1*/ B, /*mat2*/ std::move(*this),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::subtract(mat2, mat1, res);});
}

Image ConstImage::subtract(std::vector<double> const& pix, ConstImage const& mask, Type resultType) const& {
    if (resultType == Type::invalid || getBaseType(resultType) == basetype())
        return clone().subtract(pix, mask); // call rvalue method
    else // different type
        return convertTo(resultType).subtract(pix, mask); // call rvalue method
}

Image Image::subtract(std::vector<double> const& pix, ConstImage const& mask)&& {
    int sameResultType = toCVType(basetype());
    return add_subtract_pixel(pix, std::move(*this), mask,
                              [sameResultType] (cv::Mat const& mat, std::vector<double> const& pix, cv::Mat& res, cv::Mat const& mask) {
                                  cv::subtract(mat, pix, res, mask, sameResultType);
                              },
                              [] (auto a, auto b) { return a - b; });
}


Image ConstImage::multiply(ConstImage const& B, Type resultType) const {
    Image ret;
    cv::multiply(img, B.img, ret.img, 1, toCVType(resultType));
    return ret;
}

Image ConstImage::multiply(Image&& B) const& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::multiply(mat1, mat2, res);});
}

Image ConstImage::multiply(ConstImage const& B) const& {
    if (B.channels() > channels())  // for performance only in case of broadcasting
        return multiply(B.clone()); // B has multiple channels and can be used to save the result
    return B.multiply(clone());
}

Image Image::multiply(ConstImage const& B)&& {
    return B.multiply(std::move(*this));
}

Image ConstImage::multiply(std::vector<double> const& pix, ConstImage const& mask, Type resultType) const& {
    if (resultType == Type::invalid || getBaseType(resultType) == basetype())
        return clone().multiply(pix, mask); // call rvalue method
    else // different type
        return convertTo(resultType).multiply(pix, mask); // call rvalue method
}

Image Image::multiply(std::vector<double> const& pix, ConstImage const& mask)&& {
    if (empty())
        return std::move(*this);

    using std::to_string;
    checkMask(mask, channels());

    if (pix.size() != 1 && pix.size() != channels())
        IF_THROW_EXCEPTION(invalid_argument_error("The number of pixel values must be one or match number of channels of the image. "
                                                  "Here the number of values is " + to_string(pix.size()) + ", but the image has " + to_string(channels()) + "."));

    // try OpenCV operation first, for performance reasons, but it does not accept all types (e. g. it throws on uint8x2)
    if (mask.empty()) {
        try {
            cv::multiply(img, pix, img, /* scale */ 1, toCVType(basetype()));
            return std::move(*this);
        } catch (cv::Exception&) {
            /* empty, fall through */
        }
    }

    std::vector<double> const& pix_n = pix.size() < channels() ? std::vector<double>(channels(), pix.front()) : pix;
    auto op = [&pix_n](auto& v, int, int, int const& c){
        using type = std::remove_reference_t<decltype(v)>;
        v = cv::saturate_cast<type>(v * pix_n.at(c));
    };
    CallBaseTypeFunctor::run(InplacePointOperationFunctor{*this, mask, op}, basetype());

    return std::move(*this);
}


Image ConstImage::divide(ConstImage const& B, Type resultType) const {
    Image ret;
    cv::divide(img, B.img, ret.img, 1, toCVType(resultType));
    return ret;
}

Image ConstImage::divide(Image&& B) const& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::divide(mat1, mat2, res);});
}

Image ConstImage::divide(ConstImage const& B) const& {
    return divide(B.clone());
}

Image Image::divide(ConstImage const& B)&& {
    if (empty() || B.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Empty images are not allowed for arithmetic operations (only for bitwise logical operations)."));

    return SimpleBroadcastOperation(/*mat1*/ B, /*mat2*/ std::move(*this),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::divide(mat2, mat1, res);});
}

Image ConstImage::divide(std::vector<double> const& pix, ConstImage const& mask, Type resultType) const& {
    if (resultType == Type::invalid || getBaseType(resultType) == basetype())
        return clone().divide(pix, mask); // call rvalue method
    else // different type
        return convertTo(resultType).divide(pix, mask); // call rvalue method
}

Image Image::divide(std::vector<double> const& pix, ConstImage const& mask)&& {
    if (empty())
        return std::move(*this);

    using std::to_string;
    checkMask(mask, channels());

    if (pix.size() != 1 && pix.size() != channels())
        IF_THROW_EXCEPTION(invalid_argument_error("The number of pixel values must be one or match number of channels of the image. "
                                                  "Here the number of values is " + to_string(pix.size()) + ", but the image has " + to_string(channels()) + "."));

    for (double d : pix) {
        if (d == 0) {
            std::string vals = "[";
            for (unsigned int i = 0; i < pix.size(); ++i) {
                vals += std::to_string(pix.at(i));
                if (i < pix.size() - 1)
                    vals += ", ";
                else
                    vals += "]";
            }
            IF_THROW_EXCEPTION(invalid_argument_error("You try to divide the image by zero. Your divisor is: " + vals));
        }
    }


    // try OpenCV operation first, for performance reasons, but it does not accept all types (e. g. it throws on uint8x2)
    if (mask.empty()) {
        try {
            cv::divide(img, pix, img, /* scale */ 1, toCVType(basetype()));
            return std::move(*this);
        } catch (cv::Exception&) {
            /* empty, fall through */
        }
    }

    std::vector<double> const& pix_n = pix.size() < channels() ? std::vector<double>(channels(), pix.front()) : pix;
    auto op = [&pix_n](auto& v, int, int, int const& c){
        using type = std::remove_reference_t<decltype(v)>;
        v = cv::saturate_cast<type>(v / pix_n.at(c));
    };
    CallBaseTypeFunctor::run(InplacePointOperationFunctor{*this, mask, op}, type());

    return std::move(*this);
}


Image ConstImage::bitwise_and(Image&& B) const& {
    if (B.empty())
        return clone();
    if (empty())
        return std::move(B);

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::bitwise_and(mat1, mat2, res);});
}

Image ConstImage::bitwise_and(ConstImage const& B) const& {
    return bitwise_and(B.clone());
}

Image Image::bitwise_and(ConstImage const& B)&& {
    return B.bitwise_and(std::move(*this));
}


Image ConstImage::bitwise_or(Image&& B) const& {
    if (B.empty())
        return clone();
    if (empty())
        return std::move(B);

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::bitwise_or(mat1, mat2, res);});
}

Image ConstImage::bitwise_or(ConstImage const& B) const& {
    return bitwise_or(B.clone());
}

Image Image::bitwise_or(ConstImage const& B)&& {
    return B.bitwise_or(std::move(*this));
}


Image ConstImage::bitwise_xor(Image&& B) const& {
    if (B.empty())
        return clone();
    if (empty())
        return std::move(B);

    return SimpleBroadcastOperation(*this, std::move(B),
                                    [] (cv::Mat const& mat1, cv::Mat const& mat2, cv::Mat& res) {cv::bitwise_xor(mat1, mat2, res);});
}

Image ConstImage::bitwise_xor(ConstImage const& B) const& {
    return bitwise_xor(B.clone());
}

Image Image::bitwise_xor(ConstImage const& B)&& {
    return B.bitwise_xor(std::move(*this));
}


Image ConstImage::bitwise_not() const& {
    Image ret;
    if (!empty())
        cv::bitwise_not(img, ret.img);
    return ret;
}

Image Image::bitwise_not()&& {
    if (!empty())
        cv::bitwise_not(img, img);
    return std::move(*this);
}

std::vector<std::pair<ValueWithLocation, ValueWithLocation>> ConstImage::minMaxLocations(ConstImage const& mask) const {
    std::vector<std::pair<ValueWithLocation, ValueWithLocation>> minmax(channels());
    if(mask.channels() != 1 && mask.channels() != channels())
        IF_THROW_EXCEPTION(image_type_error("The mask must have one channel or the same number of channels as the image. The image has "
                                            + std::to_string(channels()) + " and the mask " + std::to_string(mask.channels()) + " channels."))
                << errinfo_image_type(mask.type());

    auto img_layers = split();
    auto mask_layers = mask.split();
    for (unsigned int c = 0; c < channels(); ++c) {
        cv::Mat const& img = img_layers.at(c).img;
        double min, max;
        cv::Point pMin, pMax;
        if (mask.empty())
            cv::minMaxLoc(img, &min, &max, &pMin, &pMax);
        else if (mask.channels() == 1) {
            cv::minMaxLoc(img, &min, &max, &pMin, &pMax, mask.img);
        }
        else { // (mask.channels() == channels)
            cv::Mat const& m = mask_layers.at(c).img;
            cv::minMaxLoc(img, &min, &max, &pMin, &pMax, m);
        }
        minmax.at(c) = std::make_pair(ValueWithLocation{min, pMin}, ValueWithLocation{max, pMax});
    }

    return minmax;
}


std::vector<double> ConstImage::mean(ConstImage const& mask) const {
    std::vector<double> mean(channels());
    if (mask.channels() == 1 && channels() <= 4) {
        cv::Scalar mean_cv = cv::mean(img, mask.img);
        for (unsigned int i = 0; i < channels(); ++i)
            mean.at(i) = mean_cv[i];
    }
    else {
        if(mask.channels() != 1 && mask.channels() != channels())
            IF_THROW_EXCEPTION(image_type_error("The mask must have one channel or the same number of channels as the image"))
                    << errinfo_image_type(mask.type());

        auto img_layers = split();
        auto mask_layers = mask.split();
        cv::Scalar temp_mean;
        for (unsigned int c = 0; c < channels(); ++c) {
            cv::Mat const& img = img_layers.at(c).img;
            if (mask.empty())
                temp_mean = cv::mean(img);
            else if (mask.channels() == 1)
                temp_mean = cv::mean(img, mask.img);
            else { // (mask.channels() == channels)
                cv::Mat const& m = mask_layers.at(c).img;
                temp_mean = cv::mean(img, m);
            }
            mean.at(c) = temp_mean[0];
        }
    }

    return mean;
}


std::pair<std::vector<double>, std::vector<double>> ConstImage::meanStdDev(ConstImage const& mask, bool sampleCorrection) const {
    std::vector<double> mean(channels());
    std::vector<double> stdDev(channels());
    if (mask.channels() == 1 && channels() <= 4) {
        double cor = 1;
        if (sampleCorrection) {
            double n = mask.empty() ? size().area() : cv::countNonZero(mask.img);
            if (n > 1)
                cor = std::sqrt(n / (n - 1));
        }

        cv::Scalar mean_cv, stddev_cv;
        cv::meanStdDev(img, mean_cv, stddev_cv, mask.img);
        for (unsigned int i = 0; i < channels(); ++i) {
            mean.at(i)   = mean_cv[i];
            stdDev.at(i) = stddev_cv[i] * cor;
        }
    }
    else {
        if(mask.channels() != 1 && mask.channels() != channels())
            IF_THROW_EXCEPTION(image_type_error("The mask must have one channel or the same number of channels as the image"))
                    << errinfo_image_type(mask.type());

        auto img_layers = split();
        auto mask_layers = mask.split();
        cv::Scalar temp_mean, temp_stddev;
        for (unsigned int c = 0; c < channels(); ++c) {
            cv::Mat const& img = img_layers.at(c).img;

            double n = size().area();
            if (mask.empty())
                cv::meanStdDev(img, temp_mean, temp_stddev);
            else if (mask.channels() == 1) {
                cv::meanStdDev(img, temp_mean, temp_stddev, mask.img);
                if (sampleCorrection)
                    n = cv::countNonZero(mask.img);
            }
            else { // (mask.channels() == channels)
                cv::Mat const& m = mask_layers.at(c).img;
                cv::meanStdDev(img, temp_mean, temp_stddev, m);
                if (sampleCorrection)
                    n = cv::countNonZero(m);
            }

            double cor = 1;
            if (sampleCorrection && n > 1)
                cor = std::sqrt(n / (n - 1));

            mean.at(c)   = temp_mean[0];
            stdDev.at(c) = temp_stddev[0] * cor;
        }
    }

    return {mean, stdDev};
}


namespace {

template<class ForwardImgIt>
ForwardImgIt unique_with_mask(ForwardImgIt img_first, ForwardImgIt img_last, cv::MatConstIterator_<uint8_t> mask_first, cv::MatConstIterator_<uint8_t> mask_last)
{
    if (img_first == img_last || mask_first == mask_last)
        return img_last;

    ForwardImgIt img_result = img_first;
    while (!*mask_first && mask_first != mask_last) {
        ++mask_first;
        ++img_first;
    }

    // check if no location was valid
    if (mask_first == mask_last)
        return img_result;

    // check if first location was invalid
    if (img_result != img_first)
        // make sure the first position is valid
        *img_result = std::move(*img_first);

    // iterate
    while (++img_first != img_last && ++mask_first != mask_last)
        if (*mask_first && !(*img_result == *img_first) && ++img_result != img_first)
            *img_result = std::move(*img_first);

    return ++img_result;
}

struct UniqueFunctor {
    Image& img;
    ConstImage const& mask;

    template<Type basetype>
    std::vector<double> operator()() const {
        static_assert(getChannels(basetype) == 1, "This functor only accepts base type to reduce code size.");
        if (img.channels() != 1)
            IF_THROW_EXCEPTION(invalid_argument_error("Only single-channel images supported for unique. The image has " + std::to_string(img.channels()) + " channels.")) << errinfo_image_type(img.type());
        if (mask.type() != Type::uint8x1)
            IF_THROW_EXCEPTION(invalid_argument_error("Only single-channel masks (with Type uint8) supported for unique. The mask has type " + to_string(mask.type()) + ".")) << errinfo_image_type(mask.type());
        if (!mask.empty() && mask.size() != img.size())
            IF_THROW_EXCEPTION(invalid_argument_error("Mask and image have different sizes. image: " + to_string(img.size()) + ", mask: " + to_string(mask.size())));

        using type = typename DataType<basetype>::base_type;
        auto begin = img.begin<type>(), end = img.end<type>();
        // remove adjacent duplicates to reduce size
        auto last = mask.empty() ? std::unique(begin, end) : unique_with_mask(begin, end, mask.begin<uint8_t>(), mask.end<uint8_t>());
        std::sort(begin, last);                 // sort remaining elements
        last = std::unique(begin, last);        // remove duplicates

        std::vector<double> vals;
        while (begin != last) {
            vals.push_back(static_cast<double>(*begin));
            ++begin;
        }
        return vals;
    }
};

struct CountUniqueFunctor {
    ConstImage const& img;
    ConstImage const& mask;

    template<Type basetype>
    std::map<double, unsigned int> operator()() const {
        static_assert(getChannels(basetype) == 1, "This functor only accepts base type to reduce code size.");
        if (img.channels() != 1)
            IF_THROW_EXCEPTION(invalid_argument_error("Only single-channel images supported for unique. The image has " + std::to_string(img.channels()) + " channels.")) << errinfo_image_type(img.type());
        if (mask.type() != Type::uint8x1)
            IF_THROW_EXCEPTION(invalid_argument_error("Only single-channel masks (with Type uint8) supported for unique. The mask has type " + to_string(mask.type()) + ".")) << errinfo_image_type(mask.type());
        if (!mask.empty() && mask.size() != img.size())
            IF_THROW_EXCEPTION(invalid_argument_error("Mask and image have different sizes. image: " + to_string(img.size()) + ", mask: " + to_string(mask.size())));

        using type = typename DataType<basetype>::base_type;
        std::map<double, unsigned int> unique;
        auto img_it = img.begin<type>(), img_end = img.end<type>();
        if (mask.empty()) {
            for (; img_it != img_end; ++img_it)
                ++unique[*img_it];
        }
        else {
            auto mask_it = mask.begin<uint8_t>(), mask_end = mask.end<uint8_t>();
            for (; img_it != img_end && mask_it != mask_end; ++img_it, ++mask_it)
                if (*mask_it)
                    ++unique[*img_it];
        }
        return unique;
    }
};
} /* anonymous namespace */

std::vector<double> ConstImage::unique(ConstImage const& mask) const {
    return clone().unique(mask);
}

std::vector<double> Image::unique(ConstImage const& mask)&& {
    return CallBaseTypeFunctor::run(UniqueFunctor{*this, mask}, basetype());
}

std::map<double, unsigned int> ConstImage::uniqueWithCount(ConstImage const& mask) const {
    return CallBaseTypeFunctor::run(CountUniqueFunctor{*this, mask}, basetype());
}

void Image::set(double val, ConstImage const& mask) {
    set(std::vector<double>(channels(), val), mask);
}

void Image::set(std::vector<double> const& vals, ConstImage const& mask) {
    using std::to_string;
    checkMask(mask, channels());
    if (vals.size() != channels())
        IF_THROW_EXCEPTION(invalid_argument_error("The number of values must match number of channels as the image. "
                                                  "Here the number of values is " + to_string(vals.size()) + ", but the image has " + to_string(channels()) + "."));

    if (mask.type() == Type::uint8x1 && channels() <= 4) { // OpenCV supports multi-channel masks since version 3.3.1. So in future, we can simplify this.
        cv::Scalar cvVals;
        for (unsigned int i = 0; i < vals.size() && i < 4; ++i)
            cvVals[i] = vals.at(i);
        img.setTo(cvVals, mask.cvMat());
        return;
    }

    auto op = [&vals] (auto& v, int, int, int const& c) {
        using type = std::remove_reference_t<decltype(v)>;
        v = cv::saturate_cast<type>(vals.at(c));
    };
    CallBaseTypeFunctor::run(InplacePointOperationFunctor{*this, mask, op}, basetype());

    // Alternative implementation based on OpenCV functions. Performance results for an older version:
    // Requires more memory (ca. x2), could be faster for some situations and slower for other (33% faster or slower).
//    unsigned int chans = channels();
//    std::vector<cv::Mat> img_chans(chans);
//    cv::split(img, img_chans);

//    std::vector<cv::Mat> mask_chans(chans);
//    cv::split(mask.img, mask_chans);

//    for (unsigned int c = 0; c < chans; ++c)
//        img_chans[c].setTo(val[c], mask_chans[c]);

//    cv::merge(img_chans, img);
}

static std::array<std::vector<double>,2> fixBounds(std::vector<Interval> const& channelRanges, Type t) {
    using std::to_string;
    int size = channelRanges.size();
    int chans = getChannels(t);
    if (size != 1 && size != chans)
        IF_THROW_EXCEPTION(image_type_error("The number of ranges is not valid. Either give one range that is used for all channels or give one range per channel. "
                                            "Here the image has " + to_string(chans) + " channels and the number of ranges is " + to_string(size) + "."))
                << errinfo_image_type(t);

    // for integer images, values must be in the int32 range
    std::vector<double> lowBounds;
    std::vector<double> highBounds;
    if (isIntegerType(t)) {
        for (Interval const& i : channelRanges) {
            // limit on int32 range, but keep nan
            bool leftClosed  = i.bounds().left()  != boost::icl::interval_bounds::open();
            bool rightClosed = i.bounds().right() != boost::icl::interval_bounds::open();
            double l = i.lower();
            double u = i.upper();

            if (l < std::numeric_limits<int32_t>::min()) {
                l = std::numeric_limits<int32_t>::min();
                leftClosed = true;
            }

            if (u > std::numeric_limits<int32_t>::max()) {
                u = std::numeric_limits<int32_t>::max();
                rightClosed = true;
            }

            // handle open intervals
            if (leftClosed)
                l = std::ceil(l);
            else
                l = std::floor(l);
            if (rightClosed)
                u = std::floor(u);
            else
                u = std::ceil(u);
            lowBounds.push_back( std::isnan(l) || leftClosed  ? l : l + 1);
            highBounds.push_back(std::isnan(u) || rightClosed ? u : u - 1);
        }
    }
    else { // floating point image. We do not support real open intervals, since cv::inRange handles only closed intervals. So we handle it as closed intervals.
        for (Interval const& i : channelRanges) {
            if (i.bounds().left() == boost::icl::interval_bounds::open() || i.bounds().right() == boost::icl::interval_bounds::open())
                std::cerr << "Warning: Currently open intervals, like " << i << ", are not supported for floating point images. Converted it to closed interval. Maybe use a flip operation on intervals, if appropriate." << std::endl;
            lowBounds.push_back(i.lower());
            highBounds.push_back(i.upper());
        }
    }

    // fill up bounds
    if (size < chans) {
        lowBounds.resize(chans, lowBounds.front());
        highBounds.resize(chans, highBounds.front());
    }

    return {std::move(lowBounds), std::move(highBounds)};
}

static std::vector<IntervalSet> fixBounds(std::vector<IntervalSet> channelSets, Type t) {
    using std::to_string;
    int size = channelSets.size();
    int chans = getChannels(t);
    if (size != 1 && size != chans)
        IF_THROW_EXCEPTION(image_type_error("The number of sets is not valid. Either give one set that is used for all channels or give one set per channel. "
                                            "Here the image has " + to_string(chans) + " channels and the number of sets is " + to_string(size) + "."))
                << errinfo_image_type(t);

    // for integer images, values must be in the int32 range and we like to convert intervals to closed intervals
    if (isIntegerType(t))
        for (IntervalSet& is : channelSets)
            is = discretizeBounds(is);

    // fill up sets
    if (size < chans)
        channelSets.resize(chans, channelSets.front());

    return channelSets;
}

Image ConstImage::createSingleChannelMaskFromRange(std::vector<Interval> const& channelRanges, bool useAnd) const {
    auto l_h = fixBounds(channelRanges, type());
    std::vector<double> const& low  = l_h[0];
    std::vector<double> const& high = l_h[1];

    Image ret;
    if (useAnd)
        cv::inRange(img, low, high, ret.img);
    else {
        // apply inRange channel-wise
        unsigned int chans = channels();
        std::vector<cv::Mat> img_chans(chans);
        cv::split(img, img_chans);

        for (unsigned int c = 0; c < chans; ++c) {
            if (c == 0)
                // first iteration
                cv::inRange(img_chans[c], low[c], high[c], ret.cvMat());
            else {
                cv::Mat temp_mask;
                cv::inRange(img_chans[c], low[c], high[c], temp_mask);
                ret.img |= temp_mask;
            }
        }
    }
    return ret;
}

Image ConstImage::createMultiChannelMaskFromRange(std::vector<Interval> const& channelRanges) const {
    unsigned int chans = channels();
    if (chans == 1)
        return createSingleChannelMaskFromRange(channelRanges);

    auto l_h = fixBounds(channelRanges, type());
    std::vector<double> const& low  = l_h[0];
    std::vector<double> const& high = l_h[1];

    // apply inRange channel-wise
    std::vector<cv::Mat> img_chans(chans);
    cv::split(img, img_chans);

    std::vector<cv::Mat> mask_chans(chans);
    for (unsigned int c = 0; c < chans; ++c)
        cv::inRange(img_chans[c], low[c], high[c], mask_chans[c]);

    Image ret;
    cv::merge(mask_chans, ret.img);
    return ret;
}


static std::vector<cv::Mat> getMaskVec(ConstImage const& src, std::vector<IntervalSet> const& channelSets) {
    assert(static_cast<unsigned int>(src.channels()) == channelSets.size());

    // apply inRange channel- and interval-wise, use OR to relate the intervals
    unsigned int chans = src.channels();
    std::vector<cv::Mat> img_chans(chans);
    cv::split(src.cvMat(), img_chans);

    std::vector<cv::Mat> mask_chans(chans);
    cv::Mat temp;
    for (unsigned int c = 0; c < chans; ++c) {
        mask_chans[c] = cv::Mat(src.height(), src.width(), CV_8UC1);
        mask_chans[c] = cv::Scalar::all(0);

        IntervalSet const& is = channelSets.at(c);
        for (Interval const& i : is) {
            cv::inRange(img_chans[c], i.lower(), i.upper(), temp);
            mask_chans[c] |= temp;
        }
    }
    return mask_chans;
}


Image ConstImage::createSingleChannelMaskFromSet(std::vector<IntervalSet> const& channelSets, bool useAnd) const {
    auto sets = fixBounds(channelSets, type());
    std::vector<cv::Mat> mask_chans = getMaskVec(*this, sets);

    // combine channel masks with AND
    Image ret(size(), Type::uint8x1);
    if (useAnd) {
        ret.set(255);
        for (cv::Mat const& m : mask_chans)
            ret.img &= m;
    }
    else {
        ret.set(0);
        for (cv::Mat const& m : mask_chans)
            ret.img |= m;
    }

    return ret;
}

Image ConstImage::createMultiChannelMaskFromSet(std::vector<IntervalSet> const& channelSets) const {
    std::vector<IntervalSet> sets = fixBounds(channelSets, type());
    std::vector<cv::Mat> mask_chans = getMaskVec(*this, sets);

    Image ret;
    cv::merge(mask_chans, ret.img);
    return ret;
}


Image ConstImage::convertTo(Type t) const {
    Image ret;
    img.convertTo(ret.img, toCVType(getBaseType(t)));
    return ret;
}

namespace {
template<Type imfu_src_type>
struct ConvertToGrayFunctor {
    ConstImage const& src;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), imfu_dst_type};
        constexpr bool doRound = isIntegerType(imfu_dst_type);
        constexpr double scale = getImageRangeMax(imfu_dst_type) / getImageRangeMax(imfu_src_type);
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double val = 0.299 * src.at<stype>(x, y, srcChans[0])
                           + 0.587 * src.at<stype>(x, y, srcChans[1])
                           + 0.114 * src.at<stype>(x, y, srcChans[2]);
                dst.at<dtype>(x, y) = static_cast<dtype>((doRound ? 0.5 : 0) + scale * val);
            }
        }
        return dst;
    }
};

template<Type imfu_src_type>
struct ConvertToNDIFunctor {
    ConstImage const& src;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), imfu_dst_type};
        constexpr double scale  = getImageRangeMax(imfu_dst_type) / (std::is_signed<dtype>::value ? 1 : 2);
        constexpr double offset = std::is_signed<dtype>::value ? 0 : scale;
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                const stype val1 = src.at<stype>(x, y, srcChans[0]);
                const stype val2 = src.at<stype>(x, y, srcChans[1]);
                const stype sum = val1 + val2;
                const double res = sum == 0 ? 0 : (val1 - val2) * (scale / sum) + offset; // (scale / sum) converts to double
                dst.at<dtype>(x, y) = cv::saturate_cast<dtype>(res);
            }
        }
        return dst;
    }
};

template<Type imfu_src_type>
struct ConvertToBUFunctor {
    ConstImage const& src;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), imfu_dst_type};
        constexpr double scale  = getImageRangeMax(imfu_dst_type) / (std::is_signed<dtype>::value ? 1 : 2);
        constexpr double offset = std::is_signed<dtype>::value ? 0 : scale;
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                const stype red  = src.at<stype>(x, y, srcChans[0]);
                const stype nir  = src.at<stype>(x, y, srcChans[1]);
                const stype swir = src.at<stype>(x, y, srcChans[2]);
                const double sum_sn = swir + nir;
                const double sum_nr = nir + red;
                const double res = (  (sum_sn == 0 ? 0 : (swir - nir) / sum_sn)
                                    - (sum_nr == 0 ? 0 : (nir - red)  / sum_nr))
                                   * (scale / 2) + offset;
                dst.at<dtype>(x, y) = cv::saturate_cast<dtype>(res);  // casting non fitting results to integers is UB (wrong number or crash?)
            }
        }
        return dst;
    }
};

template<Type imfu_src_type>
struct ConvertToTesseledCapFunctor {
    ConstImage const& src;
    ColorMapping map;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        constexpr double scale =  getImageRangeMax(imfu_dst_type) / getImageRangeMax(imfu_src_type) / (std::is_signed<dtype>::value ? 1 : 2);
        constexpr double offset = std::is_signed<dtype>::value ? 0 : getImageRangeMax(imfu_dst_type) / 2;
        std::array<double, 3*7> f;
        if (map == ColorMapping::Landsat_to_TasseledCap) {
            constexpr double max = 1 / 2.3103; // if the pixels are all one, this is one over the sum of brightness
            f = std::array<double, 3*7>{ // values from: "A Physically-Based Transformation of Thematic Mapper Data - The TM Tasseled Cap" by Crist and Cicone, 1984
                //       Blue,         Green,           Red,           NIR,         SWIR1,         SWIR2
                +0.3037 * max, +0.2793 * max, +0.4743 * max, +0.5585 * max, +0.5082 * max, +0.1863 * max, 0, // Brightness  // TODO: Check if values are correct, for which Landsat: 4, 5, 7, Digital Numbers, Reflectance factors??
                -0.2848 * max, -0.2435 * max, -0.5436 * max, +0.7243 * max, +0.0840 * max, -0.1800 * max, 0, // Greenness
                +0.1509 * max, +0.1973 * max, +0.3279 * max, +0.3406 * max, -0.7112 * max, -0.4572 * max, 0  // Wetness
            };
        }
        else if (map == ColorMapping::Modis_to_TasseledCap) {
            constexpr double max = 1 / 2.6206; // if the pixels are all one, this is one over the sum of brightness
            f = std::array<double, 3*7>{ // values from: "MODIS Tasseled Cap Transformation and its Utility" by Zhang et al, 2016
                //        Red        Near-IR           Blue          Green           M-IR           M-IR           M-IR
                //    620-670        841-876        459-479        545-565      1230-1250      1628-1652      2105-2155
                +0.3956 * max, +0.4718 * max, +0.3354 * max, +0.3834 * max, +0.3946 * max, +0.3434 * max, +0.2964 * max, // Brightness
                -0.3399 * max, +0.5952 * max, -0.2129 * max, -0.2222 * max, +0.4617 * max, -0.1037 * max, -0.4600 * max, // Greenness
                +0.10839* max, +0.0912 * max, +0.5065 * max, +0.4040 * max, -0.2410 * max, -0.4658 * max, -0.5306 * max  // Wetness
            };
        }
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double brightness = 0;
                double greeness = 0;
                double wetness = 0;
                for (unsigned int i = 0; i < srcChans.size(); ++i) {
                    double src_val = src.at<stype>(x, y, srcChans[i]);
                    brightness += f[i]    * src_val;
                    greeness   += f[i+7]  * src_val;
                    wetness    += f[i+14] * src_val;
                }
                brightness = scale * brightness + offset;
                greeness   = scale * greeness   + offset;
                wetness    = scale * wetness    + offset;
                dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(brightness);
                dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(greeness);
                dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(wetness);
            }
        }
        return dst;
    }
};


template<Type imfu_src_type>
struct ConvertToYCbCrFunctor {
    ConstImage const& src;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        constexpr double scale = getImageRangeMax(imfu_dst_type) / getImageRangeMax(imfu_src_type);
        constexpr double rangeScale = std::is_signed<dtype>::value ? 2 : 1;
        constexpr double offset = std::is_signed<dtype>::value ? 0 : getImageRangeMax(imfu_dst_type) / 2;
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                const double R = src.at<stype>(x, y, srcChans[0]) * scale;
                const double G = src.at<stype>(x, y, srcChans[1]) * scale;
                const double B = src.at<stype>(x, y, srcChans[2]) * scale;
                const double Y = 0.299 * R + 0.587 * G + 0.114 * B;
                dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(Y);
                dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>((B - Y) * (rangeScale / 1.772) + offset);
                dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>((R - Y) * (rangeScale / 1.402) + offset);
            }
        }
        return dst;
    }
};


template<Type imfu_src_type>
struct ConvertFromYCbCrFunctor {
    ConstImage const& src;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        constexpr double scale = getImageRangeMax(imfu_dst_type) / getImageRangeMax(imfu_src_type);
        constexpr double rangeScale = std::is_signed<stype>::value ? 2 : 1;
        constexpr double offset = std::is_signed<stype>::value ? 0 : getImageRangeMax(imfu_src_type) / 2;
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                const double Y  = src.at<stype>(x, y, srcChans[0]) * scale;
                const double BY = (src.at<stype>(x, y, srcChans[1]) - offset) * (1.772 * scale / rangeScale);
                const double RY = (src.at<stype>(x, y, srcChans[2]) - offset) * (1.402 * scale / rangeScale);
                dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(Y + RY);
                dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(Y - (0.114 * BY + 0.299 * RY) * (1. / 0.587));
                dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(Y + BY);
            }
        }
        return dst;
    }
};


template<Type imfu_src_type>
struct ConvertToCIEFunctor {
    ConstImage const& src;
    ColorMapping map;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        constexpr double unscale = 1. / getImageRangeMax(imfu_src_type);
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                const double R = src.at<stype>(x, y, srcChans[0]) * unscale;
                const double G = src.at<stype>(x, y, srcChans[1]) * unscale;
                const double B = src.at<stype>(x, y, srcChans[2]) * unscale;
                const double X = 0.4339532647955942 * R + 0.3762192150477565 * G + 0.1898275201566493 * B;
                const double Y = 0.2126713120163335 * R + 0.7151598465029813 * G + 0.0721688414806854 * B;
                const double Z = 0.0177577409665738 * R + 0.1094769889865922 * G + 0.8727652700468339 * B;

                constexpr double scale = getImageRangeMax(imfu_dst_type);
                if (map == ColorMapping::RGB_to_Lab || map == ColorMapping::RGB_to_Luv) {
                    constexpr double delta = 6. / 29.;
                    auto f = [](double t) { return t > std::pow(delta, 3)
                                                   ? std::cbrt(t)
                                                   : t * (1. / (3 * delta * delta)) + 4. / 29.; };
                    const double L = 1.16 * f(Y) - 0.16;  // range [0,1]
                    double val1;
                    double val2;
                    if (map == ColorMapping::RGB_to_Lab) {
                        constexpr double normScale  = !isIntegerType(imfu_dst_type) ? 1 : (std::is_signed<dtype>::value ? 107.862 * scale / (scale + 1) : 206.0972);
                        constexpr double normOffset = std::is_signed<dtype>::value ? 0 : 0.52335499948568;
                        val1 /*a*/ = (500 / normScale) * (f(X) - f(Y)) + normOffset; // float / original range [-86.1813, 98.2352], signed int range (scale+1)/scale*[-0.798995939, 0.9107489], unsigned int range [0.10519648, 1]
                        val2 /*b*/ = (200 / normScale) * (f(Y) - f(Z)) + normOffset; // float / original range [-107.862, 94.4758], signed int range (scale+1)/scale*[-1, 0.8758951], unsigned int range [0, 0.981759]
                    }
                    else {
                        constexpr double normScale  = !isIntegerType(imfu_dst_type) ? 1 : (std::is_signed<dtype>::value ? 187.66 : 313.204);
                        constexpr double normOffset = std::is_signed<dtype>::value ? 0 : 0.400837792620784;
                        double d = 1. / (X + 15 * Y + 3 * Z + 1e-100);
                        val1 /*u*/ = (1300 / normScale) * L * (4 * X * d - 0.2009) + normOffset; // float / original range [-78.9986, 187.66], signed int range [-0.4209666, 1], unsigned int range [0.14861049, 1]
                        val2 /*v*/ = (1300 / normScale) * L * (9 * Y * d - 0.461)  + normOffset; // float / original range [-125.544, 116.356], signed int range [-0.668997, 0.620036], unsigned int range [0, 0.77234007]
                    }

                    dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(L    * scale);
                    dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(val1 * scale);
                    dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(val2 * scale);
                }
                else { // just to XYZ
                    dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(X * scale);
                    dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(Y * scale);
                    dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(Z * scale);
                }
            }
        }
        return dst;
    }
};


template<Type imfu_src_type>
struct ConvertFromCIEFunctor {
    ConstImage const& src;
    ColorMapping map;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        constexpr double scale   = getImageRangeMax(imfu_dst_type);
        constexpr double unscale = 1. / getImageRangeMax(imfu_src_type);
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double X = src.at<stype>(x, y, srcChans[0]) * unscale; // might be X or L
                double Y = src.at<stype>(x, y, srcChans[1]) * unscale; // might be Y, a or u
                double Z = src.at<stype>(x, y, srcChans[2]) * unscale; // might be Z, b or v

                if (map == ColorMapping::Lab_to_RGB || map == ColorMapping::Luv_to_RGB) {
                    constexpr double delta = 6. / 29.;
                    auto f = [](double t) { return t > delta
                                                   ? t * t * t
                                                   : 3 * delta * delta * (t - 4. / 29.); };
                    if (map == ColorMapping::Lab_to_RGB) {
                        const double L = (X + 0.16) * (1. / 1.16);
                        constexpr double normScale  = !isIntegerType(imfu_src_type) ? 1 : (std::is_signed<stype>::value ? 107.862 / unscale / (1/unscale + 1) : 206.0972);
                        constexpr double normOffset = std::is_signed<stype>::value ? 0 : 0.52335499948568;
                        const double a = (normScale / 500) * (Y - normOffset);
                        const double b = (normScale / 200) * (Z - normOffset);
                        X = f(L + a);
                        Y = f(L);
                        Z = f(L - b);
                    }
                    else { // map == ColorMapping::Luv_to_RGB
                        const double& L = X;
                        if (L == 0) {
                            Y = Z = 0;
                        }
                        else {
                            constexpr double normScale  = !isIntegerType(imfu_src_type) ? 1 : (std::is_signed<stype>::value ? 187.66 : 313.204);
                            constexpr double normOffset = std::is_signed<stype>::value ? 0 : 0.400837792620784;
                            double fac = (normScale / 1300) / L;
                            const double u = fac * (Y - normOffset) + 0.2009;
                            const double v = fac * (Z - normOffset) + 0.461;
                            Y = f((L + 0.16) * (1. / 1.16));
                            X = Y * (9. / 4.) * u / v;
                            Z = Y * (12 - 3 * u - 20 * v) / (4 * v);
                        }
                    }
                }

                const double R =  3.0799307362950428 * X - 1.5371486218345551 * Y - 0.5427821144604876 * Z;
                const double G = -0.9212345908547435 * X + 1.8759903180383508 * Y + 0.0452442728163921 * Z;
                const double B =  0.0528909416210833 * X - 0.2040428170607866 * Y + 1.1511518754397034 * Z;
                dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(R * scale);
                dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(G * scale);
                dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(B * scale);
            }
        }
        return dst;
    }
};


template<Type imfu_src_type>
struct ConvertToHueFunctor {
    ConstImage const& src;
    ColorMapping map;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        constexpr double unscale = 1. / getImageRangeMax(imfu_src_type);
        constexpr double scale = getImageRangeMax(imfu_dst_type);
        constexpr double Hscale = isIntegerType(imfu_dst_type) ? 1 : 360;
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                const double R = src.at<stype>(x, y, srcChans[0]) * unscale;
                const double G = src.at<stype>(x, y, srcChans[1]) * unscale;
                const double B = src.at<stype>(x, y, srcChans[2]) * unscale;
                const double Vmax = std::max({R, G, B});
                const double Vmin = std::min({R, G, B});
                const double f = (Hscale / 6) / (Vmax - Vmin);
                double H;
                if (Vmax == Vmin)
                    H = 0;
                else if (Vmax == R)
                    H = (G - B) * f;
                else if (Vmax == G)
                    H = (B - R) * f + 2 * Hscale / 6;
                else //if (Vmax == B)
                    H = (R - G) * f + 4 * Hscale / 6;

                if (H < 0)
                    H += Hscale;

                dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(scale * H);
                if (map == ColorMapping::RGB_to_HSV) {
                    const double S = Vmax == 0 ? 0 : 1 - Vmin / Vmax;
                    dst.at<dtype>(x, y, 1) /*S*/ = cv::saturate_cast<dtype>(scale * S);
                    dst.at<dtype>(x, y, 2) /*V*/ = cv::saturate_cast<dtype>(scale * Vmax);
                }
                else {
                    const double L = (Vmin + Vmax) * 0.5;
                    const double S = L == 0 || L == 1 ? 0 : (Vmax - Vmin) * 0.5 / (L < 0.5 ? L : 1 - L);
                    dst.at<dtype>(x, y, 1) /*L*/ = cv::saturate_cast<dtype>(scale * L);
                    dst.at<dtype>(x, y, 2) /*S*/ = cv::saturate_cast<dtype>(scale * S);
                }
            }
        }
        return dst;
    }
};


template<Type imfu_src_type>
struct ConvertFromHueFunctor {
    ConstImage const& src;
    ColorMapping map;
    std::vector<unsigned int> srcChans;

    template<Type imfu_dst_type>
    Image operator()() {
        using stype = typename DataType<imfu_src_type>::base_type;
        using dtype = typename DataType<imfu_dst_type>::base_type;
        Image dst{src.size(), getFullType(imfu_dst_type, 3)};
        constexpr double scale = getImageRangeMax(imfu_dst_type);
        constexpr double unscale = 1. / getImageRangeMax(imfu_src_type);
        constexpr double Hscale = isIntegerType(imfu_src_type) ? 1 : 360;
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double C;
                double m;
                if (map == ColorMapping::HSV_to_RGB) {
                    const double S = src.at<stype>(x, y, srcChans[1]) * unscale;
                    double V = src.at<stype>(x, y, srcChans[2]) * unscale;
                    if (S == 0) {
                        V *= scale;
                        dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(V);
                        dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(V);
                        dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(V);
                        continue;
                    }
                    C = V * S;
                    m = V - C;
                }
                else {
                    double L = src.at<stype>(x, y, srcChans[1]) * unscale;
                    const double S = src.at<stype>(x, y, srcChans[2]) * unscale;
                    if (S == 0) {
                        L *= scale;
                        dst.at<dtype>(x, y, 0) = cv::saturate_cast<dtype>(L);
                        dst.at<dtype>(x, y, 1) = cv::saturate_cast<dtype>(L);
                        dst.at<dtype>(x, y, 2) = cv::saturate_cast<dtype>(L);
                        continue;
                    }
                    C = (1 - std::abs(2 * L - 1)) * S;
                    m = L - C * 0.5;
                }
                double H = src.at<stype>(x, y, srcChans[0]) * (6 * unscale / Hscale);
                while (H <  0) H += 6;
                while (H >= 6) H -= 6;
                int HCase = static_cast<int>(H);
                H -= HCase & (~1); // subtract next lower even number. Same as H = H % 2, but for double, i. e. H = std::fmod(H, 2).
                const double X = C * (1 - std::abs(H - 1)) + m;
                C += m;

                // order for C, X, m
                std::array<std::array<int, 3>, 6> order{{{0, 1, 2},  // H in [  0°,  60°) ==> R = C, G = X, B = m
                                                         {1, 0, 2},  // H in [ 60°, 120°) ==> R = X, G = C, B = m
                                                         {1, 2, 0},  // H in [120°, 180°) ==> R = m, G = C, B = X
                                                         {2, 1, 0},  // H in [180°, 240°) ==> R = m, G = X, B = C
                                                         {2, 0, 1},  // H in [240°, 300°) ==> R = X, G = 0, B = C
                                                         {0, 2, 1}}};// H in [300°, 360°) ==> R = C, G = 0, B = X

                dst.at<dtype>(x, y, order[HCase][0]) = cv::saturate_cast<dtype>(scale * C);
                dst.at<dtype>(x, y, order[HCase][1]) = cv::saturate_cast<dtype>(scale * X);
                dst.at<dtype>(x, y, order[HCase][2]) = cv::saturate_cast<dtype>(scale * m);
            }
        }
        return dst;
    }
};





struct ConvertFunctor {
    ConstImage const& src;
    ColorMapping map;
    std::vector<unsigned int> srcChans;
    Type dstType;

    template<Type srcType>
    Image operator()() {
//        using stype = typename DataType<srcType>::base_type;
//        Image posMask;
//        if (std::numeric_limits<stype>::is_signed) {
//            posMask = Image{src.size(), Type::uint8x1};
//            posMask.set(0);
//            int chans = srcChans.size();
//            for (int y = 0; y < src.height(); ++y)
//                for (int x = 0; x < src.width(); ++x)
//                    for (int c = 0; c < chans; ++c)
//                        posMask.setBoolAt(x, y, 0, posMask.boolAt(x, y, 0) || src.at<stype>(x, y, srcChans[c]) < 0);
//        }

        switch (map) {
        case ColorMapping::RGB_to_Gray:
            return CallBaseTypeFunctor::run(ConvertToGrayFunctor<srcType>{src, std::move(srcChans)}, dstType);
        case ColorMapping::RGB_to_YCbCr:
            return CallBaseTypeFunctor::run(ConvertToYCbCrFunctor<srcType>{src, std::move(srcChans)}, dstType);
        case ColorMapping::YCbCr_to_RGB:
            return CallBaseTypeFunctor::run(ConvertFromYCbCrFunctor<srcType>{src, std::move(srcChans)}, dstType);
        case ColorMapping::RGB_to_XYZ:
        case ColorMapping::RGB_to_Lab:
        case ColorMapping::RGB_to_Luv:
            return CallBaseTypeFunctor::run(ConvertToCIEFunctor<srcType>{src, map, std::move(srcChans)}, dstType);
        case ColorMapping::XYZ_to_RGB:
        case ColorMapping::Lab_to_RGB:
        case ColorMapping::Luv_to_RGB:
            return CallBaseTypeFunctor::run(ConvertFromCIEFunctor<srcType>{src, map, std::move(srcChans)}, dstType);
        case ColorMapping::RGB_to_HSV:
        case ColorMapping::RGB_to_HLS:
            return CallBaseTypeFunctor::run(ConvertToHueFunctor<srcType>{src, map, std::move(srcChans)}, dstType);
        case ColorMapping::HSV_to_RGB:
        case ColorMapping::HLS_to_RGB:
            return CallBaseTypeFunctor::run(ConvertFromHueFunctor<srcType>{src, map, std::move(srcChans)}, dstType);
        case ColorMapping::Pos_Neg_to_NDI:
            return CallBaseTypeFunctor::run(ConvertToNDIFunctor<srcType>{src, std::move(srcChans)}, dstType);
        case ColorMapping::Red_NIR_SWIR_to_BU:
            return CallBaseTypeFunctor::run(ConvertToBUFunctor<srcType>{src, std::move(srcChans)}, dstType);
        case ColorMapping::Landsat_to_TasseledCap: // TODO: Add more
        case ColorMapping::Modis_to_TasseledCap:
            return CallBaseTypeFunctor::run(ConvertToTesseledCapFunctor<srcType>{src, map, std::move(srcChans)}, dstType);
        default:
            assert(false && "This should not happen. Fix ConvertFunctor!");
            return Image{};
        }
    }
};

constexpr int requiredChannels(ColorMapping map) {
    return map == ColorMapping::Pos_Neg_to_NDI         ? 2 :
           map == ColorMapping::Modis_to_TasseledCap   ? 7 :
           map == ColorMapping::Landsat_to_TasseledCap ? 6 : // TODO: add more landsat transforms
                                                         3;
}
} /* anonymous namespace */

Image ConstImage::convertColor(ColorMapping map, Type result, std::vector<unsigned int> sourceChannels) const {
//    using CM = ColorMapping;
    for (unsigned int sc : sourceChannels)
        if (sc >= channels())
            IF_THROW_EXCEPTION(invalid_argument_error("Source channels must be in the range [0, channels-1]. One is: " + std::to_string(sc)));

    unsigned int requiredSrcChannels = requiredChannels(map);
    if (channels() < requiredSrcChannels)
        IF_THROW_EXCEPTION(image_type_error("Color space conversion from " + getFromString(map) + " requires the image to have at least " + std::to_string(requiredSrcChannels) + " channels. The provided image has " + std::to_string(channels()) + " channels."))
                << errinfo_image_type(type());

    if (!sourceChannels.empty() && sourceChannels.size() != requiredSrcChannels) {
        std::string chans;
        for (int sc : sourceChannels)
            chans += std::to_string(sc) + ", ";
        chans.pop_back();
        chans.pop_back();

        IF_THROW_EXCEPTION(invalid_argument_error("You provided an invalid number of source channels for a color space conversion from " + getFromString(map)
                                                  + ". Either no source channels should be given or " + std::to_string(requiredSrcChannels)
                                                  + ". You gave: " + std::to_string(sourceChannels.size()) + " [" + chans + "]"));
    }

    result = getBaseType(result);
    if (result == Type::invalid)
        result = basetype();

    // fill sourceChannel with 0, 1, ...
    if (sourceChannels.empty()) {
        sourceChannels.resize(requiredSrcChannels);
        std::iota(sourceChannels.begin(), sourceChannels.end(), 0);
    }

    return CallBaseTypeFunctor::run(ConvertFunctor{*this, map, std::move(sourceChannels), result}, basetype());
}


} /* namespace imagefusion */

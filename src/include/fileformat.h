#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "exceptions.h"

namespace imagefusion {

/**
 * @brief Error information for the image format
 *
 * Add to an exception `ex` with
 * @code
 * ex << errinfo_file_format(to_string(f));
 * @endcode
 * where `f` is of type `FileFormat`. Get from a caught exception with
 * @code
 * FileFormat f = boost::get_error_info<errinfo_file_format>(ex)
 * @endcode
 *
 * Note, `ex` must be of sub type `boost::exception`, which holds for all imagefusion exceptions.
 *
 * @see exceptions.h
 *
 * \ingroup errinfo
 */
using errinfo_file_format = boost::error_info<struct tag_file_format, std::string>;


/**
 * @brief The FileFormat class is made to ask for image file formats like GTiff
 *
 * This class allows easy specification of the file format. It relies on
 * [GDALDriver](http://www.gdal.org/classGDALDriver.html) to find whether a format is supported on
 * a platform. This makes sense, since Image I/O also uses
 * [GDALDriver](http://www.gdal.org/classGDALDriver.html) and thus the information is consistent.
 *
 * So to just get the file format with the name specified in the column *Code* in
 * [this table](http://www.gdal.org/formats_list.html), just use the constructor:
 * @code
 * FileFormat f("GTiff");
 * @endcode
 * However, some formats might not be available on all platforms. In such a case `f` will be equal
 * to FileFormat::unsupported. You can also check this via
 * @code
 * FileFormat::isSupported("HDF4")
 * @endcode
 * which returns a `bool`.
 *
 * To get the format of an existing image file, use
 * @code
 * FileFormat f = FileFormat::fromFile("path/to/image.bin");
 * @endcode
 * This will probe the file and if GDAL can find an appropriate driver, `f` will correspond to it.
 * You can print the format code and the long name by
 * @code
 * std::cout << f << " (" << f.longName << ")" << std::endl;
 * // Example output: ENVI (ENVI .hdr Labelled)
 * @endcode
 * and there is also a `to_string(FileFormat const&)` method. To get the typical file extension as
 * string use
 * @code
 * f.fileExtension()
 * // or to get all
 * f.allFileExtensions()
 * @endcode
 * and to guess the format from a file extension, you can use
 * @code
 * FileFormat f = FileFormat::fromFileExtension(".png");
 * // or
 * FileFormat f = FileFormat::fromFileExtension("png");
 * @endcode
 * However, the file format does not map one-to-one to their file extensions and there are even
 * file formats that do not have a specific file extension. For example the file extension `.hdr`
 * is used by a lot of file formats and the binary ENVI format has no specific file extension in
 * the driver (although the long name tells one). So using the above function with `".hdr"` would
 * never give you the ENVI file format, but maybe (depending on internal driver order) the COASP
 * file format. So always prefer `FileFormat::fromFile()` over `FileFormat::fromFileExtension()` if
 * possible.
 *
 * Finally, to get all supported image file formats on a platform, use
 * @code
 * std::vector<FileFormat> formats = FileFormat::supportedFormats();
 * @endcode
 */
class FileFormat {
public:
    /**
     * @brief Construct a FileFormat directly from the GDAL driver code
     * @param fmtStr code for the driver. Beware case is considered here!
     *
     * Example: `FileFormat("GTiff")`
     *
     * Generally, supported formats are listed in
     * [this table](http://www.gdal.org/formats_list.html), but some formats are not supported on
     * every platform (like HDF4). To get all supported formats on a platform, see
     * supportedFormats().
     *
     * Note, if a requested format does not exist, the resulting object will be equal to
     * FileFormat::unsupported.
     */
    FileFormat(std::string const& fmtStr) : driverName(fmtStr) {
        if (!isSupported(fmtStr))
            driverName.clear(); // equal to FileFormat::unsupported
    }


    /**
     * @brief Get the driver default extension
     *
     * To be more precise: This returns the GDAL driver metadata item `GDAL_DMD_EXTENSION` (without
     * trailing 'S'). Note, for some file formats this is empty although `GDAL_DMD_EXTENSIONS` (see
     * allFileExtensions()) is not.
     *
     * @return default extension without dot (e. g. `"tif"` for `FileFormat("GTiff")`) or an empty
     * string if no extension is provided.
     */
    std::string fileExtension() const;


    /**
     * @brief Get all extensions the driver provides
     *
     * To get into some more detail: This is the driver meta data item `GDAL_DMD_EXTENSIONS`. If
     * this is empty fileExtension() will be returned (which might also be empty).
     *
     * @return all provided extensions without dot separated by space (e. g. `"tif tiff"` for
     * `FileFormat("GTiff")`) or an empty string if no extension is provided.
     */
    std::string allFileExtensions() const;


    /**
     * @brief Get the driver long name
     *
     * @return long name like (e. g. `"GeoTIFF"` for `FileFormat("GTiff")`) or an empty string if
     * no long name is provided.
     */
    std::string longName() const;


    /**
     * @brief This is returned, when a format is unsupported
     *
     * This or an eqivalent object is returned when a file format is not supported. So for example
     * the following statement would be true:
     * @code
     * FileFormat("bad format") == FileFormat::unsupported
     * @endcode
     * Its string representation is an empty string.
     */
    static const FileFormat unsupported;


    /**
     * @brief Check whether a file format is supported
     * @param fmtStr code for the driver. Beware case is considered here!
     * @return true if the format is supported by GDAL, false otherwise.
     */
    static bool isSupported(std::string const& fmtStr);


    /**
     * @brief Guess the file format from a file extension
     *
     * @param fileExt is a file extension with or without dot. So both `".bmp"` and `"bmp"` would
     * be fine.
     *
     * This iterates through all supported file formats and returns the first format of which one
     * of its extensions matches. So, if you try to get the format of an existing image file,
     * rather use fromFile().
     *
     * @return an appropriate file format or FileFormat::unsupported if no match can be found.
     */
    static FileFormat fromFileExtension(std::string fileExt);


    /**
     * @brief Probe image file to get the image file format
     * @param filename is the name of the image file
     *
     * This will get the image file format by the contents of the specified file.
     *
     * @return the corresponding image file format or FileFormat::unsupported, if no appropriate
     * driver could be found or the file does not exist.
     */
    static FileFormat fromFile(std::string filename);


    /**
     * @brief Get all supported file formats
     *
     * This can be used to get all supported file formats on a platform. You could print a table
     * with
     * @code
     * #include <iostream>
     * #include <iomanip>
     *
     * // ...
     *
     * auto all = FileFormat::supportedFormats();
     * for (auto f : all)
     *     std::cout << std::left << std::setw(16) << f
     *               << std::setw(25) << f.allFileExtensions() + " (" + f.fileExtension() + ") "
     *               << f.longName() << std::endl;
     * @endcode
     * @return all formats as vector
     */
    static std::vector<FileFormat> supportedFormats();


    /**
     * @brief Convert file format to its unique string represenation
     * @param f is the FileFormat object to convert
     *
     * @return its string code, which defines the format. This is the kind of string that is
     * expected in the constructor FileFormat(std::string const&)
     */
    friend std::string to_string(FileFormat const& f) {
        return f.driverName;
    }


    /**
     * @brief Equality comparison operator
     * @param l is the left FileFormat object
     * @param r is the right FileFormat object
     *
     * @return whether they represent the same format. This can also be used to compare with
     * FileFormat::unsupported.
     */
    friend bool operator==(FileFormat const& l, FileFormat const& r) {
        return l.driverName == r.driverName;
    }

private:
    std::string driverName;

    FileFormat() { }
};

inline std::ostream& operator<<(std::ostream& out, FileFormat const& f) {
    out << to_string(f);
    return out;
}

inline bool operator!=(FileFormat const& l, FileFormat const& r) {
    return !(l == r);
}

} /* namespace imagefusion */

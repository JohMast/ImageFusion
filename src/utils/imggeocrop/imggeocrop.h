#pragma once

#include <utility>
#include <array>
#include <limits>

#include <boost/range.hpp>
#include <boost/range/join.hpp>

#include "geoinfo.h"
#include "imagefusion.h"
#include "exceptions.h"
#include "image.h"
#include "optionparser.h"


// annonymous namespace is important for test configuration
// all functions are only defined for the corresponding test
namespace {



inline imagefusion::CoordRectangle parseCropRect(std::string inputArgument, bool projSpace, std::string const& optName = "") {
    using imagefusion::option::Descriptor;
    using imagefusion::option::OptionParser;
    using imagefusion::option::Parse;
    using imagefusion::option::ArgChecker;

    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";

    std::vector<Descriptor> usageImgProjCrop = Parse::usageImage;

    // replace pixel crop option name (-c, --crop) by (--crop-pix)
    auto crop_it = std::find_if(usageImgProjCrop.begin(), usageImgProjCrop.end(), [] (Descriptor const& d) {
            return d.spec == "CROP";
    });
    crop_it->shortopt.clear();
    crop_it->longopt = "crop-pix";

    // add projection crop option (--crop-proj)
    usageImgProjCrop.push_back(Descriptor{"CROPPROJ", "", "", "crop-proj", ArgChecker::CoordRectangle, ""});

    // parse and check for errors
    OptionParser cropOptions(usageImgProjCrop);
    cropOptions.parse(inputArgument);

    if (!cropOptions["CROP"].empty() && !cropOptions["CROPPROJ"].empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Please do not specify both --crop-pix and --crop-proj" + oN ));
    if (cropOptions["CROPPROJ"].size() > 1 || cropOptions["CROP"].size() > 1)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Multiple crop options for one image are not allowed" + oN ));
    Parse::Image(inputArgument, "", false, usageImgProjCrop);

    if (projSpace && !cropOptions["CROPPROJ"].empty())
        return Parse::CoordRectangle(cropOptions["CROPPROJ"].back().arg);
    if (!projSpace && !cropOptions["CROP"].empty())
        return Parse::CoordRectangle(cropOptions["CROP"].back().arg);
    return {};
}

inline imagefusion::option::ArgStatus argCheckImageProjCrop(imagefusion::option::Option const& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("There was no angle argument given for option '" + option.name + "'"));

    parseCropRect(option.arg, true, option.name);
    return imagefusion::option::ArgStatus::OK;
}

struct ProcessedGI {
    std::vector<imagefusion::GeoInfo> gis;
    imagefusion::GeoInfo targetGI;
    bool haveGI;
};

template <class Parse>
imagefusion::CoordRectangle parseUserCrop(std::string const& arg, imagefusion::GeoInfo gi) {
    imagefusion::CoordRectangle parsed = parseCropRect(arg, /*parse projection crop*/ false);
    if (parsed.area() != 0)
        return gi.geotrans.imgToProj(parsed & imagefusion::CoordRectangle(0, 0, gi.width(), gi.height()));

    parsed = parseCropRect(arg, /*parse projection crop*/ true);
    if (parsed.area() != 0)
        return parsed & gi.projRect();

    return {};
}

const char* usageCorner =
    "  --corner=<lat/long>, \tSpecifies one corner for cropping. Use this option once to specify the top left corner in combination with --center "
    "or --width and --height to define the extents. Or just use it exactly twice to specify opposing corners, which also defines the extents.\n";

const std::vector<imagefusion::option::Descriptor> usageGeoCoordRectangle {
imagefusion::option::Descriptor::text("Option usage: <georect> requires a combination of some of the following arguments:"),
{ "WIDTH",  "", "",  "width",  imagefusion::option::ArgChecker::Float,    "" },
{ "CENTER", "", "",  "center", imagefusion::option::ArgChecker::GeoCoord, "  --center=(<lat/long>) \tSpecifies the center location. To define the extents, specify either --width and --height or a --corner additionally." },
{ "CORNER", "", "",  "corner", imagefusion::option::ArgChecker::GeoCoord, usageCorner},
{ "HEIGHT", "", "h", "h",      imagefusion::option::ArgChecker::Float,    "  -h <num>, --height=<num> \tSpecifies the height in projection space unit (usually metre)." },
{ "WIDTH",  "", "w", "w",      imagefusion::option::ArgChecker::Float,    "  -w <num>, --width=<num>  \tSpecifies the width in projection space unit (usually metre)." },
{ "HEIGHT", "", "",  "height", imagefusion::option::ArgChecker::Float,    "" },
imagefusion::option::Descriptor::text("Examples: ... --<option>=(--corner=(0d 0' 0.01\"E, 50d 0' 0.00\"N) --corner=(13d 3'14.66\"E, 40d 0' 0.00\"N)) ... \n"
                                      "          ... --<option>=(--center=(7d 4' 15.84\"E, 45d N) -w 10000 -h 5000) ... \n"
                                      "          ... --<option>=(--corner=(0d 0' 0.01\"E, 50d 0' 0.00\"N) -w 10000 -h 5000) ... \n"
                                      "          ... --<option>=(--corner=(0d 0' 0.01\"E, 50d 0' 0.00\"N) --center=(7d 4' 15.84\"E, 45d N)) ... \n")
};


// return extents in projection space
// fitLongLatRect means, that a rectangle in long lat space, defined by longlatArg, fits inside the resulting rectangle in projection space
imagefusion::CoordRectangle parseAndConvertToProjSpace(std::string const& longlatArg, imagefusion::GeoInfo gi, bool fitLongLatRect = false) {
    using imagefusion::option::Parse;

    std::string optName = "--crop-longlat";
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    std::vector<std::string> argsTokens = imagefusion::option::separateArguments(longlatArg);
    imagefusion::option::OptionParser args(usageGeoCoordRectangle);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    try {
        args.parse(argsTokens);
    }
    catch (imagefusion::invalid_argument_error& e) {
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(e.what() + std::string(" when parsing geo extents") + oN));
    }

    if (args["CORNER"].size() > 2)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("More than two corners are not allowed, you specified " + std::to_string(args["CORNER"].size()) + ". Error when parsing geo extents" + oN));
    if ((args["CORNER"].size() == 2 && (!args["CENTER"].empty() || !args["WIDTH"].empty() || !args["HEIGHT"].empty())) ||
            (args["CORNER"].size() == 1 && !args["CENTER"].empty() && (!args["WIDTH"].empty() || !args["HEIGHT"].empty())))
    {
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Specify either two corners or one corner and center location or center location and width and height. Error when parsing geo extents" + oN));
    }
    if (args["WIDTH"].empty() != args["HEIGHT"].empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("When specifying width and height, both must be specified. You specified only " + std::string(!args["WIDTH"].empty() ? "width" : "height") + ". Error when parsing geo extents" + oN));
    if (args["CORNER"].size() == 1 && args["CENTER"].empty() && args["WIDTH"].empty() && args["HEIGHT"].empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("When specifying a single corner, either an opposing corner or the center location or width and height must be specified. You specified only a single corner. Error when parsing geo extents" + oN));
    if (args["CORNER"].empty() && !args["CENTER"].empty() && args["WIDTH"].empty() && args["HEIGHT"].empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("When specifying a center location, either a corner or the width and height must be specified. You specified only the center location. Error when parsing geo extents" + oN));
    if (args["CORNER"].empty() && args["CENTER"].empty() && !args["WIDTH"].empty() && !args["HEIGHT"].empty())
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("When specifying a width and height, either the north-west corner or the center location must be specified. You specified only the width and height. Error when parsing geo extents" + oN));


    imagefusion::CoordRectangle r;
    if (args["CORNER"].size() == 2) {
        // first case: 2 corners
        imagefusion::Coordinate c1 = Parse::GeoCoord(args["CORNER"].front().arg);
        imagefusion::Coordinate c2 = Parse::GeoCoord(args["CORNER"].back().arg);
        if (fitLongLatRect) {
            imagefusion::CoordRectangle longLatRect;
            longLatRect.x = std::min(c1.x, c2.x);
            longLatRect.y = std::min(c1.y, c2.y);
            longLatRect.width  = std::abs(c1.x - c2.x);
            longLatRect.height = std::abs(c1.y - c2.y);

            // collect source boundaries
            constexpr unsigned int numPoints = 33;
            std::vector<imagefusion::Coordinate> boundariesLongLat = imagefusion::detail::makeRectBoundaryCoords(longLatRect, numPoints);

            // transform to projection coordinate space and find outer extents
            std::vector<imagefusion::Coordinate> boundariesProj = gi.longLatToProj(boundariesLongLat);
            return imagefusion::detail::getRectFromBoundaryCoords(boundariesProj);
        }
        else {
            c1 = gi.longLatToProj(c1);
            c2 = gi.longLatToProj(c2);
            r.x = std::min(c1.x, c2.x);
            r.y = std::min(c1.y, c2.y);
            r.width  = std::abs(c1.x - c2.x);
            r.height = std::abs(c1.y - c2.y);
            return r;
        }
    }

    if (!args["WIDTH"].empty()) {
        r.width = Parse::Float(args["WIDTH"].back().arg);
        if (r.width <= 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("The width must be positive. Error when parsing geo extents" + oN));
    }
    if (!args["HEIGHT"].empty()) {
        r.height = Parse::Float(args["HEIGHT"].back().arg);
        if (r.height <= 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("The height must be positive. Error when parsing geo extents" + oN));
    }

    if (args["CORNER"].size() == 1) {
        imagefusion::Coordinate northWestLongLat = Parse::GeoCoord(args["CORNER"].front().arg);

        if (!args["CENTER"].empty()) {
            // second case: corner and center
            imagefusion::Coordinate center = Parse::GeoCoord(args["CENTER"].front().arg);
            if (fitLongLatRect) {
                imagefusion::Coordinate southEastLongLat = 2 * center - northWestLongLat;
                imagefusion::CoordRectangle longLatRect;
                longLatRect.x = std::min(northWestLongLat.x, southEastLongLat.x);
                longLatRect.y = std::min(northWestLongLat.y, southEastLongLat.y);
                longLatRect.width  = std::abs(northWestLongLat.x - southEastLongLat.x);
                longLatRect.height = std::abs(northWestLongLat.y - southEastLongLat.y);

                // collect source boundaries
                constexpr unsigned int numPoints = 33;
                std::vector<imagefusion::Coordinate> boundariesLongLat = imagefusion::detail::makeRectBoundaryCoords(longLatRect, numPoints);

                // transform to projection coordinate space and find outer extents
                std::vector<imagefusion::Coordinate> boundariesProj = gi.longLatToProj(boundariesLongLat);
                return imagefusion::detail::getRectFromBoundaryCoords(boundariesProj);
            }
            else {
                imagefusion::Coordinate northWestProj = gi.longLatToProj(northWestLongLat);
                center = gi.longLatToProj(center);

                imagefusion::Coordinate southEast = 2 * center - northWestProj;
                r.x = std::min(northWestProj.x, southEast.x);
                r.y = std::min(northWestProj.y, southEast.y);
                r.width  = std::abs(northWestProj.x - southEast.x);
                r.height = std::abs(northWestProj.y - southEast.y);
                return r;
            }
        }
        else { // no center specified
            // third case: corner and width and height
            assert(!args["WIDTH"].empty() && !args["HEIGHT"].empty());
            imagefusion::Coordinate northWestProj = gi.longLatToProj(northWestLongLat);
            imagefusion::Coordinate southEastProj = northWestProj;
            southEastProj += imagefusion::Coordinate(r.width, r.height);
            imagefusion::Coordinate southEastLongLat = gi.projToLongLat(southEastProj);

            if (southEastLongLat.x < northWestLongLat.x)
                southEastProj -= imagefusion::Coordinate(2 * r.width, 0);
            if (southEastLongLat.y > northWestLongLat.y)
                southEastProj -= imagefusion::Coordinate(0, 2 * r.height);

            r.x = std::min(northWestProj.x, southEastProj.x);
            r.y = std::min(northWestProj.y, southEastProj.y);
        }
    }
    else { // no corner specified
        // fourth case: center and width and height
        assert(!args["CENTER"].empty() && !args["WIDTH"].empty() && !args["HEIGHT"].empty());
        imagefusion::Coordinate center = Parse::GeoCoord(args["CENTER"].front().arg);
        center = gi.longLatToProj(center);

        imagefusion::Coordinate tl = center - imagefusion::Coordinate(r.width / 2, r.height / 2);
        r.x = tl.x;
        r.y = tl.y;
    }

    if (fitLongLatRect) {
        // collect boundaries in projection space
        constexpr unsigned int numPoints = 33;
        std::vector<imagefusion::Coordinate> boundariesProj = imagefusion::detail::makeRectBoundaryCoords(r, numPoints);

        // transform to longitude/latitude coordinate space and find outer extents
        std::vector<imagefusion::Coordinate> boundariesLongLat = gi.projToLongLat(boundariesProj);
        imagefusion::CoordRectangle longLatRect = imagefusion::detail::getRectFromBoundaryCoords(boundariesLongLat);

        // collect boundaries in longitude/latitude space
        boundariesLongLat = imagefusion::detail::makeRectBoundaryCoords(longLatRect, numPoints);

        // transform to projection coordinate space and find outer extents
        boundariesProj = gi.longLatToProj(boundariesLongLat);
        return imagefusion::detail::getRectFromBoundaryCoords(boundariesProj);
    }

    // else: simple mode
    return r;
}

template<class Parse>
ProcessedGI getAndProcessGeoInfo(std::vector<std::string> const& imgArgs,
                                 std::vector<std::string> const& geoimgArgs,
                                 std::vector<std::string> const& longLatArgs,
                                 std::vector<bool> const& longLatFull)
{
    using std::to_string;
    using imagefusion::to_string;

    ProcessedGI ret;
    if (imgArgs.empty())
        return ret;

    // read geoinfos and user specified crop windows
    std::vector<imagefusion::GeoInfo>& gis = ret.gis;
    std::vector<imagefusion::CoordRectangle> gisRects;
    ret.haveGI = true;
    for (auto const& arg : imgArgs) {
        std::string fn = Parse::ImageFileName(arg);

        // layers are important for container files (like HDF), since otherwise only the container metadata (without any real geo info) is available
        std::vector<int> layers = Parse::ImageLayers(arg);

        gis.emplace_back(fn, layers);
        if (!gis.back().hasGeotransform())
            ret.haveGI = false;
        gisRects.push_back(parseUserCrop<Parse>(arg, gis.back()));
    }

    if (!ret.haveGI)
        return ret;

    // check that the geotransform is simple (diagonal)
    for (auto const& gi : gis) {
        if (gi.geotrans.XToY != 0 || gi.geotrans.YToX != 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(
                    "The geotransformation of " + gi.filename + " is not simple enough. "
                    "Rotations, shearing, etc. is not supported currently, sorry! Only scaling is allowed."));
    }

    // get geoinfo with finest resolution as target (assuming all coordinates have the same unit)
    double minSqrDist = std::numeric_limits<double>::infinity();
    for (auto const& gi : gis) {
        auto zero = gi.geotrans.imgToProj({0, 0});
        auto one  = gi.geotrans.imgToProj({1, 1});
        imagefusion::Coordinate diff{zero.x - one.x, zero.y - one.y};
        double sqrDist = diff.x * diff.x + diff.y * diff.y;

        if (minSqrDist > sqrDist) {
            minSqrDist = sqrDist;
            ret.targetGI = gi;
        }
    }

    // intersect main images
    imagefusion::CoordRectangle targetRect = ret.targetGI.projRect();
    for (unsigned int i = 0; i < gis.size(); ++i) {
        targetRect = intersectRect(ret.targetGI, targetRect,
                                   gis.at(i), gisRects.at(i).area() > 0 ? gisRects.at(i) : gis.at(i).projRect());
        if (targetRect.area() == 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("After intersection with " + gis.at(i).filename + " the intersection is empty."));
    }

    // intersect latitude / longitude extents with target projection space coordinates
    assert(longLatArgs.size() == longLatFull.size() && "Vectors longLatArgs and longLatFull must have the same number of elements.");
    for (unsigned int i = 0; i < longLatArgs.size(); ++i) {
        imagefusion::CoordRectangle geoExt = parseAndConvertToProjSpace(longLatArgs.at(i), ret.targetGI, /*fitLongLatRect*/ longLatFull.at(i));
        targetRect &= geoExt;
        if (targetRect.area() == 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("After intersection with latitude/longitude argument " + longLatArgs.at(i) + " the intersection is empty."));
    }

    // get geo extents and crop windows from images that are only used for geo extents and intersect them
    for (auto const& arg : geoimgArgs) {
        std::string extfilename = Parse::ImageFileName(arg);

        // layers are important for container files (like HDF), since otherwise only the container metadata (without any real geo info) is available
        std::vector<int> extlayers = Parse::ImageLayers(arg);

        imagefusion::GeoInfo gi{extfilename, extlayers};
        if (!gi.hasGeotransform()) {
            std::cout << "Image " << extfilename << " does not have geo information and is just ignored." << std::endl;
            continue;
        }
        if (gi.geotrans.XToY != 0 || gi.geotrans.YToX != 0) {
            std::cout << "The geotransformation of " << extfilename << "  is not simple (diagonal matrix). It will just be ignored." << std::endl;
            continue;
        }
        if (gi.size.area() == 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("The image " + gi.filename + " seems to be empty."));

        imagefusion::CoordRectangle giRect = parseUserCrop<Parse>(arg, gi);
        targetRect = intersectRect(ret.targetGI, targetRect,
                                   gi, giRect.area() > 0 ? giRect : gi.projRect());
        if (targetRect.area() == 0)
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("After intersection with " + arg + " the intersection is empty."));
    }

    ret.targetGI.setExtents(targetRect);
    return ret;
}



} /* annonymous namespace */

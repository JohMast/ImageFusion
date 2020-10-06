#pragma once

#include "type.h"
#include <string>
#include <boost/exception/exception.hpp>
#include <boost/icl/interval_set.hpp>
#include <opencv2/opencv.hpp>

/** @file */

/**
 * @brief General imagefusion namespace
 *
 * All functionality of the image fusion library is included in this namespace.
 */
namespace imagefusion {


/**
 * @brief A simple integer rectangle
 *
 * It has the members `x`, `y`, `width`, `height`, which can be initialized with a constructor.
 * Look at the
 * [OpenCV documentation for Rect](http://docs.opencv.org/master/d2/d44/classcv_1_1Rect__.html)
 * with `_TP = int`.
 *
 * @see to_string(Rectangle const&), operator<<(std::ostream&, Rectangle const&)
 */
using Rectangle = cv::Rect;


/**
 * @brief A simple double rectangle
 *
 * It has the members `x`, `y`, `width`, `height`, which can be initialized with a constructor.
 * Look at the
 * [OpenCV documentation for Rect](http://docs.opencv.org/master/d2/d44/classcv_1_1Rect__.html)
 * with `_TP = double`.
 *
 * @see to_string(CoordRectangle const&), operator<<(std::ostream&, CoordRectangle const&)
 */
using CoordRectangle = cv::Rect_<double>;


/**
 * @brief A simple integer size
 *
 * It has the members `width`, `height`, which can be initialized with a constructor. Look at the
 * [OpenCV documentation for Size](http://docs.opencv.org/master/d6/d50/classcv_1_1Size__.html)
 * with `_TP = int`.
 *
 * @see to_string(Size const&), operator<<(std::ostream&, Size const&)
 */
using Size = cv::Size;


/**
 * @brief A simple double size
 *
 * It has the members `width`, `height`, which can be initialized with a constructor. Look at the
 * [OpenCV documentation for Size](http://docs.opencv.org/master/d6/d50/classcv_1_1Size__.html)
 * with `_TP = double`.
 *
 * @see to_string(Dimensions const&), operator<<(std::ostream&, Dimensions const&)
 */
using Dimensions = cv::Size_<double>;


/**
 * @brief A simple integer point
 *
 * It has the members `x`, `y`, which can be initialized with a constructor. Look at the
 * [OpenCV documentation for Point](http://docs.opencv.org/master/db/d4e/classcv_1_1Point__.html)
 * with `_TP = int`.
 *
 * @see to_string(Point const&), operator<<(std::ostream&, Point const&)
 */
using Point = cv::Point;


/**
 * @brief A simple double coordinate
 *
 * It has the members `x`, `y`, which can be initialized with a constructor. Look at the
 * [OpenCV documentation for Point](http://docs.opencv.org/master/db/d4e/classcv_1_1Point__.html)
 * with `_TP = double`.
 *
 * @see to_string(Coordinate const&), operator<<(std::ostream&, Coordinate const&)
 */
using Coordinate = cv::Point2d;


/**
 * @brief A double interval
 *
 * You can create an interval with
 * @code
 * Interval::open(double lower, double upper);       // (l, u)
 * Interval::left_open(double lower, double upper);  // (l, u]
 * Interval::right_open(double lower, double upper); // [l, u)
 * Interval::closed(double lower, double upper);     // [l, u]
 * @endcode
 * You can access the bounds with i.lower() and i.upper().
 *
 * When using an interval in a function, e. g. to create a mask, the values will saturate to the
 * actual image data range (see @ref Type). So, when lower is for example -infinity, and you make a
 * mask for an int16 image, -infinity will become -32768.
 *
 * An open interval used for an integer image, e. g. in createSingleChannelMaskFromRange(), will
 * correctly exclude the open bounds. For example using
 * @code
 * Interval range = Interval::left_open(3, 9); // (3,9]
 * @endcode
 * for an integer Image, means the range [4,9]. For floating point images, open bounds are
 * currently ignored and considered as closed bounds.
 *
 * If you need to get to know which bound is open use either
 * @code
 * bool leftopen  = range.bounds().left()  == boost::icl::interval_bounds::open();
 * bool rightopen = range.bounds().right() == boost::icl::interval_bounds::open();
 * @endcode
 * (do not check for left or right closed!) or one of:
 * @code
 * range.bounds().all() == boost::icl::interval_bounds::open()
 * range.bounds().all() == boost::icl::interval_bounds::left_open()
 * range.bounds().all() == boost::icl::interval_bounds::right_open()
 * range.bounds().all() == boost::icl::interval_bounds::closed()
 * @endcode
 *
 * For more information, see the official
 * [documentation](http://www.boost.org/doc/libs/release/libs/icl/doc/html/index.html).
 */
using Interval = boost::icl::interval<double>::type;


/**
 * @brief A set of Interval%s
 *
 * This defines a set of intervals and allows to add an interval (set union)
 * @code
 * interSet += inter;
 * @endcode
 * to remove an interval (set difference)
 * @code
 * interSet -= inter;
 * @endcode
 * and intersection
 * @code
 * interSet &= inter;
 * @endcode
 * To flip the intervals in the set on an interval use:
 * @code
 * interSet ^= inter;
 * @endcode
 *
 * You can also loop through all intervals with
 * @code
 * for (auto const& i : interSet)
 *     ...
 * @endcode
 *
 * For more information, see the official
 * [documentation](http://www.boost.org/doc/libs/release/libs/icl/doc/html/index.html).
 */
using IntervalSet = boost::icl::interval_set<double>;

/**
 * @brief Make an IntervalSet appropriate for comparing bounds with integer values
 * @param is is an interval set that can contain all values including infinity
 *
 * This function will convert the interval set such that is consists of closed intervals only and
 * that no bound exceeds the range of int32_t. This allows to use the interval bounds in comparison
 * with integer values. Example:
 * @code
 * IntervalSet dis = discretizeSet(cont);
 * for (Interval const& i : dis) {
 *     int low  = static_cast<int>(i.lower());
 *     int high = static_cast<int>(i.upper());
 *     ...
 * }
 * @endcode
 *
 * @return an interval set with only closed intervals, which bounds are representable as integers.
 */
inline IntervalSet discretizeBounds(IntervalSet is) {
    // limit to int32_t range
    is &= Interval::closed(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());

    // discretize set
    boost::icl::interval_set<int32_t> is_int;
    for (Interval const& i : is) {
        double l = i.lower();
        double u = i.upper();
        if (i.bounds().left() != boost::icl::interval_bounds::open())
            l = std::ceil(l);
        if (i.bounds().right() == boost::icl::interval_bounds::open())
            u = std::ceil(u);
        is_int += boost::icl::interval<int32_t>::type(std::floor(l), std::floor(u), i.bounds());
    }

    // write discretized set to a double set with closed intervals
    IntervalSet is_new;
    for (auto const& i : is_int)
        is_new += Interval::closed(first(i), last(i));
    return is_new;
}

/**
 * @brief Convert @ref Size to string
 * @param s is the size.
 * @return string in the format "w:W x h:H", where W is `s.width` and H is `s.height`.
 */
inline std::string to_string(Size const& s) {
    return "w:" + std::to_string(s.width) + " x h:" + std::to_string(s.height);
}


/**
 * @brief Convert @ref Dimensions to string
 * @param d are the dimensions.
 * @return string in the format "w:W x h:H", where W is `s.width` and H is `s.height`
 */
inline std::string to_string(Dimensions const& d) {
    return "w:" + std::to_string(d.width) + " x h:" + std::to_string(d.height);
}


/**
 * @brief Convert @ref Point to string
 * @param p is the point.
 * @return string in the format "(x, y)".
 */
inline std::string to_string(Point const& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
}


/**
 * @brief Convert @ref Coordinate to string
 * @param c is the coordinate.
 * @return string in the format "(x, y)".
 */
inline std::string to_string(Coordinate const& c) {
    return "(" + std::to_string(c.x) + ", " + std::to_string(c.y) + ")";
}


/**
 * @brief Convert @ref Rectangle to string
 * @param r is the rectangle.
 * @return string in the format "(x, y); w:W x h:H", where W is `s.width` and H is `s.height`.
 */
inline std::string to_string(Rectangle const& r) {
    return to_string(Point{r.x, r.y}) + "; " + to_string(Size{r.width, r.height});
}


/**
 * @brief Convert @ref CoordRectangle to string
 * @param r is the rectangle.
 * @return string in the format "(x, y); w:W x h:H", where W is `s.width` and H is `s.height`.
 */
inline std::string to_string(CoordRectangle const& r) {
    return to_string(Coordinate{r.x, r.y}) + "; " + to_string(Dimensions{r.width, r.height});
}


/**
 * @brief Convenience operator `<<` for stream output
 * @param out is the output stream.
 * @param s is the size, which will be converted to a string and output on `out`.
 * @return `out` to append multiple outputs.
 * @see to_string(Size const&)
 */
inline std::ostream& operator<<(std::ostream& out, Size const& s) {
    out << to_string(s);
    return out;
}


/**
 * @brief Convenience operator `<<` for stream output
 * @param out is the output stream.
 * @param d is the dimensions, which will be converted to a string and output on `out`.
 * @return `out` to append multiple outputs.
 * @see to_string(Dimensions const&)
 */
inline std::ostream& operator<<(std::ostream& out, Dimensions const& d) {
    out << to_string(d);
    return out;
}


/**
 * @brief Convenience operator `<<` for stream output
 * @param out is the output stream.
 * @param p is the point, which will be converted to a string and output on `out`.
 * @return `out` to append multiple outputs.
 * @see to_string(Point const&)
 */
inline std::ostream& operator<<(std::ostream& out, Point const& p) {
    out << to_string(p);
    return out;
}


/**
 * @brief Convenience operator `<<` for stream output
 * @param out is the output stream.
 * @param c is the coordinate, which will be converted to a string and output on `out`.
 * @return `out` to append multiple outputs.
 * @see to_string(Coordinate const&)
 */
inline std::ostream& operator<<(std::ostream& out, Coordinate const& c) {
    out << to_string(c);
    return out;
}


/**
 * @brief Convenience operator `<<` for stream output
 * @param out is the output stream.
 * @param r is the rectangle, which will be converted to a string and output on `out`.
 * @return `out` to append multiple outputs.
 * @see to_string(Rectangle const&)
 */
inline std::ostream& operator<<(std::ostream& out, Rectangle const& r) {
    out << to_string(r);
    return out;
}


/**
 * @brief Convenience operator `<<` for stream output
 * @param out is the output stream.
 * @param r is the rectangle, which will be converted to a string and output on `out`.
 * @return `out` to append multiple outputs.
 * @see to_string(CoordRectangle const&)
 */
inline std::ostream& operator<<(std::ostream& out, CoordRectangle const& r) {
    out << to_string(r);
    return out;
}


/**
 * @brief Error information for the image size
 *
 * Add to an exception `ex` with
 * @code
 * ex << errinfo_size(s);
 * @endcode
 * where `s` is of type `Size`. Get from a caught exception
 * with
 * @code
 * Size const* t = boost::get_error_info<errinfo_size>(ex)
 * @endcode
 *
 * Note, `ex` must be of sub type `boost::exception`, which holds for all imagefusion exceptions.
 *
 * \ingroup errinfo
 *
 * @see exceptions.h
 */
using errinfo_size = boost::error_info<struct tag_size, Size>;


/**
  * @brief Interpolation method for resampling
  */
enum class InterpMethod : uint8_t {
    nearest,    ///< constant interpolation with nearest neighbor
    bilinear,   ///< bilinear interpolation
    cubic,      ///< cubic interpolation
    cubicspline ///< cubic spline interpolation
};

} /* namespace imagefusion */

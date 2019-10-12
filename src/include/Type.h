#pragma once

#include <cstdint>
#include <type_traits>
#include <array>

#include <gdal.h>
#include <opencv2/opencv.hpp>
#include <boost/exception/error_info.hpp>

#include "exceptions.h"

namespace imagefusion {


/**
 * \defgroup typegrp Dynamic type system
 *
 * @brief Type system that determines an image type at runtime (like in OpenCV)
 *
 * The @ref Type is a property of an Image. It specifies the base type of a pixel value, like 8 bit
 * unsigned integer or 32 bit floating point, and the number of channels of a pixel. The @ref Type
 * is specified at runtime to allow reading an image from a file, without knowing its type
 * beforehand. It also makes handling of Image%s easier, since Image could be implemented as a
 * usual class and not as class template.
 *
 * For image-wise operations you don't need to know the @ref Type. This will be handled by the
 * underlying OpenCV Matrix (`cv::Mat`). However, if you want to access a single pixel or the value
 * of a specific channel value in a pixel, you are required to use a real data type, like `float`
 * not just a runtime value representing it like Type::float32. So basically you have to execute
 * code, where a data type depending on the runtime type value is used. In principle this could be
 * done by using an if-else cascade, like
 * @code
 * if (img.basetype() == Type::float32) {
 *     float f = img.at<float>(x, y, c);
 *     ...
 * }
 * else if (img.basetype() == ...) {
 *     ...
 * }
 * else if ...
 * @endcode
 * This would be tedious. Before going on, note that there are the methods @ref Image::doubleAt and
 * @ref Image::setValueAt which do exactly that. However, since this is a common task, there are
 * template metaprogramming facilities provided to avoid an explicit if-else cascade. In most cases
 * you can specify a functor (as described [here](@ref
 * CallBaseTypeFunctorRestrictBaseTypesTo::run)) and use @ref CallBaseTypeFunctor, optionally with
 * specified possible types.
 *
 * In some situations though, you might feel to need a Type-dependent template class. Often the
 * feeling is wrong and you should rather isolate the type-dependent code into a functor, like
 * described above. But if you really need an instance, you can make a simple factory. Say you have
 * a class template of a data fusor, which needs the image type as compile time information at
 * construction. The data fusor does not even need to have a default constructor.
 * @code
 * template<Type t>
 * struct ExampleDataFusor : public DataFusor {
 *     // type restriction
 *     using basetype = typename DataType<getBaseType(t)>::base_type;
 *     static_assert(std::is_unsigned<basetype>::value, "ExampleDataFusor needs an unsigned type.");
 *
 *     // non-default constructor
 *     ExampleDataFusor(Image& i);
 *
 *     // remaining implementation without default constructor
 * };
 *
 * struct RestrictedFactory {
 *     Image& img;
 *     RestrictedFactory(Image& i) : img{i} { }
 *
 *     template<Type t>
 *     std::unique_ptr<DataFusor> operator()() const {
 *         return std::unique_ptr<DataFusor>(new ExampleDataFusor<t>{img});
 *     }
 *
 *     static std::unique_ptr<DataFusor> create(Image& i) {
 *         return CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8, Type::uint16>::run(RestrictedFactory{i}, i.type());
 *     }
 * };
 * @endcode
 * This would be called with an `Image` `img`:
 * @code
 * auto fusor = RestrictedFactory::create(img);
 * @endcode
 * Hereby, `auto` is `std::unique_ptr<DataFusor>`. If it is not a DataFusor, make sure to have a
 * super class, that does not require a @ref Type template argument.
 *
 * However, note that for the Parallelizer non of the above would be suitable, since it requires a
 * copy constructible DataFusor. In addition to the factory, you could implement a Proxy for the
 * DataFusor to have a copy constructible meta DataFusor. The better solution is to avoid type
 * dependent DataFusor templates completely, if possible. One alternative would be to put the type
 * dependent processing into a functor and call it in predict, as shown in the documentation of
 * Parallelizer. Another alternative would be to use a non-template DataFusor class that owns an
 * object via a pointer to the base class of a type dependent template class, which does the type
 * dependent processing. This could be used if a lot of preprocessing is required to do the
 * processing. Then the preprocessed result could be shared across copied DataFusor%s. The latter
 * pattern is quite advanced, while the one described in Parallelizer is easy to implement, if
 * possible.
 *
 * There are a lot of other supporting functions, a [traits class](@ref DataType) and also an @ref
 * errinfo type wrapping a Type. The @ref errinfo type is not described in this module because of
 * technical reasons.
 */


/**
 * @brief The Type enum defines the runtime type values
 *
 * The values are structured. They have a base type order from int8 (0) to float64 (6), which are
 * also the values for single channel. Adding a channel increments the value by type_channel_diff.
 * However, if you are interested in properties of the type, you can use various supporting
 * functions or the traits class rather than using the converted values explicitly. The base type
 * order might even change in future versions. The converted enum-values themselves are used to
 * implement the supporting functions.
 *
 * There are two concepts used with this @ref Type enum, which you have to distinguish:
 * - *base type* for the base data type of a channel value, like Type::int8 or Type::float32.
 * - *full type* for the type that specifies base type and the number of channels, like
 *   Type::int16x3 or Type::int16x1.
 *
 * Note, the base types have the same values as their single-channel analogues, e. g. Type::int16
 * has the same representation as Type::int16x1, still they belong to different semantic concepts.
 * This is similar as in the OpenCV type system.
 *
 * There is also a Type::invalid to mark an invalid type. Do not use Type::invalid for anything but
 * comparison.
 *
 * @see getChannels(Type full), getBaseType(Type full), getFullType(Type base, uint8_t channels),
 * isIntegerType(Type t), DataType, toFullType(int ocvt), toCVType(Type t),
 * toBaseType(GDALDataType t), toGDALDepth(Type t)
 *
 * \ingroup typegrp
 */
enum class Type : uint8_t {
    int8       = 0,   ///< base type, signed 8 bit, [-128, 127]
    uint8      = 1,   ///< base type, unsigned 8 bit, [0, 255]
    int16      = 2,   ///< base type, signed 16 bit, [-32768, 32767]
    uint16     = 3,   ///< base type, unsigned 16 bit, [0, 65535]
    int32      = 4,   ///< base type, signed 32 bit, [-2147483648, 2147483647]
    float32    = 5,   ///< base type, 32 bit floating point [0, 1]
    float64    = 6,   ///< base type, 64 bit floating point [0, 1]
    int8x1     = 0,   ///< full type with 1 channel  and base type int8
    uint8x1    = 1,   ///< full type with 1 channel  and base type uint8
    int16x1    = 2,   ///< full type with 1 channel  and base type int16
    uint16x1   = 3,   ///< full type with 1 channel  and base type uint16
    int32x1    = 4,   ///< full type with 1 channel  and base type int32
    float32x1  = 5,   ///< full type with 1 channel  and base type float32
    float64x1  = 6,   ///< full type with 1 channel  and base type float64
    int8x2     = 10,  ///< full type with 2 channels and base type int8
    uint8x2    = 11,  ///< full type with 2 channels and base type uint8
    int16x2    = 12,  ///< full type with 2 channels and base type int16
    uint16x2   = 13,  ///< full type with 2 channels and base type uint16
    int32x2    = 14,  ///< full type with 2 channels and base type int32
    float32x2  = 15,  ///< full type with 2 channels and base type float32
    float64x2  = 16,  ///< full type with 2 channels and base type float64
    int8x3     = 20,  ///< full type with 3 channels and base type int8
    uint8x3    = 21,  ///< full type with 3 channels and base type uint8
    int16x3    = 22,  ///< full type with 3 channels and base type int16
    uint16x3   = 23,  ///< full type with 3 channels and base type uint16
    int32x3    = 24,  ///< full type with 3 channels and base type int32
    float32x3  = 25,  ///< full type with 3 channels and base type float32
    float64x3  = 26,  ///< full type with 3 channels and base type float64
    int8x4     = 30,  ///< full type with 4 channels and base type int8
    uint8x4    = 31,  ///< full type with 4 channels and base type uint8
    int16x4    = 32,  ///< full type with 4 channels and base type int16
    uint16x4   = 33,  ///< full type with 4 channels and base type uint16
    int32x4    = 34,  ///< full type with 4 channels and base type int32
    float32x4  = 35,  ///< full type with 4 channels and base type float32
    float64x4  = 36,  ///< full type with 4 channels and base type float64
    int8x5     = 40,  ///< full type with 5 channels and base type int8
    uint8x5    = 41,  ///< full type with 5 channels and base type uint8
    int16x5    = 42,  ///< full type with 5 channels and base type int16
    uint16x5   = 43,  ///< full type with 5 channels and base type uint16
    int32x5    = 44,  ///< full type with 5 channels and base type int32
    float32x5  = 45,  ///< full type with 5 channels and base type float32
    float64x5  = 46,  ///< full type with 5 channels and base type float64
    int8x6     = 50,  ///< full type with 6 channels and base type int8
    uint8x6    = 51,  ///< full type with 6 channels and base type uint8
    int16x6    = 52,  ///< full type with 6 channels and base type int16
    uint16x6   = 53,  ///< full type with 6 channels and base type uint16
    int32x6    = 54,  ///< full type with 6 channels and base type int32
    float32x6  = 55,  ///< full type with 6 channels and base type float32
    float64x6  = 56,  ///< full type with 6 channels and base type float64
    int8x7     = 60,  ///< full type with 7 channels and base type int8
    uint8x7    = 61,  ///< full type with 7 channels and base type uint8
    int16x7    = 62,  ///< full type with 7 channels and base type int16
    uint16x7   = 63,  ///< full type with 7 channels and base type uint16
    int32x7    = 64,  ///< full type with 7 channels and base type int32
    float32x7  = 65,  ///< full type with 7 channels and base type float32
    float64x7  = 66,  ///< full type with 7 channels and base type float64
    int8x8     = 70,  ///< full type with 8 channels and base type int8
    uint8x8    = 71,  ///< full type with 8 channels and base type uint8
    int16x8    = 72,  ///< full type with 8 channels and base type int16
    uint16x8   = 73,  ///< full type with 8 channels and base type uint16
    int32x8    = 74,  ///< full type with 8 channels and base type int32
    float32x8  = 75,  ///< full type with 8 channels and base type float32
    float64x8  = 76,  ///< full type with 8 channels and base type float64
    int8x9     = 80,  ///< full type with 9 channels and base type int8
    uint8x9    = 81,  ///< full type with 9 channels and base type uint8
    int16x9    = 82,  ///< full type with 9 channels and base type int16
    uint16x9   = 83,  ///< full type with 9 channels and base type uint16
    int32x9    = 84,  ///< full type with 9 channels and base type int32
    float32x9  = 85,  ///< full type with 9 channels and base type float32
    float64x9  = 86,  ///< full type with 9 channels and base type float64
    int8x10    = 90,  ///< full type with 10 channels and base type int8
    uint8x10   = 91,  ///< full type with 10 channels and base type uint8
    int16x10   = 92,  ///< full type with 10 channels and base type int16
    uint16x10  = 93,  ///< full type with 10 channels and base type uint16
    int32x10   = 94,  ///< full type with 10 channels and base type int32
    float32x10 = 95,  ///< full type with 10 channels and base type float32
    float64x10 = 96,  ///< full type with 10 channels and base type float64
    int8x11    = 100, ///< full type with 11 channels and base type int8
    uint8x11   = 101, ///< full type with 11 channels and base type uint8
    int16x11   = 102, ///< full type with 11 channels and base type int16
    uint16x11  = 103, ///< full type with 11 channels and base type uint16
    int32x11   = 104, ///< full type with 11 channels and base type int32
    float32x11 = 105, ///< full type with 11 channels and base type float32
    float64x11 = 106, ///< full type with 11 channels and base type float64
    int8x12    = 110, ///< full type with 12 channels and base type int8
    uint8x12   = 111, ///< full type with 12 channels and base type uint8
    int16x12   = 112, ///< full type with 12 channels and base type int16
    uint16x12  = 113, ///< full type with 12 channels and base type uint16
    int32x12   = 114, ///< full type with 12 channels and base type int32
    float32x12 = 115, ///< full type with 12 channels and base type float32
    float64x12 = 116, ///< full type with 12 channels and base type float64
    int8x13    = 120, ///< full type with 13 channels and base type int8
    uint8x13   = 121, ///< full type with 13 channels and base type uint8
    int16x13   = 122, ///< full type with 13 channels and base type int16
    uint16x13  = 123, ///< full type with 13 channels and base type uint16
    int32x13   = 124, ///< full type with 13 channels and base type int32
    float32x13 = 125, ///< full type with 13 channels and base type float32
    float64x13 = 126, ///< full type with 13 channels and base type float64
    int8x14    = 130, ///< full type with 14 channels and base type int8
    uint8x14   = 131, ///< full type with 14 channels and base type uint8
    int16x14   = 132, ///< full type with 14 channels and base type int16
    uint16x14  = 133, ///< full type with 14 channels and base type uint16
    int32x14   = 134, ///< full type with 14 channels and base type int32
    float32x14 = 135, ///< full type with 14 channels and base type float32
    float64x14 = 136, ///< full type with 14 channels and base type float64
    int8x15    = 140, ///< full type with 15 channels and base type int8
    uint8x15   = 141, ///< full type with 15 channels and base type uint8
    int16x15   = 142, ///< full type with 15 channels and base type int16
    uint16x15  = 143, ///< full type with 15 channels and base type uint16
    int32x15   = 144, ///< full type with 15 channels and base type int32
    float32x15 = 145, ///< full type with 15 channels and base type float32
    float64x15 = 146, ///< full type with 15 channels and base type float64
    int8x16    = 150, ///< full type with 16 channels and base type int8
    uint8x16   = 151, ///< full type with 16 channels and base type uint8
    int16x16   = 152, ///< full type with 16 channels and base type int16
    uint16x16  = 153, ///< full type with 16 channels and base type uint16
    int32x16   = 154, ///< full type with 16 channels and base type int32
    float32x16 = 155, ///< full type with 16 channels and base type float32
    float64x16 = 156, ///< full type with 16 channels and base type float64
    int8x17    = 160, ///< full type with 17 channels and base type int8
    uint8x17   = 161, ///< full type with 17 channels and base type uint8
    int16x17   = 162, ///< full type with 17 channels and base type int16
    uint16x17  = 163, ///< full type with 17 channels and base type uint16
    int32x17   = 164, ///< full type with 17 channels and base type int32
    float32x17 = 165, ///< full type with 17 channels and base type float32
    float64x17 = 166, ///< full type with 17 channels and base type float64
    int8x18    = 170, ///< full type with 18 channels and base type int8
    uint8x18   = 171, ///< full type with 18 channels and base type uint8
    int16x18   = 172, ///< full type with 18 channels and base type int16
    uint16x18  = 173, ///< full type with 18 channels and base type uint16
    int32x18   = 174, ///< full type with 18 channels and base type int32
    float32x18 = 175, ///< full type with 18 channels and base type float32
    float64x18 = 176, ///< full type with 18 channels and base type float64
    int8x19    = 180, ///< full type with 19 channels and base type int8
    uint8x19   = 181, ///< full type with 19 channels and base type uint8
    int16x19   = 182, ///< full type with 19 channels and base type int16
    uint16x19  = 183, ///< full type with 19 channels and base type uint16
    int32x19   = 184, ///< full type with 19 channels and base type int32
    float32x19 = 185, ///< full type with 19 channels and base type float32
    float64x19 = 186, ///< full type with 19 channels and base type float64
    int8x20    = 190, ///< full type with 20 channels and base type int8
    uint8x20   = 191, ///< full type with 20 channels and base type uint8
    int16x20   = 192, ///< full type with 20 channels and base type int16
    uint16x20  = 193, ///< full type with 20 channels and base type uint16
    int32x20   = 194, ///< full type with 20 channels and base type int32
    float32x20 = 195, ///< full type with 20 channels and base type float32
    float64x20 = 196, ///< full type with 20 channels and base type float64
    int8x21    = 200, ///< full type with 21 channels and base type int8
    uint8x21   = 201, ///< full type with 21 channels and base type uint8
    int16x21   = 202, ///< full type with 21 channels and base type int16
    uint16x21  = 203, ///< full type with 21 channels and base type uint16
    int32x21   = 204, ///< full type with 21 channels and base type int32
    float32x21 = 205, ///< full type with 21 channels and base type float32
    float64x21 = 206, ///< full type with 21 channels and base type float64
    int8x22    = 210, ///< full type with 22 channels and base type int8
    uint8x22   = 211, ///< full type with 22 channels and base type uint8
    int16x22   = 212, ///< full type with 22 channels and base type int16
    uint16x22  = 213, ///< full type with 22 channels and base type uint16
    int32x22   = 214, ///< full type with 22 channels and base type int32
    float32x22 = 215, ///< full type with 22 channels and base type float32
    float64x22 = 216, ///< full type with 22 channels and base type float64
    int8x23    = 220, ///< full type with 23 channels and base type int8
    uint8x23   = 221, ///< full type with 23 channels and base type uint8
    int16x23   = 222, ///< full type with 23 channels and base type int16
    uint16x23  = 223, ///< full type with 23 channels and base type uint16
    int32x23   = 224, ///< full type with 23 channels and base type int32
    float32x23 = 225, ///< full type with 23 channels and base type float32
    float64x23 = 226, ///< full type with 23 channels and base type float64
    int8x24    = 230, ///< full type with 24 channels and base type int8
    uint8x24   = 231, ///< full type with 24 channels and base type uint8
    int16x24   = 232, ///< full type with 24 channels and base type int16
    uint16x24  = 233, ///< full type with 24 channels and base type uint16
    int32x24   = 234, ///< full type with 24 channels and base type int32
    float32x24 = 235, ///< full type with 24 channels and base type float32
    float64x24 = 236, ///< full type with 24 channels and base type float64
    int8x25    = 240, ///< full type with 25 channels and base type int8
    uint8x25   = 241, ///< full type with 25 channels and base type uint8
    int16x25   = 242, ///< full type with 25 channels and base type int16
    uint16x25  = 243, ///< full type with 25 channels and base type uint16
    int32x25   = 244, ///< full type with 25 channels and base type int32
    float32x25 = 245, ///< full type with 25 channels and base type float32
    float64x25 = 246, ///< full type with 25 channels and base type float64
    invalid   = 255 ///< invalid type
};


/**
 * @brief Error information for the image type
 *
 * Add to an exception `ex` with
 * @code
 * ex << errinfo_image_type(t);
 * @endcode
 *
 * where `t` is of type `Type`. Get from a caught exception with
 * @code
 * Type const* t = boost::get_error_info<errinfo_image_type>(ex)
 * @endcode
 *
 * Note, `ex` must be of sub type `boost::exception`, which holds for all imagefusion exceptions.
 *
 * \ingroup errinfo
 *
 * @see exceptions.h
 */
using errinfo_image_type = boost::error_info<struct tag_image_type, Type>;


/**
 * @brief Channel enum-value difference in the Type enum
 *
 * This is the difference of the enum-values between two Type%s, which only differ in the number of
 * channels by one. It is implemented as the difference between Type::int8x2 and Type::int8x1,
 * which is 10.
 *
 * \ingroup typegrp
 */
constexpr uint8_t type_channel_diff = static_cast<uint8_t>(Type::int8x2) - static_cast<uint8_t>(Type::int8x1);


std::string to_string(Type t);
constexpr unsigned int getChannels(Type t);

/**
 * @brief Make a full type from a base type with a number of channels
 *
 * @param base is the base type or single-channel type, like Type::uint8 or Type::float64x1, but
 * not a multi-channel type, like Type::int16x2.
 *
 * @param channels is the number of channels &le; 25
 *
 * @return full type, e. g. for `getFullType(Type::uint16, 3)` this returns Type::uint16x3. In case
 * of `channels` > 25 or `base` beeing a full type an `image_type_error` will be thrown.
 *
 * Note, this is a constexpr function and can be used at both compile time and runtime.
 *
 * @see getBaseType() to get the base type from a full type. Also see Image::basetype().
 *
 * \ingroup typegrp
 *
 * @throws image_type_error if channels > 25 or getChannels(base) != 1.
 */
inline constexpr Type getFullType(Type base, uint8_t channels) {
    return channels <= 25 && getChannels(base) == 1
            ? static_cast<Type>(static_cast<uint8_t>(base) + type_channel_diff * (channels-1))
            : (IF_THROW_EXCEPTION(image_type_error(channels > 25 ? "Too many channels (only 25 are supported): " + std::to_string(channels)
                                                                : to_string(base) + " is not a base type, since it has more than one channel. Use getBaseType() for the first argument of getFullType()."))
               << errinfo_image_type(base));
}


/**
 * @brief Get the number of channels from a full type
 * @param full is the full type, like Type::uint8x2.
 * @return the number of channels, e. g. for Type::uint8x2 it would return 2.
 *
 * Note, this is a constexpr function and can be used at both compile time and runtime.
 *
 * \ingroup typegrp
 */
inline constexpr unsigned int getChannels(Type full) {
    return static_cast<uint8_t>(full) / type_channel_diff + 1;
}


/**
 * @brief Get base type from a full type
 * @param full is the full type, like Type::uint8x2.
 *
 * @return the base type corresponding to the given full type, e. g. for Type::uint8x2 it would
 * return Type::uint8.
 *
 * Note, this is a constexpr function and can be used at both compile time and runtime.
 *
 * @see getFullType() for making a type with a specified number of channels.
 * \ingroup typegrp
 */
inline constexpr Type getBaseType(Type full) {
    return full == Type::invalid ? full
                                 : static_cast<Type>(static_cast<uint8_t>(full) % type_channel_diff);
}


/**
 * @brief Get the minimum value of the image data range
 * @param t is the base or full type, like Type::int16 or Type::uint8x2.
 * @return the minimum value in the data range converted to double.
 *
 * The minimum value is for integer types just the numeric limit, like -32768 for Type::int16, but
 * for floating point types it is 0.
 *
 * @remark The value for black is always 0, also for signed integer types!
 *
 * Note, this is a constexpr function and can be used at both compile time and runtime.
 *
 * @see getImageRangeMax(), DataType<Type>::min, TypeTraits<typename>::min
 * \ingroup typegrp
 */
inline constexpr double getImageRangeMin(Type t) {
    return static_cast<double>(
                getBaseType(t) == Type::int8  ? std::numeric_limits<int8_t>::min()
             : (getBaseType(t) == Type::int16 ? std::numeric_limits<int16_t>::min()
             : (getBaseType(t) == Type::int32 ? std::numeric_limits<int32_t>::min()
             : 0)));
}


/**
 * @brief Get the maximum value of the image data range
 * @param t is the base or full type, like Type::int16 or Type::uint8x2.
 * @return the maximum value in the data range converted to double.
 *
 * The maximum value is for integer types just the numeric limit, like 32767 for Type::int16, but
 * for floating point types it is 1.
 *
 * Note, this is a constexpr function and can be used at both compile time and runtime.
 *
 * @see getImageRangeMin(), DataType<Type>::max, TypeTraits<typename>::max
 * \ingroup typegrp
 */
inline constexpr double getImageRangeMax(Type t) {
    return static_cast<double>(
                getBaseType(t) == Type::int8    ? std::numeric_limits<int8_t>::max()
             : (getBaseType(t) == Type::uint8   ? std::numeric_limits<uint8_t>::max()
             : (getBaseType(t) == Type::int16   ? std::numeric_limits<int16_t>::max()
             : (getBaseType(t) == Type::uint16  ? std::numeric_limits<uint16_t>::max()
             : (getBaseType(t) == Type::int32   ? std::numeric_limits<int32_t>::max()
             : 1)))));
}


/**
 * @brief Get a type where the result of an operation fits in
 * @param t is the full type, like Type::uint8x2.
 *
 * @return the next larger signed type for integer types (int32_t is the largest supported integer
 * type) and it returns the same type for floating point types, e. g. for Type::uint8x2 it would
 * return Type::int16x2, for Type::int32x1 it would return Type::int32x1 and for Type::float32x3 it
 * would return Type::float32x3.
 *
 * Note, this is a constexpr function and can be used at both compile time and runtime.
 *
 * \ingroup typegrp
 */
inline constexpr Type getResultType(Type t) {
    return static_cast<uint8_t>(getBaseType(t)) >= static_cast<uint8_t>(Type::int32)
              ? t // int32, float32, float64
              : ( getBaseType(t) == Type::int8 || getBaseType(t) == Type::uint8
                  ? getFullType(Type::int16, getChannels(t))   // int8,  uint8
                  : getFullType(Type::int32, getChannels(t))); // int16, uint16
}


/**
 * @brief Convert a type to a string
 * @param t is the full or base type.
 *
 * @return a string like the enum name of the value. For single-channel types the base type is
 * preferred. Examples: `to_string(Type::uint16x4)` will give `"uint16x4"`, but
 * `to_string(Type::uint16x1)` will give `"uint16"` without `"x1"`.
 *
 * \ingroup typegrp
 */
inline std::string to_string(Type t) {
    if (t == Type::invalid)
        return "invalid";

    std::div_t d = std::div(static_cast<uint8_t>(t), type_channel_diff);
    int c = d.quot + 1;
    int const& t_val = d.rem;
    switch(t_val) {
    case static_cast<uint8_t>(Type::uint8):
        return "uint8" + (c > 1 ? "x" + std::to_string(c) : "");
    case static_cast<uint8_t>(Type::int8):
        return "int8" + (c > 1 ? "x" + std::to_string(c) : "");
    case static_cast<uint8_t>(Type::uint16):
        return "uint16" + (c > 1 ? "x" + std::to_string(c) : "");
    case static_cast<uint8_t>(Type::int16):
        return "int16" + (c > 1 ? "x" + std::to_string(c) : "");
    case static_cast<uint8_t>(Type::int32):
        return "int32" + (c > 1 ? "x" + std::to_string(c) : "");
    case static_cast<uint8_t>(Type::float32):
        return "float32" + (c > 1 ? "x" + std::to_string(c) : "");
    case static_cast<uint8_t>(Type::float64):
        return "float64" + (c > 1 ? "x" + std::to_string(c) : "");
    }
    return "undefined type: base type " + std::to_string(t_val) + " with " + std::to_string(c) + " channels.";
}


/**
 * @brief Convenience wrapper to print a @ref Type to a stream
 * @param out is the stream to print it.
 * @param t is the @ref Type to be printed as string.
 * @return out to append more print operations.
 *
 * \ingroup typegrp
 */
inline std::ostream& operator<<(std::ostream& out, Type const& t) {
    out << to_string(t);
    return out;
}


/**
 * @brief Convert base type of a @ref Type to a GDALDepth
 * @param t is the base or full type (of which the base type is used then) to be converted.
 *
 * @return for the argument `t`, which has the base type shown in the following table in the first
 * column, the value shown in the second column:
 * `getBaseType(t)` |   =>  | `toGDALDepth(t)`
 * ---------------- | :---: | ----------------
 * Type::uint8      | gives | GDT_Byte
 * Type::int8       | gives | GDT_Byte
 * Type::uint16     | gives | GDT_UInt16
 * Type::int16      | gives | GDT_Int16
 * Type::int32      | gives | GDT_Int32
 * Type::float32    | gives | GDT_Float32
 * Type::float64    | gives | GDT_Float64
 *
 * Note that there is only one 8 bit GDAL type, which actually is unsigned.
 *
 * \ingroup typegrp
 */
inline GDALDataType toGDALDepth(Type t) {
    uint8_t base = static_cast<uint8_t>(t) % type_channel_diff;
    switch (base) {
    case static_cast<uint8_t>(Type::uint8):
    case static_cast<uint8_t>(Type::int8):
        return GDT_Byte;
    case static_cast<uint8_t>(Type::uint16):
        return GDT_UInt16;
    case static_cast<uint8_t>(Type::int16):
        return GDT_Int16;
    case static_cast<uint8_t>(Type::int32):
        return GDT_Int32;
    case static_cast<uint8_t>(Type::float32):
        return GDT_Float32;
    case static_cast<uint8_t>(Type::float64):
        return GDT_Float64;
    }
    using std::to_string;
    IF_THROW_EXCEPTION(logic_error("You forgot to check the type " + to_string(t) + " here."))
            << errinfo_image_type(t);
    return GDT_Unknown;
}

/**
 * @brief Convert @ref Type to an equivalent OpenCV type
 * @param t is the full type to be converted.
 * @return the OpenCV equivalent type of `t`, like `toCVType(Type::uint16x2) == CV_16UC2`
 *
 * \ingroup typegrp
 */
inline int toCVType(Type t) {
    std::div_t const d = std::div(static_cast<uint8_t>(t), type_channel_diff);
    int const& base  = d.rem;
    int const  chans = d.quot + 1;
    switch (base) {
    case static_cast<uint8_t>(Type::uint8):
        return CV_MAKETYPE(CV_8U, chans);
    case static_cast<uint8_t>(Type::int8):
        return CV_MAKETYPE(CV_8S, chans);
    case static_cast<uint8_t>(Type::uint16):
        return CV_MAKETYPE(CV_16U, chans);
    case static_cast<uint8_t>(Type::int16):
        return CV_MAKETYPE(CV_16S, chans);
    case static_cast<uint8_t>(Type::int32):
        return CV_MAKETYPE(CV_32S, chans);
    case static_cast<uint8_t>(Type::float32):
        return CV_MAKETYPE(CV_32F, chans);
    case static_cast<uint8_t>(Type::float64):
        return CV_MAKETYPE(CV_64F, chans);
    }
    using std::to_string;
    IF_THROW_EXCEPTION(logic_error("You forgot to check the type " + to_string(t) + " here."))
            << errinfo_image_type(t);
    return -1;
}


/**
 * @brief Convert OpenCV type to imagefusion @ref Type
 * @param ocvt is the OpenCV type to be converted.
 * @return imagefusion @ref Type equivalent to `t`, like `toFullType(CV_16UC2) == Type::uint16x2`.
 *
 * Note OpenCV supports more channels than imagefusion, see below.
 *
 * @throws image_type_error if `t` represents a CV type with more than 25 channels
 *
 * \ingroup typegrp
 */
inline Type toFullType(int ocvt) {
    std::div_t const d = std::div(static_cast<uint8_t>(ocvt), CV_DEPTH_MAX);
    int const& base  = d.rem;
    int const  chans = d.quot + 1;
    switch (base) {
    case CV_8U:
        return getFullType(Type::uint8, chans);
    case CV_8S:
        return getFullType(Type::int8, chans);
    case CV_16U:
        return getFullType(Type::uint16, chans);
    case CV_16S:
        return getFullType(Type::int16, chans);
    case CV_32S:
        return getFullType(Type::int32, chans);
    case CV_32F:
        return getFullType(Type::float32, chans);
    case CV_64F:
        return getFullType(Type::float64, chans);
    }
    using std::to_string;
    IF_THROW_EXCEPTION(logic_error("You forgot to check the OpenCV type " + to_string(ocvt) + " here."));
    return Type::invalid;
}


/**
 * @brief Convert GDAL depth to imagefusion base type
 * @param t is the depth to be converted.
 * @return the base type corresponding to the given GDAL depth as shown in the following table
 * `t`           |   =>  | `toBaseType(t)`
 * ------------- | :---: | ----------------
 * `GDT_Byte`    | gives | Type::uint8
 * `GDT_UInt16`  | gives | Type::uint16
 * `GDT_Int16`   | gives | Type::int16
 * `GDT_Int32`   | gives | Type::int32
 * `GDT_Float32` | gives | Type::float32
 * `GDT_Float64` | gives | Type::float64
 *
 * Note, in GDAL there is no signed 8 bit type.
 *
 * \ingroup typegrp
 *
 * @throws image_type_error for
 * [GDALDataTypes](http://www.gdal.org/gdal_8h.html#a22e22ce0a55036a96f652765793fb7a4) not listed
 * in the table above, like `GDT_UInt32` or complex types.
 */
inline Type toBaseType(GDALDataType t) {
    switch (t) {
    case GDT_Unknown:
        return Type::invalid;
    case GDT_Byte:
        return Type::uint8;
    case GDT_UInt16:
        return Type::uint16;
    case GDT_Int16:
        return Type::int16;
    case GDT_Int32:
        return Type::int32;
    case GDT_Float32:
        return Type::float32;
    case GDT_Float64:
        return Type::float64;
    default:
        using std::to_string;
        IF_THROW_EXCEPTION(image_type_error(std::string("The GDAL type ") + GDALGetDataTypeName(t) + " (" + to_string(t) + ") is not compatible with imagefusion."));
        return Type::invalid;
    }
}


/**
 * @brief Check whether the given type is an integer type
 * @param t is the full or base type to check.
 *
 * @return true if the corresponding base type is an integer type, false if it is a floating point
 * type.
 *
 * The integer base types are:
 * - Type::int8
 * - Type::uint8
 * - Type::int16
 * - Type::uint16
 * - Type::int32
 *
 * Note, this is a constexpr function an can be used at both compile time and runtime.
 *
 * \ingroup typegrp
 */
inline constexpr bool isIntegerType(Type t) {
    return static_cast<uint8_t>(t) % type_channel_diff <= 4;
}


/**
 * @brief Check whether the given type is a floating point type
 * @param t is the full or base type to check.
 *
 * @return true if the corresponding base type is a floating point type, false if it is an integer
 * type.
 *
 * The floating point base types are:
 * - Type::float32
 * - Type::float64
 *
 * Note, this is a constexpr function an can be used at both compile time and runtime.
 *
 * \ingroup typegrp
 */
inline constexpr bool isFloatType(Type t) {
    return !isIntegerType(t);
}


/**
 * @brief Check whether the given type is a signed type
 * @param t is the full or base type to check.
 *
 * @return true if the corresponding base type is a signed type, false if it is an unsigned type.
 *
 * The signed types are:
 * - Type::int8
 * - Type::int16
 * - Type::int32
 * - Type::float32
 * - Type::float64
 *
 * Note, this is a constexpr function an can be used at both compile time and runtime.
 *
 * \ingroup typegrp
 */
inline constexpr bool isSignedType(Type t) {
    return getBaseType(t) != Type::uint8 && getBaseType(t) != Type::uint16;
}


/**
 * @brief Check whether the given type is an unsigned type
 * @param t is the full or base type to check.
 *
 * @return true if the corresponding base type is an unsigned type, false if it is a signed type.
 *
 * The unsigned types are:
 * - Type::uint8
 * - Type::uint16
 *
 * Note, this is a constexpr function an can be used at both compile time and runtime.
 *
 * \ingroup typegrp
 */
inline constexpr bool isUnsignedType(Type t) {
    return !isSignedType(t);
}


/**
 * @brief Get imagefusion Type and image data range from C++ type
 *
 * @tparam T is the C++ type.
 *
 * This only provides the basetype and image data range. For these and additional properties
 * TypeTraits can be used.
 *
 * \ingroup typegrp
 */
template<typename T>
struct BaseTypeTraits {
    /**
     * @brief imagefusion basetype depending on `T`
     *
     *  `T`        | `basetype`
     * ------------|--------------
     *  `int8_t`   | Type::int8
     *  `uint8_t`  | Type::uint8
     *  `int16_t`  | Type::int16
     *  `uint16_t` | Type::uint16
     *  `int32_t`  | Type::int32
     *  `float`    | Type::float32
     *  `double`   | Type::float64
     */
    constexpr static Type basetype = Type::invalid;

    /**
     * @brief Image range min for `T`
     *
     * This is basically the data range minimum except for floating point data types where it is 0.
     */
    constexpr static T min = 0;

    /**
     * @brief Image range max for `T`
     *
     * This is basically the data range maximum except for floating point data types where it is 1.
     */
    constexpr static T max = 0;
    static_assert(std::is_pod<T>::value, "Use only the base types of Type in BaseTypeTraits! Use TypeTraits for full types (and base types).");
};

/// \cond
template<>
struct BaseTypeTraits<int8_t> {
    constexpr static Type basetype = Type::int8;
    constexpr static int8_t min = std::numeric_limits<int8_t>::min();
    constexpr static int8_t max = std::numeric_limits<int8_t>::max();
};

template<>
struct BaseTypeTraits<uint8_t> {
    constexpr static Type basetype = Type::uint8;
    constexpr static uint8_t min = std::numeric_limits<uint8_t>::min();
    constexpr static uint8_t max = std::numeric_limits<uint8_t>::max();
};

template<>
struct BaseTypeTraits<int16_t> {
    constexpr static Type basetype = Type::int16;
    constexpr static int16_t min = std::numeric_limits<int16_t>::min();
    constexpr static int16_t max = std::numeric_limits<int16_t>::max();
};

template<>
struct BaseTypeTraits<uint16_t> {
    constexpr static Type basetype = Type::uint16;
    constexpr static uint16_t min = std::numeric_limits<uint16_t>::min();
    constexpr static uint16_t max = std::numeric_limits<uint16_t>::max();
};

template<>
struct BaseTypeTraits<int32_t> {
    constexpr static Type basetype = Type::int32;
    constexpr static int32_t min = std::numeric_limits<int32_t>::min();
    constexpr static int32_t max = std::numeric_limits<int32_t>::max();
};

template<>
struct BaseTypeTraits<float> {
    constexpr static Type basetype = Type::float32;
    constexpr static float min = 0;
    constexpr static float max = 1;
};

template<>
struct BaseTypeTraits<double> {
    constexpr static Type basetype = Type::float64;
    constexpr static double min = 0;
    constexpr static double max = 1;
};
/// \endcond

template<typename T>
constexpr Type BaseTypeTraits<T>::basetype;

/**
 * @brief Traits class to get Type value from data types
 *
 * @tparam T - C++ data type. May be a POD like uint8_t or float or an array type. As array types
 * `std::arrays`, `cv::Matx`, `cv::Vec`, `cv::Scalar_` and C-arrays are supported. However
 * `cv::Scalar_` is always assumed to be a four channel type, since it has four elements.
 *
 * This is the class to get an imagefusion::Type from a C++ data type and more. Examples that
 * evaluate to `true`:
 * @code
 * TypeTraits<double>::basetype == Type::float64;
 * TypeTraits<std::array<uint16_t,3>>::basetype == Type::uint16;
 * TypeTraits<std::array<uint16_t,3>>::fulltype == Type::uint16x3;
 * TypeTraits<std::array<uint16_t,3>>::channels == 3;
 * TypeTraits<double>::min == 0;
 * TypeTraits<double>::max == 1;
 * @endcode
 *
 * \ingroup typegrp
 */
template<typename T>
struct TypeTraits {
    /**
     * @brief Corresponding base type
     *
     * Example: For `T == double` the `basetype` is Type::float64. For
     * `T == std::array<uint16_t, 3>` it is Type::uint16.
     */
    constexpr static Type basetype = BaseTypeTraits<T>::basetype;

    /**
     * @brief Number of channels according to `T`
     *
     * For POD types the number of channels is just 1. For `std::array`s, `cv::Matx`, `cv::Vec` it
     * is the number of elements and for `cv::Scalar_` it is 4.
     */
    constexpr static int channels = 1;

    /**
     * @brief Corresponding full type
     *
     * Example: For `T == double` the `fulltype` is Type::float64x1 (which is the same as
     * Type::float64). For `T == std::array<uint16_t, 3>` it is Type::uint16x3.
     */
    constexpr static Type fulltype = getFullType(basetype, 1);

    /**
     * @brief Image range min
     *
     * For integer types this is just <code>std::numeric_limit<T>::min()</code>, but for floating
     * point types this is 0.
     *
     * @remark The value for black is always 0, also for signed integer types!
     */
    constexpr static T min = BaseTypeTraits<T>::min;

    /**
     * @brief Image range max
     *
     * For integer types this is just <code>std::numeric_limit<T>::max()</code>, but for floating
     * point types this is 1.
     */
    constexpr static T max = BaseTypeTraits<T>::max;
};

template<typename T>
constexpr int TypeTraits<T>::channels;

template<typename T>
constexpr Type TypeTraits<T>::basetype;

template<typename T>
constexpr Type TypeTraits<T>::fulltype;

template<typename T>
constexpr T TypeTraits<T>::min;

template<typename T>
constexpr T TypeTraits<T>::max;

/// \cond
template<typename T, std::size_t N>
struct TypeTraits<std::array<T, N>> {
    constexpr static Type basetype = BaseTypeTraits<T>::basetype;
    constexpr static int channels = N;
    constexpr static Type fulltype = getFullType(basetype, N);
    constexpr static T min = BaseTypeTraits<T>::min;
    constexpr static T max = BaseTypeTraits<T>::max;
};

template<typename T, std::size_t N>
constexpr int TypeTraits<std::array<T, N>>::channels;

template<typename T, std::size_t N>
constexpr Type TypeTraits<std::array<T, N>>::basetype;

template<typename T, std::size_t N>
constexpr Type TypeTraits<std::array<T, N>>::fulltype;

template<typename T, std::size_t N>
constexpr T TypeTraits<std::array<T, N>>::min;

template<typename T, std::size_t N>
constexpr T TypeTraits<std::array<T, N>>::max;


template<typename T, std::size_t N>
struct TypeTraits<T[N]> {
    constexpr static Type basetype = BaseTypeTraits<T>::basetype;
    constexpr static int channels = N;
    constexpr static Type fulltype = getFullType(basetype, N);
    constexpr static T min = BaseTypeTraits<T>::min;
    constexpr static T max = BaseTypeTraits<T>::max;
};

template<typename T, std::size_t N>
constexpr int TypeTraits<T[N]>::channels;

template<typename T, std::size_t N>
constexpr Type TypeTraits<T[N]>::basetype;

template<typename T, std::size_t N>
constexpr Type TypeTraits<T[N]>::fulltype;

template<typename T, std::size_t N>
constexpr T TypeTraits<T[N]>::min;

template<typename T, std::size_t N>
constexpr T TypeTraits<T[N]>::max;


template<typename T, int N, int M>
struct TypeTraits<cv::Matx<T, N, M>> {
    constexpr static Type basetype = BaseTypeTraits<T>::basetype;
    constexpr static int channels = N * M;
    constexpr static Type fulltype = getFullType(basetype, N * M);
    constexpr static T min = BaseTypeTraits<T>::min;
    constexpr static T max = BaseTypeTraits<T>::max;
};

template<typename T, int N, int M>
constexpr int TypeTraits<cv::Matx<T, N, M>>::channels;

template<typename T, int N, int M>
constexpr Type TypeTraits<cv::Matx<T, N, M>>::basetype;

template<typename T, int N, int M>
constexpr Type TypeTraits<cv::Matx<T, N, M>>::fulltype;

template<typename T, int N, int M>
constexpr T TypeTraits<cv::Matx<T, N, M>>::min;

template<typename T, int N, int M>
constexpr T TypeTraits<cv::Matx<T, N, M>>::max;


template<typename T, int N>
struct TypeTraits<cv::Vec<T, N>> {
    constexpr static Type basetype = BaseTypeTraits<T>::basetype;
    constexpr static int channels = N;
    constexpr static Type fulltype = getFullType(basetype, N);
    constexpr static T min = BaseTypeTraits<T>::min;
    constexpr static T max = BaseTypeTraits<T>::max;
};

template<typename T, int N>
constexpr int TypeTraits<cv::Vec<T, N>>::channels;

template<typename T, int N>
constexpr Type TypeTraits<cv::Vec<T, N>>::basetype;

template<typename T, int N>
constexpr Type TypeTraits<cv::Vec<T, N>>::fulltype;

template<typename T, int N>
constexpr T TypeTraits<cv::Vec<T, N>>::min;

template<typename T, int N>
constexpr T TypeTraits<cv::Vec<T, N>>::max;


template<typename T>
struct TypeTraits<cv::Scalar_<T>> {
    constexpr static Type basetype = BaseTypeTraits<T>::basetype;
    constexpr static int channels = 4;
    constexpr static Type fulltype = getFullType(basetype, 4);
    constexpr static T min = BaseTypeTraits<T>::min;
    constexpr static T max = BaseTypeTraits<T>::max;
};

template<typename T>
constexpr int TypeTraits<cv::Scalar_<T>>::channels;

template<typename T>
constexpr Type TypeTraits<cv::Scalar_<T>>::basetype;

template<typename T>
constexpr Type TypeTraits<cv::Scalar_<T>>::fulltype;

template<typename T>
constexpr T TypeTraits<cv::Scalar_<T>>::min;

template<typename T>
constexpr T TypeTraits<cv::Scalar_<T>>::max;
/// \endcond



/**
 * @brief Get C++ type and string representation from imagefusion Type
 *
 * This holds a string representation for an imagefusion base type and the corresponding C++ type.
 * DataType also provides the base type, see DataType::base_type.
 */
template<Type t>
struct BaseType {

    /**
     * @brief String representation
     *
     *  `T`             | `str`
     * -----------------|-----------
     *  `Type::int8`    | "int8"
     *  `Type::uint8`   | "uint8"
     *  `Type::int16`   | "int16"
     *  `Type::uint16`  | "uint16"
     *  `Type::int32`   | "int32"
     *  `Type::float32` | "float32"
     *  `Type::float64` | "float64"
     */
    constexpr static const char* str = "invalid";

    /**
     * @brief C++ data type
     *
     *  `T`             | `base_type`
     * -----------------|-----------
     *  `Type::int8`    | `int8_t`
     *  `Type::uint8`   | `uint8_t`
     *  `Type::int16`   | `int16_t`
     *  `Type::uint16`  | `uint16_t`
     *  `Type::int32`   | `int32_t`
     *  `Type::float32` | `float`
     *  `Type::float64` | `double`
     */
    using base_type = void;
    static_assert(getChannels(t) == 1, "Use only the base types of Type in BaseType! Use DataType for full types (and base types).");
};

/// \cond
template<>
struct BaseType<Type::int8> {
    using base_type = int8_t;
    constexpr static const char* str = "int8";
};

template<>
struct BaseType<Type::uint8> {
    using base_type = uint8_t;
    constexpr static const char* str = "uint8";
};

template<>
struct BaseType<Type::int16> {
    using base_type = int16_t;
    constexpr static const char* str = "int16";
};

template<>
struct BaseType<Type::uint16> {
    using base_type = uint16_t;
    constexpr static const char* str = "uint16";
};

template<>
struct BaseType<Type::int32> {
    using base_type = int32_t;
    constexpr static const char* str = "int32";
};

template<>
struct BaseType<Type::float32> {
    static_assert(sizeof(float) == 4, "Sorry, float requires to be 4 bytes. This seems not to be the case on this system.");
    using base_type = float;
    constexpr static const char* str = "float32";
};

template<>
struct BaseType<Type::float64> {
    static_assert(sizeof(double) == 8, "Sorry, double requires to be 8 bytes. This seems not to be the case on this system.");
    using base_type = double;
    constexpr static const char* str = "float64";
};
/// \endcond

template<Type t>
constexpr const char* BaseType<t>::str;

/**
 * @brief Traits class to get data types from compile time Type value
 *
 * @tparam t - compile-time @ref Type value to convert to real data type (base or array type) and
 * get the data range for images.
 *
 * This is the class to get a data type from a compile-time @ref Type value. A data type is
 * required to access single pixels or channel values of pixels. To get the @ref Type value at
 * compile-time, you can make a functor and use the provided template metaprogramming facilities.
 *
 * @see CallBaseTypeFunctor, CallBaseTypeFunctorRestrictBaseTypesTo
 *
 * \ingroup typegrp
 */
template<Type t>
struct DataType {
    /**
     * @brief Get the number of channels of a full type
     * @see getChannels(Type t)
     */
    constexpr static unsigned int channels = getChannels(t);


    /**
     * @brief Base data type
     *
     * The base data type can be used for channel value access. This is e. g. `int32` for `t` being
     * `Type::int32x2` (or any other number of channels).
     *
     * As an example, where you have the compile-time value @ref Type `t` from a template parameter
     * or `constexpr` etc., you can check whether all values in all channels in an Image `img` are
     * 42 like
     * @code
     * using basetype = typename DataType<t>::base_type;
     * for (int y = 0; y < img.height(); ++y)
     *     for (int x = 0; x < img.width(); ++x)
     *         for (unsigned int c = 0; c < img.channels(); ++c)
     *             if (img.at<basetype>(x, y, c) != 42)
     *                 return false;
     * return true;
     * @endcode
     * In this example only the base type has been used. This allows to use #CallBaseTypeFunctor to
     * determine `t`, if the above code is in an appropriate functor.
     */
    using base_type = typename BaseType<getBaseType(t)>::base_type;


    /**
     * @brief Pixel data type as array
     *
     * The array data type can be used for whole pixels, also for multi-channel images. So e. g.
     * for `t` being `Type::int32x2` this is `std::array<int32,2>`.
     *
     * As an example, where you have the compile-time value @ref Type `t` from a template parameter
     * or constexpr etc., you can check whether all values in all channels in an Image `img` are 42
     * like
     * @code
     * using pixeltype = typename DataType<t>::array_type;
     * for (int y = 0; y < img.height(); ++y) {
     *     for (int x = 0; x < img.width(); ++x) {
     *         pixeltype const& pixel = img.at<pixeltype>(x, y);
     *         for (int c = 0; c < DataType<t>::channels; ++c)
     *             if (pixel[c] != 42)
     *                 return false;
     *     }
     * }
     * return true;
     * @endcode
     * In this example the full type has been used. However, usually you only have `t` from calling
     * a functor with #CallBaseTypeFunctor and you can do quite the same with it:
     * @code
     * using basetype = typename DataType<t>::base_type;
     * for (int y = 0; y < img.height(); ++y) {
     *     for (int x = 0; x < img.width(); ++x) {
     *         for (unsigned int c = 0; c < img.channels(); ++c)
     *             basetype const& pixval = img.at<basetype>(x, y, c);
     *             if (pixval != 42)
     *                 return false;
     *     }
     * }
     * return true;
     * @endcode
     */
    using array_type = std::array<base_type, channels>;

    /**
     * @brief Minimum value for images
     *
     * This is just std::numeric_limits<base_type>::min() for integer types and 0 for floating
     * point types.
     *
     * @remark The value for black is always 0, also for signed integer types!
     */
    constexpr static base_type min = BaseTypeTraits<base_type>::min;


    /**
     * @brief Maximum value for images
     *
     * This is just std::numeric_limits<base_type>::max() for integer types and 1 for floating
     * point types.
     */
    constexpr static base_type max = BaseTypeTraits<base_type>::max;
};

template<Type t>
constexpr unsigned int DataType<t>::channels;

template<Type t>
constexpr typename DataType<t>::base_type DataType<t>::min;

template<Type t>
constexpr typename DataType<t>::base_type DataType<t>::max;


/**
 * @brief Check whether this is a (known) boolean fixed size type
 * @tparam T is a C++ type as std::array<bool, 4> or bool.
 *
 * This is a std::false_type if it is not a fixed size boolean type or a std::true_type if it is.
 * So you can use the member `value` and check whether it is `false` or `true`.
 *
 * Note that a std::vector<bool> will make this a std::false_type. Here is the list of
 * specializations that make it a std::true_type:
 *  * `bool`
 *  * `bool[]`
 *  * `bool[N]` with N being of type `std::size_t`
 *  * `std::array<bool, N>` with N being of type `std::size_t`
 *  * `cv::Matx<bool, N, M>` with N and M being of type `int`
 *  * `cv::Vec<bool, N>` with N being of type `int`
 *  * `cv::Scalar_<bool>`
 */
template<typename T>
struct is_known_bool_type : std::false_type { };

/// \cond
template<std::size_t N>
struct is_known_bool_type<std::array<bool, N>> : std::true_type { };

template<>
struct is_known_bool_type<bool> : std::true_type { };

template<>
struct is_known_bool_type<bool[]> : std::true_type { };

template<std::size_t N>
struct is_known_bool_type<bool[N]> : std::true_type { };

template<int N, int M>
struct is_known_bool_type<cv::Matx<bool, N, M>> : std::true_type { };

template<int N>
struct is_known_bool_type<cv::Vec<bool, N>> : std::true_type { };

template<>
struct is_known_bool_type<cv::Scalar_<bool>> : std::true_type { };
/// \endcond



/// \cond
template<uint8_t currentChannel, uint8_t... otherChannels>
struct find_chan_impl {
    template<typename ret_type, Type... types>
    struct find_type_impl {
        template<class Functor>
        static inline ret_type run(Functor&& f, uint8_t const& c, Type const& t) {
            if (c == currentChannel)
                return find_chan_impl<currentChannel>::template find_type_impl<ret_type, types...>::run(std::forward<Functor>(f), c, t);
            else
                return find_chan_impl<otherChannels...>::template find_type_impl<ret_type, types...>::run(std::forward<Functor>(f), c, t);
        }
    };
};


template<>
struct find_chan_impl<0> {
    template<typename ret_type, Type... types>
    struct find_type_impl {
        template<class Functor>
        [[noreturn]] static inline ret_type run(Functor&& /*f*/, uint8_t const& c, Type const& t) {
            IF_THROW_EXCEPTION(image_type_error("A part of the algorithm you have called is marked as not compatible with " + std::to_string(static_cast<int>(c)) + " channel images."))
                    << errinfo_image_type(getFullType(t,c));
        }
    };
};


template<uint8_t channel>
struct find_chan_impl<channel> {
    template<typename ret_type, Type currentType, Type... otherTypes>
    struct find_type_impl {
        static_assert(static_cast<uint8_t>(currentType) < type_channel_diff, "You can only restrict to base types, without channel specification");

        template<class Functor>
        static inline ret_type run(Functor&& f, uint8_t const& c, Type const& t) {
            if (t == currentType)
                return find_type_impl<ret_type, currentType>::run(std::forward<Functor>(f), c, t);
            else
                return find_type_impl<ret_type, otherTypes...>::run(std::forward<Functor>(f), c, t);
        }
    };

    template<typename ret_type, Type type>
    struct find_type_impl<ret_type, type> {
        template<class Functor>
        static inline ret_type run(Functor&& f, uint8_t const& /*c*/, Type const& /*t*/) {
            return f.template operator()<getFullType(type,channel)>();
        }
    };

    template<typename ret_type>
    struct find_type_impl<ret_type, Type::invalid> {
        template<class Functor>
        [[noreturn]] static inline ret_type run(Functor&& /*f*/, uint8_t const& c, Type const& t) {
            IF_THROW_EXCEPTION(image_type_error("A part of the algorithm you have called is marked as not compatible with images of base type " + to_string(t) + "."))
                    << errinfo_image_type(getFullType(t,c));
        }
    };
};
/// \endcond


// User callable templates
// templates to find only base type
/**
 * @brief Call a functor with base type, but try only the specified base types
 *
 * @tparam firstType and types - base types to restrict invokation to
 *
 * This calls a functor with base type, but only tries the specified base types, e. g.
 * @code
 * CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(Fun{}, img.type());
 * @endcode
 * where `img` is an Image and `Fun` is a functor class as defined [here](@ref run). The effect of
 * not trying all possible base types is that the template code will only be generated for the
 * specified base types. This could lead to a smaller code size and, because of less misses, higher
 * performance.
 *
 * There are clear indicators, when you should use a type restriction. That is, your functor `Fun`
 * requires specific base types to work properly. If it e. g. only works on floating-point images,
 * you should call it with this base type restriction. This will give you rather good error
 * messages (as exception, see @ref run), if the @ref Type argument `t` given to the `run` method
 * does not comply to the restriction.
 *
 * Note, you should *not* use a type restriction, if the functor `Fun` is usually used with a
 * specific base type, but would as well work with any base type - at least not in code that could
 * be used by someone else for a broader range of applications.
 *
 * @see CallBaseTypeFunctor, run
 *
 * \ingroup typegrp
 */
template<Type firstType, Type... types>
struct CallBaseTypeFunctorRestrictBaseTypesTo {
    /**
     * @brief Obligatory method to really invoke the functor
     *
     * @tparam Functor - functor class. Usually this is automatically resolved.
     *
     * @param f is the functor object to call. This argument supports move semantics. The functor
     * must be able to be called by:
     * @code
     * Functor::operator()<Type t>()
     * @endcode
     * which may be `const`. Note, that `t` will be the corresponding base type to the parameter
     * `t`. The return type is arbitrary and `run` will have the same return type. The returned
     * object will be returned by `run`. However, the functor may only have one return type (in
     * case of specialization). So for example a simple functor could look like
     * @code
     * struct Fun {
     *      template<Type t>
     *      bool operator()() const {
     *          return true;
     *      }
     * }
     * @endcode
     * If you restricted the base types, the functor should also reflect that. To let code fail on
     * compilation you can use `static_assert`s. For example to make sure the functor is only used
     * with floating point image types, you can use the compile-time checks from type_traits and
     * also any constexpr function in your `operator()`:
     * @code
     * template<Type t>
     * void operator()() {
     *     using basetype = typename DataType<t>::base_type;
     *     static_assert(std::is_floating_point<basetype>::value, "This functor requires a floating point image.");
     * }
     * @endcode
     * Such a functor will only compile, if the assertion are fulfilled. A simple
     * `CallBaseTypeFunctor::run(Fun{}, img.type());` won't compile. So it enforces restricted
     * calls at compile-time, like:
     * @code
     * CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(Fun{}, img.type());
     * @endcode
     * Your functor can also be more complex and have members, which are initialized by a
     * constructor or the functor can be a factory. It can provide specialized versions for
     * specific image types; alternatively, you can also use an if-else with the compile-time
     * template parameter `t` within operator().
     *
     * @param t is the image @ref Type to call the functor for. The functor will receive
     * `getBaseType(t)` as compile-time template parameter
     *
     * This method calls the given functor object `f` with the base type of the template parameter
     * `t`, i. e. with `getBaseType(t)`. This semantically converts a runtime function argument to
     * a compile-time template parameter. In the background, there is a flat if-else structure for
     * the base type.
     *
     * @return whatever `f` returns. The return type of run will be the same as the one of `f`.
     *
     * @throws image_type_error if base type is restricted to something which does not fit the Type
     * `t` given as argument in run. Example, where calling the functor `SomeFunctor` is restricted
     * to `uint8` types as template argument, but run is given an `int8` base type:
     * @code
     * CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8>::run(SomeFunctor{}, Type::int8x2);
     * @endcode
     * Usually the last `Type::int8x2` is not given explicitly at compile time, but rather comes
     * from an image, like `img.type()`, which could be Type::int8x2. Though Type::uint8x2 would
     * not throw, since the base type is Type::uint8.
     */
    template<class Functor>
    static inline auto run(Functor&& f, Type t) -> decltype(f.template operator()<firstType>()) {
        using functor_ret_type = decltype(f.template operator()<firstType>());
        return find_chan_impl<1>::template find_type_impl<functor_ret_type, firstType, types..., Type::invalid>::run(std::forward<Functor>(f), 1, getBaseType(t));
    }
};

/**
 * @brief Call a functor with base type and try all possible base types
 *
 * This calls a functor with base type and tries all possible base types, e. g.
 * @code
 * CallBaseTypeFunctor::run(Fun{}, img.type());
 * @endcode
 * where `img` is an Image and `Fun` is a functor class as defined
 * [here](@ref CallBaseTypeFunctorRestrictBaseTypesTo::run). The effect of trying all possible base
 * types is that the template code will be generated for all of these (see @ref Type). This could
 * lead to a larger code size and, because of more misses, less performance.
 *
 * However, if the functor, which you are trying to call, is itself restricted by `static_assert`s
 * (as described [here](@ref CallBaseTypeFunctorRestrictBaseTypesTo::run)) or by
 * [SFINAE](https://en.wikipedia.org/wiki/SFINAE) or similar, this call will fail at compile-time.
 * Then, you have to use an appropriate restricted version.
 *
 * @see CallBaseTypeFunctorRestrictBaseTypesTo, CallBaseTypeFunctorRestrictBaseTypesTo::run
 *
 * \ingroup typegrp
 */
using CallBaseTypeFunctor = CallBaseTypeFunctorRestrictBaseTypesTo<Type::int8, Type::uint8, Type::int16, Type::uint16, Type::int32, Type::float32, Type::float64>;

} /* namespace imagefusion */

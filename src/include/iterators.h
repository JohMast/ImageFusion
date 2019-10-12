#pragma once

#include <boost/iterator/iterator_adaptor.hpp>


namespace imagefusion {


/**
 * \defgroup imgit Image iterators
 *
 * @brief Iterators for images
 *
 * This module describes iterators, which iterate through the pixels of an Image. First there is a
 * @ref PixelIterator. If you have an `Image` `img` of a specific dynamic type, then you have to
 * use a corresponding array type, like `std::array<uint16_t,2>` or
 * `DataType<Type::uint16x2>::%array_type`. Then the dereferenced iterator `*it` is an element of
 * type `std::array<uint16_t,2>` and you can use its `at`-method or `operator[]`:
 * @code
 * Image img{5, 6, Type::uint16x2};
 * for (auto it = img.begin<std::array<uint16_t,2>>(), it_end = img.end<std::array<uint16_t,2>>(); it != it_end; ++it)
 *     std::cout << it->at(0) << ", " << (*it)[1] << std::endl;
 * @endcode
 *
 * Note, for @ref PixelIterator you need to know the full type (including number of channels) at
 * compile time.
 *
 * Secondly, there is an iterator, which only iterates through a specific channel of an image. This
 * is the ChannelValueIterator. Note, in constrast to the @ref PixelIterator above, you need to
 * specify the channel in the argument of `begin` *and* (the same in) `end`. The type you have to
 * specify is only a base data type. If you know the base type at compile time, but not the number
 * of channels, you could still use this iterator. The dereferenced iterator `*it` is of type
 * `uint16_t` in the example below.
 * @code
 * Image img{5, 6, Type::uint16x2};
 * for (auto it = img.begin<uint16_t>(0), it_end = img.end<uint16_t>(0); it != it_end; ++it)
 *     std::cout << *it << std::endl;
 * @endcode
 *
 * Note, for ChannelValueIterator you need to know the base type (but not the number of channels)
 * at compile time. To get the base type you can use the facilities of the @ref typegrp, especially
 * @ref CallBaseTypeFunctor and friends.
 */

/**
 * @brief Iterates through all pixel values of a specific channel
 * @tparam T - plain data type (not an array type), e. g. for an Image with
 *             full type `Type::uint16x3` the type `T` must be `uint16_t`.
 *
 * This iterator allows to iterate through a specific channel of an Image. So as template parameter
 * it needs a base data type and *not* an array type. To get such an iterator, use
 * Image::begin<T>(unsigned int channel) and Image::end<T>(unsigned int channel). Dereferencing
 * this iterator gives the corresponding channel value as `T&` (or `T const&` in case of @ref
 * ConstChannelValueIterator<T>).
 *
 * The iterator works for cropped images as expected. Note, regarding performance a plain direct
 * access via Image::at seems to be better than via any iterator (also @ref PixelIterator is
 * slower). This holds for read and write access.
 *
 * \see imgit
 * \ingroup imgit
 */
template<typename T>
class ChannelValueIterator : public boost::iterator_adaptor<ChannelValueIterator<T>, T*, boost::use_default, boost::random_access_traversal_tag> {
private:
    struct enabler {};

public:
    /**
     * @brief Default constructor, cannot iterate
     */
    ChannelValueIterator();


    /**
     * @brief Construct an iterator at the begin of an image
     * @param p is the pointer to a channel in pixel (0, 0).
     * @param width is the width of the image.
     * @param height is the height of the image.
     * @param diffPerCol is the distance in elements (of type `T`) to the next column.
     * @param diffPerRow is the distance in elements (of type `T`) to the next row.
     *
     * Note, the channel itself is not required to construct a ChannelValueIterator. Therefore `p`
     * must point to the correct channel.
     */
    ChannelValueIterator(T* p, std::size_t width, std::size_t height, std::ptrdiff_t diffPerCol, std::ptrdiff_t diffPerRow);


    /**
     * @brief Construct an iterator at a specified position of an image
     * @param p is the pointer to a channel in pixel (`y`, `x`).
     * @param x is the column where `p` is located.
     * @param y is the row where `p` is located.
     * @param width is the width of the image.
     * @param height is the height of the image.
     * @param diffPerCol is the distance in elements (of type `T`) to the next column.
     * @param diffPerRow is the distance in elements (of type `T`) to the next row.
     *
     * Note, the channel itself is not required to construct a ChannelValueIterator. Therefore `p`
     * must point to the correct channel.
     */
    ChannelValueIterator(T* p, std::size_t x, std::size_t y, std::size_t width, std::size_t height, std::ptrdiff_t diffPerCol, std::ptrdiff_t diffPerRow);


    /**
     * @brief This allows conversion from a ChannelValueIterator to a @ref ConstChannelValueIterator
     */
    template <class OtherValue>
    ChannelValueIterator(
        ChannelValueIterator<OtherValue> const& other,
        typename std::enable_if<std::is_convertible<OtherValue*,T*>::value, enabler>::type = enabler()
    );

    void setX(std::size_t x);
    void setY(std::size_t y);
    void setPos(Point const& pos);
    std::size_t const& getX() const;
    std::size_t const& getY() const;
    Point getPos() const;

private:
    friend class boost::iterator_core_access;
    std::size_t cur_x;
    std::size_t cur_y;
    std::size_t width;
    std::size_t height;
    std::ptrdiff_t diffPerRow;
    std::ptrdiff_t diffPerCol;

    void increment();
    void decrement();
    void advance(typename ChannelValueIterator::iterator_adaptor_::difference_type n);

    template <class OtherValue>
    typename ChannelValueIterator::iterator_adaptor_::difference_type distance_to(ChannelValueIterator<OtherValue> const& other) const;
};


/**
 * @brief Const version of the ChannelValueIterator
 *
 * @tparam T - plain data type (not an array type), e. g. for an Image with full type
 * `Type::uint16x3` the type `T` must be `uint16_t`.
 *
 * ConstImage%s will only return ConstChannelValueIterator%s as channel iterator.
 *
 * @see ChannelValueIterator
 * \ingroup imgit
 */
template<typename T>
using ConstChannelValueIterator = ChannelValueIterator<const T>;


/**
 * @brief Iterates through all pixel values
 *
 * @tparam T - array data type. For example for an Image with full type `Type::uint16x3` the type
 * `T` must be `std::array<uint16_t,3>`.
 *
 * This iterator allows to iterate through an Image. So as template parameter it needs an array
 * type and *not* a plain data type. The array type must have the correct compile time size, i. e.
 * it must be allocated on the stack. So you cannot use something like `std::vector`. Use
 * `std::array` or `cv::Vec` with the correct number of channels. To get such an iterator, use
 * Image::begin<T>() and Image::end<T>(). Dereferencing this iterator gives the corresponding pixel
 * value as `T&` (or `T const&` in case of @ref ConstPixelIterator<T>).
 *
 * The iterator works for cropped images as expected. Note, regarding performance a plain direct
 * access via Image::at seems to be better than via any iterator (also ChannelValueIterator is
 * slower). This holds for read and write access.
 *
 * The implementation from OpenCV's MatIterator is used.
 *
 * \see imgit
 * \ingroup imgit
 */
template<typename T>
using PixelIterator = cv::MatIterator_<T>;


/**
 * @brief Const version of #PixelIterator
 *
 * @tparam T - array data type. For example for an Image with full type `Type::uint16x3` the type
 * `T` must be `std::array<uint16_t,3>`.
 *
 * ConstImage%s will only return ConstPixelIterator%s as pixel iterator.
 *
 * @see PixelIterator
 * \ingroup imgit
 */
template<typename T>
using ConstPixelIterator = cv::MatConstIterator_<T>;




// implementation
template <typename T>
inline ChannelValueIterator<T>::ChannelValueIterator()
    : ChannelValueIterator::iterator_adaptor_(nullptr)
{
}

template <typename T>
inline ChannelValueIterator<T>::ChannelValueIterator(T* p, std::size_t width, std::size_t height, std::ptrdiff_t diffPerCol, std::ptrdiff_t diffPerRow)
    : ChannelValueIterator(p, 0, 0, width, height, diffPerCol, diffPerRow)
{
}

template <typename T>
inline ChannelValueIterator<T>::ChannelValueIterator(T* p, std::size_t x, std::size_t y, std::size_t width, std::size_t height, std::ptrdiff_t diffPerCol, std::ptrdiff_t diffPerRow)
    : ChannelValueIterator::iterator_adaptor_(p), cur_x{x}, cur_y{y}, width{width}, height{height}, diffPerRow{diffPerRow}, diffPerCol{diffPerCol}
{
}

template <typename T>
template <typename OtherValue>
inline ChannelValueIterator<T>::ChannelValueIterator(
    ChannelValueIterator<OtherValue> const& other,
    typename std::enable_if<std::is_convertible<OtherValue*,T*>::value, enabler>::type
)
  : ChannelValueIterator::iterator_adaptor_(other.base())
{
}


template <typename T>
inline void ChannelValueIterator<T>::increment() {
    this->base_reference() += diffPerCol;
    if (++cur_x >= width) {
        cur_x = 0;
        ++cur_y;
        this->base_reference() += diffPerRow - diffPerCol * width;
    }
}


template <typename T>
inline void ChannelValueIterator<T>::decrement() {
    this->base_reference() -= diffPerCol;
    if (cur_x-- == 0) {
        cur_x = width - 1;
        --cur_y;
        this->base_reference() += -diffPerRow + diffPerCol * width;
    }
}


template <typename T>
inline void ChannelValueIterator<T>::advance(typename ChannelValueIterator::iterator_adaptor_::difference_type n) {
    auto d = std::div(n, width);
    auto const& n_rows = d.quot;
    auto const& n_cols = d.rem;
    cur_x += n_cols;
    cur_y += n_rows;
    this->base_reference() += diffPerRow * n_rows + diffPerCol * n_cols;
}


template <typename T>
template <class OtherValue>
inline typename ChannelValueIterator<T>::iterator_adaptor_::difference_type
ChannelValueIterator<T>::distance_to(ChannelValueIterator<OtherValue> const& other) const {
    return (other.cur_y - cur_y) * width + other.cur_x - cur_x;
}


template <typename T>
inline void ChannelValueIterator<T>::setX(std::size_t x) {
    this->base_reference() += diffPerCol * (x - cur_x);
    cur_x = x;
}


template <typename T>
inline void ChannelValueIterator<T>::setY(std::size_t y) {
    this->base_reference() += diffPerRow * (y - cur_y);
    cur_y = y;
}


template <typename T>
inline std::size_t const& ChannelValueIterator<T>::getX() const {
    return cur_x;
}


template <typename T>
inline std::size_t const& ChannelValueIterator<T>::getY() const {
    return cur_y;
}

template <typename T>
inline Point ChannelValueIterator<T>::getPos() const {
    return Point{static_cast<int>(getX()),
                 static_cast<int>(getY())};
}

template <typename T>
inline void ChannelValueIterator<T>::setPos(Point const& pos) {
    setX(pos.x);
    setY(pos.y);
}

} /* namespace imagefusion */

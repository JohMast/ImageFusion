#pragma once

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <numeric>

#include <boost/exception/error_info.hpp>

#include "Image.h"
#include "exceptions.h"

namespace imagefusion {


/**
 * @brief Error information for the resolution tag of an element in a MultiResCollection
 *
 * Add to an exception `ex` with
 * @code
 * ex << errinfo_resolution_tag(tag);
 * @endcode
 * where `tag` is of type `std::string`. Get from a caught exception with
 * @code
 * std::string const* t = boost::get_error_info<errinfo_resolution_tag>(ex);
 * @endcode
 * Note, `ex` must be of sub type `boost::exception`, which holds for all imagefusion exceptions.
 *
 * \ingroup errinfo
 *
 * @see exceptions.h
 */
using errinfo_resolution_tag = boost::error_info<struct tag_resolution_tag, std::string>;


/**
 * @brief Error information for the date of an element in a MultiResCollection
 *
 * Add to an exception `ex` with
 * @code
 * ex << errinfo_date(date);
 * @endcode
 * where `date` is of type `int`. Get from a caught exception with
 * @code
   int const* t = boost::get_error_info<errinfo_date>(ex);
 * @endcode
 *
 * Note, `ex` must be of sub type `boost::exception`, which holds for all imagefusion exceptions.
 *
 * \ingroup errinfo
 *
 * @see exceptions.h
 */
using errinfo_date = boost::error_info<struct tag_date, int>;


/**
 * @brief Collection for elements that can be indexed by resolution and date
 *
 * This is the collection for the elements that are required as input for fusion. For image
 * elements, use MultiResImages. Every DataFusor requires a MultiResImages object.
 *
 * Note, assignment operator and copy constructor are defaulted. This means copying a
 * MultiResCollection will clone all the content.
 */
template<class T>
class MultiResCollection : public std::enable_shared_from_this<MultiResCollection<T>> {
public:
    /**
     * @brief Map type with date and element pairs of a single resolution
     */
    using date_map_t = std::map<int, T>;

    /**
     * @brief Map type with resolution string and date_map_t pairs
     */
    using res_map_t = std::map<std::string, date_map_t>;


    /**
     * @brief Virtual destructor for inheritance
     */
    virtual ~MultiResCollection() { }


    /**
     * @brief Check whether an element with specified resolution and date is available
     * @param res is the resolution tag.
     * @param date
     *
     * @return true if there exists an element with both the specified resolution and date in the
     * collection, otherwise false.
     */
    bool has(std::string const& res, int date) const;


    /**
     * @brief Get the element with the specified resolution and date
     * @param res is the resolution tag.
     * @param date
     *
     * @return aquired element.
     *
     * @throws not_found_error if the element does not exist. Existence can be checked with
     * has(std::string const& res, int date) const.
     */
    T& get(std::string const& res, int date);
    /// \copydoc get(std::string const& res, int date)
    T const& get(std::string const& res, int date) const;


    /**
     * @brief Get an arbitrary element with the specified date
     * @param date
     *
     * @return aquired element.
     *
     * @throws not_found_error if the element does not exist. Existence can be checked with
     * has(int date) const.
     */
    T& getAny(int date);
    /// \copydoc getAny(int date)
    T const& getAny(int date) const;


    /**
     * @brief Get an arbitrary element with the specified resolution
     * @param res is the resolution tag.
     *
     * @return arbitrary element with the specified resolution.
     *
     * @throws not_found_error if there is no element with resolution `res`. Existence can be
     * checked with has(std::string const& res) const.
     */
    T& getAny(std::string const& res);
    /// \copydoc getAny(std::string const& res)
    T const& getAny(std::string const& res) const;


    /**
     * @brief Returns an arbitrary element
     *
     * @return arbitrary element.
     *
     * @throws not_found_error if there is no element at all. You could check with count() const
     * before.
     */
    T& getAny();
    /// \copydoc getAny()
    T const& getAny() const;


    /**
     * @brief Save an element to the specified resolution and date
     * @param res is the resolution tag.
     * @param date
     * @param t is the element to set.
     *
     * This sets the element `t` (which can be moved in) in the collection to the specified date
     * and resolution. Existing elements will be replaced.
     *
     * @return freshly set element in the collection
     */
    T& set(std::string const& res, int date, T t);


    /**
     * @brief Remove element with the specified resolution and date
     * @param res is the resolution tag.
     * @param date
     *
     * @see has(std::string const&, int) const
     *
     * @throws not_found_error if the element cannot be found.
     */
    void remove(std::string const& res, int date);


    /**
     * @brief Remove all elements with the specified resolution
     * @param res is the resolution tag.
     *
     * @see has(std::string const&) const
     *
     * @throws not_found_error if there is no resolution tag `res`.
     */
    void remove(std::string const& res);


    /**
     * @brief Remove all elements with the specified date
     * @param date
     *
     * @see has(int) const
     *
     * @throws not_found_error if there is no resolution tag `res`.
     */
    void remove(int date);


    /**
     * @brief Check whether there is an element with the specified resolution
     * @param res is the resolution tag.
     *
     * @return true if the collection has got an element with the specified resolution, false
     * otherwise
     */
    bool has(std::string const& res) const;


    /**
     * @brief Check whether there is an element at the specified date
     * @param date
     *
     * @return true if the collection has got an element with the specified date, false otherwise.
     */
    bool has(int date) const;


    /**
     * @brief Get all non-empty resolution tags
     *
     * All resolution tags that have an element are collected in a vector.
     *
     * @return resolution tags as vector.
     */
    std::vector<std::string> getResolutionTags() const;


    /**
     * @brief Get all resolution tags that have an element at the specified date
     * @param date
     * @return all resolution tags that have an element at the specified date as vector.
     */
    std::vector<std::string> getResolutionTags(int date) const;


    /**
     * @brief Get all available dates of a specified resolution
     * @param res is the resolution tag.
     *
     * Finds all dates with which an element is associated in the specified resolution. The dates
     * are sorted.
     *
     * @return all dates as vector.
     */
    std::vector<int> getDates(std::string const& res) const;


    /**
     * @brief Get all available dates
     *
     * Finds all dates as the union of the set of dates for each resolution.
     *
     * @return all dates as set.
     */
    std::set<int> getDates() const;


    /**
     * @brief Get the number of non-empty resolution tags
     * @return number.
     */
    std::size_t countResolutionTags() const;


    /**
     * @brief Get the total number of elements
     * @return number of elements of all resolutions in the collection.
     */
    std::size_t count() const;


    /**
     * @brief Get the total number of elements of a specified resolution
     * @param res is the resolution tag.
     * @return number of elements of a specified resolution in the collection.
     */
    std::size_t count(std::string const& res) const;


    /**
     * @brief Get the total number of elements at a specified date
     * @param date
     * @return number of elements at a specified date in the collection.
     */
    std::size_t count(int date) const;


    /**
     * @brief Check whether the collection is empty
     *
     * @return true if there is no element in the collection, false otherwise.
     */
    bool empty() const;


protected:
    res_map_t collection;
};



/**
 * @brief The MultiResImages class
 *
 * Note, an empty image (zero-sized) counts as existing. This could be misleading for empty(),
 * which returns false, if there is at least one (possibly empty) Image in the collection.
 */
class MultiResImages : public MultiResCollection<Image> {
public:
    /**
     * @brief Clone this collection, but with shared copies of the images
     *
     * This clones the MultiResImages collection, which means, that creating or removing something
     * in the new collection will not affect the original and vice versa. However, the images in
     * the new collection are shared copies of their originals and thus changing pixel values will
     * affect the original images and vice versa. As usual with shared copies of images cropping is
     * independent.
     *
     * This is comparible to a flat copy and similarly cheap.
     *
     * @return a new collection filled with shared copies of the original images.
     */
    MultiResImages cloneWithSharedImageCopies();


    /**
     * @brief Clone this collection with clones of all images
     *
     * This clones the complete collection. This gives a new collection that is filled with clones
     * of the original collection. So the new collection will be completely independent from the
     * original one.
     *
     * This operation is very expensive.
     *
     * Note, this method does the same as the assignment operator and copy constructor.
     *
     * @return cloned collection.
     */
    MultiResImages cloneWithClonedImages() const;
};




inline MultiResImages MultiResImages::cloneWithClonedImages() const {
    return MultiResImages{*this};
}


inline MultiResImages MultiResImages::cloneWithSharedImageCopies() {
    MultiResImages ret;
    for (auto& map_p : collection) {
        // create a new map with the resolution specifier string
        auto& indexed_images = ret.collection[map_p.first];
        for (auto& image_p : map_p.second) {
            // insert a shared copy of the image
            indexed_images.emplace_hint(indexed_images.end(),
                                        image_p.first,
                                        image_p.second.sharedCopy());
        }
    }
    return ret;
}


template<class T>
inline bool MultiResCollection<T>::has(std::string const& res) const {
    auto res_map_it = collection.find(res);
    return res_map_it != collection.end();
}


template<class T>
inline std::size_t MultiResCollection<T>::countResolutionTags() const {
    return collection.size();
}


template<class T>
inline bool MultiResCollection<T>::empty() const {
    return collection.empty();
}

template<class T>
inline bool MultiResCollection<T>::has(std::string const& res, int date) const {
    auto res_map_it = collection.find(res);
    if (res_map_it == collection.end())
        return false;

    date_map_t const& res_map = res_map_it->second;
    auto it = res_map.find(date);
    return it != res_map.end();
}

template<class T>
inline bool MultiResCollection<T>::has(int date) const {
    for (auto const& res_map_p : collection) {
        date_map_t const& res_map = res_map_p.second;
        auto it = res_map.find(date);
        if (it != res_map.end())
            return true;
    }
    return false;
}

template<class T>
inline T const& MultiResCollection<T>::get(std::string const& res, int date) const {
    auto res_map_it = collection.find(res);
    if (res_map_it != collection.end()) {
        date_map_t const& res_map = res_map_it->second;
        auto it = res_map.find(date);
        if (it != res_map.end())
            return it->second;
    }

    IF_THROW_EXCEPTION(not_found_error(
                          "Could not find the requested element of " + res + " resolution with date "
                          + std::to_string(date) + ". Please call MultiResCollection::has before!"))
            << errinfo_resolution_tag(res) << errinfo_date(date);
}

template<class T>
inline T& MultiResCollection<T>::get(std::string const& res, int date) {
    return const_cast<T&>(static_cast<MultiResCollection<T> const*>(this)->get(res, date));
}



template<class T>
inline T const& MultiResCollection<T>::getAny(int date) const {
    for (auto const& res_map_p : collection) {
        date_map_t const& res_map = res_map_p.second;
        auto it = res_map.find(date);
        if (it != res_map.end())
            return it->second;
    }

    IF_THROW_EXCEPTION(not_found_error(
                          "Could not find any element with date "
                          + std::to_string(date) + ". Please call MultiResCollection::has before!"))
            << errinfo_date(date);
}

template<class T>
inline T& MultiResCollection<T>::getAny(int date) {
    return const_cast<T&>(static_cast<MultiResCollection<T> const*>(this)->getAny(date));
}



template<class T>
inline T const& MultiResCollection<T>::getAny(std::string const& res) const {
    auto res_map_it = collection.find(res);
    if (res_map_it != collection.end() && !res_map_it->second.empty()) {
        auto first_it = res_map_it->second.begin();
        return first_it->second;
    }

    IF_THROW_EXCEPTION(not_found_error(
                           "There is no element of  " + res + " to get. Please call "
                           "MultiResCollection::has(std::string) or MultiResCollection::count(std::string) before!"))
            << errinfo_resolution_tag(res);
}

template<class T>
inline T& MultiResCollection<T>::getAny(std::string const& res) {
    return const_cast<T&>(static_cast<MultiResCollection<T> const*>(this)->getAny(res));
}



template<class T>
inline T const& MultiResCollection<T>::getAny() const {
    if (!collection.empty()) {
        auto first_res_it = collection.begin();
        if (!first_res_it->second.empty()) {
            auto first_it = first_res_it->second.begin();
            return first_it->second;
        }
    }

    IF_THROW_EXCEPTION(not_found_error("There is no element to get. Please call MultiResCollection::count before!"));
}

template<class T>
inline T& MultiResCollection<T>::getAny() {
    return const_cast<T&>(static_cast<MultiResCollection<T> const*>(this)->getAny());
}


template<class T>
inline T& MultiResCollection<T>::set(std::string const& res, int date, T t) {
    date_map_t& res_map = collection[res];
    return res_map[date] = std::move(t);
}


template<class T>
inline void MultiResCollection<T>::remove(std::string const& res, int date) {
    auto res_map_it = collection.find(res);
    if (res_map_it != collection.end()) {
        date_map_t& res_map = res_map_it->second;
        auto it = res_map.find(date);
        if (it != res_map.end()) {
            res_map.erase(it);

            // remove res tag if empty
            if (res_map.empty())
                collection.erase(res_map_it);
            return;
        }
    }

    IF_THROW_EXCEPTION(not_found_error("Could not find the requested element of " + res + " resolution with date "
                                       + std::to_string(date) + ". Please call MultiResCollection::has before!"))
            << errinfo_resolution_tag(res) << errinfo_date(date);
}


template<class T>
inline void MultiResCollection<T>::remove(std::string const& res) {
    auto res_map_it = collection.find(res);
    if (res_map_it != collection.end()) {
        collection.erase(res_map_it);
        return;
    }

    IF_THROW_EXCEPTION(not_found_error("Could not find the requested resolution " + res + ". "
                                       "Please call MultiResCollection::has before!"))
            << errinfo_resolution_tag(res);
}


template<class T>
inline void MultiResCollection<T>::remove(int date) {
    for (auto res_map_it = collection.begin(), res_map_it_end = collection.end();
         res_map_it != res_map_it_end; ++res_map_it)
    {
        date_map_t& res_map = res_map_it->second;
        auto it = res_map.find(date);
        if (it != res_map.end()) {
            res_map.erase(it);

            // remove res tag if empty
            if (res_map.empty())
                collection.erase(res_map_it);
        }
    }
}


template<class T>
inline std::vector<std::string> MultiResCollection<T>::getResolutionTags() const {
    std::vector<std::string> tags;
    tags.reserve(collection.size());
    for (auto const& map_p : collection)
        tags.push_back(map_p.first);
    return tags;
}


template<class T>
inline std::vector<std::string> MultiResCollection<T>::getResolutionTags(int date) const {
    std::vector<std::string> tags;
    for (auto const& res_map_p : collection) {
        date_map_t const& res_map = res_map_p.second;
        auto it = res_map.find(date);
        if (it != res_map.end())
           tags.push_back(res_map_p.first);
    }
    return tags;
}


template<class T>
inline std::vector<int> MultiResCollection<T>::getDates(std::string const& res) const {
    std::vector<int> dates;
    auto res_map_it = collection.find(res);
    if (res_map_it != collection.end()) {
        dates.reserve(res_map_it->second.size());
        for (auto const& date_elem : res_map_it->second)
            dates.push_back(date_elem.first);
    }
    return dates;
}


template<class T>
inline std::set<int> MultiResCollection<T>::getDates() const {
    std::set<int> dates;
    for (auto const& res_map_p : collection)
        for (auto const& date_elem : res_map_p.second)
            dates.insert(date_elem.first);
    return dates;
}


template<class T>
inline std::size_t MultiResCollection<T>::count() const {
    return std::accumulate(collection.cbegin(), collection.cend(), (std::size_t)0,
                           [] (std::size_t const& sum, typename res_map_t::value_type const& m_p) {
                               return sum + m_p.second.size();
                           } );
}


template<class T>
inline std::size_t MultiResCollection<T>::count(std::string const& res) const {
    auto res_map_it = collection.find(res);
    if (res_map_it != collection.end())
        return res_map_it->second.size();
    return 0;
}


template<class T>
inline std::size_t MultiResCollection<T>::count(int date) const {
    std::size_t count = 0;
    for (auto const& res_map_p : collection) {
        date_map_t const& res_map = res_map_p.second;
        auto it = res_map.find(date);
        if (it != res_map.end())
            ++count;
    }
    return count;
}


} /* namespace imagefusion */

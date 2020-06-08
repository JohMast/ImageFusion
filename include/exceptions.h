#pragma once

#include <exception>
#include <stdexcept>
#include <boost/exception/all.hpp>

namespace imagefusion {

/**
 * \defgroup errinfo Error Information
 *
 * @brief Error information to enhance exceptions at any stage in call stack
 *
 * The error information types explained in this module can be used to enhance an exception object
 * with additional information. This works not only directly at the throw level, but also at higher
 * levels in the call stack. They can be caught as `boost::exception&`, then the error information
 * can be added with the operator << and finally the exception can be rethrown with `throw`. Look
 * at this example:
 * @code
 * void addImage(imagefusion::MultiResImages& imgs, std::string tag, int date, std::string imageFilename) {
 *     using namespace imagefusion;
 *     try {
 *         imgs.set(tag, date, Image{imageFilename});
 *     }
 *     catch(boost::exception& ex) {
 *         ex << errinfo_resolution_tag(tag) << errinfo_date(date);
 *         throw;
 *     }
 * }
 * @endcode
 *
 * If the constructor of Image threw an exception, it could not know, for which date and tag the
 * Image was supposed. Thus the error information could not be added when throwing. The
 * [boost::exception](http://www.boost.org/doc/libs/release/libs/exception/) class allows for
 * adding it later. So we can add this information and rethrow. This does not handle the exception
 * so it is kinda exception neutral.
 *
 * Finally, at user side where the exception is eventually caught, the accumulated error info can
 * be accessed as pointer to the original type, e.g.
 * @code
 * std::string const* tag = boost::get_error_info<errinfo_resolution_tag>(ex)
 * int const* date        = boost::get_error_info<errinfo_date>(ex)
 * @endcode
 * where `ex` is the caught exception. This returns a `nullptr`, if the info is not available, see
 * [boost documentation](http://www.boost.org/doc/libs/release/libs/exception/doc/tutorial_transporting_data.html).
 * If you just want to print all available error information as string, use
 * @code
 * std::cerr << diagnostic_information(ex) << std::endl;
 * @endcode
 *
 * Please also note that boost already defines some general error information types as e. g.
 * [boost::errinfo_file_name](http://www.boost.org/doc/libs/release/libs/exception/doc/errinfo_file_name.html),
 * which you can use.
 */


/** @file
 *  @brief Exception types and error information types
 *
 * This file defines all [exception types](@ref error) and includes the documentation of the
 * [error information types](@ref errinfo). Additionally, it also defines a helper macro @ref
 * IF_THROW_EXCEPTION to simplify to throw an exception with added source file and line and
 * function name.
 */

/**
 * \defgroup error Exception Types
 *
 * @brief Imagefusion exception types
 *
 * All exception defined in imagefusion inherit from `boost::exception` to be able to add arbitrary
 * error information also at higher levels in the call stack.
 *
 * There are different kinds of error info types available:
 *  - [errinfo_image_type(Type)](@ref imagefusion::errinfo_image_type)                in Type.h
 *  - [errinfo_size(Size)](@ref imagefusion::errinfo_size)                            in Image.h
 *  - [errinfo_resolution_tag(std::string)](@ref imagefusion::errinfo_resolution_tag) in MultiResImages.h
 *  - [errinfo_date(int)](@ref imagefusion::errinfo_date)                             in MultiResImages.h
 *  - [errinfo_file_format(std::string)](@ref imagefusion::errinfo_file_format)       in fileformat.h
 *
 * These are explained in more detail in @ref errinfo.
 *
 * The exceptions are structured in a hierarchy as follows:
 * \dotfile exceptions.dot
 *
 * @{
 */


/**
 * @brief Unrecoverable error
 *
 * Throw this if an unrecoverable error appeared. This could also be done with an `assert` or
 * simply `std::exit()`, but error information could be added. So if the user program installs an
 * uncaught exception handler, it could print out some useful information maybe, that helps the
 * library maintainer to fix the issue.
 *
 * @see exceptions.h, errinfo
 */
struct logic_error : public virtual std::logic_error, virtual boost::exception {
    // respecify all constructors, since inheriting constructors with using std::logic_error::logic_error does not work from a virtual base class in gcc and msvc++
    explicit logic_error(std::string const& what_arg) : std::logic_error(what_arg) { }
    explicit logic_error(const char* what_arg)        : std::logic_error(what_arg) { }
};

/**
 * @brief Base class for more specific exceptions
 *
 * This class is useful to catch all kind of runtime exceptions that appeared from imagefusion.
 *
 * @see exceptions.h, errinfo
 */
struct runtime_error : public virtual std::runtime_error, virtual boost::exception {
    // respecify all constructors, since inheriting constructors with using std::runtime_error::runtime_error does not work from a virtual base class in gcc and msvc++
    explicit runtime_error(std::string const& what_arg) : std::runtime_error(what_arg) { }
    explicit runtime_error(const char* what_arg)        : std::runtime_error(what_arg) { }
};

/**
 * @brief The type of an image caused an error.
 *
 * @see exceptions.h, errinfo
 */
struct image_type_error : public virtual runtime_error {
    // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
    explicit image_type_error(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
    explicit image_type_error(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
};

/**
 * @brief The file format (GDAL driver) of an image caused an error.
 *
 * @see exceptions.h, errinfo
 */
struct file_format_error : public virtual runtime_error {
    // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
    explicit file_format_error(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
    explicit file_format_error(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
};

/**
 * @brief A feature which is planned, but not implemented yet can throw this.
 *
 * @see exceptions.h, errinfo
 */
struct not_implemented_error : public virtual runtime_error {
    // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
    explicit not_implemented_error(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
    explicit not_implemented_error(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
};

/**
 * @brief An inappropriate size caused an error.
 *
 * @see exceptions.h, errinfo
 */
struct size_error : public virtual runtime_error {
    // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
    explicit size_error(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
    explicit size_error(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
};

/**
 * @brief Something could not be found, e.g. an image in MultiResImages.
 *
 * @see exceptions.h, errinfo
 */
struct not_found_error : public virtual runtime_error {
    // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
    explicit not_found_error(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
    explicit not_found_error(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
};


/**
 * @brief Something could not be parsed or is out of scope, e. g. a wrong command line option.
 *
 * @see exceptions.h, errinfo
 */
struct invalid_argument_error : public virtual runtime_error {
    // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
    explicit invalid_argument_error(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
    explicit invalid_argument_error(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
};


/** @} */ /* group error */


#ifdef WITH_OMP
/**
 * @brief The ThreadExceptionHelper escorts one exception thrown by a thread
 *
 * This class is quite internal and users probably do not need to deal with it.
 *
 * The Parallelizer spawns multiple threads via OpenMP to exceute the underlying data fusors in
 * parallel. If one or more of them throw an exception, this must be handled within the parallel
 * section. However, the parallelizer cannot handle the error. So it tries its best and forwards
 * the last thrown exception to the caller of the Parallelizer's predict method. If multiple
 * exceptions are thrown (e. g. one per data fusor), the first ones are ignored.
 *
 * This class is taken from http://stackoverflow.com/q/11828539/2414411 where it is discussed
 * extensively. For the usage in imagefusion have a look in Parallelizer::predict.
 */
class ThreadExceptionHelper {

    std::exception_ptr ePtr;

    void captureException() {
        #pragma omp critical (exception_catching)
            this->ePtr = std::current_exception();
    }

public:
    /// \brief Default constructor
    ThreadExceptionHelper() : ePtr(nullptr) {}


    /// \brief Rethrow the last caught exception, if caught one
    void rethrow() {
        if (this->ePtr)
            std::rethrow_exception(this->ePtr);
    }

    /// \brief Try to run the code and catch anything to rethrow later on
    template <typename Function, typename... Parameters>
    void run(Function f, Parameters... params) {
        try {
            f(params...);
        }
        catch (...) {
            captureException();
        }
    }
};
#endif /* WITH_OMP */


} /* namespace imagefusion */

/**
 * @brief Throw an exception and add information where it has been thrown
 *
 * A usual `throw some_exception_class` call can be replaced by
 * `IF_THROW_EXCEPTION(some_exception_class)`, which automatically adds the
 * [error information](@ref errinfo) about the function name, filename and line number where it was
 * thrown. The `IF` in `IF_THROW_EXCEPTION` stands for ImageFusion. Use this macro only for
 * [imagefusion exceptions](@ref error) (also any exceptions inherited from `boost::exception` are
 * fine).
 */
#if defined(__GNUC__) && (__GNUC__ < 5) // workaround for bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55425
#define IF_THROW_EXCEPTION(ex) \
    throw ex \
    << boost::throw_file(__FILE__) \
    << boost::throw_line((int)__LINE__)
#else
#define IF_THROW_EXCEPTION(ex) \
    throw ex \
    << boost::throw_function(BOOST_THROW_EXCEPTION_CURRENT_FUNCTION) \
    << boost::throw_file(__FILE__) \
    << boost::throw_line((int)__LINE__)
#endif /* defined(__GNUC__) && (__GNUC__ < 5) */

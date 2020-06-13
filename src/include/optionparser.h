#pragma once

#include "imagefusion.h"
#include "exceptions.h"
#include "Image.h"
#include "MultiResImages.h"

#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <type_traits>
#include <functional>

namespace imagefusion {

/**
 * \defgroup optionparser Command line option parser
 **/


 /**
 * @brief %This namespace contains everything for the option parser framework
 *
 * This namespace contains all classes and functions of the option parser framework. With it, you
 * can parse command line options and option files. For a detailed introduction with a full working
 * example and option syntax see module @ref optionparser. The single classes and functions are
 * also documented in detail, have a look!
 *
 * \ingroup optionparser
 */
namespace option {

/**
 * \addtogroup optionparser
 *
 * @brief %Option parser for command line arguments and option files.
 *
 * This option parser (originating from The Lean Mean C++ %Option Parser) handles program's command
 * line arguments (argc, argv). It supports short and long option formats, nested arguments and it
 * provides functions to check arguments for correctness. All this has a convenient interface to
 * make writing utilities easier. The individual classes are described in the namespace
 * imagefusion::option. A full working example is shown below. First, here are some highlights
 * shown with small code fragments:
 * <ul style="padding-left:1em;margin-left:0">
 * <li> The framework provides a usage message formatter that supports column alignment and line
 *      wrapping. You can go to the next column with `\t` and to the next row (without changing the
 *      column) with `\v`.
 *      @code printUsage(usage); @endcode
 *      where usage is a vector of Descriptor%s, which defines not only your options, but also the
 *      usage text.
 * <li> You can loop through options sequentially or in a grouped fashion:
 *      <ul style="margin-top:.5em">
 *      <li> Test for presence of an option in the argument vector:
 *           @code if (!options["QUIET"].empty()) ... @endcode
 *      <li> Get the argument of the last option of a kind:
 *           @code
 *           if (!options["NUMBER"].empty()) {
 *               std::string& arg = options["NUMBER"].back().arg;
 *               int n = Parse::Int(arg);
 *               ...
 *           }
 *           @endcode
 *      <li> Evaluate an `--enable-foo` / `--disable-foo` pair where the last one used wins:
 *           @code
 *           if (!options["FOO"].empty()) {
 *               if(options["FOO"].back().prop() == "ENABLE")
 *                   ...
 *               else // disable
 *                   ...
 *           }
 *           @endcode
 *           Have a look at the example at @ref Descriptor.
 *      <li> Cumulative option (`-v` verbose, `-vv` more verbose, `-vvv` even more verbose):
 *           @code int verbosity = options["VERBOSE"].size(); @endcode
 *      <li> Iterate over all `--file=<fname>` arguments:
 *           @code
 *           for (auto& opt : options["FILE"]) {
 *               std::string& fname = opt.arg;
 *               ...
 *           }
 *           @endcode
 *     <li> You can loop through unknown (unrecognized / unspecified) options, when set to collect
 *          them rather than throwing an error:
 *          @code
 *          for (auto& opt : options.unknown) {
 *              ...
 *          }
 *          @endcode
 *          Look at OptionParser::unknownOptionArgCheck for the setting.
 *     <li> You can loop through non-option arguments, like when calling your utility (called
 *          'executable' in the following example) with
 *          @code
 *          executable  argument  --opt  more  arguments
 *          @endcode
 *          the following code
 *          @code
 *          for (auto& opt : options.nonOptionArgs) {
 *              std::cout << opt.arg << std::endl;
 *          }
 *          @endcode
 *          would output
 *          @code
 *          argument
 *          more
 *          arguments
 *          @endcode
 *          This assumes `--opt` does not accept an argument.
 *     <li> If you really want to, you can still process some or all options in the order they were
 *          given on command line:
 *          @code
 *          for (auto& opt : options.input) {
 *              if (opt.spec() == "NUMBER") {
 *                  int n = Parse::Int(opt.arg);
 *                  ...
 *              }
 *              else if (opt.spec() == "FILE") {
 *                  std::string& fname = opt.arg;
 *                  ...
 *              }
 *              ...
 *          }
 *          @endcode
 *     </ul>
 * <li> A general (optional) pseudo option `--option-file=<file>` is available to specify files
 *      that contain options. The `--option-file=<file>` part in the parameter list is just
 *      replaced by the contents of the file, making all your options directly work in such a file.
 *      Line comments with # and line breaks between options are also allowed. See @ref
 *      ArgumentToken for examples.
 * <li> You can use quoting with single quotes '...', double quotes "..." or parenthesis pairs
 *      (...) to keep an argument together and preserve whitespace. These can even be nested and
 *      mixed to make really powerful options, like
 *      @code --image="-f 'test image.tif' --crop=(-x 1 -y 2 -w 3 -h 2)" @endcode
 *      Note, that bash will try to interpret these quotes. So the outer quoting must be made with
 *      single or double quotes on bash, but can be a parentheses pair in an option file. See @ref
 *      ArgumentToken for examples.
 * <li> Parsing and argument checking functions for all important types of imagefusion are
 *      available. For example the above image option argument could be parsed with
 *      @code Image i = Parse::Image(opt.arg); @endcode
 *      and yield the cropped image.
 * <li> On parsing failure an invalid_argument_error with a good error message is thrown. This will
 *      give the user by default a good hint, where the error occured. However, unknown options can
 *      be collected, when desired. For that see OptionParser::unknownOptionArgCheck.
 * </ul>
 *
 *
 * @par Full working example program:
 *
 * @code
 * #include <iostream>
 * #include "optionparser.h"
 *
 * using Descriptor = imagefusion::option::Descriptor;
 * using ArgChecker = imagefusion::option::ArgChecker;
 * using Parse      = imagefusion::option::Parse;
 *
 * std::vector<Descriptor> usage{
 *     Descriptor::text("Usage: example [options]\n\n"
 *                      "Options:"),
 *     //   ID, prop, short,   long, argument checking, help text
 *     {"HELP",   "",   "h", "help",  ArgChecker::None, "  -h, \t--help  \tPrint usage and exit." },
 *     {"PLUS",   "",   "p", "plus",   ArgChecker::Int, "  -p <num>, \t--plus=<num>  \tAdd to sum." },
 *     Descriptor::optfile(),
 *     {"SIZE",   "",   "s", "size",  ArgChecker::Size, "  -s <size>, \t--size=<size>  \tMultiply and add area to sum.\v"
 *                                                      "<size> either receives the following arguments:\v"
 *                                                      "  -w <num>, --width=<num>  width\v"
 *                                                      "  -h <num>, --height=<num> height\v"
 *                                                      "or must have the form '<num>x<num>' or just '(<num> <num>)', "
 *                                                      "both with optional spacing, where the first argument is the "
 *                                                      "width and the second is the height." },
 *     Descriptor::text("\nExamples:\n"
 *                      "  example -- --this_is_no_option\n"
 *                      "  example -p1 --plus=2 --size=1x10 -p 3 --plus 4 'file 1' file2\n")
 * };
 *
 * int main(int argc, char* argv[])
 * {
 *     auto options = OptionParser::parse(usage, argc, argv);
 *
 *     if (!options["HELP"].empty() || argc == 1) {
 *         printUsage(usage);
 *         return 0;
 *     }
 *
 *     int sum = 0;
 *     for (auto& o : options["PLUS"])
 *         sum += Parse::Int(o.arg);
 *
 *     for (auto& o : options["SIZE"]) {
 *         imagefusion::Size s = Parse::Size(o.arg);
 *         sum += s.width * s.height;
 *     }
 *     std::cout << "Sum: " << sum << std::endl;
 *
 *     for (std::string& nop : options.nonOptionArgs)
 *         std::cout << "Non-option: " << nop << std::endl;
 *
 *     return 0;
 * }
 * @endcode
 *
 * Example input and output on bash:
 *
 *     $ ./example -- --this_is_no_option
 *     Sum: 0
 *     Non-option: this_is_no_option
 *
 *     $ ./example -p1 --plus=2 --size=1x10 -p 3 --plus 4 'file 1' file2
 *     Sum: 20
 *     Non-option: file 1
 *     Non-option: file2
 *
 * @par Option syntax:
 * @li This option parser follows POSIX <code>getopt()</code> conventions and supports GNU-style
 *     <code>getopt_long()</code> long options as well as Perl-style single-dash long options
 *     (<code>getopt_long_only()</code>).
 * @li Short options have the format `-X` where `X` is any character that fits in a char.
 * @li Short options can be grouped, i.e. `-X -Y` is equivalent to `-XY`.
 * @li A short option may take an argument either separate (`-X foo`) or attached (`-Xfoo`). You
 *     can make the parser accept the additional format `-X=foo` by registering `X` as a long
 *     option (in addition to being a short option) and enabling single-dash long options.
 * @li An argument-taking short option may be grouped if it is the last in the group, e.g.
 *     `-ABCXfoo` or `-ABCX foo` (`foo` is the argument to the `-X` option).
 * @li A lone dash character `-` is not treated as an option. It is customarily used where a
 *     filename is expected to refer to stdin or stdout.
 * @li Long options have the format `--option-name`.
 * @li The option-name of a long option can be anything and include any characters.
 * @li [optional] Long options may be abbreviated as long as the abbreviation is unambiguous. You
 *     can set a minimum length for abbreviations. See OptionParser::minAbbrevLen.
 * @li [optional] Long options may begin with a single dash. The double dash form is always
 *     accepted, too. See OptionParser::singleDashLongopt.
 * @li A long option may take an argument either separate (`--option arg`) or attached
 *     (`--option=arg`). In the attached form the equals sign is mandatory.
 * @li An empty string marked as quote, like '' or "", is permitted as separate argument to both
 *     long and short options.
 * @li Arguments to both short and long options may start with a `-` character. E.g. `-X-X`,
 *     `-X -X` or `--long-X=-X`. If `-X` and `--long-X` take an argument, that argument will be
 *     `"-X"` in all 3 cases.
 * @li Arguments that start with a double dash `--` must not be detached. E.g. `-X--X` and
 *     `--long-X=--X` will receive `"--X"` as argument, but `-X --X` and `--long-X --X` won't.
 * @li If using the built-in ArgChecker::Optional, option arguments are optional, but expected. So
 *     anything following not starting with a double dash will be used as argument.
 * @li The special option `--` (i.e. without a name) terminates the list of options. Everything
 *     that follows is a non-option argument, even if it starts with a `-` character. The `--`
 *     itself will not appear in the parse results.
 * @li The first argument that doesn't start with `-` or `--` and does not belong to a preceding
 *     argument-taking option, will terminate the option list and is the first non-option argument.
 *     All following command line arguments are treated as non-option arguments, even if they start
 *     with `-`. This behaviour can be changed, see OptionParser::acceptsOptAfterNonOpts.
 * @li Arguments that look like options (i.e. `-` followed by at least 1 character) but aren't, are
 *     NOT treated as non-option arguments. They are treated as unknown options and by default
 *     throw an exception. See OptionParser::unknownOptionArgCheck for information on how to
 *     collect unknown options without throwing. This means that in order to pass a first
 *     non-option argument beginning with the dash character it is required to use the `--` special
 *     option, e.g.
 *     @code
 *     program -x -- --strange-filename
 *     @endcode
 *     In this example, `--strange-filename` is a non-option argument. If the `--` were omitted, it
 *     would be treated as an unknown option. Note that `--` stops interpreting arguments as
 *     options even if `OptionParser::acceptsOptAfterNonOpts` is set to false.
 * @li Arguments are separated by any whitespace, so newlines are accepted as well.
 * @li The `#` char is interpreted as line comment start, even in a string. This is especially
 *     useful in option files, see ArgumentToken. To make a `#` char, use `\#`.
 */

/**
 * @brief Get the number of columns that fit in your current terminal
 *
 * This small helper function tries to find out how wide your terminal is, i. e. how many chars fit
 * into one line.
 *
 * This is currently implemented for linux and windows. On mac it will just return 80. If someone
 * can implement and test that for mac, you are welcome!
 *
 * @return number of columns that fit into the terminal.
 *
 * @note This function is rather internal. You probably don't need to use it.
 */
int getTerminalColumns();


/**
 * @brief The ImageInput struct is used as return type for Parse::MRImage
 *
 * It just contains an image, a date and a resolution tag. These information can be used to set an
 * Image in a MultiResImages object. Remember to use `std::move()` to get the image out of this.
 */
struct ImageInput {
    /// corresponding date
    int date;
    /// Image, parsed from file
    Image i;
    /// corresponding resolution tag
    std::string tag;
};

struct Descriptor;


/**
 * @brief Parse is a collection of static methods that parse objects from a string.
 *
 * Since these methods are in a class, you can add your own methods to parse custom objects. To do
 * this, you can inherit from Parse, like shown in the following
 * @code
 * struct MyParse : public imagefusion::option::Parse {
 *     static MyClass myclass(std::string const& str);
 * };
 * @endcode
 * With this pattern, you only need `MyParse` for all of your parsing. If you do not want to add
 * own methods, you can use a `using` declaration to not have to write the namespace when using
 * Parse:
 * @code
 * using Parse = imagefusion::option::Parse;
 * @endcode
 */
struct Parse {
    /**
     * @brief Specifies the sub-options for Size
     *
     * This specifies
     * Specifier | Short options | Long options  | Argument checker
     * --------- | ------------- | ------------- | ----------------
     * "WIDTH"   | "w"           | "w", "width"  | ArgChecker::Int
     * "HEIGHT"  | "h"           | "h", "height" | ArgChecker::Int
     *
     * This is the default usage vector for Parse::Size() and for Parse::SizeSubopts().
     */
    static const std::vector<Descriptor> usageSize;

    /**
     * @brief Specifies the sub-options for Dimensions
     *
     * This specifies
     * Specifier | Short options | Long options  | Argument checker
     * --------- | ------------- | ------------- | ----------------
     * "WIDTH"   | "w"           | "w", "width"  | ArgChecker::Float
     * "HEIGHT"  | "h"           | "h", "height" | ArgChecker::Float
     *
     * This is the default usage vector for Parse::Dimensions() and for Parse::DimensionsSubopts().
     */
    static const std::vector<Descriptor> usageDimensions;

    /**
     * @brief Specifies the sub-options for Point
     *
     * This specifies
     * Specifier | Short options | Long options  | Argument checker
     * --------- | ------------- | ------------- | ----------------
     * "X"       | "x"           | "x"           | ArgChecker::Int
     * "Y"       | "y"           | "y"           | ArgChecker::Int
     *
     * This is the default usage vector for Parse::Point() and for Parse::PointSubopts().
     */
    static const std::vector<Descriptor> usagePoint;

    /**
     * @brief Specifies the sub-options for Coordinate
     *
     * This specifies
     * Specifier | Short options | Long options  | Argument checker
     * --------- | ------------- | ------------- | ----------------
     * "X"       | "x"           | "x"           | ArgChecker::Float
     * "Y"       | "y"           | "y"           | ArgChecker::Float
     *
     * This is the default usage vector for Parse::Coordinate() and for Parse::CoordinateSubopts().
     */
    static const std::vector<Descriptor> usageCoordinate;

    /**
     * @brief Specifies the sub-options for Rectangle
     *
     * This specifies
     * Specifier | Short options | Long options  | Argument checker
     * --------- | ------------- | ------------- | ----------------
     * "X"       | "x"           | "x"           | ArgChecker::Vector<int>
     * "Y"       | "y"           | "y"           | ArgChecker::Vector<int>
     * "WIDTH"   | "w"           | "w", "width"  | ArgChecker::Int
     * "HEIGHT"  | "h"           | "h", "height" | ArgChecker::Int
     * "CENTER"  | "c"           | "c", "center" | ArgChecker::Vector<double>
     *
     * This is the default usage vector for Parse::Rectangle().
     */
    static const std::vector<Descriptor> usageRectangle;

    /**
     * @brief Specifies the sub-options for CoordRectangle
     *
     * This specifies
     * Specifier | Short options | Long options  | Argument checker
     * --------- | ------------- | ------------- | ----------------
     * "X"       | "x"           | "x"           | ArgChecker::Vector<double>
     * "Y"       | "y"           | "y"           | ArgChecker::Vector<double>
     * "WIDTH"   | "w"           | "w", "width"  | ArgChecker::Float
     * "HEIGHT"  | "h"           | "h", "height" | ArgChecker::Float
     * "CENTER"  | "c"           | "c", "center" | ArgChecker::Vector<double>
     *
     * This is the default usage vector for Parse::CoordRectangle().
     */
    static const std::vector<Descriptor> usageCoordRectangle;

    /**
     * @brief Specifies the sub-options for Image
     *
     * This specifies
     * Specifier | Property  | Short options | Long options              | Argument checker
     * --------- | --------- | ------------- | ------------------------- | ----------------
     * "FILE"    |           | "f"           | "file"                    | ArgChecker::File
     * "LAYERS"  |           | "l"           | "layers"                  | ArgChecker::Vector<int>
     * "CROP"    |           | "c"           | "crop"                    | ArgChecker::Rectangle
     * "COLTAB"  | "DISABLE" |               | "disable-use-color-table" | ArgChecker::None
     * "COLTAB"  | "ENABLE"  |               | "enable-use-color-table"  | ArgChecker::None
     *
     * This is the default usage vector for Parse::Image().
     */
    static const std::vector<Descriptor> usageImage;

    /**
     * @brief Specifies the sub-options for multi-res images
     *
     * This specifies
     * Specifier | Property  | Short options | Long options              | Argument checker
     * --------- | --------- | ------------- | ------------------------- | ----------------
     * "FILE"    |           | "f"           | "file"                    | ArgChecker::File
     * "DATE"    |           | "d"           | "date"                    | ArgChecker::int
     * "TAG"     |           | "t"           | "tag"                     | ArgChecker::NonEmpty
     * "LAYERS"  |           | "l"           | "layers"                  | ArgChecker::Vector<int>
     * "CROP"    |           | "c"           | "crop"                    | ArgChecker::Rectangle
     * "COLTAB"  | "DISABLE" |               | "disable-use-color-table" | ArgChecker::None
     * "COLTAB"  | "ENABLE"  |               | "enable-use-color-table"  | ArgChecker::None
     *
     * This is the default usage vector for Parse::MRImage().
     */
    static const std::vector<Descriptor> usageMRImage;

    /**
     * @brief Specifies the sub-options for Mask
     *
     * This specifies
     * Specifier | Property  | Short options | Long options              | Argument checker
     * --------- | --------- | ------------- | ------------------------- | ----------------
     * "FILE"    |           | "f"           | "file"                    | ArgChecker::File
     * "LAYERS"  |           | "l"           | "layers"                  | ArgChecker::Vector<int>
     * "CROP"    |           | "c"           | "crop"                    | ArgChecker::Rectangle
     * "COLTAB"  | "DISABLE" |               | "disable-use-color-table" | ArgChecker::None
     * "COLTAB"  | "ENABLE"  |               | "enable-use-color-table"  | ArgChecker::None
     * "BITS"    |           | "b"           | "extract-bits"            | ArgChecker::Vector<int>
     * "RANGE"   | "VALID"   |               | "valid-ranges"            | ArgChecker::IntervalSet
     * "RANGE"   | "INVALID" |               | "invalid-ranges"          | ArgChecker::IntervalSet
     *
     * This is the default usage vector for Parse::Mask().
     */
    static const std::vector<Descriptor> usageMask;

    /**
     * @brief Specifies the sub-options for multi-res masks
     *
     * This specifies
     * Specifier | Property  | Short options | Long options              | Argument checker
     * --------- | --------- | ------------- | ------------------------- | ----------------
     * "FILE"    |           | "f"           | "file"                    | ArgChecker::File
     * "DATE"    |           | "d"           | "date"                    | ArgChecker::int
     * "TAG"     |           | "t"           | "tag"                     | ArgChecker::NonEmpty
     * "LAYERS"  |           | "l"           | "layers"                  | ArgChecker::Vector<int>
     * "CROP"    |           | "c"           | "crop"                    | ArgChecker::Rectangle
     * "COLTAB"  | "DISABLE" |               | "disable-use-color-table" | ArgChecker::None
     * "COLTAB"  | "ENABLE"  |               | "enable-use-color-table"  | ArgChecker::None
     * "BITS"    |           | "b"           | "extract-bits"            | ArgChecker::Vector<int>
     * "RANGE"   | "VALID"   |               | "valid-ranges"            | ArgChecker::IntervalSet
     * "RANGE"   | "INVALID" |               | "invalid-ranges"          | ArgChecker::IntervalSet
     *
     * This is the default usage vector for Parse::MRMask().
     */
    static const std::vector<Descriptor> usageMRMask;

    /**
     * @brief %Parse an Integer from a string
     *
     * @param str is the string that contains the integer. This has to be a whole number (without
     * decimal dot), like `3`.
     *
     * @param optName is the option name where this Integer argument is specified. For example with
     * `-n 3` the option name could be `-n` or just `n`. It is only used to provide better error
     * messages. Usually, when you parse an integer, you can be sure that no error occurs, because
     * the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-n` and `--number`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageNum = "  -n <num>, \t--number=<num> \tDescription text";
     * @endcode
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Int. It will parse the option
     * argument once to check if it can be parsed.
     *
     * Note, in contrast to std::stoi this method will abort if a decimal dot is found and throw
     * nice exceptions.
     *
     * @return the parsed integer.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static int Int(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a double precision floating point number from a string
     *
     * @param str is the string that contains the floating point number. As an example `3.234`
     * would be valid.
     *
     * @param optName is the option name where this floating point number argument is specified.
     * For example with `-n 3e3` the option name could be `-n` or just `n`. It is only used to
     * provide better error messages. Usually, when you parse a floating point number, you can be
     * sure that no error occurs, because the checking function has parsed it already by using this
     * function.
     *
     * In your usage description, you can use the following, where you replace `-n` and `--number`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageNum = "  -n <float>, \t--number=<float> \tDescription text";
     * @endcode
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Float. It will parse the option
     * argument once to check if it can be parsed.
     *
     * @return the parsed double value.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static double Float(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse an angle from a sexagesimal string
     *
     * @param str is the string that contains the floating point angle in degrees potentially in
     * sexagesimal system. As an example all of the following would be valid and mean the same:
     * <tt>4d48'38.51\"</tt>, <tt>4° 48' 38.51\"</tt>, `4° 48.64183'`, `4.810697` or `0.0839625
     * rad`.
     *
     * @param optName is the option name where this angle argument is specified. For example with
     * `-a 30` the option name could be `-a` or just `a`. It is only used to provide better error
     * messages. Usually, when you parse an angle, you can be sure that no error occurs, because
     * the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-a` and `--angle`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageAngle = "  -a <angle>, \t--angle=<angle> \tDescription text\v"
     *     "<angle> will be saved in degree and can have a `d`, `deg` or `°` as degree symbol. "
     *     "It must have one of the following forms:\v"
     *     " * `<float>` optionally appended by whitespace and a degree symbol\v"
     *     " * `<float> rad` will multiply the number by 180 / pi\v"
     *     " * `<int>° <float>'` here a degree symbol is mandatory\v"
     *     " * `<int>° <int>' <float>"` where instead of `"` also `''` can be used. A degree symbol is mandatory.\v"
     *     "Examples: -a 4.810697\v"
     *     "          -a \"4° 48.64183'\"\v"
     *     "          -a \"4d48'38.51\\\"\"\v"
     *     "          -a \"4d48'38.51''\"\v"
     *     "          -a \"4d 48' 38.51''\"\v"
     *     "          -a \"4° 48.64183'\"\v"
     *     "          -a 0.0839625rad";
     * @endcode
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Angle. It will parse the option
     * argument once to check if it can be parsed.
     *
     * @return the parsed angle in degree.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static double Angle(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a latitude / longitude degree coordinate
     *
     * @param str is the string that contains the geographic location in the usual format. As an
     * example all of the following would be valid and mean the same: `51.327905, 6.967492`,
     * `51°19'40.5"N 6°58'03.0"E`, `N51°19'40.5" E6°58'03.0"`, `6d58'03.0"E, 51d19'40.5"N`.
     *
     * @param optName is the option name where this GeoCoord argument is specified. For example
     * with `-g "51.327905, 6.967492"` the option name could be `-g` or just `g`. It is only used
     * to provide better error messages. Usually, when you parse a GeoCoord, you can be sure that
     * no error occurs, because the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-g` and `--geo-loc`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does. Also add maybe a description text for `<angle>`:
     * @code
     * const char* usageGeoLoc = "  -g <geocoord>, \t--geo-loc=<geocoord> \tDescription text\v"
     *     "<geocoord> must have one of the following forms:\v"
     *     " * `<angle>, <angle>`\v"
     *     " * `N<angle>, E<angle>` or S or W\v"
     *     " * `<angle>N, <angle>E` or S or W\v"
     *     "Examples: -g 51.327905, 6.967492\v"
     *     "          -g 51°19'40.5\"N 6°58'03.0\"E\v"
     *     "          -g N51°19'40.5\" E6°58'03.0\"\v"
     *     "          -g 6d58'03.0\"E, 51d19'40.5\"N";
     * @endcode
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::GeoCoord. It will parse the option
     * argument once to check if it can be parsed.
     *
     * @return the parsed geographic location in longitude as x and latitude as y, both in degree.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Coordinate GeoCoord(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse an image data type from a string
     *
     * @param str is the string that corresponds to some image data type. As an example `int16x2`
     * would be valid. Case will be ignored.
     *
     * @param optName is the option name where this image data type argument is specified. For
     * example with `-t Byte` the option name could be `-t` or just `t`. It is only used to provide
     * better error messages. Usually, when you parse an image data type, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-t` and `--type` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageType =
     *     "  -t <type>, \t--type=<type> \tReads an image data type. It consists of a base type, i. e. one of "
     *     "uint8, int8, uint16, int16, int32, float32 or float64, and of an optional channel specifier, which is one of "
     *     "x1, x2, x3 or x4. So a full example would be uint16x3. However, alternative base type specifiers are also allowed:\v"
     *     " * Byte is interpreted as uint8,\v"
     *     " * Float and Single are interpreted as float32,\v"
     *     " * Double is interpreted as float64";
     * @endcode
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Type. It will parse the option
     * argument once to check if it can be parsed.
     *
     * @return the parsed image data type.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Type Type(std::string str, std::string const& optName = "");

    /**
     * @brief %Parse an Interval from a string
     *
     * @param str is the string that contains the interval. It has to follow a specific format, see
     * below in the usage description. As an example `(0.1, INF)` as well as `[1000 10000]` would
     * be valid.
     *
     * @param optName is the option name where this Interval argument is specified. For example
     * with `--valid-range=[10,20]` the option name could be `--valid-range` or just `valid-range`.
     * It is only used to provide better error messages. Usually, when you parse an interval, you
     * can be sure that no error occurs, because the checking function has parsed it already by
     * using this function.
     *
     * In your usage description, you can use the following, where you replace `-i` and
     * `--interval` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usageInterval =
     *     "  -i <interval>, \t--interval=<interval> \tDescription text.\v"
     *     "<interval> must have the form '[<float>,<float>]', '(<float>,<float>)',
     *     " '[<float>,<float>)' or '(<float>,<float>]' where the comma and round brackets are optional."
     *     " Additional spacing can be added anywhere, if quoted or escaped.\v"
     *     "Examples: --interval='(100, 200)'\v"
     *     "          --interval=[100,200]";
     * @endcode
     * The round parens are optional, to avoid input errors because of missing parens eaten by
     * separateArguments().
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Interval. It will parse the option
     * argument once to check if everything can be parsed like specified in the format above.
     *
     * @return the parsed Interval object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Interval Interval(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse an IntervalSet from a string
     *
     * @param str is the string that contains the interval set. It has to follow a specific format,
     * see below in the usage description. As an example `(0.1, INF) [-5,-1)` as well as `[100 200]
     * [300 400]` would be valid.d
     *
     * @param optName is the option name where this IntervalSet argument is specified. For example
     * with `--valid-ranges=[10,20]` the option name could be `--valid-ranges` or just
     * `valid-ranges`. It is only used to provide better error messages. Usually, when you parse an
     * interval set, you can be sure that no error occurs, because the checking function has parsed
     * it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-i` and
     * `--interval-set` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usageInterval =
     *     "  -i <inter-list>, \t--interval-set=<inter-list> \tDescription text.\v"
     *     "<inter-list> must have the form '<interval> [<interval> ...], where"
     *     " the brackets mean that further intervals are optional.\v"
     *     "<interval> must have the format '[<float>,<float>]', '(<float>,<float>)',"
     *     " '[<float>,<float>)' or '(<float>,<float>]', where the comma and round"
     *     " brackets are optional, but square brackets are here actual characters."
     *     " <float> can be 'infinity' (see std::stod). Additional spacing can be added anywhere.\v"
     *     "Examples: --interval-set='(100, 200)'\v"
     *     "          --interval-set='[-100, 200] [300, inf]'";
     * @endcode
     * The round parens are optional, to avoid input errors because of missing parens eaten by
     * separateArguments().
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::IntervalSet. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed IntervalSet object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::IntervalSet IntervalSet(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a Size from a string
     *
     * @param str is the string that contains the size. It has to follow a specific format, see
     * below in the usage description. As an example `-w 3 -h 4` as well as `3 x 4` would be valid.
     *
     * @param optName is the option name where this Size argument is specified. For example with
     * `--size="-w 3 -h 4"` the option name could be `--size` or just `size`. It is only used to
     * provide better error messages. Usually, when you parse a size, you can be sure that no error
     * occurs, because the checking function has parsed it already by using this function.
     *
     * @param usageSize is the Descriptor vector that specifies the sub-options. So by providing a
     * different vector than the default, you could change the sub-option names described below,
     * but the purpose of a user specified usageSize is actually that you can add options on which
     * Parse::Size should not throw. So for example if you like to have a size option that also
     * accepts a unit, you could use the following code:
     * @code
     * std::pair<std::string, Size>
     * parseSizeWithUnit(std::string inputArgument = "-w -2 --unit=m -h=-5") {                           // example for command line input
     *     std::vector<Descriptor> usageSizeWithUnit = Parse::usageSize;                                 // copy default
     *     usageSizeWithUnit.push_back(Descriptor{"UNIT", "", "u","unit", ArgChecker::NonEmpty, ""});    // add unit option
     *     usageSizeWithUnit.push_back(Descriptor{"UNIT", "", "u","u",    ArgChecker::NonEmpty, ""});    // allow -u=m
     *
     *     // now does not throw on -u or --unit option, but still on "-2 x -5 --unit=m"
     *     Size sz = Parse::Size(inputArgument, "", usageSizeWithUnit);
     *
     *     OptionParser sizeOptions(usageSizeWithUnit);                                                  // parser just for unit option
     *     sizeOptions.unknownOptionArgCheck = ArgChecker::None;
     *     sizeOptions.singleDashLongopt = true;                                                         // do not throw on "-h=5"
     *     sizeOptions.parse(inputArgument);
     *
     *     std::string unit;
     *     if (!sizeOptions["UNIT"].empty())
     *         unit = sizeOptions["UNIT"].back().arg;
     *     return {unit, sz};
     * }
     * @endcode
     * So this either accepts the special format like "-2 x -5" without unit (since this format
     * does not accept more than two tokens) or suboptions with unit (see `inputArgument` above as
     * example).
     *
     * In your usage description, you can use the following, where you replace `-s` and `--size` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageSize =
     *     "  -s <size>, \t--size=<size> \tDescription text.\v"
     *     "<size> either receives the following arguments:\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "or must have the form '<num>x<num>' or just '(<num> <num>)', both with optional "
     *     "spacing, where the first argument is the width and the second is the height.\v"
     *     "Examples: --size='-w 100 -h 200'\v"
     *     "          --size=100x200\v"
     *     "          --size=100*200\v"
     *     "          --size='(100 200)'";
     * @endcode
     * To be more precise, every char of type `x`, ` `(space), `*`, `(`, `)`, `'` and `"` will be
     * stripped away and the remaining tokens in between are tried be parsed as exactly two
     * integers, if there is no dot `.` inside. So even extreme examples like `"((xx**5*xxx xx
     * x6xx))"` would be parsed correctly as 5x6. Note, that `\v` will go to the next table line,
     * but stay in the same column. So the lines `"\<size\> ...` and below will be aligned with
     * `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Size. It will parse the option
     * argument once to check if everything can be parsed like specified in the format above.
     *
     * @return the parsed Size object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Size Size(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageSize = Parse::usageSize);

    /**
     * @brief %Parse a Size from a string with sub-options only
     *
     * @param str is the string that contains the size. It has to follow a specific format, see
     * below in the usage description. As an example `-w 3 -h 4` would be valid. Note, something
     * like `3 x 4` would be invalid, since this method only uses sub-options for parsing.
     *
     * @param optName is the option name where this Size argument is specified. For example with
     * `--size="-w 3 -h 4"` the option name could be `--size` or just `size`. It is only used to
     * provide better error messages. Usually, when you parse a size, you can be sure that no error
     * occurs, because the checking function has parsed it already by using this function.
     *
     * @param usageSize is the Descriptor vector that specifies the sub-options. See Parse::Size()
     * for a description and an example. When using @c Parse::SizeSubopts instead of @c Parse::Size
     * in the example the special format would not be allowed at all.
     *
     * In your usage description, you can use the following, where you replace `-s` and `--size` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageSize =
     *     "  -s <size>, \t--size=<size> \tDescription text.\v"
     *     "<size> must receive the following arguments:\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "Example: --size='-w 100 -h 200'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<size\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::SizeSubopts. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Size object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Size SizeSubopts(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageSize = Parse::usageSize);

    /**
     * @brief %Parse a Size from a string with special format only
     *
     * @param str is the string that contains the size. It has to follow a specific format, see
     * below in the usage description. As an example `3 x 4` would be valid. Something like `-w 3
     * -h 4` would be invalid.
     *
     * @param optName is the option name where this Size argument is specified. For example with
     * `--size="3 x 4"` the option name could be `--size` or just `size`. It is only used to
     * provide better error messages. Usually, when you parse a size, you can be sure that no error
     * occurs, because the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-s` and `--size` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageSize =
     *     "  -s <size>, \t--size=<size> \tDescription text.\v"
     *     "<size> must have the form '<num>x<num>' or just '(<num> <num>)', both with optional "
     *     "spacing, where the first argument is the width and the second is the height.\v"
     *     "Examples: --size=100x200\v"
     *     "          --size=100*200\v"
     *     "          --size='(100 200)'";
     * @endcode
     * To be more precise, every char of type `x`, ` `(space), `*`, `(`, `)`, `'` and `"` will be
     * stripped away and the remaining tokens in between are tried be parsed as exactly two
     * integers, if there is no dot `.` inside. So even extreme examples like `"((xx**5*xxx xx
     * x6xx))"` would be parsed correctly as 5x6. Note, that `\v` will go to the next table line,
     * but stay in the same column. So the lines `"\<size\> ...` and below will be aligned with
     * `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::SizeSpecial. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Size object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Size SizeSpecial(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a Dimensions (Size with floating point numbers) from a string
     *
     * @param str is the string that contains the size / dimensions. It has to follow a specific
     * format, see below in the usage description. As an example `-w 3 -h 4` as well as `3 x 4`
     * would be valid.
     *
     * @param optName is the option name where this Dimensions argument is specified. For example
     * with `--size="-w 3 -h 4"` the option name could be `--size` or just `size`. It is only used
     * to provide better error messages. Usually, when you parse a size, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * @param usageDimensions is the Descriptor vector that specifies the sub-options. So by
     * providing a different vector than the default, you could change the sub-option names
     * described below, but the purpose of a user specified usageDimensions is actually that you
     * can add options on which Parse::Dimensions should not throw. So for example if you like to
     * have a dimensions option that also accepts a unit, you could use the following code:
     * @code
     * std::pair<std::string, Dimensions>
     * parseDimWithUnit(std::string inputArgument = "-w -2 --unit=m -h=-5") {                           // example for command line input
     *     std::vector<Descriptor> usageDimWithUnit = Parse::usageDimensions;                           // copy default
     *     usageDimWithUnit.push_back(Descriptor{"UNIT", "", "u","unit", ArgChecker::NonEmpty, ""});    // add unit option
     *     usageDimWithUnit.push_back(Descriptor{"UNIT", "", "u","u",    ArgChecker::NonEmpty, ""});    // allow -u=m
     *
     *     // now does not throw on -u or --unit option, but still on "-2 x -5 --unit=m"
     *     Dimensions dim = Parse::Dimensions(inputArgument, "", usageDimWithUnit);
     *
     *     OptionParser dimOptions(usageDimWithUnit);                                                  // parser just for unit option
     *     dimOptions.unknownOptionArgCheck = ArgChecker::None;
     *     dimOptions.singleDashLongopt = true;                                                        // do not throw on "-h=5"
     *     dimOptions.parse(inputArgument);
     *
     *     std::string unit;
     *     if (!dimOptions["UNIT"].empty())
     *         unit = dimOptions["UNIT"].back().arg;
     *     return {unit, dim};
     * }
     * @endcode
     * So this either accepts the special format like "-2 x -5" without unit (since this format
     * does not accept more than two tokens) or suboptions with unit (see `inputArgument` above as
     * example).
     *
     * In your usage description, you can use the following, where you replace `-s` and `--size` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageSize =
     *     "  -s <size>, \t--size=<size> \tDescription text.\v"
     *     "<size> either receives the following arguments:\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "or must have the form '<num>x<num>' or just '(<num> <num>)', both with optional "
     *     "spacing, where the first argument is the width and the second is the height.\v"
     *     "Examples: --size='-w 100 -h 200'\v"
     *     "          --size=100x200\v"
     *     "          --size=100*200\v"
     *     "          --size='(100 200)'";
     * @endcode
     * To be more precise, every char of type `x`, ` `(space), `*`, `(`, `)`, `'` and `"` will be
     * stripped away and the remaining tokens in between are tried be parsed as exactly two
     * integers, if there is no dot `.` inside. So even extreme examples like `"((xx**5*xxx xx
     * x6xx))"` would be parsed correctly as 5x6. Note, that `\v` will go to the next table line,
     * but stay in the same column. So the lines `"\<size\> ...` and below will be aligned with
     * `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Dimensions. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Dimensions object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Dimensions Dimensions(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageDimensions = Parse::usageDimensions);

    /**
     * @brief %Parse a Dimensions (Size with floating point numbers) from a string with sub-options only
     *
     * @param str is the string that contains the size / dimensions. It has to follow a specific
     * format, see below in the usage description. As an example `-w 3 -h 4` would be valid. Note,
     * something like `3 x 4` would be invalid, since this method only uses sub-options for
     * parsing.
     *
     * @param optName is the option name where this Dimensions argument is specified. For example
     * with `--size="-w 3 -h 4"` the option name could be `--size` or just `size`. It is only used
     * to provide better error messages. Usually, when you parse a size, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * @param usageDimensions is the Descriptor vector that specifies the sub-options. See
     * Parse::Dimensions() for a description and an example. When using @c Parse::DimensionsSubopts
     * instead of @c Parse::Dimensions in the example the special format would not be allowed at
     * all.
     *
     * In your usage description, you can use the following, where you replace `-s` and `--size` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageSize =
     *     "  -s <size>, \t--size=<size> \tDescription text.\v"
     *     "<size> must have the following arguments:\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "Example: --size='-w 100 -h 200'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<size\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::DimensionsSubopts. It will parse
     * the option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Dimensions object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Dimensions DimensionsSubopts(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageDimensions = Parse::usageDimensions);

    /**
     * @brief %Parse a Dimensions (Size with floating point numbers) from a string with special format only
     *
     * @param str is the string that contains the size / dimensions. It has to follow a specific
     * format, see below in the usage description. As an example `3 x 4` would be valid. Something
     * like `-w 3 -h 4` would be invalid.
     *
     * @param optName is the option name where this Dimensions argument is specified. For example
     * with `--size="3 x 4"` the option name could be `--size` or just `size`. It is only used to
     * provide better error messages. Usually, when you parse a size, you can be sure that no error
     * occurs, because the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-s` and `--size` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageSize =
     *     "  -s <size>, \t--size=<size> \tDescription text.\v"
     *     "<size> must have the form '<num>x<num>' or just '(<num> <num>)', both with optional "
     *     "spacing, where the first argument is the width and the second is the height.\v"
     *     "Examples: --size=100x200\v"
     *     "          --size=100*200\v"
     *     "          --size='(100 200)'";
     * @endcode
     * To be more precise, every char of type `x`, ` `(space), `*`, `(`, `)`, `'` and `"` will be
     * stripped away and the remaining tokens in between are tried be parsed as exactly two
     * integers, if there is no dot `.` inside. So even extreme examples like `"((xx**5*xxx xx
     * x6xx))"` would be parsed correctly as 5x6. Note, that `\v` will go to the next table line,
     * but stay in the same column. So the lines `"\<size\> ...` and below will be aligned with
     * `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::DimensionsSpecial. It will parse
     * the option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Dimensions object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Dimensions DimensionsSpecial(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a Point from a string
     *
     * @param str is the string that contains the point. It has to follow a specific format, see
     * below in the usage description. As an example `-x 3 -y 4` as well as `(3, 4)` would be valid
     *
     * @param optName is the option name where this Point argument is specified. For example with
     * `--point="-x 3 -y 4"` the option name could be `--point` or just `point`. It is only used to
     * provide better error messages. Usually, when you parse a point, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * @param usagePoint is the Descriptor vector that specifies the sub-options. So by providing a
     * different vector than the default, you could change the sub-option names described below,
     * but the purpose of a user specified usagePoint is actually that you can add options on which
     * Parse::Point should not throw. So for example if you like to have a point option that also
     * accepts a unit, you could use the following code:
     * @code
     * std::pair<std::string, Point>
     * parsePointWithUnit(std::string inputArgument = "-x -2 --unit=m -y=-5") {                           // example for command line input
     *     std::vector<Descriptor> usagePointWithUnit = Parse::usagePoint;                                // copy default
     *     usagePointWithUnit.push_back(Descriptor{"UNIT", "", "u","unit", ArgChecker::NonEmpty, ""});    // add unit option
     *     usagePointWithUnit.push_back(Descriptor{"UNIT", "", "u","u",    ArgChecker::NonEmpty, ""});    // allow -u=m
     *
     *     // now does not throw on -u or --unit option, but still on "-2, -5 --unit=m"
     *     Point pt = Parse::Point(inputArgument, "", usagePointWithUnit);
     *
     *     OptionParser pointOptions(usagePointWithUnit);                                                 // parser just for unit option
     *     pointOptions.unknownOptionArgCheck = ArgChecker::None;
     *     pointOptions.singleDashLongopt = true;                                                         // do not throw on "-h=5"
     *     pointOptions.parse(inputArgument);
     *
     *     std::string unit;
     *     if (!pointOptions["UNIT"].empty())
     *         unit = pointOptions["UNIT"].back().arg;
     *     return {unit, pt};
     * }
     * @endcode
     * So this either accepts the special format like "-2, -5" without unit (since this format does
     * not accept more than two tokens) or suboptions with unit (see `inputArgument` above as
     * example).
     *
     * In your usage description, you can use the following, where you replace `-p` and `--point`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usagePoint =
     *     "  -p <point>, \t--point=<point> \tDescription text\v"
     *     "<point> either receives the following arguments:\v"
     *     "  -x <num>\v"
     *     "  -y <num>\v"
     *     "or must have the form (<num>, <num>) with optional spacing and "
     *     "comma, where the first argument is for x and the second is for y.\v"
     *     "Examples: --point='-x 5 -y 6'\v"
     *     "          --point='(5, 6)'";
     * @endcode
     * To be more precise, every char of type `,` (comma), ` ` (space), `(`, `)`, `'` and `"` will
     * be stripped away and the remaining tokens in between are tried be parsed as exactly two
     * integers, if there is dot `.` inside. So even extreme examples like `")(-1,,,\"2"` would be
     * parsed correctly as (-1, 2). Note, that `\v` will go to the next table line, but stay in the
     * same column. So the lines `"\<point\> ...` and below will be aligned with `Description
     * text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Point. It will parse the option
     * argument once to check if everything can be parsed like specified in the format above.
     *
     * @return the parsed Point object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Point Point(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usagePoint = Parse::usagePoint);

    /**
     * @brief %Parse a Point from a string with sub-options only
     *
     * @param str is the string that contains the point. It has to follow a specific format, see
     * below in the usage description. As an example `-x 3 -y 4` would be valid. Note, something
     * like `(3, 4)` would be invalid, since this method only uses sub-options for parsing.
     *
     * @param optName is the option name where this Point argument is specified. For example with
     * `--point="-x 3 -y 4"` the option name could be `--point` or just `point`. It is only used to
     * provide better error messages. Usually, when you parse a point, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * @param usagePoint is the Descriptor vector that specifies the sub-options. See
     * Parse::Point() for a description and an example. When using @c Parse::PointSubopts instead
     * of @c Parse::Point in the example the special format would not be allowed at all.
     *
     * In your usage description, you can use the following, where you replace `-p` and `--point`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usagePoint =
     *     "  -p <point>, \t--point=<point> \tDescription text\v"
     *     "<point> must have the following arguments:\v"
     *     "  -x <num>\v"
     *     "  -y <num>\v"
     *     "Example: --point='-x 5 -y 6'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<point\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::PointSubopts. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Point object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Point PointSubopts(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usagePoint = Parse::usagePoint);

    /**
     * @brief %Parse a Point from a string with special format only
     *
     * @param str is the string that contains the point. It has to follow a specific format, see
     * below in the usage description. As an example `(3, 4)` would be valid. Note, something like
     * `-x 3 -y 4` would be invalid.
     *
     * @param optName is the option name where this Point argument is specified. For example with
     * `--point="3, 4"` the option name could be `--point` or just `point`. It is only used to
     * provide better error messages. Usually, when you parse a point, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * In your usage description, you can use the following, where you replace `-p` and `--point`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usagePoint =
     *     "  -p <point>, \t--point=<point> \tDescription text\v"
     *     "<point> must have the form (<num>, <num>) with optional spacing and "
     *     "comma, where the first argument is for x and the second is for y.\v"
     *     "Example: --point='(5, 6)'";
     * @endcode
     * To be more precise, every char of type `,` (comma), ` ` (space), `(`, `)`, `'` and `"` will
     * be stripped away and the remaining tokens in between are tried be parsed as exactly two
     * integers, if there is dot `.` inside. So even extreme examples like `")(-1,,,\"2"` would be
     * parsed correctly as (-1, 2). Note, that `\v` will go to the next table line, but stay in the
     * same column. So the lines `"\<point\> ...` and below will be aligned with `Description
     * text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::PointSpecial. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Point object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Point PointSpecial(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a Coordinate from a string
     *
     * @param str is the string that contains the coordinate. It has to follow a specific format,
     * see below in the usage description. As an example `-x 3.12 -y 4` as well as `(3.12, 4)`
     * would be valid.
     *
     * @param optName is the option name where this Coordinate argument is specified. For example
     * with `--coordinate="-x 3 -y 4"` the option name could be `--coordinate` or just
     * `coordinate`. It is only used to provide better error messages. Usually, when you parse a
     * coordinate, you can be sure that no error occurs, because the checking function has parsed
     * it already by using this function.
     *
     * @param usageCoordinate is the Descriptor vector that specifies the sub-options. So by
     * providing a different vector than the default, you could change the sub-option names
     * described below, but the purpose of a user specified usageCoordinate is actually that you
     * can add options on which Parse::Coordinate should not throw. So for example if you like to
     * have a coordinate option that also accepts a unit, you could use the following code:
     * @code
     * std::pair<std::string, Coordinate>
     * parseCoordWithUnit(std::string inputArgument = "-x -2 --unit=m -y=-5") {                           // example for command line input
     *     std::vector<Descriptor> usageCoordWithUnit = Parse::usageCoordinate;                                // copy default
     *     usageCoordWithUnit.push_back(Descriptor{"UNIT", "", "u","unit", ArgChecker::NonEmpty, ""});    // add unit option
     *     usageCoordWithUnit.push_back(Descriptor{"UNIT", "", "u","u",    ArgChecker::NonEmpty, ""});    // allow -u=m
     *
     *     // now does not throw on -u or --unit option, but still on "-2, -5 --unit=m"
     *     Coord c = Parse::Coordinate(inputArgument, "", usageCoordWithUnit);
     *
     *     OptionParser coordOptions(usageCoordWithUnit);                                                 // parser just for unit option
     *     coordOptions.unknownOptionArgCheck = ArgChecker::None;
     *     coordOptions.singleDashLongopt = true;                                                         // do not throw on "-h=5"
     *     coordOptions.parse(inputArgument);
     *
     *     std::string unit;
     *     if (!coordOptions["UNIT"].empty())
     *         unit = coordOptions["UNIT"].back().arg;
     *     return {unit, c};
     * }
     * @endcode
     * So this either accepts the special format like "-2, -5" without unit (since this format does
     * not accept more than two tokens) or suboptions with unit (see `inputArgument` above as
     * example).
     *
     * In your usage description, you can use the following, where you replace `-c` and
     * `--coordinate` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usuageCoordinate =
     *     "  -c <coord>, \t--coordinate=<coord> \tDescription text\v"
     *     "<coord> either receives the following arguments:\v"
     *     "  -x <float>\v"
     *     "  -y <float>\v"
     *     "or must have (<float>, <float>) with optional spacing and "
     *     "comma, where the first argument is for x and the second is for y.\v"
     *     "Examples: --coordinate='-x 3.1416 -y 42'\v"
     *     "          --coordinate='(3.1416, 42)'";
     * @endcode
     * To be more precise, every char of type `,` (comma), ` ` (space), `(`, `)`, `'` and `"` will
     * be stripped away and the remaining tokens in between are tried be parsed as exactly two
     * floating point numbers, if there is no dot `.` inside. So even extreme examples like
     * `"-x=(1e1) --y=.5"` would be parsed correctly as (10, 0.5). Note, that `\v` will go to the
     * next table line, but stay in the same column. So the lines `"\<coord\> ...` and below will
     * be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Coordinate. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Coordinate object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Coordinate Coordinate(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageCoordinate = Parse::usageCoordinate);

    /**
     * @brief %Parse a Coordinate from a string with sub-options only
     *
     * @param str is the string that contains the coordinate. It has to follow a specific format,
     * see below in the usage description. As an example `-x 3 -y 4` would be valid. Note,
     * something like `(3, 4)` would be invalid.
     *
     * @param optName is the option name where this Coordinate argument is specified. For example
     * with `--coordinate="-x 3 -y 4"` the option name could be `--coordinate` or just
     * `coordinate`. It is only used to provide better error messages. Usually, when you parse a
     * coordinate, you can be sure that no error occurs, because the checking function has parsed
     * it already by using this function.
     *
     * @param usageCoordinate is the Descriptor vector that specifies the sub-options. See
     * Parse::Coordinate() for a description and an example. When using @c Parse::CoordinateSubopts
     * instead of @c Parse::Coordinate in the example the special format would not be allowed at
     * all.
     *
     * In your usage description, you can use the following, where you replace `-c` and
     * `--coordinate` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usuageCoordinate =
     *     "  -c <coord>, \t--coordinate=<coord> \tDescription text\v"
     *     "<coord> must have the following arguments:\v"
     *     "  -x <float>\v"
     *     "  -y <float>\v"
     *     "Example: --coordinate='-x 3.1416 -y 42'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<coord\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::CoordinateSubopts. It will parse
     * the option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Coordinate object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Coordinate CoordinateSubopts(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageCoordinate = Parse::usageCoordinate);

    /**
     * @brief %Parse a Coordinate from a string with special format only
     *
     * @param str is the string that contains the coordinate. It has to follow a specific format,
     * see below in the usage description. As an example `(3, 4)` would be valid. Note, something
     * like `-x 3 -y 4` would be invalid.
     *
     * @param optName is the option name where this Coordinate argument is specified. For example
     * with `--coordinate="3, 4"` the option name could be `--coordinate` or just `coordinate`. It
     * is only used to provide better error messages. Usually, when you parse a coordinate, you can
     * be sure that no error occurs, because the checking function has parsed it already by using
     * this function.
     *
     * In your usage description, you can use the following, where you replace `-c` and
     * `--coordinate` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usuageCoordinate =
     *     "  -c <coord>, \t--coordinate=<coord> \tDescription text\v"
     *     "<coord> must have the form (<float>, <float>) with optional spacing and "
     *     "comma, where the first argument is for x and the second is for y.\v"
     *     "Example: --coordinate='(3.1416, 42)'";
     * @endcode
     * To be more precise, every char of type `,` (comma), ` ` (space), `(`, `)`, `'` and `"` will
     * be stripped away and the remaining tokens in between are tried be parsed as exactly two
     * floating point numbers, if there is no dot `.` inside. So even extreme examples like
     * `"-x=(1e1) --y=.5"` would be parsed correctly as (10, 0.5). Note, that `\v` will go to the
     * next table line, but stay in the same column. So the lines `"\<coord\> ...` and below will
     * be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::CoordinateSpecial. It will parse
     * the option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Coordinate object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Coordinate CoordinateSpecial(std::string const& str, std::string const& optName = "");

    /**
     * @brief %Parse a Rectangle from a string
     *
     * @param str is the string that contains the rectangle. It has to follow a specific format,
     * see below in the usage description. As an example `-x 1 -y 2 -w 3 -h 4` would be valid or
     * `-x=(1 3) -y=(2 5)` or `--center=(2 3.5) -w 3 -h 4`.
     *
     * @param optName is the option name where this rectangle argument is specified. For example
     * with `--rectangle="-x 1 -y 2 -w 3 -h 4"` the option name could be `--rectangle` or just
     * `rectangle`. It is only used to provide better error messages. Usually, when you parse a
     * rectangle, you can be sure that no error occurs, because the checking function has parsed it
     * already by using this function.
     *
     * @param usageRectangle is the Descriptor vector that specifies the sub-options. So by
     * providing a different vector than the default, you could change the sub-option names
     * described below, but the purpose of a user specified usageRectangle is actually that you can
     * add options on which Parse::Rectangle should not throw. So for example if you like to have a
     * rectangle option that also accepts a unit, you could use the following code:
     * @code
     * std::pair<std::string, Rectangle>
     * parseRectangleWithUnit(std::string inputArgument = "--unit=m -x (-2, 5) -y=(-5, 10)") {           // example for command line input
     *     std::vector<Descriptor> usageRectWithUnit = Parse::usageRectangle;                            // copy default
     *     usageRectWithUnit.push_back(Descriptor{"UNIT", "", "u","unit", ArgChecker::NonEmpty, ""});    // add unit option
     *     usageRectWithUnit.push_back(Descriptor{"UNIT", "", "u","u",    ArgChecker::NonEmpty, ""});    // allow -u=m
     *
     *     // now does not throw on -u or --unit option"
     *     Rectangle rect = Parse::Rectangle(inputArgument, "", usageRectWithUnit);
     *
     *     OptionParser rectOptions(usageRectWithUnit);                                                  // parser just for unit option
     *     rectOptions.singleDashLongopt = true;                                                         // do not throw on "-h=5"
     *     rectOptions.parse(inputArgument);
     *
     *     std::string unit;
     *     if (!rectOptions["UNIT"].empty())
     *         unit = rectOptions["UNIT"].back().arg;
     *     return {unit, rect};
     * }
     * @endcode
     *
     * In your usage description, you can use the following, where you replace `-r` and
     * `--rectangle` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usageRectangle =
     *     "  -r <rect>, \t--rectangle=<rect> \tDescription text\v"
     *     "<rect> requires either all of the following arguments:\v"
     *     "  -c (<num> <num), --center=(<num> <num>) x and y center\v"
     *     "  -w <num>, --width=<num>                 width\v"
     *     "  -h <num>, --height=<num>                height\v"
     *     "Examples: --rectangle='--center=(2 3.5) --width 3 -h 4)'\v"
     *     "          -r '-c (2 3.5) -w 3 -h 4'\v"
     *     "or x can be specified with:\v"
     *     "  -x <num>                 x start and\v"
     *     "  -w <num>, --width=<num>  width or just with\v"
     *     "  -x (<num> <num>)         x extents\v"
     *     "and y can be specified with:\v"
     *     "  -y <num>                 y start and\v"
     *     "  -h <num>, --height=<num> height or just with\v"
     *     "  -y (<num> <num>)         y extents\v"
     *     "Examples: --rectangle='-x1 -y=2 --width 3 -h 4)'\v"
     *     "          -r '-x 1 -y 2 -w 3 -h 4'\v"
     *     "          --rectangle='-x=(1 3) -y=(2 5)'\v"
     *     "          --rectangle='-x=(1 3) -y=2 -h=4'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<rect\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Rectangle. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed Rectangle object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Rectangle Rectangle(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageRectangle = Parse::usageRectangle);

    /**
     * @brief %Parse a CoordRectangle from a string
     *
     * @param str is the string that contains the rectangle. It has to follow a specific format,
     * see below in the usage description. As an example `-x 1 -y 2 -w 3 -h 4` would be valid or
     * `-x=(1 3) -y=(2 5)` or `--center=(2 3.5) -w 3 -h 4`.
     *
     * @param optName is the option name where this rectangle argument is specified. For example
     * with `--rectangle="-x 1 -y 2 -w 3 -h 4"` the option name could be `--rectangle` or just
     * `rectangle`. It is only used to provide better error messages. Usually, when you parse a
     * rectangle, you can be sure that no error occurs, because the checking function has parsed it
     * already by using this function.
     *
     * @param usageCoordRectangle is the Descriptor vector that specifies the sub-options. So by
     * providing a different vector than the default, you could change the sub-option names
     * described below, but the purpose of a user specified usageCoordRectangle is actually that
     * you can add options on which Parse::CoordRectangle should not throw. So for example if you
     * like to have a rectangle option that also accepts a unit, you could use the following code:
     * @code
     * std::pair<std::string, CoordRectangle>
     * parseRectangleWithUnit(std::string inputArgument = "--unit=m -x (-2, 5) -y=(-5, 10)") {           // example for command line input
     *     std::vector<Descriptor> usageRectWithUnit = Parse::usageCoordRectangle;                       // copy default
     *     usageRectWithUnit.push_back(Descriptor{"UNIT", "", "u","unit", ArgChecker::NonEmpty, ""});    // add unit option
     *     usageRectWithUnit.push_back(Descriptor{"UNIT", "", "u","u",    ArgChecker::NonEmpty, ""});    // allow -u=m
     *
     *     // now does not throw on -u or --unit option"
     *     CoordRectangle rect = Parse::CoordRectangle(inputArgument, "", usageRectWithUnit);
     *
     *     OptionParser rectOptions(usageRectWithUnit);                                                  // parser just for unit option
     *     rectOptions.singleDashLongopt = true;                                                         // do not throw on "-h=5"
     *     rectOptions.parse(inputArgument);
     *
     *     std::string unit;
     *     if (!rectOptions["UNIT"].empty())
     *         unit = rectOptions["UNIT"].back().arg;
     *     return {unit, rect};
     * }
     * @endcode
     *
     * In your usage description, you can use the following, where you replace `-r` and
     * `--rectangle` by your chosen option names and `Description text` by a useful description,
     * what the option does:
     * @code
     * const char* usageRectangle =
     *     "  -r <rect>, \t--rectangle=<rect> \tDescription text\v"
     *     "<rect> requires either all of the following arguments:\v"
     *     "  -c (<num> <num), --center=(<num> <num>) x and y center\v"
     *     "  -w <num>, --width=<num>                 width\v"
     *     "  -h <num>, --height=<num>                height\v"
     *     "Examples: --rectangle='--center=(2 3.5) --width 3 -h 4)'\v"
     *     "          -r '-c (2 3.5) -w 3 -h 4'\v"
     *     "or x can be specified with:\v"
     *     "  -x <num>                 x start and\v"
     *     "  -w <num>, --width=<num>  width or just with\v"
     *     "  -x (<num> <num>)         x extents\v"
     *     "and y can be specified with:\v"
     *     "  -y <num>                 y start and\v"
     *     "  -h <num>, --height=<num> height or just with\v"
     *     "  -y (<num> <num>)         y extents\v"
     *     "Examples: --rectangle='-x1 -y=2 --width 3 -h 4)'\v"
     *     "          -r '-x 1 -y 2 -w 3 -h 4'\v"
     *     "          --rectangle='-x=(1 3) -y=(2 5)'\v"
     *     "          --rectangle='-x=(1 3) -y=2 -h=4'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<rect\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::CoordRectangle. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * @return the parsed CoordRectangle object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::CoordRectangle CoordRectangle(std::string const& str, std::string const& optName = "", std::vector<Descriptor> const& usageCoordRectangle = Parse::usageCoordRectangle);

    /**
     * @brief %Parse a plain Image from a filename given as string
     *
     * @param str is the string that contains the Image filename and optionally a crop window and
     * layers. The string has to follow a specific format, see below in the usage description. As
     * an example `--file=test.tif --crop=(-x 1 -y 2 -w 4 -h 2) --layers=1` would be valid as well
     * as just a plain filename like `"test image.tif"`.
     *
     * @param optName is the option name where this Image argument is specified. For example with
     * `--image=test.tif` the option name could be `--image` or just `image`. It is only used to
     * provide better error messages. Usually, when you parse an image, because of the checking
     * function, you can be sure that the argument format is correct and the image file exists.
     * Only when the image itself is broken, it will throw an exception, but there `optName` won't
     * be used, so you can leave it empty.
     *
     * @param readImage decides whether the image should be read or just checked for existence.
     * When using this function only to check whether the argument format is correct, it would be
     * too wasteful to read the Image, especially for large images.
     *
     * @param usageImage is the Descriptor vector that specifies the sub-options. So by providing a
     * different vector than the default, you could change the sub-option names described below,
     * but the purpose of a user specified usageImage is actually that you can add options on which
     * Parse::Image should not throw. So for example if you like to have an image option that
     * requires another sub-option called `foo`, which receives a string argument, you could use
     * the following code:
     * @code
     * std::pair<std::string, Image>
     * parseFooImg(std::string inputArgument = "-f test.tif  --foo=bar  -l 0") {                 // example for command line input
     *     std::vector<Descriptor> usageFooImg = Parse::usageImage;                              // copy default
     *     usageFooImg.push_back(Descriptor{"FOO", "", "", "foo", ArgChecker::NonEmpty, ""});    // add --foo option
     *
     *     auto fooOptions = OptionParser::parse(usageFooImg, inputArgument);                    // OptionParser just for --foo option
     *     if (fooOptions["FOO"].empty())
     *         IF_THROW_EXCEPTION(invalid_argument_error("Option foo is required and missing"));
     *
     *     std::string foo = fooOptions["FOO"].back().arg;
     *     Image img = Parse::Image(inputArgument, "", true, usageFooImg);                       // does not throw on --foo option
     *     return {foo, img};
     * }
     * @endcode
     *
     * In your usage description, you can use the following, where you replace `-i` and `--image`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageImage =
     *     "  -i <img>, \t--image=<img> \tDescription text.\v"
     *     "<img> can be a file path. If cropping or using only a subset of channels / layers "
     *     "is desired, <img> must have the form '-f <file> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', "
     *     "where the arguments can have an arbitrary order. "
     *     "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\v"
     *     "  -f <file>,     --file=<file>       Specifies the image file path.\v"
     *     "  -l <num-list>, --layers=<num-list> Optional. Specifies the channels or layers, that will be read. Hereby a 0 means the first channel.\v"
     *     "                                     <num-list> must have the format '(<num> [<num> ...])', without commas in between or just '<num>'.\v"
     *     "  -c <rect>,     --crop=<rect>       Optional. Specifies the crop window, where the "
     *     "image will be read. A zero width or height means full width "
     *     "or height, respectively.\v"
     *     "<rect> requires all of the following arguments:\v"
     *     "  -x <num>                 x start\v"
     *     "  -y <num>                 y start\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "Examples: --image=some_image.tif\v"
     *     "          --image='-f \"test image.tif\" --crop=(-x 1 -y 2 -w 3 -h 2) -l (0 2)'\v"
     *     "          --image='-f \"test image.tif\" -l 0'\n";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<img\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Image. It will parse the option
     * argument once and check the file for existence (but not read the image) to check if
     * everything can be parsed like specified in the format above.
     *
     * If you want to have additionally a date and a resolution tag, see MRImage().
     *
     * @return the parsed Image object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Image Image(std::string const& str, std::string const& optName = "", bool readImage = true, std::vector<Descriptor> const& usageImage = Parse::usageImage);

    /**
     * @brief %Parse a multi-resolution Image from a string
     *
     * @param str is the string that contains the image filename, a date and a resolution tag.
     * Optionally it can also contain a crop window and the layers. For details, see below in the
     * usage description. As an example, with `--file=test.tif --date=1 --tag=fine --layers=1`
     * would be valid.
     *
     * @param optName is the option name where this Image argument is specified. For example with
     * `--image="--file=test.tif --date=1 --tag=fine"` the option name could be `--image` or just
     * `image`. It is only used to provide better error messages. Usually, when you parse an image,
     * because of the checking function, you can be sure that the argument format is correct and
     * the image file exists. Only when the image itself is broken, it will throw an exception, but
     * there `optName` won't be used, so you can leave it empty.
     *
     * @param readImage decides whether the image should be read or just checked for existence.
     * When using this function only to check whether the argument format is correct, it would be
     * too wasteful to read the Image, especially for large images.
     *
     * @param isDateOpt means "Is date optional?". So when set to false and the date option is not
     * found, this will throw an error. When set to true and the date option is not found, the date
     * will be set to 0. Use ImageHasDate() to check, whether the image has a date option.
     *
     * @param isTagOpt means "Is the resolution tag optional?". So when set to false and the tag
     * option is not found, this will throw an error. When set to true, and the tag option is not
     * found, the tag will be set to an empty string. Use ImageHasTag() to check, whether the image
     * has a date option.
     *
     * @param usageMRImage is the Descriptor vector that specifies the sub-options. So by providing
     * a different vector than the default, you could change the sub-option names described below,
     * but the purpose of a user specified usageMRImage is actually that you can add options on
     * which Parse::MRImage should not throw. So for example if you like to have an
     * multi-resolution image option that requires another sub-option called `foo`, which receives
     * a string argument, you could use the following code:
     * @code
     * std::pair<std::string, ImageInput>
     * parseFooImg(std::string inputArgument = "-f test.tif  --foo=bar  -l 0  -t high  -d 0") {  // example for command line input
     *     std::vector<Descriptor> usageFooImg = Parse::usageMRImage;                            // copy default
     *     usageFooImg.push_back(Descriptor{"FOO", "", "", "foo", ArgChecker::NonEmpty, ""});    // add --foo option
     *
     *     auto fooOptions = OptionParser::parse(usageFooImg, inputArgument);                    // OptionParser just for --foo option
     *     if (fooOptions["FOO"].empty())
     *         IF_THROW_EXCEPTION(invalid_argument_error("Option foo is required and missing"));
     *
     *     std::string foo = fooOptions["FOO"].back().arg;
     *     ImageInput img = Parse::MRImage(inputArgument, "", true, false, false, usageFooImg);  // does not throw on --foo option
     *     return {foo, img};
     * }
     * @endcode
     *
     * In your usage description, you can use the following, where you replace `-i` and `--image`
     * by your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageImage =
     *     "  -i <img>, \t--image=<img> \tDescription text.\v"
     *     "<img> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', "
     *     "where the arguments can have an arbitrary order. "
     *     "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\v"
     *     "  -f <file>,     --file=<file>       Specifies the image file path.\v"
     *     "  -d <num>,      --date=<num>        Specifies the date.\v"
     *     "  -t <tag>,      --tag=<tag>         Specifies the resolution tag string. <tag> can be an arbitrary string.\v"
     *     "  -c <rect>,     --crop=<rect>       Optional. Specifies the crop window, where the image will be read. A zero width or height means full width "
     *     "or height, respectively. For a description of <rect> see --rectangle=<rect>!\v"
     *     "  -l <num-list>, --layers=<num-list> Optional. Specifies the channels or layers, that will be read. Hereby a 0 means the first channel.\v"
     *     "                                     <num-list> must have the format '(<num> [<num> ...])', without commas in between or just '<num>'.\v"
     *     "Examples: --image='-f  some_image.tif  -d 0  -t HIGH'\v"
     *     "          --image='-f \"test image.tif\"  -d 1  -t HIGH  --crop=(-x 1 -y 2 -w 3 -h 2) -l (0 2)'";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<img\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::MRImage(). It will parse the
     * option argument once and check the file for existence (but not read the image) to check if
     * everything can be parsed like specified in the format above. To specify that date or tag are
     * optional the boolean template arguments can be used, e. g. `ArgChecker::MRImage<true,
     * true>()` if both should be optional.
     *
     * @return the parsed ImageInput object. This contains the image, the date (being 0 if date is
     * optional and not provided) and the resolution tag (being empty if tag is optional and not
     * provided).
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static ImageInput MRImage(std::string const& str, std::string const& optName = "", bool readImage = true, bool isDateOpt = false, bool isTagOpt = false, std::vector<Descriptor> const& usageMRImage = Parse::usageMRImage);

    /**
     * @brief %Parse a multi-resolution Image from a string and set it in a collection
     *
     * @param str is the string that contains the image in the format described at MRImage().
     *
     * @param mri is the MultiResImages collection in which the image will be set.
     *
     * @param optName is the option name where this Image argument is specified. For example with
     * `--image="--file=test.tif --date=1 --tag=fine"` the option name could be `--image` or just
     * `image`. It is only used to provide better error messages. Usually, when you parse an image,
     * because of the checking function, you can be sure that the argument format is correct and
     * the image file exists. Only when the image itself is broken, it will throw an exception, but
     * there `optName` won't be used, so you can leave it empty.
     *
     * The format is the same as in MRImage(), see there for a detailed description. This function
     * does not return an ImageInput, but instead sets the Image in the given MultiResImages
     * collection `mri`.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static void AndSetMRImage(std::string const& str, MultiResImages& mri, std::string const& optName = "");

    /**
     * @brief %Parse a plain mask image from a filename given as string
     *
     * @param str is the string that contains the mask filename and optionally a crop window,
     * layers, bits and valid and invalid ranges. The string has to follow a specific format, see
     * below in the usage description. As an example `--file=test.tif --crop=(-x 1 -y 2 -w 4 -h 2)
     * --layers=1 --extract-bits=6,7 --valid-ranges=[3,3]` would be valid as well as just a plain
     * filename like `"test mask.png"`.
     *
     * @param optName is the option name where this mask argument is specified. For example with
     * `--mask=test.tif` the option name could be `--mask` or just `mask`. It is only used to
     * provide better error messages. Usually, when you parse a mask, because of the checking
     * function, you can be sure that the argument format is correct and the mask image file
     * exists. Only when the mask image itself is broken, it will throw an exception, but there
     * `optName` won't be used, so you can leave it empty.
     *
     * @param readImage decides whether the image should be read and converted to a boolean mask or
     * just checked for existence. When using this function only to check whether the argument
     * format is correct, it would be too wasteful to read the image, especially for large images.
     *
     * @param usageMask is the Descriptor vector that specifies the sub-options. So by providing a
     * different vector than the default, you could change the sub-option names described below,
     * but the purpose of a user specified usageMask is actually that you can add options on which
     * Parse::Mask should not throw. So for example if you like to have an image option that
     * requires another sub-option called `foo`, which receives a string argument, you could use
     * the following code:
     * @code
     * std::pair<std::string, Image>
     * parseFooImg(std::string inputArgument = "-f m.tif  --foo=bar  -b 6,7  --valid-ranges=[3,3]") { // example for command line input
     *     std::vector<Descriptor> usageFooMask = Parse::usageMask;                              // copy default
     *     usageFooMask.push_back(Descriptor{"FOO", "", "", "foo", ArgChecker::NonEmpty, ""});   // add --foo option
     *
     *     auto fooOptions = OptionParser::parse(usageFooMask, inputArgument);                   // OptionParser just for --foo option
     *     if (fooOptions["FOO"].empty())
     *         IF_THROW_EXCEPTION(invalid_argument_error("Option foo is required and missing"));
     *
     *     std::string foo = fooOptions["FOO"].back().arg;
     *     Image mask = Parse::Mask(inputArgument, "", true, usageFooMask);                      // does not throw on --foo option
     *     return {foo, mask};
     * }
     * @endcode
     *
     * In your usage description, you can use the following, where you replace `-m` and `--mask` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageMask =
     *     "  -m <msk>, \t--mask=<msk> \tDescription text.\v"
     *     "<msk> can be a file path. If cropping or using only a subset of channels / layers "
     *     "is desired, <msk> must have the form '-f <file> [-c <rect>] [-l <num-list>] [-b <num-list>] [--valid-ranges=<range-list>] [--invalid-ranges=<range-list>] [--disable-use-color-table]', "
     *     "where the arguments can have an arbitrary order. "
     *     "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\v"
     *     "  -f <file>,     --file=<file>       Specifies the image file path.\v"
     *     "  -l <num-list>, --layers=<num-list> Optional. Specifies the channels or layers, that will be read. Hereby a 0 means the first channel.\v"
     *     "  -c <rect>,     --crop=<rect>       Optional. Specifies the crop window, where the image will be read. A zero width or height means full width or height, respectively.\v"
     *     "  -b <num-list>, --extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted "
     *     "from the quality layer image and then shifted to the least significant positions. By default all bits will be used.\v"
     *     "  --valid-ranges=<range-list>        Specifies the ranges of the shifted value (see --extract-bits) that should mark the location as valid (true; 255). "
     *     "Can be combined with --invalid-ranges.\v"
     *     "  --invalid-ranges=<range-list>      Specifies the ranges of the shifted value (see --extract-bits) that should mark the location as invalid (false; 0). "
     *     "Can be combined with --valid-ranges.\v"
     *     "<range-list> must have the form '<range> [<range> ...]', where the brackets mean that further intervals are optional. The different ranges are related as union.\v"
     *     "<range> should have the format '[<int>,<int>]', where the comma is optional, but the square brackets are actual characters here. Additional whitespace can be added anywhere.\v"
     *     "If you neither specify valid ranges nor invalid ranges, the conversion to boolean will be done by using true for all values except 0.\v"
     *     "<num-list> must have the format '(<num>, [<num>, ...])', without commas in between or just '<num>'.\v"
     *     "<rect> requires all of the following arguments:\v"
     *     "  -x <num>                 x start\v"
     *     "  -y <num>                 y start\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "Examples: --mask=some_image.png\v"
     *     "  Reads some_image.png (converts a possibly existing color table) and converts then 0 values to false (0) and every other value to true (255).\v"
     *     "          --mask='-f \"test image.tif\"  --crop=(-x 1 -y 2 -w 3 -h 2)  -l (0 2) -b 6,7  --valid-ranges=[3,3]'\v"
     *     "  Reads "test image.tif" and converts all values to false (0) except where bit 6 and bit 7 are both set. These will be set to true (255).\v"
     *     "          --mask='-f \"test.tif\"  -b 7 -b 6 -b 0  --valid-ranges=[1,7]  --invalid-ranges=[3,3]'\v"
     *     "  Reads test.tif and converts all values to true (255) where any of bits 0, 6 and 7 is set, but not if bit 6 and 7 are set and bit 0 is clear.\n";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<msk\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Mask. It will parse the option
     * argument once and check the file for existence (but not read the image) to check if
     * everything can be parsed like specified in the format above.
     *
     * If you want to have additionally a date and a resolution tag, see MRMask().
     *
     * @return the parsed image converted to a multi-channel mask Image. This means the base type
     * will be Type::uint8 and it will contain only values 0 and 255.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static imagefusion::Image Mask(std::string const& str, std::string const& optName = "", bool readImage = true, std::vector<Descriptor> const& usageMask = Parse::usageMask);

    /**
     * @brief %Parse a multi-resolution mask image from a filename given as string
     *
     * @param str is the string that contains the mask filename, a date and a resolution tag.
     * Optionally it can also contain a crop window, layers, bits and valid and invalid ranges. The
     * string has to follow a specific format, see below in the usage description. As an example
     * `--file=test.tif --date=1 --tag=fine --crop=(-x 1 -y 2 -w 4 -h 2) --layers=1
     * --extract-bits=6,7 --valid-ranges=[3,3]` would be valid as well as just a plain filename
     * like `"test mask.png"`.
     *
     * @param optName is the option name where this mask argument is specified. For example with
     * `--mask="test.tif -d 0 -t h"` the option name could be `--mask` or just `mask`. It is only
     * used to provide better error messages. Usually, when you parse a mask, because of the
     * checking function, you can be sure that the argument format is correct and the mask image
     * file exists. Only when the mask image itself is broken, it will throw an exception, but
     * there `optName` won't be used, so you can leave it empty.
     *
     * @param readImage decides whether the image should be read and converted to a boolean mask or
     * just checked for existence. When using this function only to check whether the argument
     * format is correct, it would be too wasteful to read the image, especially for large images.
     *
     * @param isDateOpt means "Is date optional?". So when set to false and the date option is not
     * found, this will throw an error. When set to true and the date option is not found, the date
     * will be set to 0. Use ImageHasDate() to check, whether the image has a date option.
     *
     * @param isTagOpt means "Is the resolution tag optional?". So when set to false and the tag
     * option is not found, this will throw an error. When set to true, and the tag option is not
     * found, the tag will be set to an empty string. Use ImageHasTag() to check, whether the image
     * has a date option.
     *
     * @param usageMRMask is the Descriptor vector that specifies the sub-options. So by providing a
     * different vector than the default, you could change the sub-option names described below,
     * but the purpose of a user specified usageMask is actually that you can add options on which
     * Parse::MRMask should not throw. So for example if you like to have an image option that
     * requires another sub-option called `foo`, which receives a string argument, you could use
     * the following code:
     * @code
     * std::pair<std::string, ImageInput>
     * parseFooImg(std::string inputArgument = "-f m.tif  --foo=bar  -b 6,7  --valid-ranges=[3,3]  -t high  -d 0") { // example for command line input
     *     std::vector<Descriptor> usageFooMask = Parse::usageMRMask;                              // copy default
     *     usageFooMask.push_back(Descriptor{"FOO", "", "", "foo", ArgChecker::NonEmpty, ""});     // add --foo option
     *
     *     auto fooOptions = OptionParser::parse(usageFooMask, inputArgument);                     // OptionParser just for --foo option
     *     if (fooOptions["FOO"].empty())
     *         IF_THROW_EXCEPTION(invalid_argument_error("Option foo is required and missing"));
     *
     *     std::string foo = fooOptions["FOO"].back().arg;
     *     ImageInput mrMask = Parse::MRMask(inputArgument, "", true, false, false, usageFooMask); // does not throw on --foo option
     *     return {foo, mrMask};
     * }
     * @endcode
     *
     * In your usage description, you can use the following, where you replace `-m` and `--mask` by
     * your chosen option names and `Description text` by a useful description, what the option
     * does:
     * @code
     * const char* usageMRMask =
     *     "  -m <msk>, \t--mask=<msk> \tDescription text.\v"
     *     "<msk> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>] [-b <num-list>] [--valid-ranges=<range-list>] [--invalid-ranges=<range-list>] [--disable-use-color-table]', "
     *     "where the arguments can have an arbitrary order. "
     *     "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\v"
     *     "  -f <file>,     --file=<file>       Specifies the image file path.\v"
     *     "  -d <num>,      --date=<num>        Specifies the date.\v"
     *     "  -t <tag>,      --tag=<tag>         Specifies the resolution tag string. <tag> can be an arbitrary string.\v"
     *     "  -l <num-list>, --layers=<num-list> Optional. Specifies the channels or layers, that will be read. Hereby a 0 means the first channel.\v"
     *     "  -c <rect>,     --crop=<rect>       Optional. Specifies the crop window, where the image will be read. A zero width or height means full width or height, respectively.\v"
     *     "  -b <num-list>, --extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted "
     *     "from the quality layer image and then shifted to the least significant positions. By default all bits will be used.\v"
     *     "  --valid-ranges=<range-list>        Specifies the ranges of the shifted value (see --extract-bits) that should mark the location as valid (true; 255). "
     *     "Can be combined with --invalid-ranges.\v"
     *     "  --invalid-ranges=<range-list>      Specifies the ranges of the shifted value (see --extract-bits) that should mark the location as invalid (false; 0). "
     *     "Can be combined with --valid-ranges.\v"
     *     "<range-list> must have the form '<range> [<range> ...]', where the brackets mean that further intervals are optional. The different ranges are related as union.\v"
     *     "<range> should have the format '[<int>,<int>]', where the comma is optional, but the square brackets are actual characters here. Additional whitespace can be added anywhere.\v"
     *     "If you neither specify valid ranges nor invalid ranges, the conversion to boolean will be done by using true for all values except 0.\v"
     *     "<num-list> must have the format '(<num>, [<num>, ...])', without commas in between or just '<num>'.\v"
     *     "<rect> requires all of the following arguments:\v"
     *     "  -x <num>                 x start\v"
     *     "  -y <num>                 y start\v"
     *     "  -w <num>, --width=<num>  width\v"
     *     "  -h <num>, --height=<num> height\v"
     *     "Examples: --mask='-f \"test image.tif\"  -d 0  -t HIGH  --crop=(-x 1 -y 2 -w 3 -h 2)  -l (0 2) -b 6,7  --valid-ranges=[3,3]'\v"
     *     "  Reads "test image.tif" and converts all values to false (0) except where bit 6 and bit 7 are both set. These will be set to true (255).\v"
     *     "          --mask='-f \"test.tif\"  -d 0  -t HIGH  -b 7 -b 6 -b 0  --interp-ranges=[1,7]  --non-interp-ranges=[3,3]'\v"
     *     "  Reads test.tif and converts all to true (255) if any of bits 0, 6 and 7 is set, but not if bit 6 and 7 are set and bit 0 is clear.\n";
     * @endcode
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"\<msk\> ...` and below will be aligned with `Description text`.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::MRMask. It will parse the option
     * argument once and check the file for existence (but not read the image) to check if
     * everything can be parsed like specified in the format above. To specify that date or tag are
     * optional the boolean template arguments can be used, e. g. `ArgChecker::MRMask<true,
     * true>()` if both should be optional.
     *
     * @return the parsed image converted to a multi-channel mask Image. This means the base type
     * will be Type::uint8 and it will contain only values 0 and 255.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    static ImageInput MRMask(std::string const& str, std::string const& optName = "", bool readImage = true, bool isDateOpt = false, bool isTagOpt = false, std::vector<Descriptor> const& usageMRMask = Parse::usageMRMask);

    /**
     * @brief %Parse the filename of an Image or MRImage argument
     *
     * @param str is the string that either contains the image filename as option
     * `--file=<filename>` / `-f <filename>` or is just the `<filename>`.
     *
     * @return the filename for the image.
     *
     * @throws invalid_argument_error if the string does not comply to the described format or the
     * image does not exist.
     */
    static std::string ImageFileName(std::string const& str);

    /**
     * @brief %Parse the layer specification vector of an Image or MRImage argument
     *
     * @param str is the string that may contain a layer vector as option `--layers=<num-list>` /
     * `-l <num-list>`.
     *
     * @return the vector of layers or an empty vector if none has been specified.
     *
     * @throws invalid_argument_error if the string does not comply to the format of Image or
     * MRImage.
     */
    static std::vector<int> ImageLayers(std::string const& str);

    /**
     * @brief %Parse the crop window of an Image or MRImage argument
     *
     * @param str is the string that may contain a crop window as option `--crop=<rectangle>` / `-c
     * <rectangle>`.
     *
     * @return the specified crop window or an empty imagefusion::CoordRectangle, if not specified.
     *
     * @throws invalid_argument_error if the string does not comply to the format of Image or
     * MRImage.
     */
    static imagefusion::CoordRectangle ImageCropRectangle(std::string const& str);

    /**
     * @brief %Parse the date of an MRImage argument
     *
     * @param str is the string that must contain a date as option `--date=<num>` / `-d <num>`.
     *
     * @return the date.
     *
     * @throws invalid_argument_error if the string does not comply to the format of MRImage, i. e.
     * also if the date option is not present.
     */
    static int ImageDate(std::string const& str);

    /**
     * @brief Check whether the argument string specifies a date
     * @param str is argument string that might contain a date option
     * @return true if it contains, false if not
     */
    static bool ImageHasDate(std::string const& str);

    /**
     * @brief %Parse the tag of an MRImage argument
     *
     * @param str is the string that must contain a tag as option `--tag=<string>` / `-t <string>`.
     *
     * @return the tag.
     *
     * @throws invalid_argument_error if `str` does not comply to the format of MRImage, i. e. also
     * if the tag option is not present.
     */
    static std::string ImageTag(std::string const& str);

    /**
     * @brief Check whether the argument string specifies an image tag
     * @param str is argument string that might contain a tag option
     * @return true if it contains, false if not
     */
    static bool ImageHasTag(std::string const& str);

    /**
     * @brief Check whether a possibly existing color table should be ignored.
     * @param str is argument string that determines whether it should be ignored or not
     * @return true if it contains, false if not
     */
    static bool ImageIgnoreColorTable(std::string const& str);

    /**
     * @brief %Parse the template type from a string
     *
     * @tparam T is the type to parse
     *
     * @param str is the string to parse.
     *
     * @param optName is the option name where this argument is specified. It is only used to
     * provide better error messages. Usually, when you parse something, you can be sure that no
     * error occurs, because the checking function has parsed it already by using this function.
     *
     * This method will forward the parsing to the appropriate method to parse a `T`. E. g. if `T`
     * is a double, it will call Float(). It is useful in templated parsing functions. Here is the
     * full list:
     * `T`                               | method used
     * ----------------------------------|----------------------
     * `double`                          | Parse::Float
     * `int`                             | Parse::Int
     * `imagefusion::Type`               | Parse::Type
     * `imagefusion::Interval`           | Parse::Interval
     * `imagefusion::IntervalSet`        | Parse::IntervalSet
     * `imagefusion::Size`               | Parse::Size
     * `imagefusion::Dimensions`         | Parse::Dimensions
     * `imagefusion::Point`              | Parse::Point
     * `imagefusion::Coordinate`         | Parse::Coordinate
     * `imagefusion::Rectangle`          | Parse::Rectangle
     * `imagefusion::CoordRectangle`     | Parse::CoordRectangle
     * `imagefusion::Image`              | Parse::Image
     * `imagefusion::option::ImageInput` | Parse::MRImage
     * `std::vector<T>`                  | Parse::Vector<T>
     * `T` (none of the above)           | [std::istringstream::operator>>](http://en.cppreference.com/w/cpp/io/basic_istream/operator_gtgt)
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Arg<T>. It will parse the option
     * argument by calling this function to see whether it throws an error.
     *
     * @note There is currently no way to parse an @ref Parse::Angle "angle" or a @ref
     * Parse::GeoCoord "geographic location (latitude / longitude)", since they parse double and
     * @ref imagefusion::Coordinate "Coordinate", respectively. So these do not provide distinct
     * types, but only distinct formats.
     *
     * @return the parsed object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format of type
     * `T`. However, if you used the corresponding checking function this can only happen while
     * using the OptionParser and not at the second run, when you actually use the argument.
     */
    template <typename T>
    static T Arg(std::string const& str, std::string const& optName = "") {
        return ArgImpl<T>::Arg(str, optName);
    }

    // exclude implementation from doxygen documentation
    /// \cond
    template <typename T>
    struct ArgImpl;
    /// \endcond

    /**
     * @brief %Parse a std::vector from a string
     *
     * @tparam T is the type of each vector element.
     *
     * @param str is the string that contains the vector. This has to follow a specific format, see
     * below in the usage description. As an example `3.432 4` would be valid, but commas and
     * parens are not allowed.
     *
     * @param optName is the option name where this vector argument is specified. For example with
     * `--vector-double="3.432 4"` the option name could be `--vector-double` or just
     * `vector-double`. It is only used to provide better error messages. Usually, when you parse a
     * vector, you can be sure that no error occurs, because the checking function has parsed it
     * already by using this function.
     *
     * In your usage description, you can use something like the following, where you replace `-v`
     * and `--vector-double` by your chosen option names, `Description text` by a useful
     * description, what the option does and `float` by the used data type. Also the examples have
     * to be changed.
     * @code
     * const char* usageVector =
     *     "  -v <float-list>, \t--vector-double=<float-list> \tDescription text\v"
     *     "<float-list> must have the format '(<float>[,] [<float>[,] ...])' or just '<float>'.\v"
     *     "Examples: --vector-double='3.1416, 42, -1.5'"
     *     "          --vector-double='(3.1416) (42) (-1.5)'"
     *     "          --vector-double=3.1416";
     * @endcode
     *
     * Note, that `\v` will go to the next table line, but stay in the same column. So the lines
     * `"<float-list> ...` and below will be aligned with `Description text`.
     *
     * To parse the string this function separates the arguments into tokens (with
     * separateArguments). This allows quoting (parens, single or double quotes) to have spaces in
     * elements. As separator commas or any whitespace can be used. This also means if an element
     * contains a comma quoting is required to protect it. E. g. for a vector of Point%s `"(-1, 2),
     * (3 4) (-x=5 -y=6)"` would be parsed to three elements (-1, 2), (3, 4), and (5, 6), but
     * `"-1,2"` to parse a single point would not be valid! Then, Parse::Arg() is used to try to
     * parse each T. For unknown T (see the table in Parse::Arg()) operator>>(std::istream&, T&) is
     * used. So if you have a custom class you can either define operator>>(std::istream&, T&) in
     * the same namespace, where your custom class is defined or you can specialize the Arg method
     * (or for partial specialization the undocumented ArgImpl::Arg method) for your custom type.
     * For all the types that can be parsed with the methods in Parse, there exists an overload.
     *
     * As argument check (Descriptor::checkArg), use ArgChecker::Vector<T>. It will parse the
     * option argument once to check if everything can be parsed like specified in the format
     * above.
     *
     * \parblock
     * \note It does not make sense to parse a vector<std::string> with this, since the operator>>
     * stops at whitespace. Use just separateArguments(std::string const&) instead!
     * \endparblock
     *
     * \note You can also make your own specialization, if you prefer this instead of overloading
     * operator>>. The pattern is quite simple:
     * @code
     * template<>
     * inline std::vector<YourType> Parse::Vector(std::string const& str, std::string const& optName) {
     *     std::vector<std::string> strVec = separateArguments(str);
     *     std::vector<YourType> ret;
     *     for (std::string const& s : strVec)
     *         ret.push_back(ParseYourType(s, optName)); // optName for better error messages
     *     return ret;
     * }
     * @endcode
     * With this, you can also directly use `ArgChecker::Vector<YourType>`
     *
     * @return the parsed vector object.
     *
     * @throws invalid_argument_error if the string does not comply to the described format.
     * However, if you used the corresponding checking function this can only happen while using
     * the OptionParser and not at the second run, when you actually use the argument.
     */
    template<typename T>
    static std::vector<T> Vector(std::string const& str, std::string const& optName = "");
};


/**
 * @brief A parsed option from the command line together with its argument.
 *
 * The most important thing you can do with an option, is requesting its argument with Option::arg
 * to parse it. You get in touch with Option objects when you index a group of options in the
 * OptionsParser object, like:
 * @code
 * auto options = OptionParser::parse(usage, argc, argv); // checking also option arguments ...might fail here
 * std::vector<Option>& numOpts = options["NUMBER"];
 * for (Option& o : numOpts) {
 *     int n = Parse::Int(o.arg); // will not fail here, if ArgChecker::Int has been used for this option in usage
 *     ...
 * }
 * @endcode
 * Usually, when you access an option argument, it has already been checked by the checking
 * function (see @ref Descriptor::checkArg and @ref ArgChecker) while parsing, as mentioned by the
 * comments in the code fragment above. So you can be sure that the argument can be parsed the way
 * you need it. If there is no argument, the string will be empty.
 *
 * If you collect unknown options (see @ref OptionParser::unknownOptionArgCheck), the name is often
 * more interesting than the argument (if it accepts arguments at all).
 *
 * @see OptionParser, Descriptor
 */
class Option
{
public:
    /**
     * @brief The name of the option as used on the command line without dashes.
     *
     * The main purpose of this is to display the user the failing option in error messages.
     * However, for unknown options the option name is contained here.
     */
    std::string name;

    /**
     * @brief This Option's argument (or empty).
     *
     * So if an option
     *
     *     --number=7
     * is given on command line and now `o` is the Option object corresponding to it, `arg` will be
     * just `"7"`.
     *
     * If no or an empty argument has been given, this will be empty. If this is just a string
     * argument, you can use it as it is. Otherwise have a look at @ref Parse, which provides a lot
     * of functions to parse string arguments into other types.
     */
    std::string arg;

    /**
     * @brief Pointer to this Option's Descriptor.
     *
     * You can access the option specifier via Option::spec() and the additional option property
     * via Option::prop().
     *
     * Note, for unknown options this is `nullptr`.
     */
    const Descriptor* desc;

    /**
     * @brief Creates a new Option with empty name and arg and a `nullptr` for desc.
     */
    Option() :
       name{}, arg{}, desc{nullptr}
    {
    }

    /**
     * @brief Creates a new Option with the given arguments.
     * @param desc_ is a pointer to the Descriptor that matches this option.
     * @param name_ is the name of the option.
     * @param arg_ is the argument of the option.
     */
    Option(const Descriptor* desc_, std::string name_, std::string arg_)
        : name{std::move(name_)}, arg{std::move(arg_)}, desc{desc_}
    { }

    /**
     * @brief Get the specified property of this option
     *
     * This provides an additional property for an option. For example suppose multiple options
     * belong to the same option group. Then the Descriptor vector should be defined similarly as:
     * @code
     * std::vector<Descriptor> usage = {
     *   ...
     *   {"STUFF", "DISABLE", "", "disable-stuff", ArgChecker::None, "  --disable-stuff \tThis disables stuff."},
     *   {"STUFF", "ENABLE",  "", "enable-stuff",  ArgChecker::None, "  --enable-stuff \tThis enables stuff."}
     *   ...
     * };
     * @endcode
     * After parsing the parameters into the OptionParser object `options`, one can ask for the
     * property of the last specified STUFF-option:
     * @code
     * if (options["STUFF"].back().prop() == "ENABLE")
     *     ...
     * @endcode
     * If the property is not required, it can be set to an empty string `""` in the Descriptor
     * vector.
     *
     * So this returns the Descriptor::prop string. However, for unknown options it will return an
     * empty string. The unknown options can be accessed with
     * @code
     * options.unknown
     * @endcode
     *
     * @return property value @c desc->prop if @ref desc is not `nullptr` and an empty string
     * otherwise.
     */
    std::string prop() const;

    /**
     * @brief Get the option specification value.
     *
     * This returns the associated option specifier (ID), which was given as the first value in a
     * Descriptor element. E. g. suppose you have a Descriptor vector like
     * @code
     * std::vector<Descriptor> usage = {
     *   ...
     *   {"STUFF", "", "s", "stuff", ArgChecker::None, "  -s, \t--stuff \tStuff."},
     *   ...
     * };
     * @endcode
     * After parsing the parameters into the OptionParser object `options, you can get the group of
     * these options as
     * @code
     * options["STUFF"]
     * @endcode
     * which is a vector of Option%s. Each of these Option%s has its specifier set to `"STUFF"`.
     *
     * The specifier is more useful when processing the command line parameters in their original
     * order, like
     * @code
     * for (Option& opt : options.input) {
     *     if (opt.spec() == "NUMBER") {
     *         int n = Parse::Int(opt.arg);
     *         ...
     *     }
     *     else if (opt.spec() == "FILE") {
     *         std::string& fname = opt.arg;
     *         ...
     *     }
     *     else if (opt.spec() == "STUFF") {
     *         ...
     *     }
     *     ...
     * }
     * @endcode
     *
     * So this returns the Descriptor::spec string. However, for unknown options it will return an
     * empty string. The unknown options can be accessed with
     * @code
     * options.unknown
     * @endcode
     *
     * @return the option specifier @c desc->spec if @ref desc is not `nullptr` and an empty string
     * otherwise.
     */
    std::string spec() const;
};

/**
 * @brief Convert an Option to a string
 * @param o is the option to convert
 * @return simply `o.name + " " + o.arg`. Examples for print outs:
 *
 *     number 123
 *     n 123
 */
inline std::string to_string(Option const& o) {
    return o.name + (o.arg.empty() ? "" : " ") + o.arg;
}

/**
 * @brief Output an Option to a stream
 * @param out is the output stream
 * @param o is the option to output
 *
 * Simply prints the string that to_string(Option const& o) returns. Examples for print outs:
 *
 *     number 123
 *     n 123
 *
 * @return `out` to append further strings.
 */
inline std::ostream& operator<<(std::ostream& out, Option const& o) {
    return out << to_string(o);
}




/**
 * @brief Possible results when checking if an argument is valid for a certain option.
 *
 * In the case that no argument is provided for an option that takes an optional argument, return
 * codes @c ArgStatus::OK and @c ArgStatus::IGNORE are equivalent.
 */
enum class ArgStatus
{
  /// The option does not take an argument.
  NONE,
  /// The argument is acceptable for the option.
  OK,
  /// The argument is not acceptable but that's non-fatal because the option's argument is optional.
  IGNORE,
  /// The argument is not acceptable and that's fatal.
  ILLEGAL
};


/**
 * @brief Collection of static methods to check different kinds of option arguments
 *
 * You specify in the Descriptor which function should check a potential argument. The function
 * must have the signature @ref CheckArg, like all of the methods of ArgChecker have. The function
 * decides whether the option accepts arguments at all and if so it can check whether it complies
 * to what is expected. So when an option does not require an argument, ArgChecker::None can be
 * used, which will always return ArgStatus::NONE. Thus an argument following the option will not
 * be consumed. ArgChecker is hence a collection of argument checking functions for different
 * purposes. For example if an option must have an integer argument you can write:
 * @code
 * std::vector<Descriptor> usage{
 *     ...
 *     { ..., ArgChecker::Int, ...},
 *     ...
 * };
 * @endcode
 *
 * These checking functions are called while the option parser is working, so usually in the call
 * `options.parse(argc, argv);` or `OptionParse::parse(usage, argc, argv)`. The function in the
 * example will try to parse the argument as integer to see whether the argument is valid. If it
 * does not work, it will throw an @ref invalid_argument_error.
 *
 * However, not all types of arguments are completely parsed. ArgChecker::Image for example will
 * not read in the whole image just to check whether it works. It will instead just check whether
 * the file exists. If it does not exist, it throws an invalid_argument_error as well.
 *
 * There are many different checking functions. Some just are more general, like
 *  * ArgChecker::None, which does not accept any argument,
 *  * ArgChecker::NonEmpty, which requires an arbitrary argument to not fail and
 *  * ArgChecker::Optional, which does accept an arbitrary argument, but does also not fail, if
 *    there is no argument to consume.
 *
 * Others require a specific format, since they try to parse the argument, like
 *  * ArgChecker::Int, which checks for an integer argument,
 *  * ArgChecker::Size, which checks for a @ref Size argument
 *  * and many more.
 *
 * You can also extend the ArgChecker with your own checking functions for a custom argument, like
 * shown in the following example:
 * @code
 * struct MyArgChecker : public imagefusion::option::ArgChecker {
 *     static imagefusion::option::ArgStatus MyClass(imagefusion::option::Option const& option) {
 *         if (option.arg.empty())
 *             IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("There was argument given for option '" + option.name + "'"));
 *
 *         parseMyClass(option.arg, option.name); // should throw invalid_argument_error on parsing error
 *         return ArgStatus::OK;
 *     }
 * };
 * @endcode
 * With this you would use in your Descriptor vector `MyArgChecker::MyClass` as checkArg to ensure,
 * that the argument can be parsed as `MyClass`. For other options with ordinary arguments you can
 * also use `MyArgChecker` for consistency, like `MyArgChecker::Int`.
 *
 * Alternatively you can add an explicit specialization of `Parse::Arg<T>` like:
 * @code
 * template<>
 * inline MyClass imagefusion::option::Parse::Arg<MyClass>(std::string const& str, std::string const& optName = "") {
 *     ... // throw invalid_argument_error on parsing error
 * }
 * @endcode
 * and then use `ArgChecker::Arg<MyClass>` for checking.
 */
struct ArgChecker {
    /**
     * @brief Unknown option, never succeeds
     *
     * If this is used for a Descriptor::checkArg, an invalid_argument_error will be thrown for
     * this option by OptionParser::parse.
     *
     * @return ArgStatus::ILLEGAL
     */
    static ArgStatus Unknown(Option const&);

    /**
     * @brief Checks if the argument is a non empty string
     * @param option to be checked
     * @return ArgStatus::OK if the option argument is non empty.
     *
     * @throws invalid_argument_error if the argument is empty.
     */
    static ArgStatus NonEmpty(Option const& option);

    /**
     * @brief Checks if the argument is an integer number
     * @param option to be checked
     *
     * @return ArgStatus::OK if the argument can be parsed as an integer and does not contain a
     * decimal dot.
     *
     * @throws invalid_argument_error if argument contains a decimal dot or cannot be parsed as
     * integer.
     */
    static ArgStatus Int(Option const& option);

    /**
     * @brief Checks if the argument is a floating point number
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed as a double.
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Float().
     */
    static ArgStatus Float(Option const& option);

    /**
     * @brief Checks if the argument is an angle
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed as an angle.
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Angle().
     */
    static ArgStatus Angle(Option const& option);

    /**
     * @brief Checks if the argument is a geographic location
     * @param option to be checked
     *
     * @return ArgStatus::OK if the argument can be parsed as a geographic location (latitude /
     * longitude).
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::GeoCoord().
     */
    static ArgStatus GeoCoord(Option const& option);

    /**
     * @brief Checks if the argument is an image data type
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed as a data type.
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Type().
     */
    static ArgStatus Type(Option const& option);

    /**
     * @brief Checks if the argument is an interval
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed as a interval.
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Interval().
     */
    static ArgStatus Interval(Option const& option);

    /**
     * @brief Checks if the argument is an interval set
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed as a interval set.
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::IntervalSet().
     */
    static ArgStatus IntervalSet(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Image
     * @param option to be checked
     *
     * Note, this uses Parse::Image() with `readImage = false` to check the argument. So the image
     * file is only checked for existence.
     *
     * @return ArgStatus::OK if the argument can be parsed with Parse::Image
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Image().
     */
    static ArgStatus Image(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as multi-resolution Image
     *
     * @tparam isDateOpt (is date optional?) specifies that no error will be thrown if the date
     * option is not specified.
     *
     * @tparam isTagOpt (is tag optional?) specifies that no error will be thrown if the tag option
     * is not specified.
     *
     * @param option to be checked
     *
     * Note, this uses Parse::MRImage() with `readImage = false` to check the argument. So the
     * image file is only checked for existence.
     *
     * @return ArgStatus::OK if the argument can be parsed with Parse::MRImage
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::MRImage().
     */
    template<bool isDateOpt = false, bool isTagOpt = false>
    static ArgStatus MRImage(Option const& option) {
        if (option.arg.empty())
            IF_THROW_EXCEPTION(invalid_argument_error("There was no multi-res image input argument given for option '" + option.name + "'"));

        Parse::MRImage(option.arg, option.name, /*readImage*/ false, isDateOpt, isTagOpt);
        return ArgStatus::OK;
    }

    /**
     * @brief Checks if the argument can be parsed as Mask
     * @param option to be checked
     *
     * Note, this uses Parse::Mask() with `readImage = false` to check the argument. So the image
     * file is only checked for existence.
     *
     * @return ArgStatus::OK if the argument can be parsed with Parse::Mask
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Mask().
     */
    static ArgStatus Mask(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as multi-resolution Mask
     * @tparam isDateOpt (is date optional?) specifies that no error will be thrown if the date
     * option is not specified.
     * @tparam isTagOpt (is tag optional?) specifies that no error will be thrown if the tag option
     * is not specified.
     * @param option to be checked
     *
     * Note, this uses Parse::MRMask() with `readImage = false` to check the argument. So the image
     * file is only checked for existence.
     *
     * @return ArgStatus::OK if the argument can be parsed with Parse::MRMask
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::MRMask().
     */
    template<bool isDateOpt = false, bool isTagOpt = false>
    static ArgStatus MRMask(Option const& option) {
        if (option.arg.empty())
            IF_THROW_EXCEPTION(invalid_argument_error("There was no multi-res mask image input argument given for option '" + option.name + "'"));

        Parse::MRMask(option.arg, option.name, /*readImage*/ false, isDateOpt, isTagOpt);
        return ArgStatus::OK;
    }

    /**
     * @brief Checks if the argument can be parsed as Size
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::Size
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Size().
     */
    static ArgStatus Size(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Size with sub-options only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::SizeSubopts
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::SizeSubopts().
     */
    static ArgStatus SizeSubopts(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Size with special format only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::SizeSpecial
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::SizeSpecial().
     */
    static ArgStatus SizeSpecial(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Dimensions
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::Dimensions
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Dimensions().
     */
    static ArgStatus Dimensions(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Dimensions with sub-options only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::DimensionsSubopts
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Dimensions().
     */
    static ArgStatus DimensionsSubopts(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Dimensions with special format only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::DimensionsSpecial
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::DimensionsSpecial().
     */
    static ArgStatus DimensionsSpecial(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Point
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::Point
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Point().
     */
    static ArgStatus Point(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Point with sub-options only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::PointSubopts
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::PointSubopts().
     */
    static ArgStatus PointSubopts(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Point with special format only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::PointSpecial
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::PointSpecial().
     */
    static ArgStatus PointSpecial(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Coordinate
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::Coordinate
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Coordinate().
     */
    static ArgStatus Coordinate(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Coordinate with sub-options only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::CoordinateSubopts
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::CoordinateSubopts().
     */
    static ArgStatus CoordinateSubopts(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Coordinate with special format only
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::CoordinateSpecial
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::CoordinateSpecial().
     */
    static ArgStatus CoordinateSpecial(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as Rectangle
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::Rectangle
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::Rectangle().
     */
    static ArgStatus Rectangle(Option const& option);

    /**
     * @brief Checks if the argument can be parsed as CoordRectangle
     * @param option to be checked
     * @return ArgStatus::OK if the argument can be parsed with Parse::CoordRectangle
     *
     * @throws invalid_argument_error if the format does not comply to the one described in
     * Parse::CoordRectangle().
     */
    static ArgStatus CoordRectangle(Option const& option);

    /**
     * @brief Checks if the argument is an existing file path
     * @param option to be checked
     * @return ArgStatus::OK if the argument is an existing file path
     *
     * @throws invalid_argument_error if the argument is not an existing file path.
     */
    static ArgStatus File(Option const& option);

    /**
     * @brief For options that don't take an argument.
     *
     * Makes the OptionParser remove a maybe existing attached argument. Does not throw. This can
     * be used for `OptionParser::unknownOptionArgCheck` to collect unknown options without
     * argument.
     *
     * @return ArgStatus::NONE
     */
    static ArgStatus None(Option const&);


    /**
     * @brief For options that may have an optional argument
     * @param option to be checked
     *
     * This will never throw an exception, but eat the next argument (which may be empty), except
     * it is detached and begins with a double dash. So if you use `ArgChecker::Optional` for
     * OptionParser::unknownOptionArgCheck to receive unknown options with argument
     *
     *     --unknown-option argument --unknown-option= non-option
     *     -u argument -u '' non-option
     * `argument` will be eaten as argument by `--unknown-option` and `-u`, but non-option` is not
     * `an argument (but a non-option instead), since the * unknown options before receive an empty
     * `argument.
     *
     * @return ArgStatus::OK if the option has a (maybe unknown) name and ArgStatus::IGNORE
     * otherwise.
     */
    static ArgStatus Optional(Option const& option);


    /**
     * @brief Checks if the argument can be parsed with Parse::Arg<T>
     * @tparam T is the type the option argument is supposed to have
     * @param option to be checked
     *
     * For a non-empty argument just call Parse::Arg<T> to check, whether it can be parsed.
     * However, for `T = Image` and `T = InputImage` it will read the image and drop it. So use
     * ArgCheck::Image and ArgCheck::MRImage instead.
     *
     * @return ArgStatus::OK if the option argument can be parsed with Parse::Arg<T>.
     *
     * @throws invalid_argument_error if the argument is empty or if Parse::Arg<T> throws it.
     */
    template <typename T>
    static T Arg(Option const& option) {
        if (option.arg.empty())
            IF_THROW_EXCEPTION(invalid_argument_error("There was no argument given for option '" + option.name + "'"));

        Parse::Arg<T>(option.arg, option.name);
        return ArgStatus::OK;
    }

    /**
     * @brief Checks if the argument can be parsed as vector of Ts
     * @tparam T is the type of vector elements
     * @param option to be checked
     *
     * In your Descriptor vector you can specify e. g. that you expect a vector of @ref
     * imagefusion::Rectangle "Rectangles", by using `ArgChecker::Vector<Rectangle>` for @ref
     * Descriptor::checkArg "checkArg". Have a look at Parse::Vector() for what types this is
     * supported.
     *
     * @return ArgStatus::OK if the argument can be parsed with Parse::Vector<T>
     *
     * @throws invalid_argument_error if the argument cannot be parsed with Parse::Vector<T>.
     */
    template<typename T>
    static ArgStatus Vector(Option const& option);
};

/**
 * @brief Signature of functions that check whether an argument is valid
 *
 * Every Descriptor has such a function assigned in its Descriptor:
 * @code
 * std::vector<Descriptor> usage{ ..., {"PREDAREA", "", "p", "pred-area", ArgChecker::Rectangle, "..."}, ... };
 *                                                                        ^^^^^^^^^^^^^^^^^^^^^
 * @endcode
 *
 * A CheckArg function has the following signature:
 * @code
 * ArgStatus CheckArg(Option const& option);
 * @endcode
 * It is used to check if a potential argument would be acceptable for the option. It will even be
 * called if there is no argument. In that case `option::arg` will be empty.
 *
 * See @ref ArgStatus for the meaning of the return values.
 *
 * Often the pre-defined checks in ArgChecker suffice. You can also extend ArgChecker to provide
 * your own checking functions, see the example at ArgChecker.
 */
//using CheckArg = ArgStatus (*)(Option const& option);
using CheckArg = std::function<ArgStatus(Option const& option)>;

/**
 * @brief Describes an option, its help text and its argument checking function.
 *
 * The most common usage of this class is the definition of options. This can be done with a vector
 * of Descriptor%s. It defines all options including their expected arguments as well as their help
 * text. Consider the following example:
 * @code
 * std::vector<Descriptor> usage{
 *   // first element usually just introduces the help
 *   Descriptor::text("Usage: program [options]\n\n"
 *                    "Options:"),
 *
 *   // then the regular options follow
 *   // group specifier, property,  short, long option,      checkArg function,     help table entry
 *   {  "FILTER",        "DISABLE", "",    "disable-filter", ArgChecker::None,      "\t--disable-filter  \tDisable filtering of similar..."},
 *   {  "FILTER",        "ENABLE",  "",    "enable-filter",  ArgChecker::None,      "\t--enable-filter  \tEnable filtering of similar..."  },
 *   {  "PREDAREA",      "",        "pa",  "pred-area",      ArgChecker::Rectangle, "  -p <rect>, -a <rect>, \t--pred-area=<rect>  \tSpecify prediction area to..." },
 *
 *   // insert the predefined option-file Descriptor
 *   Descriptor::optfile() // (for alphabetic order put this before prediction area option)
 * };
 * @endcode
 * This vector is used to parse the command line arguments:
 * @code
 * auto options = OptionParser::parse(usage, argc, argv);
 * // or:
 * OptionParser options(usage);
 * options.parse(argc, argv);
 * @endcode
 * and you can for example ask the last filter option, whether it is disable or enable:
 * @code
 * bool doFilter = true; // default
 * if (!options["FILTER"].empty() && options["FILTER"].back().prop() == "DISABLE")
 *     doFilter = false;
 * @endcode
 * and the for `false` as default:
 * @code
 * bool doFilter = false; // default
 * if (!options["FILTER"].empty() && options["FILTER"].back().prop() == "ENABLE")
 *     doFilter = true;
 * @endcode
 *
 * Besides option parsing, the vector can also be used to print the help text in a pretty way with
 * printUsage(). The help text of the above vector can be printed with
 * @code
 * printUsage(usage);
 * @endcode
 * and would give the following output:
 *
 *     Usage: program [options]
 *
 *     Options:
 *                             --disable-filter     Disable filtering of similar...
 *                             --enable-filter      Enable filtering of similar...
 *       -p <rect>, -a <rect>, --pred-area=<rect>   Specify prediction area to...
 *                             --option-file=<file> Read options from a file. The options in this file are specified in the same way
 *                                                  as on the command line. You can use newlines between options and line comments
 *                                                  with # (use \# to get a non-comment #). The specified options in the file
 *                                                  replace the --option-file=<file> argument before they are parsed.
 * However, take this as one example for a help text. Other styles are also possible. Also for
 * complete independent option specification and help text specification, two vectors of
 * Descriptor%s can be defined.
 *
 * Note, there is a special pseudo-option, which is build-in (but can be deactivated with
 * OptionParser::expandOptionsFiles). It can be used as long option `--option-file=<file>`. This is
 * not specified in any Descriptor. The option name can be changed with
 * OptionParser::optFileOptName. The options in the given file are expanded before the specified
 * options are parsed. So an option file can hold all the options, which are specified with your
 * Descriptor vector. You should not try to parse an `--option-file` argument, but you can still
 * describe it in your Descriptor vector to inform the user about it. To make this easier, just use
 * Descriptor::optfile() as one element of your vector as shown above.
 *
 * There are also more special elements, such as text() to add a text without option and
 * breakTable() (or `"\f"`) to make a table break. Table breaks reset the column spacing. Any help
 * text can contain the escape sequences `\t`, `\v`, `\n` and `\f`. See the documentation of
 * printUsage() and breakTable().
 */
struct Descriptor
{
    /**
     * @brief %Option specifier, used to create option groups
     *
     * Command line options whose Descriptors have the same `spec` will end up in the same group in
     * the order in which they appear on the command line. If you have multiple long option aliases
     * that refer to the same option, give their descriptors the same `spec`.
     *
     * If you have multiple options that belong to the same group, but have different meanings (e.
     * g. `--enable-foo` and `--disable-foo`), you should give them the same `spec`, but
     * distinguish them by different values for @ref prop. Then they end up in the same option
     * group and you can just check the @ref prop value of the last element. This way you get the
     * usual behaviour where later switches on the command line override earlier ones without
     * having to code it manually.
     *
     * @see the example at @ref Descriptor!
     */
    std::string spec;

    /**
     * @brief Use to distinguish options with the same @ref spec.
     *
     * So, in contrast to @ref spec, options are not grouped according to their `prop` value. So
     * consider this as an additional property that can be used to distinguish different flavors of
     * options or so. If you do not need this, just use an empty string.
     *
     * If you have multiple options that belong to the same group, but have different meanings (e.
     * g. `--enable-foo` and `--disable-foo`), you should give them the same @ref spec, but
     * distinguish them by different values for `prop`. Then they end up in the same option group
     * and you can just check the `prop` value of the last element. This way you get the usual
     * behaviour where later switches on the command line override earlier ones without having to
     * code it manually.
     *
     * @see the example at @ref Descriptor!
     */
    std::string prop;

    /**
     * @brief Short option characters
     *
     * Provide a short option name, like `"p"` to be used with a single dash on command line, like
     * `-p`. You can give multiple characters, like `"pqr"`, which makes the parser accept all of
     * `-p`, `-q` and `-r` for this option. However do not include the dash character `-` or you'll
     * get undefined behaviour.
     *
     * If this Descriptor should not have short option characters, use the empty string `""`.
     *
     * @see the example at @ref Descriptor!
     */
    std::string shortopt;

    /**
     * @brief The long option name (without the leading `--`).
     *
     * Long option name, like `"prediction-area"` to be used with a double dash on command line,
     * like `--prediction-area`.
     *
     * If this Descriptor should not have a long option name, use the empty string `""`.
     *
     * While @ref shortopt allows multiple short option characters, each Descriptor can have only a
     * single long option name. If you want to have multiple long option names referring to the
     * same option use separate Descriptors that have the same @ref spec and @ref prop. You may
     * repeat short option characters in such an alias Descriptor but there's no need to. However,
     * you should repeat the @ref checkArg function for consistency.
     *
     * @see the example at @ref Descriptor!
     */
    std::string longopt;

    /**
     * @brief Function to check for a possible argument
     *
     * The function provided here decides whether the option accepts an argument and if so it
     * checks the argument for correctness.
     *
     * For each option that matches @ref shortopt or @ref longopt this function will be called to
     * check a potential argument for the option. For that purpose the function receives an
     * appropriate Option object to check. The Option object usually contains the option
     * Descriptor, the name that was given on command line and the option argument string. If there
     * is no potential argument @ref Option::arg will be empty. If the option given on command line
     * is not specified in any Descriptor @ref Option::desc will be @c nullptr.
     *
     * There are several default checking functions in ArgChecker:
     *  * ArgChecker::None will not look for an argument. So in case of an attached argument like
     *    `--foo=5` it will be ignored. In case of an detached argument like `-n 5` the `5` will be
     *    handled as non-option argument and collected in OptionParser::nonOptionArgs.
     *  * ArgChecker::Optional will collect an argument, but also not throw an error if there is
     *    none.
     *  * ArgChecker::NonEmpty is similar as ArgChecker::Optional, but will throw when there is no
     *    argument. No argument means, that the possible argument begins with a double dash `--` or
     *    the option is simply the last string in the command line.
     *  * There are a lot of checking function for build in types, like ArgChecker::Int,
     *    ArgChecker::Rectangle and many more.
     *
     * See @ref CheckArg and ArgChecker for more information.
     */
    CheckArg checkArg;

    /**
     * @brief The usage text associated with the options in this Descriptor.
     *
     * This usage help description serves as help for the user. This is supposed to be printed,
     * when using a `--help` option or if no arguments are provided or maybe even if some argument
     * is specified in a wrong format. Note, as special chars, you can use `\f` for breaking the
     * table, `\n` for going to the first column in the next line, `\t` for changing to the next
     * column (which is not the same as just printing a tab) and `\v` for changing to the next line
     * while staying in the same column! So your usage description can be nice table. Print it with
     * printUsage().
     *
     * Note, the help text is not really related in a fixed way with the rest of the descriptor. It
     * is just combined in one structure to help the utility developer / maintainer to keep the
     * usage documentation up-to-date. You can use dummy Descriptors (see text()) to add text to
     * the usage that is maybe not related to a specific option.
     *
     * See @ref printUsage(std::vector<Descriptor> const& usage,int,int,int) "printUsage()" for
     * special formatting characters you can use in @c help to get a column layout.
     *
     * @attention Must be UTF-8-encoded. Since C++11 you can use the "u8" prefix to make sure
     * string literals are properly encoded.
     */
    std::string help;

    /**
     * @brief Suggested descriptor for option-file option
     *
     * @param optFileOptName is the long option name for the pseudo options file option. It is put
     * into the text, so you can easily match the help text with the behavior in case you change
     * the name of the pseudo-option with OptionParser::optFileOptName.
     *
     * This Descriptor does not specify an option, since the `--option-file=<file>` is build in and
     * expanded before the real parsing starts. It just defines the the usage / help text for
     * documentation of your utility. Note, you can change the option name (see
     * OptionParser::optFileOptName) or deactivate the option `--option-file=<file>` completely
     * (see OptionParser::expandOptionsFiles).
     *
     * So this is just a text()-Descriptor with the following text. If you need a different text,
     * just use a text()-Descriptor with a different text.
     *
     *     "  \t" + optFileOptName + "=<file> \tRead options from a file. The options "
     *     "in this file are specified in the same way as on the command line. You can "
     *     "use newlines between options and line comments with # (use \\# to get a "
     *     "non-comment #). The specified options in the file replace the "
     *     + optFileOptName + "=<file> argument before they are parsed."
     *
     * @return Descriptor with help text that will be printed when printUsage is used.
     */
    static Descriptor optfile(std::string optFileOptName = "--option-file") {
        return text("  \t" + optFileOptName + "=<file> \tRead options from a file. The options "
                    "in this file are specified in the same way as on the command line. You can "
                    "use newlines between options and line comments with # (use \\# to get a "
                    "non-comment #). The specified options in the file replace the "
                    + optFileOptName + "=<file> argument before they are parsed.");
    }

    /**
     * @brief Element that does not specify any option, but only a usage text
     *
     * @param usageText is the text to display.
     *
     * If you use this Descriptor as first element in the Descriptor vector, this text will be the
     * first part of your usage / help text. So you can provide something similar to:
     *
     *     "Usage: yourUtility [options]\n\n"
     *     "Options:"
     *
     * The text can have table alignment, with `\t` and `\v`, but is not required to. This
     * alignment can also be broken with breakTable(). Newlines with `\n` are recognized as well.
     * See also printUsage().
     *
     * @return dummy Descriptor with usage text that will be printed when printUsage() is used.
     */
    static Descriptor text(std::string const& usageText = "") {
        return Descriptor{"", "", "", "", ArgChecker::None, usageText};
    }

    /**
     * @brief Element that breaks a table column layout
     *
     * So to have a newly aligned table, you need to break the current one with a break table
     * element. So a Descriptor vector with the elements
     * @code
     * Descriptor::text("long cell \t| another cell"),
     * Descriptor::text("even longer cell \t| cell"),
     * Descriptor::breakTable(),
     * Descriptor::text("short cell \t| another cell"),
     * Descriptor::text("cell \t| cell"),
     * @endcode
     * gives with printUsage():
     *
     *     long cell        | another cell
     *     even longer cell | cell
     *     short cell | another cell
     *     cell       | cell
     * Without breakTable()-element it would be:
     *
     *     long cell        | another cell
     *     even longer cell | cell
     *     short cell       | another cell
     *     cell             | cell
     *
     * Note, to add a Descriptor element breakTable() is only the preferred way to break the table
     * alignment. In general, when the printUsage() implementation finds a `\f` character, it will
     * skip the rest of the element and consider the help text of the next Descriptor element as
     * new table (with new alignment). So the above help text with breakTable()- element could be
     * defined alternatively with
     * @code
     * Descriptor::text("long cell \t| another cell"),
     * Descriptor::text("even longer cell \t| cell\f"), // Note the \f in the end
     * Descriptor::text("short cell \t| another cell"),
     * Descriptor::text("cell \t| cell"),
     * @endcode
     *
     * @return table break Descriptor element (a text()-Descriptor with a `"\f"` as text).
     *
     * @see printUsage()
     */
    static Descriptor breakTable() {
        return text("\f");
    }
};




/**
 * @brief Separates arguments by whitespace (and other custom chars), supports quoting and line comments
 *
 * This can be used to tokenize a stream into separate argument tokens. So assuming you have an
 * input stream (maybe a `stringstream` from a string) declared as `std::istream is`. To get an
 * argument token from it just use
 * @code
 * ArgumentToken tok;
 * is >> tok;
 * @endcode
 * Because of a conversion operator, `tok` can be used as string. However, there are also
 * to_string(ArgumentToken const& t) and operator<<(std::ostream& out, ArgumentToken const& t)
 * functions for convenience.
 *
 * Generally, arguments are separated by whitespaces and custom separator characters set in
 * ArgumentToken::sep, but if whitespace or the custom characters should be included in an
 * argument, quoting with single quotes '...', double quotes "..." or parenthesis pairs (...) can
 * be used. These can also be nested to support nested arguments. Each run with ArgumentToken will
 * only strip the outermost / first level of quoting. A good example, where this is utilized is
 * Parse::Image(). The argument for an Image can consist of multiple arguments, so they have to be
 * quoted. One of them is a Rectangle, which again accepts multiple arguments, so it also has to be
 * quoted. This looks like the following:
 *
 *     --image="-f 'test image.tif' --crop=(-x 1 -y 2 -w 3 -h 2)"
 * Hereby the first token is be the whole thing, but with the outer quoting stripped:
 * <tt>\--image=-f 'test image.tif' \--crop=(-x 1 -y 2 -w 3 -h 2)</tt>. It is not meant to be used
 * like this, but shows the behavior. Now suppose you have the argument of `--image` and separate
 * it into tokens:
 *
 *     -f 'test image.tif' --crop=(-x 1 -y 2 -w 3 -h 2)
 * Hereby, the tokens (written in angle brackets) are `<-f>`, `<test image.tif>` and `<--crop=-x 1
 * -y 2 -w 3 -h 2>`. Note the single quotes around `test image.tif` and the parens in the argument
 * of crop have been stripped, but preserved the whitespace. To get all these tokens in a vector of
 * strings, just use @ref separateArguments(std::string const& str, std::string const& sep).
 *
 * To support files every whitespace character, including newlines and tabs, is considered as
 * ordinary whitespace. Line comments can be made with `#`. It will skip the rest of the line, even
 * in a quotation to support line comments in nested arguments. To make a `#` character, use `\#`.
 * Hence, nice option files can be written, e. g.
 *
 *     # option file to fuse with a small window and [...]
 *     --window-size  11  # must be odd
 *     --slices       50
 * And `--option-file` can even be used in a recursive fashion, like
 *
 *     # meta option file
 *     --option-file  fast-settings.cfg
 *     --option-file  toy-input-images.cfg
 * Any whitespace is handled in the same way. So you can make newlines wherever a space would
 * separate arguments, also in nested options:
 *
 *     # input images (new and old)
 *     --image=(--file='day 0 fine old.tif' # has clouds
 *              -d 0 -t "fine old")
 *     --image=(--file='day 1 fine old.tif'
 *              -d 1 -t "fine old")
 *     --image=(--file='day 0 fine new.tif' # uses a fill value of -9999 where clouds have been
 *              -d 0 -t "fine new")
 *     --image=(--file='day 1 fine new.tif'
 *              -d 1 -t "fine new")
 *     # ... define other old and new images
 *
 *     # old or new?
 *     --use-tag="fine new"
 *     #--use-tag="fine old"
 * In this case the program could decide to only read in the images with the used tag to not occupy
 * memory for unused images.
 *
 * \note The quoting will not only used by this framework, but also by the bash. It uses single and
 * double quotes to preserve whitespace, but also tries to parse parens (not for quoting). So if
 * you want to preserve whitespace in a string or nested argument, you have to use single or double
 * quotes on bash *only for the outermost quoting* or just escape the whitespace. In option files
 * this is not a problem, since bash does not process the option files. Let's consider an example
 * with a file option. On *bash* the following forms of quoting and escaping would preserve the
 * whitespace:
 *
 *     --file='file 1.tif'
 *     --file="file 1.tif"
 *     --file=file\ 1.tif
 * Parens work in bash only for inner quotings, which is not shown here (see the crop argument in
 * the example above). In an *option file* you could use
 *
 *     --file='file 1.tif'
 *     --file="file 1.tif"
 *     --file=file\ 1.tif
 *     --file=(file 1.tif)
 * So there it works as expected. This is most handy with nested options, see Parse::Image() for
 * example for readability.
 *
 * Additionally to whitespace other characters can be added as separator, see @ref sep.
 *
 * @see separateArguments(std::string const& str, std::string const& sep)
 */
class ArgumentToken {
public:
    using string = std::string;
    /**
     * @brief Additional separator tokens
     *
     * Whitespace is always a separator. In `sep` additional tokens may be specified. Example:
     * @code
     * ArgumentToken arg;
     * arg.sep = ",";
     * @endcode
     * Then reading from one of these streams yields the same results
     * @code
     * std::istringstream iss1("1,2, 3 ,4 , 5");
     * std::istringstream iss2("1 2  3  4   5");
     *
     * while (iss1 >> tok || !tok.empty())
     *     std::cout << "<" << tok << "> ";
     * std::cout << std::endl;
     *
     * while (iss2 >> tok || !tok.empty())
     *     std::cout << "<" << tok << "> ";
     * std::cout << std::endl;
     * @endcode
     */
    std::string sep;

    /**
     * @brief Feed this ArgumentToken from an input stream
     * @param is is an input stream.
     * @param a is the argument token to replace by the next token.
     *
     * This method allows to read in the next token, like
     * @code
     * std::stringstream is("-x 1 -y 2 -w 3 -h 2");
     * ArgumentToken tok;
     * is >> tok;
     * // tok now contains "-x"
     * @endcode
     * If you write a parsing function for your own type, you could write:
     * @code
     * struct MyType {
     *     std::string someString;
     *     double someDouble;
     * };
     *
     * std::istream& operator>>(std::istream &is, MyType &e) {
     *     // assume the first token would be a string, which may contain spaces
     *     ArgumentToken tok;
     *     is >> tok;
     *     e.someString = tok;
     *
     *     // second token should be a double and could be parsed directly, but we use
     *     // ArgumentToken to remove quoting and Parse::Float() to throw a proper exception
     *     is >> tok;
     *     e.someDouble = Parse::Float(tok); // throws invalid_argument_error on parsing error
     *     return is;
     * }
     * @endcode
     * Note, this function throws an invalid_argument_error on parsing error. So, this function
     * would allow to use Parse::Arg<MyType>(), Parse::Vector<MyType>(), ArgChecker::Arg<MyType>
     * and ArgChecker::Vector<MyType> (if your `operator>>` and `MyType` are declared in the same
     * namespace, called ADL).
     *
     * @return `is` for chaining
     */
    friend std::istream& operator>>(std::istream& is, ArgumentToken& a);

    /**
     * @brief Access parsed token as std::string
     */
    operator string() const;

    /**
     * @brief Check whether it is empty
     * @return true, if the internal parsed string token is empty, false otherwise.
     */
    bool empty() const;
private:
    std::string data;
};

/**
 * @brief Convert an ArgumentToken to a string
 * @param t is the ArgumentToken to convert
 * @return simply `static_cast<std::string>(t)`, which uses @ref ArgumentToken::operator string()
 * const
 */
inline std::string to_string(ArgumentToken const& t) {
    return static_cast<std::string>(t);
}

/**
 * @brief Output an ArgumentToken to a stream
 * @param out is the output stream
 * @param t is the ArgumentToken to output
 *
 * Simply prints the string that to_string(ArgumentToken const& o) returns.
 *
 * @return `out` to append further strings.
 */
inline std::ostream& operator<<(std::ostream& out, ArgumentToken const& t) {
    return out << to_string(t);
}


/**
 * @brief Separate arguments into string tokens
 * @param strstr is the input string stream used as input
 *
 * @param sep are additional separator characters, that will be handled like whitespace.
 *
 * This tokenizes a string stream according to the rules of ArgumentToken. So quoting and
 * commenting is possible. Actually it just reads from `strstr` to an ArgumentToken in a loop and
 * saves each token in the vector it returns.
 *
 * @return tokenized string. Example: A `strstr` filled with `"-x 1 -y 2 -w 3 -h 2"` would return a
 * vector with the elements `"-x"`, `"1"`, `"-y"`, `"2"`, `"-w"`, `"3"`, `"-h"` and `"2"`.
 *
 * @see ArgumentToken
 */
inline std::vector<std::string> separateArguments(std::istream& strstr, std::string const& sep = "") {
    std::vector<std::string> args;
    ArgumentToken arg;
    arg.sep = sep;
    while (strstr >> arg || !arg.empty())
        args.push_back(arg);

    return args;
}

/**
 * @brief Separate arguments into string tokens
 * @param str is the string to separate
 * @param sep are additional separator characters, that will be handled like whitespace.
 *
 * This tokenizes a string according to the rules of ArgumentToken. So quoting and commenting is
 * possible.
 *
 * @return tokenized string. Example: A `str` filled with `"-x 1 -y 2 -w 3 -h 2"` would return a
 * vector with the elements `"-x"`, `"1"`, `"-y"`, `"2"`, `"-w"`, `"3"`, `"-h"` and `"2"`.
 *
 * @see ArgumentToken
 */
inline std::vector<std::string> separateArguments(std::string const& str, std::string const& sep = "") {
    std::stringstream strstr(str);
    return separateArguments(strstr, sep);
}

/**
 * @brief Separate arguments into string tokens
 * @param file is an options file to read the arguments from.
 * @param sep are additional separator characters, that will be handled like whitespace.
 *
 * This tokenizes the contents of a file stream according to the rules of ArgumentToken. So quoting
 * and commenting is possible.
 *
 * @return tokenized string. For example a file stream with the contents:
 *
 *     # option file to fuse with a small window and [...]
 *     --window-size  11  # must be odd
 *     --slices       50
 * would return a vector with the elements `"--window-size"`, `"11"`, `"--slices"`, `"50"`
 *
 * @see ArgumentToken
 */
inline std::vector<std::string> separateArguments(std::ifstream& file, std::string const& sep = "") {
    std::stringstream strstr;
    strstr << file.rdbuf();
    return separateArguments(strstr, sep);
}


// exclude implementation from doxygen documentation
/// \cond
template<typename T>
struct Parse::ArgImpl {
    static T Arg(std::string const& str, std::string const& optName) {
        std::istringstream ss(str);
        T t;
        ss >> t;
        if (!ss.eof() && ss.fail())
            IF_THROW_EXCEPTION(invalid_argument_error("Cannot parse " + str +
                                                      (optName.empty() ? std::string("") : " for option '" + optName + "'")));
        return t;
    }
};

template <>
struct Parse::ArgImpl<double> {
    static double Arg(std::string const& str, std::string const& optName) {
        return Parse::Float(str, optName);
    }
};

template <>
struct Parse::ArgImpl<int> {
    static int Arg(std::string const& str, std::string const& optName) {
        return Parse::Int(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Type> {
    static imagefusion::Type Arg(std::string const& str, std::string const& optName) {
        return Parse::Type(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Interval> {
    static imagefusion::Interval Arg(std::string const& str, std::string const& optName) {
        return Parse::Interval(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::IntervalSet> {
    static imagefusion::IntervalSet Arg(std::string const& str, std::string const& optName) {
        return Parse::IntervalSet(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Size> {
    static imagefusion::Size Arg(std::string const& str, std::string const& optName) {
        return Parse::Size(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Dimensions> {
    static imagefusion::Dimensions Arg(std::string const& str, std::string const& optName) {
        return Parse::Dimensions(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Point> {
    static imagefusion::Point Arg(std::string const& str, std::string const& optName) {
        return Parse::Point(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Coordinate> {
    static imagefusion::Coordinate Arg(std::string const& str, std::string const& optName) {
        return Parse::Coordinate(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Rectangle> {
    static imagefusion::Rectangle Arg(std::string const& str, std::string const& optName) {
        return Parse::Rectangle(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::CoordRectangle> {
    static imagefusion::CoordRectangle Arg(std::string const& str, std::string const& optName) {
        return Parse::CoordRectangle(str, optName);
    }
};

template <>
struct Parse::ArgImpl<imagefusion::Image> {
    static imagefusion::Image Arg(std::string const& str, std::string const& optName) {
        return Parse::Image(str, optName);
    }
};

template <>
struct Parse::ArgImpl<ImageInput> {
    static ImageInput Arg(std::string const& str, std::string const& optName) {
        return Parse::MRImage(str, optName);
    }
};

template <typename T>
struct Parse::ArgImpl<std::vector<T>> {
    static std::vector<T> Arg(std::string const& str, std::string const& optName) {
        return Parse::Vector<T>(str, optName);
    }
};
/// \endcond


template<typename T>
inline std::vector<T> Parse::Vector(std::string const& str, std::string const& optName) {
    std::vector<std::string> str_vec = separateArguments(str, ",");
    std::vector<T> r;
    for (std::string const& s : str_vec) {
        T t = Arg<T>(s, optName);
        r.push_back(std::move(t));
    }
    return r;
}


/**
 * @brief Parses options, checks their arguments and provides structured access to them
 *
 * This is the most important class; it is the OptionParser. On the one hand, it parses the options
 * and on the other hand it holds the structures to access them in a convenient way. There is an
 * object @ref input, which holds the parsed options in the order they came in and an object @ref
 * groups, which holds the options grouped and each group is ordered like the options of this kind
 * came in. For arguments which are not options and also are not arguments for any option, there is
 * a vector @ref nonOptionArgs. When also unknown options are collected (see @ref
 * unknownOptionArgCheck), these are stored in the vector @ref unknown. Assuming a program call
 *
 *     utility -n 1 -ab --size=5x4 -n 10 -b "maybe a file" more stuff
 * the options are ordered like illustrated here:
 * @image html parsed_options.png
 *
 * A short non-complete example:
 * @code
 * int main(int argc, char* argv[])
 * {
 *     auto options = OptionParser::parse(usage, argc, argv);
 *
 *     if (!options["HELP"].empty()) {
 *         printUsage(usage);
 *         return 0;
 *     }
 *
 *     if (!options["NUM"].empty()) {
 *         std::string const& lastNumArg = options["NUM"].back().arg;
 *         int num = Parse::Int(lastNumArg); // 10 for the example input above
 *         ...
 *     }
 *     ...
 * }
 * @endcode
 * For a complete example, see @ref optionparser.
 */
class OptionParser {
public:
    using option_type = Option;

    /**
     * @brief Usage Descriptor vector that defines the options
     *
     * This defines the options the parser accepts. Usually this is set with the first argument in
     * the constructor of OptionParser. See the example at @ref optionparser and the one at @ref
     * Descriptor.
     */
    std::vector<Descriptor> usage;

    /**
     * @brief Non-option arguments
     *
     * So, when parsing something like `"--num=1 output.tif --no-option"` `nonOptionArgs` would
     * contain `"output.tif"` and `"--no-option"`, since the first non-option stops the parsing (at
     * least by default). Also a double dash without option `--` makes all following tokens be
     * considered as non-options. Note, since the expandOptFiles() is quite simple, the option
     * files will be expanded also in the non-options section. The argument tokens from the file
     * would then be recognized as non-options (by default).
     */
    std::vector<std::string> nonOptionArgs;

    /**
     * @brief Parsed options in the order as given on command line input
     *
     * To access e. g. the first option, use either
     * @code
     * options.input[0]
     * // or shorter:
     * options[0]
     * @endcode
     * where `options` is a OptionParser object. If you like to process all
     * options in order, you can use:
     * @code
     * for(auto& o : options.input) {
     *     if (o.spec() == "NUM")
     *         ...
     * }
     * @endcode
     * However, note that this does not include non-option arguments nor unknown options.
     */
    std::vector<option_type> input;

    /**
     * @brief Parsed options grouped by the option specifier spec.
     *
     * To access e. g. all options `"NUM"`, use
     * @code
     * options.groups["NUM"]
     * // or shorter:
     * options["NUM"]
     * @endcode
     * where `options` is an OptionParser object. Often you are only interested in the last option
     * of one type. Then use something like
     * @code
     * if (!options["NUM"].empty()) {
     *     std::string const& lastNumArg = options["NUM"].back().arg;
     *     int num = Parse::Int(lastNumArg);
     *     ...
     * }
     * @endcode
     * If you want to go through all the options of one kind, you can use
     * @code
     * int sum = 0;
     * for (auto& o : options["NUM"])
     *     sum += Parse::Int(o.arg);
     * @endcode
     *
     * In some cases, you may be interested in the number how often an option has been given on
     * command line, like
     * @code
     * int verbosity = options["VERBOSE"].size();
     * @endcode
     */
    std::map<std::string, std::vector<option_type>> groups;

    /**
     * @brief Collection of the unknown options
     *
     * If unknownOptionArgCheck is set to a function that accepts unknown options (by default it
     * throws an error), then these are collected here.
     *
     * An unknown option is a string in the argument vector that starts with a dash character and
     * does not match any Descriptor's @ref Descriptor::shortopt "shortopt" or @ref
     * Descriptor::longopt "longopt". However, a single dash (without further characters) is always
     * a non-option argument and double dash terminates the list of options, but does not appear in
     * any collection.
     *
     * Example:
     * @code
     * std::vector<Descriptor> usage{
     *     {"FILE", "", "f", "file",  ArgChecker::File, "  ..." }
     * };
     *
     * int main(int argc, char* argv[]) {
     *     OptionParser options(usage);
     *     options.unknownOptionArgCheck = ArgChecker::None;  // accept unknown options, but without argument
     *     options.parse(argc, argv);
     *     for (auto& o : options.unknown)
     *         std::cout << "Unknown option: " << o << std::endl;
     *     return 0;
     * }
     * @endcode
     * This would allow to specify unknown options and for a command line input
     *
     *     utility  -u  --foo  --file=test.txt
     * this outputs
     *
     *     Unknown option: u
     *     Unknown option: foo
     * @see unknownOptionArgCheck
     */
    std::vector<option_type> unknown;

    /**
     * @brief Accepts options after non-options?
     *
     * If set to true, option parsing will not stop at the first non-option argument. Instead it
     * will go on and look for options after non-options. However, a double dash `--` will still
     * stop parsing. Example:
     *
     *     "--number 1 path/to/file --number 2 -- --strange-file"
     * Hereby, the two numbers would be interpreted as options if `acceptsOptAfterNonOpts` is true,
     * but `"path/to/file"` and `"--strange-file"` will be non-option arguments (see @ref
     * nonOptionArgs).
     */
    bool acceptsOptAfterNonOpts = false;

    /**
     * @brief Accept single dash long options?
     *
     * When set to true, long options may begin with a single dash. The double dash form will still
     * be recognized. Note that single dash long options with more than a single letter or attached
     * argument take precedence over short options and short option groups. E. g. `-file` would be
     * interpreted as `--file` and not as `-f -i -l -e` (assuming a long option named `"file"`
     * exists). Example:
     *
     *     -foo=10  -foo 10  -x=5
     * would then be accepted if there are long options `foo` and `x`. If it is false, it is just
     * not allowed and would be interpreted as short options (and in the example throw an error).
     * Also not that a single dash long option's detached argument must not start with a dash:
     *
     *     -foo=-10    # ok
     *     -foo -10    # not ok
     * There is one exception: If `singleDashLongopt` is `true` and a single char option with a
     * detached argument that starts with a dash could be recognized as long option name, but it
     * can also be recognized as short option, it will be. Example:
     *
     *     -x=-10    # ok, if x is a long option
     *     -x -10    # ok, if x is a short option, even when it is a long option, too
     */
    bool singleDashLongopt = false;

    /**
     * @brief Argument checking function for unknown options
     *
     * If you leave it set to the default ArgChecker::Unknown, an invalid_argument_error with a
     * good error meassage will be thrown, when an unknown option is parsed. If you like to collect
     * unknown options you can set it to ArgChecker::None.
     *
     * Note that the @ref unknownOptionArgCheck function can also be set to a checking function
     * that accepts arguments like ArgChecker::Optional or ArgChecker::NonEmpty. However, this
     * might have unwanted effects, since the argument (or short option!) after the unknown option
     * gets eaten then. Usually, when the parsing should just not throw an error on unknown
     * options, ArgChecker::None should be used.
     *
     * @see unknown for an example.
     */
    CheckArg unknownOptionArgCheck = ArgChecker::Unknown;

    /**
     * @brief Option name for the options file
     *
     * Since the option file option is actually not parsed as option, its name cannot be specified
     * in the usage Descriptor vector. This setting provides a way to change the option name for
     * it.
     *
     * @note The double dashes `--` are required here in contrast to the long option names in the
     * Descriptor%s.
     */
    std::string optFileOptName = "--option-file";

    /**
     * @brief Expand options files?
     *
     * When set to true options file pseudo-options are expanded recursively before parsing. The
     * name for this pseudo-option is by default `--option-file`, but can be changed with @ref
     * optFileOptName.
     *
     * If it is false, it will not be expanded. You could then implement a different behavior for
     * options files or just not support options files.
     */
    bool expandOptionsFiles = true;

    /**
     * @brief Length of abbreviations to be accepted as long option
     *
     * Using a value `minAbbrevLen > 0` enables abbreviated long options. The parser will match a
     * prefix of a long option as if it was the full long option (e. g. `--foob=10` will be
     * interpreted as if it was `--foobar=10`), as long as the prefix has at least `minAbbrevLen`
     * characters (not counting the `--`) and is unambiguous. Be careful if combining `minAbbrevLen
     * = 1` with `singleDashLongopt = true` because the ambiguity check does not consider short
     * options and abbreviated single dash long options will take precedence over short options.
     *
     * When left to 0, abbreviations are not allowed.
     */
    unsigned int minAbbrevLen = 0;

    /**
     * @brief Default constructor. Does nothing
     *
     * Before parsing can be done, a usage Descriptor vector has to be set.
     */
    OptionParser()
        : OptionParser(std::vector<Descriptor>{})
    { }

    /**
     * @brief Construct OptionsParser with usage Descriptor vector
     *
     * @param opts is the usage Descriptor vector that specifies the options.
     * See @ref OptionParser::usage "usage"!
     *
     * @param acceptsOptAfterNonOpts specifies whether options should be accepted after non-options.
     * See @ref acceptsOptAfterNonOpts!
     *
     * @param singleDashLongopt specifies whether long options can be used with a single dash.
     * See @ref singleDashLongopt!
     *
     * @param unknownOptionArgCheck is the checking function for %unknown arguments.
     * See @ref unknownOptionArgCheck!
     *
     * @param optFileOptName specifies the name of the pseudo-option for options files.
     * See @ref optFileOptName!
     *
     * @param expandOptionsFiles specifies whether option files are expanded at all.
     * See @ref expandOptionsFiles!
     *
     * @param minAbbrevLen specifies whether long options may be abbreviated and if so with which minimum length.
     * See @ref minAbbrevLen!
     *
     * When constructing an OptionParser with this constructor, the object is
     * ready to parse().
     */
    OptionParser(std::vector<Descriptor> opts,
                 bool acceptsOptAfterNonOpts = false,
                 bool singleDashLongopt = false,
                 CheckArg unknownOptionArgCheck = ArgChecker::Unknown,
                 std::string optFileOptName = "--option-file",
                 bool expandOptionsFiles = true,
                 unsigned int minAbbrevLen = 0)
        : usage(std::move(opts)),
          acceptsOptAfterNonOpts(acceptsOptAfterNonOpts),
          singleDashLongopt(singleDashLongopt),
          unknownOptionArgCheck(unknownOptionArgCheck),
          optFileOptName(optFileOptName),
          expandOptionsFiles(expandOptionsFiles),
          minAbbrevLen(minAbbrevLen)
    { }

    /**
     * @brief Clear the parsed options. All settings are preserved.
     *
     * @return `*this` for chaining.
     */
    OptionParser& clear() {
        input.clear();
        nonOptionArgs.clear();
        groups.clear();
        unknown.clear();
        return *this;
    }

    /**
     * @brief Parse arguments directly from command line
     *
     * @param argc is the number of elements from `argv` that are to be parsed. If you pass -1, the
     * number will be determined automatically. In that case the `argv` list must end with a
     * `nullptr` and you have to remove the program name from the list (or just give `argv + 1`
     * in).
     *
     * @param argv are the arguments to be parsed. If you pass -1 as `argc` the last pointer in the
     * `argv` list must be a `nullptr` to mark the end and you have to remove the program name from
     * the list (or just give `argv + 1` in).
     *
     * @param dropFirstArg If true and `argc > 0`, the first argument is dropped for parsing. This
     * is the default behaviour, since the first argument is usually the program name. If false,
     * the first argument is used as ordinary command line argument.
     *
     * To add default arguments, you can just parse them before parsing the real arguments, like:
     * @code
     * OptionParser options(usage);
     * options.parse("--enable-foo  --number=5").parse(argc, argv);
     * @endcode
     * Then they go right before the arguments given on command line. So if you only take the last
     * option argument, the default can be overridden by the options specified on commandline.
     * Note, you can also specify `options.optFileOptName + "=" + defOptFile` here, to have a
     * default options file.
     *
     * @return `*this` for chaining.
     *
     * @throw runtime_error if @ref OptionParser::usage "usage" is empty.
     *
     * @throw invalid_argument_error if any @ref Descriptor::checkArg "argument checking function"
     * in the Descritor vector @ref OptionParser::usage "usage" throws this exception. However, in
     * principle any exception can be thrown by an argument checking function, since it can be
     * defined by the user. The provided argument checking functions in @ref ArgChecker will throw
     * only invalid_argument_error%s.
     */
    OptionParser& parse(int argc,
                        char* argv[],
                        bool dropFirstArg = true)
    {
        // skip first argument, usually program name
        if (dropFirstArg && argc > 0) {
            --argc;
            ++argv;
        }

        std::vector<std::string> args_loc;
        for (int i = 0; i < argc; ++i)
            args_loc.push_back(argv[i]);

        return parse(std::move(args_loc));
    }

    /**
     * @brief Parse arguments from argument tokens
     *
     * @param args is a vector of argument tokens. You can get such a vector from a string with
     * separateArguments() and from command line argument c-array with:
     * @code
     * std::vector<std::string> args_loc;
     * for (int i = 0; i < argc; ++i)
     *     args_loc.push_back(argv[i]);
     * @endcode
     * However, rather use the appropriate overloads of parse() for these purposes.
     *
     * To add default arguments, you can just parse them before parsing the real arguments, like:
     * @code
     * OptionParser options(usage);
     * options.parse("--enable-foo  --number=5").parse(args);
     * @endcode
     * Then they go right before the arguments given on command line. So if you only take the last
     * option argument, the default can be overridden by the options specified on commandline.
     * Note, you can also specify `options.optFileOptName + "=" + defOptFile` here, to have a
     * default options file.
     *
     * @return `*this` for chaining.
     *
     * @throw runtime_error if @ref OptionParser::usage "usage" is empty.
     *
     * @throw invalid_argument_error if any @ref Descriptor::checkArg "argument checking function"
     * in the Descritor vector @ref OptionParser::usage "usage" throws this exception. However, in
     * principle any exception can be thrown by an argument checking function, since it can be
     * defined by the user. The provided argument checking functions in @ref ArgChecker will throw
     * only invalid_argument_error%s.
     */
    OptionParser& parse(std::vector<std::string> args);

    /**
     * @brief Parse arguments directly from a string
     *
     * @param args is a string with options to parse, like `"--num=1 --option-file=config.cfg"`.
     * Note qupting with parens works here also for the outermost quote, since this is not
     * processed by bash.
     *
     * To add default arguments, you can just parse them before parsing the real arguments, like:
     * @code
     * OptionParser options(usage);
     * options.parse("--enable-foo  --number=5").parse(args);
     * @endcode
     * Then they go right before the arguments given on command line. So if you only take the last
     * option argument, the default can be overridden by the options specified on commandline.
     * Note, you can also specify `options.optFileOptName + "=" + defOptFile` here, to have a
     * default options file.
     *
     * @return `*this` for chaining.
     *
     * @throw runtime_error if @ref OptionParser::usage "usage" is empty.
     *
     * @throw invalid_argument_error if any @ref Descriptor::checkArg "argument checking function"
     * in the Descritor vector @ref OptionParser::usage "usage" throws this exception. However, in
     * principle any exception can be thrown by an argument checking function, since it can be
     * defined by the user. The provided argument checking functions in @ref ArgChecker will throw
     * only invalid_argument_error%s.
     */
    OptionParser& parse(std::string args)
    {
        std::vector<std::string> args_vec = separateArguments(args);
        return parse(std::move(args_vec));
    }

    /**
     * @brief Simple parse method with default settings
     *
     * @param opts is the usage Descriptor vector that specifies the options. See @ref
     * OptionParser::usage "usage"!
     *
     * @param args is a vector of argument tokens. You can get such a vector with
     * separateArguments().
     *
     * @param def are the default options, which will be parsed right before the other arguments.
     *
     * @return the processed OptionParser object with the parsed options.
     *
     * @throw runtime_error if @ref OptionParser::usage "usage" is empty.
     *
     * @throw invalid_argument_error if any @ref Descriptor::checkArg "argument checking function"
     * in the Descritor vector @ref OptionParser::usage "usage" throws this exception. However, in
     * principle any exception can be thrown by an argument checking function, since it can be
     * defined by the user. The provided argument checking functions in @ref ArgChecker will throw
     * only invalid_argument_error%s.
     */
    static OptionParser parse(std::vector<Descriptor> opts,
                              std::vector<std::string> args,
                              std::string const& def = "")
    {
        OptionParser op(std::move(opts));
        op.parse(def).parse(args);
        return op;
    }

    /**
     * @brief Simple parse method with default settings
     *
     * @param opts is the usage Descriptor vector that specifies the options. See @ref
     * OptionParser::usage "usage"!
     *
     * @param args is a string with options to parse, like `"--numbers=(1, 2, 5)
     * --option-file=config.cfg"`. Note quoting with parens works here also for the outermost
     * quote, since this is not processed by bash.
     *
     * @param def are the default options, which will be parsed right before the other arguments.
     *
     * @return the processed OptionParser object with the parsed options.
     *
     * @throw runtime_error if @ref OptionParser::usage "usage" is empty.
     *
     * @throw invalid_argument_error if any @ref Descriptor::checkArg "argument checking function"
     * in the Descritor vector @ref OptionParser::usage "usage" throws this exception. However, in
     * principle any exception can be thrown by an argument checking function, since it can be
     * defined by the user. The provided argument checking functions in @ref ArgChecker will throw
     * only invalid_argument_error%s.
     */
    static OptionParser parse(std::vector<Descriptor> opts,
                              std::string args,
                              std::string const& def = "")
    {
        OptionParser op(std::move(opts));
        op.parse(def).parse(args);
        return op;
    }

    /**
     * @brief Simple parse method with default settings
     *
     * @param opts is the usage Descriptor vector that specifies the options. See @ref
     * OptionParser::usage "usage"!
     *
     * @param argc is the number of elements from `argv` that are to be parsed. If you pass -1, the
     * number will be determined automatically. In that case the `argv` list must end with a
     * `nullptr` and you have to remove the program name from the list (or just give `argv + 1`
     * in).
     *
     * @param argv are the arguments to be parsed. If you pass -1 as `argc` the last pointer in the
     * `argv` list must be `nullptr` to mark the end and you have to remove the program name from
     * the list (or just give `argv + 1` in).
     *
     * @param def are the default options, which will be parsed right before the other arguments.
     *
     * @param dropFirstArg If true and `argc > 0`, the first argument is dropped for parsing. This
     * is the default behaviour, since the first argument is usually the program name. If false,
     * the first argument is used as ordinary command line argument.
     *
     * @return the processed OptionParser object with the parsed options.
     *
     * @throw runtime_error if @ref OptionParser::usage "usage" is empty.
     *
     * @throw invalid_argument_error if any @ref Descriptor::checkArg "argument checking function"
     * in the Descritor vector @ref OptionParser::usage "usage" throws this exception. However, in
     * principle any exception can be thrown by an argument checking function, since it can be
     * defined by the user. The provided argument checking functions in @ref ArgChecker will throw
     * only invalid_argument_error%s.
     */
    static OptionParser parse(std::vector<Descriptor> opts,
                              int argc,
                              char* argv[],
                              std::string const& def = "",
                              bool dropFirstArg = true)
    {
        OptionParser op(std::move(opts));
        op.parse(def).parse(argc, argv, dropFirstArg);
        return op;
    }

    /**
     * @brief Get option group
     *
     * @param opt is the option specifier, i. e. the first argument in the corresponding
     * Descriptor.
     *
     * This is a shorthand for `groups.at(opt)` for convenience. Note, for every option specified
     * in usage, also a group (vector) exists. So `at()` will not throw. The groups of unused
     * options just stay empty. So you can check for their presence with
     * @code
     * if (!options["NUM"].empty())
     *     ...
     * @endcode
     *
     * @return option group vector.
     *
     * @see groups
     */
    std::vector<option_type>& operator[](std::string opt) {
        try {
            return groups.at(opt);
        }
        catch(std::out_of_range&) {
            IF_THROW_EXCEPTION(logic_error("You tried to access an option group, which you haven't specified: group " + opt));
        }
    }

    /// \copydoc operator[](std::string opt)
    std::vector<option_type> const& operator[](std::string opt) const {
        try {
            return groups.at(opt);
        }
        catch(std::out_of_range&) {
            IF_THROW_EXCEPTION(logic_error("You tried to access an option group, which you haven't specified: group " + opt));
        }
    }

    /**
     * @brief Get the option with the specified index
     *
     * @param idx is the position of the option to get, i. e. 0 gives the first option that has
     * been specified on command line.
     *
     * This is a shorthand for `input.at(idx)` for convenience. To loop through all recognized
     * options, you could use:
     * @code
     * for(auto& o : options.input) {
     *     if (o.spec() == "NUM")
     *         ...
     * }
     * @endcode
     *
     * @return option at position idx.
     *
     * @see input, optionCount()
     */
    option_type& operator[](unsigned int idx) {
        return input.at(idx);
    }

    /// \copydoc operator[](unsigned int idx)
    option_type const& operator[](unsigned int idx) const {
        return input.at(idx);
    }

    /**
     * @brief Number of arguments after the options
     *
     * This is a shorthand for `nonOptionArgs.size()`.
     * @return number of non-option arguments
     */
    std::size_t nonOptionArgCount() const {
        return nonOptionArgs.size();
    }

    /**
     * @brief Number of options
     *
     * This is a shorthand for `input.size()` for convenience.
     *
     * @return the number of options that have been specified on command line
     */
    std::size_t optionCount() const {
        return input.size();
    }

protected:
    /**
     * @internal
     * @brief Store an option in @ref input and @ref groups while parsing
     * @param o is the option to store
     *
     * parseBackend(), which does the parsing, uses this function to store a new found option into
     * @ref input and @ref groups or if `o.desc` is `nullptr` in @ref unknown.
     */
    void store(Option const& o) {
        if (o.desc != nullptr) {
            input.push_back(o);
            groups[o.spec()].push_back(o);
        }
        else
            unknown.push_back(o);
    }


    /**
     * @internal
     * @brief Parses the given argument vector.
     *
     * @param gnu if true, parseBackend() will not stop at the first non-option argument. This is
     * the default behaviour of GNU getopt() but is not conforming to POSIX.
     *
     * @param usage vector of Descriptor objects that describe the options to support.
     *
     * @param args is a vector of argument tokens. You can get such a vector with
     * separateArguments().
     *
     * @param min_abbr_len Passing a value <code> min_abbr_len > 0 </code> enables abbreviated long
     * options. The parser will match a prefix of a long option as if it was the full long option
     * (e.g. @c --foob=10 will be interpreted as if it was @c --foobar=10 ), as long as the prefix
     * has at least @c min_abbr_len characters (not counting the @c -- ) and is unambiguous. @n Be
     * careful if combining @c min_abbr_len=1 with @c single_minus_longopt=true because the
     * ambiguity check does not consider short options and abbreviated single minus long options
     * will take precedence over short options.
     *
     * @param single_minus_longopt Passing @c true for this option allows long options to begin
     * with a single minus. The double minus form will still be recognized. Note that single minus
     * long options take precedence over short options and short option groups. E.g. @c -file would
     * be interpreted as @c --file and not as <code> -f -i -l -e </code> (assuming a long option
     * named @c "file" exists).
     */
    void parseBackend(bool gnu, std::vector<Descriptor> const& usage, std::vector<std::string> args,
               int min_abbr_len = 0, bool single_minus_longopt = false);
};


/**
 * @brief Expand the option files recursively
 * @param options are argument tokens, potentially containing `optName`.
 * @param optName is the name for the options file pseude-option.
 *
 * This searches in the argument list for optName in the beginning of an element and replaces it by
 * the argument tokens found in the file (using separateArguments() for tokenization). The file can
 * be specified after an equal sign (`optName=<file>)` or whitespace (`optName <file>`). This is
 * used before actually parsing the arguments. Therefore also `optName=<file>` options in the
 * non-option section will be expanded.
 *
 * @return true if an option file has been expanded once (another run could expand a nested
 * `optName` option), and false if no `optName` options have been found.
 *
 * @note This function is rather internal. You probably don't need to use it.
 */
bool expandOptFiles(std::vector<std::string>& options, std::string const& optName = "--option-file");

inline OptionParser& OptionParser::parse(std::vector<std::string> args) {
    // make for every possible group specified in usage an empty vector by touching it
    for (auto const& desc : usage)
        groups[desc.spec];

    if (args.empty())
        return *this;

    if (usage.empty())
        IF_THROW_EXCEPTION(runtime_error("Cannot parse arguments without usage. Use constructor with usage before parse()"));

    if (expandOptionsFiles)
        // looping allows recursion of option files
        while (expandOptFiles(args, optFileOptName))
            ;

    // use parser from 'The Lean Mean C++ Option Parser' (modified)
    parseBackend(acceptsOptAfterNonOpts, usage, args, minAbbrevLen, singleDashLongopt);
    return *this;
}


void printUsage(std::vector<Descriptor> const& usage, int width = -1, int last_column_min_percent = 50, int last_column_own_line_max_percent = 75);


/**
 * @internal
 * @brief Interface for Functors that write (part of) a string somewhere.
 */
struct IStringWriter
{
    /**
     * @brief Writes the given number of chars beginning at the given pointer somewhere.
     */
    virtual void operator()(const char*, int) = 0;
};

/**
 * @internal
 * @brief Encapsulates a function with signature <code>func(string, size)</code> where string can
 * be initialized with a const char* and size with an int.
 */
template<typename Function>
struct FunctionWriter : public IStringWriter
{
    Function* write;

    virtual void operator()(const char* str, int size) override
    {
        (*write)(str, size);
    }

    FunctionWriter(Function* w) :
        write(w)
    {
    }
};

/**
 * @internal
 * @brief Encapsulates a reference to an object with a <code>write(string, size)</code> method like
 * that of @c std::ostream.
 */
template<typename OStream>
struct OStreamWriter : public IStringWriter
{
    OStream& ostream;

    virtual void operator()(const char* str, int size) override
    {
        ostream.write(str, size);
    }

    OStreamWriter(OStream& o) :
        ostream(o)
    {
    }
};

/**
 * @internal
 * @brief Like OStreamWriter but encapsulates a @c const reference, which is typically a temporary
 * object of a user class.
 */
template<typename Temporary>
struct TemporaryWriter : public IStringWriter
{
    const Temporary& userstream;

    virtual void operator()(const char* str, int size) override
    {
        userstream.write(str, size);
    }

    TemporaryWriter(const Temporary& u) :
        userstream(u)
    {
    }
};

/**
 * @internal
 * @brief Encapsulates a function with the signature <code>func(fd, string, size)</code> (the
 * signature of the @c write() system call), where fd can be initialized from an int, string from a
 * const char* and size from an int.
 */
template<typename Syscall>
struct SyscallWriter : public IStringWriter
{
    Syscall* write;
    int fd;

    virtual void operator()(const char* str, int size) override
    {
        (*write)(fd, str, size);
    }

    SyscallWriter(Syscall* w, int f) :
        write(w), fd(f)
    {
    }
};

/**
 * @internal
 * @brief Encapsulates a function with the same signature as @c std::fwrite().
 */
template<typename Function, typename Stream>
struct StreamWriter : public IStringWriter
{
    Function* fwrite;
    Stream* stream;

    virtual void operator()(const char* str, int size) override
    {
        (*fwrite)(str, size, 1, stream);
    }

    StreamWriter(Function* w, Stream* s) :
        fwrite(w), stream(s)
    {
    }
};

/**
 * @internal
 * @brief This is the implementation that is shared between all printUsage() functions.
 */
void printUsageBackend(IStringWriter& write, std::vector<Descriptor> const& usage, int width = 80,
                       int last_column_min_percent = 50, int last_column_own_line_max_percent = 75);


/**
 * @brief Print a usage Descriptor vector, see @ref printUsage()
 * @tparam OStream is the output stream type. This type can be deduced by `prn`.
 * @param prn can be an output stream like std::out or a std::stringstream or a std::ofstream.
 * @param usage, width, last_column_min_percent, last_column_own_line_max_percent - see @ref
 * printUsage()
 *
 * Example:
 * @code
 * printUsage(std::cerr, usage)
 *
 * std::ostringstream sstr;
 * printUsage(sstr, usage);
 * @endcode
 */
template<typename OStream>
void printUsage(OStream& prn, std::vector<Descriptor> const& usage, int width = -1,
                int last_column_min_percent = 50, int last_column_own_line_max_percent = 75)
{
    OStreamWriter<OStream> write(prn);
    printUsageBackend(write, usage, width, last_column_min_percent, last_column_own_line_max_percent);
}

/**
 * @brief Print a usage Descriptor vector, see @ref printUsage()
 * @tparam Function is the function pointer type. This type can be deduced by `prn`.
 * @param prn can be any function or functor with signature `prn(const char* str, int size)`.
 * @param usage, width, last_column_min_percent, last_column_own_line_max_percent - see @ref
 * printUsage()
 *
 * Example with function:
 * @code
 * void my_write(const char* str, int size) {
 *     fwrite(str, size, 1, stdout);
 * }
 *
 * printUsage(my_write, usage);
 * @endcode
 *
 * Example with functor:
 * @code
 * struct MyWriteFunctor {
 *     void operator()(const char* buf, size_t size) {
 *         fwrite(str, size, 1, stdout);
 *     }
 * };
 *
 * MyWriteFunctor wfunctor;
 * printUsage(&wfunctor, usage);
 * @endcode
 */
template<typename Function>
void printUsage(Function* prn, std::vector<Descriptor> const& usage, int width = -1,
                int last_column_min_percent = 50, int last_column_own_line_max_percent = 75)
{
    FunctionWriter<Function> write(prn);
    printUsageBackend(write, usage, width, last_column_min_percent, last_column_own_line_max_percent);
}

/**
 * @brief Print a usage Descriptor vector, see @ref printUsage()
 * @tparam Temporary is the type of `prn`. This type can be deduced by `prn`.
 * @param prn can be a temporary with a method `write` with the signature
 * `Temporary::write(const char* buf, size_t size) const`
 * @param usage, width, last_column_min_percent, last_column_own_line_max_percent - see @ref
 * printUsage()
 *
 * Example:
 * @code
 * struct MyWriter {
 *     void write(const char* buf, size_t size) const {
 *         fwrite(str, size, 1, stdout);
 *     }
 * };
 *
 * printUsage(MyWriter(), usage);
 * @endcode
 */
template<typename Temporary>
void printUsage(const Temporary& prn, std::vector<Descriptor> const& usage, int width = -1,
                int last_column_min_percent = 50, int last_column_own_line_max_percent = 75)
{
    TemporaryWriter<Temporary> write(prn);
    printUsageBackend(write, usage, width, last_column_min_percent, last_column_own_line_max_percent);
}

/**
 * @brief Print a usage Descriptor vector, see @ref printUsage()
 * @tparam Syscall is the function pointer type. This type can be deduced by `prn`.
 * @param prn can be system call function, like write, with the signature
 * `prn(int fd, const char* str, int size)`.
 * @param fd is the file descriptor to write to.
 * @param usage, width, last_column_min_percent, last_column_own_line_max_percent - see @ref
 * printUsage()
 *
 * Examples:
 * @code
 * printUsage(write, 1, usage);
 * @endcode
 */
template<typename Syscall>
void printUsage(Syscall* prn, int fd, std::vector<Descriptor> const& usage, int width = -1,
                int last_column_min_percent = 50, int last_column_own_line_max_percent = 75)
{
    SyscallWriter<Syscall> write(prn, fd);
    printUsageBackend(write, usage, width, last_column_min_percent, last_column_own_line_max_percent);
}

/**
 * @brief Print a usage Descriptor vector, see @ref printUsage()
 * @tparam Function is the function pointer type. This type can be deduced by `prn`.
 * @tparam Stream is the C-style stream type, like std::FILE. This type can be deduced by `stream`.
 * @param prn can be a C-style stream writing function with signature compatible to
 * `prn(const char* str, int size, int one, Stream stream)`.
 * @param stream is a C-style stream pointer, like stdout.
 * @param usage, width, last_column_min_percent, last_column_own_line_max_percent - see @ref
 * printUsage()
 *
 * Examples:
 * @code
 * printUsage(fwrite, stdout, usage);
 *
 * if(std::FILE* file = std::fopen("file.out", "w")) {
 *     printUsage(fwrite, file, usage);
 *     std::fclose(file);
 * }
 * @endcode
 */
template<typename Function, typename Stream>
void printUsage(Function* prn, Stream* stream, std::vector<Descriptor> const& usage, int width = -1,
                int last_column_min_percent = 50, int last_column_own_line_max_percent = 75)
{
    StreamWriter<Function, Stream> write(prn, stream);
    printUsageBackend(write, usage, width, last_column_min_percent, last_column_own_line_max_percent);
}


/**
 * @brief Outputs a nicely formatted usage string with support for multi-column formatting and
 * line-wrapping.
 *
 * printUsage() takes the @c help texts of a Descriptor vector and formats them into a usage
 * message, wrapping lines to achieve the desired output width.
 *
 * <b>Table formatting:</b>
 *
 * Aside from plain strings, which are simply line-wrapped, the usage may contain tables. Tables
 * are used to align elements in the output.
 *
 * @code
 * // Without a table. The explanatory texts are not aligned.
 * -c, --create  Creates something.
 * -k, --kill  Destroys something.
 *
 * // With table formatting. The explanatory texts are aligned.
 * -c, --create  Creates something.
 * -k, --kill    Destroys something.
 * @endcode
 *
 * Table formatting removes the need to pad help texts manually with spaces to achieve alignment.
 * To create a table, simply insert \\t (tab) characters to separate the cells within a row.
 *
 * @code
 * using Descriptor = imagefusion::option::Descriptor;
 * std::vector<Descriptor> usage{
 *   {..., "-c, --create  \tCreates something." },
 *   {..., "-k, --kill  \tDestroys something." }, ...
 * };
 * @endcode
 *
 * Note that you must include the minimum amount of space desired between cells yourself. Table
 * formatting will insert further spaces as needed to achieve alignment.
 *
 * You can insert line breaks within cells by using \\v (vertical tab).
 *
 * @code
 * std::vector<Descriptor> usage{
 *   {..., "-c,\v--create  \tCreates\vsomething." },
 *   {..., "-k,\v--kill  \tDestroys\vsomething." }, ...
 * };
 *
 * // results in
 *
 * -c,       Creates
 * --create  something.
 * -k,       Destroys
 * --kill    something.
 * @endcode
 *
 * You can mix lines that do not use \\t or \\v with those that do. The plain lines will not mess
 * up the table layout. Alignment of the table columns will be maintained even across these
 * interjections.
 *
 * @code
 * std::vector<Descriptor> usage{
 *   {...,            "-c, --create  \tCreates something." },
 *   Descriptor::text("----------------------------------"),
 *   {...,            "-k, --kill  \tDestroys something." }, ...
 * };
 *
 * // results in
 *
 * -c, --create  Creates something.
 * ----------------------------------
 * -k, --kill    Destroys something.
 * @endcode
 *
 * You can have multiple tables within the same usage whose columns are aligned independently.
 * Simply insert a Descriptor::breakTable() element
 *
 * @code
 * std::vector<Descriptor> usage{
 *   {..., "Long options:" },
 *   {..., "--very-long-option  \tDoes something long." },
 *   {..., "--ultra-super-mega-long-option  \tTakes forever to complete." },
 *   Descriptor::breakTable(),
 *   {..., "Short options:" },
 *   {..., "-s  \tShort." },
 *   {..., "-q  \tQuick." }, ...
 * };
 *
 * // results in
 *
 * Long options:
 * --very-long-option              Does something long.
 * --ultra-super-mega-long-option  Takes forever to complete.
 * Short options:
 * -s  Short.
 * -q  Quick.
 *
 * // Without the table break it would be
 *
 * Long options:
 * --very-long-option              Does something long.
 * --ultra-super-mega-long-option  Takes forever to complete.
 * Short options:
 * -s                              Short.
 * -q                              Quick.
 * @endcode
 *
 * The last cell of a row will be broken into multiple indented lines if required.
 *
 * @code
 * std::vector<Descriptor> usage{
 *   Descriptor::text("first cell  \there the second cell is really really long and will be indented at the second cell start."),
 *   Descriptor::text("This line would be not be indented, if it were too long and had to be broken... uups! ;-)"),
 *   Descriptor::text("also first cell  \tsecond cell  \tthird cell, which is way too long to be printed in a single line."), ...
 * };
 *
 * // results in
 *
 * first cell      here the second cell is really really long and
 *                 will be indented at the second cell start
 * This line would be not be indented, if it were too long and
 * had to be broken... uups! ;-)
 * also first cell second cell  third cell, which is way too long
 *                              to be printed in a single line.
 * @endcode
 *
 * If the last cell of a row should be considered for alignment of columns after that column, a
 * cell with a space can be added with "<code>\\t </code>":
 *
 * @code
 * std::vector<Descriptor> usage{
 *   Descriptor::text("Column 1 line 1  \t\tColumn 3 line 1\n"
 *                    "Column 1 line 2  \tColumn 2 line 2   \t \n" // note the space after the \t
 *                    "Column 1 line 3  \t\tColumn 3 line 3"), ...
 * };
 *
 * // results in
 *
 * Column 1 line 1                    Column 3 line 1
 * Column 1 line 2  Column 2 line 2
 * Column 1 line 3                    Column 3 line 3
 *
 * // Without the space cell it would be
 * Column 1 line 1  Column 3 line 1
 * Column 1 line 2  Column 2 line 2
 * Column 1 line 3  Column 3 line 3
 * @endcode
 * This behaviour is intended to allow multi lines (as in the previous example) and sub-tables like:
 * @code
 * std::vector<Descriptor> usage{
 *   {..., "  -o <bool>, --opt=<bool>  \tIf you give <bool> the value...\n"
 *         "\t \t* true and have...\v"
 *              "  - specified a filename, it will do this.\v"
 *              "  - not specified a filename, it will do that.\n"
 *         "\t \t* false it will just exit."}, ...
 * };
 *
 * // results in
 *   -o <bool>, --opt=<bool>  If you give <bool> the value...
 *                             * true and have...
 *                               - specified a filename, it will do this.
 *                               - not specified a filename, it will do that.
 *                             * false it will just exit.
 * @endcode
 * Here the sub-table relies on this behaviour, since otherwise `"* true [...]"` and the lines
 * below would be aligned after `"[...] the value..."`.
 *
 * <b>Output methods:</b>
 *
 *
 * Apart from this method there are further printUsage() functions implemented as a set of
 * functions. Hence, you have great flexibility in your choice of the output method. The following
 * examples demonstrate typical uses. Anything that's similar enough will work. Assume that usage
 * is some Descriptor vector as in other examples.
 *
 * Simple and most common example:
 * @code
 * printUsage(usage);
 * @endcode
 *
 * More advanced examples
 * @code
 * #include <unistd.h>  // write()
 * #include <iostream>  // cout
 * #include <sstream>   // ostringstream
 * #include <cstdio>    // fwrite()
 * using namespace std;
 *
 * void my_write(const char* str, int size) {
 *   fwrite(str, size, 1, stdout);
 * }
 *
 * struct MyWriter {
 *   void write(const char* buf, size_t size) const {
 *      fwrite(str, size, 1, stdout);
 *   }
 * };
 *
 * struct MyWriteFunctor {
 *   void operator()(const char* buf, size_t size) {
 *      fwrite(str, size, 1, stdout);
 *   }
 * };
 * ...
 * printUsage(usage);                 // again this function, same as printUsage(cout, usage)
 * printUsage(my_write, usage);       // custom write function
 * printUsage(MyWriter(), usage);     // temporary of a custom class
 * MyWriter writer;
 * printUsage(writer, usage);         // custom class object
 * MyWriteFunctor wfunctor;
 * printUsage(&wfunctor, usage);      // custom functor
 * printUsage(write, 1, usage);       // write() to file descriptor 1
 * printUsage(cout, usage);           // an ostream&
 * printUsage(fwrite, stdout, usage); // fwrite() to stdout
 * ostringstream sstr;
 * printUsage(sstr, usage);           // an ostringstream&
 *
 * @endcode
 *
 * @par Notes:
 * @li the @c write() method of a class that is to be passed as a temporary as @c MyWriter() is in
 *     the example, must be a @c const method, because temporary objects are passed as const
 *     reference. This only applies to temporary objects that are created and destroyed in the same
 *     statement. If you create an object like @c writer in the example, this restriction does not
 *     apply.
 * @li a functor like @c MyWriteFunctor in the example must be passed as a pointer. This differs
 *     from the way functors are passed to e.g. the STL algorithms.
 * @li All printUsage() functions are tiny wrappers around a shared non-template implementation. So
 *     there's no penalty for using different versions in the same program.
 * @li printUsage() always interprets Descriptor::help as UTF-8 and always produces UTF-8-encoded
 *     output. If your system uses a different charset, you must do your own conversion. You may
 *     also need to change the font of the console to see non-ASCII characters properly. This is
 *     particularly true for Windows.
 * @li @b Security @b warning: Do not insert untrusted strings (such as user-supplied arguments)
 *     into the usage. printUsage() has no protection against malicious UTF-8 sequences.
 *
 * @param usage the Descriptor vector whose @c help texts will be formatted.
 * @param width the maximum number of characters per output line. Note that this number is in
 *        actual characters, not bytes. printUsage() supports UTF-8 in @c help and will count
 *        multi-byte UTF-8 sequences properly. Asian wide characters are counted as 2 characters.
 *        The default argument of -1 means, it will try to find the column width itself.
 * @param last_column_min_percent (0-100) The minimum percentage of @c width that should be
 *        available for the last column (which typically contains the textual explanation of an
 *        option). If less space is available, the last column will be printed on its own line,
 *        indented according to @c last_column_own_line_max_percent.
 * @param last_column_own_line_max_percent (0-100) If the last column is printed on its own line
 *        due to less than @c last_column_min_percent of the width being available, then only
 *        @c last_column_own_line_max_percent of the extra line(s) will be used for the
 *        last column's text. This ensures an indentation. See example below.
 *
 * @code
 * // width=20, last_column_min_percent=50 (i.e. last col. min. width=10)
 * --3456789 1234567890
 *           1234567890
 *
 * // width=20, last_column_min_percent=75 (i.e. last col. min. width=15)
 * // last_column_own_line_max_percent=75
 * --3456789
 *      123456789012345
 *      67890
 *
 * // width=20, last_column_min_percent=75 (i.e. last col. min. width=15)
 * // last_column_own_line_max_percent=25 (i.e. max. 5)
 * --3456789
 *                12345
 *                67890
 *                12345
 *                67890
 * @endcode
 */
inline void printUsage(std::vector<Descriptor> const& usage, int width,
                int last_column_min_percent, int last_column_own_line_max_percent)
{
    OStreamWriter<std::ostream> write(std::cout);
    printUsageBackend(write, usage, width, last_column_min_percent, last_column_own_line_max_percent);
}


inline void Parse::AndSetMRImage(std::string const& str, MultiResImages& mri, std::string const& optName) {
    ImageInput in = MRImage(str, optName, true);
    mri.set(in.tag, in.date, std::move(in.i));
}

inline std::string Option::prop() const {
  return desc == nullptr ? std::string() : desc->prop;
}

inline std::string Option::spec() const {
  return desc == nullptr ? std::string() : desc->spec;
}

template<typename T>
inline ArgStatus ArgChecker::Vector(Option const& option) {
    Parse::Vector<T>(option.arg);
    return ArgStatus::OK;
}


inline ArgumentToken::operator string() const {
    return data;
}

inline bool ArgumentToken::empty() const {
    return data.empty();
}


} /* namespace option */
} /* namespace imagefusion */

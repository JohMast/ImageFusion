#include <boost/test/unit_test.hpp>

#include "optionparser.h"
#include "optionparserimpl.h"
#include "imagefusion.h"
#include "Image.h"
#include "MultiResImages.h"
#include "helpers_test.h"

#include <string>
#include <array>
#include <vector>

namespace imagefusion {
namespace option {

BOOST_AUTO_TEST_SUITE(optionparser_suite)


const char* useFirst0 = "Usage: prog_name [options]\n\n"
                        "Options:";
const char* useFirstA = "  -a \t\tJust Option a. Does not accept an argument.";
const char* useFirstB = "  -b \t\tJust Option b. Also no argument.";
const char* useFirstN = "  -n <num>, \t--number=<num> \tOption n with a number.";
const char* useFirstR = "  -r <rect>, \t--rectangle=<rect> \tSet some rectangle argument\v"
                          "<rect> requires all of the following arguments:\v"
                          "  -x <num>                 x start\v"
                          "  -y <num>                 y start\v"
                          "  -w <num>, --width=<num>  width\v"
                          "  -h <num>, --height=<num> height\v"
                          "Examples: --rectangle=(-x1 -y=2 --width 3 -h 4)\v"
                          "          -r (-x 1 -y 2 -w 3 -h 4)";
const char* useFirstS = "  -s <size>, \t--size=<size> \tSet some size argument.\v"
                          "<size> either receives the following arguments:\v"
                          "  -w <num>, --width=<num>  width\v"
                          "  -h <num>, --height=<num> height\v"
                          "or must have the form '<num>x<num>' or just '(<num> <num>)' both with optional spacing, where the first argument is the width and the second is the height.\v"
                          "Examples: --size=(-w 100 -h 200)\v"
                          "          --size=100x200\v"
                          "          --size=100*200\v"
                          "          --size=(100 200)";
const char* useFirstP = "  -p <point>, \t--point=<point> \tSet some 2D integer point argument.\v"
                          "<point> either receives the following arguments:\v"
                          "  -x <num>\v"
                          "  -y <num>\v"
                          "or must have (<num>, <num>) with optional spacing and comma, where the first argument is for x and the second is for y.\v"
                          "Examples: --point=(-x 5 -y 6)\v"
                          "          --point=(5, 6)";
const char* useFirstC = "  -c <coord>, \t--coordinate=<coord> \tSet some 2D double coordinate arguments.\v"
                          "<coord> either receives the following arguments:\v"
                          "  -x <float>\v"
                          "  -y <float>\v"
                          "or must have (<float>, <float>) with optional spacing and comma, where the first argument is for x and the second is for y.\v"
                          "Examples: --coordinate=(-x 3.1416 -y 42)\v"
                          "          --coordinate=(3.1416, 42)";
const char* useFirstD = "  -d <float-list>, \t--doublevec=<float-list> \tSet some double vector argument.\v"
                          "<float-list> must have the format '(<float> [<float> ...])' without commas inbetween or just '<float>'.\v"
                          "Examples: --doublevec=(3.1416 42 -1.5)"
                          "          --doublevec=3.1416";
const char* useFirstI = "  -i <img>, \t--image=<img> \tRead an image.\v"
                          "<img> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>]', where the arguments can have an arbitrary order.\v"
                          "  -f <file>,     --file=<file>       Specifies the image file path.\v"
                          "  -d <num>,      --date=<num>        Specifies the date.\v"
                          "  -t <tag>,      --tag=<tag>         Specifies the resolution tag string. <tag> can be an arbitrary string.\v"
                          "  -c <rect>,     --crop=<rect>       Optional. Specifies the crop window, where the image will be read. A zero width or height means full width "
                                                               "or height, respectively. For a description of <rect> see --rectangle=<rect>!\v"
                          "  -l <num-list>, --layers=<num-list> Optional. Specifies the channels or layers, that will be read. Hereby a 0 means the first channel.\v"
                          "                                     <num-list> must have the format '(<num> [<num> ...])', without commas inbetween or just '<num>'.\v"
                          "Examples: --image='-f  some_image.tif  -d 0 -t HIGH'\v"
                          "          --image=(-f 'test image.tif' -d 1 -t HIGH --crop=(-x 1 -y 2 -w 3 -h 2) -l (0 2) )";

std::vector<Descriptor> usageFirst = {
    Descriptor::text(useFirst0),
    { "A",         "", "a", "",           ArgChecker::None,           useFirstA},
    { "B",         "", "b", "",           ArgChecker::None,           useFirstB},
    { "N",         "", "n", "number",     ArgChecker::Int,            useFirstN},
    { "RECT",      "", "r", "rectangle",  ArgChecker::Rectangle,      useFirstR},
    { "SIZE",      "", "s", "size",       ArgChecker::Size,           useFirstS},
    { "POINT",     "", "p", "point",      ArgChecker::Point,          useFirstP},
    { "COORD",     "", "c", "coordinate", ArgChecker::Coordinate,     useFirstC},
    { "DOUBLEVEC", "", "d", "doublevec",  ArgChecker::Vector<double>, useFirstD},
    { "IMG",       "", "i", "image",      ArgChecker::MRImage<>,      useFirstI}
};

BOOST_AUTO_TEST_CASE(buildin_types)
{
    std::string arguments = "-ab -n 1 --rectangle='-x1 -y=2 --width 3 -h -4' --number=2 -n 3"
                            " -p'-x 5 -y=6' --size='7 x 8' --coordinate='9 -0.5'"
                            " --doublevec=(1 -3.14 42) -d (2) -d ( 3 ) -d ( ) -d ()"
                            " -i '-f  ../test_resources/images/formats/uint16x2.tif  -d 0 -t HIGH'"
                            // now nesting difficult options:
                            " -i (-f '../test_resources/images/formats/uint16x2.tif' -d 1 -t HIGH -c (-x 1 -y 2 -w 3 -h 2) -l 1)";
    OptionParser options(usageFirst);
    options.parse(arguments);
    BOOST_CHECK_EQUAL(options["A"].size(),         1);
    BOOST_CHECK_EQUAL(options["B"].size(),         1);
    BOOST_CHECK_EQUAL(options["N"].size(),         3);
    BOOST_CHECK_EQUAL(options["RECT"].size(),      1);
    BOOST_CHECK_EQUAL(options["SIZE"].size(),      1);
    BOOST_CHECK_EQUAL(options["POINT"].size(),     1);
    BOOST_CHECK_EQUAL(options["COORD"].size(),     1);
    BOOST_CHECK_EQUAL(options["IMG"].size(),       2);
    BOOST_CHECK_EQUAL(options["DOUBLEVEC"].size(), 5);

    BOOST_CHECK_EQUAL(Parse::Int(options["N"][0].arg), 1);
    BOOST_CHECK_EQUAL(Parse::Int(options["N"][1].arg), 2);
    BOOST_CHECK_EQUAL(Parse::Int(options["N"][2].arg), 3);

    Rectangle r = Parse::Rectangle(options["RECT"].back().arg);
    BOOST_CHECK_EQUAL(r.x,      1);
    BOOST_CHECK_EQUAL(r.y,      2);
    BOOST_CHECK_EQUAL(r.width,  3);
    BOOST_CHECK_EQUAL(r.height, -4);

    Size s = Parse::Size(options["SIZE"].back().arg);
    BOOST_CHECK_EQUAL(s.width,  7);
    BOOST_CHECK_EQUAL(s.height, 8);

    Point p = Parse::Point(options["POINT"].back().arg);
    BOOST_CHECK_EQUAL(p.x, 5);
    BOOST_CHECK_EQUAL(p.y, 6);

    Coordinate c = Parse::Coordinate(options["COORD"].back().arg);
    BOOST_CHECK_EQUAL(c.x,  9);
    BOOST_CHECK_EQUAL(c.y, -0.5);

    BOOST_CHECK_EQUAL(Parse::ImageFileName(options["IMG"].back().arg), "../test_resources/images/formats/uint16x2.tif");
    BOOST_CHECK_EQUAL(Parse::ImageFileName("../test_resources/images/formats/uint16x4.tif"), "../test_resources/images/formats/uint16x4.tif");

    BOOST_CHECK(Parse::ImageLayers(options["IMG"].back().arg) == std::vector<int>{1});
    BOOST_CHECK((Parse::ImageLayers("-l (4 2 1)") == std::vector<int>{4,2,1}));
    BOOST_CHECK(Parse::ImageLayers("-f ../test_resources/images/formats/uint16x4.tif") == std::vector<int>{});

    BOOST_CHECK_EQUAL(Parse::ImageCropRectangle(options["IMG"].back().arg), (CoordRectangle{1, 2, 3, 2}));
    BOOST_CHECK_EQUAL(Parse::ImageCropRectangle("-f ../test_resources/images/formats/uint16x4.tif"), CoordRectangle{});

    BOOST_CHECK_EQUAL(Parse::ImageDate(options["IMG"].back().arg), 1);
    BOOST_CHECK_THROW(Parse::ImageDate("-f ../test_resources/images/formats/uint16x4.tif"), invalid_argument_error);
    BOOST_CHECK(Parse::ImageHasDate(options["IMG"].back().arg));
    BOOST_CHECK(!Parse::ImageHasDate("-f ../test_resources/images/formats/uint16x4.tif"));

    BOOST_CHECK_EQUAL(Parse::ImageTag(options["IMG"].back().arg), "HIGH");
    BOOST_CHECK_THROW(Parse::ImageTag("-f ../test_resources/images/formats/uint16x4.tif"), invalid_argument_error);
    BOOST_CHECK(Parse::ImageHasTag(options["IMG"].back().arg));
    BOOST_CHECK(!Parse::ImageHasTag("-f ../test_resources/images/formats/uint16x4.tif"));

    BOOST_CHECK(!Parse::ImageIgnoreColorTable(options["IMG"].back().arg));
    BOOST_CHECK(Parse::ImageIgnoreColorTable("-f ../test_resources/images/formats/uint16x4.tif --disable-use-color-table"));

    BOOST_CHECK_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x4.tif -d 0"), invalid_argument_error);
    BOOST_CHECK_NO_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x4.tif -d 0", "", false, false, true));

    BOOST_CHECK_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x4.tif -t bla"), invalid_argument_error);
    BOOST_CHECK_NO_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x4.tif -t bla", "", false, true, false));

    BOOST_CHECK_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x4.tif"), invalid_argument_error);
    BOOST_CHECK_NO_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x4.tif", "", false, true, true));

    MultiResImages mri;
    for (auto const& opt : options["IMG"])
        Parse::AndSetMRImage(opt.arg, mri);

    // from: " -i '-f  ../test_resources/images/formats/uint16x2.tif  -d 0 -t HIGH'"
    if (mri.has("HIGH", 0)) {
        Image& i = mri.get("HIGH", 0);
        BOOST_CHECK_EQUAL(i.width(),    6);
        BOOST_CHECK_EQUAL(i.height(),   5);
        BOOST_CHECK_EQUAL(i.channels(), 2);
    }
    else
        BOOST_ERROR("Image HIGH 0 not read.");

    // from: " -i (-f '../test_resources/images/formats/uint16x2.tif' -d 1 -t HIGH -c (-x 1 -y 2 -w 3 -h 2) -l 1)"
    if (mri.has("HIGH", 1)) {
        Image& i = mri.get("HIGH", 1);
        BOOST_CHECK_EQUAL(i.width(),    3);
        BOOST_CHECK_EQUAL(i.height(),   2);
        BOOST_CHECK_EQUAL(i.channels(), 1);
    }
    else
        BOOST_ERROR("Image HIGH 1 not read.");

    mri.remove("HIGH", 1);
    ImageInput ii = Parse::MRImage(options["IMG"].back().arg);
    mri.set(ii.tag, ii.date, std::move(ii.i));

    if (mri.has("HIGH", 0) && mri.has("HIGH", 1)) {
        Image& i_full = mri.get("HIGH", 0);
        Image& i_crop = mri.get("HIGH", 1);
        i_full.crop({/*x*/ 1, /*y*/ 2, /*width*/ 3, /*height*/ 2});
        for (int y = 0; y < i_crop.height(); ++y)
            for (int x = 0; x < i_crop.width(); ++x)
                BOOST_CHECK_EQUAL(i_crop.at<uint16_t>(x, y, 0),
                                  i_full.at<uint16_t>(x, y, 1));
    }

    std::vector<std::vector<double>> vecs;
    vecs.push_back({1., -3.14, 42.});
    vecs.push_back({2.});
    vecs.push_back({3.});
    vecs.push_back({});
    vecs.push_back({});
    auto vec_it = vecs.begin();
    auto vec_end = vecs.end();
    BOOST_CHECK_EQUAL(options["DOUBLEVEC"].size(), vecs.size());
    for (auto const& opt : options["DOUBLEVEC"]) {
        BOOST_CHECK(vec_it != vec_end);
        std::vector<double> vd = Parse::Vector<double>(opt.arg);
//        for (double d : vd)
//            std::cout << d << '\t';
//        std::cout << std::endl;
        BOOST_CHECK_EQUAL_COLLECTIONS(vec_it->begin(), vec_it->end(), vd.begin(), vd.end());
        ++vec_it;
    }

    std::vector<std::string> names = {
        "a", "b", "n", "rectangle", "number", "n", "p", "size",
        "coordinate", "doublevec", "d", "d", "d", "d", "i", "i"};
    auto name_it = names.begin();
    BOOST_CHECK_EQUAL(options.input.size(), names.size());
    for (auto& o : options.input)
        BOOST_CHECK_EQUAL(o.name, *name_it++);
//    printUsage(std::cout, usageFirst);
}


BOOST_AUTO_TEST_CASE(const_correctness)
{
    std::string arguments = "-n 1 --number=2 -abn 3";
    OptionParser options(usageFirst);
    options.parse(arguments);
    auto const& opt_const = options;
    BOOST_CHECK_EQUAL(opt_const["A"].size(), 1);
    BOOST_CHECK_EQUAL(opt_const["B"].size(), 1);
    BOOST_CHECK_EQUAL(opt_const["N"].size(), 3);
    BOOST_CHECK_EQUAL(opt_const["RECT"].size(), 0);

    BOOST_CHECK_EQUAL(std::stoi(opt_const["N"][0].arg), 1);
    BOOST_CHECK_EQUAL(std::stoi(opt_const["N"][1].arg), 2);
    BOOST_CHECK_EQUAL(std::stoi(opt_const["N"][2].arg), 3);

    std::vector<std::string> optOrder = {"N", "N", "A", "B", "N"};
    for (unsigned int i = 0; i < options.optionCount(); ++i)
        BOOST_CHECK_EQUAL(opt_const[i].spec(), optOrder[i]);
}


// test to add a new sub-option to the image option
BOOST_AUTO_TEST_CASE(add_suboption)
{
    // test code is similar to the one in the documentation of the argument usageImage of Parse::Image
    std::string inputArgument = "-f ../test_resources/images/formats/uint16x2.tif  --foo=bar  -l 0";
    std::vector<Descriptor> usageFooImg = Parse::usageImage;
    usageFooImg.push_back(Descriptor{"FOO", "", "", "foo", ArgChecker::NonEmpty, ""});

    OptionParser fooOptions{usageFooImg};
    BOOST_CHECK_NO_THROW(fooOptions.parse(inputArgument));

    BOOST_CHECK(!fooOptions["FOO"].empty());
    if (!fooOptions["FOO"].empty())
        BOOST_CHECK_EQUAL(fooOptions["FOO"].back().arg, "bar");
    BOOST_CHECK_NO_THROW(Parse::Image(inputArgument, "", true, usageFooImg));
}


std::vector<Descriptor> usageTypeTest = {
    Descriptor::text("Usage..."),
    { "OPT", "ONE", "a", "", ArgChecker::None, "Option description..."},
    { "OPT", "TWO", "b", "", ArgChecker::None, "Option description..."}
};

BOOST_AUTO_TEST_CASE(type_test)
{
    std::string arguments = "-abbab";
    OptionParser options(usageTypeTest);
    options.parse(arguments);
    BOOST_CHECK_EQUAL(options["OPT"].size(), 5);
    std::vector<std::string> list = {"ONE", "TWO", "TWO", "ONE", "TWO"};
    for (unsigned int i = 0; i < options.optionCount(); ++i)
        BOOST_CHECK_EQUAL(options[i].prop(), list[i]);
}

BOOST_AUTO_TEST_CASE(option_end_test)
{
    std::string arguments = "-- -abbab";
    OptionParser options(usageTypeTest);
    options.parse(arguments);
    BOOST_CHECK(options["OPT"].empty());
    BOOST_CHECK_EQUAL(options.nonOptionArgCount(), 1);
    BOOST_CHECK_EQUAL(options.nonOptionArgs.at(0), "-abbab");
}


std::vector<Descriptor> usageUnbalancedParensTest = {
    { "A", "", "a", "aa", ArgChecker::Interval,    "Option description..."},
    { "B", "", "b", "bb", ArgChecker::IntervalSet, "Option description..."},
};

BOOST_AUTO_TEST_CASE(unbalancedParensTest)
{
    // using opening parens does not work:                        "-a  (0,2]    --aa  (3,5]    --aa=(6,8]");
    // even with quotes or parens around:                         "-a '(0, 2]'  --aa '(3  5]'  --aa='(6,8]'");
    // omitting helps:
    auto options = OptionParser::parse(usageUnbalancedParensTest, "-a '0, 2]'   --aa '3  5]'   --aa='6,8]'");

    if (options["A"].size() != 3) {
        std::string err = "Interval parsing with unbalanced paratheses failed. Expected '(0, 2]', '(3, 5]' and '(6, 8]', got '";
        for (auto const& o : options["A"])
            err += o.arg + "', '";
        err.pop_back(); err.pop_back(); err.pop_back();
        BOOST_ERROR(err);
    }
    else {
        BOOST_CHECK_EQUAL(Parse::Interval(options["A"].at(0).arg), Interval::left_open(0, 2)); // from -a '0, 2]'
        BOOST_CHECK_EQUAL(Parse::Interval(options["A"].at(1).arg), Interval::left_open(3, 5)); // from --aa '3  5]'
        BOOST_CHECK_EQUAL(Parse::Interval(options["A"].at(2).arg), Interval::left_open(6, 8)); // from --aa='6,8]'
    }


    // escaping parantheses works as well:
    options = OptionParser::parse(usageUnbalancedParensTest, "-b '0, 2] [4 5 6 7'   --bb=\\(6,8,9,11   --bb '[3  5 7,9]'");
    if (options["B"].size() != 3) {
        std::string err = "Interval parsing with unbalanced paratheses failed. Expected '0, 2] [4 5 6 7', '6,8,9,11' and '[3  5 7,9]', got '";
        for (auto const& o : options["B"])
            err += o.arg + "', '";
        err.pop_back(); err.pop_back(); err.pop_back();
        BOOST_ERROR(err);
    }
    else {
        BOOST_CHECK_EQUAL(Parse::IntervalSet(options["B"].at(0).arg), IntervalSet() + Interval::left_open(0, 2) + Interval::right_open(4, 5) + Interval::open(6, 7)); // from -b '0, 2] [4 5 6 7'
        BOOST_CHECK_EQUAL(Parse::IntervalSet(options["B"].at(1).arg), IntervalSet() + Interval::open(6, 8) + Interval::open(9, 11));                                  // from --bb='6,8,9,11'
        BOOST_CHECK_EQUAL(Parse::IntervalSet(options["B"].at(2).arg), IntervalSet() + Interval::right_open(3, 5) + Interval::left_open(7, 9));                        // from --bb '[3  5 7,9]'
    }
}

BOOST_AUTO_TEST_CASE(parsing)
{
    double a;
    /* parsing an angle
     * different formats:
     *  * degree, minutes, seconds, where degree has a mandatory "d", "deg", "°" appended
     *  * degree, minutes, where degree has a mandatory "d", "deg", "°" appended
     *  * degree with optional "d", "deg", "°"
     *  * radians with mandatory "rad" (otherwise it is interpreted as degree)
     */
    BOOST_CHECK_NO_THROW(a = Parse::Angle("4d48'38.51\""));
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("N4d48'38.51\""));
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("n4d48'38.51\""));
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4d48'38.51\"E"));
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4d48'38.51\"e"));
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("-4d48'38.51\""));
    BOOST_CHECK_CLOSE_FRACTION(a, -4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("S4d48'38.51\""));
    BOOST_CHECK_CLOSE_FRACTION(a, -4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("s4d48'38.51\""));
    BOOST_CHECK_CLOSE_FRACTION(a, -4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4d48'38.51\"W"));
    BOOST_CHECK_CLOSE_FRACTION(a, -4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4d48'38.51\"w"));
    BOOST_CHECK_CLOSE_FRACTION(a, -4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle(" 4 d 48 ' 38.51 \" ")); // test whitespace
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4d48'38.51''")); // test '' instead "
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4deg48'38.51''")); // test deg instead of d
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4º48'38.51''")); // test º (masculine ordinal indicator) instead of d
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4°48'38.51''")); // test ° (degree sign) instead of d
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4° 48.64183'")); // test degree and minutes only
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4.810697")); // test direct float
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4.810697 d")); // test direct float with d
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4.810697 deg")); // test direct float with deg
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("4.810697 º")); // test direct float with º
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("0.0839625rad")); // test angle in rad
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    BOOST_CHECK_NO_THROW(a = Parse::Angle("0.0839625 rad")); // test angle in rad
    BOOST_CHECK_CLOSE_FRACTION(a, 4.810697, 1e-4);

    Coordinate gc;
    /* parsing a geographic location
     * longitude (E/W) is saved in x, latitude (N/S) is saved in y
     * Formats are:
     *  * <angle>, <angle> (first is latitude, second is longitude)
     *  * N/S<angle>, E/W<angle>
     *  * E/W<angle>, N/S<angle>
     *  * <angle>N/S, <angle>E/W
     *  * <angle>E/W, <angle>N/S
     * Do not check different formats for angle here, since that has been done above
     */
    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51.327905, 6.967492")); // test y (latitude), x (longitude)
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51.327905 6.967492")); // test y (latitude) x (longitude)
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51.327905d 6.967492")); // test y (latitude) x (longitude), in >> angle should not swallow the second angle as arc minutes
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51d 19.674' 6.967492")); // test y (latitude) x (longitude), in >> angle should not swallow the second angle as arc seconds
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51d 19.674' 6.967492d")); // test y (latitude) x (longitude), in >> angle should not swallow the second angle as arc seconds
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("-51°19'40.5\" -6°58'03.0\"")); // test negative values
    BOOST_CHECK_CLOSE_FRACTION(gc.y, -51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, -6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51°19'40.5\"N 6°58'03.0\"E")); // test postfix N E
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51°19'40.5\"S 6°58'03.0\"e")); // test postfix S e
    BOOST_CHECK_CLOSE_FRACTION(gc.y, -51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("51°19'40.5\"n 6°58'03.0\"W")); // test postfix n W
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, -6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("6°58'03.0\"E 51°19'40.5\"N")); // test postfix E N
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("N51°19'40.5\" E6°58'03.0\"")); // test prefix N E
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("E6°58'03.0\" N51°19'40.5\"")); // test prefix E N
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("w6°58'03.0\" N51°19'40.5\"")); // test prefix w N
    BOOST_CHECK_CLOSE_FRACTION(gc.y, 51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, -6.967492,  1e-4);

    BOOST_CHECK_NO_THROW(gc = Parse::GeoCoord("E6°58'03.0\" s51°19'40.5\"")); // test prefix E s
    BOOST_CHECK_CLOSE_FRACTION(gc.y, -51.327905, 1e-4);
    BOOST_CHECK_CLOSE_FRACTION(gc.x, 6.967492,  1e-4);

    Type t;
    /* parsing a type
     * consists of <BASE> or <BASE>x<NUM>, where <BASE> is one of
     *  * uint8, byte,
     *  * int8,
     *  * uint16,
     *  * int16,
     *  * int32,
     *  * float32, single, float,
     *  * float64, double
     * and for <NUM> is must hold 1 <= <NUM> <= 4. The case is completely
     * ignored (even for the 'x'). Spaces around x and in the end are ignored.
     * Everything after <BASE> and before 'x' is ignored.
     */
    BOOST_CHECK_NO_THROW(t = Parse::Type("uint8"));
    BOOST_CHECK(t == Type::uint8);

    BOOST_CHECK_NO_THROW(t = Parse::Type("uint8x1"));
    BOOST_CHECK(t == Type::uint8);

    BOOST_CHECK_NO_THROW(t = Parse::Type("uInT8X1"));
    BOOST_CHECK(t == Type::uint8);

    BOOST_CHECK_NO_THROW(t = Parse::Type("byte"));
    BOOST_CHECK(t == Type::uint8);

    BOOST_CHECK_NO_THROW(t = Parse::Type("Bytex1"));
    BOOST_CHECK(t == Type::uint8);

    BOOST_CHECK_NO_THROW(t = Parse::Type("Byte x 2 "));
    BOOST_CHECK(t == Type::uint8x2);

    BOOST_CHECK_NO_THROW(t = Parse::Type("int8x2"));
    BOOST_CHECK(t == Type::int8x2);

    BOOST_CHECK_NO_THROW(t = Parse::Type("uint16x3"));
    BOOST_CHECK(t == Type::uint16x3);

    BOOST_CHECK_NO_THROW(t = Parse::Type("int16x3"));
    BOOST_CHECK(t == Type::int16x3);

    BOOST_CHECK_NO_THROW(t = Parse::Type("int32x4"));
    BOOST_CHECK(t == Type::int32x4);

    BOOST_CHECK_NO_THROW(t = Parse::Type("float32"));
    BOOST_CHECK(t == Type::float32);

    BOOST_CHECK_NO_THROW(t = Parse::Type("float"));
    BOOST_CHECK(t == Type::float32);

    BOOST_CHECK_NO_THROW(t = Parse::Type("single"));
    BOOST_CHECK(t == Type::float32);

    BOOST_CHECK_NO_THROW(t = Parse::Type("float64"));
    BOOST_CHECK(t == Type::float64);

    BOOST_CHECK_NO_THROW(t = Parse::Type("double "));
    BOOST_CHECK(t == Type::float64);


    Size s;
    Dimensions d;
    /* parsing a size
     * 1. "...<num>...<num>..." where '...' can be any chars in ' *x"')(,' and
     *    first argument will be used as width and second as height.
     *    pretty forms: "<num>x<num>"
     *                  "<num> * <num>"
     *                  "(<num> <num>)"
     *                  "<num>, <num>"
     * 2. arguments: -w <num> / --width=<num>
     *               -h <num> / --height=<num>
     *    As always: - only the last of each will be used.
     *               - one level of quoting will be stripped, i. e. the
     *                 outermost (...), '...' or "..."
     */
    BOOST_CHECK_NO_THROW(s = Parse::Size("  1    2"));
    BOOST_CHECK_EQUAL(s.width,  1);
    BOOST_CHECK_EQUAL(s.height, 2);

    BOOST_CHECK_NO_THROW(s = Parse::Size("xx  -1 xxxxx   x-2xx"));
    BOOST_CHECK_EQUAL(s.width,  -1);
    BOOST_CHECK_EQUAL(s.height, -2);

    BOOST_CHECK_NO_THROW(s = Parse::Size("xx**3*xxxxx   x4xx"));
    BOOST_CHECK_EQUAL(s.width,  3);
    BOOST_CHECK_EQUAL(s.height, 4);

    BOOST_CHECK_NO_THROW(s = Parse::Size("(xx*'*5*xxx\"xx   x'6xx)"));
    BOOST_CHECK_EQUAL(s.width,  5);
    BOOST_CHECK_EQUAL(s.height, 6);

    BOOST_CHECK_NO_THROW(s = Parse::Size("(xx*'*5*xxx\"xx   x'6xx)"));
    BOOST_CHECK_EQUAL(s.width,  5);
    BOOST_CHECK_EQUAL(s.height, 6);

    BOOST_CHECK_NO_THROW(s = Parse::Size("-w 5 -h 6 -w \"3\" -h 2"));
    BOOST_CHECK_EQUAL(s.width,  3);
    BOOST_CHECK_EQUAL(s.height, 2);

    BOOST_CHECK_NO_THROW(s = Parse::Size("-w'5' --height=(6)"));
    BOOST_CHECK_EQUAL(s.width,  5);
    BOOST_CHECK_EQUAL(s.height, 6);

    BOOST_CHECK_NO_THROW(s = Parse::Size("-w=5 --h=6"));
    BOOST_CHECK_EQUAL(s.width,  5);
    BOOST_CHECK_EQUAL(s.height, 6);

    BOOST_CHECK_NO_THROW(s = Parse::Size("-w1 '-h 3'"));
    BOOST_CHECK_EQUAL(s.width,  1);
    BOOST_CHECK_EQUAL(s.height, 3);

    BOOST_CHECK_NO_THROW(s = Parse::Size("-w 1 6 -h 2 meter"));
    BOOST_CHECK_EQUAL(s.width,  1);
    BOOST_CHECK_EQUAL(s.height, 2);

    BOOST_CHECK_NO_THROW(s = Parse::SizeSpecial("(xx*'*5*xxx\"xx   x'6xx)"));
    BOOST_CHECK_EQUAL(s.width,  5);
    BOOST_CHECK_EQUAL(s.height, 6);

    BOOST_CHECK_NO_THROW(s = Parse::SizeSubopts("-w 5 -h 6 -w \"3\" -h 2"));
    BOOST_CHECK_EQUAL(s.width,  3);
    BOOST_CHECK_EQUAL(s.height, 2);

    BOOST_CHECK_NO_THROW(d = Parse::Dimensions("-w=5.5 --h=6"));
    BOOST_CHECK_EQUAL(d.width,  5.5);
    BOOST_CHECK_EQUAL(d.height, 6);

    BOOST_CHECK_NO_THROW(d = Parse::Dimensions("-w1 '-h 3.5'"));
    BOOST_CHECK_EQUAL(d.width,  1);
    BOOST_CHECK_EQUAL(d.height, 3.5);

    BOOST_CHECK_NO_THROW(d = Parse::DimensionsSpecial("(xx*'*5*xxx\"xx   x'6xx)"));
    BOOST_CHECK_EQUAL(d.width,  5);
    BOOST_CHECK_EQUAL(d.height, 6);

    BOOST_CHECK_NO_THROW(d = Parse::DimensionsSubopts("-w 5 -h 6 -w \"3\" -h 2"));
    BOOST_CHECK_EQUAL(d.width,  3);
    BOOST_CHECK_EQUAL(d.height, 2);


    Point p;
    /* parsing a point
     * 1. "...<num>...<num>..." where '...' can be any chars in ' "')(,'
     *    and first argument will be used as x and second as y.
     *    pretty forms: "(<num>, <num>)"
     *                  "<num> <num>"
     * 2. arguments: -x <num>
     *               -y <num>
     *    As always: - only the last of each will be used.
     *               - one level of quoting will be stripped, i. e. the
     *                 outermost (...), '...' or "..."
     */
    BOOST_CHECK_NO_THROW(p = Parse::Point(")(-1,,,'\"2"));
    BOOST_CHECK_EQUAL(p.x, -1);
    BOOST_CHECK_EQUAL(p.y, 2);

    BOOST_CHECK_NO_THROW(p = Parse::Point("-x=(1) --y=\"2\""));
    BOOST_CHECK_EQUAL(p.x, 1);
    BOOST_CHECK_EQUAL(p.y, 2);

    BOOST_CHECK_NO_THROW(p = Parse::Point("-x1 -y2 -x3 -y4"));
    BOOST_CHECK_EQUAL(p.x, 3);
    BOOST_CHECK_EQUAL(p.y, 4);

    BOOST_CHECK_NO_THROW(p = Parse::Point("-x'5' -y 6"));
    BOOST_CHECK_EQUAL(p.x, 5);
    BOOST_CHECK_EQUAL(p.y, 6);

    BOOST_CHECK_NO_THROW(p = Parse::Point("-x1 '-y 3'"));
    BOOST_CHECK_EQUAL(p.x, 1);
    BOOST_CHECK_EQUAL(p.y, 3);

    BOOST_CHECK_NO_THROW(p = Parse::Point("-x 1 6 -y 2 meter"));
    BOOST_CHECK_EQUAL(p.x, 1);
    BOOST_CHECK_EQUAL(p.y, 2);

    BOOST_CHECK_NO_THROW(p = Parse::PointSpecial(")(-1,,,'\"2"));
    BOOST_CHECK_EQUAL(p.x, -1);
    BOOST_CHECK_EQUAL(p.y, 2);

    BOOST_CHECK_NO_THROW(p = Parse::PointSubopts("-x 5 -y 6 -x \"3\" -y 2"));
    BOOST_CHECK_EQUAL(p.x, 3);
    BOOST_CHECK_EQUAL(p.y, 2);


    Coordinate c;
    /* parsing a coordinate
     * 1. "...<float>...<float>..." where '...' can be any chars in ' "')(,'
     *    and first argument will be used as x and second as y.
     *    pretty forms: "(<float>, <float>)"
     *                  "<float> <float>"
     * 2. arguments: -x <float>
     *               -y <float>
     *    As always: - only the last of each will be used.
     *               - one level of quoting will be stripped, i. e. the
     *                 outermost (...), '...' or "..."
     */
    BOOST_CHECK_NO_THROW(c = Parse::Coordinate(")(-1.5,,,'\"2.25"));
    BOOST_CHECK_EQUAL(c.x, -1.5);
    BOOST_CHECK_EQUAL(c.y, 2.25);

    BOOST_CHECK_NO_THROW(c = Parse::Coordinate("-x=(1.) --y=\".5\""));
    BOOST_CHECK_EQUAL(c.x, 1);
    BOOST_CHECK_EQUAL(c.y, 0.5);

    BOOST_CHECK_NO_THROW(c = Parse::Coordinate("-x1 -y2 -x3e4 -y5e-1"));
    BOOST_CHECK_EQUAL(c.x, 30000);
    BOOST_CHECK_EQUAL(c.y, 0.5);

    BOOST_CHECK_NO_THROW(c = Parse::Coordinate("-x'5' -y 6"));
    BOOST_CHECK_EQUAL(c.x, 5);
    BOOST_CHECK_EQUAL(c.y, 6);

    BOOST_CHECK_NO_THROW(c = Parse::Coordinate("-x1 '-y 3'"));
    BOOST_CHECK_EQUAL(c.x, 1);
    BOOST_CHECK_EQUAL(c.y, 3);

    BOOST_CHECK_NO_THROW(c = Parse::Coordinate("-x 1 6 -y 2 meter"));
    BOOST_CHECK_EQUAL(c.x, 1);
    BOOST_CHECK_EQUAL(c.y, 2);

    BOOST_CHECK_NO_THROW(c = Parse::CoordinateSpecial(")(-1,,,'\"2"));
    BOOST_CHECK_EQUAL(c.x, -1);
    BOOST_CHECK_EQUAL(c.y, 2);

    BOOST_CHECK_NO_THROW(c = Parse::CoordinateSubopts("-x 5 -y 6 -x \"3\" -y 2"));
    BOOST_CHECK_EQUAL(c.x, 3);
    BOOST_CHECK_EQUAL(c.y, 2);


    Interval i;
    /* parse an interval
     * 1. "(<float>...<float>)" where '...' can be any chars in " ,"
     * 2. "[<float>...<float>)" where '...' can be any chars in " ,"
     * 3. "(<float>...<float>]" where '...' can be any chars in " ,"
     * 4. "[<float>...<float>]" where '...' can be any chars in " ,"
     *    and first argument will be used as lower and second as upper bound.
     */
    BOOST_CHECK_NO_THROW(i = Parse::Interval(" (-1.5, +2.25)"));
    BOOST_CHECK_EQUAL(i.lower(), -1.5);
    BOOST_CHECK_EQUAL(i.upper(), 2.25);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("[ +1.5, 2.5 )"));
    BOOST_CHECK_EQUAL(i.lower(), 1.5);
    BOOST_CHECK_EQUAL(i.upper(), 2.5);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::right_open());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("(  -2.25  -1.25 ]  "));
    BOOST_CHECK_EQUAL(i.lower(), -2.25);
    BOOST_CHECK_EQUAL(i.upper(), -1.25);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::left_open());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("[ 1.5, 2.5 ]"));
    BOOST_CHECK_EQUAL(i.lower(), 1.5);
    BOOST_CHECK_EQUAL(i.upper(), 2.5);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::closed());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("  -1.5, 2.25   "));
    BOOST_CHECK_EQUAL(i.lower(), -1.5);
    BOOST_CHECK_EQUAL(i.upper(), 2.25);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("  1 2   "));
    BOOST_CHECK_EQUAL(i.lower(), 1);
    BOOST_CHECK_EQUAL(i.upper(), 2);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("  1. 2.   "));
    BOOST_CHECK_EQUAL(i.lower(), 1);
    BOOST_CHECK_EQUAL(i.upper(), 2);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("  100   "));
    BOOST_CHECK_EQUAL(i.lower(), 100);
    BOOST_CHECK_EQUAL(i.upper(), 100);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::closed());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("200"));
    BOOST_CHECK_EQUAL(i.lower(), 200);
    BOOST_CHECK_EQUAL(i.upper(), 200);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::closed());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("-200"));
    BOOST_CHECK_EQUAL(i.lower(), -200);
    BOOST_CHECK_EQUAL(i.upper(), -200);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::closed());

    BOOST_CHECK_NO_THROW(i = Parse::Interval("+200"));
    BOOST_CHECK_EQUAL(i.lower(), 200);
    BOOST_CHECK_EQUAL(i.upper(), 200);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::closed());

    BOOST_CHECK_NO_THROW(i = Parse::Interval(".5"));
    BOOST_CHECK_EQUAL(i.lower(), 0.5);
    BOOST_CHECK_EQUAL(i.upper(), 0.5);
    BOOST_CHECK_EQUAL(i.bounds(), boost::icl::interval_bounds::closed());


    IntervalSet is;
    /* parse an interval set
     * 1. "<interval> [<interval> ...]"
     */
    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet(" (-1.5, 2.25)"));
    BOOST_CHECK_EQUAL(length(is), 2.25 - (-1.5));
    BOOST_CHECK_EQUAL(interval_count(is), 1);
    BOOST_CHECK_EQUAL(is.begin()->lower(), -1.5);
    BOOST_CHECK_EQUAL(is.begin()->upper(), 2.25);
    BOOST_CHECK_EQUAL(is.begin()->bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet(" (-inf, 0)"));
    BOOST_CHECK_EQUAL(is.begin()->lower(), -std::numeric_limits<double>::infinity());
    BOOST_CHECK_EQUAL(is.begin()->upper(), 0);
    BOOST_CHECK_EQUAL(is.begin()->bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet("  1  "));
    BOOST_CHECK_EQUAL(is.begin()->lower(), 1);
    BOOST_CHECK_EQUAL(is.begin()->upper(), 1);
    BOOST_CHECK_EQUAL(is.begin()->bounds(), boost::icl::interval_bounds::closed());

    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet("2"));
    BOOST_CHECK_EQUAL(is.begin()->lower(), 2);
    BOOST_CHECK_EQUAL(is.begin()->upper(), 2);
    BOOST_CHECK_EQUAL(is.begin()->bounds(), boost::icl::interval_bounds::closed());
    return;
    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet(" [-INFINITY, INF]"));
    BOOST_CHECK_EQUAL(is.begin()->lower(), -std::numeric_limits<double>::infinity());
    BOOST_CHECK_EQUAL(is.begin()->upper(), std::numeric_limits<double>::infinity());
    BOOST_CHECK_EQUAL(is.begin()->bounds(), boost::icl::interval_bounds::open());

    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet("1 2 , 3 4"));
    BOOST_CHECK_EQUAL(is, IntervalSet() + Interval::open(1, 2) + Interval::open(3, 4));

    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet(" [-1.5, 2.25)  (2, 5] (5, 6)"));
    BOOST_CHECK_EQUAL(length(is), 6 - (-1.5));
    BOOST_CHECK_EQUAL(interval_count(is), 1);
    BOOST_CHECK_EQUAL(is.begin()->lower(), -1.5);
    BOOST_CHECK_EQUAL(is.begin()->upper(), 6);
    BOOST_CHECK_EQUAL(is.begin()->bounds(), boost::icl::interval_bounds::right_open());
    BOOST_CHECK_EQUAL(is, IntervalSet() + Interval::right_open(-1.5, 2.25) + Interval::left_open(2, 5) + Interval::open(5, 6));

    BOOST_CHECK_NO_THROW(is = Parse::IntervalSet(" (1, 2],  [3,4]  5 6 , 7 8"));
    BOOST_CHECK_EQUAL(length(is), 4);
    BOOST_CHECK_EQUAL(interval_count(is), 4);
    BOOST_CHECK_EQUAL(is, IntervalSet() + Interval::left_open(1, 2) + Interval::closed(3, 4) + Interval::open(5, 6) + Interval::open(7, 8));
    std::vector<double> low{1, 3, 5, 7};
    std::vector<double> up{2, 4, 6, 8};
    std::vector<boost::icl::interval_bounds> bounds{
        boost::icl::interval_bounds::left_open(),
        boost::icl::interval_bounds::closed(),
        boost::icl::interval_bounds::open(),
        boost::icl::interval_bounds::open()
    };

    auto i_it = is.begin();
    for (unsigned int i = 0; i < interval_count(is); ++i) {
        BOOST_CHECK_EQUAL(i_it->lower(), low.at(i));
        BOOST_CHECK_EQUAL(i_it->upper(), up.at(i));
        BOOST_CHECK_EQUAL(i_it->bounds(), bounds.at(i));
        ++i_it;
    }


    Rectangle r;
    CoordRectangle cr;
    /* parsing a rectangle
     * arguments: -x <num> / -x (<num> <num>)
     *            -y <num> / -y (<num> <num>)
     *            -c (<num> <num>) / --center=(<num> <num>)
     *            -w <num> / --width=<num>
     *            -h <num> / --height=<num>
     * As always: - only the last of each same will be used.
     *            - one level of quoting will be stripped, i. e. the
     *              outermost (...), '...' or "..."
     */
    BOOST_CHECK_NO_THROW(r = Parse::Rectangle("'-w 123' -h456 --width=3 -x12 -y-99 --h=4 -x(1) -y='2'"));
    BOOST_CHECK_EQUAL(r.x,      1);
    BOOST_CHECK_EQUAL(r.y,      2);
    BOOST_CHECK_EQUAL(r.width,  3);
    BOOST_CHECK_EQUAL(r.height, 4);

    BOOST_CHECK_NO_THROW(r = Parse::Rectangle("-x(1 3) -y='2 5'"));
    BOOST_CHECK_EQUAL(r.x,      1);
    BOOST_CHECK_EQUAL(r.y,      2);
    BOOST_CHECK_EQUAL(r.width,  3);
    BOOST_CHECK_EQUAL(r.height, 4);

    BOOST_CHECK_NO_THROW(r = Parse::Rectangle("--center=(2 3.5) -w=3 -h 4"));
    BOOST_CHECK_EQUAL(r.x,      1);
    BOOST_CHECK_EQUAL(r.y,      2);
    BOOST_CHECK_EQUAL(r.width,  3);
    BOOST_CHECK_EQUAL(r.height, 4);

    BOOST_CHECK_NO_THROW(cr = Parse::CoordRectangle("--center=(2 3.5) -w=3 -h 4"));
    BOOST_CHECK_EQUAL(cr.x,      0.5);
    BOOST_CHECK_EQUAL(cr.y,      1.5);
    BOOST_CHECK_EQUAL(cr.width,  3);
    BOOST_CHECK_EQUAL(cr.height, 4);

    BOOST_CHECK_NO_THROW(cr = Parse::CoordRectangle("-x(1 3) -y='2 5'"));
    BOOST_CHECK_EQUAL(cr.x,      1);
    BOOST_CHECK_EQUAL(cr.y,      2);
    BOOST_CHECK_EQUAL(cr.width,  2);
    BOOST_CHECK_EQUAL(cr.height, 3);

    // parse int
    char const* arg_float = "1.1";
    Option of{nullptr, arg_float, arg_float};
    BOOST_CHECK_THROW(ArgChecker::Int(of), invalid_argument_error);

    char const* arg_int = "11";
    Option oi{nullptr, arg_int, arg_int};
    BOOST_CHECK(ArgChecker::Int(oi) == ArgStatus::OK);

    // parse masks
    Image mask1, mask2;
    ImageInput imgin;
    // test RGB indexed file. C0: 5*x + 40*y, C1: 255 - 5*x - 40*y, C2: 40*x + 5*y
    // bit    7 6 5 4 3 2 1 0
    // mask   1 0 1 0 1 0 1 0
    // res    7   5   3   1
    // vals 128  32   8   2
    // range values:
    //  3:            8 + 2 = 10  <--  min
    //  4:       32         = 32
    //  5:       32   +   2 = 34
    //  6:       32 + 8     = 40
    //  7:       32 + 8 + 2 = 42  <--  max
    Image ref{"../test_resources/images/formats/uint8x3_colortable.png"};
    constexpr uint8_t bitmask = 0b01010101;
    BOOST_CHECK_NO_THROW(mask1 = Parse::Mask("-f ../test_resources/images/formats/uint8x3_colortable.png  -b 7 -b 3 -b 5 -b1  --valid-ranges=[3,7]"));
    BOOST_CHECK_NO_THROW(imgin = Parse::MRMask("-f ../test_resources/images/formats/uint8x3_colortable.png  --extract-bits=3,5,7,1  --invalid-ranges='[1,2] [8,15]", /*optName*/ "", /*readImage*/ true, /*isDateOpt*/ true, /*isTagOpt*/ true));
    mask2 = Image(imgin.i.sharedCopy());
    BOOST_CHECK_EQUAL(mask1.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(mask2.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(mask1.size(), ref.size());
    BOOST_CHECK_EQUAL(mask1.size(), mask2.size());
    for (int y = 0; y < ref.height(); ++y) {
        for (int x = 0; x < ref.width(); ++x) {
            for (unsigned int c = 0; c < ref.channels(); ++c) {
                uint8_t r = ref.at<uint8_t>(x, y, c) & bitmask;
                uint8_t m1 = mask1.at<uint8_t>(x, y, c);
                uint8_t m2 = mask2.at<uint8_t>(x, y, c);
                BOOST_CHECK(10 <= r && r <= 42 ? m1 == 255 : m1 == 0);
                BOOST_CHECK(10 <= r && r <= 42 ? m2 == 255 : m2 == 0);
            }
        }
    }

    BOOST_CHECK_NO_THROW(mask1 = Parse::Mask("-f ../test_resources/images/formats/uint8x3_colortable.png  --valid-ranges=[3,7]")); // no bit option
    BOOST_CHECK_NO_THROW(imgin = Parse::MRMask("-f ../test_resources/images/formats/uint8x3_colortable.png  --invalid-ranges='[0,2] [8,255]", /*optName*/ "", /*readImage*/ true, /*isDateOpt*/ true, /*isTagOpt*/ true));
    mask2 = Image(imgin.i.sharedCopy());
    BOOST_CHECK_EQUAL(mask1.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(mask2.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(mask1.size(), ref.size());
    BOOST_CHECK_EQUAL(mask1.size(), mask2.size());
    for (int y = 0; y < ref.height(); ++y) {
        for (int x = 0; x < ref.width(); ++x) {
            for (unsigned int c = 0; c < ref.channels(); ++c) {
                uint8_t r  = ref.at<uint8_t>(x, y, c);
                uint8_t m1 = mask1.at<uint8_t>(x, y, c);
                uint8_t m2 = mask2.at<uint8_t>(x, y, c);
                BOOST_CHECK(3 <= r && r <= 7 ? m1 == 255 : m1 == 0);
                BOOST_CHECK(3 <= r && r <= 7 ? m2 == 255 : m2 == 0);
            }
        }
    }

    BOOST_CHECK_NO_THROW(mask1 = Parse::Mask("-f ../test_resources/images/formats/uint8x3_colortable.png  -b 7 -b 3 -b 5 -b1")); // no ranges option
    BOOST_CHECK_NO_THROW(imgin = Parse::MRMask("-f ../test_resources/images/formats/uint8x3_colortable.png  --extract-bits=3,5,7,1", /*optName*/ "", /*readImage*/ true, /*isDateOpt*/ true, /*isTagOpt*/ true));
    mask2 = Image(imgin.i.sharedCopy());
    BOOST_CHECK_EQUAL(mask1.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(mask2.type(), Type::uint8x3);
    BOOST_CHECK_EQUAL(mask1.size(), ref.size());
    BOOST_CHECK_EQUAL(mask1.size(), mask2.size());
    for (int y = 0; y < ref.height(); ++y) {
        for (int x = 0; x < ref.width(); ++x) {
            for (unsigned int c = 0; c < ref.channels(); ++c) {
                uint8_t r = ref.at<uint8_t>(x, y, c) & bitmask;
                uint8_t m1 = mask1.at<uint8_t>(x, y, c);
                uint8_t m2 = mask2.at<uint8_t>(x, y, c);
                BOOST_CHECK(r != 0 ? m1 == 255 : m1 == 0);
                BOOST_CHECK(r != 0 ? m2 == 255 : m2 == 0);
            }
        }
    }

    // parse vectors
    std::vector<int> vi = Parse::Vector<int>("1 , 2, 3   ,4");
    BOOST_CHECK_EQUAL(vi.size(), 4);
    BOOST_CHECK_EQUAL(vi.at(0), 1);
    BOOST_CHECK_EQUAL(vi.at(1), 2);
    BOOST_CHECK_EQUAL(vi.at(2), 3);
    BOOST_CHECK_EQUAL(vi.at(3), 4);

    std::vector<double> vd = Parse::Vector<double>("1.5 2 3.5   4");
    BOOST_CHECK_EQUAL(vd.size(), 4);
    BOOST_CHECK_EQUAL(vd.at(0), 1.5);
    BOOST_CHECK_EQUAL(vd.at(1), 2);
    BOOST_CHECK_EQUAL(vd.at(2), 3.5);
    BOOST_CHECK_EQUAL(vd.at(3), 4);

    std::vector<Size> vs = Parse::Vector<Size>("1x2 (3 x 4) '-5 6'");
    BOOST_CHECK_EQUAL(vs.size(), 3);
    BOOST_CHECK_EQUAL(vs.at(0), (Size{1, 2}));
    BOOST_CHECK_EQUAL(vs.at(1), (Size{3, 4}));
    BOOST_CHECK_EQUAL(vs.at(2), (Size{-5, 6}));

    std::vector<Rectangle> vr = Parse::Vector<Rectangle>("(-x -1 -y 2 -w 3 -h 4), (-x=-3 -y=4 --width=5 --height=6)");
    BOOST_CHECK_EQUAL(vr.size(), 2);
    BOOST_CHECK_EQUAL(vr.at(0), (Rectangle{-1, 2, 3, 4}));
    BOOST_CHECK_EQUAL(vr.at(1), (Rectangle{-3, 4, 5, 6}));

    std::vector<Point> vp = Parse::Vector<Point>("(-1, 2) (3 4) (-x=5 -y=6)");
    BOOST_CHECK_EQUAL(vp.size(), 3);
    BOOST_CHECK_EQUAL(vp.at(0), (Point{-1, 2}));
    BOOST_CHECK_EQUAL(vp.at(1), (Point{3,  4}));
    BOOST_CHECK_EQUAL(vp.at(2), (Point{5,  6}));

    std::vector<Coordinate> vc = Parse::Vector<Coordinate>("(-x 1.5 -y -2) (-3.5, 4) (-x=-3 -y=4e1)");
    BOOST_CHECK_EQUAL(vc.size(), 3);
    BOOST_CHECK_EQUAL(vc.at(0), (Coordinate{1.5, -2}));
    BOOST_CHECK_EQUAL(vc.at(1), (Coordinate{-3.5, 4}));
    BOOST_CHECK_EQUAL(vc.at(2), (Coordinate{-3,  40}));

    // parse images
    BOOST_CHECK_NO_THROW(Parse::Image("-f ../test_resources/images/formats/uint16x2.tif -l 1 -c (-x 1 -y 2 -w 3 -h 2)"));
    BOOST_CHECK_NO_THROW(Parse::Image("-f ../test_resources/images/formats/uint16x2.tif -l (1 0) -c (-x 1 -y 2 -w 3 -h 2)"));
    BOOST_CHECK_NO_THROW(Parse::Image("-f ../test_resources/images/formats/uint16x2.tif"));
    BOOST_CHECK_NO_THROW(Parse::Image("../test_resources/images/formats/uint16x2.tif"));
    BOOST_CHECK_NO_THROW(Parse::Image("../test_resources/images/formats/uint16x2.tif -l 1 -c (-x 1 -y 2 -w 3 -h 2)"));
    BOOST_CHECK_NO_THROW(Parse::Image("-l 1 -c (-x 1 -y 2 -w 3 -h 2) ../test_resources/images/formats/uint16x2.tif"));

    BOOST_CHECK_NO_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x2.tif  -d 0  -t HIGH"));
    BOOST_CHECK_NO_THROW(Parse::MRImage("../test_resources/images/formats/uint16x2.tif  -d 0  -t HIGH"));
    BOOST_CHECK_NO_THROW(Parse::MRImage("-d 0  -t HIGH  ../test_resources/images/formats/uint16x2.tif"));

//    std::vector<Image> vi = Parse::Vector<Image>("../test_resources/images/formats/uint16x2.tif "
//                                                 "(-f ../test_resources/images/formats/uint16x2.tif) "
//                                                 "(-f ../test_resources/images/formats/uint16x2.tif -l (1 0) -c (-x 1 -y 2 -w 3 -h 2))");
//    BOOST_CHECK_EQUAL(vi.size(), 3);
//    BOOST_CHECK_EQUAL(vi[0].size(), Size(5,6));
//    BOOST_CHECK_EQUAL(vi[1].size(), Size(5,6));
//    BOOST_CHECK_EQUAL(vi[2].size(), Size(3,2));

//    std::vector<ImageInput> vmi = Parse::Vector<ImageInput>("(-f ../test_resources/images/formats/uint16x2.tif -d 1 -t res) "
//                                                            "(-f ../test_resources/images/formats/uint16x2.tif -d 2 -t bla) "
//                                                            "(-f ../test_resources/images/formats/uint16x2.tif -d 3 -t res -l (1 0) -c (-x 1 -y 2 -w 3 -h 2))");
//    BOOST_CHECK_EQUAL(vmi.size(), 3);
//    BOOST_CHECK_EQUAL(vmi[0].i.size(), Size(5,6));
//    BOOST_CHECK_EQUAL(vmi[1].i.size(), Size(5,6));
//    BOOST_CHECK_EQUAL(vmi[2].i.size(), Size(3,2));
//    BOOST_CHECK_EQUAL(vmi[0].date, 1);
//    BOOST_CHECK_EQUAL(vmi[1].date, 2);
//    BOOST_CHECK_EQUAL(vmi[2].date, 3);
//    BOOST_CHECK_EQUAL(vmi[0].tag, "res");
//    BOOST_CHECK_EQUAL(vmi[1].tag, "tag");
//    BOOST_CHECK_EQUAL(vmi[2].tag, "res");
}

BOOST_AUTO_TEST_CASE(subdatasets)
{
    std::string sdsFilename = "test.nc";
    if (!createMultiImageFile(sdsFilename))
        return;

    Image img_num;
    BOOST_CHECK_NO_THROW(img_num = Parse::Image(sdsFilename + " -l 0"));
    Image img_name;
    BOOST_CHECK_NO_THROW(img_name = Parse::Image("'NETCDF:\"" + sdsFilename + "\":Band1'"));
    for (int y = 0; y < img_num.height() && y < img_name.height(); ++y)
        for (int x = 0; x < img_num.width() && x < img_name.width(); ++x)
            BOOST_CHECK_EQUAL(img_num.at<uint8_t>(x, y), img_name.at<uint8_t>(x, y));

    BOOST_CHECK_EQUAL(Parse::ImageFileName("'NETCDF:\"" + sdsFilename + "\":Band1'"), sdsFilename);
    BOOST_CHECK_EQUAL(Parse::ImageFileName("-f " + sdsFilename + " -l 0,1"), sdsFilename);
}

BOOST_AUTO_TEST_CASE(bad_parsing)
{
    BOOST_CHECK_THROW(Parse::Int(""),    invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Int("1.1"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Int("a"),   invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Float(""),  invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Float("a"), invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Angle("4 48'38.51\""), invalid_argument_error); // missing degree symbol
    BOOST_CHECK_THROW(Parse::Angle("4d48 38.51\""), invalid_argument_error); // missing minute symbol
    BOOST_CHECK_THROW(Parse::Angle("4d48'38.51"  ), invalid_argument_error); // missing second symbol
    BOOST_CHECK_THROW(Parse::Angle("4d48'38.51\" bla"), invalid_argument_error); // additional stuff after seconds
    BOOST_CHECK_THROW(Parse::Angle("4d48'38.51\" 12"), invalid_argument_error);  // additional stuff after seconds
    BOOST_CHECK_THROW(Parse::Angle("4a48'38.51\""), invalid_argument_error);  // wrong degree symbol
    BOOST_CHECK_THROW(Parse::Angle("N -4d 48' 38.51\""), invalid_argument_error);  // negative deg, with cardinal direction
    BOOST_CHECK_THROW(Parse::Angle("-4d 48' 38.51\" N"), invalid_argument_error);  // negative deg, with cardinal direction
    BOOST_CHECK_THROW(Parse::Angle("4d -48' 38.51\""), invalid_argument_error);  // negative minutes
    BOOST_CHECK_THROW(Parse::Angle("4d 48' -38.51\""), invalid_argument_error);  // negative seconds
    BOOST_CHECK_THROW(Parse::Angle("4d 68' 38.51\""), invalid_argument_error);  // minutes too large
    BOOST_CHECK_THROW(Parse::Angle("4d 48' 68.51\""), invalid_argument_error);  // seconds too large

    BOOST_CHECK_THROW(Parse::GeoCoord("-51°19'40.5\"N 6°58'03.0\"E" ), invalid_argument_error); // test negative latitude with cardinal direction
    BOOST_CHECK_THROW(Parse::GeoCoord("N -51°19'40.5\" 6°58'03.0\"E" ), invalid_argument_error); // test negative latitude with cardinal direction
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N -6°58'03.0\"E" ), invalid_argument_error); // test negative longitude with cardinal direction
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N E -6°58'03.0\"" ), invalid_argument_error); // test negative longitude with cardinal direction
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N 6°58'03.0\"" ), invalid_argument_error); // test missing E/W and existing N/S
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"  6°58'03.0\"E"), invalid_argument_error); // test missing N/S and existing E/W
    BOOST_CHECK_THROW(Parse::GeoCoord(",51°19'40.5\"N  6°58'03.0\"E"), invalid_argument_error); // test garbage at the start
    BOOST_CHECK_THROW(Parse::GeoCoord("bla 51°19'40.5\"N  6°58'03.0\"E"), invalid_argument_error); // test garbage at the start
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N bla 6°58'03.0\"E"), invalid_argument_error); // test garbage in the middle
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N / 6°58'03.0\"E"), invalid_argument_error); // test garbage in the middle
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N 6°58'03.0\"E bla"), invalid_argument_error); // test garbage at the end
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\"N 6°58'03.0\"E,"), invalid_argument_error); // test garbage at the end
    BOOST_CHECK_THROW(Parse::GeoCoord("51°19'40.5\""), invalid_argument_error); // test missing second angle

    BOOST_CHECK_THROW(Parse::Type(" uint8"),  invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Type("uint8x0"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Type("uint8x5"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Type("uint"),    invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Type("int"),     invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Size(""),            invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("1"),           invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("1.1"),         invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("1.1 2"),       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("1 2 3"),       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("-w=1 3"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("--w1 -h3"),    invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("'-w 1 -h 3'"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("1 -h 3"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Size("bla"),         invalid_argument_error);
    BOOST_CHECK_THROW(Parse::SizeSpecial("-w=5 -h=6"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::SizeSubopts("5 x 6"),     invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Dimensions("1 2 3"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::DimensionsSpecial("-w=5 -h=6"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::DimensionsSubopts("5 x 6"),     invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Point(""),            invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("1"),           invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("1.1"),         invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("1.1 2"),       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("1 2 3"),       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("-x=1 3"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("1 -y 3"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Point("bla"),         invalid_argument_error);
    BOOST_CHECK_THROW(Parse::PointSpecial("-x=5 -y=6"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::PointSubopts("5, 6"),      invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Coordinate(""),            invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Coordinate("1"),           invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Coordinate("1.1"),         invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Coordinate("1 2 3"),       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Coordinate("-x=1 3"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Coordinate("1 -y 3"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Coordinate("bla"),         invalid_argument_error);
    BOOST_CHECK_THROW(Parse::CoordinateSpecial("-x=5 -y=6"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::CoordinateSubopts("5, 6"),      invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Interval(" (-1.5,, 2.25)"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("((-1.5, 2.25))"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("]-1.5, 2.25)"),   invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("[aargh, 2.25)"),  invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("1, a"),           invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("(1"),             invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("[1"),             invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Interval("1,"),             invalid_argument_error);

//    BOOST_CHECK_THROW(Parse::IntervalSet("1 2 3"),                    invalid_argument_error); // is now read as (1, 2) [3, 3]
    BOOST_CHECK_THROW(Parse::IntervalSet("1,, 2"),                    invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet("((1 2))"),                  invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet("]1, 2)"),                   invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet("[a, 2)"),                   invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet(" (1, 2]  [3,4]  5 6  7 a"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet("(1"),                       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet("[1"),                       invalid_argument_error);
    BOOST_CHECK_THROW(Parse::IntervalSet("1,"),                       invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5)   -w=3 -h 4 -x 1 -y 2"), invalid_argument_error); // when center is used x and y must not be specified
    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5)   -w=3 -h 4 -x 1     "), invalid_argument_error); // when center is used x and y must not be specified
    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5)   -w=3 -h 4      -y 2"), invalid_argument_error); // when center is used x and y must not be specified
    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5)        -h 4          "), invalid_argument_error); // when center is used width and height must be specified
    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5)   -w=3               "), invalid_argument_error); // when center is used width and height must be specified
    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5)                      "), invalid_argument_error); // when center is used width and height must be specified
    BOOST_CHECK_THROW(Parse::Rectangle("-c (2)      -w=3 -h 4          "), invalid_argument_error); // center needs 2 numbers instead of 1
    BOOST_CHECK_THROW(Parse::Rectangle("-c(2 3.5 3) -w=3 -h 4          "), invalid_argument_error); // center needs 2 numbers instead of 3
    BOOST_CHECK_THROW(Parse::Rectangle("     -h 4 -x (1 2 3) -y 2      "), invalid_argument_error); // x must have 1 or 2 numbers
    BOOST_CHECK_THROW(Parse::Rectangle("-w=3      -x 1       -y (2 3 4)"), invalid_argument_error); // y must have 1 or 2 numbers
    BOOST_CHECK_THROW(Parse::Rectangle("-w=3 -h 4 -x (1 2)   -y 2      "), invalid_argument_error); // either width and 1 x number as start or no width and 2 x numbers
    BOOST_CHECK_THROW(Parse::Rectangle("     -h 4 -x 1       -y 2      "), invalid_argument_error); // either width and 1 x number as start or no width and 2 x numbers
    BOOST_CHECK_THROW(Parse::Rectangle("-w=3 -h 4 -x 1       -y (2 3)  "), invalid_argument_error); // either height and 1 y number as start or no height and 2 y numbers
    BOOST_CHECK_THROW(Parse::Rectangle("-w=3      -x 1       -y 2      "), invalid_argument_error); // either height and 1 y number as start or no height and 2 y numbers

    BOOST_CHECK_THROW(Parse::MRImage("                                                  -d 0  -t HIGH"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x2.tif        -t HIGH"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::MRImage("-f ../test_resources/images/formats/uint16x2.tif  -d 0         "), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::MRImage("-f not-existing-file                              -d 0  -t HIGH"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::MRImage("-fdt"),                                                            invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Image("-f"),                   invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Image("-f not-existing-file"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Image("not-existing-file"),    invalid_argument_error);

    BOOST_CHECK_THROW(Parse::Vector<int>("1 2.3 4"),      invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Vector<double>("1.1 2.3 a"), invalid_argument_error);
    BOOST_CHECK_THROW(Parse::Vector<Point>("-1,2"),       invalid_argument_error);
}


BOOST_AUTO_TEST_CASE(tokenizer)
{
    std::istringstream iss;
    ArgumentToken tok;
    iss.str("--image=\"-f 'test image.tif' --crop=(-x 1 -y 2 -w 3 -h 2)\"");
    iss >> tok;
    BOOST_CHECK(iss.eof() || !iss.fail());
    BOOST_CHECK_EQUAL(static_cast<std::string>(tok),
                      "--image=-f 'test image.tif' --crop=(-x 1 -y 2 -w 3 -h 2)");


    iss.str("-f 'test image.tif' --crop=(-x 1 -y 2 -w 3 -h 2)");
    iss.clear();
    iss >> tok;
    BOOST_CHECK(iss.eof() || !iss.fail());
    BOOST_CHECK_EQUAL(static_cast<std::string>(tok),
                      "-f");
    iss >> tok;
    BOOST_CHECK(iss.eof() || !iss.fail());
    BOOST_CHECK_EQUAL(static_cast<std::string>(tok),
                      "test image.tif");
    iss >> tok;
    BOOST_CHECK(iss.eof() || !iss.fail());
    BOOST_CHECK_EQUAL(static_cast<std::string>(tok),
                      "--crop=-x 1 -y 2 -w 3 -h 2");
}


BOOST_AUTO_TEST_CASE(tokenizer_sep)
{
    std::istringstream iss1("-x 1,-y 2 ,-w 3 ,-h 2 , -d 5");
    std::istringstream iss2("-x 1 -y 2  -w 3  -h 2   -d 5");
    ArgumentToken tok1;
    ArgumentToken tok2;
    tok1.sep = ",";
    while (iss1 >> tok1, iss2 >> tok2, iss1 && iss2)
        BOOST_CHECK_EQUAL(static_cast<std::string>(tok1), static_cast<std::string>(tok2));
    BOOST_CHECK(tok1.empty() == tok2.empty());
    if (!tok1.empty() && !tok2.empty())
        BOOST_CHECK_EQUAL(static_cast<std::string>(tok1), static_cast<std::string>(tok2));
}


// show, how to read custom types. Let's read <string, int> pairs, a map and a vector of <string, int> pairs
struct myelem : public std::pair<std::string, int> {
    using std::pair<std::string, int>::pair;
};
//using myelem = std::pair<std::string, int>; // does not work, since operator>> cannot be found, since it is defined in a different namespace (imagefusion::option) than myelem (std)
using mymap = std::map<myelem::first_type, myelem::second_type>;
using myvec = std::vector<myelem>;


std::istream& operator>>(std::istream &is, myelem &e) {
    // to respect quotation, we use an ArgumentToken to parse the string
    ArgumentToken tok;
    is >> tok;
    e.first = tok;
    is >> tok;
    e.second = imagefusion::option::Parse::Int(tok);
    return is;
}


struct MyParse : public Parse {
    static mymap mapE(std::string const& str);
    static mymap mapRaw(std::string const& str);
    static myelem elem(std::string const& str);
};

struct ArgCheckCustom : public ArgChecker {
    static ArgStatus MyMap(const Option& option) {
        if (option.name == "--map-e")
            MyParse::mapE(option.arg);
        else if (option.name == "--map-raw")
            MyParse::mapRaw(option.arg);
        return ArgStatus::OK;
    }

    static ArgStatus MyElem(const Option& option) {
        if (option.arg.empty())
            IF_THROW_EXCEPTION(invalid_argument_error("Element option requires an argument!"));

        MyParse::elem(option.arg);
        return ArgStatus::OK;
    }
};

std::vector<Descriptor> usageCustom = {
{ "ELEM",    "", "e", "",        ArgCheckCustom::MyElem,         "option description..."},
{ "MAPE",    "", "",  "map-e",   ArgCheckCustom::MyMap,          "option description..."},
{ "MAPRAW",  "", "",  "map-raw", ArgCheckCustom::MyMap,          "option description..."},
{ "VEC",     "", "v", "",        ArgCheckCustom::Vector<myelem>, "option description..."}
};

std::vector<Descriptor> usageMapE = {
{ "ELEM",    "", "e", "", ArgCheckCustom::MyElem,  "option description..."}
};

// parse single elem
myelem MyParse::elem(std::string const& str) {
    std::istringstream iss(str);
    myelem e;
    iss >> e;
    if (!iss.eof() && iss.fail())
        IF_THROW_EXCEPTION(invalid_argument_error("Could not read the element '" + std::string(str) + "'"));
    return e;
}

// parse map as "-e ('one' 1) -e ('two' 2) -e ('three' 3)"
mymap MyParse::mapE(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMapE);
    args.parse(argsTokens);
    mymap m;
    for (auto const& opt : args["ELEM"])
        m.insert(elem(opt.arg));
    return m;
}

// parse map as "('one' 1) ('two' 2) ('three' 3)"
mymap MyParse::mapRaw(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    mymap m;
    for (std::string const& arg : argsTokens)
        m.insert(elem(arg));
    return m;
}


BOOST_AUTO_TEST_CASE(custom_type)
{
    std::string arguments = " -e ('test 4' 1)"
                            " --map-raw=(('one' 1) ('two' 2) ('three' 3))"
                            " -e(works 42)"
                            " --map-e=(-e ('four' 4) -e('five' 5) -e('six' 6))"
                            " -e'yay 101'"
                            " -v (('seven' 7) ('eight' 8) ('nine' 9))"
                            " -e('woohoo' 001)";
    OptionParser options(usageCustom);
    options.parse(arguments);
    BOOST_CHECK_EQUAL(options["ELEM"].size(),   4);
    BOOST_CHECK_EQUAL(options["MAPE"].size(),   1);
    BOOST_CHECK_EQUAL(options["MAPRAW"].size(), 1);
    BOOST_CHECK_EQUAL(options["VEC"].size(),    1);

    auto elem_list = {myelem{"test 4", 1}, myelem{"works", 42}, myelem{"yay", 101}, myelem{"woohoo", 1}};
    auto it = elem_list.begin();
    for (auto const& opt : options["ELEM"]) {
        myelem e = MyParse::elem(opt.arg);
        BOOST_CHECK_EQUAL(e.first, it->first);
        BOOST_CHECK_EQUAL(e.second, it->second);
//        std::cout << e.first << ": " << e.second << std::endl;
        if (++it == elem_list.end())
            break;
    }

    if (!options["MAPRAW"].empty()) {
        mymap m = MyParse::mapRaw(options["MAPRAW"].back().arg);
        BOOST_CHECK_EQUAL(m["one"],   1);
        BOOST_CHECK_EQUAL(m["two"],   2);
        BOOST_CHECK_EQUAL(m["three"], 3);
    }

    if (!options["MAPE"].empty()) {
        mymap m = MyParse::mapE(options["MAPE"].back().arg);
        BOOST_CHECK_EQUAL(m["four"], 4);
        BOOST_CHECK_EQUAL(m["five"], 5);
        BOOST_CHECK_EQUAL(m["six"],  6);
    }

    if (!options["VEC"].empty()) {
        auto v = MyParse::Vector<myelem>(options["VEC"].back().arg);
        BOOST_CHECK_EQUAL(v[0].first,  "seven");
        BOOST_CHECK_EQUAL(v[1].first,  "eight");
        BOOST_CHECK_EQUAL(v[2].first,  "nine");
        BOOST_CHECK_EQUAL(v[0].second, 7);
        BOOST_CHECK_EQUAL(v[1].second, 8);
        BOOST_CHECK_EQUAL(v[2].second, 9);
    }

    // bad tests
    BOOST_CHECK_THROW(MyParse::elem(""),                                            invalid_argument_error);
    BOOST_CHECK_THROW(MyParse::elem("()"),                                          invalid_argument_error);
    BOOST_CHECK_THROW(MyParse::elem("'string'"),                                    invalid_argument_error);
    BOOST_CHECK_THROW(MyParse::elem("'string' 'no int'"),                           invalid_argument_error);
    BOOST_CHECK_THROW(MyParse::elem("string with spaces 1"),                        invalid_argument_error);
    BOOST_CHECK_THROW(MyParse::elem("string 2.54"),                                 invalid_argument_error);
    BOOST_CHECK_THROW(MyParse::elem("('seven' 7) ('eight' 8) ('almost ten' 9.99)"), invalid_argument_error);
}


BOOST_AUTO_TEST_CASE(default_arguments)
{
    std::string default_args = "-n 10 -b";
    std::string args1 = "-n 1 -abn 3";
    OptionParser options1(usageFirst);
    BOOST_CHECK_NO_THROW(options1.parse(default_args).parse(args1));
    BOOST_CHECK_EQUAL(options1["A"].size(), 1);
    BOOST_CHECK_EQUAL(options1["B"].size(), 2);
    BOOST_CHECK_EQUAL(options1["N"].size(), 3);
    BOOST_CHECK_EQUAL(Parse::Int(options1["N"].front().arg), 10);
    BOOST_CHECK_EQUAL(Parse::Int(options1["N"].back().arg),  3);

    OptionParser options2(usageFirst);
    BOOST_CHECK_NO_THROW(options2.parse(default_args).parse(""));
    BOOST_CHECK_EQUAL(options2["B"].size(), 1);
    BOOST_CHECK_EQUAL(options2["N"].size(), 1);
    BOOST_CHECK_EQUAL(Parse::Int(options2["N"].back().arg), 10);

    BOOST_CHECK_NO_THROW(options2.parse("--option-file='../test_resources/other_resources/good_opts2'"));
//    BOOST_CHECK_EQUAL(options2["RECT"].size(), 1);
//    BOOST_CHECK_EQUAL(options2["N"].size(), 1);
//    BOOST_CHECK_EQUAL(options2.optionCount(), 2);
//    BOOST_CHECK_EQUAL(options2.nonOptionArgCount(), 0);
//    BOOST_CHECK_EQUAL(Parse::Int(options2["N"].arg), 5);
}

BOOST_AUTO_TEST_CASE(option_file)
{
    // parse option file with a lot of whitespace at end of file
    OptionParser options(usageFirst);
    options.parse("--option-file='../test_resources/other_resources/good_opts2'");
    BOOST_CHECK_EQUAL(options["RECT"].size(), 1);
    BOOST_CHECK_EQUAL(options["N"].size(), 1);
    BOOST_CHECK_EQUAL(options.optionCount(), 2);
    BOOST_CHECK_EQUAL(options.nonOptionArgCount(), 0);
    BOOST_CHECK_EQUAL(Parse::Int(options["N"].back().arg), 5);

    // parse option file with no newline at end of file
    options.clear();
    options.parse("--option-file='../test_resources/other_resources/good_opts1'");
    BOOST_CHECK_EQUAL(options["RECT"].size(), 1);
    BOOST_CHECK_EQUAL(options["N"].size(),    3);
    BOOST_CHECK_EQUAL(options["SIZE"].size(), 1);
    BOOST_CHECK_EQUAL(options["IMG"].size(),  1);
    BOOST_CHECK_EQUAL(options.optionCount(), 6);
    BOOST_CHECK_EQUAL(options.nonOptionArgCount(), 0);

    BOOST_CHECK_EQUAL(Parse::Int(options["N"][0].arg), 1);
    BOOST_CHECK_EQUAL(Parse::Int(options["N"][1].arg), 2);
    BOOST_CHECK_EQUAL(Parse::Int(options["N"][2].arg), 5);

    Rectangle r = Parse::Rectangle(options["RECT"].back().arg);
    BOOST_CHECK_EQUAL(r.x,      4);
    BOOST_CHECK_EQUAL(r.y,      6);
    BOOST_CHECK_EQUAL(r.width,  1);
    BOOST_CHECK_EQUAL(r.height, 2);

    ImageInput ii = Parse::MRImage(options["IMG"].back().arg);
    BOOST_CHECK_EQUAL(ii.i.width(),  324);
    BOOST_CHECK_EQUAL(ii.i.height(), 324);
    BOOST_CHECK_EQUAL(ii.date,       0);
    BOOST_CHECK_EQUAL(ii.tag,        "hey");

    // parse file with '#' and no newline at the end of file
    options.clear();
    options.optFileOptName = "--conf";
    options.parse("--conf='../test_resources/other_resources/good_opts3'");
    BOOST_CHECK_EQUAL(options["N"].size(), 1);
    BOOST_CHECK_EQUAL(Parse::Int(options["N"].back().arg), 2);
}



enum class MathOps { UNKNOWN, PLUS, MINUS, MUL };

int eval(MathOps op, std::string const& arg) {
    std::vector<std::string> s = separateArguments(arg);
    BOOST_REQUIRE_EQUAL(s.size(), 2);
    std::array<int,2> i;
    for (unsigned int j = 0; j < i.size(); ++j) {
        int len = s[j].size();
        if (len > 4) { // && s[j][0] == '-' && s[j][1] == '-') {
            if (s[j][2] == 'p')
                i[j] = eval(MathOps::PLUS,  s[j].substr(7));
            else if(s[j][3] == 'i')
                i[j] = eval(MathOps::MINUS, s[j].substr(8));
            else
                i[j] = eval(MathOps::MUL,   s[j].substr(6));
        }
        else
            i[j] = std::stoi(s[j]);
    }

    if (op == MathOps::PLUS)
        return i[0] + i[1];
    if (op == MathOps::MINUS)
        return i[0] - i[1];
//  if (op == MathOps::MUL)
    return i[0] * i[1];
}

int eval(std::string const& s) {
    int len = s.size();
    if (len > 4) {
        if (s[2] == 'p')
            return eval(MathOps::PLUS,  s.substr(8, len-1-8));
        else if(s[3] == 'i')
            return eval(MathOps::MINUS, s.substr(9, len-1-9));
        else
            return eval(MathOps::MUL,   s.substr(7, len-1-7));
    }
    return 0;
}

BOOST_AUTO_TEST_CASE(nested_quoting_levels)
{
    // arguments represent 1 + (((2 + 1) * (3 - (2 - 1))) + 3)
    //                     1 + ((   3    * (3 -    1   )) + 3)
    //                     1 + ((   3    *    2         ) + 3)
    //                     1 + (         6                + 3)
    //                     1 +                            9
    //                      10
    // plus(1, plus(mul(plus(2, 1), minus(3, minus(2, 1))), 3))
    std::string parens = "--plus=(1 --plus=(--mul=(--plus=(2 1) --minus=(3 --minus=(2 1))) 3))";
//    std::cout << "Only parens: " << parens << std::endl;
    BOOST_CHECK_EQUAL(eval(parens), 10);

    std::string mixed = "--plus=(1 --plus='--mul=(--plus=\"2 1\" --minus=\"3 --minus=(2 1)\") 3')";
//    std::cout << "Mixed parens and quotes: " << mixed << std::endl;
    BOOST_CHECK_EQUAL(eval(mixed),  10);
}



void check_sep_lvl(std::string const& ls, int lvl = 0) {
//    std::cout << "lvl: " << lvl << ": " << ls << std::endl;
    std::vector<std::string> vec = separateArguments(ls);
    for (std::string const& s : vec) {
//        for (int i = 0; i < lvl+1; ++i)
//            std::cout << "\t";
//        std::cout << s << " ";
        if (s.length() == 1) {
            if (std::isdigit(s.front())) {
                BOOST_CHECK_EQUAL(std::stoi(s), lvl);
//                std::cout << "verified." << std::endl;
            }
            else {
                // check that escape char has been swallowed, so \" --> " and \' --> ' in level 0
                BOOST_CHECK(s.front() == '\"' || s.front() == '\'');
//                std::cout << "wasted." << std::endl;
            }
        }
        else {
//            std::cout << " splitting..." << std::endl;
            check_sep_lvl(s, lvl + 1);
        }
    }
}

BOOST_AUTO_TEST_CASE(nested_quoting_levels_and_escaping)
{
    std::string simple = "0 0 '1 \" 2 ( 3 ) 2 \" 1' 0";
    //     <==>           0 0 (1  ( 2 ( 3 ) 2  ) 1) 0
    check_sep_lvl(simple);

    std::string multiple_same_levels = "0 0 '1 \" 2 2 \" 1 \" 2 2 \" 1' 0 '1 \" 2 2 \" 1 \" 2 2 \" 1' 0";
    //     <==>                         0 0 (1  ( 2 2  ) 1  ( 2 2  ) 1) 0 (1  ( 2 2  ) 1  ( 2 2  ) 1) 0
    check_sep_lvl(multiple_same_levels);

    std::string complex_escapes = "0 0 \\' '1 \" 2 \\' \\\" 2 \"' 0";
    //     <==>                    0 0  \' (1  ( 2  \'   \" 2  )) 0
    check_sep_lvl(complex_escapes);
}

// verify that when quoting ends within a char sequence, it breaks the token
BOOST_AUTO_TEST_CASE(quoting_as_separator)
{
    std::string s1 = " abc='def'ghi  ";
    std::string s2 = " abc='def' ghi ";
    auto tokens1 = separateArguments(s1);
    auto tokens2 = separateArguments(s2);
    BOOST_CHECK_EQUAL_COLLECTIONS(tokens1.begin(), tokens1.end(), tokens2.begin(), tokens2.end());
}

// verify that the first quoting leaves the inner quoting chars as they are
// these can be used as usual characters
BOOST_AUTO_TEST_CASE(quoting_symbols_as_chars)
{
    std::string arg = "--opt=(13d10' 0.94\"E, 53d53'39.37\"N) --opt=(13d19' 5.80\"E, 53d48'32.79\"N)";
    auto token = separateArguments(arg);
//    for (std::string const& t : token)
//        std::cout << "<" << t << ">" << std::endl;
    BOOST_CHECK_EQUAL(token.size(), 2);
    BOOST_REQUIRE(!token.empty());
    BOOST_CHECK_EQUAL(token.front(), "--opt=13d10' 0.94\"E, 53d53'39.37\"N");
    BOOST_CHECK_EQUAL(token.back(),  "--opt=13d19' 5.80\"E, 53d48'32.79\"N");
}

// check whether copy assignment exists for Descriptor
BOOST_AUTO_TEST_CASE(copy_function)
{
    Descriptor d;
    d = Parse::usageImage.front(); // should not give a compile error
}

//enum class OptsErr {UNKNOWN, IMG};
//const Descriptor<OptsErr> usageErr[] = {
//    Descriptor<OptsErr>::start("bla"),
//    { OptsErr::IMG,     "", "i", "image", ArgChecker::Image,   "  -i <img>, \t--image=<img> \tplain image"},
//    Descriptor<OptsErr>::optfile()
//};

//BOOST_AUTO_TEST_CASE(error_and_usage_text)
//{
//    try {
//        OptionParser options(usageFirst);
//        options.parse(" --rectangle=(-x 1 -y 2 -w=3 -h)");
////        options.parse(" -i (-ftd)");
////        options.parse(" -i (-f '../test_resources/images/formats/uint16x2.tif' -t HIGH --crop (-x 1 -y 2 -w 4 -h 2) -l 1)");
////        options.parse(" --point=(-t -5)");
////        std::cout << Parse::Point(options["POINT"].back().arg) << std::endl;

//        OptionParser options(usageErr);
////        options.parse(" -i (-f '../test_resources/images/formats/uint16x2.tif' --crop (-x 1 -y 2 -w 4 -h 2) -l 1)");
////        options.parse(" -i (-f)");
//    }
//    catch(runtime_error& e) {
//        std::cout << diagnostic_information(e) << std::endl;
//    printUsage(std::cout, usageFirst);
//    }
//    printUsage(usageErr);
//}


std::vector<Descriptor> usageUnknown = {
    { "NUM", "", "n", "number", ArgChecker::Int,      "  -n <num>, \t--number=<num> \tjust a number"},
    { "BLA", "", "b", "numbla", ArgChecker::Optional, "  -b <num>, \t--numbla=<num> \tjust a number"},
    { "BLA", "", "",  "b",      ArgChecker::Optional, ""},
    Descriptor::optfile()
};


BOOST_AUTO_TEST_CASE(get_unknown_options_with_arg)
{
    // test that...
    // * unknown options are collected
    // * --no-argument is not taken as argument for --arbitrary, since it is detached and starts with a double dash
    // * -4 is not taken as argument for --no-argument, since it is detached, starts with a single dash and --no-argument is not a valid short option
    // * -14 is not taken as argument for --b, since it is detached, starts with a single dash and --b is not a valid short option
    // * -25 is taken as argument for -b, since it is a short option
    // * short unknown options eat up the next token (desired for unknown options?)
    OptionParser options(usageUnknown);
    options.unknownOptionArgCheck = ArgChecker::Optional;
    options.parse("--test=--5  --arbitrary  --no-argument -4  -A--B  -C  --D -E ''  -F  -G -last --b -14 -b -25");
//    for (auto const& o : options.unknown)
//        std::cout << "name: <" << o.name << ">, arg: <" << o.arg << ">" << std::endl;
    BOOST_REQUIRE_EQUAL(options.unknown.size(), 10);
    BOOST_REQUIRE_EQUAL(options["BLA"].size(), 2);
    BOOST_CHECK_EQUAL(options.unknown[0].name,  "test");
    BOOST_CHECK_EQUAL(options.unknown[0].arg,   "--5");
    BOOST_CHECK_EQUAL(options.unknown[1].name,  "arbitrary");
    BOOST_CHECK_EQUAL(options.unknown[1].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[2].name,  "no-argument");
    BOOST_CHECK_EQUAL(options.unknown[2].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[3].name,  "4");
    BOOST_CHECK_EQUAL(options.unknown[3].arg,   "-A--B");
    BOOST_CHECK_EQUAL(options.unknown[4].name,  "C");
    BOOST_CHECK_EQUAL(options.unknown[4].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[5].name,  "D");
    BOOST_CHECK_EQUAL(options.unknown[5].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[6].name,  "E");
    BOOST_CHECK_EQUAL(options.unknown[6].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[7].name,  "F");
    BOOST_CHECK_EQUAL(options.unknown[7].arg,   "-G");
    BOOST_CHECK_EQUAL(options.unknown[8].name,  "l");
    BOOST_CHECK_EQUAL(options.unknown[8].arg,   "ast");
    BOOST_CHECK_EQUAL(options.unknown[9].name,  "1");
    BOOST_CHECK_EQUAL(options.unknown[9].arg,   "4");
    BOOST_CHECK_EQUAL(options["BLA"][0].name,  "b");
    BOOST_CHECK_EQUAL(options["BLA"][0].arg,   "");
    BOOST_CHECK_EQUAL(options["BLA"][1].name,  "b");
    BOOST_CHECK_EQUAL(options["BLA"][1].arg,   "-25");
    BOOST_CHECK_EQUAL(options.nonOptionArgs.size(), 0);
}

BOOST_AUTO_TEST_CASE(get_unknown_options_with_arg_and_single_dash_long_options)
{
    // test that...
    // * unknown options are collected
    // * --no-argument is not taken as argument for --arbitrary, since it is detached and starts with a double dash
    // * -4 is not taken as argument for --no-argument, since it is detached, starts with a single dash and --no-argument is not a valid short option
    // * -14 is not taken as argument for --b, since it is detached, starts with a single dash and --b is not a valid short option
    // * -25 is taken as argument for -b, since it is a short option
    // * short unknown options eat up the next token (desired for unknown options?)
    OptionParser options(usageUnknown);
    options.unknownOptionArgCheck = ArgChecker::Optional;
    options.singleDashLongopt = true;
    options.parse("--test=--5  --arbitrary  --no-argument -4  -A--B  -last --b -14 -b -25");
//    for (auto const& o : options.unknown)
//        std::cout << "name: <" << o.name << ">, arg: <" << o.arg << ">" << std::endl;
    BOOST_REQUIRE_EQUAL(options.unknown.size(), 6);
    BOOST_REQUIRE_EQUAL(options["BLA"].size(), 2);
    BOOST_CHECK_EQUAL(options.unknown[0].name,  "test");
    BOOST_CHECK_EQUAL(options.unknown[0].arg,   "--5");
    BOOST_CHECK_EQUAL(options.unknown[1].name,  "arbitrary");
    BOOST_CHECK_EQUAL(options.unknown[1].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[2].name,  "no-argument");
    BOOST_CHECK_EQUAL(options.unknown[2].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[3].name,  "4");
    BOOST_CHECK_EQUAL(options.unknown[3].arg,   "-A--B");
    BOOST_CHECK_EQUAL(options.unknown[4].name,  "l");
    BOOST_CHECK_EQUAL(options.unknown[4].arg,   "ast");
    BOOST_CHECK_EQUAL(options.unknown[5].name,  "1");
    BOOST_CHECK_EQUAL(options.unknown[5].arg,   "4");
    BOOST_CHECK_EQUAL(options["BLA"][0].name,  "b");
    BOOST_CHECK_EQUAL(options["BLA"][0].arg,   "");
    BOOST_CHECK_EQUAL(options["BLA"][1].name,  "b");
    BOOST_CHECK_EQUAL(options["BLA"][1].arg,   "-25");
    BOOST_CHECK_EQUAL(options.nonOptionArgs.size(), 0);
}

BOOST_AUTO_TEST_CASE(get_unknown_options_without_arg)
{
    OptionParser options(usageUnknown);
    options.unknownOptionArgCheck = ArgChecker::None;
    options.parse("--test=--5 --arbitrary --no-argument -4 -A--B -C --D -E -F -last");
//    for (auto const& o : options.unknown)
//        std::cout << "name: <" << o.name << ">, arg: <" << o.arg << ">" << std::endl;
    BOOST_REQUIRE_EQUAL(options.unknown.size(), 13);
    BOOST_CHECK_EQUAL(options.unknown[0].name,  "test");
    BOOST_CHECK_EQUAL(options.unknown[0].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[1].name,  "arbitrary");
    BOOST_CHECK_EQUAL(options.unknown[1].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[2].name,  "no-argument");
    BOOST_CHECK_EQUAL(options.unknown[2].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[3].name,  "4");
    BOOST_CHECK_EQUAL(options.unknown[3].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[4].name,  "A");
    BOOST_CHECK_EQUAL(options.unknown[4].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[5].name,  "C");
    BOOST_CHECK_EQUAL(options.unknown[5].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[6].name,  "D");
    BOOST_CHECK_EQUAL(options.unknown[6].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[7].name,  "E");
    BOOST_CHECK_EQUAL(options.unknown[7].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[8].name,  "F");
    BOOST_CHECK_EQUAL(options.unknown[8].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[9].name,  "l");
    BOOST_CHECK_EQUAL(options.unknown[9].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[10].name,  "a");
    BOOST_CHECK_EQUAL(options.unknown[10].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[11].name,  "s");
    BOOST_CHECK_EQUAL(options.unknown[11].arg,   "");
    BOOST_CHECK_EQUAL(options.unknown[12].name,  "t");
    BOOST_CHECK_EQUAL(options.unknown[12].arg,   "");
    BOOST_CHECK_EQUAL(options.nonOptionArgs.size(), 0);
}


BOOST_AUTO_TEST_CASE(options_after_non_options)
{
    // parse with acceptsOptAfterNonOpts = true
    OptionParser options(usageUnknown);
    options.unknownOptionArgCheck = ArgChecker::Optional;
    options.acceptsOptAfterNonOpts = true;
    //             option argument option arg no-option    option arg nooption        option      stop nooption
    options.parse("--test argument --bam  ''  a-non-option --bomm=    next-non-option --arbitrary --   --also-non-option");
    BOOST_CHECK_EQUAL(options.unknown.size(), 4);
    BOOST_CHECK_EQUAL(options.unknown[0].name, "test");
    BOOST_CHECK_EQUAL(options.unknown[0].arg,  "argument");
    BOOST_CHECK_EQUAL(options.unknown[1].name, "bam");
    BOOST_CHECK_EQUAL(options.unknown[1].arg,  "");
    BOOST_CHECK_EQUAL(options.unknown[2].name, "bomm");
    BOOST_CHECK_EQUAL(options.unknown[2].arg,  "");
    BOOST_CHECK_EQUAL(options.unknown[3].name, "arbitrary");
    BOOST_CHECK_EQUAL(options.unknown[3].arg,  "");
    BOOST_CHECK_EQUAL(options.nonOptionArgs.size(), 3);
    BOOST_CHECK_EQUAL(options.nonOptionArgs[0], "a-non-option");
    BOOST_CHECK_EQUAL(options.nonOptionArgs[1], "next-non-option");
    BOOST_CHECK_EQUAL(options.nonOptionArgs[2], "--also-non-option");
}


BOOST_AUTO_TEST_CASE(abbreviated_options)
{
    // parse with minAbbrevLen = 3, show ambiguity leads to unknown option
    OptionParser options(usageUnknown);
    options.unknownOptionArgCheck = ArgChecker::Optional;
    options.minAbbrevLen = 3;
    options.parse("--numbe=1 --numbl=2 --num=3");
    BOOST_CHECK_EQUAL(options["NUM"].size(), 1);
    BOOST_CHECK_EQUAL(options["NUM"].front().name, "numbe");
    BOOST_CHECK_EQUAL(options["NUM"].front().arg,  "1");
    BOOST_CHECK_EQUAL(options["BLA"].size(),       1);
    BOOST_CHECK_EQUAL(options["BLA"].front().name, "numbl");
    BOOST_CHECK_EQUAL(options["BLA"].front().arg,  "2");
    BOOST_CHECK_EQUAL(options.unknown.size(),       1);
    BOOST_CHECK_EQUAL(options.unknown.front().name, "num");
    BOOST_CHECK_EQUAL(options.unknown.front().arg,  "3");
}

BOOST_AUTO_TEST_CASE(do_not_expand_option_file)
{
    // parse with expandOptionFiles = false
    OptionParser options(usageUnknown);
    options.unknownOptionArgCheck = ArgChecker::Optional;
    options.expandOptionsFiles = false;
    options.parse("--option-file=not-existing");
    BOOST_CHECK_EQUAL(options.unknown.size(),       1);
    BOOST_CHECK_EQUAL(options.unknown.front().name, "option-file");
    BOOST_CHECK_EQUAL(options.unknown.front().arg,  "not-existing");
}


BOOST_AUTO_TEST_CASE(streq_test)
{
    using imagefusion::option_impl_detail::streq;
    BOOST_CHECK( streq("foo",     "foo=bar"));
    BOOST_CHECK(!streq("foo",     "foobar"));
    BOOST_CHECK( streq("foo",     "foo"));
    BOOST_CHECK(!streq("foo=bar", "foo"));
}


BOOST_AUTO_TEST_CASE(streq_with_abbreviations_test)
{
    using imagefusion::option_impl_detail::streq;
    BOOST_CHECK( streq("foo", "foo=bar", /*anything*/ 1));
    BOOST_CHECK( streq("foo", "fo=bar",  2));
    BOOST_CHECK( streq("foo", "fo",      2));
    BOOST_CHECK(!streq("foo", "fo",      0));
    BOOST_CHECK(!streq("foo", "f=bar",   2));
    BOOST_CHECK(!streq("foo", "f",       2));
    BOOST_CHECK(!streq("fo",  "foo=bar", /*anything*/ 1));
    BOOST_CHECK(!streq("foo", "foobar",  /*anything*/ 1));
    BOOST_CHECK(!streq("foo", "fobar",   /*anything*/ 1));
    BOOST_CHECK( streq("foo", "foo",     /*anything*/ 1));
}

// build this test case only in Boost 1.59+, since from there on it is possible to disable a test case by default
// we do not want to run these tests by default, since they print a lot of stuff
// run with --run_test=optionparser_suite/usage_text_test
#if BOOST_VERSION / 100 % 1000 > 58 //|| 1


std::vector<Descriptor> test_not_last_column_break = {
    Descriptor::text("first cell  \there the second cell is really really long and will be indented at the second cell start."),
    Descriptor::text(),
    Descriptor::text("This line would be not be indented, if it were too long and had to be broken... uups! ;-)"),
    Descriptor::text("also first cell  \tsecond cell  \tthird cell, which is way too long to be printed in a single line.")
};

std::vector<Descriptor> test_vtabs = {
    Descriptor::text("Cölüümn 1 line ı is long  \vColumn 1 line 2  \vColumn 1 line 3  \t\vColumn 2 line 2  \tColumn 3 line 1  \v \vColumn 3 line 3")
};

std::vector<Descriptor> test_columns = {
    Descriptor::text("Column 1 line 1  \t\tColumn 3 line 1\n"
                          "Column 1 line 2  \tColumn 2 line 2   \tColumn 3 line 2\n"
                          "Column 1 line 3  \t\tColumn 3 line 3")
};

std::vector<Descriptor> sub_table = {
    Descriptor::text("  -o <bool>, --opt=<bool>  \tIf you give <bool> the value...\n"
                         "\t o \ttrue and have...\v"
                         "* specified a filename, it will do this.\v"
                         "* not specified a filename, it will do that.\n"
                         "\t o \tfalse it will just exit.")
};

std::vector<Descriptor> test_column1 = {
    Descriptor::text("11 \t21\v22\v23\t 31\nxx")
};


std::vector<Descriptor> test_tables = {
    Descriptor::breakTable(),
    Descriptor::breakTable(),
    Descriptor::text("Each table has its own column widths and is not aligned with other tables."),
    Descriptor::text("Table 1 Column 1 Line 1 \tTable 1 Column 2 Line 1 \tTable 1 Column 3 Line 1\n"
                          "Table 1 Col 1 Line 2 \tTable 1 Col 2 Line 2 \tTable 1 Col 3 Line 2"
                          ),
    Descriptor::text("Table 1 Col 1 Line 3 \tTable 1 Col 2 Line 3 \tTable 1 Column 3 Line 3\n"
                          "Table 1 Col 1 Line 4 \tTable 1 Column 2 Line 4 \tTable 1 Column 3 Line 4"
                          ),
    Descriptor::breakTable(),
    Descriptor::breakTable(),
    Descriptor::text( "This is the only line of table 2."),
    Descriptor::breakTable(),
    Descriptor::text( "This is the very long 1st line of table 3. It is more than 80 characters in length and therefore needs to be wrapped. In fact it is so long that it needs to be wrapped multiple times to fit into a normal 80 characters terminal.\v"
                           "This is the very long 2nd line of table 3. It is more than 80 characters in length and therefore needs to be wrapped. In fact it is so long that it needs to be wrapped multiple times to fit into a normal 80 characters terminal.\v"
                           "This is a reasonably sized line 3 of table 3."
                         ),
    Descriptor::breakTable(),
    Descriptor::text("Table 4:\n"
                          "  \tTable 4 C 2 L 1 \tTable 4 C 3 L 1 \tTable 4 C 4 L 1\n"
                            "\tTable 4 C 2 L 2 \tTable 4 C 3 L 2 \tTable 4 C 4 L 2"
                          ),
    Descriptor::breakTable(),
    Descriptor::text("This is the only line of table 5"),
    Descriptor::breakTable(),
    Descriptor::text("Table 6 C 1 L 1 \tTable 6 C 2 L 1 \tTable 6 C 3 L 1\n"
                          "Table 6 C 1 L 2 \tTable 6 C 2 L 2 \tTable 6 C 3 L 2"
                         ),
    Descriptor::breakTable(),
    Descriptor::text("Table 7 Column 1 Line 1 \tTable 7 Column 2 Line 1 \tTable 7 Column 3 Line 1\n"
                          "Table 7 Column 1 Line 2 \tTable 7 Column 2 Line 2 \tTable 7 Column 3 Line 2\n"
                         )
};

std::vector<Descriptor> test_nohelp = {
    Descriptor::text(),
    Descriptor::text(),
    Descriptor::text()
};

std::vector<Descriptor> test_wide = {
    Descriptor::text("111\v112\v113\v114\v115 \t"
                                 "121\v122\v123\v124 \t"
                                 "131\v132\v133 \t"
                                 "141\v142 \t"
                                 "151"
   ),
    Descriptor::text("211 \t221 \t231 \t241 \t251\n"
                                 "212 \t222 \t232 \t242\n"
                                 "213 \t223 \t233\n"
                                 "214 \t224\n"
                                 "215"
   )
};

std::vector<Descriptor> test_overlong = {
    Descriptor::text("Good \t| \tGood \t| \tThis is good."),
    Descriptor::text("Good \t| \tThis is an overlong cell asfd. \t| \tThis is good."),
    Descriptor::text("Good \t| \tGood \t| \tThis is good.")
};

std::vector<Descriptor> test_toomanycolumns = {
    Descriptor::text("This \ttable \thas \ttoo \tmany \tcolumns. \tThe \tlast \tcolumns \tare \tdiscarded."),
    Descriptor::text("1\t2\t3\t4\t5\t6\t7\t8\t9\t10\t11")
};

std::vector<Descriptor> test_ownline = {
    Descriptor::text("1234567890AB\vBA0987654321\tStarts on its own line and is indented somewhat.\vThis one, too.")
};

std::vector<Descriptor> test_inline_break = {
    Descriptor::text("long cell \t| another cell"),
    Descriptor::text("even longer cell \t| cell\f"),
    Descriptor::breakTable(),
    Descriptor::text("short cell \t| another cell"),
    Descriptor::text("cell \t| cell")
};

std::vector<Descriptor> test_consecutive_breaks = {
    Descriptor::text("first table\f"),
    Descriptor::text("second table\f"),
    Descriptor::text("third table\f")
};

std::vector<Descriptor> test_skip_at_break = {
    Descriptor::text("skip the rest of this line\fSKIPPED CONTENT"),
    Descriptor::text("second table without break"),
    Descriptor::text("third table with skipped content after here\f THIS IS SKIPPED"),
    Descriptor::text("last table without break")
};

std::vector<Descriptor> vertical_column_bug = {
    Descriptor::text("first \tsecond\v"
                     "next row\v"
                     "This last row should not set the column width.\n"), // fixed
    Descriptor::text("first column -> \tsecond column -> \tthird column")
};


void stdout_write(const char* str, int size)
{
    fwrite(str, size, 1, stdout);
}

// deactivate warnings of unused results of ::write
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
struct stdout_writer
{
    void write(const char* buf, size_t size) const
    {
        ::write(1, buf, size);
    }
};

struct stdout_write_functor
{
    void operator()(const char* buf, size_t size)
    {
        ::write(1, buf, size);
    }
};
#pragma GCC diagnostic pop

#if BOOST_VERSION / 100 % 1000 > 58
// usage text test outputs, disabled by default, run with --run_test=optionparser_suite/usage_text_test
BOOST_AUTO_TEST_CASE(usage_text_test, * boost::unit_test::disabled())
#else /* Boost version <= 1.58 */
BOOST_AUTO_TEST_CASE(usage_text_test)
#endif
{
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(test_not_last_column_break, 63);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_write, test_vtabs);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_writer(), test_columns);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(write, 1, test_column1);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(sub_table);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(std::cout, test_tables);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(fwrite, stdout, test_nohelp);
    std::cout << "---------------------------------------------------------------" << std::endl;
    std::ostringstream sst;
    printUsage(sst, test_wide, 80);
    std::cout << sst.str();
    std::cout << "---------------------------------------------------------------" << std::endl;
    stdout_write_functor stdout_write_f;
    printUsage(&stdout_write_f, test_overlong, 30);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_write, test_toomanycolumns);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_write, test_ownline, 20);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_write, test_inline_break, 80);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_write, test_consecutive_breaks, 80);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(stdout_write, test_skip_at_break, 80);
    std::cout << "---------------------------------------------------------------" << std::endl;
    printUsage(vertical_column_bug, 140);
    std::cout << "---------------------------------------------------------------" << std::endl;
}
#endif /* Boost version > 1.58 */

BOOST_AUTO_TEST_SUITE_END()

} /* namespace option */
} /* namespace imagefusion */


#include "optionparserimpl.h"
#include "optionparser.h"
#include "imagefusion.h"
#include "geoinfo.h"

#include <boost/tokenizer.hpp>
#include "../../include/filesystem.h"
#include <boost/predef.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>

namespace imagefusion {
namespace option {

int Parse::Int(std::string const& str, std::string const& optName) {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    if (str.find('.') != std::string::npos)
        IF_THROW_EXCEPTION(invalid_argument_error("Found a '.' in '" + str + "' instead of just an integer" + oN));

    try {
        return std::stoi(str);
    }
    catch (std::invalid_argument&) {
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as integer" + oN));
    }
}

double Parse::Float(std::string const& str, std::string const& optName) {
    try {
        return std::stod(str);
    }
    catch (std::invalid_argument&) {
        std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as double" + oN));
    }
}


namespace {

struct AngleWrapper {
    double angle;
};

std::stringstream& operator>>(std::stringstream& in, AngleWrapper& a) {
    // first part must be a number
    double& angle = a.angle;
    if (!(in >> angle))
        IF_THROW_EXCEPTION(invalid_argument_error("The angle has to start with a number, got '" + in.str() + "' when parsing an angle"));

    char c;
    // either finished, degree symbol or "rad"
    if (!(in >> c)) {
        // finished
        in.clear();
        return in;
    }

    if (c == 'd') {
        // degree symbol "d" or "deg"
        // now either finished, "(d)eg" or number
        if (!(in >> c)) {
            // finished
            in.clear();
            return in;
        }

        if (c == 'e') {
            // "deg", skip 'g'
            if (!(in >> c) || c != 'g')
                IF_THROW_EXCEPTION(invalid_argument_error("Expected 'deg' after '" + std::to_string(angle) + "' in '" + in.str() + "' when parsing an angle"));
        }
        else {
            // number
            if (!std::isdigit(c))
                IF_THROW_EXCEPTION(invalid_argument_error("Expected arc minute after 'd' in '" + in.str() + "' when parsing an angle"));
            in.putback(c);
        }
    }
    else if (c == 'r') {
        // "rad", skip 'a' and 'd'
        if (!(in >> c) || c != 'a' || !(in >> c) || c != 'd')
            IF_THROW_EXCEPTION(invalid_argument_error("Expected 'rad' after '" + std::to_string(angle) + "' in '" + in.str() + "' when parsing an angle"));
        // now should be finished
        if (in >> c)
            IF_THROW_EXCEPTION(invalid_argument_error("When using 'rad' the 'rad' has to be the last token, found '" + std::string{c} + "' in '" + in.str() + "' when parsing an angle"));
        angle *= 180 / M_PI;
        in.clear();
        return in;
    }
    else {
        // check for UTF-8 degree symbol ° or º
        std::string dS1 = "°";
        std::string dS2 = "º";
        assert(dS1.front() == dS2.front()); // both start with the same first byte
        if (c != dS1.front()) {
            in.putback(c);
            return in;
        }
        std::string degreeSymbol{c};
        if (!(in >> c)) {
            in.putback(c);
            return in;
//            IF_THROW_EXCEPTION(invalid_argument_error("Invalid format, expected 'd', 'deg', 'º' or 'rad' after '" + std::to_string(angle) + "', but string ended after '" + degreeSymbol + "', so parsing an angle from '" + in.str() + "' failed"));
        }
        degreeSymbol.push_back(c);
        if (degreeSymbol.find(dS1) != 0 && degreeSymbol.find(dS2) != 0) {
            in.putback(degreeSymbol.back());
            in.putback(degreeSymbol.front());
            return in;
//            IF_THROW_EXCEPTION(invalid_argument_error("Invalid format, expected 'd', 'deg', 'º' or 'rad' after '" + std::to_string(angle) + "', but got '" + degreeSymbol + "', so parsing an angle from '" + in.str() + "' failed"));
        }
    }

    // finished or arc minutes, check carefully whether the mandatory minute symbol ' appears after the number
    in >> std::noskipws;
    std::string numStr;
    bool haveSkippedPre = false;
    bool isNumComplete = false;
    bool usedNum = false;
    std::string skippedPost;
    while (in.peek() != std::stringstream::traits_type::eof()) {
        c = in.get();
        if (!haveSkippedPre && std::isspace(c))
            continue;
        haveSkippedPre = true;

        if (!isNumComplete && (std::isdigit(c) || c == '.'))
            numStr.push_back(c);
        else if (std::isspace(c)) {
            isNumComplete = true;
            skippedPost.push_back(c);
        }
        else {
            if (numStr.empty() || c != '\'')
                break;
            usedNum = true;
            double minutes;
            try {
                minutes = Parse::Float(numStr);
            }
            catch (invalid_argument_error& e) {
                IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" for arc minutes when parsing an angle")));
            }
            if (minutes < 0)
                IF_THROW_EXCEPTION(invalid_argument_error("Negative arc minutes are not allowed, got " + std::to_string(minutes) + " minutes when parsing an angle from '" + in.str() + "'"));
            if (minutes > 60)
                IF_THROW_EXCEPTION(invalid_argument_error("Arc minutes must be less or equal to 60, got " + std::to_string(minutes) + " minutes when parsing an angle from '" + in.str() + "'"));
            if (angle < 0)
                angle -= minutes / 60;
            else
                angle += minutes / 60;
            break;
        }
    }

    if (!usedNum && !numStr.empty()) {
        // no minute symbol, so we might have read the next token,
        // try to put back the whole string (including whitespace) into the stream
        in.putback(c);
        for (auto it = skippedPost.crbegin(), it_end = skippedPost.crend(); it != it_end && in.putback(*it); ++it)
            ;
        for (auto it = numStr.crbegin(), it_end = numStr.crend(); it != it_end && in.putback(*it); ++it)
            ;
        in >> std::skipws;
        return in;
    }

    // finished or arc seconds, check carefully whether the mandatory minute symbol ' appears after the number
    numStr.clear();
    haveSkippedPre = false;
    isNumComplete = false;
    usedNum = false;
    skippedPost.clear();
    while (in.peek() != std::stringstream::traits_type::eof()) {
        c = in.get();
        if (!haveSkippedPre && std::isspace(c))
            continue;
        haveSkippedPre = true;

        if (!isNumComplete && (std::isdigit(c) || c == '.'))
            numStr += c;
        else if (std::isspace(c)) {
            isNumComplete = true;
            skippedPost.push_back(c);
        }
        else {
            isNumComplete = true;
            if (numStr.empty())
                break;
            if (c != '"' && c != '\'')
                break;
            else if (c == '\'') {
                char c2;
                if (!(in >> c2) || c2 != '\'') {
                    in.putback(c2);
                    break;
                }
            }
            usedNum = true;

            double seconds;
            try {
                seconds = Parse::Float(numStr);
            }
            catch (invalid_argument_error& e) {
                IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" for arc seconds when parsing an angle")));
            }
            if (seconds < 0)
                IF_THROW_EXCEPTION(invalid_argument_error("Negative arc seconds are not allowed, got " + std::to_string(seconds) + " seconds when parsing an angle from '" + in.str() + "'"));
            if (seconds > 60)
                IF_THROW_EXCEPTION(invalid_argument_error("Arc seconds must be less or equal to 60, got " + std::to_string(seconds) + " seconds when parsing an angle from '" + in.str() + "'"));
            if (angle < 0)
                angle -= seconds / 3600;
            else
                angle += seconds / 3600;
            break;
        }
    }

    if (!usedNum && !numStr.empty()) {
        // no seconds symbol, so we read maybe the next token,
        // try to put back the whole string (including whitespace) into the stream
        in.putback(c);
        for (auto it = skippedPost.crbegin(), it_end = skippedPost.crend(); it != it_end && in.putback(*it); ++it)
            ;
        for (auto it = numStr.crbegin(), it_end = numStr.crend(); it != it_end && in.putback(*it); ++it)
            ;
    }

    // success
    in >> std::skipws;
    in.clear();
    return in;
}

} /* anonymous namespace */

double Parse::Angle(std::string const& str, std::string const& optName) {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";

    std::string cardDir = "NnSsEeWw";
    auto isCardinalDir = [&cardDir] (char c) {
        return cardDir.find(c) != std::string::npos; };

    auto posCard = str.find_first_of(cardDir);
    if (posCard != str.find_last_of(cardDir))
        IF_THROW_EXCEPTION(invalid_argument_error("An angle may only contain one cardinal direction (N/S/E/W), found multiple in '" + str + "'" + oN));

    // start either with first angle or skip cardinal direction
    std::stringstream in(str);
    char c;
    if (!(in >> c))
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse an angle from '" + str + "'" + oN));
    if (std::isdigit(c) || c == '-' || c == '+' || c == '.')
        in.putback(c);
    else if (!isCardinalDir(c))
        IF_THROW_EXCEPTION(invalid_argument_error("An angle has to start with a cardinal direction or the number. Could not find either in '" + str + "'" + oN));

    AngleWrapper a;
    try {
        if (!(in >> a))
            IF_THROW_EXCEPTION(invalid_argument_error("Could not parse an angle from '" + str + "'"));
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + oN));
    }

    std::string rest;
    while (in >> rest)
        if (rest.size() != 1 || !isCardinalDir(rest.front()))
            IF_THROW_EXCEPTION(invalid_argument_error("Nothing allowed after the angle, found '" + rest + "' in '" + str + "'" + oN));

    if (posCard != std::string::npos) {
        if (a.angle < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("When specifying a cardinal direction (N/S/E/W), the angle must be non-negative, found " + std::to_string(a.angle) + " in '" + str + "'" + oN));

        if (std::string("SsWw").find(str.at(posCard)) != std::string::npos)
            return -a.angle;
    }
    return a.angle;
}


imagefusion::Coordinate Parse::GeoCoord(std::string const& str, std::string const& optName) {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";

    std::string cardDir = "NnSsEeWw";
    auto isCardinalDir = [&cardDir] (char c) {
        return cardDir.find(c) != std::string::npos; };

    // check that there is either one of N/S and one of E/W or none of N/S/E/W
    auto posNS = str.find_first_of("NnSs");
    if (posNS != str.find_last_of("NnSs"))
        IF_THROW_EXCEPTION(invalid_argument_error("A GeoCoord may only contain one latitude cardinal direction (N/S), found multiple in '" + str + "'" + oN));

    auto posEW = str.find_first_of("EeWw");
    if (posEW != str.find_last_of("EeWw"))
        IF_THROW_EXCEPTION(invalid_argument_error("A GeoCoord may only contain one longitude cardinal direction (E/W), found multiple in '" + str + "'" + oN));

    if ((posNS != std::string::npos) != (posEW != std::string::npos))
        IF_THROW_EXCEPTION(invalid_argument_error("A GeoCoord must either have cardinal directions for both of latitude and longitude or for none."
                                                  " Found one for "    + (posNS != std::string::npos ? std::string("latitude") : std::string("longitude")) +
                                                  " in, but none for " + (posNS != std::string::npos ? std::string("longitude") : std::string("latitude")) + " in '" + str + "'" + oN));

    int signNS = 1;
    if (posNS != std::string::npos && (str.at(posNS) == 'S' || str.at(posNS) == 's'))
        signNS = -1;

    int signEW = 1;
    if (posEW != std::string::npos && (str.at(posEW) == 'W' || str.at(posEW) == 'w'))
        signEW = -1;

    // whitespace will be skipped automatically
    std::stringstream in(str);
    AngleWrapper a1;
    AngleWrapper a2;
    char c;

    // start either with first angle or skip cardinal direction
    if (!(in >> c))
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse a GeoCoord from '" + str + "'" + oN));
    if (std::isdigit(c) || c == '-' || c == '+' || c == '.')
        in.putback(c);
    else if (!isCardinalDir(c))
        IF_THROW_EXCEPTION(invalid_argument_error("A GeoCoord has to start with a cardinal direction or an angle. Could not find either in '" + str + "'" + oN));

    // parse first angle, decide later if longitude or latitude
    try {
        if (!(in >> a1))
            IF_THROW_EXCEPTION(invalid_argument_error("Could not parse first angle of GeoCoord from '" + str + "'"));
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + oN));
    }

    // find start of second angle, skip ',' and cardinal direction
    while (in >> c) {
        if (std::isdigit(c) || c == '-' || c == '+' || c == '.') {
            in.putback(c);
            break;
        }
        else if (!isCardinalDir(c) && c != ',')
            IF_THROW_EXCEPTION(invalid_argument_error("Inbetween the two angles only comma ',' and cardinal direction are allowed in a GeoCoord, found '" + std::string{c} + "' in '" + str + "'" + oN));
    }
    if (!in)
        IF_THROW_EXCEPTION(invalid_argument_error("String '" + str + "' ended before the second angle for the GeoCoord could be parsed" + oN));

    // parse second angle, decide later if longitude or latitude
    try {
        if (!(in >> a2))
            IF_THROW_EXCEPTION(invalid_argument_error("Could not parse second angle of GeoCoord from '" + str + "'"));
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + oN));
    }

    std::string rest;
    while (in >> rest)
        if (rest.size() != 1 || !isCardinalDir(rest.front()))
        IF_THROW_EXCEPTION(invalid_argument_error("No garbage allowed after a GeoCoord, found '" + rest + "' in '" + str + "'" + oN));

    // only if E/W came before N/S, use first angle as longitude and second as latitude
    if (posEW < posNS) {
        if (a1.angle < 0)
            IF_THROW_EXCEPTION(invalid_argument_error("When specifying a cardinal direction (N/S/E/W), the angle must be non-negative, found " + std::to_string(a1.angle) + " with " + std::string{str.at(posEW)} + " in '" + str + "'" + oN));
        if (a2.angle < 0 && posNS != std::string::npos)
            IF_THROW_EXCEPTION(invalid_argument_error("When specifying a cardinal direction (N/S/E/W), the angle must be non-negative, found " + std::to_string(a2.angle) + " with " + std::string{str.at(posNS)} + " in '" + str + "'" + oN));

        return imagefusion::Coordinate{a1.angle * signEW, a2.angle * signNS};
    }

    if (a1.angle < 0 && posNS != std::string::npos)
        IF_THROW_EXCEPTION(invalid_argument_error("When specifying a cardinal direction (N/S/E/W), the angle must be non-negative, found " + std::to_string(a1.angle) + " with " + std::string{str.at(posNS)} + " in '" + str + "'" + oN));
    if (a2.angle < 0 && posEW != std::string::npos)
        IF_THROW_EXCEPTION(invalid_argument_error("When specifying a cardinal direction (N/S/E/W), the angle must be non-negative, found " + std::to_string(a2.angle) + " with " + std::string{str.at(posEW)} + " in '" + str + "'" + oN));

    return imagefusion::Coordinate{a2.angle * signEW, a1.angle * signNS};
}

imagefusion::Type Parse::Type(std::string str, std::string const& optName) {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    if (str.size() < 4)
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as type (too short to be a type)" + oN));

    std::transform(str.begin(), str.end(), str.begin(), ::tolower);

    imagefusion::Type basetype;
    if (str.substr(0, 5) == "uint8" || str.substr(0, 4) == "byte")
        basetype = imagefusion::Type::uint8;
    else if (str.substr(0, 4) == "int8")
        basetype = imagefusion::Type::int8;
    else if (str.substr(0, 6) == "uint16")
        basetype = imagefusion::Type::uint16;
    else if (str.substr(0, 5) == "int16")
        basetype = imagefusion::Type::int16;
    else if (str.substr(0, 5) == "int32")
        basetype = imagefusion::Type::int32;
    else if (str.substr(0, 7) == "float64" || str.substr(0, 6) == "double")
        basetype = imagefusion::Type::float64;
    else if (str.substr(0, 5) == "float"   || str.substr(0, 6) == "single") // must be checked after float64
        basetype = imagefusion::Type::float32;
    else
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as type (given type is unknown)" + oN));

    int channels = 1;
    std::string::size_type n = str.rfind("x");
    try {
        if (n != std::string::npos)
            channels = std::stoi(str.substr(n+1));
    }
    catch (std::invalid_argument&) {
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse the channel specifier in '" + str + "' (after the 'x' a 1, 2, 3 or 4 must follow)" + oN));
    }
    if (channels < 1 || channels > 4)
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as valid type, since the number of channels is invalid ("
                                                  + std::to_string(channels) + ", but only 1, 2, 3 or 4 is allowed)" + oN));

    return imagefusion::getFullType(basetype, static_cast<uint8_t>(channels));
}


namespace {

std::stringstream& operator>>(std::stringstream& in, Interval& interval) {
    // open:   '(a,b)', or just 'a,b' (parens could have been eaten by separateArguments()),
    // closed: '[a,b]'
    char first;
    if (!(in >> first))
        return in;
    bool isLeftOpen  = first != '[';

    // put back number if there was no bracket
    if (first != '(' && first != '[')
        in.putback(first);

    std::string s;
    bool skipped;
    char c;

    // extract lower bound as string
    skipped = false;
    while (in.peek() != std::stringstream::traits_type::eof()) {
        c = in.get();
        if (!skipped && std::isspace(c))
            continue;
        skipped = true;
        if (std::isalnum(c) || c == '-' || c == '+' || c == '.')
            s += c;
        else {
            in.putback(c);
            break;
        }
    }

    // convert lower bound to double
    double low;
    try {
        low = std::stod(s);
    }
    catch (std::invalid_argument&) {
        IF_THROW_EXCEPTION(invalid_argument_error("Could not read lower bound from '" +
                                                  (in.peek() != std::stringstream::traits_type::eof() ? std::string(1, in.peek()) : std::string("EOL")) +
                                                  "' in '" + in.str() + "' for an interval"));
    }

    if (std::isinf(low))
        isLeftOpen = true;

    // check whether the interval format was just <float>, which should be interpreted as [a, a]
    in >> c;
    if (!in) {
        if (std::isalnum(first) || first == '-' || first == '+' || first == '.') {
            interval = Interval::closed(low, low);
            in.clear();
            return in;
        }
        IF_THROW_EXCEPTION(invalid_argument_error("Interval string ended after lower bound '" + std::to_string(low) + "', so failed to parse an interval from '" + in.str() + "'"));
    }

    // skip comma inbetween low and high
    if (c != ',')
        in.putback(c);

    // extract upper bound as string
    s.clear();
    skipped = false;
    while (in.peek() != std::stringstream::traits_type::eof()) {
        c = in.get();
        if (!skipped && std::isspace(c))
            continue;
        skipped = true;
        if (std::isalnum(c) || c == '-' || c == '+' || c == '.')
            s += c;
        else {
            in.putback(c);
            break;
        }
    }


    // convert upper bound to double
    double high;
    try {
        high = std::stod(s);
    }
    catch (std::invalid_argument&) {
        std::string leftPart = (isLeftOpen ? "(" : "[") + std::to_string(low) + ",";
        IF_THROW_EXCEPTION(invalid_argument_error("Could not read upper bound after '" + leftPart + "' in '" + in.str() + "' for an interval"));
    }

    // catch missing closing parenthesis to avoid fail flag
    bool isRightOpen = true;
    if (!in.eof()) {
        in >> c;
        isRightOpen = c != ']';

        // put back number if there was no bracket
        if (c != ')' && c != ']')
            in.putback(c);
    }

    if (std::isinf(high))
        isRightOpen = true;

    if (isLeftOpen && isRightOpen)
        interval = Interval::open(low, high);

    else if (isLeftOpen)
        interval = Interval::left_open(low, high);

    else if (isRightOpen)
        interval = Interval::right_open(low, high);

    else
        interval = Interval::closed(low, high);
    return in;
}

} /* anonymous namespace */

Interval Parse::Interval(std::string const& str, std::string const& optName) {
    // whitespace will be skipped automatically
    std::stringstream in(str);
    imagefusion::Interval i;

    try {
        in >> i;
        return i;
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + (optName.empty() ? std::string("") : " for option '" + optName + "'")));
    }
}

IntervalSet Parse::IntervalSet(std::string const& str, std::string const& optName) {
    // whitespace will be skipped automatically
    std::stringstream in(str);

    imagefusion::Interval i;
    imagefusion::IntervalSet is;

    try {
        while (in >> i) {
            // add interval to set
            is += i;

            // skip ',' inbetween intervals
            if (in.peek() != std::stringstream::traits_type::eof()) {
                char c = in.get();
                if (c != ',')
                    in.putback(c);
            }
        }
        return is;
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + (optName.empty() ? std::string("") : " for option '" + optName + "'")));
    }
}


const std::vector<Descriptor> Parse::usageSize {
Descriptor::text("<size> either receives the following arguments:"),
{ "WIDTH",   "","", "width", ArgChecker::Int,     "" },
{ "WIDTH",   "","w","w",     ArgChecker::Int,     "  -w <num>, \t--width=<num>, \twidth." },
{ "HEIGHT",  "","h","h",     ArgChecker::Int,     "  -h <num>, \t--height=<num>, \theight.\n"
                                                  "or must have the form '<num>x<num>' or just '(<num> <num>)' both with optional spacing, where the first argument is the width and the second is the height.\n" },
{ "HEIGHT",  "","", "height",ArgChecker::Int,     "Examples: ... --<option>=(-w 100 -h 200) ...\n"
                                                  "          ... --<option>=100x200 ...\n"
                                                  "          ... --<option>=(100 200) ..." }
};

const std::vector<Descriptor> Parse::usageDimensions {
Descriptor::text("<size> either receives the following arguments:"),
{ "WIDTH",   "","", "width", ArgChecker::Float,   "" },
{ "WIDTH",   "","w","w",     ArgChecker::Float,   "  -w <num>, \t--width=<num>, \twidth." },
{ "HEIGHT",  "","h","h",     ArgChecker::Float,   "  -h <num>, \t--height=<num>, \theight.\n"
                                                  "or must have the form '<num>x<num>' or just '(<num> <num>)' both with optional spacing, where the first argument is the width and the second is the height.\n" },
{ "HEIGHT",  "","", "height",ArgChecker::Float,   "Examples: ... --<option>=(-w 100 -h 200) ...\n"
                                                  "          ... --<option>=100x200 ...\n"
                                                  "          ... --<option>=(100 200) ..." }
};

namespace {

template <typename T>
cv::Size_<T> parseSizeSubopts(std::vector<Descriptor> const& usage, std::string const& str, std::string const& optName = "") {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    try {
        // will throw on negative numbers like '(-2 -5)' with error: Unknown option '2'.
        args.parse(argsTokens);
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing size") + oN));
    }

    if (args["WIDTH"].empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Width not specified" + oN));

    if (args["HEIGHT"].empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Height not specified" + oN));

    cv::Size_<T> s;
    s.width  = Parse::Arg<T>(args["WIDTH"].back().arg);
    s.height = Parse::Arg<T>(args["HEIGHT"].back().arg);
    return s;
}

template <typename T>
cv::Size_<T> parseSizeSpecial(std::string const& str, std::string const& optName = "") {
    using tokenizer = boost::tokenizer<boost::char_separator<char>>;
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    boost::char_separator<char> sep(" x*()'\",");
    tokenizer tokens(str, sep);
    if (std::distance(tokens.begin(), tokens.end()) != 2)
        IF_THROW_EXCEPTION(invalid_argument_error("Only two numbers allowed separated by ' ', 'x', '*', '(', ')', ''', '\"' or ',' when parsing size" + oN));

    try {
        tokenizer::iterator it = tokens.begin();
        cv::Size_<T> s;
        s.width = Parse::Arg<T>(*it);
        ++it;
        s.height = Parse::Arg<T>(*it);
        return s;
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing size") + oN));
    }
}

template <typename T>
cv::Size_<T> parseSize(std::vector<Descriptor> const& usage, std::string const& str, std::string const& optName) {
    // first, try to parse with option syntax, like "-w <num> -h <num>"
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    std::string errSub;
    try {
        // will throw on negative numbers like '(-2 -5)' with error: Unknown option '2' when parsing size.
        return parseSizeSubopts<T>(usage, str);
    }
    catch (invalid_argument_error& e) {
        errSub = e.what();
    }

    // no luck with the option syntax. Try to parse other forms, like "(<num>x<num>)"
    try {
        // will throw on negative numbers like '(-2 -5)' with error: Unknown option '2' when parsing size.
        return parseSizeSpecial<T>(str);
    }
    catch (invalid_argument_error& e) {
//        std::ostringstream usage_ss;
//        printUsage(usage_ss, usage);
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as two numbers (error: " + e.what()
                + "), but also not with sub-options " + "(error: " + errSub + ") when parsing size" + oN));
    }
}

} /* annonymous namespace */

Size Parse::Size(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageSize) {
    return parseSize<int>(usageSize, str, optName);
}

Size Parse::SizeSubopts(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageSize) {
    return parseSizeSubopts<int>(usageSize, str, optName);
}

Size Parse::SizeSpecial(std::string const& str, std::string const& optName) {
    return parseSizeSpecial<int>(str, optName);
}

Dimensions Parse::Dimensions(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageDimensions) {
    return parseSize<double>(usageDimensions, str, optName);
}

Dimensions Parse::DimensionsSubopts(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageDimensions) {
    return parseSizeSubopts<double>(usageDimensions, str, optName);
}

Dimensions Parse::DimensionsSpecial(std::string const& str, std::string const& optName) {
    return parseSizeSpecial<double>(str, optName);
}



const std::vector<Descriptor> Parse::usagePoint {
Descriptor::text("Option usage: <point> either receives the following arguments:\n"),
{ "X",       "","x","x", ArgChecker::Int, "  -x <num>" },
{ "Y",       "","y","y", ArgChecker::Int, "  -y <num>\n"
                                          "or must have (<num>, <num>) with optional spacing and comma, where the first argument is for x and the second is for y.\n"},
Descriptor::text("Examples: ... --<option>=(-x 5 -y 6) ... \n"
                 "          ... --<option>=(5, 6) ...")
};

const std::vector<Descriptor> Parse::usageCoordinate {
Descriptor::text("Option usage: <point> either receives the following arguments:\n"),
{ "X",       "","x","x", ArgChecker::Float, "  -x <float>" },
{ "Y",       "","y","y", ArgChecker::Float, "  -y <float>\n"
                                            "or must have (<float>, <float>) with optional spacing and comma, where the first argument is for x and the second is for y.\n" },
Descriptor::text("Examples: ... --<option>=(-x 3.1416 -y 42) ... \n"
                 "          ... --<option>=(3.1416, 42) ...")
};

namespace {

template <typename T>
cv::Point_<T> parsePointSubopts(std::vector<Descriptor> const& usage, std::string const& str, std::string const& optName = "") {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    try {
        // will throw on negative numbers like '(-2 -5)' with error: Unknown option '2'.
        args.parse(argsTokens);
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing point") + oN));
    }

    if (args["X"].empty())
        IF_THROW_EXCEPTION(invalid_argument_error("x value not specified" + oN));
    if (args["Y"].empty())
        IF_THROW_EXCEPTION(invalid_argument_error("y value not specified" + oN));

    cv::Point_<T> p;
    p.x = Parse::Arg<T>(args["X"].back().arg);
    p.y = Parse::Arg<T>(args["Y"].back().arg);
    return p;
}

template <typename T>
cv::Point_<T> parsePointSpecial(std::string const& str, std::string const& optName = "") {
    using tokenizer = boost::tokenizer<boost::char_separator<char>>;
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    boost::char_separator<char> sep(" ()'\",");
    tokenizer tokens(str, sep);
    if (std::distance(tokens.begin(), tokens.end()) != 2)
        IF_THROW_EXCEPTION(invalid_argument_error("Only two numbers allowed separated by ' ', '(', ')', ''', '\"' or ',' when parsing point" + oN));

    try {
        tokenizer::iterator it = tokens.begin();
        cv::Point_<T> p;
        p.x = Parse::Arg<T>(*it);
        ++it;
        p.y = Parse::Arg<T>(*it);
        return p;
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing point") + oN));
    }
}

template <typename T>
cv::Point_<T> parsePoint(std::vector<Descriptor> const& usage, std::string const& str, std::string const& optName) {
    // first, try to parse with option syntax, like "-x <num> -y <num>"
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    std::string errSub;
    try {
        // will throw on negative numbers like '(-2 -5)' with error: Unknown option '2'.
        return parsePointSubopts<T>(usage, str);
    }
    catch (invalid_argument_error& e) {
        errSub = e.what();
    }

    // no luck with the option syntax. Try to parse other forms, like "(<num>, <num>)"
    try {
        return parsePointSpecial<T>(str);
    }
    catch (invalid_argument_error& e) {
//        std::ostringstream usage_ss;
//        printUsage(usage_ss, usage);
        IF_THROW_EXCEPTION(invalid_argument_error("Could not parse '" + str + "' as two numbers (error: " + e.what()
                + "), but also not with sub-options " + "(error: " + errSub + ") when parsing point" + oN));
    }
}

} /* annonymous namespace */

Point Parse::Point(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usagePoint) {
    return parsePoint<int>(usagePoint, str, optName);
}

Point Parse::PointSubopts(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usagePoint) {
    return parsePointSubopts<int>(usagePoint, str, optName);
}

Point Parse::PointSpecial(std::string const& str, std::string const& optName) {
    return parsePointSpecial<int>(str, optName);
}

Coordinate Parse::Coordinate(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageCoordinate) {
    return parsePoint<double>(usageCoordinate, str, optName);
}

Coordinate Parse::CoordinateSubopts(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageCoordinate) {
    return parsePointSubopts<double>(usageCoordinate, str, optName);
}

Coordinate Parse::CoordinateSpecial(std::string const& str, std::string const& optName) {
    return parsePointSpecial<double>(str, optName);
}




const std::vector<Descriptor> Parse::usageRectangle {
Descriptor::text("Option usage: <rect> requires a combination of some of the following arguments:"),
{ "WIDTH",    "","", "width",  ArgChecker::Int,            "" },
{ "X",        "","x","x",      ArgChecker::Vector<int>,    "  -x <num>, \t\tdefines x start or\n"
                                                                          "  -x (<num> <num>), \t\tdefines x start and x end" },
{ "Y",        "","y","y",      ArgChecker::Vector<int>,    "  -y <num>, \t\tdefines y start\n"
                                                                          "  -y (<num> <num>), \t\tdefines y start and y end" },
{ "CENTER",   "","c","c",      ArgChecker::Vector<double>, "  -c (<num> <num>), \t--center=(<num> <num>) \tdefines the (x y) center point" },
{ "WIDTH",    "","w","w",      ArgChecker::Int,            "  -w <num>, \t--width=<num> \tdefines the width" },
{ "HEIGHT",   "","h","h",      ArgChecker::Int,            "  -h <num>, \t--height=<num> \tdefines the height" },
{ "HEIGHT",   "","", "height", ArgChecker::Int,            "" },
{ "CENTER",   "","", "center", ArgChecker::Vector<double>, "Examples: ... --<option>='-x 1 -y 2 -w 10 -h 10' ... \n"
                                                           "          ... --<option>='-c (5.5 6.5) -w 10 -h 10' ... \n"
                                                           "          ... --<option>='-x (1 10) -y (2 11)' ... \n" }
};

const std::vector<Descriptor> Parse::usageCoordRectangle {
Descriptor::text("Option usage: <rect> requires a combination of some of the following arguments:"),
{ "WIDTH",    "","", "width",  ArgChecker::Float,          "" },
{ "X",        "","x","x",      ArgChecker::Vector<double>, "  -x <num>, \t\tdefines x start or\n"
                                                                          "  -x (<num> <num>), \t\tdefines x start and x end" },
{ "Y",        "","y","y",      ArgChecker::Vector<double>, "  -y <num>, \t\tdefines y start\n"
                                                                          "  -y (<num> <num>), \t\tdefines y start and y end" },
{ "CENTER",   "","c","c",      ArgChecker::Vector<double>, "  -c (<num> <num>), \t--center=(<num> <num>) \tdefines the (x y) center point" },
{ "WIDTH",    "","w","w",      ArgChecker::Float,          "  -w <num>, \t--width=<num> \tdefines the width" },
{ "HEIGHT",   "","h","h",      ArgChecker::Float,          "  -h <num>, \t--height=<num> \tdefines the height" },
{ "HEIGHT",   "","", "height", ArgChecker::Float,          "" },
{ "CENTER",   "","", "center", ArgChecker::Vector<double>, "Examples: ... --<option>='-x 1 -y 2 -w 10 -h 10' ... \n"
                                                           "          ... --<option>='-c (5.5 6.5) -w 10 -h 10' ... \n"
                                                           "          ... --<option>='-x (1 10) -y (2 11)' ... \n" }
};


namespace {

template <class T>
cv::Rect_<T> parseRect(std::vector<Descriptor> const& usage, std::string const& str, std::string const& optName) {
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    try {
        args.parse(argsTokens);
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing rectangle") + oN));
    }

    if (!args["CENTER"].empty() &&
            (!args["X"].empty() || !args["Y"].empty()))
    {
        IF_THROW_EXCEPTION(invalid_argument_error("Either center point or x and y extents can be specified, but not center and x or y extents. Error when parsing rectangle" + oN));
    }

    constexpr T sizeFix = std::numeric_limits<T>::is_integer ? 1 : 0;
    cv::Rect_<T> r;
    if (!args["WIDTH"].empty())
        r.width = Parse::Arg<T>(args["WIDTH"].back().arg);
    if (!args["HEIGHT"].empty())
        r.height = Parse::Arg<T>(args["HEIGHT"].back().arg);

    if (!args["CENTER"].empty()) {
        if (args["WIDTH"].empty() ||
            args["HEIGHT"].empty())
        {
            IF_THROW_EXCEPTION(invalid_argument_error("When center point is specified, width and height are required. Error when parsing rectangle" + oN));
        }

        std::vector<double> xyCenter = Parse::Vector<double>(args["CENTER"].back().arg);
        if (xyCenter.size() != 2) {
            IF_THROW_EXCEPTION(invalid_argument_error("Center point requires one number for x center and one for y center, like (x y). Error when parsing rectangle" + oN));
        }

        r.x = cv::saturate_cast<T>(xyCenter.front() - (r.width  - sizeFix) / 2.0);
        r.y = cv::saturate_cast<T>(xyCenter.back()  - (r.height - sizeFix) / 2.0);
    }
    else if (!args["X"].empty() &&
             !args["Y"].empty())
    {
        std::vector<T> x = Parse::Vector<T>(args["X"].back().arg);
        if (x.size() == 1 && !args["WIDTH"].empty()) {
            r.x = x.front();
        }
        else if (x.size() == 2 && args["WIDTH"].empty()) {
            r.x = std::min(x.front(), x.back());
            r.width = std::abs(x.front() - x.back()) + sizeFix;
        }
        else if (x.size() > 2)
            IF_THROW_EXCEPTION(invalid_argument_error("Only two numbers allowed to specify x start and x end, like (xstart xend). Error when parsing rectangle" + oN));
        else if (x.size() == 2 && !args["WIDTH"].empty())
            IF_THROW_EXCEPTION(invalid_argument_error("Either specify just x start and width or x start and x end, but not x start, x end and width. Error when parsing rectangle" + oN));
        else // (x.size() == 1 && args["WIDTH"].empty())
            IF_THROW_EXCEPTION(invalid_argument_error("Either specify x start and width or x start and x end. You specified only x start. Error when parsing rectangle" + oN));

        std::vector<T> y = Parse::Vector<T>(args["Y"].back().arg);
        if (y.size() == 1 && !args["HEIGHT"].empty()) {
            r.y = y.front();
        }
        else if (y.size() == 2 && args["HEIGHT"].empty()) {
            r.y = std::min(y.front(), y.back());
            r.height = std::abs(y.front() - y.back()) + sizeFix;
        }
        else if (y.size() > 2)
            IF_THROW_EXCEPTION(invalid_argument_error("Only two numbers allowed to specify y start and y end, like (ystart yend). Error when parsing rectangle" + oN));
        else if (y.size() == 2 && !args["HEIGHT"].empty())
            IF_THROW_EXCEPTION(invalid_argument_error("Either specify just y start and height or y start and y end, but not y start, y end and height. Error when parsing rectangle" + oN));
        else // (y.size() == 1 && args["HEIGHT"].empty())
            IF_THROW_EXCEPTION(invalid_argument_error("Either specify y start and height or y start and y end. You specified only y start. Error when parsing rectangle" + oN));
    }
    else {
        IF_THROW_EXCEPTION(invalid_argument_error("Either specify center point, width and height or x and y extents. Error when parsing rectangle" + oN));
    }

    return r;
}

} /* annonymous namespace */

Rectangle Parse::Rectangle(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageRectangle) {
    return parseRect<int>(usageRectangle, str, optName);
}

CoordRectangle Parse::CoordRectangle(std::string const& str, std::string const& optName, std::vector<Descriptor> const& usageCoordRectangle) {
    return parseRect<double>(usageCoordRectangle, str, optName);
}


const std::vector<Descriptor> Parse::usageImage {
Descriptor::text("<img> must have the form '-f <file> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', where the arguments can have an arbitrary order. "
                 "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"),
{ "FILE",   "",       "f","file",                    ArgChecker::NonEmpty,    "  -f <file>, \t--file=<file>, \tImage file path (string)." },
{ "LAYERS", "",       "l","layers",                  ArgChecker::Vector<int>, "  -l <num-list>, \t--layers=<num-list>, \tOptional. Load only specified channels or layers. Hereby a 0 means the first channel.\v"
                                                                                                                        "<num-list> must have the format '(<num> [<num> ...])' without commas inbetween or just '<num>'" },
{ "CROP",   "",       "c","crop",                    ArgChecker::Rectangle,   "  -c <rect>, \t--crop=<rect>, \tOptional. Specifies the crop window, where the image will be read. A zero width or height means full width or height, respectively.\v"
                                                                                                              "<rect> requires all of the following arguments:\v"
                                                                                                              " -x <num>                 x start\v"
                                                                                                              " -y <num>                 y start\v"
                                                                                                              " -w <num>, --width=<num>  width\v"
                                                                                                              " -h <num>, --height=<num> height\n" },
{ "COLTAB", "DISABLE","", "disable-use-color-table", ArgChecker::None,        "  \t--disable-use-color-table, \tFor images with color table, do not convert indexed colors." },
{ "COLTAB", "ENABLE", "", "enable-use-color-table",  ArgChecker::None,        "  \t--enable-use-color-table, \tFor images with color table, convert indexed colors. Default." },
Descriptor::text("Example: ... --<option>=(--file='test image.tif') ... \n"
                 "         ... --<option>=(-f test.tif --crop=(-x 1 -y 2 -w 3 -h 4) --layers=(0 2) ) ...")
};

imagefusion::Image Parse::Image(std::string const& str, std::string const& optName, bool readImage, std::vector<Descriptor> const& usageImage) {
    std::string defaultArgs = "--enable-use-color-table  ";
    std::vector<std::string> argsTokens = separateArguments(defaultArgs + str);
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    OptionParser args(usageImage);
    args.acceptsOptAfterNonOpts = true;
    try {
        args.parse(argsTokens);
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing plain image") + oN));
    }

    std::string filename;
    if (!args["FILE"].empty())
        filename = args["FILE"].back().arg;
    else if (args.nonOptionArgCount() >= 1)
        // ok no option 'file' found, but maybe the filename is given directly, like '-i FILE' or --img="FILE -l 0"
        filename = args.nonOptionArgs.front();
    else
        IF_THROW_EXCEPTION(invalid_argument_error("Argument for plain image missing. You need to specify the image file" + oN));

    std::vector<int> l;
    for (auto opt : args["LAYERS"]) {
        auto addLayers = Parse::Vector<int>(opt.arg);
        l.insert(l.end(), addLayers.begin(), addLayers.end());
    }

    imagefusion::Rectangle r;
    if (args["CROP"].size() > 1)
        IF_THROW_EXCEPTION(invalid_argument_error("Multiple crop options for one image are not allowed" + oN /*+ ".\n" + usage_ss.str()*/));
    if (!args["CROP"].empty())
        r = Parse::Rectangle(args["CROP"].back().arg);

    if (readImage) {
        try {
            bool ignoreColorTable = args["COLTAB"].back().prop() == "DISABLE" ? true : false;
            return imagefusion::Image(filename, l, r, /*flipH*/ false, /*flipV*/ false, ignoreColorTable);
        }
        catch (runtime_error& e) {
            IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing plain image") + oN));
        }
    }

    GeoInfo gi;
    try {
        gi = GeoInfo{filename, l, r}; // also if str does not contain layers, this will call the constructor, which tries to read all subdatasets (if any)
    }
    catch (runtime_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing plain image") + oN));
    }

    if (gi.baseType == Type::invalid) {
        IF_THROW_EXCEPTION(invalid_argument_error("The selected subdatasets of '" + str + "' have different data types when parsing plain image" + oN))
                << boost::errinfo_file_name(filename);
    }
    return imagefusion::Image{};
}


const std::vector<Descriptor> Parse::usageMRImage {
Descriptor::text("<img> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>] [--disable-use-color-table]', where the arguments can have an arbitrary order. "
                 "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"),
{ "FILE",   "",       "f","file",                    ArgChecker::NonEmpty,    "  -f <file>, \t--file=<file>, \tImage file path (string)." },
{ "DATE",   "",       "d","date",                    ArgChecker::Int,         "  -d <num>, \t--date=<num>, \tDate (number)." },
{ "TAG",    "",       "t","tag",                     ArgChecker::NonEmpty,    "  -t <tag>, \t--tag=<tag>, \tResolution tag (string)." },
{ "LAYERS", "",       "l","layers",                  ArgChecker::Vector<int>, "  -l <num-list>, \t--layers=<num-list>, \tOptional. Load only specified channels or layers. Hereby a 0 means the first channel.\v"
                                                                                           "<num-list> must have the format '(<num> [<num> ...])' without commas inbetween or just '<num>'" },
{ "CROP",   "",       "c","crop",                    ArgChecker::Rectangle,   "  -c <rect>, \t--crop=<rect>, \tOptional. Specifies the crop window, where the image will be read. A zero width or height means full width or height, respectively.\v"
                                                                                           "<rect> requires all of the following arguments:\v"
                                                                                           " -x <num>                 x start\v"
                                                                                           " -y <num>                 y start\v"
                                                                                           " -w <num>, --width=<num>  width\v"
                                                                                           " -h <num>, --height=<num> height\n" },
{ "COLTAB", "DISABLE","", "disable-use-color-table", ArgChecker::None,        "  \t--disable-use-color-table, \tFor images with color table, do not convert indexed colors." },
{ "COLTAB", "ENABLE", "", "enable-use-color-table",  ArgChecker::None,        "  \t--enable-use-color-table, \tFor images with color table, convert indexed colors. Default." },
Descriptor::text("Example: ... --<option>=(--file='test image.tif' -d 0 -t HIGH) ... \n"
                 "         ... --<option>=(-f test.tif -d 0 -t HIGH --crop=(-x 1 -y 2 -w 3 -h 4) --layers=(0 2) ) ...")
};


ImageInput Parse::MRImage(std::string const& str, std::string const& optName, bool readImage, bool isDateOpt, bool isTagOpt, std::vector<Descriptor> const& usageMRImage) {
    std::string defaultArgs = "--enable-use-color-table  ";
    std::vector<std::string> argsTokens = separateArguments(defaultArgs + str);
    std::string oN = optName.empty() ? std::string() : " for option '" + optName + "'";
    OptionParser args(usageMRImage);
    args.acceptsOptAfterNonOpts = true;
    try {
        args.parse(argsTokens);
    }
    catch (invalid_argument_error& e) {
        IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing multi-res image") + oN));
    }

    std::string filename;
    if (!args["FILE"].empty())
        filename = args["FILE"].back().arg;
    else if (args.nonOptionArgCount() >= 1)
        // ok no option 'file' found, but maybe the filename is given directly, like --img="FILE -d 0 -t high"
        filename = args.nonOptionArgs.front();
    else
        IF_THROW_EXCEPTION(invalid_argument_error("Argument for multi-res image missing. You need to specify the image file" + oN));

    ImageInput in;
    if (args["DATE"].empty()) {
        if (isDateOpt)
            in.date = 0;
        else
            IF_THROW_EXCEPTION(invalid_argument_error("Arguments for multi-res image missing. You need to specify date" + oN /*+ ".\n" + usage_ss.str()*/));
    }
    else
        in.date = Parse::Int(args["DATE"].back().arg);

    if (args["TAG"].empty()) {
        if (isTagOpt)
            in.tag = "";
        else
            IF_THROW_EXCEPTION(invalid_argument_error("Arguments for multi-res image missing. You need to specify tag" + oN /*+ ".\n" + usage_ss.str()*/));
    }
    else
        in.tag = args["TAG"].back().arg;

    std::vector<int> l;
    for (auto opt : args["LAYERS"]) {
        auto addLayers = Parse::Vector<int>(opt.arg);
        l.insert(l.end(), addLayers.begin(), addLayers.end());
    }

    imagefusion::Rectangle r;
    if (args["CROP"].size() > 1)
        IF_THROW_EXCEPTION(invalid_argument_error("Multiple crop options for one image are not allowed" + oN /*+ ".\n" + usage_ss.str()*/));
    if (!args["CROP"].empty())
        r = Parse::Rectangle(args["CROP"].back().arg);

    if (readImage) {
        try {
            bool ignoreColorTable = args["COLTAB"].back().prop() == "DISABLE" ? true : false;
            in.i.read(filename, l, r, /*flipH*/ false, /*flipV*/ false, ignoreColorTable);
        }
        catch (runtime_error& e) {
            IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing multi-res image") + oN));
        }
    }
    else {
        GeoInfo gi;
        try {
            gi = GeoInfo{filename, l, r}; // also if str does not contain layers, this will call the constructor, which tries to read all subdatasets (if any)
        }
        catch (runtime_error& e) {
            IF_THROW_EXCEPTION(invalid_argument_error(e.what() + std::string(" when parsing multi-res image") + oN));
        }

        if (gi.baseType == Type::invalid) {
            IF_THROW_EXCEPTION(invalid_argument_error("The selected subdatasets of '" + str + "' have different data types when parsing multi-res image" + oN))
                    << boost::errinfo_file_name(filename);
        }
    }
    return in;
}


namespace {

struct MaskFromBits {
    std::vector<int> bits;
    imagefusion::IntervalSet const& validSet;
    imagefusion::ConstImage const& img;

    template<imagefusion::Type basetype>
    imagefusion::Image operator()() {
        using imgval_t = typename imagefusion::DataType<basetype>::base_type;

        int nbits = bits.size();
    //     unsigned int maxVal = std::pow(2, nbits);
        std::string errAdd = "bits 0 - " + std::to_string(sizeof(imgval_t) * 8 - 1) + " can be extracted";// and with the number of bits you selected the resulting value range is 0 - " + std::to_string(maxVal);
        if (!bits.empty() && bits.back() >= static_cast<int>(sizeof(imgval_t) * 8))
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("You are trying to extract bits up to bit " + std::to_string(bits.back()) + " although only " + errAdd));

        imgval_t andBitMask = 0;
        for (int b : bits)
            andBitMask |= 1 << b;

        bool isContiguous = true;
        for (int p = 0; p < nbits - 1; ++p) {
            if (bits.at(p + 1) != bits.at(p) + 1) {
                isContiguous = false;
                break;
            }
        }

        unsigned int width = img.width();
        unsigned int height = img.height();
        unsigned int nchans = img.channels();
        imagefusion::Image mask(width, height, imagefusion::getFullType(imagefusion::Type::uint8, nchans));
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                for (unsigned int c = 0; c < nchans; ++c) {
                    imgval_t qlVal = img.at<imgval_t>(x, y, c);

                    imgval_t shiftedVal = 0;
                    if (bits.empty())
                        shiftedVal = qlVal;
                    else if (isContiguous)
                        shiftedVal = (qlVal & andBitMask) >> bits.front();
                    else { // collect each bit and shift it to the front positions
                        qlVal &= andBitMask;
                        for (int p = 0; p < nbits; ++p) {
                            int b = bits.at(p);
                            imgval_t shiftedBit = qlVal & (1 << b);
                            shiftedBit >>= (b - p);
                            shiftedVal |= shiftedBit;
                        }
                    }

                    bool doInterp = contains(validSet, shiftedVal); // from boost
                    mask.setBoolAt(x, y, c, doInterp);
                }
            }
        }
        return mask;
    }
};

} /* anonymous namespace */


const std::vector<Descriptor> Parse::usageMask {
Descriptor::text("<msk> must have the form '-f <file> [-c <rect>] [-l <num-list>] [-b <num-list>] [--valid-ranges=<range-list>] [--invalid-ranges=<range-list>] [--disable-use-color-table]', where the arguments can have an arbitrary order. "
            "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"),
{ "FILE",   "",       "f","file",                    ArgChecker::NonEmpty,    "  -f <file>, \t--file=<file>, \tImage file path (string)." },
{ "LAYERS", "",       "l","layers",                  ArgChecker::Vector<int>, "  -l <num-list>, \t--layers=<num-list>, \tOptional. Load only specified channels or layers. Hereby a 0 means the first channel.\v"
                                                                                                                        "<num-list> must have the format '(<num> [<num> ...])' without commas inbetween or just '<num>'" },
{ "CROP",   "",       "c","crop",                    ArgChecker::Rectangle,   "  -c <rect>, \t--crop=<rect>, \tOptional. Specifies the crop window, where the image will be read. A zero width or height means full width or height, respectively.\v"
                                                                                                              "<rect> requires all of the following arguments:\v"
                                                                                                              " -x <num>                 x start\v"
                                                                                                              " -y <num>                 y start\v"
                                                                                                              " -w <num>, --width=<num>  width\v"
                                                                                                              " -h <num>, --height=<num> height\n" },
{ "COLTAB", "DISABLE","", "disable-use-color-table", ArgChecker::None,        "  \t--disable-use-color-table, \tFor images with color table, do not convert indexed colors." },
{ "COLTAB", "ENABLE", "", "enable-use-color-table",  ArgChecker::None,        "  \t--enable-use-color-table, \tFor images with color table, convert indexed colors. Default." },
{ "RANGE",  "VALID",  "", "valid-ranges",            ArgChecker::IntervalSet, "  \t--valid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as valid (true; 255). "
                                                                                                            "Can be combined with --invalid-ranges." },
{ "RANGE",  "INVALID","", "invalid-ranges",          ArgChecker::IntervalSet, "  \t--invalid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as invalid (false; 0). "
                                                                                                                  "Can be combined with --valid-ranges." },
{ "BITS",   "",       "b","extract-bits",            ArgChecker::Vector<int>, "  -b <num-list>, \t--extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted "
                                                                                                                             "from the quality layer image and then shifted to the least significant positions. By default all bits will be used." },
Descriptor::text("Example: ... --<option>=(--file='test image.tif') ... \n"
                 "         ... --<option>=(-f \"test.tif\"  -b 7 -b 6 -b 0  --interp-ranges=[1,7]  --non-interp-ranges=[3,3]) ...")
};


imagefusion::Image Parse::Mask(std::string const& str, std::string const& optName, bool readImage, std::vector<Descriptor> const& usageMask) {
    auto maskOptions = imagefusion::option::OptionParser::parse(usageMask, str);

    std::vector<int> bits;
    for (auto const& opt : maskOptions["BITS"]) {
        std::vector<int> b = Parse::Vector<int>(opt.arg);
        bits.insert(bits.end(), b.begin(), b.end());
    }
    std::sort(bits.begin(), bits.end());
    if (!bits.empty() && bits.front() < 0)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("You are trying to extract negative bit positions (" + std::to_string(bits.front()) + "), which does not make any sense"));

    imagefusion::IntervalSet validSet;
    bool hasValidRanges = !maskOptions["RANGE"].empty();
    if (!hasValidRanges || maskOptions["RANGE"].front().prop() == "INVALID")
        // if first range is invalid, start with all values valid
        validSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    if (!hasValidRanges)
        validSet -= imagefusion::Interval::closed(0, 0);
    for (auto const& opt : maskOptions["RANGE"]) {
        imagefusion::IntervalSet set = Parse::IntervalSet(opt.arg);
        if (opt.prop() == "VALID")
            validSet += set;
        else
            validSet -= set;
    }

    imagefusion::Image img = Image(str, optName, readImage, usageMask);
    if (!readImage)
        return img;

    if (!isIntegerType(img.type()))
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Mask images represent bitmasks. So only integer types are supported. " + Parse::ImageFileName(str) + " is of type " + to_string(img.type()) + "."))
            << boost::errinfo_file_name(Parse::ImageFileName(str)) << imagefusion::errinfo_image_type(img.type());

    try {
        using imagefusion::Type;
        img = imagefusion::CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8, Type::int8, Type::uint16, Type::int16, Type::int32>::run(
                    MaskFromBits{bits, validSet, img}, img.basetype());
    }
    catch (imagefusion::invalid_argument_error& e) {
        std::string fn = Parse::ImageFileName(str);
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(e.what() + std::string(", where the image is ") + fn + " and has the type " + to_string(img.type())))
            << boost::errinfo_file_name(fn) << imagefusion::errinfo_image_type(img.type());
    }
    return img;
}


const std::vector<Descriptor> Parse::usageMRMask {
Descriptor::text("<msk> must have the form '-f <file> -d <num> -t <tag> [-c <rect>] [-l <num-list>] [-b <num-list>] [--valid-ranges=<range-list>] [--invalid-ranges=<range-list>] [--disable-use-color-table]', where the arguments can have an arbitrary order. "
            "The option --enable-use-color-table is not mentioned but by default added and can be overriden by --disable-use-color-table to prevent conversion of indexed colors.\n"),
{ "FILE",   "",       "f","file",                    ArgChecker::NonEmpty,    "  -f <file>, \t--file=<file>, \tImage file path (string)." },
{ "DATE",   "",       "d","date",                    ArgChecker::Int,         "  -d <num>, \t--date=<num>, \tDate (number)." },
{ "TAG",    "",       "t","tag",                     ArgChecker::NonEmpty,    "  -t <tag>, \t--tag=<tag>, \tResolution tag (string)." },
{ "LAYERS", "",       "l","layers",                  ArgChecker::Vector<int>, "  -l <num-list>, \t--layers=<num-list>, \tOptional. Load only specified channels or layers. Hereby a 0 means the first channel.\v"
                                                                                                                        "<num-list> must have the format '(<num> [<num> ...])' without commas inbetween or just '<num>'" },
{ "CROP",   "",       "c","crop",                    ArgChecker::Rectangle,   "  -c <rect>, \t--crop=<rect>, \tOptional. Specifies the crop window, where the image will be read. A zero width or height means full width or height, respectively.\v"
                                                                                                              "<rect> requires all of the following arguments:\v"
                                                                                                              " -x <num>                 x start\v"
                                                                                                              " -y <num>                 y start\v"
                                                                                                              " -w <num>, --width=<num>  width\v"
                                                                                                              " -h <num>, --height=<num> height\n" },
{ "COLTAB", "DISABLE","", "disable-use-color-table", ArgChecker::None,        "  \t--disable-use-color-table, \tFor images with color table, do not convert indexed colors." },
{ "COLTAB", "ENABLE", "", "enable-use-color-table",  ArgChecker::None,        "  \t--enable-use-color-table, \tFor images with color table, convert indexed colors. Default." },
{ "RANGE",  "VALID",  "", "valid-ranges",            ArgChecker::IntervalSet, "  \t--valid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as valid (true; 255). "
                                                                                                            "Can be combined with --invalid-ranges." },
{ "RANGE",  "INVALID","", "invalid-ranges",          ArgChecker::IntervalSet, "  \t--invalid-ranges=<range-list> \tSpecifies the ranges of the shifted value (see --extract-bits) that should mark the location as invalid (false; 0). "
                                                                                                                  "Can be combined with --valid-ranges." },
{ "BITS",   "",       "b","extract-bits",            ArgChecker::Vector<int>, "  -b <num-list>, \t--extract-bits=<num-list> \tOptional. Specifies the bits to use. The selected bits will be sorted (so the order is irrelevant), extracted "
                                                                                                                             "from the quality layer image and then shifted to the least significant positions. By default all bits will be used." },
Descriptor::text("Example: ... --<option>=(--file='test image.tif')  -d 0  -t HIGH  ... \n"
                 "         ... --<option>=(-f \"test.tif\"  -d 0  -t HIGH  -b 7 -b 6 -b 0  --interp-ranges=[1,7]  --non-interp-ranges=[3,3]) ...")
};


ImageInput Parse::MRMask(std::string const& str, std::string const& optName, bool readImage, bool isDateOpt, bool isTagOpt, std::vector<Descriptor> const& usageMask) {
    auto maskOptions = imagefusion::option::OptionParser::parse(usageMask, str);

    std::vector<int> bits;
    for (auto const& opt : maskOptions["BITS"]) {
        std::vector<int> b = Parse::Vector<int>(opt.arg);
        bits.insert(bits.end(), b.begin(), b.end());
    }
    std::sort(bits.begin(), bits.end());
    if (!bits.empty() && bits.front() < 0)
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("You are trying to extract negative bit positions (" + std::to_string(bits.front()) + "), which does not make any sense"));

    imagefusion::IntervalSet validSet;
    bool hasValidRanges = !maskOptions["RANGE"].empty();
    if (!hasValidRanges || maskOptions["RANGE"].front().prop() == "INVALID")
        // if first range is invalid, start with all values valid
        validSet += imagefusion::Interval::closed(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    if (!hasValidRanges)
        validSet -= imagefusion::Interval::closed(0, 0);
    for (auto const& opt : maskOptions["RANGE"]) {
        imagefusion::IntervalSet set = Parse::IntervalSet(opt.arg);
        if (opt.prop() == "VALID")
            validSet += set;
        else
            validSet -= set;
    }

    ImageInput in = MRImage(str, optName, readImage, isDateOpt, isTagOpt, usageMask);
    if (!readImage)
        return in;

    if (!isIntegerType(in.i.type()))
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Mask images represent bitmasks. So only integer types are supported. " + Parse::ImageFileName(str) + " is of type " + to_string(in.i.type()) + "."))
            << boost::errinfo_file_name(Parse::ImageFileName(str)) << imagefusion::errinfo_date(in.date) << imagefusion::errinfo_resolution_tag(in.tag) << imagefusion::errinfo_image_type(in.i.type());

    try {
        using imagefusion::Type;
        in.i = imagefusion::CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8, Type::int8, Type::uint16, Type::int16, Type::int32>::run(
                    MaskFromBits{bits, validSet, in.i}, in.i.basetype());
    }
    catch (imagefusion::invalid_argument_error& e) {
        std::string fn = Parse::ImageFileName(str);
        IF_THROW_EXCEPTION(imagefusion::invalid_argument_error(e.what() + std::string(", where the image is ") + fn + " and has the type " + to_string(in.i.type())))
            << boost::errinfo_file_name(fn) << imagefusion::errinfo_date(in.date) << imagefusion::errinfo_resolution_tag(in.tag) << imagefusion::errinfo_image_type(in.i.type());
    }
    return in;
}


std::string Parse::ImageFileName(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    if (str.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("The argument is empty."));

    std::string filename;
    if (!args["FILE"].empty())
        filename = args["FILE"].back().arg;
    else if (args.nonOptionArgCount() >= 1) {
        // ok no option 'file' found, but maybe the filename is given directly, like --img="FILE -d 0 -t high"
        filename = args.nonOptionArgs.front();
    }

    // check whether GDAL can open str (could be a subdataset)
    std::vector<int> l;
    for (auto opt : args["LAYERS"]) {
        auto addLayers = Parse::Vector<int>(opt.arg);
        l.insert(l.end(), addLayers.begin(), addLayers.end());
    }
    GeoInfo gi{filename}; // in case of a multi-image-file, this may not dive into the subdatasets, since then the filename (of the in-memory virtual dataset) would be empty
    return gi.filename;
}

std::vector<int> Parse::ImageLayers(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    if (args["LAYERS"].empty())
        return {};

    std::vector<int> l;
    for (auto opt : args["LAYERS"]) {
        auto addLayers = Parse::Vector<int>(opt.arg);
        l.insert(l.end(), addLayers.begin(), addLayers.end());
    }
    return l;
}

CoordRectangle Parse::ImageCropRectangle(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    if (!args["CROP"].empty())
        return Parse::CoordRectangle(args["CROP"].back().arg);
    return {};
}

int Parse::ImageDate(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    if (args["DATE"].empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Date argument for multi-res image missing."));

    return Parse::Int(args["DATE"].back().arg);
}

bool Parse::ImageHasDate(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    return !args["DATE"].empty();
}

std::string Parse::ImageTag(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    if (args["TAG"].empty())
        IF_THROW_EXCEPTION(invalid_argument_error("Tag argument for multi-res image missing."));
    return args["TAG"].back().arg;
}

bool Parse::ImageHasTag(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    return !args["TAG"].empty();
}

bool Parse::ImageIgnoreColorTable(std::string const& str) {
    std::vector<std::string> argsTokens = separateArguments(str);
    OptionParser args(usageMRImage);
    args.singleDashLongopt = true;
    args.acceptsOptAfterNonOpts = true;
    args.unknownOptionArgCheck = ArgChecker::None;
    args.parse(argsTokens);

    return !args["COLTAB"].empty() && args["COLTAB"].back().prop() == "DISABLE";
}

ArgStatus ArgChecker::Unknown(const Option& option) {
    IF_THROW_EXCEPTION(invalid_argument_error("Unknown option '" + option.name + "'"));
    return ArgStatus::ILLEGAL;
}

ArgStatus ArgChecker::NonEmpty(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no argument given for option '" + option.name + "'"));
    return ArgStatus::OK;
}

ArgStatus ArgChecker::File(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no filename given for option '" + option.name + "'"));
    
    if (!imagefusion::filesystem::exists(option.arg))
        IF_THROW_EXCEPTION(invalid_argument_error("File '" + option.arg + "' does not exist"))
                << boost::errinfo_file_name(option.arg);

    return ArgStatus::OK;
}

ArgStatus ArgChecker::Int(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no integer argument given for option '" + option.name + "'"));

    Parse::Int(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Float(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no floating point number argument given for option '" + option.name + "'"));

    Parse::Float(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Angle(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no angle argument given for option '" + option.name + "'"));

    Parse::Angle(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::GeoCoord(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no geographic location argument given for option '" + option.name + "'"));

    Parse::GeoCoord(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Type(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no image data type argument given for option '" + option.name + "'"));

    Parse::Type(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Interval(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no interval argument given for option '" + option.name + "'"));

    Parse::Interval(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::IntervalSet(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no interval set argument given for option '" + option.name + "'"));

    Parse::IntervalSet(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Image(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no image input argument given for option '" + option.name + "'"));

    Parse::Image(option.arg, option.name, /*readImage*/ false);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Mask(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no mask image input argument given for option '" + option.name + "'"));

    Parse::Mask(option.arg, option.name, /*readImage*/ false);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Size(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no size argument given for option '" + option.name + "'"));

    Parse::Size(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::SizeSubopts(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no size argument given for option '" + option.name + "'"));

    Parse::SizeSubopts(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::SizeSpecial(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no size argument given for option '" + option.name + "'"));

    Parse::SizeSpecial(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Dimensions(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no dimensions / size argument given for option '" + option.name + "'"));

    Parse::Dimensions(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::DimensionsSubopts(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no dimensions / size argument given for option '" + option.name + "'"));

    Parse::DimensionsSubopts(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::DimensionsSpecial(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no dimensions / size argument given for option '" + option.name + "'"));

    Parse::DimensionsSpecial(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Point(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no point argument given for option '" + option.name + "'"));

    Parse::Point(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::PointSubopts(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no point argument given for option '" + option.name + "'"));

    Parse::PointSubopts(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::PointSpecial(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no point argument given for option '" + option.name + "'"));

    Parse::PointSpecial(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Coordinate(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no coordinate argument given for option '" + option.name + "'"));

    Parse::Coordinate(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::CoordinateSubopts(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no coordinate argument given for option '" + option.name + "'"));

    Parse::CoordinateSubopts(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::CoordinateSpecial(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no coordinate argument given for option '" + option.name + "'"));

    Parse::CoordinateSpecial(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::Rectangle(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no rectangle argument given for option '" + option.name + "'"));

    Parse::Rectangle(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::CoordRectangle(const Option& option) {
    if (option.arg.empty())
        IF_THROW_EXCEPTION(invalid_argument_error("There was no rectangle argument given for option '" + option.name + "'"));

    Parse::CoordRectangle(option.arg, option.name);
    return ArgStatus::OK;
}

ArgStatus ArgChecker::None(const Option&) {
    return ArgStatus::NONE;
}

ArgStatus ArgChecker::Optional(const Option& option) {
    if (!option.name.empty())
        return ArgStatus::OK;
    else
        return ArgStatus::IGNORE;
}




std::istream &operator>>(std::istream &is, ArgumentToken &a) {
    a.data.clear();
    bool is_start = true;
    bool is_escaping = false;
    std::vector<char> q; // quoting chars and level (via size)

    is >> std::noskipws;
    for (char c; is >> c; ) {
//        std::cout << "got " << c << std::endl;
        // trim beginning space
        if (is_start && (std::isspace(c) || a.sep.find(c) != std::string::npos))
            continue;

        if (is_escaping) {
//            std::cout << "escaped " << c << std::endl;
            if (q.empty()) {
                // only expand escapes at level 0
                if (c == 'n')
                    a.data.push_back('\n');
                else if (c == 't')
                    a.data.push_back('\t');
                else
                    a.data.push_back(c);
            }
            else {
                // for higher levels preserve sequence for later
                a.data.push_back('\\');
                a.data.push_back(c);
            }
            is_escaping = false;
            continue;
        }

        if (c == '#') {
            // skip comment until end of line or file
            while (is >> c && c != '\n')
                ;
            continue;
        }


        if (!q.empty() && c == q.back()) {
//            std::cout << "<" << c << ">, decrease level from " << q.size() << " to " << (q.size()-1) << std::endl;
            // quotation end, decrease level
            if (q.size() > 1)
                // for higher levels, preserve quotation for later
                a.data.push_back(c);
            q.pop_back();
            if (q.empty())
                break;
            continue;
        }

        is_start = false;

        if ((q.empty() && (c == '\"' || c == '\'')) || c == '(') {
            // quotation start, increase level
//            std::cout << "<" << c << ">, increase level from " << q.size() << " to " << (q.size()+1) << std::endl;
            if (!q.empty())
                a.data.push_back(c);
            if (c == '(')
                c = ')';
            q.push_back(c);
            continue;
        }

        if (q.empty() && (std::isspace(c) || a.sep.find(c) != std::string::npos))
            // only at level 0 space and separator means: regular argument ends here
            break;

        if (c == '\\') {
//            std::cout << "escaping" << std::endl;
            is_escaping = true;
            continue;
        }

        a.data.push_back(c);
    }

    return is;
}


bool expandOptFiles(std::vector<std::string>& options, std::string const& optName) {
    std::vector<std::string> filesNotOpened;
    bool expandedSomething = false;
    for (unsigned int i = 0; i < options.size(); ++i) {
        std::string const& opt = options[i];
        if (opt.find(optName) == 0) {
            std::string fname;
            if (opt.length() == optName.size()) {
                // separated filename by space, like --option-file FILE. Erase also next argument FILE (before ifstream).
                options.erase(options.begin() + i);
                if (i == options.size()) {
                    filesNotOpened.push_back("No filename given at the end of arguments.");
                    break;
                }
                else
                    fname = options[i];
            }
            else {
                // assigned filename, like --option-file=FILE
                if (opt[optName.size()] == '=' && opt.length() > optName.size() + 1)
                    fname = opt.substr(optName.size() + 1);
                else {
                    filesNotOpened.push_back("filename empty (do not use space before or after '=').");
                    continue;
                }
            }
            options.erase(options.begin() + i);

            std::ifstream file(fname);
            if (!file.is_open())
                filesNotOpened.push_back("Could not open file: " + fname);

            std::vector<std::string> argsFromFile = separateArguments(file);
            options.insert(options.begin() + i, argsFromFile.begin(), argsFromFile.end());
            expandedSomething = true;
        }
    }

    // for (std::string const& s : filesNotOpened)
        //**Commented couts and cerr until a fix is found* std::cerr << "Warning: " << s << std::endl;

//        std::cout << "Arguments from file:" << std::endl;
//        for (std::string const& s : options)
//            std::cout << "<" << s << ">" << std::endl;
    return expandedSomething;
}


void OptionParser::parseBackend(bool gnu, std::vector<Descriptor> const& usage,
                                 std::vector<std::string> args,
                                 int min_abbr_len, bool single_minus_longopt)
{
    using imagefusion::option_impl_detail::streq;
    auto arg = std::begin(args);
    while (arg != std::end(args)) {
        std::string param = *arg;

        if (param.empty()) {
            ++arg;
            continue;
        }

        // in POSIX mode the first non-option argument terminates the option list
        // a lone minus character is a non-option argument
        if (param[0] != '-' || param.size() == 1) {
            if (gnu) {
                nonOptionArgs.push_back(std::move(param));
                ++arg;
                continue;
            }
            else
                break;
        }
        // it holds: param[0] == '-'
        //      and  param.size() >= 2

        // -- terminates the option list. The -- itself is skipped.
        if (param[1] == '-' && param.size() == 2) {
            ++arg;
            break;
        }

        bool handle_short_options = true;
        if (param[1] == '-') { // if --long-option
            param.erase(0, 1); // remove one '-' here and the other in the do-while loop
            handle_short_options = false;
        }

        bool try_single_minus_longopt = single_minus_longopt;
        bool have_more_args = arg + 1 != std::end(args);

        do { // loop over short options in group, for long options the body is executed only once
            param.erase(0, 1); // point at the 1st/next option character
            auto descr = std::end(usage);

            std::string optname;
            std::string optarg;
            bool isSepArg = false;
            /******************** long option **********************/
            if (!handle_short_options || try_single_minus_longopt) {
                descr = std::find_if(std::begin(usage), std::end(usage),
                                     [param] (Descriptor const& d) {
                                         return streq(d.longopt, param); });

                if (descr == std::end(usage) && min_abbr_len > 0) { // if we should try to match abbreviated long options
                    auto descr1 = std::find_if(std::begin(usage), std::end(usage),
                                               [param, min_abbr_len] (Descriptor const& d) {
                                                   return streq(d.longopt, param, min_abbr_len); });
                    if (descr1 != std::end(usage)) {
                        // now test if the match is unambiguous by checking for another match
                        auto descr2 = std::find_if(descr1 + 1, std::end(usage),
                                                   [param, min_abbr_len] (Descriptor const& d) {
                                                       return streq(d.longopt, param, min_abbr_len); });

                        // if there was no second match it's unambiguous, so accept i1 as idx
                        if (descr2 == std::end(usage))
                            descr = descr1;
                    }
                }

                bool search_again_as_short_option = false;
                auto pos = param.find('=');
                if (pos != std::string::npos) {
                    // attached argument
                    optname = param.substr(0, pos);
                    optarg = param.substr(pos + 1);
                }
                else {
                    // possibly detached argument
                    optname = param;
                    if (have_more_args) {
                        optarg = *(arg + 1);
                        isSepArg = true;
                    }

                    // do not accept detached arguments beginning with a dash '-'
                    // this prevents eating short options when collecting unknown long options with optional argument
                    if (!optarg.empty() && optarg[0] == '-') {
                        optarg.clear();
                        isSepArg = false;
                        if (handle_short_options && try_single_minus_longopt && param.size() == 1) {
                            // exception: if this is a single dash long option with a single letter that exists also as short option
                            // this allows e. g. negative numbers as argument, like -x -1, if x is a short AND a long option and single_minus_longopt is true
                            auto short_descr = std::find_if(std::begin(usage), std::end(usage),
                                                           [param] (Descriptor const& d) {
                                                           return d.shortopt.find(param[0]) != std::string::npos; });
                            if (short_descr != std::end(usage))
                                search_again_as_short_option = true;
                        }
                    }
                }

                // if we found something, disable handle_short_options (only relevant if single_minus_longopt)
                if (descr != std::end(usage) && !search_again_as_short_option)
                    handle_short_options = false;

                try_single_minus_longopt = false; // prevent looking for longopt in the middle of shortopt group
            }

            /************************ short option ***********************************/
            if (handle_short_options)
            {
                if (param.empty() || param[0] == '-')
                    break; // end of short option group
                optname = param[0];

                descr = std::find_if(std::begin(usage), std::end(usage),
                                     [optname] (Descriptor const& d) {
                                         return d.shortopt.find(optname.front()) != std::string::npos; });

                if (param.size() == 1) { // if the potential argument is separate
                    if (have_more_args) {
                        optarg = *(arg + 1);
                        isSepArg = true;
                    }

                    // do not accept detached arguments beginning with a double dash '--'
                    if (optarg.size() >= 2 && optarg[0] == '-' && optarg[1] == '-') {
                        optarg.clear();
                        isSepArg = false;
                    }
                }
                else // if the potential argument is attached
                    optarg = param.substr(1);
            }

            Option option;
            CheckArg check;
            if (descr != std::end(usage)) {
                option = Option(&*descr, optname, optarg);
                check = descr->checkArg;
            }
            else { // unknown option
                option = Option(nullptr, optname, optarg);
                check = unknownOptionArgCheck;
            }

            switch (check(option))
            {
            case ArgStatus::ILLEGAL:
                IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("Check of option '" + to_string(option) + "' failed."));
            case ArgStatus::OK:
                // skip one element of the argument vector, if it's a separated argument
                if (isSepArg)
                    ++arg;

                // No further short options are possible after an argument
                handle_short_options = false;

                break;
            case ArgStatus::IGNORE:
            case ArgStatus::NONE:
                option.arg.clear();
                break;
            }

            store(option);

        } while (handle_short_options);

        ++arg;
    } // while

    for (; arg != std::end(args); ++arg) {
        std::string next = *arg;
        if (!next.empty())
            nonOptionArgs.push_back(std::move(next));
    }
}



void printUsageBackend(IStringWriter& write, std::vector<Descriptor> const& usage, int width,
                       int last_column_min_percent, int last_column_own_line_max_percent)
{
    using imagefusion::option_impl_detail::LinePartIterator;
    using imagefusion::option_impl_detail::LineWrapper;
    using imagefusion::option_impl_detail::upmax;
    using imagefusion::option_impl_detail::indent;

    if (width < 1) // protect against nonsense values
        width = getTerminalColumns();

    if (width > 10000) // protect against overflow in the following computation
        width = 10000;

//    std::cout << "Using width: " << width << std::endl;

    int last_column_min_width = ((width * last_column_min_percent) + 50) / 100;
    int last_column_own_line_max_width = ((width * last_column_own_line_max_percent) + 50) / 100;
    if (last_column_own_line_max_width == 0)
        last_column_own_line_max_width = 1;

    LinePartIterator part(usage);
    while (part.nextTable())
    {

        /***************** Determine column widths *******************************/

        constexpr int maxcolumns = 17; // 17 columns are enough for everyone
        int col_width[maxcolumns];
        std::vector<int> last_cols;
        int verylastcol;
        int leftwidth;
        int overlong_column_threshold = 10000;
        do
        {
            // get last columns
            verylastcol = 0;
            part.restartTable();
            while (part.nextRow()) {
                while (part.next()) {
                    if (part.column() < maxcolumns && part.screenLength() > 0) {
                        upmax(verylastcol, part.column());

                        if (last_cols.size() <= static_cast<unsigned int>(part.row()))
                            last_cols.resize(part.row() + 1);
                        upmax(last_cols[part.row()], part.column());
                    }
                }
            }
//            std::cout << "last cols: ";
//            for (int lc : last_cols)
//                std::cout << lc << " ";
//            std::cout << std::endl;

            // get the column widths
            for (int i = 0; i < maxcolumns; ++i)
                col_width[i] = 0;
            part.restartTable();
            while (part.nextRow())
                while (part.next())
                        // We don't let the last cell in a row influence the column width.
                        // If that behaviour is desired the user can add another cell with only a space with "\t " or "\v ".
                        if (part.screenLength() < overlong_column_threshold && ((!last_cols.empty() && part.column() < last_cols[part.row()]) || part.column() == verylastcol))
                            upmax(col_width[part.column()], part.screenLength());
//            std::cout << "col widths: ";
//            for (int cw : col_width)
//                std::cout << cw << " ";
//            std::cout << std::endl;

            /*
            * If the last column doesn't fit on the same
            * line as the other columns, we can fix that by starting it on its own line.
            * However we can't do this for any of the columns 0..verylastcol-1.
            * If their sum exceeds the maximum width we try to fix this by iteratively
            * ignoring the widest line parts in the width determination until
            * we arrive at a series of column widths that fit into one line.
            * The result is a layout where everything is nicely formatted
            * except for a few overlong fragments.
            * */

            leftwidth = 0;
            overlong_column_threshold = 0;
            for (int i = 0; i < verylastcol; ++i)
            {
                leftwidth += col_width[i];
                upmax(overlong_column_threshold, col_width[i]);
            }

        } while (leftwidth > width);


        /**************** Determine tab stops and last column handling **********************/

        int tabstop[maxcolumns];
        tabstop[0] = 0;
        for (int i = 1; i < maxcolumns; ++i)
            tabstop[i] = tabstop[i - 1] + col_width[i - 1];

        int rightwidth = width - tabstop[verylastcol];
        bool print_last_column_on_own_line = false;
        if (rightwidth < last_column_min_width &&  // if we don't have the minimum requested width for the last column
            rightwidth < col_width[verylastcol]    // and there is at least one last cell that requires more than the space available
           )
        {
            print_last_column_on_own_line = true;
            rightwidth = last_column_own_line_max_width;
        }

        // If verylastcol == 0 we must disable print_last_column_on_own_line because
        // otherwise 2 copies of the last (and only) column would be output.
        // Actually this is just defensive programming. It is currently not
        // possible that verylastcol==0 and print_last_column_on_own_line==true
        // at the same time, because verylastcol==0 => tabstop[verylastcol] == 0 =>
        // rightwidth==width => rightwidth>=last_column_min_width  (unless someone passes
        // a bullshit value >100 for last_column_min_percent) => the above if condition
        // is false => print_last_column_on_own_line==false
        if (verylastcol == 0)
            print_last_column_on_own_line = false;

        std::vector<LineWrapper> lineWrapper(verylastcol+1);
        for (int i = 0; i < verylastcol; ++i)
            lineWrapper[i] = LineWrapper(tabstop[i], width);
        lineWrapper[verylastcol] = LineWrapper(width - rightwidth, width);

        /***************** Print out all rows of the table *************************************/

        part.restartTable();
        while (part.nextRow())
        {
            int x = -1;
            bool flushedWithNewLine = false;
            while (part.next())
            {
                if (part.column() > verylastcol)
                    continue; // drop excess columns (can happen if verylastcol == maxcolumns-1)

                if (part.column() == 0)
                {
                    if (x >= 0 && !flushedWithNewLine)
                        write("\n", 1);
                    x = 0;
                }

                indent(write, x, tabstop[part.column()]);
                if (!print_last_column_on_own_line || part.column() != verylastcol) {
                    lineWrapper[part.column()].process(write, part.data(), part.length());
                    x += part.screenLength();
                    if (!lineWrapper[part.column()].buf_empty()) {
                        write("\n", 1);
                        lineWrapper[part.column()].flush(write);
                        x = 0;
                        flushedWithNewLine = true;
                    }
                    else
                        flushedWithNewLine = false;
                }
            } // while

            if (print_last_column_on_own_line)
            {
                part.restartRow();
                while (part.next())
                {
                    if (part.column() == verylastcol)
                    {
                        if (!flushedWithNewLine)
                            write("\n", 1);
                        int _ = 0;
                        indent(write, _, width - rightwidth);
                        lineWrapper[verylastcol].process(write, part.data(), part.length());
                        write("\n", 1);
                        lineWrapper[verylastcol].flush(write);
                        flushedWithNewLine = true;
                    }
                }
            }

            if (!flushedWithNewLine)
                write("\n", 1);
        }
    }
}


} /* namespace option */
} /* namespace imagefusion */

//************************** internal stuff **********************************//

namespace imagefusion {
namespace option_impl_detail {


void indent(imagefusion::option::IStringWriter& write, int& x, int want_x) {
    int indent = want_x - x;
    if (indent < 0)
    {
        write("\n", 1);
        indent = want_x;
    }

    if (indent > 0)
    {
        char space = ' ';
        for (int i = 0; i < indent; ++i)
            write(&space, 1);
        x = want_x;
    }
}

void LinePartIterator::update_length() {
    screenlen = 0;
    for (len = 0; ptr[len] != 0 && ptr[len] != '\v' && ptr[len] != '\t' && ptr[len] != '\n' && ptr[len] != '\f'; ++len)
    {
        ++screenlen;
        unsigned ch = (unsigned char) ptr[len];
        if (ch > 0xC1) // everything <= 0xC1 (yes, even 0xC1 itself) is not a valid UTF-8 start byte
        {
            // int __builtin_clz (unsigned int x)
            // Returns the number of leading 0-bits in x, starting at the most significant bit
            unsigned mask = (unsigned) -1 >> __builtin_clz(ch ^ 0xff);
            ch = ch & mask; // mask out length bits, we don't verify their correctness
            while (((unsigned char) ptr[len + 1] ^ 0x80) <= 0x3F) // while next byte is continuation byte
            {
                ch = (ch << 6) ^ (unsigned char) ptr[len + 1] ^ 0x80; // add continuation to char code
                ++len;
            }
            // ch is the decoded unicode code point
            if (ch >= 0x1100 && isWideChar(ch)) // the test for 0x1100 is here to avoid the function call in the Latin case
                ++screenlen;
        }
    }
}


bool LinePartIterator::nextTable() {
    // If this is NOT the first time nextTable() is called after the constructor,
    // then skip to the next table break (i.e. a Descriptor with help == "\f")
    if (rowdesc != usage_end) {
        while (tablestart != usage_end && (tablestart->help.empty() || tablestart->help.find('\f') == std::string::npos))
            ++tablestart;
        if (tablestart != usage_end)
            ++tablestart;
    }

    // Find the next table after the break (if any)
    while (tablestart != usage_end && !tablestart->help.empty() && tablestart->help.front() == '\f')
        ++tablestart;

    restartTable();
    return tablestart != usage_end;
}

void LinePartIterator::restartTable() {
    rowdesc = tablestart;
    rowstart = tablestart != usage_end ? tablestart->help.data() : nullptr;
    ptr = nullptr;
}

bool LinePartIterator::nextRow() {
    if (ptr == nullptr)
    {
        restartRow();
        current_row = 0;
        return rowstart != nullptr;
    }

    while (*ptr != '\0' && (*ptr != '\n' /*|| *ptr == '\f'*/)) {
        if (*ptr == '\f') // table break
            return false;
        ++ptr;
    }

    if (*ptr == '\0')
    {
        auto next_rowdesc = rowdesc;
        ++next_rowdesc;
        if (next_rowdesc == usage_end || (!next_rowdesc->help.empty() && next_rowdesc->help.front() == '\f')) // table break
            return false;

        ++rowdesc;
        rowstart = rowdesc->help.data();
        current_row += max_line_in_block;
    }
    else // if (*ptr == '\n')
    {
        current_row += max_line_in_block;
        rowstart = ptr + 1;
    }
    ++current_row;

    restartRow();
    return true;
}


bool LinePartIterator::next() {
    if (ptr == nullptr)
        return false;

    if (col == -1)
    {
        col = 0;
        update_length();
        return true;
    }

    ptr += len;
    while (true)
    {
        switch (*ptr)
        {
        case '\v':
            upmax(max_line_in_block, ++line_in_block);
            ++ptr;
            break;
        case '\t':
            if (!hit_target_line) // if previous column did not have the targetline
            { // then "insert" a 0-length part
                update_length();
                hit_target_line = true;
                return true;
            }

            hit_target_line = false;
            line_in_block = 0;
            ++col;
            ++ptr;
            break;
        case '\0':
        case '\n':
            if (!hit_target_line) // if previous column did not have the targetline
            { // then "insert" a 0-length part
                update_length();
                hit_target_line = true;
                return true;
            }

            if (++target_line_in_block > max_line_in_block)
            {
                update_length();
                return false;
            }

            hit_target_line = false;
            line_in_block = 0;
            col = 0;
            ptr = rowstart;
            continue;
        case '\f':
            return false;
        default:
            ++ptr;
            continue;
        } // switch

        if (line_in_block == target_line_in_block)
        {
            update_length();
            hit_target_line = true;
            return true;
        }
    } // while
}


void LineWrapper::output(imagefusion::option::IStringWriter& write, const char* data, int len) {
    if (buf_full())
        write_one_line(write);

    buf_store(data, len);
}

void LineWrapper::write_one_line(imagefusion::option::IStringWriter& write) {
    if (wrote_something) // if we already wrote something, we need to start a new line
    {
        write("\n", 1);
        int _ = 0;
        indent(write, _, x);
    }

    if (!buf_empty())
    {
        buf_next();
        write(datbuf[tail], lenbuf[tail]);
    }

    wrote_something = true;
}


void LineWrapper::flush(imagefusion::option::IStringWriter& write) {
    if (buf_empty())
        return;
    int _ = 0;
    indent(write, _, x);
    wrote_something = false;
    while (!buf_empty())
        write_one_line(write);
    write("\n", 1);
}


void LineWrapper::process(imagefusion::option::IStringWriter& write, const char* data, int len) {
    wrote_something = false;

    while (len > 0)
    {
        if (len <= width) // quick test that works because utf8width <= len (all wide chars have at least 2 bytes)
        {
            output(write, data, len);
            len = 0;
        }
        else // if (len > width)  it's possible (but not guaranteed) that utf8len > width
        {
            int utf8width = 0;
            int maxi = 0;
            while (maxi < len && utf8width < width)
            {
                int charbytes = 1;
                unsigned ch = (unsigned char) data[maxi];
                if (ch > 0xC1) // everything <= 0xC1 (yes, even 0xC1 itself) is not a valid UTF-8 start byte
                {
                    // int __builtin_clz (unsigned int x)
                    // Returns the number of leading 0-bits in x, starting at the most significant bit
                    unsigned mask = (unsigned) -1 >> __builtin_clz(ch ^ 0xff);
                    ch = ch & mask; // mask out length bits, we don't verify their correctness
                    while ((maxi + charbytes < len) && //
                           (((unsigned char) data[maxi + charbytes] ^ 0x80) <= 0x3F)) // while next byte is continuation byte
                    {
                        ch = (ch << 6) ^ (unsigned char) data[maxi + charbytes] ^ 0x80; // add continuation to char code
                        ++charbytes;
                    }
                    // ch is the decoded unicode code point
                    if (ch >= 0x1100 && isWideChar(ch)) // the test for 0x1100 is here to avoid the function call in the Latin case
                    {
                        if (utf8width + 2 > width)
                            break;
                        ++utf8width;
                    }
                }
                ++utf8width;
                maxi += charbytes;
            }

            // data[maxi-1] is the last byte of the UTF-8 sequence of the last character that fits
            // onto the 1st line. If maxi == len, all characters fit on the line.

            if (maxi == len)
            {
                output(write, data, len);
                len = 0;
            }
            else // if (maxi < len)  at least 1 character (data[maxi] that is) doesn't fit on the line
            {
                int i;
                for (i = maxi; i >= 0; --i)
                    if (data[i] == ' ')
                        break;

                if (i >= 0)
                {
                    output(write, data, i);
                    data += i + 1;
                    len -= i + 1;
                }
                else // did not find a space to split at => split before data[maxi]
                { // data[maxi] is always the beginning of a character, never a continuation byte
                    output(write, data, maxi);
                    data += maxi;
                    len -= maxi;
                }
            }
        }
    }
    if (!wrote_something) // if we didn't already write something to make space in the buffer
        write_one_line(write); // write at most one line of actual output
}


} /* namespace option_impl_detail */
} /* namespace imagefusion */


// getTerminalColumns is here, since at least windows.h defines too many macros, which lead to name clashes
#if BOOST_OS_LINUX && !BOOST_PLAT_MINGW
    #include <sys/ioctl.h>
    int imagefusion::option::getTerminalColumns() {
        winsize w;
        ioctl(0, TIOCGWINSZ, &w);
        return w.ws_col < 1 ? 120 : w.ws_col;
    }
#elif BOOST_OS_WINDOWS || BOOST_PLAT_MINGW
    #include <windows.h>
    int imagefusion::option::getTerminalColumns() {
        CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
        if(!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo))
            return 120;
        return csbiInfo.dwSize.X - 1;
    }
#else
    int imagefusion::option::getTerminalColumns() {
        return 120;
    }
#endif


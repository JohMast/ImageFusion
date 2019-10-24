#pragma once

#include "optionparser.h"

#include <vector>


extern const char* usageQLText;

extern std::vector<imagefusion::option::Descriptor> usageQL;

struct Parse : public imagefusion::option::Parse {
    static imagefusion::option::ImageInput QL(std::string const& str, std::string const& optName = "", bool readImage = true, bool isDateOpt = false, bool isTagOpt = false);
};

struct ArgChecker : public imagefusion::option::ArgChecker {
    template<bool isDateOpt = false, bool isTagOpt = false>
    static imagefusion::option::ArgStatus QL(const imagefusion::option::Option& option) {
        if (option.arg.empty())
            IF_THROW_EXCEPTION(imagefusion::invalid_argument_error("There was no image input quality layer argument given for option '" + option.name + "'"));

        Parse::QL(option.arg, option.name, /*readImage*/ false, isDateOpt, isTagOpt);
        return imagefusion::option::ArgStatus::OK;
    }
};


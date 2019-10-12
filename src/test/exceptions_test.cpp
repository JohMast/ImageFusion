#include <boost/test/unit_test.hpp>

#include "exceptions.h"
#include "MultiResImages.h"
#include "Type.h"
#include "Image.h"

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(exceptions_suite)

// test string conversions
BOOST_AUTO_TEST_CASE(to_string)
{
    try {
        IF_THROW_EXCEPTION(runtime_error("A reason."))
                << errinfo_image_type(Type::float32)
                << errinfo_size(Size{0,10})
                << errinfo_resolution_tag("resolution tag")
                << errinfo_date(5);
    }
    catch (runtime_error const& e) {
        std::string s;
        BOOST_CHECK_NO_THROW(s = boost::diagnostic_information(e));
//        std::cout << s << std::endl;
    }
}

BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

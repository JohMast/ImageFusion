#include "utils_common.h"

#include <map>
#include <vector>
#include <string>

#include "MultiResImages.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(utils_common_suite)

using namespace imagefusion;

bool compareJobs(std::map<std::vector<int>, std::vector<int>> const& j1, std::map<std::vector<int>, std::vector<int>> const& j2) {
    auto it1 = j1.begin();
    auto it2 = j2.begin();
    auto it_end1 = j1.end();
    auto it_end2 = j2.end();

    while (it1 != it_end1 && it2 != it_end2) {
        std::vector<int> const& pairDates1 = it1->first;
        std::vector<int> const& pairDates2 = it2->first;
        BOOST_CHECK_EQUAL_COLLECTIONS(pairDates1.begin(), pairDates1.end(), pairDates2.begin(), pairDates2.end());

        std::vector<int> const& predDates1 = it1->second;
        std::vector<int> const& predDates2 = it2->second;
        BOOST_CHECK_EQUAL_COLLECTIONS(predDates1.begin(), predDates1.end(), predDates2.begin(), predDates2.end());
        ++it1;
        ++it2;
    }
    return it1 == it_end1 && it2 == it_end2;
}

// test making jobs
BOOST_AUTO_TEST_CASE(job_creation)
{
    std::map<std::vector<int>, std::vector<int>> jobs, expected;
    imagefusion::MultiResCollection<std::string> mri;
    std::string h = "h";
    std::string l = "l";

    // test creation of jobs with double pair mode and leaving single pair dates
    // 1 2 3 (4) 5 6 (7) 8 9 (10) 11 12 13
    // expected:
    // (4) 1 2 3
    // (4) 5 6 (7)
    // (7) 8 9 (10)
    // (10) 11 12 13

    mri.set(l,  1, "");
    mri.set(l,  2, "");
    mri.set(l,  3, "");
    mri.set(l,  4, "");
    mri.set(l,  5, "");
    mri.set(l,  6, "");
    mri.set(l,  7, "");
    mri.set(l,  8, "");
    mri.set(l,  9, "");
    mri.set(l, 10, "");
    mri.set(l, 11, "");
    mri.set(l, 12, "");
    mri.set(l, 13, "");
    mri.set(h,  4, "");
    mri.set(h,  7, "");
    mri.set(h, 10, "");
    auto wrapped = helpers::parseJobs(mri, /*minPairs*/ 1, /*doRemovePredDatesWithOnePair*/ false, /*doUseSinglePairMode*/ false);

    expected.insert(std::make_pair(std::vector<int>{4},
                                   std::vector<int>{1, 2, 3}));
    expected.insert(std::make_pair(std::vector<int>{4, 7},
                                   std::vector<int>{5, 6}));
    expected.insert(std::make_pair(std::vector<int>{7, 10},
                                   std::vector<int>{8, 9}));
    expected.insert(std::make_pair(std::vector<int>{10},
                                   std::vector<int>{11, 12, 13}));

    BOOST_CHECK_MESSAGE(compareJobs(wrapped.jobs, expected), "Number of jobs do not match. Settings: single pairs not removed, double pair mode.");
    expected.clear();


    // test creation of jobs with double pair mode and removing single pair dates
    // 1 2 3 (4) 5 6 (7) 8 9 (10) 11 12 13
    // expected:
    // (4) 5 6 (7)
    // (7) 8 9 (10)
    wrapped = helpers::parseJobs(mri, /*minPairs*/ 1, /*doRemovePredDatesWithOnePair*/ true, /*doUseSinglePairMode*/ false);

    expected.insert(std::make_pair(std::vector<int>{4, 7},
                                   std::vector<int>{5, 6}));
    expected.insert(std::make_pair(std::vector<int>{7, 10},
                                   std::vector<int>{8, 9}));

    BOOST_CHECK_MESSAGE(compareJobs(wrapped.jobs, expected), "Number of jobs do not match. Settings: single pairs removed, double pair mode.");
}

BOOST_AUTO_TEST_SUITE_END()

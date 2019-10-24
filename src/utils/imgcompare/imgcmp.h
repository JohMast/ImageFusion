#pragma once

#include <limits>
#include <numeric>
#include <utility>

#include <boost/filesystem.hpp>

#include <opencv2/opencv.hpp>

#include "Type.h"
#include "imagefusion.h"
#include "Image.h"
#include "optionparser.h"

// annonymous namespace is important for test configuration
// all functions are only defined for the corresponding test
namespace {

/**
 * @brief Check if two floating point numbers are close with an absolute and a relative tolerance
 * @param x is a number.
 * @param y is a number.
 *
 * @param reltol is the relative tolerance. By default `reltol` is \f$ 10 \, \varepsilon \f$.
 *
 * @param abstol is the absolute tolerance. By default `abstol` is \f$ 100 \, \varepsilon \f$.
 *
 * Checks first whether
 * \f[
 *      |x - y| <= t_{\text{abs}}
 * \f]
 * where \f$ t_{\text{abs}} \f$ is `abstol`. This catches cases near 0. Then checks whether
 * \f[
 *      |x - y| \le \max \{ |x|, |y| \}} \cdot t_{\text{rel}
 * \f]
 * where \f$ t_{\text{rel}} \f$ is `reltol`. See this
 * [blog post](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/)
 * for rationale.
 *
 * @return true if the numbers are equal up to the relative tolerance, false otherwise.
 */
bool areClose(double x, double y,
              double reltol = 10  * std::numeric_limits<double>::epsilon(),
              double abstol = 100 * std::numeric_limits<double>::epsilon())
{
    double diff = std::abs(x - y);
    if (diff <= abstol)
        return true;
    return diff <= std::max(std::abs(x), std::abs(y)) * reltol;
}

std::pair<std::string, std::string> splitToFileBaseAndExtension(std::string const& filename) {
    boost::filesystem::path p = filename;
    std::string stem = p.stem().string();        // "test.txt" --> "test"
    std::string ext  = p.extension().string();   // "test.txt" --> ".txt"
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // return extension only, if something comes before (otherwise its just a hidden file, like ".bashrc")
    if (stem.empty())
        std::swap(ext, stem);
    return {stem, ext};
}

/**
 * @brief The Stats struct contains some statistical values, which are printed in imgcompare.
 */
struct Stats {
    // used for single and two channel mode
    int validPixels;
    double mean;
    double stddev;
    double min;
    double max;
    int minCount;
    int maxCount;
    imagefusion::Point minLoc;
    imagefusion::Point maxLoc;

    // used only for two channel mode
    double nonzeros;
    double aad;
    double rmse;
};

/**
 * @brief Get some statistical values
 * @param img is the single or multi channel image to analyze
 *
 * @param mask is the single or multi channel mask (uint8) with 0 values, where `img` should be
 * ignored for calculating min, max, mean, std deviation aad and rmse. For the number of non-zeros
 * the mask is ignored, since it is assumed that `img` is set to 0 at invalid locations.
 *
 * This will figure out the minimum and maximum values with their first found locations, mean value
 * and standard deviation, the number of non-zeros and some norms that can be used for the average
 * absolute difference (AAD) and the root mean square error (RMSE). This is done for all channels
 * separately.
 *
 * @return statistical values, one Stats object for each channel.
 */
std::vector<Stats> computeStats(imagefusion::ConstImage const& img, imagefusion::ConstImage const& mask = {}) {
    std::vector<Stats> allStats;
    std::vector<imagefusion::Image> img_layers = img.split();
    std::vector<imagefusion::Image> mask_layers;
    if (mask.channels() > 1)
        mask_layers = mask.split();
    for (unsigned int c = 0; c < img_layers.size(); ++c) {
        cv::Mat const& i = img_layers[c].cvMat();
        cv::Mat const& m = mask.channels() > 1 ? mask_layers[c].cvMat() : mask.cvMat();

        Stats stats;
        cv::minMaxLoc(i, &stats.min, &stats.max, &stats.minLoc, &stats.maxLoc, m);

        cv::Mat temp = i == stats.min;
        if (!m.empty())
            temp.setTo(cv::Scalar::all(0), ~m);
        stats.minCount = cv::countNonZero(temp);

        temp = i == stats.max;
        if (!m.empty())
            temp.setTo(cv::Scalar::all(0), ~m);
        stats.maxCount = cv::countNonZero(temp);

        stats.validPixels = i.size().area();

        if (!m.empty())
            stats.validPixels = cv::countNonZero(m);

        cv::Scalar mean_val, std_dev;
        cv::meanStdDev(i, mean_val, std_dev, m);
        stats.mean   = mean_val[0]; // can be negative for single image mode (aad cannot, so this is different)
        stats.stddev = std_dev[0];

        stats.aad  = cv::norm(i, cv::NORM_L1, m);
        stats.rmse = cv::norm(i, cv::NORM_L2, m);

        // nonzeros does not need a mask, since the masked out values in diff (here: i) have been set to 0 already!
        stats.nonzeros = cv::countNonZero(i);

        allStats.push_back(stats);
    }

    return allStats;
}


/**
 * @brief Print a Pixel
 * @param i is the image
 * @param x is the x coordinate
 * @param y is the y coordinate
 * @param add1 is the string to add between "Pixel " and "at [...]"
 * @param add2 is the string to add after "): ". For example space.
 */
void printPixel(imagefusion::ConstImage const& i, int x, int y, std::string add1 = "", std::string add2 = "") {
    std::cout << "Pixel " << add1 << "at (" << x << ", " << y << "): " << add2;
    if (i.channels() > 1)
        std::cout << "[";
    for (unsigned int c = 0; c < i.channels(); ++c) {
        std::cout << i.doubleAt(x, y, c);
        if (c + 1 < i.channels())
            std::cout << ", ";
    }
    if (i.channels() > 1)
        std::cout << "]";
    std::cout << std::endl;
}


/**
 * @brief Check if the first image fits into the second (by its size)
 * @param i0 is the first image.
 * @param i1 is the second image.
 *
 * Will exit if the width of one image is smaller than of the other, but for the height the other
 * way round.
 *
 * @return true if width and height of the first image are smaller the the ones of the second
 * image, false if width and height of the second image are smaller than the ones of the first
 * image.
 */
bool isFirstSmaller(imagefusion::Image& i0, imagefusion::Image& i1) {
    if ((i0.width() < i1.width() && i0.height() > i1.height()) ||
        (i0.width() > i1.width() && i0.height() < i1.height()))
    {
        std::cerr << "The images have incompatible sizes: "
                  << i0.size() << " and " << i1.size()
                  << ". None of them fits into the other." << std::endl;
        std::exit(1);
    }

    return i0.width() < i1.width();
}


/**
 * @brief This returns markings where the strings are different
 * @param s1 is the first string.
 * @param s2 is the second string.
 *
 * This uses the Levenshtein distance (edit distance) algorithm to find the char sequences that are
 * different for both strings. For example:
 *
 *     123MM456iiiiii789
 *     |||  |||      ||| << This marks the differences
 *     abcMMdefiiiiiighi
 *
 *     fineImageFile.tif
 *     |||
 *     |||||||||
 *     verycoarseImageFile.tif
 *
 *     000common1234shared
 *     |||      ||||
 *           |||      |
 *     commonABCsharedD
 * @return array with both boolean vectors of differences (true means different char)
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Levenshtein_distance)
 */
std::array<std::vector<bool>, 2> findStringDiffs(const std::string& s1, const std::string& s2)
{
    // fill table of edit operations
    //  * diagonal direction means substitute, +1 for different chars, +0 for same chars
    //  * right means first word has additional char, +1
    //  * down means second word has additional char, +1
    /* Example: "sitting", "kitten"
     *     s i t t i n g
     *   0 1 2 3 4 5 6 7
     *    \
     * k 1 1 2 3 4 5 6 7
     *      \
     * i 2 2 1 2 3 4 5 6
     *        \
     * t 3 3 2 1 2 3 4 5
     *          \
     * t 4 4 3 2 1 2 3 4
     *            \
     * e 5 5 4 3 2 2 3 4
     *              \
     * n 6 6 5 4 3 3 2-3
     */
    const std::size_t len1 = s1.size();
    const std::size_t len2 = s2.size();
    std::vector<std::vector<unsigned int>> d(len1 + 1, std::vector<unsigned int>(len2 + 1));

    d[0][0] = 0;
    for (unsigned int i = 1; i <= len1; ++i)
        d[i][0] = i;
    for (unsigned int j = 1; j <= len2; ++j)
        d[0][j] = j;

    for(unsigned int i = 1; i <= len1; ++i)
        for(unsigned int j = 1; j <= len2; ++j)
            d[i][j] = std::min({ d[i - 1][j] + 1,
                                 d[i][j - 1] + 1,
                                 d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1) });

    // print the table (without shortest path)
//    std::cout << "     ";
//    for (auto c : s1)
//        std::cout << c << "  ";
//    std::cout << std::endl;
//    std::string s = " " + s2 + " ";
//    for (unsigned int j = 0; j < len2 + 1; ++j) {
//        std::cout << s[j];
//        for (unsigned int i = 0; i < len1 + 1; ++i)
//            std::cout << std::setw(2) << d[i][j] << " ";
//        std::cout << std::endl;
//    }

    // traverse the table from bottom right through the minimum values and note differences
    std::vector<bool> diff1(len1);
    std::vector<bool> diff2(len2);
    int val = d[len1][len2];
    int i = len1;
    int j = len2;
    int left = val + 1;
    int above = val + 1;
    int aboveleft = val + 1;
    while (val != 0) {
        if (j > 0)
            left = d[i][j-1];
        if (i > 0)
            above = d[i-1][j];
        if (i > 0 && j > 0)
            aboveleft = d[i-1][j-1];

        if (aboveleft < val) {
            diff1[--i] = true;
            diff2[--j] = true;
        }
        else if (above < val) {
            diff1[--i] = true;
        }
        else if (left < val) {
            diff2[--j] = true;
        }
        else {
            --i;
            --j;
            continue;
        }
        --val;
    }

    // print differences
//    std::cout << s1 << std::endl;
//    for (bool b : diff1)
//        std::cout << (b ? "|" : " ");
//    std::cout << std::endl;

//    std::cout << s2 << std::endl;
//    for (bool b : diff2)
//        std::cout << (b ? "|" : " ");
//    std::cout << std::endl;

    return {diff1, diff2};
}


/**
 * @brief The CandidateFactory class generates abbreviated strings in a spcific order
 *
 * A candidate factory is initialized with a string and a boolean vector that marks chars, which
 * should be preserved and not abbreviated if possible. Such a factory generates one candidate at a
 * time in a specific order. The first candidates abbreviate just one non-preserveable character.
 * Then the number of non-preservable characters increases. When all possible combinations to
 * replace non-preservable characters have been tried, also one preservable character is replaced
 * and again all combinations tried. The number of replaced preservable characters also increases.
 * Abbreviation means that one sequence of chars is replaced by an ellipsis ('...'). Example: The
 * string 'MM456iiiiii' would generate 54 candidates. With '456' as preserverd characters the order
 * would be as follows:
 *
 *     MM456iiiiii
 *       |||       <-- markers for preservable characters
 *
 *     Candidates:
 *       1. ...M456iiiiii    (1 char replaced, 1 common, 0 preservable)
 *       2. M...456iiiiii    (1 char replaced, 1 common, 0 preservable)
 *       3. MM456...iiiii    (1 char replaced, 1 common, 0 preservable)
 *       4. MM456i...iiii    (1 char replaced, 1 common, 0 preservable)
 *       5. MM456ii...iii    (1 char replaced, 1 common, 0 preservable)
 *       6. MM456iii...ii    (1 char replaced, 1 common, 0 preservable)
 *       7. MM456iiii...i    (1 char replaced, 1 common, 0 preservable)
 *       8. MM456iiiii...    (1 char replaced, 1 common, 0 preservable)
 *       9. ...456iiiiii     (2 char replaced, 2 common, 0 preservable)
 *      10. MM456...iiii     (2 char replaced, 2 common, 0 preservable)
 *      11. MM456i...iii     (2 char replaced, 2 common, 0 preservable)
 *      12. MM456ii...ii     (2 char replaced, 2 common, 0 preservable)
 *      13. MM456iii...i     (2 char replaced, 2 common, 0 preservable)
 *      14. MM456iiii...     (2 char replaced, 2 common, 0 preservable)
 *      15. MM456...iii      (3 char replaced, 3 common, 0 preservable)
 *      16. MM456i...ii      (3 char replaced, 3 common, 0 preservable)
 *      17. MM456ii...i      (3 char replaced, 3 common, 0 preservable)
 *      18. MM456iii...      (3 char replaced, 3 common, 0 preservable)
 *      19. MM456...ii       (4 char replaced, 4 common, 0 preservable)
 *      20. MM456i...i       (4 char replaced, 4 common, 0 preservable)
 *      21. MM456ii...       (4 char replaced, 4 common, 0 preservable)
 *      22. MM456...i        (5 char replaced, 5 common, 0 preservable)
 *      23. MM456i...        (5 char replaced, 5 common, 0 preservable)
 *      24. MM456...         (6 char replaced, 6 common, 0 preservable)
 *      25. MM...56iiiiii    (1 char replaced, 0 common, 1 preservable)
 *      26. MM4...6iiiiii    (1 char replaced, 0 common, 1 preservable)
 *      27. MM45...iiiiii    (1 char replaced, 0 common, 1 preservable)
 *      28. M...56iiiiii     (2 char replaced, 1 common, 1 preservable)
 *      29. MM45...iiiii     (2 char replaced, 2 common, 1 preservable)
 *      30. ...56iiiiii      (3 char replaced, 3 common, 1 preservable)
 *      31. MM45...iiii      (3 char replaced, 4 common, 1 preservable)
 *      32. MM45...iii       (4 char replaced, 5 common, 1 preservable)
 *      33. MM45...ii        (5 char replaced, 6 common, 1 preservable)
 *      34. MM45...i         (6 char replaced, 7 common, 1 preservable)
 *      35. MM45...          (7 char replaced, 8 common, 1 preservable)
 *      36. MM...6iiiiii     (2 char replaced, 0 common, 2 preservable)
 *      37. MM4...iiiiii     (2 char replaced, 0 common, 2 preservable)
 *      38. M...6iiiiii      (3 char replaced, 1 common, 2 preservable)
 *      39. ...6iiiiii       (4 char replaced, 2 common, 2 preservable)
 *      40. MM...iiiiii      (3 char replaced, 0 common, 3 preservable)
 *      41. M...iiiiii       (4 char replaced, 1 common, 3 preservable)
 *      42. ...iiiiii        (5 char replaced, 2 common, 3 preservable)
 *      43. M...iiiii        (5 char replaced, 2 common, 3 preservable)
 *      44. ...iiiii         (6 char replaced, 3 common, 3 preservable)
 *      45. M...iiii         (6 char replaced, 3 common, 3 preservable)
 *      46. ...iiii          (7 char replaced, 4 common, 3 preservable)
 *      47. M...iii          (7 char replaced, 4 common, 3 preservable)
 *      48. ...iii           (8 char replaced, 5 common, 3 preservable)
 *      49. M...ii           (8 char replaced, 5 common, 3 preservable)
 *      50. ...ii            (9 char replaced, 6 common, 3 preservable)
 *      51. M...i            (9 char replaced, 6 common, 3 preservable)
 *      52. ...i             (10char replaced, 7 common, 3 preservable)
 *      53. M...             (10char replaced, 7 common, 3 preservable)
 *      54. ...              (11char replaced, 8 common, 3 preservable)
 */
class CandidateFactory {
public:
    /**
     * @brief Constructor
     * @param s is the string to abbreviate, e. g. 'MM456iiiiii'
     *
     * @param pres are the markers for preserved characters, e. g. `{false, false, true, true,
     * true, false, false, false, false, false, false}` to preserve '456' in the above string.
     */
    CandidateFactory(std::string s, std::vector<bool> pres)
        : s{s}, pres{pres}
    {
        aquireNext();
        maxpres = std::accumulate(pres.begin(), pres.end(), 0,
                                  [] (int s, bool d) { return s + (d ? 1 : 0); });
    }

    /**
     * @brief Checks whether there exists another candidate
     * @return true if so, false otherwise
     */
    bool hasNext() const {
        return next != "";
    }

    /**
     * @brief Get next candidate
     *
     * @return next abbreviated candidate with ellipsis ('...') or an empty string if no further
     * candidate exists.
     */
    std::string getNext() const {
        std::string ret = next;
        aquireNext();
        return ret;
    }

private:
    std::string s;
    std::vector<bool> pres;

    mutable unsigned int npres = 0;        // number of preserved chars to remove
    mutable unsigned int ncommon = 1;      // number of non-preserved (common) chars to remove
    mutable unsigned int currentIdx = 0;   // start index in string to search for next candidate
    mutable std::string next = "";
    unsigned int maxpres;                  // number of total preserved chars

    void aquireNext() const {
        std::size_t len = s.size();

        // while number of preserved characters to be replaced is ok
        while (npres <= maxpres) {
            std::size_t ndelete = npres + ncommon;
            while (currentIdx + ndelete <= len && ncommon <= len - maxpres) {
                unsigned int npresDel = std::accumulate(pres.begin() + currentIdx, pres.begin() + currentIdx + ndelete, 0u,
                                                        [] (int s, bool d) { return s + (d ? 1 : 0); });
                ++currentIdx;
//                std::cout << "currentIdx: " << (currentIdx-1) << ", ncommon: " << ncommon << ", npres: " << npres << ", pres deleted: " << npresDel << std::endl;
                if (npresDel == npres)
                {
                    next = s;
                    next.replace(currentIdx-1, ndelete, "...");
//                    std::cout << "currentIdx: " << (currentIdx-1) << ", ncommon: " << ncommon << ", npres: " << npres << ", next cand: " << next << ", pres deleted: " << npresDel << std::endl;
                    return;
                }
            }
            currentIdx = 0;
            ++ncommon;
            if (npres + ncommon > len || ncommon > len - maxpres) {
                ncommon = 0;
                ++npres;
            }
        }
        next = "";
    }
};


/**
 * @brief Shorten preferably the common parts of two strings until they are short enough
 * @param s1 is the first string.
 * @param s2 is the second string.
 *
 * @param isShortEnough is a predicate, which defines when to stop the abbreviations. For example
 * to shorten to 13 chars you could use
 * @code
 * auto isShortEnough = [] (std::string const& s) { return s.size() < 13; };
 * @endcode
 * as argument.
 *
 * This function abbreviates two strings, but leaves the first three and last three characters
 * untouched. For that it finds the common parts of two strings using the Levenshtein algorithm.
 * Then it tries different abbreviations and stops, when the predicate `isShortEnough` is satisfied
 * (or uses the last candidate). The order of the candidates is chosen in a way, that tries to
 * preserve the different parts of the strings if possible.
 *
 * @return the two abbreviated strings as array
 */
std::array<std::string, 2> shorten(std::string s1, std::string s2, std::function<bool(std::string const&)> isShortEnough, unsigned int keepFront = 3, unsigned int keepBack = 3) {
    auto diffs = findStringDiffs(s1, s2);
    std::array<std::string, 2> s{s1, s2};
    for (int i : {0, 1}) {
        std::size_t len = s[i].size();
        if (len > keepFront + keepBack && !isShortEnough(s[i])) {
            std::string start = s[i].substr(0, keepFront);
            std::string end = s[i].substr(len - keepBack, keepBack);
            std::vector<bool> diff{diffs[i].begin() + keepFront, diffs[i].end() - keepBack};
            CandidateFactory cf{s[i].substr(keepFront, len - keepFront - keepBack), diff};
            while (cf.hasNext()) {
                s[i] = start + cf.getNext() + end;
//                std::cout << "next: " << s[i] << " (length: " << s[i].size() << ")" << std::endl;
                if (isShortEnough(s[i])) {
//                    std::cout << "accepted." << std::endl;
                    break;
                }
            }
        }
    }
//    std::cout << "shortened0: " << s[0] << std::endl;
//    std::cout << "shortened1: " << s[1] << std::endl << std::endl;
    return s;
}


// arrowedLine will be available in some future openCV version (maybe 3?), use this as backport until then. Code from: http://stackoverflow.com/a/24466638
void arrowedLine(cv::Mat& img, cv::Point pt1, cv::Point pt2, const cv::Scalar& color,
                 int thickness=1, int line_type=8, int shift=0, double tipLength=0.1)
{
    const double tipSize = norm(pt1-pt2)*tipLength; // Factor to normalize the size of the tip depending on the length of the arrow
    cv::line(img, pt1, pt2, color, thickness, line_type, shift);
    const double angle = atan2( (double) pt1.y - pt2.y, (double) pt1.x - pt2.x );
    cv::Point p(cvRound(pt2.x + tipSize * cos(angle + CV_PI / 4)),
    cvRound(pt2.y + tipSize * sin(angle + CV_PI / 4)));
    cv::line(img, p, pt2, color, thickness, line_type, shift);
    p.x = cvRound(pt2.x + tipSize * cos(angle - CV_PI / 4));
    p.y = cvRound(pt2.y + tipSize * sin(angle - CV_PI / 4));
    cv::line(img, p, pt2, color, thickness, line_type, shift);
}

/**
 * @brief Get ticks on a linear scale
 * @param min is the minimum value.
 * @param max is the maximum value.
 * @param maxticks is the maximum number of ticks to generate.
 *
 * The ticks are generated as fine as possible between `min` and `max` with the restriction of
 * `maxticks` and 'good looking numbers'. `min` and `max` are only included, if they fit to the
 * sequence of ticks. The base sequences are:
 *  * 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
 *  * 0, 2, 4, 6, 8
 *  * 0, 2.5, 5, 7.5
 *  * 0, 5
 *
 * These are scaled to an appropriate dimension and translated, such that they match the ticks one
 * would naturally select. For example the base sequences can be generated with
 *  * `makeLinTicks(0, 9, 10)`
 *  * `makeLinTicks(0, 9, 5)` or e. g. `makeLinTicks(0, 9, 9)`
 *  * `makeLinTicks(0, 9, 4)`
 *  * `makeLinTicks(0, 9, 2)`
 *
 * Another example: `makeLinTicks(101.1, 102.5, 6)` would generate 101.25, 101.5, 101.75, 102,
 * 102.25, 102.5.
 *
 * The minimum step width is two orders lower than `max - min`. This limits the maximum number of
 * steps to 100 to 999, depending on the value of `max - min`.
 *
 * If you need minor and major ticks, just call this function twice with a different value of
 * `maxticks`.
 *
 * @return ticks for a linear scale.
 */
std::vector<double> makeLinTicks(double min, double max, unsigned int maxticks) {
    double span = max - min;
    if (span <= 0 || maxticks == 0)
        return {};

    // 10^(floor(log10(span)))
    double ref = std::pow(10, std::floor(std::log10(span)));
    std::vector<double> refs{ref / 10, ref, ref * 10};
    std::vector<double> factors{10, 5, 4, 2};

    for (double ref : refs) {
        for (double f : factors) {
            double spacing = ref / f;
            double first = std::ceil( min * f / ref) / f * ref;
            double last  = std::floor(max * f / ref) / f * ref;

            if ((last - first) / spacing + 1 <= maxticks) {
                std::vector<double> ticks{first};
                while (!areClose(first, last)) {
                    first += spacing;
                    ticks.push_back(first);
                }
                return ticks;
            }
        }
    }
    return {};
}


/**
 * @brief Get ticks on a logarithmic scale
 * @param min is the minimum value.
 * @param max is the maximum value.
 *
 * The ticks are generated between `min` and `max`. `min` and `max` are only included, if they can
 * be expressed as \f$ n \, 10^i \f$, where \f$ 0 < n < 10 \f$ and \f$ i \f$ are integers. As an
 * example `makeLogTicks(0.1, 10)` would give the values 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8,
 * 0.9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10. However `makeLogTicks(0.71, 2.3)` would give 0.8, 0.9, 1, 2.
 *
 * If you want to check whether a tick `t` is a major tick, use something like
 * @code
 * double exp = std::log10(t);
 * double roundExp = std::round(exp);
 * if (areClose(exp, roundExp))
 *     // ... do something with this major tick
 * @endcode
 *
 * @return (minor) ticks for a logarithmic scale.
 */
std::vector<double> makeLogTicks(double min, double max) {
    if (min <= 0 || max <= 0 || min >= max)
        return {};

    double firstExp = static_cast<int>(std::floor(std::log10(min)));
    double lastExp  = static_cast<int>(std::ceil( std::log10(max)));

    std::vector<double> ticks;
    for (int exp = firstExp; exp <= lastExp; ++exp) {
        for (double val = std::pow(10, exp), spacing = val; val < 9.5 * spacing; val += spacing) {
            if (val > max && !areClose(val, max))
                break;

            if (val > min || areClose(val, min))
                ticks.push_back(val);
        }
    }
    return ticks;
}


/**
 * @brief Draw a line, with image multiplied by 0.5
 * @param plot is the Image to draw into.
 * @param p1 is the starting point.
 * @param p2 is the end point.
 *
 * @param pattern is by default a short dash. To make e. g. a dash-dot pattern you could use `{true,
 * true, true, false, false, true, false, false}`.
 */
void drawDarkeningLine(imagefusion::Image& plot, imagefusion::Point const& p1, imagefusion::Point const& p2, std::vector<bool> const& pattern = {true, true, false, false}) {
    assert(plot.basetype() == imagefusion::Type::uint8);
    unsigned int channels = plot.channels();
    cv::LineIterator lit(plot.cvMat(), p1, p2); // assumes uint8 image
    std::vector<bool> const& p = pattern.empty() ? std::vector<bool>{true, true, false, false}
                                                 : pattern;
    int len = p.size();
    for (int i = 0; i < lit.count; ++i, ++lit)
        if (p[i % len])
            for (unsigned int c = 0; c < channels; ++c)
                (*lit)[c] /= 2;
}


/**
 * @brief Draw horizontal lines, with pixel values multiplied by 0.5
 * @param plot is the Image to draw into.
 * @param pixticks are the y positions (in pixel) where the lines should be drawn.
 *
 * @param pattern is by default a short dash. To make e. g. a dash-dot pattern you could use `{true,
 * true, true, false, false, true, false, false}`.
 */
void drawHorizontalLines(imagefusion::Image& plot, std::vector<int> const& pixticks, std::vector<bool> const& pattern = {true, true, false, false}) {
    assert(plot.basetype() == imagefusion::Type::uint8);
    for (int px : pixticks)
        drawDarkeningLine(plot, imagefusion::Point(0, px), imagefusion::Point(plot.width() - 1, px), pattern);
}


/**
 * @brief Draw vertical lines, with pixel values multiplied by 0.5
 * @param plot is the Image to draw into.
 * @param pixticks are the x positions (in pixel) where the lines should be drawn.
 *
 * @param pattern is by default a short dash. To make e. g. a dash-dot pattern you could use `{true,
 * true, true, false, false, true, false, false}`.
 */
void drawVerticalLines(imagefusion::Image& plot, std::vector<int> const& pixticks, std::vector<bool> const& pattern = {true, true, false, false}) {
    assert(plot.basetype() == imagefusion::Type::uint8);
    for (int px : pixticks)
        drawDarkeningLine(plot, imagefusion::Point(px, plot.height() - 1), imagefusion::Point(px, 0), pattern);
}



/**
 * @brief Draw vertical grid, with pixel values multiplied by 0.5
 * @param plotRegion is the plot region of the image to draw the grid into.
 *
 * @param ticks are the x positions (as function values), where the lines should be drawn.
 *
 * @param pixticks are the x positions (in pixel), where the lines should be drawn.
 *
 * @param pattern is by default a short dash. To make e. g. a dash-dot pattern you could use `{true,
 * true, true, false, false, true, false, false}`.
 *
 * Note, if the first and the last pixel positions are at the left or right border, they are
 * omitted.
 *
 * Note also that line at the function value 0 ignores the pattern and is drawn solid.
 */
void drawVerticalGrid(imagefusion::Image& plotRegion, std::vector<double> ticks, std::vector<int> pixticks, std::vector<bool> const& pattern = {true, true, false, false}) {
    assert(plotRegion.basetype() == imagefusion::Type::uint8);
    assert(ticks.size() == pixticks.size());
    if (ticks.empty())
        return;

    std::vector<bool> const& p = pattern.empty() ? std::vector<bool>{true, true, false, false}
                                                 : pattern;

    // remove grid lines that are too close at the frame border
    if (pixticks.front() <= 0) {
        pixticks.erase(pixticks.begin());
        ticks.erase(ticks.begin());
    }

    if (ticks.empty())
        return;

    if (pixticks.back() >= plotRegion.width() - 1) {
        pixticks.pop_back();
        ticks.pop_back();
    }

    // find the zero tick
    auto tickit = ticks.end();
    for (auto it = ticks.begin(), it_end = ticks.end(); it != it_end; ++it) {
        if (areClose(*it, 0)) {
            tickit = it;
            break;
        }
    }

    // draw it with a solid line and remove it from the vector
    if (tickit != ticks.end()) {
        int idx0 = tickit - ticks.begin();
        int px0 = pixticks.at(idx0);
        drawDarkeningLine(plotRegion, imagefusion::Point(px0, plotRegion.height() - 1), imagefusion::Point(px0, 0), {true});
        pixticks.erase(pixticks.begin() + idx0);
    }

    // draw remaining lines with the given pattern
    drawVerticalLines(plotRegion, pixticks, p);
}


/**
 * @brief Draw horizontal grid, with pixel values multiplied by 0.5
 * @param plotRegion is the plot region of the image to draw the grid into.
 * @param orig defines the origin (bottom left) point of the plot.
 *
 * @param ticks are the y positions (as function values), where the lines should be drawn.
 *
 * @param pixticks are the y positions (in pixel), where the lines should be drawn. Note, the pixel
 * values for small tick values can be small and for large they can be large. So they are not yet
 * inverted for the different coordinate systems. It will be drawn from the bottom left.
 *
 * @param pattern is by default a short dash. To make e. g. a dash-dot pattern you could use
 * `{true, true, true, false, false, true, false, false}`.
 *
 * Note, if the first and the last pixel positions are at the left or right border, they are
 * omitted.
 *
 * Note also that line at the function value 0 ignores the pattern and is drawn solid.
 */
void drawHorizontalGrid(imagefusion::Image& plotRegion, std::vector<double> ticks, std::vector<int> pixticks, std::vector<bool> const& pattern = {true, true, false, false}) {
    assert(plotRegion.basetype() == imagefusion::Type::uint8);
    assert(ticks.size() == pixticks.size());
    if (ticks.empty())
        return;

    std::vector<bool> const& p = pattern.empty() ? std::vector<bool>{true, true, false, false}
                                                 : pattern;

    // remove grid lines that are too close at the frame border
    if (pixticks.front() <= 0) {
        pixticks.erase(pixticks.begin());
        ticks.erase(ticks.begin());
    }

    if (ticks.empty())
        return;

    if (pixticks.back() >= plotRegion.height() - 1) {
        pixticks.pop_back();
        ticks.pop_back();
    }

    // invert pixticks to start from the bottom
    int height = plotRegion.height();
    std::transform(pixticks.begin(), pixticks.end(), pixticks.begin(), [&](int px){ return height - 1 - px; });

    // find the zero tick
    auto tickit = ticks.end();
    for (auto it = ticks.begin(), it_end = ticks.end(); it != it_end; ++it) {
        if (areClose(*it, 0)) {
            tickit = it;
            break;
        }
    }

    // draw it with a solid line and remove it from the vector
    if (tickit != ticks.end()) {
        int idx0 = tickit - ticks.begin();
        int px0 = pixticks.at(idx0);
        drawDarkeningLine(plotRegion, imagefusion::Point(0, px0), imagefusion::Point(plotRegion.width() - 1, px0), {true});
        pixticks.erase(pixticks.begin() + idx0);
    }

    // draw remaining lines with the given pattern
    drawHorizontalLines(plotRegion, pixticks, p);
}



/**
 * @brief Generate ticks values and their corresponding pixel values
 * @param min is the minimum value, which is assumed to be at the origin of the plot. This may be 0
 * and then stays zero.
 * @param max is the maximum value, which is assumed to be the last value in the plot.
 * @param plotSize is the width or height of the plot.
 * @return tick values (in real values) and their pixel values
 */
std::pair<std::vector<double>, std::vector<int>> generateLogTicksWithPixelPositions(int min, int max, int plotSize) {
    if (max == min)
        return std::make_pair(std::vector<double>{static_cast<double>(min)}, std::vector<int>{0});

    assert(max > min);
    assert(min >= 0);
    assert(plotSize > 0);

    bool zeroMin = min == 0;
    if (zeroMin)
        min = 1;

    std::vector<double> ticks = makeLogTicks(min, max);
    if (ticks.empty() || !areClose(ticks.front(), min))
        ticks.insert(ticks.begin(), min);
    if (!areClose(ticks.back(), max))
        ticks.push_back(max);

    if (zeroMin)
        min = 0;

    std::vector<int> pixticks(ticks.size());
    auto valToPixel = [&] (double val) { return static_cast<int>(std::round((std::log(val - min) + 1) / (std::log(max - min) + 1) * (plotSize - 1))); };
    std::transform(ticks.begin(), ticks.end(), pixticks.begin(), valToPixel);

    if (zeroMin) {
        ticks.insert(ticks.begin(), 0);
        pixticks.insert(pixticks.begin(), 0);
    }

    return {ticks, pixticks};
}


/**
 * @brief Generate ticks values and their corresponding pixel values
 * @param min is the minimum value, which is assumed to be at the origin of the plot.
 * @param max is the maximum value, which is assumed to be the last value in the plot.
 * @param plotSize is the width or height of the plot.
 * @return tick values (in real values) and their pixel values.
 */
std::pair<std::vector<double>, std::vector<int>> generateTicksWithPixelPositions(double min, double max, int plotSize) {
    assert(plotSize > 0);
    constexpr int zerosize = 14;

    std::vector<double> ticks = makeLinTicks(min, max, plotSize / zerosize);
    if (ticks.empty() || !areClose(ticks.front(), min))
        ticks.insert(ticks.begin(), min);
    if (!areClose(ticks.back(),  max))
        ticks.push_back(max);

    std::vector<int> pixticks(ticks.size());
    auto valToPixel = [&] (double val) { return static_cast<int>(std::round((val - min) / (max - min) * (plotSize - 1))); };
    std::transform(ticks.begin(), ticks.end(), pixticks.begin(), valToPixel);
    return {ticks, pixticks};
}

/**
 * @brief Draw the x ticks on the x-axis
 * @param plot is the image to draw the ticks.
 * @param orig defines the origin (bottom left) point of the plot.
 * @param plotWidth is the width of the plot.
 * @param pixticks are the pixel values to draw.
 */
void drawXTicks(imagefusion::Image& plot, imagefusion::Point orig, std::vector<int> const& pixticks) {
    imagefusion::Image imageXTicks{plot.sharedCopy({orig.x, orig.y + 1, plot.width() - orig.x, 5})};
    drawVerticalLines(imageXTicks, pixticks, {true});
}


/**
 * @brief Draw the y ticks on the y-axis
 * @param plot is the image to draw the ticks.
 * @param orig defines the origin (bottom left) point of the plot.
 * @param plotHeight is the height of the plot.
 *
 * @param pixticks are the pixel values to draw. Note, the pixel values for small tick values can
 * be small and for large they can be large. So they are not yet inverted for the different
 * coordinate systems. This will be done with help of `orig`.
 */
void drawYTicks(imagefusion::Image& plot, imagefusion::Point orig, std::vector<int> pixticks) {
    std::transform(pixticks.begin(), pixticks.end(), pixticks.begin(), [orig] (int px) { return orig.y - px - 1; });
    imagefusion::Image imageYTicks{plot.sharedCopy({orig.x - 6, 0, 5, orig.y})};
    drawHorizontalLines(imageYTicks, pixticks, {true});
}


/**
 * @brief Place the labels next to the x-axis
 * @param plot is the image to put the labels into.
 * @param orig defines the origin (bottom left) point of the plot (in pixels).
 * @param ticks_and_pxpos are the tick values and their corresponding pixel values.
 * @param islogplot tells this function whether it is a log plot. If the axis should be
 * locarithmically scaled, only the labels with integer exponent (like 0.1, 1, 10) are placed e. g.
 * as 1e1.
 */
void placeXLabels(imagefusion::Image& plot, imagefusion::Point orig, std::vector<double> const& ticks, std::vector<int> const& pixticks, bool islogplot = false) {
    assert(ticks.size() == pixticks.size());
    if (ticks.empty())
        return;

    constexpr int framecolor = 128;
    constexpr int fontFace = cv::FONT_HERSHEY_COMPLEX_SMALL;
    std::stringstream ss;
    int baseline;

    // put min and max label first
    ss.str("");
    ss << std::setprecision(5) << ticks.front();
    std::string tickText = ss.str();
    cv::Size tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
    int bottomLine = orig.y + tickTextSize.height + 7;
    int leftTextBorder = orig.x + pixticks.front() + tickTextSize.width / 2;;
    cv::putText(plot.cvMat(), tickText, cv::Point{leftTextBorder - tickTextSize.width, bottomLine}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
    if (ticks.size() <= 1)
        return;

    ss.str("");
    ss << std::setprecision(5) << ticks.back();
    tickText = ss.str();
    tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
    int rightTextBorder = orig.x + pixticks.back() - tickTextSize.width / 2;
    cv::putText(plot.cvMat(), tickText, cv::Point{rightTextBorder, bottomLine}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
    if (ticks.size() <= 2)
        return;

    // put the zero label if appropriate
    int leftZeroBorder = plot.width();
    int rightZeroBorder = 0;
    // find the zero tick
    auto tickit = ticks.end() - 1;
    for (auto it = ticks.begin() + 1, it_end = ticks.end() - 1; it != it_end; ++it) {
        if (areClose(*it, 0)) {
            tickit = it;
            break;
        }
    }
    // place it and set the bounds
    if (tickit != ticks.end() - 1) {
        int idx0 = tickit - ticks.begin();
        int center = orig.x + pixticks.at(idx0);
        tickText = "0";
        tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
        int textleft  = center - tickTextSize.width / 2;
        int textright = center + tickTextSize.width / 2;
        if (leftTextBorder + 5 < textleft && textright < rightTextBorder - 5) { // between min and max
            leftZeroBorder  = textleft;
            rightZeroBorder = textright;
            cv::putText(plot.cvMat(), tickText, cv::Point{leftZeroBorder, bottomLine}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
        }
        if (ticks.size() <= 3)
            return;
    }

    // now start from min + 1 and place every label that fits
    for (unsigned int i = 1; i < ticks.size() - 1; ++i) {
        ss.str("");
        if (islogplot) {
            double exp = std::log10(ticks[i]);
            double roundExp = std::round(exp);
            // only print major labels (ticks that have an integer exponent)
            if (areClose(exp, roundExp))
                ss << std::setprecision(5) << "1e" << roundExp;
            else
                continue;
        }
        else {
            ss << std::setprecision(5) << ticks[i];
        }
        tickText = ss.str();
        tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
        int center = orig.x + pixticks[i];
        int textleft  = center - tickTextSize.width / 2;
        int textright = center + tickTextSize.width / 2;
        if (leftTextBorder + 5 < textleft && textright < rightTextBorder - 5       // between min and max
            && (textright < leftZeroBorder - 5 || textleft > rightZeroBorder + 5)) // not overlapping with 0-label
        {
            cv::putText(plot.cvMat(), tickText, cv::Point{textleft, bottomLine}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
            leftTextBorder = textright;
        }
    }
}


/**
 * @brief Place the labels next to the y-axis
 * @param plot is the image to put the labels into.
 * @param orig defines the origin (bottom left) point of the plot.
 *
 * @param ticks_and_pxpos are the tick values and their corresponding pixel values. Note, the pixel
 * values for small tick values can be small and for large they can be large. So they are not yet
 * inverted for the different coordinate systems. This will be done with help of `orig`.
 *
 * @param islogplot tells this function whether it is a log plot. If the axis should be
 * locarithmically scaled onle the labels with integer exponent (like 0.1, 1, 10) are placed e. g.
 * as 1e1.
 */
void placeYLabels(imagefusion::Image& plot, imagefusion::Point orig, std::vector<double> const& ticks, std::vector<int> const& pixticks, bool islogplot = false) {
    assert(ticks.size() == pixticks.size());
    if (ticks.empty())
        return;

    constexpr int framecolor = 128;
    constexpr int fontFace = cv::FONT_HERSHEY_COMPLEX_SMALL;
    std::stringstream ss;
    int baseline;

    // put min and max label first
    ss.str("");
    ss << std::setprecision(5) << ticks.front();
    std::string tickText = ss.str();
    cv::Size tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
    int rightLine = orig.x - 6;
    int lowerTextBorder = orig.y - tickTextSize.height / 2;
    cv::putText(plot.cvMat(), tickText, cv::Point{rightLine - tickTextSize.width, lowerTextBorder + tickTextSize.height}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
    if (ticks.size() <= 1)
        return;

    ss.str("");
    ss << std::setprecision(5) << ticks.back();
    tickText = ss.str();
    tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
    int upperTextBorder = orig.y - pixticks.back() + tickTextSize.height / 2;
    cv::putText(plot.cvMat(), tickText, cv::Point{rightLine - tickTextSize.width, upperTextBorder}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
    if (ticks.size() <= 2)
        return;

    // put the zero label if appropriate
    int upperZeroBorder = plot.height();
    int lowerZeroBorder = 0;
    // find the zero tick
    auto tickit = ticks.end() - 1;
    for (auto it = ticks.begin() + 1, it_end = ticks.end() - 1; it != it_end; ++it) {
        if (areClose(*it, 0)) {
            tickit = it;
            break;
        }
    }
    // place it and set the bounds
    if (tickit != ticks.end() - 1) {
        int idx0 = tickit - ticks.begin();
        int center = orig.y - pixticks.at(idx0);
        tickText = "0";
        tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
        int texttop    = center - tickTextSize.height / 2;
        int textbottom = center + tickTextSize.height / 2;
        if (lowerTextBorder - 5 > textbottom && texttop > upperTextBorder + 5) { // between min and max
            upperZeroBorder = texttop;
            lowerZeroBorder = textbottom;
            cv::putText(plot.cvMat(), tickText, cv::Point{rightLine - tickTextSize.width, lowerZeroBorder}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
        }
        if (ticks.size() <= 3)
            return;
    }

    // now start from min and place every label that fits
    for (unsigned int i = 1; i < ticks.size() - 1; ++i) {
        ss.str("");
        if (islogplot) {
            double exp = std::log10(ticks[i]);
            double roundExp = std::round(exp);
            // only print major labels (ticks that have an integer exponent)
            if (areClose(exp, roundExp))
                ss << std::setprecision(5) << "1e" << roundExp;
            else
                continue;
        }
        else {
            ss << std::setprecision(5) << ticks[i];
        }
        tickText = ss.str();
        tickTextSize = cv::getTextSize(tickText, fontFace, 1, 1, &baseline);
        int center = orig.y - pixticks[i] - 1;
        int texttop    = center - tickTextSize.height / 2;
        int textbottom = center + tickTextSize.height / 2;
        if (lowerTextBorder - 5 > textbottom && texttop > upperTextBorder + 5       // between min and max
            && (texttop > lowerZeroBorder + 5 || textbottom < upperZeroBorder - 5)) // not overlapping with 0-label
        {
            cv::putText(plot.cvMat(), tickText, cv::Point{rightLine - tickTextSize.width, textbottom}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
            lowerTextBorder = texttop;
        }
    }
}


/**
 * @brief Detect empty border
 *
 * @param img is the Image to look into. This must be an Image of basetype Type::uint8.
 *
 * @param borderCol is the color of the border. One value for each channel. Any pixel, which has
 * not the same color is not considered as border. If only one value is specified for a 3 channel
 * image, the value is used for all channels.
 *
 * @return crop rectangle to remove the borders from the Image.
 */
imagefusion::Rectangle detectBorderCropBounds(imagefusion::ConstImage const& img, std::vector<uint8_t> borderCol = {255}) {
    assert(img.basetype() == imagefusion::Type::uint8);
    unsigned int chans = img.channels();
    assert(borderCol.size() == 1 || borderCol.size() == chans);
    if (borderCol.size() < chans) {
        uint8_t color = borderCol.front();
        borderCol.resize(chans, color);
    }

    imagefusion::Size s = img.size();
    int top;
    for (top = 0; top < s.height; ++top)
        for (int x = 0; x < s.width; ++x)
            for (unsigned int c = 0; c < chans; ++c)
                if (img.at<uint8_t>(x, top, c) != borderCol[c])
                    goto foundTop;
    foundTop:

    int bottom;
    for (bottom = s.height - 1; bottom > top; --bottom)
        for (int x = 0; x < s.width; ++x)
            for (unsigned int c = 0; c < chans; ++c)
                if (img.at<uint8_t>(x, bottom, c) != borderCol[c])
                    goto foundBottom;
    foundBottom:

    int left;
    for (left = 0; left < s.width; ++left)
        for (int y = top; y < bottom; ++y)
            for (unsigned int c = 0; c < chans; ++c)
                if (img.at<uint8_t>(left, y, c) != borderCol[c])
                    goto foundLeft;
    foundLeft:

    int right;
    for (right = s.width - 1; right > left; --right)
        for (int y = top; y < bottom; ++y)
            for (unsigned int c = 0; c < chans; ++c)
                if (img.at<uint8_t>(right, y, c) != borderCol[c])
                    goto foundRight;
    foundRight:

    return {left, top, right - left + 1, bottom - top + 1};
}

/**
 * @brief The PlotScatterFunctor functor draws the inner scatter plot part
 */
struct PlotScatterFunctor {
    double min, max;
    int16_t plotSize;
    imagefusion::ConstImage const& i1;
    imagefusion::ConstImage const& i2;
    imagefusion::Image& plot;
    imagefusion::ConstImage const& mask;

    template<imagefusion::Type imfutype>
    int operator()() {
        static_assert(getChannels(imfutype) == 1, "Scatter plot can only be used as base type functor.");
        assert(i1.channels() == 1);
        assert(i2.channels() == 1);

        using type = typename imagefusion::BaseType<imfutype>::base_type;
        constexpr bool isInt = isIntegerType(imfutype);
        bool hasMask = !mask.empty();

        // simple mode, where scatter dots are just a pixel
        if (plotSize - 1 <= max - min || !isInt) {
            double factor = (plotSize - 1) / (max - min);
            double min_loc = min;
            auto transform = [factor, min_loc] (double coord) {
                return static_cast<int>(std::round((coord - min_loc) * factor));
            };

            for (int y = 0; y < i1.height(); ++y) {
                for (int x = 0; x < i1.width(); ++x) {
                    if (hasMask && !mask.boolAt(x, y, 0))
                        continue;
                    int px = transform(i1.at<type>(x, y));
                    int py = plotSize - 1 - transform(i2.at<type>(x, y));

                    plot.setBoolAt(px, py, 0, false);
                }
            }
            return 0;
        }

        // complicated mode, where scatter dots are circles
        int dia = std::ceil(plotSize / (max - min + 1));
        if (dia % 2 == 0) // find next larger odd diameter
            ++dia;

        int largeSize = dia * (max - min + 1);
        cv::Mat largePlot(largeSize, largeSize, CV_8UC1);
        largePlot = 255;

        double min_loc = min;
        auto transform = [min_loc, dia] (double coord) {
            return static_cast<int>(std::round((coord - min_loc) * dia + dia / 2));
        };

        int r = dia / 2;
        for (int y = 0; y < i1.height(); ++y) {
            for (int x = 0; x < i1.width(); ++x) {
                if (hasMask && !mask.boolAt(x, y, 0))
                    continue;
                int px = transform(i1.at<type>(x, y));
                int py = largeSize - 1 - transform(i2.at<type>(x, y));
                if (largePlot.at<uint8_t>(py, px) != 255)
                    continue;

                cv::circle(largePlot, {px, py}, r, cv::Scalar::all(0), CV_FILLED);
            }
        }

        cv::resize(largePlot, plot.cvMat(), plot.size());
        return static_cast<int>(plotSize / (max - min + 1) / 2);
    }
};


/**
 * @brief Make a scatter plot
 * @param i1 is the single-channel image for the horizontal axis.
 * @param i2 is the single-channel image for the vertical axis.
 *
 * @param mask (uint8x1) with 0 values, where i1 and i2 should be ignored. May be empty to mark all
 * values as valid.
 *
 * @param range is the range of [min, max] from i1 and i2, where min and max are taken from all
 * channels.
 *
 * @param plotSize is the size of the plot (without frame). If a negative number is given, the
 * natural size will be used, which is max - min brightness values. If this is larger than
 * `|plotSize|`, use `|plotSize|` instead. So it is a maximum number up to the natural size will be
 * used. However, the chosen size must be greater than two. So choosing 0, will use the natural
 * size, no matter how large it might be.
 *
 * @param drawFrame determines whether a frame is drawn and about the color scheme. When set to
 * true this will draw a frame with axis around the plot and make it black-on-white image instead
 * of white-on-black.
 *
 * @param grid determines whether to draw a grid.
 *
 * @param withLegend determines whether to print "x: fn1" and "y: fn2" below the plot (only if
 * `drawFrame`)
 *
 * @param fn1 is the legend entry for x-axis, typically the filename of `i1`.
 * @param fn2 is the legend entry for y-axis, typically the filename of `i2`.
 * @return the plot.
 */
imagefusion::Image plotScatter(imagefusion::ConstImage const& i1,
                               imagefusion::ConstImage const& i2,
                               imagefusion::ConstImage const& mask,
                               imagefusion::Interval range, int16_t plotSize = -512, bool drawFrame = true, bool grid = false,
                               bool withLegend = false, std::string fn1 = "", std::string fn2 = "") {
    assert(i1.channels() == 1);
    assert(i2.channels() == 1);
    assert(i1.size() == i2.size());
    assert(i1.type() == i2.type());

    double min = range.lower();
    double max = range.upper();

    bool isInteger = isIntegerType(i1.type());
    if (isInteger && max - min <= 1)
        IF_THROW_EXCEPTION(imagefusion::runtime_error("The images are too homogeneous to draw a scatter plot."));

    if (isInteger) {
        // for negative plot size, find natural (unscaled) plot size, up to -plotSize (minimum 20)
        int specPlotSize = plotSize;
        if (specPlotSize < 3)
            plotSize = max - min > std::numeric_limits<int16_t>::max()
                      ? std::numeric_limits<int16_t>::max()
                      : static_cast<int16_t>(std::round(max - min + 1));
        if (-specPlotSize > 2 && -specPlotSize < plotSize)
            plotSize = -specPlotSize;
    }
    else { // floating point image, range: [0, 1]
        // cannot compute a natural plot size easily, so just take |size|, but at least 100
        plotSize = std::max(std::abs((int)plotSize), 100);
    }

    // plot without frame
    if (!drawFrame) {
        imagefusion::Image plot{plotSize, plotSize, imagefusion::Type::uint8};
        plot.set(255);
        imagefusion::CallBaseTypeFunctor::run(PlotScatterFunctor{min, max, plotSize, i1, i2, plot, mask},
                                              i1.type());
        return plot;
    }

    // plot with frame section
    constexpr int framecolor = 128;

    // make plot image
    constexpr int zerosize = 14; // size (width and height) of a zero with current font and size
    int scatterFrameLeft   = 10 * zerosize;
    int scatterFrameBottom = 10 * zerosize;
    int scatterFrameTop    = 5 * zerosize;
    int scatterFrameRight  = 5 * zerosize;

    // plot
    imagefusion::Image plot{
            plotSize + scatterFrameLeft + scatterFrameRight,
            plotSize + scatterFrameBottom + scatterFrameTop,
            imagefusion::Type::uint8};
    plot.set(255);
    imagefusion::Image plotRegion{plot.sharedCopy({scatterFrameLeft, scatterFrameTop, plotSize, plotSize})};
    int offset = imagefusion::CallBaseTypeFunctor::run(PlotScatterFunctor{min, max, plotSize, i1, i2, plotRegion, mask},
                                                                          i1.type());


    // inner ticks and labels
    imagefusion::Point orig{scatterFrameLeft, scatterFrameTop + plotSize};
    auto ticks_and_pxpos = generateTicksWithPixelPositions(min, max, plotSize - 2 * offset);
    drawXTicks(plot, orig + imagefusion::Point{offset, 0}, ticks_and_pxpos.second);
    drawYTicks(plot, orig - imagefusion::Point{0, offset}, ticks_and_pxpos.second);
    placeXLabels(plot, orig + imagefusion::Point{offset, 0}, ticks_and_pxpos.first, ticks_and_pxpos.second);
    placeYLabels(plot, orig - imagefusion::Point{0, offset}, ticks_and_pxpos.first, ticks_and_pxpos.second);

    // frame / axis
    cv::rectangle(plot.cvMat(), cv::Rect(scatterFrameLeft - 1, scatterFrameTop - 1, plotSize + 2, plotSize + 2), cv::Scalar::all(framecolor));
    arrowedLine(plot.cvMat(), cv::Point{scatterFrameLeft - 1,             scatterFrameTop},
                              cv::Point{scatterFrameLeft - 1,             scatterFrameTop - 20},       cv::Scalar::all(framecolor), 1, 8, 0, 0.5);
    arrowedLine(plot.cvMat(), cv::Point{scatterFrameLeft + plotSize + 1,  scatterFrameTop + plotSize},
                              cv::Point{scatterFrameLeft + plotSize + 20, scatterFrameTop + plotSize}, cv::Scalar::all(framecolor), 1, 8, 0, 0.5);

    // grid
    if (grid) {
        drawVerticalGrid(plotRegion, ticks_and_pxpos.first, ticks_and_pxpos.second);
        drawHorizontalGrid(plotRegion, ticks_and_pxpos.first, ticks_and_pxpos.second);
    }

    //legend
    imagefusion::Rectangle cr = detectBorderCropBounds(plot);
    plot.crop({cr.x, 0, cr.width, plot.height()});
    if (withLegend) {
        constexpr int fontFace = cv::FONT_HERSHEY_COMPLEX_SMALL;
        int baseline;
        int totalWidth = plot.width();
        auto doesFitIntoPlot = [&] (std::string const& s) { return cv::getTextSize(s, fontFace, 1, 1, &baseline).width < totalWidth; };
        auto shortened = shorten("x: " + fn1, "y: " + fn2, doesFitIntoPlot, 4);
        cv::putText(plot.cvMat(), shortened[0], cv::Point{0, scatterFrameTop + plotSize + 2 * (zerosize + 6)}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
        cv::putText(plot.cvMat(), shortened[1], cv::Point{0, scatterFrameTop + plotSize + 3 * (zerosize + 6)}, fontFace, 1, cv::Scalar::all(framecolor), 1, CV_AA);
    }
    plot.crop(detectBorderCropBounds(plot));

    return plot;
}


/**
 * @brief Calculate a histogram from an image
 */
struct HistFunctor {
    unsigned int nbins;
    imagefusion::Interval const& range;
    imagefusion::ConstImage const& i;
    imagefusion::ConstImage const& m;

    /**
     * @param i is the image from which the histogram is created. This can be empty. In that case
     * an empty hist will be returned, which means it has still `nbins` values, but all are zero.
     *
     * @param nbins is the number of bins to use.
     *
     * @param m is an optional mask image. Values marked with 0 will be ignored, values marked with
     * 255 are used.
     *
     * @return pair with the bins vector and histogram in a vector of size `nbins`.
     */
    HistFunctor(imagefusion::ConstImage const& i,
                unsigned int nbins,
                imagefusion::Interval const& range,
                imagefusion::ConstImage const& m = {})
        : nbins(nbins), range(range), i(i), m(m)
    { }

    template<imagefusion::Type imfutype>
    std::pair<std::vector<double>,std::vector<unsigned int>> operator()() {
        static_assert(getChannels(imfutype) == 1, "GetHistFunctor may only be used as BaseTypeFunctor.");
        assert(i.channels() == 1 && "HistFunctor does currently only work for single-channel images.");
        using type = typename imagefusion::BaseType<imfutype>::base_type;

        // calculate bin width
        double binWidth = (range.upper() - range.lower()) / nbins;
        double firstBin = range.lower() + binWidth / 2;
        bool leftOpen  = range.bounds().left()  == boost::icl::interval_bounds::open();
        bool rightOpen = range.bounds().right() == boost::icl::interval_bounds::open();

        // make histogram
        int w = i.width();
        int h = i.height();
        std::vector<unsigned int> hist(nbins + 1); // one extra to catch the value imgMaxLimit in floating point images
        bool hasMask = !m.empty();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (hasMask && !m.boolAt(x, y, 0))
                    continue;

                type v = i.at<type>(x, y);
                if (v < range.lower() || v > range.upper() || (leftOpen && v == range.lower()) || (rightOpen && v == range.upper()))
                    continue;

                int bin = static_cast<int>((v - range.lower()) / binWidth);
                ++hist.at(bin);
            }
        }

        // handle range.upper() values in floating point images (for integer images it is 0)
        unsigned int floatmaxvals = hist.back();
        hist.pop_back();
        hist.back() += floatmaxvals;

        assert(hist.size() == nbins);
        std::vector<double> bins(nbins);
        for (unsigned int i = 0; i < nbins; ++i)
            bins[i] = firstBin + binWidth * i;

        return std::make_pair(bins, hist);
    }
};


/**
 * @brief Get a histogram from an image
 *
 * @param i is the image from which the histogram is created. This can be empty. In that case an
 * empty hist will be returned, which means it has still `nbins` values, but all are zero.
 *
 * @param nbins is the number of bins to use.
 *
 * @param range is the data range of the histogram.
 *
 * @param m is an optional mask image. Values marked with 0 will be ignored, values marked with 255
 * are used.
 *
 * @return pair with the bins vector and histogram in a vector of size `nbins`.
 */
std::pair<std::vector<double>,std::vector<unsigned int>>
computeHist(imagefusion::ConstImage const& i,
        unsigned int nbins,
        imagefusion::Interval const& range,
        imagefusion::ConstImage const& m = {})
{
    return imagefusion::CallBaseTypeFunctor::run(HistFunctor{i, nbins, range, m}, i.type());
}

imagefusion::Interval findRange(std::string rangeArg,
                               std::string rangeCmd,
                               imagefusion::ConstImage const& i0,
                               imagefusion::ConstImage const& i1 = {},
                               imagefusion::ConstImage const& mask = {})
{
    if (rangeArg == "auto") {
        using pair_t = std::pair<imagefusion::ValueWithLocation, imagefusion::ValueWithLocation>;
        auto cmp_min = [] (pair_t const& p1, pair_t const& p2) {
            return (p1.first.val < p2.first.val && p1.first.p != imagefusion::Point(-1, -1))               // only if valid min
                || (p1.first.p != imagefusion::Point(-1, -1) && p2.first.p == imagefusion::Point(-1, -1)); // valid < invalid
        };
        auto cmp_max = [] (pair_t const& p1, pair_t const& p2) {
            return (p1.second.val < p2.second.val && p2.second.p != imagefusion::Point(-1, -1))              // only if valid max
                || (p2.second.p != imagefusion::Point(-1, -1) && p1.second.p == imagefusion::Point(-1, -1)); // invalid < valid
        };

        auto minMaxLoc = i0.minMaxLocations(mask);
        double min0 = std::min_element(minMaxLoc.begin(), minMaxLoc.end(), cmp_min)->first.val;
        double max0 = std::max_element(minMaxLoc.begin(), minMaxLoc.end(), cmp_max)->second.val;
        if (i1.empty())
            return imagefusion::Interval::closed(min0, max0);

        minMaxLoc = i1.minMaxLocations(mask);
        double min1 = std::min_element(minMaxLoc.begin(), minMaxLoc.end(), cmp_min)->first.val;
        double max1 = std::max_element(minMaxLoc.begin(), minMaxLoc.end(), cmp_max)->second.val;
        return imagefusion::Interval::closed(std::min(min0, min1), std::max(max0, max1));
    }

    // user specified range
    imagefusion::Interval range = imagefusion::option::Parse::Interval(rangeArg, rangeCmd);
    imagefusion::Type t = i0.basetype();
    if (isIntegerType(t)) {
        // limit on image range, but keep nan
        bool leftClosed  = range.bounds().left()  != boost::icl::interval_bounds::open();
        bool rightClosed = range.bounds().right() != boost::icl::interval_bounds::open();
        double l = range.lower();
        double u = range.upper();

        if (l < getImageRangeMin(t)) {
            l = getImageRangeMin(t);
            leftClosed = true;
        }

        if (u > getImageRangeMax(t)) {
            u = getImageRangeMax(t);
            rightClosed = true;
        }

        // handle open intervals
        if (leftClosed)
            l = std::ceil(l);
        else
            l = std::floor(l);

        if (rightClosed)
            u = std::floor(u);
        else
            u = std::ceil(u);

        // convert to closed double interval
        range = imagefusion::Interval::closed(std::isnan(l) || leftClosed ? l : l + 1, std::isnan(u) || rightClosed ? u : u - 1);
    }
    return range;
}


/**
 * @brief Draw a raw histogram plot of one or two histograms
 * @param hist1 is the first histogram.
 * @param hist2 is the second histogram.
 * @param s is the plot size.
 * @param logplot determines whether to use logarithmic scale for the histograms.
 * @param gray determines whether to use only gray scale. The image will still have 3 channels.
 * @return plot of the histogram(s) without axis or so.
 */
imagefusion::Image drawHistPlot(std::vector<unsigned int> h1,
                                std::vector<unsigned int> h2,
                                imagefusion::Size const& s,
                                bool logplot = false,
                                bool gray = false)
{
    const unsigned int nbins = h1.size();
    unsigned int maxcount1 = *std::max_element(h1.begin(), h1.end());
    unsigned int maxcount2 = *std::max_element(h2.begin(), h2.end());
    unsigned int maxcount  = std::max(maxcount1, maxcount2);

    // normalize for plot height
    if (logplot) {
        // logarithmic scale (log(0) --> 0, log(x) --> log(x) + 1
        double factor = (s.height - 1) / (std::log(maxcount) + 1);
        auto toLogPixelScale = [&](unsigned int count) {
            return count > 0 ? static_cast<unsigned int>(std::round((std::log(count) + 1) * factor))
                             : 0;
        };
        std::transform(h1.begin(), h1.end(), h1.begin(), toLogPixelScale);
        std::transform(h2.begin(), h2.end(), h2.begin(), toLogPixelScale);
    }
    else {
        double factor = static_cast<double>(s.height - 1) / maxcount;
        auto toLinPixelScale = [&](unsigned int count) {
            return static_cast<unsigned int>(std::round(count * factor));
        };
        std::transform(h1.begin(), h1.end(), h1.begin(), toLinPixelScale);
        std::transform(h2.begin(), h2.end(), h2.begin(), toLinPixelScale);
    }

    // draw histogram in integer pixel precision, inner outlines are overlapping
    const double barWidth = static_cast<double>(s.width - 1) / nbins;
    const unsigned int initBarWidth = static_cast<unsigned int>(std::ceil(barWidth));
    const bool withOutline = initBarWidth > 3;
    const imagefusion::Size initSize(initBarWidth * nbins + 1, s.height);
    cv::Mat initPlot(initSize, CV_8UC3);
    initPlot.setTo(cv::Scalar::all(255));
    for (unsigned int i = 0; i < nbins; ++i) {
        int height1 = h1[i];
        int height2 = h2[i];
        int min = std::min(height1, height2);
        int max = std::max(height1, height2);

        // draw different (upper) part
        auto col = height1 > height2 ? cv::Scalar(255, 128, 128)
                                     : height2 > 0 ? cv::Scalar(128, 128, 255)
                                                   : cv::Scalar(0, 0, 0); // fill used as outline
        if (gray)
            col = cv::Scalar(128, 128, 128);

        cv::rectangle(initPlot, cv::Rect(i * initBarWidth, initSize.height - 1 - max, initBarWidth + 1, max + 1), col, CV_FILLED);
        if (withOutline)
            cv::rectangle(initPlot, cv::Rect(i * initBarWidth, initSize.height - 1 - max, initBarWidth + 1, max + 1), cv::Scalar(0, 0, 0), 1);

        // draw common part
        if (min > 0) { // in case of gray min is 0
            if (withOutline) {
                cv::rectangle(initPlot, cv::Rect(i * initBarWidth + 1, initSize.height - min, initBarWidth - 1, min - 1), cv::Scalar(192, 128, 192), CV_FILLED);

                auto col = height1 == height2 ? cv::Scalar(0, 0, 0) : cv::Scalar(64, 0, 64);
                cv::line(initPlot, cv::Point( i    * initBarWidth + 1, initSize.height - 1 - min),
                                   cv::Point((i+1) * initBarWidth - 1, initSize.height - 1 - min), col, 1);
            }
            else
                cv::rectangle(initPlot, cv::Rect(i * initBarWidth, initSize.height - 1 - min, initBarWidth + 1, min + 1), cv::Scalar(192, 128, 192), CV_FILLED);
        }
    }

    // if no outlines are drawn remove one pixel from the right border, since in otherwise the last bin would be one pixel wider than the others
    if (!withOutline)
        initPlot = initPlot(cv::Rect{0, 0, initSize.width-1, initSize.height});


    // scale down to specified size
    imagefusion::Image plot;
    cv::resize(initPlot, plot.cvMat(), s);

    return plot;
}


/**
 * @brief Plot one or two histograms
 *
 * @param hist1 is the first histogram, plotted in red (if `!gray`). `hist1` can be empty, which
 * means that it has the same dimension as `hist2`, but is filled with zeros.
 *
 * @param hist2 is the second histogram, plotted in blue (if `!gray`). `hist2` can be empty, which
 * means that it has the same dimension as `hist1`, but is filled with zeros.
 *
 * @param bins of `hist1` and `hist2`.
 *
 * @param basetype is the image type, which is used to get the data range min and max and for
 * prettier number printing in case of integer type.
 *
 * @param plotSize is the size of the plot (without frame).
 *
 * @param withLegend determines whether to print a legend with `fn1` and `fn2` below the plot.
 *
 * @param logplot determines whether to scale the histogram logarithmically.
 *
 * @param grid determines whether to draw a horizontal grid.
 *
 * @param gray determines whether to use grayscale. This is meant to be used only for plotting a
 * single histogram.
 *
 * @param fn1 is the legend entry corresponding to `hist1`, typically the filename of the image,
 * where `hist1` originates from.
 *
 * @param fn2 is the legend entry corresponding to `hist2`, typically the filename of the image,
 * where `hist2` originates from.
 *
 * @param doDrawFrame is useful for testing. It switches off the axis and legend.
 *
 * When plotting two histograms in one diagram, the overlapping regions are drawn in violet. Around
 * each bar is a black frame, except their rounded up width is less than 4.
 *
 * @return the plot.
 */
imagefusion::Image plotHist(std::vector<unsigned int> const& hist1,
                            std::vector<unsigned int> const& hist2,
                            std::vector<double> const& bins,
                            imagefusion::Interval range,
                            imagefusion::Type /*basetype*/,
                            imagefusion::Size const& plotSize,
                            bool withLegend = true,
                            bool logplot = false,
                            bool grid = false,
                            bool gray = false,
                            std::string fn1 = "",
                            std::string fn2 = "",
                            bool doDrawFrame = true)
{
    assert(hist1.size() == hist2.size());
    assert(hist1.size() == bins.size());

    withLegend &= !fn1.empty() || !fn2.empty();
    const unsigned int nbins = hist1.size();

    // draw plot without frame
    if (!doDrawFrame)
        return drawHistPlot(hist1, hist2, plotSize, logplot, gray);

    // plot with frame section

    constexpr int framecolor = 128;

    // get max
    unsigned int maxcount1 = *std::max_element(hist1.begin(), hist1.end());
    unsigned int maxcount2 = *std::max_element(hist2.begin(), hist2.end());
    unsigned int maxcount  = std::max(maxcount1, maxcount2);

    // bin infos
    double imgMinLimit = range.lower();
    double imgMaxLimit = range.upper();

    // make plot image
    constexpr int zerosize = 14; // size (width and height) of a zero with current font and size
    int histFrameLeft   = 10 * zerosize;
    int histFrameBottom = 10 * zerosize;
    int histFrameTop    = 5 * zerosize;
    int histFrameRight  = 5 * zerosize;

    imagefusion::Image plot{
            plotSize.width  + histFrameLeft + histFrameRight,
            plotSize.height + histFrameBottom + histFrameTop,
            imagefusion::Type::uint8x3};
    plot.set(255);

    // y-axis ticks and labels
    imagefusion::Point orig{histFrameLeft, histFrameTop + plotSize.height};
    auto yticks_and_pxpos = logplot ? generateLogTicksWithPixelPositions(0, maxcount, plotSize.height)
                                    : generateTicksWithPixelPositions(0, maxcount, plotSize.height);
    drawYTicks(plot, orig, yticks_and_pxpos.second);
    placeYLabels(plot, orig, yticks_and_pxpos.first, yticks_and_pxpos.second, logplot);

    // x-axis ticks and labels
    std::vector<double> xticks(nbins);
    for (unsigned int i = 0; i < nbins; ++i)
        xticks[i] = bins[i];
    if (imgMinLimit < 0 && imgMaxLimit > 0) {
        auto pos = std::lower_bound(xticks.begin(), xticks.end(), 0);
        if (*pos != 0)
            xticks.insert(pos, 0);
    }

    std::vector<int> xpixticks(xticks.size());
    std::transform(xticks.begin(), xticks.end(), xpixticks.begin(), [&](double t){ return static_cast<int>(std::round((t - imgMinLimit) / (imgMaxLimit - imgMinLimit) * (plotSize.width - 1))); });

    drawXTicks(plot, orig, xpixticks);
    placeXLabels(plot, orig, xticks, xpixticks);

    // frame / axis
    cv::rectangle(plot.cvMat(), cv::Rect(histFrameLeft - 1, histFrameTop - 1, plotSize.width + 2, plotSize.height + 2), cv::Scalar::all(framecolor));
    arrowedLine(plot.cvMat(), cv::Point{histFrameLeft - 1, histFrameTop},
                              cv::Point{histFrameLeft - 1, histFrameTop - 20}, cv::Scalar::all(framecolor), 1, 8, 0, 0.5);

    // plot
    imagefusion::Image rawPlot = drawHistPlot(hist1, hist2, plotSize, logplot, gray);
    imagefusion::Image plotRegion{plot.sharedCopy({histFrameLeft, histFrameTop, plotSize.width, plotSize.height})};
    plotRegion.copyValuesFrom(rawPlot);

    // grid
    if (grid)
        drawHorizontalGrid(plotRegion, yticks_and_pxpos.first, yticks_and_pxpos.second);

    // legend
    imagefusion::Rectangle cr = detectBorderCropBounds(plot);
    plot.crop({cr.x, 0, cr.width, plot.height()});
    if (withLegend) {
        constexpr int fontFace = cv::FONT_HERSHEY_COMPLEX_SMALL;
        int baseline;
        int totalWidth = plot.width();
        auto doesFitIntoPlot = [&] (std::string const& s) { return cv::getTextSize(s, fontFace, 1, 1, &baseline).width < totalWidth; };
        auto shortened = shorten(fn1, fn2, doesFitIntoPlot, 1);
        cv::putText(plot.cvMat(), shortened[0], cv::Point{0, histFrameTop + plotSize.height + 2 * (zerosize + 6)},                     fontFace, 1, gray ? cv::Scalar::all(framecolor) : cv::Scalar(255, 128, 128), 1, CV_AA);
        cv::putText(plot.cvMat(), shortened[1], cv::Point{0, histFrameTop + plotSize.height + (fn1.empty() ? 2 : 3) * (zerosize + 6)}, fontFace, 1, gray ? cv::Scalar::all(framecolor) : cv::Scalar(128, 128, 255), 1, CV_AA);
    }
    plot.crop(detectBorderCropBounds(plot));

    return plot;
}

} /* annonymous namespace */

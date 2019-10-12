#include <chrono>
#include <memory>
#include <random>

#include <armadillo>

#include <boost/test/unit_test.hpp>

#include "spstfm.h"
#include "Image.h"
#include "Type.h"
#include "Starfm.h"

namespace imagefusion {
namespace spstfm_impl_detail {

BOOST_AUTO_TEST_SUITE(spstfm_suite)

// test uniqueRandomVector()
BOOST_AUTO_TEST_CASE(test_unique_random_numbers)
{
    // test that it is a permutation of 0:10
    auto vec = uniqueRandomVector(11);
    BOOST_CHECK_EQUAL(vec.size(), 11);
    for (int i = 0; i <= 10; ++i)
        BOOST_CHECK(std::find(vec.begin(), vec.end(), i) != vec.end());
}

// test mostVariance()
BOOST_AUTO_TEST_CASE(most_variance)
{
    constexpr unsigned int ps = 10;
    uint16_t b = 0;
    uint16_t w = std::numeric_limits<uint16_t>::max();
    uint16_t s = 256;
    uint16_t d = b + s;
    uint16_t l = w - s;

    // from high variance to low variance
    /*   img[0]      img[1]      img[2]      img[3]      img[4]      img[5]
     * 0000099999  0000000000  1111111111  1177117711  2222222222  3333333333
     * 0000099999  0000000000  8111111111  1177117711  2666666662  5555555555
     * 0000099999  0000000000  8811111111  1177117711  2622226262  3333333333
     * 0000099999  0000000000  8881111111  1177117711  2626666262  5555555555
     * 0000099999  8000000000  8888111111  1177117711  2626226262  3333333333
     * 0000099999  8888888888  8888811111  1177117711  2626226262  5555555555
     * 0000099999  8888888888  8888881111  1177117711  2626666262  3333333333
     * 0000099999  8888888888  8888888111  1177117711  2622222262  5555555555
     * 0000099999  8888888888  8888888811  1177117711  2666666662  3333333333
     * 0000099999  8888888888  8888888881  1177117711  2222222222  5555555555
     */
    std::array<Image, 6> patches;
    for (unsigned int i = 0; i < patches.size(); ++i)
        patches[i] = Image{ps, ps, Type::uint16x1};

    for (unsigned int y = 0; y < ps; ++y)
        patches[0].at<std::array<uint16_t, ps>>(0, y) = {b, b, b, b, b, w, w, w, w, w};


    for (unsigned int y = 0; y < ps; ++y)
        if (y < ps / 2)
            patches[1].at<std::array<uint16_t, ps>>(0, y) = {b, b, b, b, b, b, b, b, b, b};
        else
            patches[1].at<std::array<uint16_t, ps>>(0, y) = {l, l, l, l, l, l, l, l, l, l};
    patches[1].at<uint16_t>(0, 4) = l;

    for (unsigned int y = 0; y < ps; ++y)
        for (unsigned int x = 0; x < ps; ++x)
            patches[2].at<uint16_t>(x, y) = x >= y ? d : l;

    l -= s;
    patches[3].set(d);
    for (unsigned int x : {2,3, 6,7})
        for (unsigned int y = 0; y < ps; ++y)
            patches[3].at<uint16_t>(x, y) = l;

    d += 3*s;
    l -= 3*s;
    patches[4].set(d);
    for (unsigned int d : {1, 3}) {
        for (unsigned int c = d; c < ps - d; ++c) {
            patches[4].at<uint16_t>(c, d)    = l;
            patches[4].at<uint16_t>(c, ps-d) = l;
            patches[4].at<uint16_t>(d,    c) = l;
            patches[4].at<uint16_t>(ps-d, c) = l;
        }
    }

    d += 2*s;
    l -= 2*s;
    patches[5].set(d);
    for (unsigned int y : {1, 3, 5, 7, 9})
        patches[5].at<std::array<uint16_t, ps>>(0, y) = {l, l, l, l, l, l, l, l, l, l};

    // merge images as patches into one image
    constexpr unsigned int npx = 3;
    constexpr unsigned int npy = 2;
    assert(npx * npy == patches.size() && "Product must match the number of patches");
    Image all(npx * ps, npy * ps, patches[0].type());
    for (unsigned int py = 0; py < npy; ++py) {
        for (unsigned int px = 0; px < npx; ++px) {
            Image cropped{all.sharedCopy(Rectangle(px * ps, py * ps, ps, ps))};
            unsigned int pi = px + py * npx;
            cropped.copyValuesFrom(patches[pi]);
        }
    }
//    cv::namedWindow("orig", cv::WINDOW_NORMAL);
//    cv::imshow("orig", all.cvMat());
//    cv::waitKey(0);

    // test
    Rectangle fullArea{0, 0, all.width(), all.height()};
    Image mask{all.size(), Type::uint8x1};
    mask.set(255);
    auto vec = mostVariance(all, mask, ps, /*patchOverlap*/ 0, fullArea, /*channel*/ 0);
    auto expected = {0, 1, 2, 3, 4, 5};
    BOOST_CHECK_EQUAL_COLLECTIONS(vec.begin(), vec.end(), expected.begin(), expected.end());
}


// test duplicatesPatches()
BOOST_AUTO_TEST_CASE(find_duplicate_patches)
{
    constexpr unsigned int ps = 5;
    std::array<Image, 4> patches;
    for (unsigned int i = 0; i < patches.size(); ++i)
        patches[i] = Image{ps, ps, Type::uint16x1};

    for (unsigned int y = 0; y < ps; ++y) {
        patches[0].at<std::array<uint16_t, ps>>(0, y) = {0,1,2,3,4}; // same
        patches[1].at<std::array<uint16_t, ps>>(0, y) = {4,3,2,1,0};
        patches[2].at<std::array<uint16_t, ps>>(0, y) = {0,1,2,1,0};
        patches[3].at<std::array<uint16_t, ps>>(0, y) = {0,1,2,3,4}; // same
    }

    // merge images as patches into one image
    constexpr unsigned int npx = 2;
    constexpr unsigned int npy = 2;
    assert(npx * npy == patches.size() && "Product must match the number of patches");
    Image all(npx * ps, npy * ps, patches[0].type());
    for (unsigned int py = 0; py < npy; ++py) {
        for (unsigned int px = 0; px < npx; ++px) {
            Image cropped{all.sharedCopy(Rectangle(px * ps, py * ps, ps, ps))};
            unsigned int pi = px + py * npx;
            cropped.copyValuesFrom(patches[pi]);
        }
    }

    // test
    Rectangle fullArea{0, 0, all.width(), all.height()};
    Image mask{all.size(), Type::uint8x1};
    mask.set(255);
    auto vec = duplicatesPatches(all, mask, ps, /*patchOverlap*/ 0, fullArea, /*channel*/ 0);
    BOOST_CHECK_EQUAL(vec.size(), 1);
    BOOST_CHECK(vec.front() == 0 || vec.front() == 3);
}


/*
 * test mostlyInvalidPatches(), which returns patch indices that have mostly
 * invalid pixels in their mask.
 */
BOOST_AUTO_TEST_CASE(sort_out_invalid_patches) {
    constexpr unsigned int psize = 9;
    constexpr unsigned int npx = 9;
    constexpr unsigned int npy = 9;
    constexpr unsigned int imgwidth  = npx * psize;
    constexpr unsigned int imgheight = npy * psize;

    Image singleMask(imgwidth, imgheight, Type::uint8x1);
    Image multiMask(imgwidth, imgheight, Type::uint8x3);

    singleMask.set(255);
    multiMask.set(255);

    for (unsigned int pyi = 0; pyi < npy; ++pyi) {
        for (unsigned int pxi = 0; pxi < npx; ++pxi) {
            // get patch
            Rectangle crop(pxi * psize, pyi * psize, psize, psize);
            Image singlePatch{singleMask.sharedCopy(crop)};
            Image multiPatch{multiMask.sharedCopy(crop)};

            // set the invalid pixels to 0 (invalid)
            unsigned int pi = npx * pyi + pxi;
            auto sp_it = singlePatch.begin<uint8_t>(0);
            auto mp_it = multiPatch.begin<std::array<uint8_t,3>>();
            for (unsigned int count = 0; count < psize*psize; ++count) {
                // single channel mask gets so many invalid pixels as the patch index is: 0, 1, 2, ..., 78, 79, 80
                if (count < pi)
                    *sp_it++ = 0;

                // first channel of multi channel mask as well: 0, 1, 2, ... 80
                if (count < pi)
                    (*mp_it)[0] = 0;

                // second channel of multi channel mask gets double so many: 0, 2, 4, ..., 78, 80, 81, 81, 81, ...
                if (count < 2 * pi)
                    (*mp_it)[1] = 0;

                // third channel of multi channel mask gets half so many: 0, 0, 1, 1, 2, 2, ..., 38, 38, 39, 39, 40
                if (count < pi / 2)
                    (*mp_it)[2] = 0;
                ++mp_it;
            }
        }
    }

    Rectangle sampleArea{0, 0, imgwidth, imgheight}; // patches fit exactly by definition of imgwidth and imgheight
    std::vector<size_t> inval;
    double tol;
    unsigned int allowedPixels;

    // tolerance of 50% results in 40 out of 81 allowed invalid pixels
    tol = 0.5;
    allowedPixels = static_cast<unsigned int>(std::floor(psize*psize * tol));

    // for single mask the patch index *is* the number of invalid pixels, thus the first illegal index is 41 (one after the allowedPixels patch)
    inval = mostlyInvalidPatches(singleMask, tol, psize, /*pover*/ 0, sampleArea, /*channel*/ 2); // channel must be ignored for a single channel image
    BOOST_CHECK_EQUAL(inval.front(), allowedPixels + 1);
    BOOST_CHECK_EQUAL(inval.size(), npx*npy - (allowedPixels + 1));

    // same for the first channel of multi mask
    inval = mostlyInvalidPatches(multiMask, tol, psize, /*pover*/ 0, sampleArea, /*channel*/ 0);
    BOOST_CHECK_EQUAL(inval.front(), allowedPixels + 1);
    BOOST_CHECK_EQUAL(inval.size(), npx*npy - (allowedPixels + 1));

    // the second channel of multi mask has doubled the number of invalid pixels, i.e. patch 20 has 40 invalid pixels and patch 21 is illegal with 42 invalid pixels
    inval = mostlyInvalidPatches(multiMask, tol, psize, /*pover*/ 0, sampleArea, /*channel*/ 1);
    BOOST_CHECK_EQUAL(inval.front(), allowedPixels / 2 + 1); // first illegal patch index is 21
    BOOST_CHECK_EQUAL(inval.size(), npx*npy - (allowedPixels / 2 + 1));

    // the third channel of multi mask has halfed the number of invalid pixels. The last patch has exactly 40 invalid pixels which is allowedPixels. So there are no invalid patches.
    inval = mostlyInvalidPatches(multiMask, tol, psize, /*pover*/ 0, sampleArea, /*channel*/ 2);
    BOOST_CHECK(inval.empty());
}


// test extractPatch()
BOOST_AUTO_TEST_CASE(get_patch)
{
    Image img{256, 256, Type::uint16x3};
    uint16_t i = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x, ++i)
            img.at<std::array<uint16_t,3>>(x, y) = {
                    static_cast<uint16_t>(i),
                    static_cast<uint16_t>(65535 - i),
                    static_cast<uint16_t>(i / 10)};


//    int c = 1;
    // test that a double patch has correct values for all channels
    Rectangle fullArea{0, 0, img.width(), img.height()};
    for (unsigned int c = 0; c < img.channels(); ++c)
    {
        arma::mat patch = extractPatch(img, /*pxi*/ 0, /*pyi*/ 0, /*patchSize*/ 8, /*patchOverlap*/ 0, fullArea, /*channel*/ c);
        BOOST_REQUIRE_EQUAL(patch.n_cols, 1);
        BOOST_REQUIRE_EQUAL(patch.n_rows, 8 * 8);

        auto cropped = img.constSharedCopy(Rectangle{0, 0, 8, 8});
        auto it = patch.begin();
        for (int y = 0; y < cropped.height(); ++y)
            for (int x = 0; x < cropped.width(); ++x, ++it)
                if (*it != static_cast<double>(cropped.at<uint16_t>(x, y, c)))
                    BOOST_ERROR("Check double patch has failed at " + to_string(Point(x,y)) + ", because " + std::to_string(cropped.at<uint16_t>(x, y, c)) + " != " + std::to_string(*it));
    }

    // test OpenCV saturation behaviour:
    // from int to int8_t
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(-100), -100);
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(-200), std::numeric_limits<int8_t>::min());
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(100), 100);
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(200), std::numeric_limits<int8_t>::max());

    // from double to int8_t
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(-100.5), -100);
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(-200.5), std::numeric_limits<int8_t>::min());
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(100.5), 100);
    BOOST_CHECK_EQUAL(cv::saturate_cast<int8_t>(200.5), std::numeric_limits<int8_t>::max());

    // no saturation from double to int (as documented everywhere)
    // fails: BOOST_CHECK_EQUAL(cv::saturate_cast<int32_t>(1e100), std::numeric_limits<int32_t>::max());

    // test position of patch with different overlaps. It starts at pi * (patchSize - patchOverlap)
    arma::mat patch;
    Point p;

    patch = extractPatch(img, /*pxi*/ 3, /*pyi*/ 2, /*patchSize*/ 8, /*patchOverlap*/ 0, fullArea, /*channel*/ 0);
    p.x = static_cast<int>(std::round(patch(0))) % img.width();
    p.y = static_cast<int>(std::round(patch(0))) / img.width();
    BOOST_CHECK_EQUAL(p, Point(3 * 8, 2 * 8));

    patch = extractPatch(img, /*pxi*/ 3, /*pyi*/ 2, /*patchSize*/ 8, /*patchOverlap*/ 1, fullArea, /*channel*/ 0);
    p.x = static_cast<int>(std::round(patch(0))) % img.width();
    p.y = static_cast<int>(std::round(patch(0))) / img.width();
    BOOST_CHECK_EQUAL(p, Point(3 * 7, 2 * 7));

    patch = extractPatch(img, /*pxi*/ 3, /*pyi*/ 2, /*patchSize*/ 8, /*patchOverlap*/ 2, fullArea, /*channel*/ 0);
    p.x = static_cast<int>(std::round(patch(0))) % img.width();
    p.y = static_cast<int>(std::round(patch(0))) / img.width();
    BOOST_CHECK_EQUAL(p, Point(3 * 6, 2 * 6));
}

void visualizePatches(arma::mat const& patches, int npx, int npy) {
    int dim = patches.n_rows;
    int nSamples = patches.n_cols;
    assert(npx * npy >= nSamples);
    int psize = static_cast<int>(std::sqrt(dim));

    Image img(npx * (psize + 1), npy * (psize + 1), Type::uint8x1);
    img.set(0);

    int px = 0;
    int py = 0;
    for (int s = 0; s < nSamples; ++s) {
        for (unsigned int y = py * (psize + 1), yend = y + psize, d = 0; y < yend; ++y) {
            for (unsigned int x = px * (psize + 1), xend = x + psize; x < xend; ++x, ++d) {
                img.at<uint8_t>(x, y) = patches(d, s);
                std::cout << patches(d, s) << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
        ++px;
        if (px >= npx) {
            px = 0;
            ++py;
        }
    }

    cv::namedWindow("patches", cv::WINDOW_NORMAL);
    cv::imshow("patches", img.cvMat());
    cv::waitKey(0);
}


BOOST_AUTO_TEST_CASE(sample_function)
{
    constexpr int patchSize = 5;
    constexpr int dim = patchSize * patchSize;
    constexpr int patchOverlap = 1;
    constexpr int patchDist = patchSize - patchOverlap;
    Image img{5 * patchDist + patchOverlap, 6 * patchDist + patchOverlap, Type::int16x2};

    int16_t i = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x, ++i)
            img.at<std::array<int16_t,2>>(x, y) = {i, (int16_t)(-i)};

    Rectangle fullArea{0, 0, img.width(), img.height()};
    Image mask{img.size(), Type::uint8x1};
    mask.set(255);
    constexpr int nSamples = 10;
    arma::mat samples(dim, nSamples);
    initSingleSamples(img, samples, mask, /*fillValue*/ 0, uniqueRandomVector(nSamples), patchSize, patchOverlap, fullArea, 0);

    // check that every sample is somewhere in the image
    for (int s = 0; s < nSamples; ++s) {
        int val = static_cast<int>(samples(0, s));
        int x = std::abs(val % img.width());
        int y = std::abs(val / img.width());
        ConstImage cropped = img.sharedCopy(Rectangle(x, y, patchSize, patchSize));
        for (int y = 0; y < patchSize; ++y)
            for (int x = 0; x < patchSize; ++x)
                BOOST_CHECK_EQUAL(cropped.at<int16_t>(x, y, 0),
                                  static_cast<int16_t>(samples(x + y * patchSize, s)));
    }
}


// check that matrix-matrix multiplication works as expected. This is used at reconstruction for d_h * alpha or as code dictHigh.d * alpha
BOOST_AUTO_TEST_CASE(matrix_multiplication)
{
    // make some samples
    constexpr int nSamples = 5;
    constexpr int dim = 10;
    cv::Mat_<double> samples(dim, nSamples);
    samples.setTo(cv::Scalar::all(0));
    for (int x = 0; x < nSamples; ++x)
        for (int y = 0; y <= x; ++y)
            samples(y, x) = x + y * nSamples;

    // multiply with each unit vector and check that the result is the corresponding sample
    cv::Mat_<double> vec(nSamples, 1);
    for (int i = 0; i < nSamples; ++i) {
        // unit vector
        vec.setTo(cv::Scalar::all(0));
        vec(i) = 1;

        // multiply vector with sample matrix
        cv::Mat_<double> res = samples * vec;
        BOOST_REQUIRE_EQUAL(res.size(), cv::Size(1, dim));

        // check that the result is the corresponding column from the sample matrix
        for (int y = 0; y < dim; ++y)
            BOOST_CHECK_EQUAL(res(y), samples(y, i));
    }
}


// check that GPSR works for a simple case. It should prefer the non-optimal sparse solution
/* this compares the result to the corresponding matlab code, which uses the reference implementation:
 * dim = 5;
 * sample = (1:dim)';
 * dict = [eye(dim), sample+2];
 *
 * tau = 0.1 * max(abs(dict' * sample))
 * [alpha, alpha_debias] = GPSR_BB(sample, dict, tau, ...
 *     'ToleranceD', 1e-10, 'Debias',   1, 'Initialization', 0, ...
 *     'ToleranceA', 1e-6,  'MiniterD', 0, 'StopCriterion',  1)
 */
BOOST_AUTO_TEST_CASE(gpsr_simple)
{
    constexpr unsigned int dim = 5;
    constexpr unsigned int atoms = 6;
    arma::mat sample(dim, 1);
    arma::mat dict(dim, atoms, arma::fill::eye);

    for (unsigned int i = 0; i < dim; ++i) {
        sample(i) = i + 1;
        dict(i, dim) = sample(i) + 2;
    }

    SpstfmOptions::GPSROptions opts;
    opts.tolA = 1e-6;
    opts.debias = false;
    arma::vec x = gpsr(sample, dict, opts);
    BOOST_REQUIRE_EQUAL(x.n_rows, atoms);
    BOOST_REQUIRE_EQUAL(x.n_cols, 1);
    arma::vec nzs = arma::nonzeros(x);
    BOOST_CHECK_EQUAL(nzs.n_elem, 1);
    BOOST_CHECK_CLOSE_FRACTION(0.5666666666666667, x(atoms-1), 1e-10);
//    std::cout << x << std::endl;

    opts.tolD = 1e-10;
    opts.debias = true;
    x = gpsr(sample, dict, opts);
    BOOST_REQUIRE_EQUAL(x.n_rows, atoms);
    BOOST_REQUIRE_EQUAL(x.n_cols, 1);
    nzs = arma::nonzeros(x);
    BOOST_CHECK_EQUAL(nzs.n_elem, 1);
    BOOST_CHECK_CLOSE_FRACTION(0.6296296296296297, x(atoms-1), 1e-10);
//    std::cout << x << std::endl;
}

// check that GPSR with continuation gives slightly different results for alpha as without.
/* this compares the result to the corresponding matlab code, which uses the reference implementation:
 * dim = 5;
 * sample = eye(dim, 1);
 * dict = (1:dim)';
 * for i = 1:10
 *     dict(:,end+1) = dict(:,end) + 1;
 * end
 *
 * tau = 0.1 * max(abs(dict' * sample))
 * [alpha, alpha_debias] = GPSR_BB(sample, dict, tau, 'StopCriterion', 1, ...
 *     'ToleranceD', 1e-10, 'Debias',   1, 'Initialization', 0, ...
 *     'ToleranceA', 1e-6,  'MiniterD', 0, 'Continuation',   1)
 *
 * [alpha, alpha_debias] = GPSR_BB(sample, dict, tau, 'StopCriterion', 1, ...
 *     'ToleranceD', 1e-10, 'Debias',   1, 'Initialization', 0, ...
 *     'ToleranceA', 1e-6,  'MiniterD', 0, 'Continuation',   0)
 */
BOOST_AUTO_TEST_CASE(gpsr_continuation)
{
    constexpr unsigned int dim = 5;
    constexpr unsigned int atoms = 11;
    arma::mat sample(dim, 1, arma::fill::eye);
    arma::mat dict(dim, atoms);
    for (unsigned int i = 0; i < dim; ++i)
        for (unsigned int j = 0; j < atoms; ++j)
            dict(i, j) = i + j + 1;

    // without continuation, without debiasing
    SpstfmOptions::GPSROptions opts;
    opts.tolA = 1e-6;
    opts.debias = false;
    opts.continuation = false;
    arma::vec x1 = gpsr(sample, dict, opts);
    BOOST_REQUIRE_EQUAL(x1.n_rows, atoms);
    BOOST_REQUIRE_EQUAL(x1.n_cols, 1);
    arma::vec nzs = arma::nonzeros(x1);
    BOOST_CHECK_EQUAL(nzs.n_elem, 2);
    BOOST_CHECK_CLOSE_FRACTION(-0.044295259623484, x1(0), 1e-10);
    BOOST_CHECK_CLOSE_FRACTION(0.022205534927740, x1(atoms-1), 1e-10);

    // with continuation, without debiasing
    opts.continuation = true;
    arma::vec x2 = gpsr(sample, dict, opts);
    BOOST_REQUIRE_EQUAL(x2.n_rows, atoms);
    BOOST_REQUIRE_EQUAL(x2.n_cols, 1);
    nzs = arma::nonzeros(x2);
    BOOST_CHECK_EQUAL(nzs.n_elem, 2);
    BOOST_CHECK_CLOSE_FRACTION(-0.044203444551138, x2(0), 1e-10);
    BOOST_CHECK_CLOSE_FRACTION(0.022180344404474, x2(atoms-1), 1e-10);
//    std::cout << x1 << std::endl;
//    std::cout << x2 << std::endl;


    // without continuation, with debiasing
    opts.continuation = false;
    opts.tolD = 1e-10;
    opts.debias = true;
    x1 = gpsr(sample, dict, opts);
    BOOST_REQUIRE_EQUAL(x1.n_rows, atoms);
    BOOST_REQUIRE_EQUAL(x1.n_cols, 1);
    nzs = arma::nonzeros(x1);
    BOOST_CHECK_EQUAL(nzs.n_elem, 2);
    BOOST_CHECK_CLOSE_FRACTION(-0.28, x1(0), 1e-10);
    BOOST_CHECK_CLOSE_FRACTION(0.08, x1(atoms-1), 1e-10);

    // with continuation, with debiasing
    opts.continuation = true;
    x2 = gpsr(sample, dict, opts);
    BOOST_REQUIRE_EQUAL(x2.n_rows, atoms);
    BOOST_REQUIRE_EQUAL(x2.n_cols, 1);
    nzs = arma::nonzeros(x2);
    BOOST_CHECK_EQUAL(nzs.n_elem, 2);
    BOOST_CHECK_CLOSE_FRACTION(-0.28, x2(0), 1e-10);
    BOOST_CHECK_CLOSE_FRACTION(0.08, x2(atoms-1), 1e-10);
//    std::cout << x1 << std::endl;
//    std::cout << x2 << std::endl;
}


// check the result of a singular value decomposition received by OpenCV and by Armadillo
// result: Armadillo (6.7) gives the exact same result as GNU Octave (4.0.3), OpenCV gives a different result (and is slower than Armadillo).
BOOST_AUTO_TEST_CASE(svd) {
    using namespace std::chrono;
    constexpr int rows = 30;
    constexpr int cols = rows / 3;
    cv::Mat_<double> A(rows, cols);
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            A(i,j) = i + j;

    // calculate u, w, vt in A = u * w * vt
//    steady_clock::time_point svd_start_cv = steady_clock::now();
    cv::SVD svd(A);
//    steady_clock::time_point svd_end_cv = steady_clock::now();
//    duration<double> svd_duration_cv = duration_cast<duration<double>>(svd_end_cv - svd_start_cv);
//    std::cout << "OpenCV SVD took " << svd_duration_cv.count() << " s." << std::endl;

    BOOST_CHECK(svd.u.depth()  == CV_64F);
    BOOST_CHECK(svd.w.depth()  == CV_64F);
    BOOST_CHECK(svd.vt.depth() == CV_64F);

    // check that (because we do not use cv::SVD::FULL_UV):
    // u is 10 x 3 (with cv::SVD::FULL_UV it would be 10 x 10)
    BOOST_CHECK_EQUAL(svd.u.rows, A.rows);
    BOOST_CHECK_EQUAL(svd.u.cols, A.cols);

    // w is 3 x 1 as the diagonal of a 3 x 3 matrix
    BOOST_CHECK_EQUAL(svd.w.rows, A.cols);
    BOOST_CHECK_EQUAL(svd.w.cols, 1);

    // vt is 3 x 3, already transposed
    BOOST_CHECK_EQUAL(svd.vt.cols, A.cols);
    BOOST_CHECK_EQUAL(svd.vt.rows, A.cols);

//    std::cout << svd.u << std::endl;
//    std::cout << svd.w << std::endl;
//    std::cout << svd.vt << std::endl;

    arma::mat B(rows, cols);
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            B(i,j) = i + j;

    // calculate u, s, v in A = u * s * v'
    arma::mat u1;
    arma::vec s1;
    arma::mat v1;
    steady_clock::time_point svd_start_arma = steady_clock::now();
    svd_econ(u1, s1, v1, B);
    steady_clock::time_point svd_end_arma = steady_clock::now();
    duration<double> svd_duration_arma = duration_cast<duration<double>>(svd_end_arma - svd_start_arma);
//    std::cout << "Armadillo SVD took " << svd_duration_arma.count() << " s." << std::endl;

    // check that (because by default left and right singular vectors are computed):
    // u is 10 x 3
    BOOST_CHECK_EQUAL(u1.n_rows, B.n_rows);
    BOOST_CHECK_EQUAL(u1.n_cols, B.n_cols);

    // s is 3 x 1 as the diagonal of a 3 x 3 matrix
    BOOST_CHECK_EQUAL(s1.n_rows, B.n_cols);
    BOOST_CHECK_EQUAL(s1.n_cols, 1);

    // v is 3 x 3
    BOOST_CHECK_EQUAL(v1.n_cols, B.n_cols);
    BOOST_CHECK_EQUAL(v1.n_rows, B.n_cols);

//    std::cout << s1 << std::endl;

    arma::mat u2;
    arma::vec s2;
    arma::mat v2;
    svd_start_arma = steady_clock::now();
    arma::mat C(A.ptr<double>(), rows, cols, /*copy*/ false, /*fixed size*/ true);
    C.reshape(C.n_cols, C.n_rows); // does not copy data
    C = std::move(C).t();          // modifies the memory in-place (modifies A)
    svd_econ(u2, s2, v2, std::move(C));
    svd_end_arma = steady_clock::now();
    svd_duration_arma = duration_cast<duration<double>>(svd_end_arma - svd_start_arma);
//    std::cout << "Armadillo SVD took " << svd_duration_arma.count() << " s." << std::endl;

    // check that (because by default left and right singular vectors are computed):
    // u is 10 x 3
    BOOST_CHECK_EQUAL(u2.n_rows, B.n_rows);
    BOOST_CHECK_EQUAL(u2.n_cols, B.n_cols);

    // s is 3 x 1 as the diagonal of a 3 x 3 matrix
    BOOST_CHECK_EQUAL(s2.n_rows, B.n_cols);
    BOOST_CHECK_EQUAL(s2.n_cols, 1);

    // v is 3 x 3
    BOOST_CHECK_EQUAL(v2.n_cols, B.n_cols);
    BOOST_CHECK_EQUAL(v2.n_rows, B.n_cols);

//    std::cout << s2 << std::endl;

    BOOST_CHECK_EQUAL_COLLECTIONS(u1.begin(), u1.end(), u2.begin(), u2.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(s1.begin(), s1.end(), s2.begin(), s2.end());
    BOOST_CHECK_EQUAL_COLLECTIONS(v1.begin(), v1.end(), v2.begin(), v2.end());

    // cannot test that... they are too different
//    for (int i = 0; i < rows; ++i)
//        for (int j = 0; j < cols; ++j)
//            BOOST_CHECK_CLOSE_FRACTION(std::abs(svd.u.at<double>(i, j)), std::abs(u1(i,j), 1e-10));

    // for the singular values check only absolute tolerance, since for small values, they are too different
    for (int i = 0; i < cols; ++i)
        BOOST_CHECK_LE(std::abs(svd.w.at<double>(i) - s1(i)), 1e-13);

//    std::cout << A << std::endl << std::endl;
//    std::cout << B << std::endl << std::endl;
//    std::cout << C << std::endl << std::endl;
//    std::cout << svd.u << std::endl << std::endl;
//    std::cout << u1 << std::endl << std::endl << std::endl;
//    std::cout << svd.vt << std::endl << std::endl;
//    std::cout << v1 << std::endl << std::endl;
}

// check objective function
BOOST_AUTO_TEST_CASE(objective_function_test) {
    arma::mat A{{1, 2, 3},
                {4, 5, 6}};
    arma::mat B{{10, -12},
                {-11, 13}};
    arma::mat C{{1, -1, 1},
                {2, 2, -2}};
    double scale = 1. / A.n_elem;

    // check squared norm of A
    BOOST_CHECK_CLOSE(objective_simple(A, arma::zeros<arma::mat>(2, 2), C, 0), (1*1 + 2*2 + 3*3 + 4*4 + 5*5 + 6*6) * scale, 1e-10); // = 91 / 6

    // check l2 part
    BOOST_CHECK_CLOSE(objective_simple(A, B, C, 0), 5476 * scale, 1e-10); // number calculated in Octave with:1 (norm(A - B * C, 'fro'))^2

    // check l1 part
    BOOST_CHECK_CLOSE(objective_simple(A, B, C, 1) - 5476 * scale, 9 * scale, 1e-10); // 1 + 1 + 1 + 2 + 2 + 2

    // check improved l1 part
    C = arma::mat{{3, -2, 1},
                  {8, 2, -4}};
    std::vector<double> taus{1, 2, 3};
    BOOST_CHECK_CLOSE(objective_improved(A, B, C, taus) - 20729 * scale, 34 * scale, 1e-10); // (norm(A - B * C, 'fro'))^2 = 20729,  (3 + 8) * 1 + (2 + 2) * 2 + (1 + 4) * 3 = 34

    // verify that l1 norm of a vector is just the abs sum
    arma::vec v{1, -2, 3, -4};
    BOOST_CHECK_EQUAL(arma::norm(v, 1), 10);
}

// check sorting indices
BOOST_AUTO_TEST_CASE(sort_indices_test) {
    //                 0 1 2 3 4 5
    std::vector<int> v{5,1,4,2,0,3};
    std::vector<size_t> exp{0,2,5,3,1,4};
    std::vector<size_t> i = sort_indices(v);
    BOOST_CHECK_EQUAL_COLLECTIONS(exp.begin(), exp.end(), i.begin(), i.end());
}

BOOST_AUTO_TEST_CASE(copy_arma_to_opencv) {
    arma::mat A{{1, 2, 3},
                {4, 5, 6}};
    Image B{5, 5, Type::uint8x2};

    copy<uint8_t, arma::mat>(A, B, 1);

    for (unsigned int y = 0; y < A.n_rows; ++y)
        for (unsigned int x = 0; x < A.n_cols; ++x)
            BOOST_CHECK_EQUAL(B.at<uint8_t>(x, y, 1), A(y, x));
}

// check that the objective function decreases after updating the columns. Note, this is not in general the case, only for the selected numbers. So if this test fails, the algorithms have changed.
BOOST_AUTO_TEST_CASE(ksvd_test) {
    constexpr unsigned int dim = 3;
    constexpr unsigned int natoms = 5;
    constexpr unsigned int nsamples = 10;
    arma::mat samples(dim, nsamples);
    for (unsigned int i = 0; i < samples.n_rows; ++i)
        for (unsigned int j = 0; j < samples.n_cols; ++j)
            samples(i,j) = i + j;

    // subtract mean and variance
    samples -= arma::mean(arma::mean(samples));
    samples /= arma::var(arma::vectorise(samples));
//    std::cout << "samples:" << std::endl << samples << std::endl;

    arma::mat dict_mat(dim, natoms);
    dict_mat = samples.head_cols(natoms);
//    std::cout << "dict0:" << std::endl << dict.d << std::endl;

    // get sparse coefficients
    arma::mat coeff(natoms, nsamples);
    double maxtau = 0;
    SpstfmOptions::GPSROptions opts;
    opts.tolA = 1e-6;
    opts.tolD = 1e-1;
    opts.minIterD = 1;
    opts.debias = true;
    opts.continuation = true;
    for (unsigned int c = 0; c < nsamples; ++c) {
        auto sample = samples.col(c);
        arma::vec sparse = gpsr(sample, dict_mat, opts, &maxtau);
        coeff.col(c) = sparse;
    }
//    std::cout << "coeff0:" << std::endl << coeff << std::endl;
//    std::cout << "error0:" << std::endl << (samples - dict.d * coeff) << std::endl;


    // check objective function
    double obj1 = objective_simple(samples, dict_mat, coeff, maxtau);
//    std::cout << "F0: " << obj1 << ", tau: " << maxtau << std::endl;

    // update dictionary columns with K-SVD
    dict_mat = ksvd(samples, dict_mat, coeff, true, SpstfmOptions::DictionaryNormalization::independent);
//    std::cout << "dict1:" << std::endl << dict.d << std::endl;

    // get sparse coefficients for updated dictionary
    maxtau = 0;
    for (unsigned int c = 0; c < nsamples; ++c) {
        auto sample = samples.col(c);
        arma::vec sparse = gpsr(sample, dict_mat, opts, &maxtau);
        coeff.col(c) = sparse;
    }
//    std::cout << "coeff1:" << std::endl << coeff << std::endl;
//    std::cout << "error1:" << std::endl << (samples - dict.d * coeff) << std::endl;

    // check objective function again
    double obj2 = objective_simple(samples, dict_mat, coeff, maxtau);
//    std::cout << "F1: " << obj2 << ", tau: " << maxtau << std::endl;

    // check that the objective function decreased
    BOOST_CHECK_LT(obj2, obj1);
}

// test simple reconstruction, i.e. eye-dictionary, with full image size
BOOST_AUTO_TEST_CASE(simple_reconstruction) {
    SpstfmOptions opt;
    const unsigned int dist = opt.getPatchSize() - opt.getPatchOverlap();
    constexpr unsigned int npx = 3;
    constexpr unsigned int npy = 4;
    const unsigned int imgwidth  = npx * dist + opt.getPatchOverlap();
    const unsigned int imgheight = npy * dist + opt.getPatchOverlap();
    constexpr unsigned int max = std::numeric_limits<uint16_t>::max() - 1;
    constexpr unsigned int avg = max / 2;

    // set one dark, one mid and one bright image
    Image bright(imgwidth, imgheight, Type::uint16x2);
    Image dark(bright.size(), Type::uint16x2);
    Image mid(bright.size(), Type::uint16x2);
    for (int y = 0; y < bright.height(); ++y) {
        for (int x = 0; x < bright.width(); ++x) {
            bright.at<uint16_t>(x, y, 1) = max - y * imgwidth - x;
            dark.at<uint16_t>(x, y, 1) = y * imgwidth + x;
        }
    }
    mid.set(avg);
    Rectangle nocrop{0, 0, bright.width(), bright.height()};


    // prepare dictionary trainer for reconstruction using unit dictionaries
    Image output{bright.size(), Type::uint16x2};
    unsigned int dim = opt.getPatchSize() * opt.getPatchSize();
//    opt.setSparsityWeight(0);
    DictTrainer dt;
    dt.output = Image{output.sharedCopy()};
    dt.opt = opt;
    dt.dictsConcat.emplace_back(2 * dim, dim);
    dt.dictsConcat.emplace_back(2 * dim, dim);
    dt.sampleMask = Image{mid.size(), Type::uint8x1};
    dt.sampleMask.set(255);
    highMatView(dt.dictsConcat.at(1)) = arma::eye<arma::mat>(dim, dim);
    lowMatView(dt.dictsConcat.at(1))  = arma::eye<arma::mat>(dim, dim);

    // reconstruct from dark to mid
    dt.weights1 = arma::ones<arma::mat>(npy, npx);
    dt.weights3 = arma::zeros<arma::mat>(npy, npx);
    BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, nocrop, nocrop, /*channel*/ 1));
    for (int y = 0; y < bright.height(); ++y)
        for (int x = 0; x < bright.width(); ++x)
            BOOST_CHECK_EQUAL(output.at<uint16_t>(x, y, 1), avg);

    // reconstruct from bright to mid
    dt.weights1 = arma::zeros<arma::mat>(npy, npx);
    dt.weights3 = arma::ones<arma::mat>(npy, npx);
    BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, nocrop, nocrop, /*channel*/ 1));
    for (int y = 0; y < bright.height(); ++y)
        for (int x = 0; x < bright.width(); ++x)
            BOOST_CHECK_EQUAL(output.at<uint16_t>(x, y, 1), avg);

    // reconstruct from dark and bright to mid
    dt.weights1 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.weights3 = arma::ones<arma::mat>(npy, npx) * 0.5;
    BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, nocrop, nocrop, /*channel*/ 1));
    for (int y = 0; y < bright.height(); ++y)
        for (int x = 0; x < bright.width(); ++x)
            BOOST_CHECK_EQUAL(output.at<uint16_t>(x, y, 1), avg);

//    Image chan1{output.size(), output.basetype()};
//    cv::mixChannels(output.cvMat(), chan1.cvMat(), std::vector<int>{1, 0});
//    std::cout << chan1.cvMat() << std::endl;
//    cv::namedWindow("reconst", cv::WINDOW_NORMAL);
//    cv::imshow("reconst", chan1.cvMat());
//    cv::waitKey(0);
}

// test reconstruction with different sizes (tests crop)
BOOST_AUTO_TEST_CASE(reconstruction_zero_overlap) {
    // zero overlap
    SpstfmOptions opt;
    opt.setPatchSize(5);
    opt.setPatchOverlap(0);
    const unsigned int dist = opt.getPatchSize() - opt.getPatchOverlap();
    constexpr unsigned int npx = 5;
    constexpr unsigned int npy = 3;
    const unsigned int imgwidth  = npx * dist + opt.getPatchOverlap();
    const unsigned int imgheight = npy * dist + opt.getPatchOverlap();
    constexpr unsigned int max = std::numeric_limits<uint16_t>::max() - 1;
    constexpr unsigned int avg = max / 2;

    // set one dark, one mid and one bright image
    Image bright(imgwidth, imgheight, Type::uint16x1);
    Image dark(bright.size(), Type::uint16x1);
    Image mid(bright.size(), Type::uint16x1);
    for (int y = 0; y < bright.height(); ++y) {
        for (int x = 0; x < bright.width(); ++x) {
            bright.at<uint16_t>(x, y) = max - y * imgwidth - x;
            mid.at<uint16_t>(x, y)    = avg - y * imgwidth + x;
            dark.at<uint16_t>(x, y)   = y * imgwidth + x;
        }
    }

    // prepare dictionary trainer for reconstruction
    unsigned int dim = opt.getPatchSize() * opt.getPatchSize();
//    opt.setSparsityWeight(0);
    DictTrainer dt;
    dt.dictsConcat.emplace_back(2 * dim, dim);
    highMatView(dt.dictsConcat.front()) = arma::eye<arma::mat>(dim, dim);
    lowMatView(dt.dictsConcat.front())  = arma::eye<arma::mat>(dim, dim);
    dt.weights1 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.weights3 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.sampleMask = Image{mid.size(), Type::uint8x1};
    dt.sampleMask.set(255);
    dt.opt = opt;

    // reconstruct for different crop offsets
    Rectangle fullArea{0, 0, mid.width(), mid.height()};
    for (unsigned int xoffset = 0; xoffset < dist; ++xoffset) {
        for (unsigned int yoffset = 0; yoffset < dist; ++yoffset) {
//            std::cout << "xoffset: " << xoffset << ", yoffset: " << yoffset << std::endl;
            dt.output = Image(bright.width() - xoffset, bright.height() - yoffset, Type::uint16x1);
            ConstImage const& out = dt.output;

            Rectangle crop(xoffset, yoffset, out.width(), out.height());
            BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, crop, fullArea, /*channel*/ 0));
            for (int y = 0; y < out.height(); ++y) {
                for (int x = 0; x < out.width(); ++x) {
//                    std::cout << "x: " << x << ", y: " << y << ", xoffset: " << xoffset << ", yoffset: " << yoffset
//                              << ", out.size(): " << out.size() << ", mid.size(): " << mid.size() << std::endl;
                    BOOST_CHECK_EQUAL(out.at<uint16_t>(x, y), mid.at<uint16_t>(xoffset + x, yoffset + y));
                }
            }

            crop = Rectangle(0, 0, out.width(), out.height());
            dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, crop, fullArea, /*channel*/ 0);
            for (int y = 0; y < out.height(); ++y)
                for (int x = 0; x < out.width(); ++x)
                    BOOST_CHECK_EQUAL(out.at<uint16_t>(x, y), mid.at<uint16_t>(x, y));
        }
    }
}

// test limit case with an image consisting of 1 patch with crop sizes from 1x1
BOOST_AUTO_TEST_CASE(reconstruction_one_patch) {
    // zero overlap
    constexpr unsigned int psize = 5;
    SpstfmOptions opt;
    opt.setPatchSize(psize);
    opt.setPatchOverlap(0);
    constexpr unsigned int npx = 1;
    constexpr unsigned int npy = 1;
    const unsigned int imgwidth  = npx * psize + opt.getPatchOverlap();
    const unsigned int imgheight = npy * psize + opt.getPatchOverlap();
    constexpr unsigned int max = std::numeric_limits<uint16_t>::max() - 1;
    constexpr unsigned int avg = max / 2;

    // set one dark, one mid and one bright image
    Image bright(imgwidth, imgheight, Type::uint16x1);
    Image dark(bright.size(), Type::uint16x1);
    Image mid(bright.size(), Type::uint16x1);
    for (int y = 0; y < bright.height(); ++y) {
        for (int x = 0; x < bright.width(); ++x) {
            bright.at<uint16_t>(x, y) = max - y * imgwidth - x;
            mid.at<uint16_t>(x, y)    = avg - y * imgwidth + x;
            dark.at<uint16_t>(x, y)   = y * imgwidth + x;
        }
    }

    // prepare dictionary trainer for reconstruction
    unsigned int dim = psize * psize;
//    opt.setSparsityWeight(0);
    DictTrainer dt;
    dt.dictsConcat.emplace_back(2 * dim, dim);
    highMatView(dt.dictsConcat.front()) = arma::eye<arma::mat>(dim, dim);
    lowMatView(dt.dictsConcat.front())  = arma::eye<arma::mat>(dim, dim);
    dt.weights1 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.weights3 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.sampleMask = Image{mid.size(), Type::uint8x1};
    dt.sampleMask.set(255);
    dt.opt = opt;

    // reconstruct for different crop offsets
    Rectangle fullArea{0, 0, mid.width(), mid.height()};
    for (unsigned int cropsize = 1; cropsize <= imgwidth; ++cropsize) {
        dt.output = Image(cropsize, cropsize, Type::uint16x1);
        ConstImage const& out = dt.output;
        for (unsigned int offset = 0; offset <= imgwidth - cropsize; ++offset) {
//            std::cout << "cropsize: " << cropsize << ", offset: " << offset << std::endl;
            Rectangle crop(offset, offset, cropsize, cropsize);
            BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, crop, fullArea, /*channel*/ 0));
            for (int y = 0; y < out.height(); ++y)
                for (int x = 0; x < out.width(); ++x)
                    BOOST_CHECK_EQUAL(out.at<uint16_t>(x, y), mid.at<uint16_t>(offset + x, offset + y));
        }
    }
}

// test limit case with an image consisting of 1 patch row with crop sizes from 3x1
BOOST_AUTO_TEST_CASE(reconstruction_one_patch_row) {
    // zero overlap
    constexpr unsigned int psize = 5;
    SpstfmOptions opt;
    opt.setPatchSize(psize);
    opt.setPatchOverlap(0);
    constexpr unsigned int npx = 3;
    constexpr unsigned int npy = 1;
    const unsigned int imgwidth  = npx * psize + opt.getPatchOverlap();
    const unsigned int imgheight = npy * psize + opt.getPatchOverlap();
    constexpr unsigned int max = std::numeric_limits<uint16_t>::max() - 1;
    constexpr unsigned int avg = max / 2;

    // set one dark, one mid and one bright image
    Image bright(imgwidth, imgheight, Type::uint16x1);
    Image dark(bright.size(), Type::uint16x1);
    Image mid(bright.size(), Type::uint16x1);
    for (int y = 0; y < bright.height(); ++y) {
        for (int x = 0; x < bright.width(); ++x) {
            bright.at<uint16_t>(x, y) = max - y * imgwidth - x;
            mid.at<uint16_t>(x, y)    = avg - y * imgwidth + x;
            dark.at<uint16_t>(x, y)   = y * imgwidth + x;
        }
    }

    // prepare dictionary trainer for reconstruction
    unsigned int dim = psize * psize;
//    opt.setSparsityWeight(0);
    DictTrainer dt;
    dt.dictsConcat.emplace_back(2 * dim, dim);
    highMatView(dt.dictsConcat.front()) = arma::eye<arma::mat>(dim, dim);
    lowMatView(dt.dictsConcat.front())  = arma::eye<arma::mat>(dim, dim);
    dt.weights1 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.weights3 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.sampleMask = Image{mid.size(), Type::uint8x1};
    dt.sampleMask.set(255);
    dt.opt = opt;

    // reconstruct for different crop offsets
    Rectangle fullArea{0, 0, mid.width(), mid.height()};
    for (unsigned int cropheight = 1; cropheight <= imgheight; ++cropheight) {
        int cropwidth = npx / npy * cropheight;
        dt.output = Image(cropwidth, cropheight, Type::uint16x1);
        ConstImage const& out = dt.output;
        for (unsigned int yoffset = 0; yoffset <= imgheight - cropheight; ++yoffset) {
            int xoffset = npx / npy * yoffset;
//            std::cout << "cropheight: " << cropheight << ", cropwidth: " << cropwidth << ", xoffset: " << xoffset << ", yoffset: " << yoffset << std::endl;
            Rectangle crop(xoffset, yoffset, cropwidth, cropheight);
            BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, crop, fullArea, /*channel*/ 0));
            for (int y = 0; y < out.height(); ++y) {
                for (int x = 0; x < out.width(); ++x) {
//                    std::cout << "x: " << x << ", y: " << y << ", xoffset: " << xoffset << ", yoffset: " << yoffset
//                              << ", out.size(): " << out.size() << ", mid.size(): " << mid.size() << std::endl;
                    BOOST_CHECK_EQUAL(out.at<uint16_t>(x, y), mid.at<uint16_t>(xoffset + x, yoffset + y));
                }
            }
        }
    }
}

// test limit case with an image consisting of 1 patch column with crop sizes from 1x3
BOOST_AUTO_TEST_CASE(reconstruction_one_patch_column) {
    // zero overlap
    constexpr unsigned int psize = 5;
    SpstfmOptions opt;
    opt.setPatchSize(psize);
    opt.setPatchOverlap(0);
    constexpr unsigned int npx = 1;
    constexpr unsigned int npy = 3;
    const unsigned int imgwidth  = npx * psize + opt.getPatchOverlap();
    const unsigned int imgheight = npy * psize + opt.getPatchOverlap();
    constexpr unsigned int max = std::numeric_limits<uint16_t>::max() - 1;
    constexpr unsigned int avg = max / 2;

    // set one dark, one mid and one bright image
    Image bright(imgwidth, imgheight, Type::uint16x1);
    Image dark(bright.size(), Type::uint16x1);
    Image mid(bright.size(), Type::uint16x1);
    for (int y = 0; y < bright.height(); ++y) {
        for (int x = 0; x < bright.width(); ++x) {
            bright.at<uint16_t>(x, y) = max - y * imgwidth - x;
            mid.at<uint16_t>(x, y)    = avg - y * imgwidth + x;
            dark.at<uint16_t>(x, y)   = y * imgwidth + x;
        }
    }

    // prepare dictionary trainer for reconstruction
    unsigned int dim = psize * psize;
//    opt.setSparsityWeight(0);
    DictTrainer dt;
    dt.dictsConcat.emplace_back(2 * dim, dim);
    highMatView(dt.dictsConcat.front()) = arma::eye<arma::mat>(dim, dim);
    lowMatView(dt.dictsConcat.front())  = arma::eye<arma::mat>(dim, dim);
    dt.weights1 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.weights3 = arma::ones<arma::mat>(npy, npx) * 0.5;
    dt.sampleMask = Image{mid.size(), Type::uint8x1};
    dt.sampleMask.set(255);
    dt.opt = opt;

    // reconstruct for different crop offsets
    Rectangle fullArea{0, 0, mid.width(), mid.height()};
    for (unsigned int cropwidth = 1; cropwidth <= imgwidth; ++cropwidth) {
        int cropheight = npy / npx * cropwidth;
        dt.output = Image(cropwidth, cropheight, Type::uint16x1);
        ConstImage const& out = dt.output;
        for (unsigned int xoffset = 0; xoffset <= imgwidth - cropwidth; ++xoffset) {
            int yoffset = npy / npx * xoffset;
//            std::cout << "cropheight: " << cropheight << ", cropwidth: " << cropwidth << ", xoffset: " << xoffset << ", yoffset: " << yoffset << std::endl;
            Rectangle crop(xoffset, yoffset, cropwidth, cropheight);
            BOOST_CHECK_NO_THROW(dt.reconstructImage(dark, bright, dark, mid, bright, /*fillL21*/ 0, /*fillL23*/ 0, crop, fullArea, /*channel*/ 0));
            for (int y = 0; y < out.height(); ++y) {
                for (int x = 0; x < out.width(); ++x) {
//                    std::cout << "x: " << x << ", y: " << y << ", xoffset: " << xoffset << ", yoffset: " << yoffset
//                              << ", out.size(): " << out.size() << ", mid.size(): " << mid.size() << std::endl;
                    BOOST_CHECK_EQUAL(out.at<uint16_t>(x, y), mid.at<uint16_t>(xoffset + x, yoffset + y));
                }
            }
        }
    }
}

// test padding of patches
BOOST_AUTO_TEST_CASE(padding_patches) {
    constexpr unsigned int size = 10;
    Image img{size, size, Type::uint8x2};
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            img.at<uint8_t>(x, y, 0) = x + size * y;
            img.at<uint8_t>(x, y, 1) = 255 - img.at<uint8_t>(x, y, 0);
        }
    }

    // make extended sample area
    Rectangle fullArea{-img.width(), -img.height(), 3*img.width(), 3*img.height()};

    // choose exact matching size, so there are 2 x 2 patches: |0 1|2 3|4 5|
    unsigned int psize = 5;                                 // |   |img|   |
    constexpr unsigned int pover = 0;

    arma::mat p, ref;
    // negative diagonal
    // get non-existing patch from (-5, -5) to (-1, -1),
    p = extractPatch(img, /*px*/ 1, /*py*/ 1, psize, pover, fullArea, /*channel*/ 1);
    p.reshape(psize, psize);
    p = p.t();

    // should be the same as the patch from (0, 0) to (4, 4) but flipped in both directions
    ref = extractPatch(img, /*px*/ 2, /*py*/ 2, psize, pover, fullArea, /*channel*/ 1);
    ref.reshape(psize, psize);
    ref = arma::flipud(arma::fliplr(ref.t()));
    BOOST_CHECK(arma::approx_equal(p, ref, "absdiff", 0));

    // right
    // get non-existing patch from (10, 0) to (14, 4),
    p = extractPatch(img, /*px*/ 5, /*py*/ 2, psize, pover, fullArea, /*channel*/ 0);
    p.reshape(psize, psize);
    p = p.t();

    // should be the same as the patch from (5, 0) to (9, 4) but flipped horizontally
    ref = extractPatch(img, /*px*/ 2, /*py*/ 2, psize, pover, fullArea, /*channel*/ 0);
    ref.reshape(psize, psize);
    ref = arma::fliplr(ref.t());
    BOOST_CHECK(arma::approx_equal(p, ref, "absdiff", 0));

    // lower bound
    // get non-existing patch from (0, 10) to (4, 14),
    p = extractPatch(img, /*px*/ 2, /*py*/ 4, psize, pover, fullArea, /*channel*/ 0);
    p.reshape(psize, psize);
    p = p.t();

    // should be the same as the patch from (0, 5) to (4, 9) but flipped vertically
    ref = extractPatch(img, /*px*/ 2, /*py*/ 3, psize, pover, fullArea, /*channel*/ 0);
    ref.reshape(psize, psize);
    ref = arma::flipud(ref.t());
    BOOST_CHECK(arma::approx_equal(p, ref, "absdiff", 0));

    // check out of bounds errors
    BOOST_CHECK_THROW(extractPatch(img, /*px*/ -1, /*py*/  0, psize, pover, fullArea, /*channel*/ 0), size_error);
    BOOST_CHECK_THROW(extractPatch(img, /*px*/  0, /*py*/ -1, psize, pover, fullArea, /*channel*/ 0), size_error);
    BOOST_CHECK_THROW(extractPatch(img, /*px*/  6, /*py*/  0, psize, pover, fullArea, /*channel*/ 0), size_error);
    BOOST_CHECK_THROW(extractPatch(img, /*px*/  0, /*py*/  6, psize, pover, fullArea, /*channel*/ 0), size_error);

    // not out of bounds errors, but almost
    BOOST_CHECK_NO_THROW(extractPatch(img, /*px*/  0, /*py*/  0, psize, pover, fullArea, /*channel*/ 0));
    BOOST_CHECK_NO_THROW(extractPatch(img, /*px*/  0, /*py*/  0, psize, pover, fullArea, /*channel*/ 0));
    BOOST_CHECK_NO_THROW(extractPatch(img, /*px*/  5, /*py*/  0, psize, pover, fullArea, /*channel*/ 0));
    BOOST_CHECK_NO_THROW(extractPatch(img, /*px*/  0, /*py*/  5, psize, pover, fullArea, /*channel*/ 0));


    // choose non-exact matching size, so there are 2.5 x 2.5 patches: |0 1 2 3 4|5 6 7
    psize = 4;                                                      // |    |img |    |

    // left bound
    // get half-existing patch from (-2, 2) to (1, 5)
    p = extractPatch(img, /*px*/ 2, /*py*/ 3, psize, pover, fullArea, /*channel*/ 0);
    p.reshape(psize, psize);
    p = p.t();
    BOOST_CHECK(arma::approx_equal(p, arma::fliplr(p), "absdiff", 0));
    for (int y = 0; y < (int)psize; ++y)
        for (int x = 0; x < (int)psize / 2; ++x)
            BOOST_CHECK_EQUAL(p(y, x + 2), img.at<uint8_t>(x + 0, y + 2, 0));

    // upper bound
    // get half-existing patch from (2, -2) to (5, 1)
    p = extractPatch(img, /*px*/ 3, /*py*/ 2, psize, pover, fullArea, /*channel*/ 0);
    p.reshape(psize, psize);
    p = p.t();
    BOOST_CHECK(arma::approx_equal(p, arma::flipud(p), "absdiff", 0));
    for (int y = 0; y < (int)psize / 2; ++y)
        for (int x = 0; x < (int)psize; ++x)
            BOOST_CHECK_EQUAL(p(y + 2, x), img.at<uint8_t>(x + 2, y + 0, 0));

    // upper left bound
    // get half-existing patch from (-2, -2) to (1, 1)
    p = extractPatch(img, /*px*/ 2, /*py*/ 2, psize, pover, fullArea, /*channel*/ 0);
    p.reshape(psize, psize);
    p = p.t();
    BOOST_CHECK(arma::approx_equal(p, arma::flipud(p), "absdiff", 0));
    BOOST_CHECK(arma::approx_equal(p, arma::fliplr(p), "absdiff", 0));
    for (int y = 0; y < (int)psize / 2; ++y)
        for (int x = 0; x < (int)psize / 2; ++x)
            BOOST_CHECK_EQUAL(p(y + 2, x + 2), img.at<uint8_t>(x, y, 0));
}

// test that overflowing patches are saturated when averaging (not for int32_t though)
BOOST_AUTO_TEST_CASE(saturate_patches) {
    // zero overlap
    constexpr unsigned int psize = 5;
    SpstfmOptions opt;
    opt.setPatchSize(psize);
    opt.setPatchOverlap(0);

    constexpr unsigned int npx = 2;
    constexpr unsigned int npy = 2;
    const unsigned int imgwidth  = npx * psize + opt.getPatchOverlap();
    const unsigned int imgheight = npy * psize + opt.getPatchOverlap();
    Rectangle nocrop(0, 0, imgwidth, imgheight);

    // prepare dictionary trainer for averaging
    unsigned int dim = psize * psize;
    spstfm_impl_detail::DictTrainer dt;
    dt.opt = opt;
    dt.output = Image(imgwidth, imgheight, Type::uint16x1);
    ConstImage const& out = dt.output;

    // set top row to max + i and bottom row to min - i to provoke overflow
    constexpr int max = std::numeric_limits<uint16_t>::max();
    constexpr int min = std::numeric_limits<uint16_t>::min();
    std::vector<arma::mat> topPatches(npx);
    std::vector<arma::mat> bottomPatches(npx);
    for (unsigned int pxi = 0; pxi < npx; ++pxi) {
        arma::mat& tp = topPatches[pxi];
        arma::mat& bp = bottomPatches[pxi];
        tp.set_size(psize, psize);
        bp.set_size(psize, psize);
        for (unsigned int i = 0; i < dim; ++i) {
            tp(i) = max + i;
            bp(i) = min - i;
        }
    }

    // averaging should saturate to [min, max]
    dt.outputAveragedPatchRow(topPatches, topPatches,    /*pyi*/ 0, nocrop, npx, npy, 0);
    dt.outputAveragedPatchRow(topPatches, bottomPatches, /*pyi*/ 1, nocrop, npx, npy, 0);

    // check that
    for (int y = 0; y < out.height(); ++y)
        for (int x = 0; x < out.width(); ++x)
            BOOST_CHECK_EQUAL(out.at<uint16_t>(x, y), y < static_cast<int>(psize) ? max : min);
}

// test averaging of patches
BOOST_AUTO_TEST_CASE(averaging_patches) {
    constexpr unsigned int npx = 3;
    constexpr unsigned int npy = 2;
    constexpr unsigned int psize = 7;
    constexpr unsigned int pover = 2;
    constexpr unsigned int dist = psize - pover;
    constexpr unsigned int imgwidth  = npx * dist + pover;
    constexpr unsigned int imgheight = npy * dist + pover;

    SpstfmOptions opt;
    opt.setPatchSize(psize);
    opt.setPatchOverlap(pover);

    spstfm_impl_detail::DictTrainer dt;
    dt.opt = opt;

    // prepare patches, every number is dividable by 4, overlapping rows and columns are marked with #
    /*                            ###  ###     ###  ###                 ###  ###     ###  ###
     *  [  0,   4,   8,  12,  16,  20,  24;   [ 80,  84,  88,  92,  96, 100, 104;   [160, 164, 168, 172, 176, 180, 184;
     *    28,  32,  36,  40,  44,  48,  52;    108, 112, 116, 120, 124, 128, 132;    188, 192, 196, 200, 204, 208, 212;
     *    56,  60,  64,  68,  72,  76,  80;    136, 140, 144, 148, 152, 156, 160;    216, 220, 224, 228, 232, 236, 240;
     *    84,  88,  92,  96, 100, 104, 108;    164, 168, 172, 176, 180, 184, 188;    244, 248, 252, 256, 260, 264, 268;
     *   112, 116, 120, 124, 128, 132, 136;    192, 196, 200, 204, 208, 212, 216;    272, 276, 280, 284, 288, 292, 296;
     *#  140, 144, 148, 152, 156, 160, 164; #  220, 224, 228, 232, 236, 240, 244; #  300, 304, 308, 312, 316, 320, 324; #
     *#  168, 172, 176, 180, 184, 188, 192] #  248, 252, 256, 260, 264, 268, 272] #  328, 332, 336, 340, 344, 348, 352] #
     *                            ###  ###     ###  ###                 ###  ###     ###  ###
     *# [240, 244, 248, 252, 256, 260, 264; # [320, 324, 328, 332, 336, 340, 344; # [400, 404, 408, 412, 416, 420, 424; #
     *#  268, 272, 276, 280, 284, 288, 292; #  348, 352, 356, 360, 364, 368, 372; #  428, 432, 436, 440, 444, 448, 452; #
     *   296, 300, 304, 308, 312, 316, 320;    376, 380, 384, 388, 392, 396, 400;    456, 460, 464, 468, 472, 476, 480;
     *   324, 328, 332, 336, 340, 344, 348;    404, 408, 412, 416, 420, 424, 428;    484, 488, 492, 496, 500, 504, 508;
     *   352, 356, 360, 364, 368, 372, 376;    432, 436, 440, 444, 448, 452, 456;    512, 516, 520, 524, 528, 532, 536;
     *   380, 384, 388, 392, 396, 400, 404;    460, 464, 468, 472, 476, 480, 484;    540, 544, 548, 552, 556, 560, 564;
     *   408, 412, 416, 420, 424, 428, 432]    488, 492, 496, 500, 504, 508, 512]    568, 572, 576, 580, 584, 588, 592]
     *                            ###  ###     ###  ###                 ###  ###     ###  ###
     */
    std::vector<arma::mat> topPatchesOrig(npx);
    std::vector<arma::mat> bottomPatchesOrig(npx);
    for (unsigned int pxi = 0; pxi < npx; ++pxi) {
        arma::mat& tp = topPatchesOrig[pxi];
        arma::mat& bp = bottomPatchesOrig[pxi];
        tp.set_size(psize, psize);
        bp.set_size(psize, psize);
        for (unsigned int x = 0; x < psize; ++x) {
            for (unsigned int y = 0; y < psize; ++y) {
                tp(y, x) = 4 * (x + psize * y) + 80 * pxi;
                bp(y, x) = 4 * (x + psize * y) + 80 * (pxi + npx);
            }
        }
    }

    // test averages with different sizes and offsets for the crop window
    for (unsigned int xoffset = 0; xoffset < psize; ++xoffset) {
        for (unsigned int yoffset = 0; yoffset < psize; ++yoffset) {
//            std::cout << "xoffset: " << xoffset << ", yoffset: " << yoffset << std::endl;
            // init image
            unsigned int cropwidth  = imgwidth  - xoffset;
            unsigned int cropheight = imgheight - yoffset;
            dt.output = Image(cropwidth, cropheight, Type::uint16x1);
            ConstImage const& out = dt.output;

            // test with crop window starting top-left at (0, 0) or (xoffset, yoffset)
            for (bool doStartOff : {false, true}) {
                // make a copy of the patches, since averaging will modify the overlapping regions
                std::vector<arma::mat> topPatches = topPatchesOrig;
                std::vector<arma::mat> bottomPatches = bottomPatchesOrig;

                // average both rows, starting from (0, 0) or (xoffset, yoffset)
                unsigned int xstart = doStartOff ? xoffset : 0;
                unsigned int ystart = doStartOff ? yoffset : 0;
                Rectangle crop(xstart, ystart, cropwidth, cropheight);
                dt.outputAveragedPatchRow(topPatches, topPatches,    /*pyi*/ 0, crop, npx, npy, 0);
                dt.outputAveragedPatchRow(topPatches, bottomPatches, /*pyi*/ 1, crop, npx, npy, 0);

                // test the result, (xout, yout) are coordinates in the output window
                for (unsigned int yout = 0; yout < cropheight; ++yout) {
                    for (unsigned int xout = 0; xout < cropwidth; ++xout) {
                        // (x, y) are coordinates to a fully reconstructed image (which is not offset)
                        unsigned int x = xout + xstart;
                        unsigned int y = yout + ystart;
//                        std::cout << "x: " << x << ", y: " << y << std::endl;

                        // check if the current coordinates are in an overlapping region
                        bool isXOverlap = x > pover && x < imgwidth  - pover && x % dist < pover;
                        bool isYOverlap = y > pover && y < imgheight - pover && y % dist < pover;

                        // calculate patch index pi and element index i
                        unsigned int pxi = std::min(x / dist, npx - 1);
                        unsigned int pyi = std::min(y / dist, npy - 1);
                        unsigned int pi = pxi + pyi * npx;
                        unsigned int i  = x - pxi * dist + (y - pyi * dist) * psize;

                        // check if the output value is averaged correctly
                        uint16_t val = out.at<uint16_t>(xout, yout);
                        if (isXOverlap && isYOverlap) {
                            int lefti  = i + dist;
                            int leftpi = pi - 1;
                            int upperi  = i + dist * psize;
                            int upperpi = pi - npx;
                            int upperlefti  = i + dist * psize + dist;
                            int upperleftpi = pi - npx - 1;
                            BOOST_CHECK_EQUAL(val, 4/4 * (i + upperi + lefti + upperlefti) + 80/4 * (pi + upperpi + leftpi + upperleftpi));

                        }
                        else if (isYOverlap) {
                            int upperi  = i + dist * psize;
                            int upperpi = pi - npx;
                            BOOST_CHECK_EQUAL(val, 4/2 * (i + upperi) + 80/2 * (pi + upperpi));
                        }
                        else if (isXOverlap) {
                            int lefti  = i + dist;
                            int leftpi = pi - 1;
                            BOOST_CHECK_EQUAL(val, 4/2 * (i + lefti) + 80/2 * (pi + leftpi));
                        }
                        else
                            BOOST_CHECK_EQUAL(val, 4 * i + pi * 80);
                    }
                }
            }
        }
    }
}


struct tinyImageSet {
    void makeImgs(int chans = 1) {
        const unsigned int dist = psize - pover;
        constexpr unsigned int npx = 9;
        constexpr unsigned int npy = 9;
        imgwidth  = npx * dist + pover + 1;
        imgheight = npy * dist + pover + 1;
        const unsigned int baseborder = 2;
        const unsigned int background_col = 0;

        // set one dark, one mid and one bright image
        std::array<Image, 3> low;
        std::array<Image, 3> high;

        // random number generator with fixed seed
        std::minstd_rand rng(0);
        std::normal_distribution<> normal_dist(0, 5);

        for (unsigned int i = 0; i < 3; ++i) {
            unsigned int border = (i + 1) * baseborder;
            unsigned int col = 50 * std::pow(2, i);

            // init image
            high[i] = Image(imgwidth, imgheight, getFullType(Type::uint8, chans));
            low[i]  = high[i].clone();
            high[i].set(background_col);
            low[i].set(background_col);

            // draw high-res image
            Rectangle rect(border, border, imgwidth - 2 * border, imgheight - 2 * border);
            high[i].crop(rect);
            high[i].set(col);
            high[i].uncrop();

            // draw low-res image simulating an upscaling (x2) with bilinear interpolation
            low[i].crop(rect);
            low[i].adjustCropBorders(1,1,1,1);
            low[i].set(col);
            low[i].uncrop();
            for (unsigned int x : {border - 1, imgwidth - 1 - border + 1})
                for (unsigned int y = border - 1; y < imgheight - border + 1; ++y)
                    for (int c = 0; c < chans; ++c)
                        low[i].at<uint8_t>(x, y, c) *= 0.25;
            for (unsigned int y : {border - 1, imgheight - 1 - border + 1})
                for (unsigned int x = border - 1; x < imgwidth - border + 1; ++x)
                    for (int c = 0; c < chans; ++c)
                        low[i].at<uint8_t>(x, y, c) *= 0.25;
            for (unsigned int x : {border, imgwidth - 1 - border})
                for (unsigned int y = border - 1; y < imgheight - border + 1; ++y)
                    for (int c = 0; c < chans; ++c)
                        low[i].at<uint8_t>(x, y, c) *= 0.75;
            for (unsigned int y : {border, imgheight - 1 - border})
                for (unsigned int x = border - 1; x < imgwidth - border + 1; ++x)
                    for (int c = 0; c < chans; ++c)
                        low[i].at<uint8_t>(x, y, c) *= 0.75;

            // add some randomness with fixed seed
            for (unsigned int y = 0; y < imgheight; ++y)
                for (unsigned int x = 0; x < imgwidth; ++x)
                    for (int c = 0; c < chans; ++c)
                        low[i].at<uint8_t>(x, y, c) = cv::saturate_cast<uint8_t>(low[i].at<uint8_t>(x, y, c) + normal_dist(rng));

            // write out source and reference images
    //        low[i].write("rect_l" + std::to_string(i) + ".png");
    //        high[i].write("rect_h" + std::to_string(i) + ".png");
        }


        imgs = std::make_shared<MultiResImages>();
        imgs->set(lowTag,  1, low[0]);
        imgs->set(lowTag,  2, low[1]);
        imgs->set(lowTag,  3, low[2]);
        imgs->set(highTag, 1, high[0]);
        imgs->set(highTag, 2, high[1]);
        imgs->set(highTag, 3, high[2]);
    }

    void makeOpts() {
    //    opt.dbg_recordTrainingStopFunctions = true;
        opt.setPatchSize(psize);
        opt.setPatchOverlap(pover);
        opt.setPredictionArea(Rectangle(0, 0, imgwidth, imgheight));
        opt.setDate1(1);
        opt.setDate3(3);
        opt.setHighResTag(highTag);
        opt.setLowResTag(lowTag);
        opt.setSamplingStrategy(SpstfmOptions::SamplingStrategy::variance);
    //    opt.setDictionaryReuse(SpstfmOptions::ExistingDictionaryHandling::improve);
        opt.setDictionaryReuse(SpstfmOptions::ExistingDictionaryHandling::clear);
        opt.setDictSize(15);
        opt.setNumberTrainingSamples(30);
        opt.setBestShotErrorSet(SpstfmOptions::BestShotErrorSet::train_set);

    }

    tinyImageSet() {
        makeImgs();
        makeOpts();
    }

    ~tinyImageSet() { }

    std::shared_ptr<MultiResImages> imgs;
    const unsigned int psize = 3;
    const unsigned int pover = 1;
    unsigned int imgwidth;
    unsigned int imgheight;
    std::string highTag = "high";
    std::string lowTag  = "low";

    SpstfmOptions opt;
};


BOOST_FIXTURE_TEST_CASE(save_load_dictionary, tinyImageSet)
{
    constexpr int predDate = 2;
    constexpr int numTrainIter = 5;

    SpstfmFusor df;
    df.srcImages(imgs);

    opt.setMinTrainIter(numTrainIter);
    opt.setMaxTrainIter(numTrainIter);
    opt.setDictionaryReuse(SpstfmOptions::ExistingDictionaryHandling::clear);

    // train the dictionary without prediction
    df.processOptions(opt);
    df.train();
    arma::mat dict1 = df.getDictionary();

    // train the dictionary with prediction
    df.processOptions(opt);
    df.predict(predDate);
    arma::mat dict2 = df.getDictionary();

    // dictionaries should be the same
    BOOST_CHECK(arma::approx_equal(dict1, dict2, "absdiff", /*tol*/ 0));


    // overwrite dictionary by zeros
    dict1.zeros();
    df.setDictionary(dict1);

    // check that it has really been set
    dict2 = df.getDictionary();
    BOOST_CHECK(arma::approx_equal(dict1, dict2, "absdiff", /*tol*/ 0));

    // check that a new fusor can receive a dictionary
    df = SpstfmFusor{};
    BOOST_CHECK_NO_THROW(df.setDictionary(dict1));
}


/*
 * This test uses some input image set. Some pixels are chosen to be invalid.
 * These form a mask. The pixels also get modified to some random values. A
 * dictionary is trained and an output is predicted with these images and the
 * mask. Then the invalid pixels get other random values. Again a dictionary is
 * trained and an output predicted. The resulting dictionaries and outputs
 * should be equal except at the invalid pixel locations. Their predicted value
 * is undefined.
 *
 * An anti-test is also performed, which does not use the mask. Its dictionary
 * and output should be different and is just performed to exclude trivial
 * errors as taking always the same dictionary, etc.
 */
BOOST_FIXTURE_TEST_CASE(fusion_with_mask, tinyImageSet)
{
    makeImgs(2);
    // find random positions where modifications should be done
    std::random_device rd;
    std::minstd_rand rng(rd());
    std::uniform_int_distribution<unsigned int> disx(0, imgwidth - 1);
    std::uniform_int_distribution<unsigned int> disy(0, imgheight - 1);
    std::uniform_int_distribution<uint8_t> dis_val;

    std::vector<unsigned int> xpos;
    std::vector<unsigned int> ypos;
    for (int i = 0; i < 30; ++i) {
        xpos.push_back(disx(rng));
        ypos.push_back(disy(rng));
    }

    // make mask
    Image negMask(imgwidth, imgheight, Type::uint8x1);
    negMask.set(0);
    for (unsigned int i = 0; i < xpos.size(); ++i)
        negMask.setBoolAt(xpos.at(i), ypos.at(i), 0, true);

    Image posMask = negMask.bitwise_not();

    // get shortcuts for images
    Image& h1 = imgs->get(highTag, 1);
    Image& h3 = imgs->get(highTag, 3);
    Image& l1 = imgs->get(lowTag,  1);
    Image& l2 = imgs->get(lowTag,  2);
    Image& l3 = imgs->get(lowTag,  3);
    unsigned int chans = h1.channels();

    // prepare prediction
    constexpr int predDate = 2;
    constexpr int numTrainIter = 5;
    opt.setMinTrainIter(numTrainIter);
    opt.setMaxTrainIter(numTrainIter);
    opt.setDictionaryReuse(SpstfmOptions::ExistingDictionaryHandling::clear);
    opt.setSamplingStrategy(SpstfmOptions::SamplingStrategy::variance);
    opt.setInvalidPixelTolerance(1);
    opt.setPredictionArea(Rectangle()); // tests setting no prediction area for SpstfmFusor

    SpstfmFusor df;
    df.srcImages(imgs);
    df.processOptions(opt);

    // modify images to random values
    for (unsigned int i = 0; i < xpos.size(); ++i) {
        unsigned int x = xpos.at(i);
        unsigned int y = ypos.at(i);

        for (unsigned int c = 0; c < chans; ++c) {
            h1.at<uint8_t>(x, y, c) = dis_val(rng);
            h3.at<uint8_t>(x, y, c) = dis_val(rng);
            l1.at<uint8_t>(x, y, c) = dis_val(rng);
            l2.at<uint8_t>(x, y, c) = dis_val(rng);
            l3.at<uint8_t>(x, y, c) = dis_val(rng);
        }
    }

    // predict with random value pixels, but ignore them due to mask
    df.predict(predDate, posMask);
    arma::mat dict_rand1 = df.getDictionary();
    Image out_rand1 = df.outputImage();
    df.predict(predDate); // anti-test (result must be different)
    arma::mat dict_rand1_anti = df.getDictionary();
    Image out_rand1_anti = df.outputImage();

    // modify images to different random values
    for (unsigned int i = 0; i < xpos.size(); ++i) {
        unsigned int x = xpos.at(i);
        unsigned int y = ypos.at(i);

        for (unsigned int c = 0; c < chans; ++c) {
            h1.at<uint8_t>(x, y, c) = dis_val(rng);
            h3.at<uint8_t>(x, y, c) = dis_val(rng);
            l1.at<uint8_t>(x, y, c) = dis_val(rng);
            l2.at<uint8_t>(x, y, c) = dis_val(rng);
            l3.at<uint8_t>(x, y, c) = dis_val(rng);
        }
    }

    // predict with random value pixels, but ignore them due to mask
    df.predict(predDate, posMask);
    arma::mat dict_rand2 = df.getDictionary();
    Image out_rand2 = df.outputImage();
    df.predict(predDate); // anti-test (result must be different)
    arma::mat dict_rand2_anti = df.getDictionary();
    Image out_rand2_anti = df.outputImage();

    // dictionaries should be the same
    BOOST_CHECK(arma::approx_equal(dict_rand1, dict_rand2, "absdiff", /*tol*/ 0)); // works independent on the implementation

    // anti-test dictionaries should be different
    BOOST_CHECK(!arma::approx_equal(dict_rand1_anti, dict_rand2_anti, "absdiff", /*tol*/ 0));

    // images should be the same (and anti-test images not everywhere)
    BOOST_REQUIRE(out_rand1.type() == out_rand2.type() && out_rand1.type() == out_rand1_anti.type() && out_rand1_anti.type() == out_rand2_anti.type());
    BOOST_REQUIRE(out_rand1.size() == out_rand2.size() && out_rand1.size() == out_rand1_anti.size() && out_rand1_anti.size() == out_rand2_anti.size());
    bool all_equal_anti_rand1_rand2 = true;
    std::vector<std::pair<Point,int>> rand1_rand2_diffs;
    for (unsigned int y = 0; y < imgheight; ++y) {
        for (unsigned int x = 0; x < imgwidth; ++x) {
            if (negMask.boolAt(x, y, 0))
                continue;

            for (unsigned int c = 0; c < chans; ++c) {
                if (out_rand1.at<uint8_t>(x, y, c) != out_rand2.at<uint8_t>(x, y, c))
                    rand1_rand2_diffs.emplace_back(Point(x, y), c);

                if (out_rand1_anti.at<uint8_t>(x, y, c) != out_rand2_anti.at<uint8_t>(x, y, c))
                    all_equal_anti_rand1_rand2 = false;
            }
        }
    }

    // check anti-tests. If they are wrong, something in the test is wrong (used two times the same image maybe?)
    BOOST_CHECK_MESSAGE(!all_equal_anti_rand1_rand2, "Anti-test failed. rand1 output image and rand2 output image are equal even in the masked regions.");

    // check test with nice messages
    if (!rand1_rand2_diffs.empty()) {
        std::string err_str;
        for (auto p : rand1_rand2_diffs) {
            unsigned int x = p.first.x;
            unsigned int y = p.first.y;
            unsigned int c = p.second;
            err_str += " At (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(c) + "): "
                    + std::to_string((int)(out_rand1.at<uint8_t>(x, y, c))) + " != " + std::to_string((int)(out_rand2.at<uint8_t>(x, y, c))) + ".";
        }
        BOOST_ERROR("There are " << (rand1_rand2_diffs.size() / 2) << " nonequal pixels of rand1 output image and rand2 output image:\n" << err_str);
    }
}


BOOST_FIXTURE_TEST_CASE(tiny_spstfm_fusion, tinyImageSet)
{
    constexpr int predDate = 2;

    Image& ref = imgs->get(highTag, predDate);

    std::vector<double> errors_aad;
    std::vector<double> errors_rmse;
    SpstfmFusor df;
    df.srcImages(imgs);
    for (int numTrainIter = 30; numTrainIter <= 30; ++numTrainIter) {
        opt.setMinTrainIter(numTrainIter);
        opt.setMaxTrainIter(numTrainIter);
//        std::cout << "numTrainIter: " << numTrainIter << std::endl;
//        opt.setMinTrainIter(numTrainIter > 0 ? 1 : 0);
//        opt.setMaxTrainIter(numTrainIter > 0 ? 1 : 0);

        df.processOptions(opt);
        df.predict(predDate);

        ConstImage const& out = df.outputImage();
//        out.write("rect_h" + std::to_string(predDate) + "_p" + std::to_string(numTrainIter) + ".png");

        double aad  = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L1) / (imgwidth * imgheight);
        double rmse = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L2) / (imgwidth * imgheight);
        errors_aad.push_back(aad);
        errors_rmse.push_back(rmse);
//        out.absdiff(ref).write("rect_h" + std::to_string(predDate) + "_diff" + std::to_string(numTrainIter) + ".png");
//        std::cout << "test_set_error = [";
//        for (double e : df.getDbgTestSetError())
//            std::cout << e << ", ";
//        std::cout << "];" << std::endl;
//        df.getDbgObjectiveMaxTau().clear();
//        df.getDbgObjective().clear();
//        df.getDbgTestSetError().clear();
    }

//    std::cout << "objective_maxtau = [";
//    for (double e : df.getDbgObjectiveMaxTau())
//        std::cout << e << ", ";
//    std::cout << "];" << std::endl;
//    std::cout << "objective = [";
//    for (double e : df.getDbgObjective())
//        std::cout << e << ", ";
//    std::cout << "];" << std::endl;
//    std::cout << "aad = [";
//    for (double e : errors_aad)
//        std::cout << e << ", ";
//    std::cout << "];" << std::endl;
//    std::cout << "rmse = [";
//    for (double e : errors_rmse)
//        std::cout << e << ", ";
//    std::cout << "];" << std::endl;

    // run STARFM for comparison
    {
        StarfmOptions o;
        o.setSinglePairDate(1);
        o.setTemporalUncertainty(1);
        o.setSpectralUncertainty(1);
        o.setNumberClasses(40);
        o.setPredictionArea(Rectangle(0, 0, imgwidth, imgheight));
        o.setWinSize(51);
        o.setHighResTag(highTag);
        o.setLowResTag(lowTag);

        StarfmFusor starfm;
        starfm.srcImages(imgs);
        starfm.processOptions(o);
        starfm.predict(predDate);

        ConstImage const& out = starfm.outputImage();
//        out.write("rect_h" + std::to_string(predDate) + "_starfm.png");

        double aad_starfm  = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L1) / (imgwidth * imgheight);
        double rmse_starfm = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L2) / (imgwidth * imgheight);
//        std::cout << "STARFM errors, AAD: " << aad_starfm << ", RMSE: " << rmse_starfm << std::endl;

        // check that spstfm is at least twice as good as STARFM. This is already the case before training.
        BOOST_CHECK_LE(2 * errors_aad.back(),  aad_starfm);
        BOOST_CHECK_LE(2 * errors_rmse.back(), rmse_starfm);
    }

    // check that training improves the result
    BOOST_CHECK_LE(errors_aad.back(),  errors_aad.front());
    BOOST_CHECK_LE(errors_rmse.back(), errors_rmse.front());
}



// build this test case only in Boost 1.59+, since from there on it is possible to disable a test case by default
// we do not want to run this test by default, since it runs too long
// run with --run_test=spstfm_suite/spstfm_fusion
#if BOOST_VERSION / 100 % 1000 > 58 //|| 1
#if BOOST_VERSION / 100 % 1000 > 58
// usage text test outputs, disabled by default, run with --run_test=spstfm_suite/spstfm_fusion
BOOST_AUTO_TEST_CASE(spstfm_fusion, * boost::unit_test::disabled())
#else /* Boost version <= 1.58 */
BOOST_AUTO_TEST_CASE(spstfm_fusion)
#endif
{
    using namespace std::chrono;

    std::string highTag = "high";
    std::string lowTag  = "low";
    auto imgs = std::make_shared<MultiResImages>();
    std::string path = "../test_resources/images/";
//    imgs->set(lowTag,   1, Image(path + "MODISAQUIRED1.tif"));
//    imgs->set(lowTag,   2, Image(path + "MODISAQUIRED2.tif"));
//    imgs->set(lowTag,   3, Image(path + "MODISAQUIRED3.tif"));
//    imgs->set(highTag,  1, Image(path + "LANDSATAQUIRED1.tif"));
//    imgs->set(highTag,  2, Image(path + "LANDSATAQUIRED2.tif"));
//    imgs->set(highTag,  3, Image(path + "LANDSATAQUIRED3.tif"));

    imgs->set(lowTag,  1, Image(path + "artificial-set1/shapes_noise_l1.png"));
    imgs->set(lowTag,  2, Image(path + "artificial-set1/shapes_noise_l2.png"));
    imgs->set(lowTag,  3, Image(path + "artificial-set1/shapes_noise_l3.png"));
    imgs->set(highTag, 1, Image(path + "artificial-set1/shapes_h1.png"));
    imgs->set(highTag, 2, Image(path + "artificial-set1/shapes_h2.png"));
    imgs->set(highTag, 3, Image(path + "artificial-set1/shapes_h3.png"));
//    // make low resolution shapes dark
//    for (int date : {1, 2, 3})
//        imgs->get(lowTag, date).cvMat() /= 5;

//    imgs->set(lowTag,  1, Image(path + "artificial-set1/shapes_l1.png"));
//    imgs->set(lowTag,  2, Image(path + "artificial-set1/shapes_l2.png"));
//    imgs->set(lowTag,  3, Image(path + "artificial-set1/shapes_l3.png"));
//    imgs->set(highTag, 1, Image(path + "artificial-set1/shapes_h1.png"));
//    imgs->set(highTag, 2, Image(path + "artificial-set1/shapes_h2.png"));
//    imgs->set(highTag, 3, Image(path + "artificial-set1/shapes_h3.png"));

//    imgs->set(lowTag,  1, Image(path + "real-set1/M_2016_220-bilinear.tif"));
//    imgs->set(lowTag,  2, Image(path + "real-set1/M_2016_236-bilinear.tif"));
//    imgs->set(lowTag,  3, Image(path + "real-set1/M_2016_243-bilinear.tif"));
//    imgs->set(highTag, 1, Image(path + "real-set1/L_2016_220.tif"));
//    imgs->set(highTag, 2, Image(path + "real-set1/L_2016_236.tif"));
//    imgs->set(highTag, 3, Image(path + "real-set1/L_2016_243.tif"));

//    Rectangle crop{700, 700, 300, 300};
//    imgs->set(lowTag,  1, Image{Image{path + "real-set2/M_2016158_NIR-bilinear.tif"}.sharedCopy(crop)});
//    imgs->set(lowTag,  2, Image{Image{path + "real-set2/M_2016238_NIR-bilinear.tif"}.sharedCopy(crop)});
//    imgs->set(lowTag,  3, Image{Image{path + "real-set2/M_2016254_NIR-bilinear.tif"}.sharedCopy(crop)});
//    imgs->set(lowTag,  1, Image{Image{path + "real-set2/M_art_2016158_NIR.tif"}.sharedCopy(crop)}); // make these images with the multiply_low_res_imgs test (see the bottom of this file)
//    imgs->set(lowTag,  2, Image{Image{path + "real-set2/M_art_2016238_NIR.tif"}.sharedCopy(crop)});
//    imgs->set(lowTag,  3, Image{Image{path + "real-set2/M_art_2016254_NIR.tif"}.sharedCopy(crop)});
//    imgs->set(highTag, 1, Image{Image{path + "real-set2/L_2016158_NIR.tif"}.sharedCopy(crop)});
//    imgs->set(highTag, 2, Image{Image{path + "real-set2/L_2016238_NIR.tif"}.sharedCopy(crop)});
//    imgs->set(highTag, 3, Image{Image{path + "real-set2/L_2016254_NIR.tif"}.sharedCopy(crop)});

    constexpr int predDate = 2;
    constexpr int border = 5;
    Rectangle predArea{border, border, imgs->getAny().width() - 2*border, imgs->getAny().height() - 2*border};
//    imgs->get(highTag, 1).constSharedCopy(predArea).absdiff(imgs->get(highTag,        3).constSharedCopy(predArea)).write("high_image_diff_1-3.tif");
//    imgs->get(highTag, 1).constSharedCopy(predArea).absdiff(imgs->get(highTag,        2).constSharedCopy(predArea)).write("high_image_diff_1-2.tif");
//    imgs->get(highTag, 2).constSharedCopy(predArea).absdiff(imgs->get(highTag,        3).constSharedCopy(predArea)).write("high_image_diff_2-3.tif");
//    imgs->get(lowTag,  1).constSharedCopy(predArea).absdiff(imgs->get(lowTag,         3).constSharedCopy(predArea)).write("low_image_diff_1-3.tif");
//    imgs->get(lowTag,  1).constSharedCopy(predArea).absdiff(imgs->get(lowTag,         2).constSharedCopy(predArea)).write("low_image_diff_1-2.tif");
//    imgs->get(lowTag,  2).constSharedCopy(predArea).absdiff(imgs->get(lowTag,         3).constSharedCopy(predArea)).write("low_image_diff_2-3.tif");

    ConstImage ref = imgs->get(highTag, predDate).sharedCopy(predArea);
//    ref.write("high_image_date_2.tif");
//    imgs->get(highTag, 1).constSharedCopy(predArea).write("high_image_date_1.tif");
//    imgs->get(highTag, 3).constSharedCopy(predArea).write("high_image_date_3.tif");
//    imgs->get(lowTag,  1).constSharedCopy(predArea).write("low_image_date_1.tif");
//    imgs->get(lowTag,  2).constSharedCopy(predArea).write("low_image_date_2.tif");
//    imgs->get(lowTag,  3).constSharedCopy(predArea).write("low_image_date_3.tif");

    // run STARFM for comparison
    {
        StarfmOptions o;
        o.setSinglePairDate(1);
        o.setTemporalUncertainty(1);
        o.setSpectralUncertainty(1);
        o.setNumberClasses(40);
        o.setPredictionArea(predArea);
        o.setWinSize(51);
        o.setHighResTag(highTag);
        o.setLowResTag(lowTag);

        StarfmFusor starfm;
        starfm.srcImages(imgs);
        starfm.processOptions(o);
        starfm.predict(predDate);

        ConstImage const& out = starfm.outputImage();
        out.write("result_starfm_from_date_1.tif");
        out.absdiff(ref).write("diff_starfm_from_date_1.tif");

        double aad  = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L1) / predArea.area();
        double rmse = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L2) / predArea.area();
        std::cout << "STARFM errors from date 1, AAD: " << aad << ", RMSE: " << rmse << std::endl;

        o.setSinglePairDate(3);
        starfm.processOptions(o);
        starfm.predict(predDate);
        out.write("result_starfm_from_date_3.tif");
        out.absdiff(ref).write("diff_starfm_from_date_3.tif");

        aad  = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L1) / predArea.area();
        rmse = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L2) / predArea.area();
        std::cout << "STARFM errors from date 3, AAD: " << aad << ", RMSE: " << rmse << std::endl;
    }

    SpstfmOptions o;
    o.dbg_recordTrainingStopFunctions = true;
    o.setTrainingStopNumberTestSamples(1);
    o.setPredictionArea(predArea);
    o.setDate1(1);
    o.setDate3(3);
    o.setHighResTag(highTag);
    o.setLowResTag(lowTag);
    o.setSamplingStrategy(SpstfmOptions::SamplingStrategy::variance);
    o.setDictionaryReuse(SpstfmOptions::ExistingDictionaryHandling::improve);
//    o.setDictionaryReuse(SpstfmOptions::ExistingDictionaryHandling::clear);
//    o.setNumberTrainingSamples(300);
//    o.setDictSize(42);
//    o.setUseBuildUpIndexForWeights(true);
    o.setSubtractMeanUsage(        SpstfmOptions::SampleNormalization::none);
    o.setDivideNormalizationFactor(SpstfmOptions::SampleNormalization::separate);
    o.setUseStdDevForSampleNormalization(true);
    o.setDictionaryInitNormalization(SpstfmOptions::DictionaryNormalization::independent);
    o.setDictionaryKSVDNormalization(SpstfmOptions::DictionaryNormalization::independent);

    SpstfmOptions::GPSROptions gpsrReconOpt;
    gpsrReconOpt.tolA = 1e-5;
    gpsrReconOpt.tolD = 1e-1;
    o.setGPSRReconstructionOptions(gpsrReconOpt);

    SpstfmOptions::GPSROptions gpsrTrainOpt;
    gpsrTrainOpt.tolA = 1e-6;
    gpsrTrainOpt.tolD = 1e-1;
    o.setGPSRTrainingOptions(gpsrTrainOpt);

    std::vector<double> errors_aad;
    std::vector<double> errors_rmse;
    SpstfmFusor df;
    df.srcImages(imgs);
    steady_clock::time_point start = steady_clock::now();
    for (int numTrainIter = 0; numTrainIter <= 15; ++numTrainIter) {
        std::cout << "iteration: " << numTrainIter << std::endl;
        // for first case, do not iterate, otherwise iterate once
        o.setMinTrainIter(numTrainIter > 0 ? 1 : 0);
        o.setMaxTrainIter(numTrainIter > 0 ? 1 : 0);
//        o.setMinTrainIter(numTrainIter);
//        o.setMaxTrainIter(numTrainIter);

        // do prediction
        steady_clock::time_point start_iter = steady_clock::now();
        df.processOptions(o);
        df.predict(predDate);
        steady_clock::time_point end_iter = steady_clock::now();
        duration<double> time_iter = duration_cast<duration<double>>(end_iter - start_iter);
        std::cout << "execution time: " << time_iter.count()   << " s" << std::endl;

        // output image
        ConstImage const& out = df.outputImage();
        out.write("result_" + std::to_string(numTrainIter) + ".tif");
        out.absdiff(ref).write("diff_" + std::to_string(numTrainIter) + ".tif");

        // check error
        double aad  = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L1) / predArea.area();
        double rmse = cv::norm(out.cvMat(), ref.cvMat(), cv::NORM_L2) / predArea.area();
        errors_aad.push_back(aad);
        errors_rmse.push_back(rmse);

        std::cout << "error norms, it " << numTrainIter << " (AAD):  [";
        for (double e : errors_aad)
            std::cout << e << ", ";
        std::cout << "];" << std::endl;

        std::cout << "error norms, it " << numTrainIter << " (RMSE): [";
        for (double e : errors_rmse)
            std::cout << e << ", ";
        std::cout << "];" << std::endl;

        std::cout << "train set error, it " << numTrainIter << ":    [";
        for (double e : df.getDbgTrainSetError())
            std::cout << e << ", ";
        std::cout << "];" << std::endl;
    }
    steady_clock::time_point end = steady_clock::now();
    duration<double> time = duration_cast<duration<double>>(end - start);
    std::cout << "execution time for all iterations: " << time.count()   << " s" << std::endl;
}
#endif /* Boost version > 1.58 */



//// small test to multiply the dark low resolution single-channel images by 4.7178, which is the factor of the standard deviations between the first high and low resolution images
//BOOST_AUTO_TEST_CASE(multiply_low_res_imgs)
//{
//    std::vector<Image> imgs;
//    Rectangle crop{700, 700, 300, 300};
//    std::string path = "../test_resources/images/";
////    Rectangle crop{895, 895, 100, 100};
//    imgs.push_back(Image{Image{path + "real-set2/M_2016158_NIR-bilinear.tif"}.sharedCopy(crop)});
//    imgs.push_back(Image{Image{path + "real-set2/M_2016238_NIR-bilinear.tif"}.sharedCopy(crop)});
//    imgs.push_back(Image{Image{path + "real-set2/M_2016254_NIR-bilinear.tif"}.sharedCopy(crop)});
//    for (int i : {0, 1, 2}) {
//        std::string filename = "M_art_2016_" + std::to_string(i) + ".tif";
//        Image img = imgs[i];
//        img.cvMat() *= 4.7178;
//        img.write(filename);
//    }
//}



// should give a compile error (cannot be tested like this, since it gives a compile error).
//BOOST_AUTO_TEST_CASE(parallelizer)
//{
//    Parallelizer<SpstfmFusor> p;
//}

BOOST_AUTO_TEST_SUITE_END()

} /* spstfm_impl_detail */
} /* namespace imagefusion */

#include <boost/version.hpp>
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <chrono>
#include <vector>
#include <cstdint>

#include <opencv2/opencv.hpp>
#include "Image.h"



// build this module only in Boost 1.59+, since from there on it is possible to disable a test suite by default
// we do not want to run these tests by default, since they take a lot of time
// run with --run_test=img_perf_suite
#if BOOST_VERSION / 100 % 1000 > 58 //|| 1


#if BOOST_VERSION / 100 % 1000 > 58
// image performance suite, disabled by default, run with --run_test=img_perf_suite
BOOST_AUTO_TEST_SUITE(img_perf_suite, * boost::unit_test::disabled())
#else /* Boost version <= 1.58 */
BOOST_AUTO_TEST_SUITE(img_perf_suite)
#endif


// test write performance
BOOST_AUTO_TEST_CASE(write_performance)
{
    using namespace std::chrono;
    using namespace imagefusion;

    constexpr std::size_t height = 50000;
    constexpr std::size_t width  = 10000;

    std::cout << "Write performance results" << std::endl;

    // checks with vector
    duration<double> vec_raw_time;
    duration<double> vec_checked_time;
    duration<double> vec_it_time;
    {
        std::vector<uint8_t> vec(height*width, 4);

        // no test, just caching
        {
            std::size_t s = vec.size();
            for (std::size_t i = 0; i < s; ++i)
                vec[i] = 5;
        }

        // raw access; []
        {
            steady_clock::time_point start = steady_clock::now();

            std::size_t s = vec.size();
            for (std::size_t i = 0; i < s; ++i)
                vec[i] = 5;

            steady_clock::time_point end = steady_clock::now();
            vec_raw_time = duration_cast<duration<double>>(end - start);
        }

        // checked access; at
        {
            steady_clock::time_point start = steady_clock::now();

            std::size_t s = vec.size();
            for (std::size_t i = 0; i < s; ++i)
                vec.at(i) = 6;

            steady_clock::time_point end = steady_clock::now();
            vec_checked_time = duration_cast<duration<double>>(end - start);
        }

        // iterator access
        {
            steady_clock::time_point start = steady_clock::now();

            auto end_it = vec.end();
            for (auto it = vec.begin(); it != end_it; ++it)
                *it = 7;

            steady_clock::time_point end = steady_clock::now();
            vec_it_time = duration_cast<duration<double>>(end - start);
        }
    }
    std::cout << "  vector raw access:        " << vec_raw_time.count()     << " s" << std::endl;
    std::cout << "  vector checked access:    " << vec_checked_time.count() << " s" << std::endl;
    std::cout << "  vector iterator access:   " << vec_it_time.count()      << " s" << std::endl;

    // OpenCV tests
    duration<double> ocv_at_time;
    duration<double> ocv_it_time;
    {
        cv::Mat img(height, width, CV_8U);

        // no test, just caching
        {
            cv::MatIterator_<uint8_t> end_it = img.end<uint8_t>();
            for (cv::MatIterator_<uint8_t> it = img.begin<uint8_t>(); it != end_it; ++it)
                *it = 6;
        }

        // at method
        {
            steady_clock::time_point start = steady_clock::now();

            for (std::size_t i = 0; i < height; ++i)
                for (std::size_t j = 0; j < width; ++j)
                    img.at<uint8_t>(i, j) = 5;

            steady_clock::time_point end = steady_clock::now();
            ocv_at_time = duration_cast<duration<double>>(end - start);
        }

        // iterator
        {
            steady_clock::time_point start = steady_clock::now();

            cv::MatIterator_<uint8_t> end_it = img.end<uint8_t>();
            for (cv::MatIterator_<uint8_t> it = img.begin<uint8_t>(); it != end_it; ++it)
                *it = 6;

            steady_clock::time_point end = steady_clock::now();
            ocv_it_time = duration_cast<duration<double>>(end - start);
        }
    }
    std::cout << "  OpenCV iterator access:   " << ocv_it_time.count()      << " s (" << (ocv_it_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;
    std::cout << "  OpenCV at access:         " << ocv_at_time.count()      << " s (" << (ocv_at_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;

    // Image tests
    duration<double> img_at_time;
    duration<double> img_chan_at_time;
    duration<double> img_chan_it_time;
    duration<double> img_pix_it_time;
    {
        Image img(width, height, Type::uint8x1);

        // no test, just caching
        {
            for (std::size_t y = 0; y < height; ++y)
                for (std::size_t x = 0; x < width; ++x)
                    img.at<uint8_t>(x, y) = 5;
        }

        // set pixels via at method
        {
            steady_clock::time_point start = steady_clock::now();

            for (std::size_t y = 0; y < height; ++y)
                for (std::size_t x = 0; x < width; ++x)
                    img.at<uint8_t>(x, y) = 5;

            steady_clock::time_point end = steady_clock::now();
            img_at_time = duration_cast<duration<double>>(end - start);
        }

        // set pixels via channel at method
        {
            steady_clock::time_point start = steady_clock::now();

            for (std::size_t y = 0; y < height; ++y)
                for (std::size_t x = 0; x < width; ++x)
                    img.at<uint8_t>(x, y, 0) = 5;

            steady_clock::time_point end = steady_clock::now();
            img_chan_at_time = duration_cast<duration<double>>(end - start);
        }

        // set pixels via channel iterator
        {
            steady_clock::time_point start = steady_clock::now();

            for (auto it = img.begin<uint8_t>(0), it_end = img.end<uint8_t>(0); it != it_end; ++it)
                    *it = 5;

            steady_clock::time_point end = steady_clock::now();
            img_chan_it_time = duration_cast<duration<double>>(end - start);
        }

        // set pixels via pixel iterator
        {
            steady_clock::time_point start = steady_clock::now();

            for (auto it = img.begin<uint8_t>(), it_end = img.end<uint8_t>(); it != it_end; ++it)
                    *it = 5;

            steady_clock::time_point end = steady_clock::now();
            img_pix_it_time = duration_cast<duration<double>>(end - start);
        }
    }
    std::cout << "  Image at access:          " << img_at_time.count()      << " s (" << (img_at_time.count() / vec_raw_time.count())      << "x speed down to vector raw access)" << std::endl;
    std::cout << "  Image channel at access:  " << img_chan_at_time.count() << " s (" << (img_chan_at_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;
    std::cout << "  Image channel iterator:   " << img_chan_it_time.count() << " s (" << (img_chan_it_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;
    std::cout << "  Image pixel iterator:     " << img_pix_it_time.count()  << " s (" << (img_pix_it_time.count() / vec_raw_time.count())  << "x speed down to vector raw access)" << std::endl;
    std::cout << std::endl;
}


// test read performance
BOOST_AUTO_TEST_CASE(read_performance)
{
    using namespace std::chrono;
    using namespace imagefusion;

    constexpr std::size_t height = 50000;
    constexpr std::size_t width  = 10000;

    std::cout << "Read performance results" << std::endl;
    bool dummy = false;

    // checks with vector
    duration<double> vec_raw_time;
    duration<double> vec_checked_time;
    duration<double> vec_it_time;
    {
        std::vector<uint8_t> vec_(height*width, 4);
        std::vector<uint8_t> const& vec = vec_;

        // no test, just caching
        {
            std::size_t s = vec.size();
            for (std::size_t i = 0; i < s; ++i)
                dummy |= vec[i] == 5;
        }

        // raw access; []
        {
            steady_clock::time_point start = steady_clock::now();

            std::size_t s = vec.size();
            for (std::size_t i = 0; i < s; ++i)
                dummy |= vec[i] == 5;

            steady_clock::time_point end = steady_clock::now();
            vec_raw_time = duration_cast<duration<double>>(end - start);
        }

        // checked access; at
        {
            steady_clock::time_point start = steady_clock::now();

            std::size_t s = vec.size();
            for (std::size_t i = 0; i < s; ++i)
                dummy |= vec.at(i) == 5;

            steady_clock::time_point end = steady_clock::now();
            vec_checked_time = duration_cast<duration<double>>(end - start);
        }

        // iterator access
        {
            steady_clock::time_point start = steady_clock::now();

            auto end_it = vec.end();
            for (auto it = vec.begin(); it != end_it; ++it)
                dummy |= *it == 5;

            steady_clock::time_point end = steady_clock::now();
            vec_it_time = duration_cast<duration<double>>(end - start);
        }
    }
    std::cout << "  vector raw access:        " << vec_raw_time.count()     << " s" << std::endl;
    std::cout << "  vector checked access:    " << vec_checked_time.count() << " s" << std::endl;
    std::cout << "  vector iterator access:   " << vec_it_time.count()      << " s" << std::endl;

    // OpenCV tests
    duration<double> ocv_at_time;
    duration<double> ocv_it_time;
    {
        cv::Mat img_(height, width, CV_8U);
        cv::Mat const& img = img_;

        // no test, just caching
        {
            cv::MatConstIterator_<uint8_t> end_it = img.end<uint8_t>();
            for (cv::MatConstIterator_<uint8_t> it = img.begin<uint8_t>(); it != end_it; ++it)
                dummy |= *it == 6;
        }

        // at method
        {
            steady_clock::time_point start = steady_clock::now();

            for (std::size_t i = 0; i < height; ++i)
                for (std::size_t j = 0; j < width; ++j)
                    dummy |= img.at<uint8_t>(i, j) == 5;

            steady_clock::time_point end = steady_clock::now();
            ocv_at_time = duration_cast<duration<double>>(end - start);
        }

        // iterator
        {
            steady_clock::time_point start = steady_clock::now();

            cv::MatConstIterator_<uint8_t> end_it = img.end<uint8_t>();
            for (cv::MatConstIterator_<uint8_t> it = img.begin<uint8_t>(); it != end_it; ++it)
                dummy |= *it == 6;

            steady_clock::time_point end = steady_clock::now();
            ocv_it_time = duration_cast<duration<double>>(end - start);
        }
    }
    std::cout << "  OpenCV iterator access:   " << ocv_it_time.count() << " s (" << (ocv_it_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;
    std::cout << "  OpenCV at access:         " << ocv_at_time.count() << " s (" << (ocv_at_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;

    // Image tests
    duration<double> img_at_time;
    duration<double> img_chan_at_time;
    duration<double> img_chan_it_time;
    duration<double> img_pix_it_time;
    {
        Image img_(width, height, Type::uint8);
        Image const& img = img_;

        // no test, just caching
        {
            for (std::size_t y = 0; y < height; ++y)
                for (std::size_t x = 0; x < width; ++x)
                    dummy |= img.at<uint8_t>(x, y) == 5;
        }

        // get pixels via at method
        {
            steady_clock::time_point start = steady_clock::now();

            for (std::size_t y = 0; y < height; ++y)
                for (std::size_t x = 0; x < width; ++x)
                    dummy |= img.at<uint8_t>(x, y) == 5;

            steady_clock::time_point end = steady_clock::now();
            img_at_time = duration_cast<duration<double>>(end - start);
        }

        // get pixels via channel at method
        {
            steady_clock::time_point start = steady_clock::now();

            for (std::size_t y = 0; y < height; ++y)
                for (std::size_t x = 0; x < width; ++x)
                    dummy |= img.at<uint8_t>(x, y, 0) == 5;

            steady_clock::time_point end = steady_clock::now();
            img_chan_at_time = duration_cast<duration<double>>(end - start);
        }

        // get values via channel iterator
        {
            steady_clock::time_point start = steady_clock::now();

            for (auto it = img.begin<uint8_t const>(0), it_end = img.end<uint8_t const>(0); it != it_end; ++it)
                    dummy |= *it == 5;

            steady_clock::time_point end = steady_clock::now();
            img_chan_it_time = duration_cast<duration<double>>(end - start);
        }

        // get values via pixel iterator
        {
            steady_clock::time_point start = steady_clock::now();

            for (auto it = img.begin<uint8_t const>(), it_end = img.end<uint8_t const>(); it != it_end; ++it)
                    dummy |= *it == 5;

            steady_clock::time_point end = steady_clock::now();
            img_pix_it_time = duration_cast<duration<double>>(end - start);
        }
    }
    std::cout << "  Image at access:          " << img_at_time.count()      << " s (" << (img_at_time.count() / vec_raw_time.count())      << "x speed down to vector raw access)" << std::endl;
    std::cout << "  Image channel at access:  " << img_chan_at_time.count() << " s (" << (img_chan_at_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;
    std::cout << "  Image channel iterator:   " << img_chan_it_time.count() << " s (" << (img_chan_it_time.count() / vec_raw_time.count()) << "x speed down to vector raw access)" << std::endl;
    std::cout << "  Image pixel iterator:     " << img_pix_it_time.count()  << " s (" << (img_pix_it_time.count() / vec_raw_time.count())  << "x speed down to vector raw access)" << std::endl;
    std::cout << "(dummy " << dummy << ")" << std::endl << std::endl;
}


BOOST_AUTO_TEST_CASE(set_multimask_performance)
{
    using namespace std::chrono;
    using namespace imagefusion;

    constexpr std::size_t height = 10000;
    constexpr std::size_t width  = 10000;
    constexpr unsigned int chans = 3;
    Image i{width, height, Type::uint8x3};
    Image all_one_mask{width, height, Type::uint8x3};
    Image part_one_mask{width, height, Type::uint8x3};

    all_one_mask.set(255);

    for (unsigned int y = 0; y < height; ++y)
        for (unsigned int x = 0; x < width; ++x)
            for (unsigned int c = 0; c < chans; ++c)
                if (x > c * width / 3 && x < (c+1) * width / 3)
                    part_one_mask.at<uint8_t>(x, y, c) = 255;
                else
                    part_one_mask.at<uint8_t>(x, y, c) = 0;


    std::cout << "Performance results for set with a multi-channel mask" << std::endl;

    // no test, just caching
    i.set(1);

    duration<double> no_mask_time;
    {
        steady_clock::time_point start = steady_clock::now();

        i.set(2);

        steady_clock::time_point end = steady_clock::now();
        no_mask_time = duration_cast<duration<double>>(end - start);
    }

    duration<double> all_one_time;
    {
        steady_clock::time_point start = steady_clock::now();

        i.set(3, all_one_mask);

        steady_clock::time_point end = steady_clock::now();
        all_one_time = duration_cast<duration<double>>(end - start);
    }

    duration<double> part_one_time;
    {
        steady_clock::time_point start = steady_clock::now();

        i.set(4, part_one_mask);

        steady_clock::time_point end = steady_clock::now();
        part_one_time = duration_cast<duration<double>>(end - start);
    }

    std::cout << "  Without mask:         " << no_mask_time.count()  << " s" << std::endl;
    std::cout << "  With mask of ones:    " << all_one_time.count()  << " s (" << (all_one_time.count() / no_mask_time.count())  << "x speed down to set without mask)" << std::endl;
    std::cout << "  With mask of stripes: " << part_one_time.count() << " s (" << (part_one_time.count() / no_mask_time.count()) << "x speed down to set without mask)" << std::endl;
}


BOOST_AUTO_TEST_CASE(sub_pixel_crop)
{
    using namespace std::chrono;
    using namespace imagefusion;

    constexpr int width  = 1e4;
    constexpr int height = 1e4;
    Image i{width, height, Type::uint8x3}; // requires width * height * 3 bytes memory, so for 2e4 this is 1.2 GB, and additionally almost the same for the cropped image
    Coordinate offset{   100,   100  };
    Coordinate offset_x{ 100.5, 100  };
    Coordinate offset_y{ 100,   100.5};
    Coordinate offset_xy{100.5, 100.5};
    Size size{width - static_cast<int>(2 * offset.x), height - static_cast<int>(2 * offset.y)};

    std::cout << "Performance results for sub pixel crop:" << std::endl;

    // no test, just caching
    i.set(1);

    duration<double> time;
    {
        steady_clock::time_point start = steady_clock::now();

        Image cropped = i.clone(offset, size);

        steady_clock::time_point end = steady_clock::now();
        time = duration_cast<duration<double>>(end - start);
    }
    std::cout << "  Required time (integers): " << time.count()     << " s" << std::endl;

    duration<double> time_x;
    {
        steady_clock::time_point start = steady_clock::now();

        Image cropped = i.clone(offset_x, size);

        steady_clock::time_point end = steady_clock::now();
        time_x = duration_cast<duration<double>>(end - start);
    }
    std::cout << "  Required time (x real):   " << time_x.count()   << " s" << std::endl;

    duration<double> time_y;
    {
        steady_clock::time_point start = steady_clock::now();

        Image cropped = i.clone(offset_y, size);

        steady_clock::time_point end = steady_clock::now();
        time_y = duration_cast<duration<double>>(end - start);
    }
    std::cout << "  Required time (y real):   " << time_y.count()   << " s" << std::endl;

    duration<double> time_xy;
    {
        steady_clock::time_point start = steady_clock::now();

        Image cropped = i.clone(offset_xy, size);

        steady_clock::time_point end = steady_clock::now();
        time_xy = duration_cast<duration<double>>(end - start);
    }
    std::cout << "  Required time (x,y real): " << time_xy.count()  << " s" << std::endl;
}



BOOST_AUTO_TEST_SUITE_END()

#endif /* Boost version > 1.58 */

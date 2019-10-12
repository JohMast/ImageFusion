#include "Options.h"
#include "DataFusor.h"
#include "Parallelizer.h"
#include "ParallelizerOptions.h"
#include "Image.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <iostream>

namespace imagefusion {


BOOST_AUTO_TEST_SUITE(Parallelizer_Suite)


// make a stupid DataFusor, that only copies an image from one location to another one
class CopierOptions : public Options {
public:
    void setInputTag(std::string tag) {
        inputTag = tag;
    }

    std::string const& getInputTag() {
        return inputTag;
    }

private:
    std::string inputTag;
};


class Copier : public DataFusor {
public:
    using options_type = CopierOptions;

    void predict(int date, ConstImage const& mask = ConstImage{}) override {
        Rectangle predArea = opt.getPredictionArea();

        std::string srcTag = opt.getInputTag();
        ConstImage src = imgs->get(srcTag, date).sharedCopy();
        bool use_mask = !mask.empty();
        ConstImage cropped_mask;
        if (use_mask) {
            if (!mask.isMaskFor(src))
            {
                IF_THROW_EXCEPTION(runtime_error("The mask you gave into predict does not fit to the "
                                                 "source image. mask has size " + to_string(mask.size()) +
                                                 " and type " + to_string(mask.type()) + ". Source image, "
                                                 "which has size " + to_string(src.size()) + " and "
                                                 + std::to_string(src.channels()) + " channels. The criteria"
                                                 "for masks are listed in the documentation of ConstImage::isMaskFor"))
                        << errinfo_image_type(mask.type())
                        << errinfo_size(mask.size());
            }
            cropped_mask = mask.sharedCopy(predArea);
        }
        src.crop(predArea);

        // check if there is an appropriate image at the destination date and tag and use it
        if (output.size() != src.size() || output.type() != src.type())
            output = Image{src.size(), src.type()}; // create new image

        // copy the image in a CPU costly way, instead of simply using dst.copyValuesFrom(src);
        auto src_it_end = src.end<uint8_t>();
        auto mask_it = cropped_mask.begin<uint8_t>();
        auto dst_it = output.begin<uint8_t>();
        for (auto src_it = src.begin<uint8_t>(); src_it != src_it_end; ++src_it, ++dst_it) {
            double log_val = std::log10(*src_it);
            int pix_val = std::round(std::pow(10, log_val));
            if (!use_mask || *mask_it++)
                *dst_it = static_cast<uint8_t>(pix_val);
        }
    }

    void processOptions(Options const& o) override {
        opt = dynamic_cast<CopierOptions const&>(o);
    }

    Options const& getOptions() const override {
        return opt;
    }

private:
    options_type opt;
};


BOOST_AUTO_TEST_CASE(basic) {
    using namespace std::chrono;

    // make a sample image
    constexpr int width       = 1000;
    constexpr int height      = 1100;
    constexpr int pred_x      = 100;
    constexpr int pred_y      = 100;
    constexpr int pred_width  = width  - 2 * pred_x;
    constexpr int pred_height = height - 2 * pred_y;
    Image img{width, height, Type::uint8x1};
    int i = 0;
    for (auto it = img.begin<uint8_t>(), it_end = img.end<uint8_t>(); it != it_end; ++it)
        *it = static_cast<uint8_t>(i++ % 200);

    // put it into a MultiResImages collection
    auto imgs = std::make_shared<MultiResImages>();
    imgs->set("src", 0, std::move(img));

    // set options for Copier
    CopierOptions c_opt;
    c_opt.setInputTag("src");
    c_opt.setPredictionArea(Rectangle{pred_x, pred_y, pred_width, pred_height});

    // serial execution
    duration<double> serial_time;
    {
        // execute Copier
        steady_clock::time_point start = steady_clock::now();
        Copier c;
        c.srcImages(imgs);
        c.processOptions(c_opt);
        c.predict(0);
        Image& dstImg = c.outputImage();
        steady_clock::time_point end = steady_clock::now();
        serial_time = duration_cast<duration<double>>(end - start);

        // validate copied values and independecy of memory
        i = 0;
        ConstImage const& srcImg = c.srcImages().get("src", 0);
        BOOST_CHECK_EQUAL(dstImg.width(),  pred_width);
        BOOST_CHECK_EQUAL(dstImg.height(), pred_height);
        for (int x = 0; x < /* check only a part of dstImg.width() */ 10; ++x) {
            for (int y = 0; y < dstImg.height(); ++y) {
                int orig_x = x + pred_x;
                int orig_y = y + pred_y;
                BOOST_CHECK_EQUAL(dstImg.at<uint8_t>(x, y), (orig_x + orig_y * width) % 200);
                dstImg.at<uint8_t>(x, y) = (x + 10 * y) % 200;
                BOOST_CHECK_EQUAL(dstImg.at<uint8_t>(x, y), (x + y * 10) % 200);
                BOOST_CHECK_EQUAL(srcImg.at<uint8_t>(orig_x, orig_y), (orig_x + orig_y * width) % 200);
            }
        }
    }
    BOOST_TEST_PASSPOINT();

    // parallel execution
    duration<double> parallel_time;
    {
        // change c_opt options, which are unused to make sure it is set by p_opt
        c_opt.setPredictionArea(Rectangle{0,0,1,1});

        // set options for Parallelizer
        ParallelizerOptions<CopierOptions> p_opt;
        p_opt.setNumberOfThreads(2);
        p_opt.setPredictionArea(Rectangle{pred_x, pred_y, pred_width, pred_height});
        p_opt.setAlgOptions(c_opt);

        // execute Copier
        BOOST_TEST_PASSPOINT();
        steady_clock::time_point start = steady_clock::now();
        Parallelizer<Copier> p;
        p.srcImages(imgs);
        p.processOptions(p_opt);
        p.predict(0);
        Image& dstImg = p.outputImage();
        steady_clock::time_point end = steady_clock::now();
        parallel_time = duration_cast<duration<double>>(end - start);
        BOOST_TEST_PASSPOINT();

        // validate copied values and independecy of memory
        i = 0;
        ConstImage const& srcImg = p.srcImages().get("src", 0);
        BOOST_CHECK_EQUAL(dstImg.width(),  pred_width);
        BOOST_CHECK_EQUAL(dstImg.height(), pred_height);
        for (int x = 0; x < /* check only a part of dstImg.width() */ 10; ++x) {
            for (int y = 0; y < dstImg.height(); ++y) {
                int orig_x = x + pred_x;
                int orig_y = y + pred_y;
                BOOST_CHECK_EQUAL(dstImg.at<uint8_t>(x, y), (orig_x + orig_y * width) % 200);
                dstImg.at<uint8_t>(x, y) = (x + 10 * y) % 200;
                BOOST_CHECK_EQUAL(dstImg.at<uint8_t>(x, y), (x + y * 10) % 200);
                BOOST_CHECK_EQUAL(srcImg.at<uint8_t>(orig_x, orig_y), (orig_x + orig_y * width) % 200);
            }
        }
    }

    // check that parallel execution was faster...
    bool faster = parallel_time.count() < serial_time.count();
    BOOST_WARN_LE(parallel_time.count(), serial_time.count());
    if (!faster) {
        std::cerr << __FILE__ << ":" << __LINE__
                  << " Note, that parallel execution (" << parallel_time.count() << " s)"
                  << " was not faster than serial (" << serial_time.count() << " s)." << std::endl;
    }
//    std::cout << "serial execution:   " << serial_time.count()   << " s" << std::endl;
//    std::cout << "parallel execution: " << parallel_time.count() << " s" << std::endl;
}


// check that also images with less height than requested threads can be predicted
BOOST_AUTO_TEST_CASE(tiny) {
    // make a sample image
    constexpr int width       = 200;
    constexpr int height      = 1;
    Image img{width, height, Type::uint8x1};
    for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
            img.at<uint8_t>(x, y) = x;

    // put it into a MultiResImages collection
    auto imgs = std::make_shared<MultiResImages>();
    imgs->set("src", 0, std::move(img));

    // set options for Copier
    CopierOptions c_opt;
    c_opt.setInputTag("src");
    c_opt.setPredictionArea(Rectangle{0, 0, width, height});

    // set options for Parallelizer
    ParallelizerOptions<CopierOptions> p_opt; // uses as many threads as cores by default
    p_opt.setPredictionArea(Rectangle{0, 0, width, height});
    p_opt.setAlgOptions(c_opt);

    // execute Copier
    Parallelizer<Copier> p;
    p.srcImages(imgs);
    p.processOptions(p_opt);
    BOOST_REQUIRE_NO_THROW(p.predict(0));
    Image& dstImg = p.outputImage();

    // validate copied values
    BOOST_REQUIRE_EQUAL(dstImg.width(),  width);
    BOOST_REQUIRE_EQUAL(dstImg.height(), height);
    for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
            BOOST_CHECK_EQUAL(dstImg.at<uint8_t>(x, y), x);
}



// test if processing parallelizer options multiple times with unmodified number of threads will forward the other modified values to the data fusors
BOOST_AUTO_TEST_CASE(process_options_multiple_times) {
    Image one{5, 6, Type::uint8x1};
    one.set(1);

    std::shared_ptr<MultiResImages> imgs{new MultiResImages};
    imgs->set("src", 0, std::move(one));

    CopierOptions c_opt;
    c_opt.setInputTag("src");

    ParallelizerOptions<CopierOptions> p_opt;
    p_opt.setNumberOfThreads(1);
    p_opt.setAlgOptions(c_opt);

    Parallelizer<Copier> p;
    p.srcImages(imgs);

    // copy 1-by-1 image
    p_opt.setPredictionArea(Rectangle{0,0,1,1});
    p.processOptions(p_opt);
    p.predict(0);
    Image& first = p.outputImage();
    BOOST_CHECK_EQUAL(first.width(),  1);
    BOOST_CHECK_EQUAL(first.height(), 1);
    BOOST_CHECK_EQUAL(first.at<uint8_t>(0,0), 1);

    // copy 2-by-2 image
    p_opt.setPredictionArea(Rectangle{0,0,2,2});
    p.processOptions(p_opt); // this should put the new prediction area into the nested data fusors!
    p.predict(0);
    Image& second = p.outputImage();
    BOOST_CHECK_EQUAL(second.width(),  2);
    BOOST_CHECK_EQUAL(second.height(), 2);
    BOOST_CHECK_EQUAL(second.at<uint8_t>(0,0), 1);
}


BOOST_AUTO_TEST_CASE(mask) {
    using namespace std::chrono;

    constexpr int width       = 10;
    constexpr int height      = 20;
    constexpr int pred_x      = 2;
    constexpr int pred_y      = 4;
    constexpr int pred_width  = width  - 2 * pred_x;
    constexpr int pred_height = height - 2 * pred_y;

    // make a sample image
    Image src{width, height, Type::uint8x1};
    src.set(1);

    // mask with left half set to 255
    Image mask{width, height, Type::uint8x1};
    mask.set(0);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width/2; ++x)
            mask.at<uint8_t>(x, y) = 255;

    // output image
    Image out{pred_width, pred_height, Type::uint8x1};
    out.set(0);

    // put them into a MultiResImages collection
    std::shared_ptr<MultiResImages> imgs{new MultiResImages};
    imgs->set("src", 0, std::move(src));

    // set options for Copier
    CopierOptions c_opt;
    c_opt.setInputTag("src");

    // set options for Parallelizer
    ParallelizerOptions<CopierOptions> p_opt;
    p_opt.setNumberOfThreads(2);
    p_opt.setPredictionArea(Rectangle{pred_x, pred_y, pred_width, pred_height});
    p_opt.setAlgOptions(c_opt);

    // execute Copier
    Parallelizer<Copier> p;
    p.outputImage() = Image{out.sharedCopy()};
    p.srcImages(imgs);
    p.processOptions(p_opt);
    p.predict(0, mask);

    // validate copied values
    for (int y = 0; y < out.height(); ++y)
        for (int x = 0; x < out.width(); ++x)
            BOOST_CHECK_EQUAL(out.at<uint8_t>(x, y), x < out.width()/2 ? 1 : 0);
}



class NotWorkingDataFusor : public DataFusor {
public:
    using options_type = Options;

    struct SpecialException : public virtual runtime_error {
        // respecify all constructors, since inheriting constructors from a virtual base class does not work in gcc and msvc++
        explicit SpecialException(std::string const& what_arg) : std::runtime_error(what_arg), runtime_error(what_arg) { }
        explicit SpecialException(const char* what_arg)        : std::runtime_error(what_arg), runtime_error(what_arg) { }
    };

    void predict(int, ConstImage const& mask = ConstImage{}) override {
        (void)mask; // supresses compiler warning
        // Copier assumes uint8 type, but in general this exeption would be useful
        IF_THROW_EXCEPTION(SpecialException("This is an example for a data fusor throwing an exception"));
    }

    void processOptions(Options const& o) override {
        opt = o;
    }

    Options const& getOptions() const override {
        return opt;
    }

private:
    options_type opt;
};

BOOST_AUTO_TEST_CASE(exception) {
    // test exception in serial execution
    NotWorkingDataFusor e1;
    BOOST_CHECK_THROW(e1.predict(0), NotWorkingDataFusor::SpecialException);


    // test exception in parallel execution

    // specify acceptable options and source images
    ParallelizerOptions<NotWorkingDataFusor::options_type> opt;
    opt.setPredictionArea({0,0,1,100});

    auto srcImgs = std::make_shared<MultiResImages>();
    srcImgs->set("", 0, Image{});

    Parallelizer<NotWorkingDataFusor> e2;
    e2.srcImages(srcImgs);
    e2.processOptions(opt);

    // call predict and check that the SpecialException can be caught here
    if (e2.getOptions().getNumberOfThreads() > 1) {
        BOOST_CHECK_THROW(e2.predict(0), NotWorkingDataFusor::SpecialException);
    }
    else {
        BOOST_WARN("Parallelizer_Suite/exception is not tested, since by default only 1 thread is selected.");
    }
}


BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

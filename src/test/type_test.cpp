#include "DataFusor.h"
#include "imagefusion.h"
#include "Image.h"
#include "Options.h"
#include "Proxy.h"

#ifdef WITH_OMP
#include "ParallelizerOptions.h"
#include "Parallelizer.h"
#endif /* WITH_OMP */

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <iostream>
#include <iomanip>

namespace imagefusion {

BOOST_AUTO_TEST_SUITE(Type_Suite)


BOOST_AUTO_TEST_CASE(test_getChannels) {
    // test with uint8
    BOOST_CHECK_EQUAL(getChannels(Type::uint8),   1);
    BOOST_CHECK_EQUAL(getChannels(Type::uint8x1), 1);
    BOOST_CHECK_EQUAL(getChannels(Type::uint8x2), 2);
    BOOST_CHECK_EQUAL(getChannels(Type::uint8x3), 3);
    BOOST_CHECK_EQUAL(getChannels(Type::uint8x4), 4);

    // test with float64
    BOOST_CHECK_EQUAL(getChannels(Type::float64),   1);
    BOOST_CHECK_EQUAL(getChannels(Type::float64x1), 1);
    BOOST_CHECK_EQUAL(getChannels(Type::float64x2), 2);
    BOOST_CHECK_EQUAL(getChannels(Type::float64x3), 3);
    BOOST_CHECK_EQUAL(getChannels(Type::float64x4), 4);
}

BOOST_AUTO_TEST_CASE(test_getFullType) {
    // test with uint8, varying new channels
    BOOST_CHECK(getFullType(Type::uint8, 1) == Type::uint8);
    BOOST_CHECK(getFullType(Type::uint8, 1) == Type::uint8x1);
    BOOST_CHECK(getFullType(Type::uint8, 2) == Type::uint8x2);
    BOOST_CHECK(getFullType(Type::uint8, 3) == Type::uint8x3);
    BOOST_CHECK(getFullType(Type::uint8, 4) == Type::uint8x4);

    // test exceptions on multi-channel type as base type
    BOOST_CHECK_THROW(getFullType(Type::uint8x2, 1), image_type_error);
    BOOST_CHECK_THROW(getFullType(Type::uint8x3, 1), image_type_error);
    BOOST_CHECK_THROW(getFullType(Type::uint8x4, 1), image_type_error);

    // test with float64, varying new channels
    BOOST_CHECK(getFullType(Type::float64, 1) == Type::float64);
    BOOST_CHECK(getFullType(Type::float64, 1) == Type::float64x1);
    BOOST_CHECK(getFullType(Type::float64, 2) == Type::float64x2);
    BOOST_CHECK(getFullType(Type::float64, 3) == Type::float64x3);
    BOOST_CHECK(getFullType(Type::float64, 4) == Type::float64x4);
}

BOOST_AUTO_TEST_CASE(test_getBaseType) {
    // test with uint8
    BOOST_CHECK(getBaseType(Type::uint8)   == Type::uint8);
    BOOST_CHECK(getBaseType(Type::uint8x1) == Type::uint8);
    BOOST_CHECK(getBaseType(Type::uint8x2) == Type::uint8);
    BOOST_CHECK(getBaseType(Type::uint8x3) == Type::uint8);
    BOOST_CHECK(getBaseType(Type::uint8x4) == Type::uint8);

    // test with float64
    BOOST_CHECK(getBaseType(Type::float64)   == Type::float64);
    BOOST_CHECK(getBaseType(Type::float64x1) == Type::float64);
    BOOST_CHECK(getBaseType(Type::float64x2) == Type::float64);
    BOOST_CHECK(getBaseType(Type::float64x3) == Type::float64);
    BOOST_CHECK(getBaseType(Type::float64x4) == Type::float64);
}

BOOST_AUTO_TEST_CASE(test_getResultType) {
    // vary type
    BOOST_CHECK(getResultType(Type::uint8x1)   == Type::int16x1);
    BOOST_CHECK(getResultType(Type::int8x1)    == Type::int16x1);
    BOOST_CHECK(getResultType(Type::uint16x1)  == Type::int32x1);
    BOOST_CHECK(getResultType(Type::int16x1)   == Type::int32x1);
    BOOST_CHECK(getResultType(Type::int32x1)   == Type::int32x1);
    BOOST_CHECK(getResultType(Type::float32x1) == Type::float32x1);
    BOOST_CHECK(getResultType(Type::float64x1) == Type::float64x1);

    // vary channels for int16 and int32
    BOOST_CHECK(getResultType(Type::int16x1)   == Type::int32x1);
    BOOST_CHECK(getResultType(Type::int32x1)   == Type::int32x1);
    BOOST_CHECK(getResultType(Type::int16x2)   == Type::int32x2);
    BOOST_CHECK(getResultType(Type::int32x2)   == Type::int32x2);
    BOOST_CHECK(getResultType(Type::int16x3)   == Type::int32x3);
    BOOST_CHECK(getResultType(Type::int32x3)   == Type::int32x3);
    BOOST_CHECK(getResultType(Type::int16x4)   == Type::int32x4);
    BOOST_CHECK(getResultType(Type::int32x4)   == Type::int32x4);
}

BOOST_AUTO_TEST_CASE(test_getImageRangeMin) {
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::uint8x1),   0);
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::int8x2),    std::numeric_limits<int8_t>::min());
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::uint16x3),  0);
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::int16x4),   std::numeric_limits<int16_t>::min());
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::int32x1),   std::numeric_limits<int32_t>::min());
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::float32x2), 0);
    BOOST_CHECK_EQUAL(getImageRangeMin(Type::float64x3), 0);
}

BOOST_AUTO_TEST_CASE(test_getImageRangeMax) {
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::uint8x1),   std::numeric_limits<uint8_t>::max());
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::int8x2),    std::numeric_limits<int8_t>::max());
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::uint16x3),  std::numeric_limits<uint16_t>::max());
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::int16x4),   std::numeric_limits<int16_t>::max());
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::int32x1),   std::numeric_limits<int32_t>::max());
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::float32x2), 1);
    BOOST_CHECK_EQUAL(getImageRangeMax(Type::float64x3), 1);
}

BOOST_AUTO_TEST_CASE(test_BaseType) {
    BOOST_CHECK((std::is_same<BaseType<Type::int8>::base_type,    int8_t>::value));
    BOOST_CHECK((std::is_same<BaseType<Type::uint8>::base_type,   uint8_t>::value));
    BOOST_CHECK((std::is_same<BaseType<Type::int16>::base_type,   int16_t>::value));
    BOOST_CHECK((std::is_same<BaseType<Type::uint16>::base_type,  uint16_t>::value));
    BOOST_CHECK((std::is_same<BaseType<Type::int32>::base_type,   int32_t>::value));
    BOOST_CHECK((std::is_same<BaseType<Type::float32>::base_type, float>::value));
    BOOST_CHECK((std::is_same<BaseType<Type::float64>::base_type, double>::value));
}

BOOST_AUTO_TEST_CASE(test_DataType) {
    // test channels
    BOOST_CHECK_EQUAL(DataType<Type::uint8>::channels,   1);
    BOOST_CHECK_EQUAL(DataType<Type::uint8x1>::channels, 1);
    BOOST_CHECK_EQUAL(DataType<Type::uint8x2>::channels, 2);
    BOOST_CHECK_EQUAL(DataType<Type::uint8x3>::channels, 3);
    BOOST_CHECK_EQUAL(DataType<Type::uint8x4>::channels, 4);

    // test min
    BOOST_CHECK_EQUAL(DataType<Type::uint8>::min,   0);
    BOOST_CHECK_EQUAL(DataType<Type::int8>::min,    std::numeric_limits<int8_t>::min());
    BOOST_CHECK_EQUAL(DataType<Type::uint16>::min,  0);
    BOOST_CHECK_EQUAL(DataType<Type::int16>::min,   std::numeric_limits<int16_t>::min());
    BOOST_CHECK_EQUAL(DataType<Type::int32>::min,   std::numeric_limits<int32_t>::min());
    BOOST_CHECK_EQUAL(DataType<Type::float32>::min, 0);
    BOOST_CHECK_EQUAL(DataType<Type::float64>::min, 0);

    // test max
    BOOST_CHECK_EQUAL(DataType<Type::uint8>::max,   std::numeric_limits<uint8_t>::max());
    BOOST_CHECK_EQUAL(DataType<Type::int8>::max,    std::numeric_limits<int8_t>::max());
    BOOST_CHECK_EQUAL(DataType<Type::uint16>::max,  std::numeric_limits<uint16_t>::max());
    BOOST_CHECK_EQUAL(DataType<Type::int16>::max,   std::numeric_limits<int16_t>::max());
    BOOST_CHECK_EQUAL(DataType<Type::int32>::max,   std::numeric_limits<int32_t>::max());
    BOOST_CHECK_EQUAL(DataType<Type::float32>::max, 1);
    BOOST_CHECK_EQUAL(DataType<Type::float64>::max, 1);

    // test array type
    BOOST_CHECK((std::is_same<DataType<Type::int8>::array_type,   std::array<int8_t,1>>::value));
    BOOST_CHECK((std::is_same<DataType<Type::int8x1>::array_type, std::array<int8_t,1>>::value));
    BOOST_CHECK((std::is_same<DataType<Type::int8x2>::array_type, std::array<int8_t,2>>::value));
    BOOST_CHECK((std::is_same<DataType<Type::int8x3>::array_type, std::array<int8_t,3>>::value));
    BOOST_CHECK((std::is_same<DataType<Type::int8x4>::array_type, std::array<int8_t,4>>::value));
}

BOOST_AUTO_TEST_CASE(test_TypeTraits) {
    // check basetype, vary type
    BOOST_CHECK_EQUAL(TypeTraits<int8_t>::basetype,   Type::int8);
    BOOST_CHECK_EQUAL(TypeTraits<uint8_t>::basetype,  Type::uint8);
    BOOST_CHECK_EQUAL(TypeTraits<int16_t>::basetype,  Type::int16);
    BOOST_CHECK_EQUAL(TypeTraits<uint16_t>::basetype, Type::uint16);
    BOOST_CHECK_EQUAL(TypeTraits<int32_t>::basetype,  Type::int32);
    BOOST_CHECK_EQUAL(TypeTraits<float>::basetype,    Type::float32);
    BOOST_CHECK_EQUAL(TypeTraits<double>::basetype,   Type::float64);

    // check basetype, using array types
    BOOST_CHECK_EQUAL((TypeTraits<std::array<int16_t, 3>>::basetype),  Type::int16);
    BOOST_CHECK_EQUAL((TypeTraits<int16_t[3]>::basetype),              Type::int16);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Matx<int16_t, 3, 1>>::basetype), Type::int16);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Vec<int16_t, 3>>::basetype),     Type::int16);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Scalar_<int16_t>>::basetype),    Type::int16);

    // check channels, using array types
    BOOST_CHECK_EQUAL((TypeTraits<std::array<int16_t, 3>>::channels),  3);
    BOOST_CHECK_EQUAL((TypeTraits<int16_t[3]>::channels),              3);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Matx<int16_t, 3, 1>>::channels), 3);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Vec<int16_t, 3>>::channels),     3);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Scalar_<int16_t>>::channels),    4);

    // check full, using array types
    BOOST_CHECK_EQUAL((TypeTraits<std::array<int16_t, 3>>::fulltype),  Type::int16x3);
    BOOST_CHECK_EQUAL((TypeTraits<int16_t[3]>::fulltype),              Type::int16x3);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Matx<int16_t, 3, 1>>::fulltype), Type::int16x3);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Vec<int16_t, 3>>::fulltype),     Type::int16x3);
    BOOST_CHECK_EQUAL((TypeTraits<cv::Scalar_<int16_t>>::fulltype),    Type::int16x4);

    // test min
    BOOST_CHECK_EQUAL(TypeTraits<uint8_t>::min,   0);
    BOOST_CHECK_EQUAL(TypeTraits<int8_t>::min,  std::numeric_limits<int8_t>::min());
    BOOST_CHECK_EQUAL(TypeTraits<uint16_t>::min,  0);
    BOOST_CHECK_EQUAL(TypeTraits<int16_t>::min, std::numeric_limits<int16_t>::min());
    BOOST_CHECK_EQUAL(TypeTraits<int32_t>::min,  std::numeric_limits<int32_t>::min());
    BOOST_CHECK_EQUAL(TypeTraits<float>::min,    0);
    BOOST_CHECK_EQUAL(TypeTraits<double>::min,   0);

    // test max
    BOOST_CHECK_EQUAL(TypeTraits<uint8_t>::max,   std::numeric_limits<uint8_t>::max());
    BOOST_CHECK_EQUAL(TypeTraits<int8_t>::max,  std::numeric_limits<int8_t>::max());
    BOOST_CHECK_EQUAL(TypeTraits<uint16_t>::max,  std::numeric_limits<uint16_t>::max());
    BOOST_CHECK_EQUAL(TypeTraits<int16_t>::max, std::numeric_limits<int16_t>::max());
    BOOST_CHECK_EQUAL(TypeTraits<int32_t>::max,  std::numeric_limits<int32_t>::max());
    BOOST_CHECK_EQUAL(TypeTraits<float>::max,    1);
    BOOST_CHECK_EQUAL(TypeTraits<double>::max,   1);
}



// test call of a user functor, that requires compile time type information
struct PrintAllFunctor {
    Image const* img;
    bool print;

    PrintAllFunctor(Image const* img, bool do_print = true) : img{img}, print{do_print} { }

    template<Type t>
    bool operator()() const {
        using type = typename DataType<t>::base_type;
        for (int y = 0; y < img->height(); ++y) {
            for (int x = 0; x < img->width(); ++x) {
                if (print) {
                    std::cout << "( ";
                    for (unsigned int c = 0; c < img->channels(); ++c) {
                        type pixval = img->at<type>(x, y, c);
                        std::cout << std::setw(3) << (double)(pixval) << " ";
                    }
                    std::cout << "); ";
                }
            }
            if (print)
                std::cout << std::endl;
        }
        return true;
    }
};


BOOST_AUTO_TEST_CASE(unrestricted_functor_test) {
    // make an example image of some type
    constexpr Type t = Type::int8x2;
    Image img{5, 6, t};
    for (int x = 0; x < img.width(); ++x)
        for (int y = 0; y < img.height(); ++y)
            img.at<DataType<t>::base_type>(x, y, 0) = x + 100*y;

    // and call that functor without knowlegde of the image type. PrintAllFunctor works for all possible types. (The functor could also be called from a DataFusor algorithm like this.)
    bool do_really_print = false;
    bool return_value = false;
    BOOST_CHECK_NO_THROW(return_value = CallBaseTypeFunctor::run(PrintAllFunctor{&img, do_really_print}, img.type()));
    BOOST_CHECK(return_value);
}


// test call of a restricted user functor, that allows only floating point images
struct NormFunctor {
    Image* img;

    NormFunctor(Image* img) : img{img} { }

    template<Type t>
    double operator()() const { // return type cannot depend on Type
        using type = typename DataType<t>::base_type;
        unsigned int chans = img->channels(); // runtime number of channels
        static_assert(std::is_floating_point<type>::value, "This functor requires a floating point image.");
        type max = 0;
        for (int y = 0; y < img->height(); ++y)
            for (int x = 0; x < img->width(); ++x)
                for (unsigned int c = 0; c < chans; ++c)
                    max = std::max(max, std::abs(img->at<type>(x, y, c)));

        if (max > 0)
            for (int y = 0; y < img->height(); ++y)
                for (int x = 0; x < img->width(); ++x)
                    for (unsigned int c = 0; c < chans; ++c)
                        img->at<type>(x, y, c) /= max;

        return max;
    }

    using CallBaseTypeFunctor = CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>;
};


BOOST_AUTO_TEST_CASE(restricted_functor_test) {
    // make an example image of some type
    auto val = [] (int x, int y) { return x + 10 * y; };
    constexpr Type t = Type::float32x2;
    Image img{5, 6, t};
    for (int x = 0; x < img.width(); ++x) {
        for (int y = 0; y < img.height(); ++y) {
            img.at<DataType<t>::base_type>(x, y, 0) = val(x,y);
            img.at<DataType<t>::base_type>(x, y, 1) = 2 * val(x,y);
        }
    }

//    CallBaseTypeFunctor::run(NormFunctor{img}, img.type()); // gives compile error since it is restricted: static assertion failed: This functor requires a floating point image.
    BOOST_CHECK_THROW(   NormFunctor::CallBaseTypeFunctor::run(NormFunctor{&img}, Type::int32x1), image_type_error);
    BOOST_CHECK_NO_THROW(NormFunctor::CallBaseTypeFunctor::run(NormFunctor{&img}, img.type()));
    BOOST_CHECK_NO_THROW((CallBaseTypeFunctorRestrictBaseTypesTo<Type::float32, Type::float64>::run(NormFunctor{&img}, img.type())));

    double max = 2 * val(img.width() - 1, img.height() - 1);
    for (int x = 0; x < img.width(); ++x) {
        for (int y = 0; y < img.height(); ++y) {
            BOOST_CHECK_CLOSE(img.at<DataType<t>::base_type>(x, y, 0), val(x,y) / max, 1e-5);
            BOOST_CHECK_CLOSE(img.at<DataType<t>::base_type>(x, y, 1), 2 * val(x,y) / max, 1e-5);
        }
    }

//    std::cout << "Normed image: " << std::endl;
//    CallBaseTypeFunctor::run(PrintAllFunctor{&img}, img.type());
}



// test a specialized user functor
struct SpecializedFunctor {
    // we cannot partially specialize a template function, however, if we define all possibilities, this does not count at specialization
    // non-floating point types return a 0
    template<Type t>
    inline typename std::enable_if<!std::is_floating_point<typename DataType<t>::base_type>::value, int>::type operator()() const { return 0; }

    // floating point types return a 1
    template<Type t>
    inline typename std::enable_if<std::is_floating_point<typename DataType<t>::base_type>::value, int>::type operator()() const { return 1; }
};

// explicit specialization
// uint8 returns a 2
template<>
inline int SpecializedFunctor::operator()<Type::uint8>() const { return 2; }

// does the same, but is much easier.
// Note, since t is a compile time value, the ifs and elses could be optimized away and leave only the matching code "return ...;" in the function.
struct SimpleSpecializedFunctor {
    template<Type t>
    inline int operator()() const {
        if (t == Type::uint8)
            return 2;
        else if (std::is_floating_point<typename DataType<t>::base_type>::value)
            return 1;
        else // not uint8x1 and not floating point type
            return 0;
    }
};

BOOST_AUTO_TEST_CASE(specialized_functor_test) {
    // test some types
    Type ts8   = Type::int8x3;
    Type tu8   = Type::uint8x1;
    Type tu8x2 = Type::uint8x2;
    Type tf32  = Type::float32x2;
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SpecializedFunctor{},       ts8),   0);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SpecializedFunctor{},       tf32),  1);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SpecializedFunctor{},       tu8),   2);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SpecializedFunctor{},       tu8x2), 2);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SimpleSpecializedFunctor{}, ts8),   0);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SimpleSpecializedFunctor{}, tf32),  1);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SimpleSpecializedFunctor{}, tu8),   2);
    BOOST_CHECK_EQUAL(CallBaseTypeFunctor::run(SimpleSpecializedFunctor{}, tu8x2), 2);
}


// test call of a templated DataFusor algorithm
class SaturateIncrementFusor : public DataFusor {
public:
    using options_type = Options;

    void processOptions(Options const& o) override {
        options = o;
    }

    Options const& getOptions() const override {
        return options;
    }

    options_type options;
};

template<Type t>
class SaturateIncrementFusorImpl : public SaturateIncrementFusor {
    void predict(int date, ConstImage const& mask = ConstImage{}) override {
        (void)mask; // supresses compiler warning
        using type = typename DataType<t>::base_type;
        ConstImage src = imgs->get("test", date).sharedCopy(options.getPredictionArea());
        if (output.size() != src.size() || output.type() != src.type())
            output = Image{src.size(), src.type()}; // create new image

        for (int y = 0; y < output.height(); ++y)
            for (int x = 0; x < output.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (src.at<type>(x, y, c) <= std::numeric_limits<type>::max() - 1)
                        output.at<type>(x, y, c) = src.at<type>(x, y, c) + 1;
                    else
                        output.at<type>(x, y, c) = std::numeric_limits<type>::max();
    }
};

struct SimpleSaturateIncrementFusorFactory {
    template<Type t>
    std::unique_ptr<DataFusor> operator()() const {
        return std::unique_ptr<DataFusor>(new SaturateIncrementFusorImpl<t>{});
    }

    static std::unique_ptr<DataFusor> create(Type const& t) {
        return CallBaseTypeFunctor::run(SimpleSaturateIncrementFusorFactory{}, t);
    }
};

// test restrictions and non-default constructor
template<Type t>
struct SaturateIncrementFusorRestrictedImpl : public SaturateIncrementFusorImpl<t> {
    // channel and type restrictions
    using basetype = typename DataType<getBaseType(t)>::base_type;
    static_assert(std::is_unsigned<basetype>::value, "SaturateIncrementFusorRestrictedImpl needs an unsigned type.");

    // non default constructor
    SaturateIncrementFusorRestrictedImpl(Image const& i) {
        (void)i; // supresses compiler warning
        assert(i.basetype() == t);
        // do something
    }
};


struct RestrictedSaturateIncrementFusorFactory {
    Image const& img;

    RestrictedSaturateIncrementFusorFactory(Image const& i) : img{i} { }

    template<Type t>
    std::unique_ptr<DataFusor> operator()() const {
        return std::unique_ptr<DataFusor>(new SaturateIncrementFusorRestrictedImpl<t>{img});
    }

    static std::unique_ptr<DataFusor> create(Image const& i) {
        // using other channels or types will fail at compile-time because of restricting static asserts
        return CallBaseTypeFunctorRestrictBaseTypesTo<Type::uint8, Type::uint16>::run(RestrictedSaturateIncrementFusorFactory{i}, i.type());
    }
};




BOOST_AUTO_TEST_CASE(fusor_test) {
    // make an example image of some type
    constexpr Type t = Type::uint8x1; // int8x1 will fail at runtime because of restricted fusor
    Image img{5, 6, t};
    for (int x = 0; x < img.width(); ++x)
        for (int y = 0; y < img.height(); ++y)
            img.at<DataType<t>::base_type>(x, y, 0) = 120 + x + y;

//    std::cout << "Not incremented image: " << std::endl;
//    CallBaseTypeFunctor::run(PrintAllFunctor{&img}, img.type());

    Options o;
    o.setPredictionArea(Rectangle{0, 0, img.width(), img.height()});

    std::string tag = "test";
    std::shared_ptr<MultiResImages> mri{new MultiResImages};
    mri->set(tag, 0, std::move(img));

    // test incrementation with simple factory
    {
        auto img_orig = mri->get(tag,0).clone(); // clone is not necessary

        auto inc = SimpleSaturateIncrementFusorFactory::create(img_orig.type());
        inc->srcImages(mri);
        inc->processOptions(o);
        inc->predict(0);
        Image& img_res = inc->outputImage();

        using type = DataType<t>::base_type;
        for (int y = 0; y < img_res.height(); ++y)
            for (int x = 0; x < img_res.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (img_orig.at<type>(x, y, c) <= std::numeric_limits<type>::max() - 1)
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), img_orig.at<type>(x, y, c) + 1);
                    else
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), std::numeric_limits<type>::max());

//        std::cout << "Incremented image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_res}, img_res.type());
    }

    // test incrementation with restricted factory
    {
        auto img_orig = mri->get(tag,0).clone();

        auto inc = RestrictedSaturateIncrementFusorFactory::create(img_orig);
        inc->srcImages(mri);
        inc->processOptions(o);
        inc->predict(0);
        Image& img_res = inc->outputImage();

        using type = DataType<t>::base_type;
        for (int y = 0; y < img_res.height(); ++y)
            for (int x = 0; x < img_res.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (img_orig.at<type>(x, y, c) <= std::numeric_limits<type>::max() - 1)
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), img_orig.at<type>(x, y, c) + 1);
                    else
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), std::numeric_limits<type>::max());

//        std::cout << "Incremented image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_res}, img_res.type());
    }
}


// show how to use a templated data fusor with parallelizer
/* Make a class, which
 *  * you can instanciate once,
 *  * is copyable by copy-constructor and copy-assignment (required by Parallelizer) and
 *  * acts as proxy to the real DataFusor (inherit from Proxy), which cannot be copied easily.
 * Then you can make one instance as sample and give it to the parallelizer when constructing.
 * The Parallelizer will make n copies of that sample, where n is the number of threads.
 * When making the copy magic, do not forget the Proxy::df! Use either the base constructor or swap(Proxy&,Proxy&).
 * Note, this is an advanced pattern. To achieve the same, but without base class
 * and templated sub class, without a factory and without a Proxy object, see
 * below at SaturateDecrementFusor!
 */
class SaturateIncrementProxy : public Proxy<SaturateIncrementFusorImpl> {
public:
    // first construction by you to make a sample
    SaturateIncrementProxy(Type t)
        : Proxy{SimpleFactory::create(t)}, t{t}
    { }

    // all other ones are copies, called in Parallelizer
    SaturateIncrementProxy(SaturateIncrementProxy const& sample)
        : SaturateIncrementProxy{sample.t}
    { }

    SaturateIncrementProxy& operator=(SaturateIncrementProxy sample) noexcept {
        swap(*this, sample);
        return *this;
    }

    friend void swap(SaturateIncrementProxy& first, SaturateIncrementProxy& second) noexcept;

    // move is not required, but nice to have
    SaturateIncrementProxy(SaturateIncrementProxy&& sample) noexcept
        : Proxy{std::move(sample.df)}, t{sample.t}
    { }

    using options_type = SaturateIncrementFusor::options_type;

private:
    Type t;
};

inline void swap(SaturateIncrementProxy& first, SaturateIncrementProxy& second) noexcept {
    using std::swap;
    swap(static_cast<Proxy<SaturateIncrementFusorImpl>&>(first),   // very important!
         static_cast<Proxy<SaturateIncrementFusorImpl>&>(second)); // very important!
    swap(first.t, second.t);
}


// simple way to use a type dependent algorithm, which is also parallelizable.
struct SatDecrementerFunctor {
    ConstImage const& src;
    Image& out;

    SatDecrementerFunctor(ConstImage const& src, Image& out) : src{src}, out{out} { }

    template<Type t>
    inline void operator()() const {
        using type = typename DataType<t>::base_type;
        for (int y = 0; y < out.height(); ++y)
            for (int x = 0; x < out.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (src.at<type>(x, y, c) >= std::numeric_limits<type>::min() + 1)
                        out.at<type>(x, y, c) = src.at<type>(x, y, c) - 1;
                    else
                        out.at<type>(x, y, c) = std::numeric_limits<type>::min();
    }
};

class SaturateDecrementFusor : public DataFusor {
public:
    using options_type = Options;

    void processOptions(Options const& o) override {
        options = o;
    }

    Options const& getOptions() const override {
        return options;
    }

    void predict(int date, ConstImage const& mask = ConstImage{}) override {
        (void)mask; // supresses compiler warning
        ConstImage src = imgs->get("test", date).sharedCopy(options.getPredictionArea());
        if (output.size() != src.size() || output.type() != src.type())
            output = Image{src.size(), src.type()}; // create new image
        CallBaseTypeFunctor::run(SatDecrementerFunctor{src, output}, output.type());
    }

    options_type options;
};


// more advanced way to use a type dependent algorithm, which is also parallelizable.
// this is suitable if the type dependent part needs a lot of type depending preprocessing, which is required only once per instantiation
struct SatDecrementerBase {
    virtual ~SatDecrementerBase() = default;
    virtual std::unique_ptr<SatDecrementerBase> copy() const = 0;
    virtual void setSourceImage(ConstImage i) = 0;
    virtual void setOutputImage(Image i) = 0;
    virtual void dec() = 0;
    virtual Type type() const = 0;
};

template<Type t>
struct SatDecrementer : public SatDecrementerBase {
    ConstImage src;
    Image out;
    double d;

    // this example does not really make much sense, since the images given in the constructor are overriden before used
    // first construction expensive, copies are cheap
    SatDecrementer(ConstImage src, Image out) : src{std::move(src)}, out{std::move(out)}, d{0} {
        // ... expensive preprocessing to get d
    }

    std::unique_ptr<SatDecrementerBase> copy() const override {
        return std::unique_ptr<SatDecrementerBase>{new SatDecrementer<t>{*this}};
    }

    void setSourceImage(ConstImage i) override {
        src = std::move(i);
    }

    void setOutputImage(Image i) override {
        out = std::move(i);
    }

    void dec() override {
        using type = typename DataType<t>::base_type;
        for (int y = 0; y < out.height(); ++y)
            for (int x = 0; x < out.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c) {
                    if (src.at<type>(x, y, c) >= std::numeric_limits<type>::min() + 1)
                        out.at<type>(x, y, c) = src.at<type>(x, y, c) - 1;
                    else
                        out.at<type>(x, y, c) = std::numeric_limits<type>::min();
                }
    }

    Type type() const override {
        return t;
    }
};

struct SatDecrementerFactory {
    ConstImage src;
    Image out;

    SatDecrementerFactory(ConstImage src, Image out) : src{std::move(src)}, out{std::move(out)} { }

    template<Type t>
    std::unique_ptr<SatDecrementerBase> operator()() {
        return std::unique_ptr<SatDecrementerBase>(new SatDecrementer<t>{std::move(src), std::move(out)});
    }

    static std::unique_ptr<SatDecrementerBase> create(ConstImage src, Image out) {
        Type t = src.type();
        return CallBaseTypeFunctor::run(SatDecrementerFactory{std::move(src), std::move(out)}, t);
    }
};

class SaturateDecrementFusorWithObj : public DataFusor {
public:
    using options_type = Options;

    // expensive
    SaturateDecrementFusorWithObj(ConstImage src, Image out)
        : decrementer{SatDecrementerFactory::create(std::move(src), std::move(out))} { }

    // cheap
    SaturateDecrementFusorWithObj(SaturateDecrementFusorWithObj const& s)
        : DataFusor(s), decrementer{s.decrementer->copy()} { }

    SaturateDecrementFusorWithObj& operator=(SaturateDecrementFusorWithObj const& s) {
        decrementer = s.decrementer->copy();
        return *this;
    }

    void processOptions(Options const& o) override {
        options = o;
    }

    Options const& getOptions() const override {
        return options;
    }

    void predict(int date, ConstImage const& mask = ConstImage{}) override {
        (void)mask; // supresses compiler warning
        BOOST_CHECK_EQUAL(decrementer->type(), Type::int8x1); // fixed by test below, due to constexpr Type t = Type::int8x1;
        ConstImage src = imgs->get("test", date).sharedCopy(options.getPredictionArea());
        if (output.size() != src.size() || output.type() != src.type())
            output = Image{src.size(), src.type()}; // create new image

        decrementer->setSourceImage(std::move(src));
        decrementer->setOutputImage(Image{output.sharedCopy()});
        decrementer->dec();
    }

private:
    options_type options;
    std::unique_ptr<SatDecrementerBase> decrementer;
};




#ifdef WITH_OMP
// do the same as before, but using a Parallelizer
BOOST_AUTO_TEST_CASE(parallelized_fusor_test) {
    constexpr int width  = 6;
    constexpr int height = 5;
    constexpr Type t = Type::int8x1;
    Image img{width, height, t};
    for (int x = 0; x < img.width(); ++x)
        for (int y = 0; y < img.height(); ++y)
            img.at<DataType<t>::base_type>(x, y, 0) = 120 + x + y;

//    std::cout << "Not incremented image: " << std::endl;
//    CallBaseTypeFunctor::run(PrintAllFunctor{&img}, img.type());

    std::string tag = "test";
    std::shared_ptr<MultiResImages> mri{new MultiResImages};
    mri->set(tag, 0, std::move(img));

    ParallelizerOptions<Options> pOpt;
    pOpt.setNumberOfThreads(2);
    pOpt.setPredictionArea(Rectangle{/* x */ 0, /* y */ 0,  width, height});
    pOpt.setAlgOptions(Options{});

    {
        // here we set the output image to a shared copy of the src image, so it will get modified
        Image img_orig = mri->get(tag,0).clone();
        Image& img_res = mri->get(tag,0);

        Parallelizer<SaturateIncrementProxy> p{SaturateIncrementProxy{img_orig.type()}};
        p.srcImages(mri);
        p.processOptions(pOpt);
        p.outputImage() = Image{img_res.sharedCopy()};
        p.predict(0);

        using type = DataType<t>::base_type;
        for (int y = 0; y < img_res.height(); ++y)
            for (int x = 0; x < img_res.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (img_orig.at<type>(x, y, c) <= std::numeric_limits<type>::max() - 1)
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), img_orig.at<type>(x, y, c) + 1);
                    else
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), std::numeric_limits<type>::max());

//        std::cout << "Incremented image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_res}, img_res.type());
    }

    {
        // here we let the algorithm create a new output image
        Image img_orig = mri->get(tag,0).clone();

        Parallelizer<SaturateDecrementFusor> p;
        p.srcImages(mri);
        p.processOptions(pOpt);
        p.outputImage() = Image{};
        p.predict(0);
        // put clone of prediction of day 0 to day 1
        mri->set(tag, 1, p.outputImage());
        // predict from src day 1
        p.predict(1);
        Image& img_res = p.outputImage();

        using type = DataType<t>::base_type;
        for (int y = 0; y < img_res.height(); ++y)
            for (int x = 0; x < img_res.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (     img_orig.at<type>(x, y, c) >= std::numeric_limits<type>::min() + 2)
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), img_orig.at<type>(x, y, c) - 2);
                    else if (img_orig.at<type>(x, y, c) >= std::numeric_limits<type>::min() + 1)
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), img_orig.at<type>(x, y, c) - 1);
                    else
                        BOOST_CHECK_EQUAL(img_res.at<type>(x, y, c), std::numeric_limits<type>::min());

//        std::cout << "Doubly decremented image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_res}, img_res.type());
    }

    {
        Image img_orig = mri->get(tag,0).clone();
        Image& img_res = mri->get(tag,0);

//        std::cout << "Orig image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_orig}, img_orig.type());

        Parallelizer<SaturateDecrementFusorWithObj> p{SaturateDecrementFusorWithObj{img_orig, Image{img_res.sharedCopy()}}};
        p.srcImages(mri);
        p.processOptions(pOpt);
        p.outputImage() = Image{img_res.sharedCopy()};

        p.predict(0);
//        std::cout << "Singly decremented image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_res}, img_res.type());

        p.predict(0);
//        std::cout << "Doubly decremented image: " << std::endl;
//        CallBaseTypeFunctor::run(PrintAllFunctor{&img_res}, img_res.type());

        using type = DataType<t>::base_type;
        for (int y = 0; y < img_res.height(); ++y)
            for (int x = 0; x < img_res.width(); ++x)
                for (unsigned int c = 0; c < DataType<t>::channels; ++c)
                    if (     img_orig.at<type>(x, y, c) >= std::numeric_limits<type>::min() + 2)
                        BOOST_CHECK_EQUAL(static_cast<int>(img_res.at<type>(x, y, c)),
                                          static_cast<int>(img_orig.at<type>(x, y, c)) - 2);
                    else if (img_orig.at<type>(x, y, c) >= std::numeric_limits<type>::min() + 1)
                        BOOST_CHECK_EQUAL(static_cast<int>(img_res.at<type>(x, y, c)),
                                          static_cast<int>(img_orig.at<type>(x, y, c)) - 1);
                    else
                        BOOST_CHECK_EQUAL(static_cast<int>(img_res.at<type>(x, y, c)),
                                          static_cast<int>(std::numeric_limits<type>::min()));
    }
}
#endif /* WITH_OMP */

BOOST_AUTO_TEST_SUITE_END()

} /* namespace imagefusion */

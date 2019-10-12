#pragma once

#include <string>
#include <memory>
#include <iterator>
#include <type_traits>

#include <opencv2/opencv.hpp>
#include <boost/exception/error_info.hpp>

#include "imagefusion.h"
#include "Type.h"
#include "exceptions.h"
#include "iterators.h"
#include "fileformat.h"
#include "GeoInfo.h"

class GDALDataset;

namespace imagefusion {

class Image;

enum class ColorMapping {
    /**
     * @brief Convert Red-Green-Blue to Gray
     *
     * The result is
     * \f[
     *     Y = 0.299 \, R + 0.587 \, G + 0.114 \, B,
     * \f]
     * which is also the Y in ColorMapping::RGB_to_YCrCb_JPEG. The result fits into the source base
     * type, so the result type can be left Type::invalid. When using a different output type than
     * input type, the result is rescaled to the output range, which is [0, m], where m is the
     * maximum of the result image data range (see getImageRangeMax()).
     *
     * @remark A conversion from Gray to RGB is not available. Usually in such a conversion every
     * channel gets the same value. However, this can be achieved by using
     * @code
     * rgb.merge({gray.sharedCopy(), gray.sharedCopy(), gray.sharedCopy()});
     * @endcode
     */
    RGB_to_Gray,

    /**
     * @brief Convert Red-Green-Blue to normed CIE XYZ.Rec 709 with D65 white point
     *
     * The result is
     * \f[
     *     \begin{pmatrix} X\\ Y\\ Z \end{pmatrix}
     *     =
     *     \begin{pmatrix}
     *       \frac{1}{0.950455} & 0 & 0\\
     *       0                  & 1 & 0\\
     *       0                  & 0 & \frac{1}{1.088754}
     *     \end{pmatrix}
     *     \begin{pmatrix}
     *       0.412453 & 0.35758  & 0.180423\\
     *       0.212671 & 0.715159 & 0.072169\\
     *       0.019334 & 0.119194 & 0.950227
     *     \end{pmatrix}
     *     \begin{pmatrix} R\\ G\\ B \end{pmatrix}.
     * \f]
     * Note, the diagonal matrix norms the values so they make use of the full range. When using a
     * different output type than input type, the result is rescaled to the output range, which is
     * [0, m], where m is the maximum of the result image data range (see getImageRangeMax()).
     *
     *
     * The coefficients are taken from
     * [here](https://www.cs.rit.edu/~ncs/color/t_convert.html#RGB%20to%20XYZ%20&%20XYZ%20to%20RGB).
     * The scaling values are the row sums.
     */
    RGB_to_XYZ,

    /**
     * @brief Convert normed CIE XYZ.Rec 709 with D65 white point to Red-Green-Blue
     *
     * The result is
     * \f[
     *     \begin{pmatrix} R\\ G\\ B \end{pmatrix}
     *     =
     *     \begin{pmatrix}
     *        3.240479 & -1.53715  & -0.498535\\
     *       -0.969256 &  1.875992 &  0.041556\\
     *        0.055648 & -0.204043 &  1.057311
     *     \end{pmatrix}
     *     \begin{pmatrix}
     *       0.950455 & 0 & 0\\
     *       0        & 1 & 0\\
     *       0        & 0 & 1.088754
     *     \end{pmatrix}
     *     \begin{pmatrix} X\\ Y\\ Z \end{pmatrix}.
     * \f]
     * Note, the diagonal matrix scales the values to the original range. So be careful when
     * comparing values. When using a different output type than input type, the result is rescaled
     * to the output range, which is [0, m], where m is the maximum of the result image data range
     * (see getImageRangeMax()).
     *
     * The coefficients are taken from
     * [here](https://www.cs.rit.edu/~ncs/color/t_convert.html#RGB%20to%20XYZ%20&%20XYZ%20to%20RGB).
     * The scaling values are the row sums.
     */
    XYZ_to_RGB,

    /**
     * @brief Convert Red-Green-Blue to Luma, blue-difference and red-difference chroma
     *
     * The result is
     * \f{align*}{
     *     Y   &= 0.299 \, R + 0.587 \, G + 0.114 \, B\\
     *     C_b &= (B - Y) \cdot \frac{1}{1.772}\\
     *     C_r &= (R - Y) \cdot \frac{1}{1.402},
     * \f}
     * The natural range of Y (which is just the gray channel) is [0, m], where m is the maximum of
     * the result image data range (see getImageRangeMax()). For \f$ C_b, C_r \f$ the natural range
     * would be [-m/2, m/2]. For floating point and signed integer types these are scaled by the
     * factor 2, such that the range is [-m, m]. For unsigned integer type m/2 is added instead,
     * such that the range is [0, m] with the zero point is at round(m/2).
     *
     * Note, the order of Cr and Cb is different than in OpenCV. Here it is as usual (see e. g.
     * [ITU JFIF specification](http://www.itu.int/rec/T-REC-T.871-201105-I/en)).
     */
    RGB_to_YCbCr,

    /**
     * @brief Convert Luma, blue-difference and red-difference chroma to Red-Green-Blue
     *
     * The result is
     * \f{alignat*}{3
     *     R &= Y &              &                  &             +& 1.402 \cdot (C_r)\\
     *     G &= Y &- (0.114 \cdot& 1.772 \cdot (C_b)& + 0.299 \cdot& 1.402 \cdot (C_r)) / 0.587\\
     *     B &= Y &             +& 1.772 \cdot (C_b),
     * \f}
     * Note, that \f$ C_b \f$, \f$ C_r \f$ are not the plain values from the source image. Let \f$
     * C_b' \f$, \f$ C_r' \f$ be the plain values from the source image. Then for source images
     * with floating point and signed integer types it holds \f$ C_b = \frac{C_b'}{2} \f$ and \f$
     * C_r = \frac{C_r'}{2} \f$. For unsigned integer source images it holds \f$ C_b = C_b' - \frac
     * m 2 \f$ and \f$ C_r = C_r' - \frac m 2 \f$, where \f$ m \f$ is the maximum of the source
     * image range (see getImageRangeMax()). Then \f$ C_b \f$, \f$ C_r \f$ are in the range [-m/2,
     * m/2]. If the result image type differs from the source image type, the output values (\f$ R
     * \f$, \f$ G \f$, \f$ B \f$) are rescaled to the range [0, n], where n is the maximum of the
     * result image range.
     *
     * Note, the order of Cr and Cb is different than in OpenCV. Here it is as usual (see e. g.
     * [ITU JFIF specification](http://www.itu.int/rec/T-REC-T.871-201105-I/en)).
     */
    YCbCr_to_RGB,

    /** Convert Red-Green-Blue to Hue-Saturation-Value
     *
     * The values are calculated as follows:
     * \f{align*}{
     *     M &:= \max(R, G, B)\\
     *     S &:= \begin{cases}
     *            \frac{M - \min(R, G, B)}{M} \cdot n & \text{if } M \neq 0\\
     *            0 & \text{otherwise}
     *          \end{cases}\\
     *     H &:= \begin{cases}
     *            0 & \text{if } M = 0\\
     *                   60 \, \frac{G - B}{M - \min(R, G, B)}               \cdot \frac{n}{s} & \text{if } M \neq 0 \wedge M = R\\
     *            \left( 60 \, \frac{B - R}{M - \min(R, G, B)} + 120 \right) \cdot \frac{n}{s} & \text{if } M \neq 0 \wedge M = G\\
     *            \left( 60 \, \frac{R - G}{M - \min(R, G, B)} + 240 \right) \cdot \frac{n}{s} & \text{if } M \neq 0 \wedge M = B
     *          \end{cases}\\
     *     V &:= M \cdot \frac n m,
     * \f}
     * where \f$ n \f$ is the maximum of the output image data range and \f$ m \f$ the maximum of
     * the source image data range (see getImageRangeMax()). The scaling \f$ s \f$ is 1 for
     * floating point result images and 360 for other result image types.
     *
     * That \f$ H \f$ is 0 for no saturation is not required, but rather a convention.
     */
    RGB_to_HSV,

    /** Convert Hue-Saturation-Value to Red-Green-Blue
     *
     * First Saturation and Value are normalized to the range [0, 1]:
     * \f[
     *     S' := \frac S m,
     *     \qquad
     *     V' := \frac V m,
     * \f]
     * where \f$ m \f$ is the maximum of the source image data range (see getImageRangeMax()). Let
     * \f$ n \f$ be the maximum of the result image data range. Then, if \f$ S' = 0 \f$ it follows
     * that \f$ R = G = B = V' \cdot n \f$. Otherwise we calculate the values as follows:
     * \f{align*}{
     *     C &:= V' \cdot S'\\
     *     m &:= V' - C\\
     *     H' &:= \frac H s\\
     *     X &:= C \cdot (1 - |H'\ \mathrm{mod}\ 2 - 1|)\\
     *     (R',G',B') &:=
     *       \begin{cases}
     *         (C,X,0) &\text{if } 0 \leq H' < 1\\
     *         (X,C,0) &\text{if } 1 \leq H' < 2\\
     *         (0,C,X) &\text{if } 2 \leq H' < 3\\
     *         (0,X,C) &\text{if } 3 \leq H' < 4\\
     *         (X,0,C) &\text{if } 4 \leq H' < 5\\
     *         (C,0,X) &\text{if } 5 \leq H' < 6
     *       \end{cases}\\
     *     (R, G, B) &= (R' + m, G' + m, B' + m)
     * \f}
     * where \f$ n \f$ is the maximum of the output image data range and \f$ m \f$ the maximum of
     * the source image data range (see getImageRangeMax()). The scaling \f$ s \f$ is 1 for
     * floating point source images and 60 for other source image types.
     *
     * That \f$ H \f$ is 0 for no saturation is not required, but rather a convention.
     */
    HSV_to_RGB,

    /** Convert Red-Green-Blue to Hue-Luminosity-Saturation
     *
     * The values are calculated as follows:
     * \f{align*}{
     *     V_{\text{max}} &:= \max(R, G, B)\\
     *     V_{\text{min}} &:= \min(R, G, B)\\
     *     L &:= \frac{V_{\text{max}} + V_{\text{min}}}{2} \cdot \frac n m\\
     *     S &:= \begin{cases}
     *            \frac{V_{\text{max}} - V_{\text{min}}}{V_{\text{max}} + V_{\text{min}}} \cdot n & \text{if } 0 < L < \frac n 2\\
     *            \frac{V_{\text{max}} - V_{\text{min}}}{2 \, m - V_{\text{max}} - V_{\text{min}}} \cdot n & \text{if } \frac n 2 \le L < n\\
     *            0 & \text{otherwise}
     *          \end{cases}\\
     *     H &:= \begin{cases}
     *            0 & \text{if } S = 0\\
     *                   60 \, \frac{G - B}{V_{\text{max}} - V_{\text{min}}}               \cdot \frac{n}{s} & \text{if } S \neq 0 \wedge V_{\text{max}} = R\\
     *            \left( 60 \, \frac{B - R}{V_{\text{max}} - V_{\text{min}}} + 120 \right) \cdot \frac{n}{s} & \text{if } S \neq 0 \wedge V_{\text{max}} = G\\
     *            \left( 60 \, \frac{R - G}{V_{\text{max}} - V_{\text{min}}} + 240 \right) \cdot \frac{n}{s} & \text{if } S \neq 0 \wedge V_{\text{max}} = B
     *          \end{cases}
     * \f}
     * where \f$ n \f$ is the maximum of the output image data range and \f$ m \f$ the maximum of
     * the source image data range (see getImageRangeMax()). The scaling \f$ s \f$ is 1 for
     * floating point result images and 360 for other result image types.
     *
     * That \f$ H \f$ is 0 for no saturation is not required, but rather a convention.
     */
    RGB_to_HLS,

    /** Convert Hue-Luminosity-Saturation to Red-Green-Blue
     *
     * First Saturation and Luminosity are normalized to the range [0, 1]:
     * \f[
     *     S' := \frac S m,
     *     \qquad
     *     L' := \frac L m,
     * \f]
     * where \f$ m \f$ is the maximum of the source image data range (see
     * getImageRangeMax()). Let \f$ n \f$ be the maximum of the result image
     * data range.
     * Then, if \f$ S' = 0 \f$ it follows that \f$ R = G = B = L' \cdot n \f$.
     * Otherwise we calculate the values as follows:
     * \f{align*}{
     *     C &:= (1 - |2\,L' - 1|) \cdot S'\\
     *     m &:= L' - \frac C 2\\
     *     H' &:= \frac H s\\
     *     X &:= C \cdot (1 - |H'\ \mathrm{mod}\ 2 - 1|)\\
     *     (R',G',B') &:=
     *       \begin{cases}
     *         (C,X,0) &\text{if } 0 \leq H' < 1\\
     *         (X,C,0) &\text{if } 1 \leq H' < 2\\
     *         (0,C,X) &\text{if } 2 \leq H' < 3\\
     *         (0,X,C) &\text{if } 3 \leq H' < 4\\
     *         (X,0,C) &\text{if } 4 \leq H' < 5\\
     *         (C,0,X) &\text{if } 5 \leq H' < 6
     *       \end{cases}\\
     *     (R, G, B) &= (R' + m, G' + m, B' + m)
     * \f}
     * where \f$ n \f$ is the maximum of the output image data range and \f$ m \f$ the maximum of
     * the source image data range (see getImageRangeMax()). The scaling \f$ s \f$ is 1 for
     * floating point source images and 60 for other source image types.
     *
     * That \f$ H \f$ is 0 for no saturation is not required, but rather a convention.
     */
    HLS_to_RGB,

    /**
     * @brief Convert Red-Green-Blue to CIE L*a*b*
     *
     * First RGB is transformed to normed XYZ, see ColorMapping::RGB_to_XYZ. These are normalized
     * to the range [0,1] by using \f$ X_n := \frac X m \f$, \f$ Y_n := \frac Y m \f$ and
     * \f$ Z_n := \frac Z m \f$, where \f$ m \f$ is the maximum of the source image range (see
     * getImageRangeMax()). From these values \f$ L^* \f$, \f$ a^* \f$ and \f$ b^* \f$ are
     * calculated as follows:
     * \f{align*}{
     *     L^* &:= ( 1.16 \cdot f(Y_n) - 0.16) \cdot n\\
     *     a^* &:= \left( 500 \cdot (f(X_n) - f(Y_n)) \cdot \frac{1}{s} + o \right) \cdot n\\
     *     b^* &:= \left( 200 \cdot (f(Y_n) - f(Z_n)) \cdot \frac{1}{s} + o \right) \cdot n,
     * \f}
     * where
     * \f[
     *     f(t) := \begin{cases}
     *               \sqrt[3]{t}                            & \text{if } t > \delta^3\\
     *               \frac{t}{3\,\delta^2} + \frac{16}{116} & \text{otherwise}
     *             \end{cases},
     * \f]
     * \f$ \delta := \frac{6}{29} \f$ and \f$ n \f$ is the maximum of the output image range and
     * \f$ s \f$ is a scale factor and \f$ o \f$ is an offset.
     *  * For result images with floating point type
     *    \f$ s = 1 \f$, \f$ o = 0 \f$. Then
     *    \f$ a^* \f$ is in the range [-86.1813, 98.2352] and
     *    \f$ b^* \f$ is in the range [-107.862, 94.4758].
     *  * For result images with unsigned integer type
     *    \f$ s = 206.0972 \f$, \f$ o = 0.52335499948568 \f$. Then
     *    \f$ a^* \f$ is in the range \f$ n \cdot [0.10519648, 1] \f$ and
     *    \f$ b^* \f$ is in the range \f$ n \cdot [0, 0.981759] \f$.
     *  * For result images with signed integer type
     *    \f$ s = 107.862 \cdot \frac{n}{n + 1} \f$, \f$ o = 0 \f$. Then
     *    \f$ a^* \f$ is in the range \f$ (n+1) \cdot [-0.798995939, 0.9107489] \f$ and
     *    \f$ b^* \f$ is in the range \f$ (n+1) \cdot [-1, 0.8758951] \f$.
     *
     * Note, for all types \f$ L \f$ is in the range \f$ [0, n] \f$ (standard
     * is [0,100]).
     */
    RGB_to_Lab,

    /**
     * @brief Convert CIE L*a*b* to Red-Green-Blue
     *
     * First \f$ L^* \f$ is scaled to [0, 1] and \f$ a^* \f$ and \f$ b^* \f$ to their natural
     * ranges by using
     * \f$ L^*_n := \frac{L^*}{n} \f$,
     * \f$ a^*_n := s \cdot \left( \frac{a^*}{n} - o \right) \f$ and
     * \f$ b^*_n := s \cdot \left( \frac{b^*}{n} - o \right) \f$, where n is the maximum of the
     * source image range (see getImageRangeMax()). For \f$ s \f$ and \f$ o \f$ see
     * ColorMapping::RGB_to_Lab. From these values \f$ X \f$, \f$ Y \f$ and \f$ Z \f$ are
     * calculated as follows:
     * \f{align*}{
     *     X &:= m \cdot f^{-1}\left( \frac{L^*_n + 0.16}{1.16} + \frac{a^*_n}{500} \right)\\
     *     Y &:= m \cdot f^{-1}\left( \frac{L^*_n + 0.16}{1.16} \right)\\
     *     Z &:= m \cdot f^{-1}\left( \frac{L^*_n + 0.16}{1.16} - \frac{b^*_n}{200} \right),
     * \f}
     * where
     * \f[
     *     f^{-1}(t) := \begin{cases}
     *                    t^3                                                 & \text{if } t > \delta\\
     *                    3\,\delta^2 \cdot \left( t - \frac{16}{116} \right) & \text{otherwise}
     *                  \end{cases}
     * \f]
     * and \f$ \delta := \frac{6}{29} \f$ and \f$ m \f$ is the maximum of the output image range.
     *
     * Then the XYZ coordinates are transformed to RGB using ColorMapping::XYZ_to_RGB.
     */
    Lab_to_RGB,

    /**
     * @brief Convert Red-Green-Blue to CIE L*u*v*
     *
     * First RGB is transformed to normed XYZ, see ColorMapping::RGB_to_XYZ. These are normalized
     * to the range [0,1] by using \f$ X_n := \frac X m \f$, \f$ Y_n := \frac Y m \f$ and
     * \f$ Z_n := \frac Z m \f$, where \f$ m \f$ is the maximum of the source image range (see
     * getImageRangeMax()). From these values \f$ L^* \f$, \f$ u^* \f$ and \f$ v^* \f$ are
     * calculated as follows:
     * \f{align*}{
     *     L^* &:= (1.16 \cdot f(Y_n) - 0.16) \cdot n\\
     *     u^* &:= \left( 1300 \cdot L^* \cdot (u' - u_n') \cdot \frac 1 s + o \right) \cdot n\\
     *     v^* &:= \left( 1300 \cdot L^* \cdot (v' - v_n') \cdot \frac 1 s + o \right) \cdot n\\
     * \f}
     * where
     * \f[
     *     f(t) := \begin{cases}
     *               \sqrt[3]{t}                          & \text{if } t > \delta^3\\
     *               \frac{t}{3\,\delta^2} + \frac{4}{29} & \text{otherwise}
     *             \end{cases},
     * \f]
     * \f{align*}{
     *     u' &:= \frac{4 \, X_n}{X_n + 15 \, Y_n + 3 \, Z_n} \quad\text{if } X_n + Y_n + Z_n \neq 0,
     *     v' &:= \frac{9 \, Y_n}{X_n + 15 \, Y_n + 3 \, Z_n} \quad\text{if } X_n + Y_n + Z_n \neq 0,
     * \f}
     * and \f$ u' = v' = 0 \f$ if \f$ X_n + Y_n + Z_n = 0 \f$.
     * \f$ \delta := \frac{6}{29} \f$, \f$ u_n' := 0.2009 \f$, \f$ v_n' = 0.461 \f$
     * ((2° observer and standard illuminant C)[https://en.wikipedia.org/wiki/CIELUV]),
     * \f$ n \f$ is the maximum of the output image range
     * and \f$ s \f$ is a scale factor and \f$ o \f$ is an offset.
     *  * For result images with floating point type
     *    \f$ s = 1 \f$, \f$ o = 0 \f$. Then
     *    \f$ u^* \f$ is in the range [-78.9986, 187.66] and
     *    \f$ v^* \f$ is in the range [-125.544, 116.356].
     *  * For result images with unsigned integer type
     *    \f$ s = 313.204 \f$, \f$ o = 0.400837792620784 \f$. Then
     *    \f$ u^* \f$ is in the range \f$ n \cdot [0.14861049, 1] \f$ and
     *    \f$ v^* \f$ is in the range \f$ n \cdot [0, 0.77234007] \f$.
     *  * For result images with signed integer type
     *    \f$ s = 187.66 \f$, \f$ o = 0 \f$. Then
     *    \f$ u^* \f$ is in the range \f$ n \cdot [-0.4209666, 1] \f$ and
     *    \f$ v^* \f$ is in the range \f$ n \cdot [-0.668997, 0.620036] \f$.
     *
     * Note, for all types \f$ L \f$ is in the range \f$ [0, n] \f$ (standard is [0,100]).
     */
    RGB_to_Luv,

    /**
     * @brief Convert CIE L*u*v* to Red-Green-Blue
     *
     * If \f$ L = 0 \f$ we set \f$ R = G = B = 0 \f$. Otherwise we scale \f$ L^* \f$ is to [0, 1]
     * and \f$ u^* \f$ and \f$ v^* \f$ to their natural ranges by using
     * \f$ L^*_n := \frac{L^*}{n} \f$,
     * \f$ u^*_n := s \cdot \left( \frac{u^*}{n} - o \right) \f$ and
     * \f$ v^*_n := s \cdot \left( \frac{v^*}{n} - o \right) \f$, where n is the maximum of the
     * source image range (see getImageRangeMax()). For \f$ s \f$ and \f$ o \f$ see
     * ColorMapping::RGB_to_Luv. From these values \f$ X \f$, \f$ Y \f$ and \f$ Z \f$ are
     * calculated as follows:
     * \f{align*}{
     *     u' &:= \frac{u^*_n}{1300 \, L^*_n} + u'_n
     *     v' &:= \frac{v^*_n}{1300 \, L^*_n} + v'_n
     *     Y &:= m \cdot f^{-1}\left( \frac{L^*_n + 0.16}{1.16} \right)\\
     *     X &:= m \cdot Y \cdot \frac{9 \, u'}{4 \, v'}\\
     *     Z &:= m \cdot Y \cdot \frac{12 - 3 \, u' - 20 \, v'}{4 \, v'},
     * \f}
     * where
     * \f[
     *     f^{-1}(t) := \begin{cases}
     *                    t^3                                                 & \text{if } t > \delta\\
     *                    3\,\delta^2 \cdot \left( t - \frac{16}{116} \right) & \text{otherwise}
     *                  \end{cases}
     * \f]
     * and \f$ \delta := \frac{6}{29} \f$ and \f$ m \f$ is the maximum of the output image range.
     * For \f$ u'_n \f$ and \f$ v'_n \f$ see ColorMapping::RGB_to_Luv.
     *
     * Then the XYZ coordinates are transformed to RGB using ColorMapping::XYZ_to_RGB.
     */
    Luv_to_RGB,

    /**
     * @brief Convert two channels to normalized difference index
     *
     * There are many simple normalized difference indices. The formula is always
     * \f[
     *     Y = \left( \frac{\mathrm P - \mathrm N}{\mathrm P + \mathrm N} \cdot \frac 1 s + o \right) \cdot n,
     * \f]
     * where \f$ n \f$ is the maximum of the result image data range (see getImageRangeMat()).
     *  * For floating point and signed integer result images \f$ s = 1 \f$ and
     *    \f$ o = 0 \f$. So \f$ Y \in [-n, n] \f$.
     *  * For unsigned integer result images \f$ s = 2 \f$ and \f$ o = 0.5 \f$.
     *    So \f$ Y \in [0, n] \f$ with round(n/2) being the zero offset.
     *
     * The following table tells you, which channels are to use for which index:
     * <table>
     * <tr><th> Index                    <th colspan="2"> Channels
     * <tr><td> NDVI (Vegetation)        <td> P: NIR   <td> N: Red
     * <tr><td> GNDVI (Green Vegetation) <td> P: NIR   <td> N: Green
     * <tr><td> NDBI (Built-Up)          <td> P: SWIR1 <td> N: NIR
     * <tr><td> NDWI (Water)             <td> P: NIR   <td> N: Red
     * <tr><td> MNDWI (Modified Water)   <td> P: Green <td> N: SWIR1
     * <tr><td> NDSI (Snow)              <td> P: Green <td> N: SWIR1
     * </table>
     *
     * The wavelength ranges of the bands are listed
     * [here](www.harrisgeospatial.com/docs/spectralindices.html) and in
     * [Landsat Thematic Mapper](https://landsat.usgs.gov/what-are-band-designations-landsat-satellites).
     */
    Pos_Neg_to_NDI,

    /**
     * @brief Convert Red, Near Infrared and Shortwave Infrared 1 to continuous Build-Up Index
     *
     * The result is
     * \f[
     *     Y = \left( \left( \frac{SWIR - NIR}{SWIR + NIR} - \frac{NIR - Red}{NIR + Red} \right) \cdot \frac 1 s + o \right) \cdot n,
     * \f]
     * where \f$ n \f$ is the maximum of the result image data range (see getImageRangeMat()).
     *  * For floating point and signed integer result images \f$ s = 2 \f$ and
     *    \f$ o = 0 \f$. So \f$ Y \in [-n, n] \f$.
     *  * For unsigned intger result images \f$ s = 4 \f$ and \f$ o = 0.5 \f$.
     *    So \f$ Y \in [0, n] \f$ with round(n/2) being the zero offset.
     *
     * In
     * [Landsat Thematic Mapper](https://landsat.usgs.gov/what-are-band-designations-landsat-satellites)
     * the wavelength of Near Infrared is between 0.77 µm and 0.9 µm and the wavelength of
     * Shortwave Infrared 1 is between 1.55 µm and 1.75 µm.
     */
    Red_NIR_SWIR_to_BU
};

/**
 * @brief getFromString
 * @param map
 * @return
 */
inline std::string getFromString(ColorMapping map) {
    switch (map) {
    case ColorMapping::RGB_to_Gray:
    case ColorMapping::RGB_to_XYZ:
    case ColorMapping::RGB_to_YCbCr:
    case ColorMapping::RGB_to_HLS:
    case ColorMapping::RGB_to_Lab:
    case ColorMapping::RGB_to_Luv:
    case ColorMapping::RGB_to_HSV:
        return "Red-Green-Blue";
    case ColorMapping::XYZ_to_RGB:
        return "CIE XYZ.Rec 709 with D65 white point";
    case ColorMapping::YCbCr_to_RGB:
        return "YCbCr (JPEG)";
    case ColorMapping::HSV_to_RGB:
        return "Hue-Saturation-Value";
    case ColorMapping::HLS_to_RGB:
        return "Hue-Luminosity-Saturation";
    case ColorMapping::Lab_to_RGB:
        return "CIE L*a*b*";
    case ColorMapping::Luv_to_RGB:
        return "CIE L*u*v*";
    case ColorMapping::Pos_Neg_to_NDI:
        return "GeneralPositiveNegative";
    case ColorMapping::Red_NIR_SWIR_to_BU:
        return "Red-NearInfrared-ShortwaveInfrared";
    }
    assert(false && "This should not happen. Fix getFromString(ColorMapping)!");
    return "";
}

inline std::string getToString(ColorMapping map) {
    switch (map) {
    case ColorMapping::RGB_to_Gray:
        return "Gray";
    case ColorMapping::XYZ_to_RGB:
    case ColorMapping::YCbCr_to_RGB:
    case ColorMapping::HLS_to_RGB:
    case ColorMapping::Lab_to_RGB:
    case ColorMapping::Luv_to_RGB:
    case ColorMapping::HSV_to_RGB:
        return "Red-Green-Blue";
    case ColorMapping::RGB_to_XYZ:
        return "CIE XYZ.Rec 709 with D65 white point";
    case ColorMapping::RGB_to_YCbCr:
        return "YCbCr (JPEG)";
    case ColorMapping::RGB_to_HSV:
        return "Hue-Saturation-Value";
    case ColorMapping::RGB_to_HLS:
        return "Hue-Luminosity-Saturation";
    case ColorMapping::RGB_to_Lab:
        return "CIE L*a*b*";
    case ColorMapping::RGB_to_Luv:
        return "CIE L*u*v*";
    case ColorMapping::Pos_Neg_to_NDI:
        return "Normalized Difference Index";
    case ColorMapping::Red_NIR_SWIR_to_BU:
        return "Continuous Build-Up Index";
    }
    assert(false && "This should not happen. Fix getToString(ColorMapping)!");
    return "";
}

inline std::string to_string(ColorMapping map) {
    return getFromString(map) + " to " + getToString(map);
}


/**
 * @brief A value of a single channel with location
 *
 * This can be used to save some value at (p.x, p.y).
 *
 * @see minmaxLocations
 */
struct ValueWithLocation {
    double val;
    Point p;
};

inline bool operator==(ValueWithLocation const& v1, ValueWithLocation const& v2) {
    return v1.val == v2.val && v1.p.x == v2.p.x && v1.p.y == v2.p.y;
}

inline bool operator!=(ValueWithLocation const& v1, ValueWithLocation const& v2) {
    return !(v1 == v2);
}

/**
 * @brief Constant version of Image
 *
 * This is an Image which does not allow for modifications, at least not easily. A ConstImage can
 * always be used as argument type for a function, when an Image is just used as read-only source.
 * Usual Image%s can be cheaply converted implicitly to ConstImage%s, since ConstImage is the base
 * class of Image and Image does not add any field.
 *
 * @see Image
 *
 * \ingroup typegrp
 */
class ConstImage {
protected:
    /**
     * @brief OpenCV matrix that holds the image data
     *
     * This `cv::Mat` object is the only state of Image. So it can be changed by the user without
     * worrying about inconsistent states. That is why it is accessible with cvMat(). This has also
     * the advantage that every method from OpenCV is usable on it and thus -- indirectly -- on
     * Image.
     *
     * Generally, Image can be considered as interface for `cv::Mat`. It adds some high level
     * features and read and write methods, which use GDAL to support more image formats. Also,
     * copying an Image copies the image contents (cloning), so it behaves quite the standard way
     * in contrast to `cv::Mat`. With access to this member no feature from OpenCV is really
     * missing.
     *
     * Beware the different ordering of width and height and also x and y of Image in contrast to
     * `cv::Mat`. Image uses (width, height) and (x, y).
     */
    cv::Mat img;

    friend class Image;

public:
    /**
     * @brief Construct empty image without size
     */
    ConstImage() = default;


    /**
     * @brief Construct with OpenCV's Mat
     */
    ConstImage(cv::Mat img) noexcept;


    /**
     * @brief Construct from size and type
     * @param s is the size with area > 0 of the image.
     * @param t is the full type of the image, e. g. Type::uint16x3 means 3 channel image with 16
     * bit unsigned integers
     *
     * Note, for performance reasons the image is not initialized. It is filled with arbitrary
     * data.
     *
     * @throws size_error if s.width &le; 0 or s.height &le; 0. Note zero sized images are not
     * supported, except for default construction.
     */
    ConstImage(Size s, Type t);


    /**
     * @brief Construct image
     * @param width is the width > 0 of the image.
     * @param height is the height > 0 of the image.
     * @param t is the full type of the image, e. g. Type::uint16x3 means 3 channel image with 16
     * bit unsigned integers
     *
     * Note, for performance reasons the image is not initialized. It is filled with arbitrary
     * data.
     *
     * @throws size_error if width &le; 0 or height &le; 0. Note zero sized images are not
     * supported, except for default construction.
     */
    ConstImage(int width, int height, Type t);


    /**
     * @brief Construct image from an image file
     *
     * @param filename is the image file to read
     *
     * @param channels specifies optionally which channels (0-based) to read, otherwise all will be
     * read. Example: {0, 2} would only read channels 0 and 2. Example: {0,0,0} would work for a
     * single-channel source image and would read channel 0 three times to fill a 3-channel image.
     *
     * @param r limits optionally the region to read. Note, a zero at width or height means full
     * width or full height, respectively, but are limited then by the bounds. Also note in case of
     * `flipH` or `flipV` the region is specified in the unflipped image.
     *
     * @param flipH sets whether to read the image flipped horizontally.
     *
     * @param flipV sets whether to read the image flipped vertically.
     *
     * @param ignoreColorTable determines, whether a possibly existing color table will be ignored.
     * When using `false` an image with indexed colors will not be converted. This is for example
     * important when reading python-fmask images. For images without color table
     * `ignoreColorTable` does not have any effect. Note, that the support for writing color tables
     * is rather limited and depends on the driver. So when not converting the colors directly the
     * color information might be lost or worse: changed in such a way, that it will result in a
     * different number of channels. So you might want to remove the color table in the
     * corresponding GeoInfo object to prevent that case. See the documentation at @ref
     * GeoInfo::addTo(std::string const&) const
     *
     * This constructor uses the @ref Image::read method. See there for details regarding how to
     * handle GeoInfo.
     *
     * This can throw at least the following exceptions:
     *
     * @throws runtime_error if `filename` cannot be found or opened with any GDAL driver.
     *
     * @throws size_error if `r` is ill-formed, i. e. out of the bounds of the image or has
     * negative width or height.
     *
     * @throws image_type_error if `channels` specifies channels that do not exist.
     *
     * @see GeoInfo
     */
    explicit ConstImage(std::string const& filename, std::vector<int> channels = {}, Rectangle r = {0, 0, 0, 0}, bool flipH = false, bool flipV = false, bool ignoreColorTable = false);


    /**
     * @brief Copy construct an image by cloning
     * @param i is the image to clone.
     *
     * Note, copying an Image or a ConstImage means copying the contents of the image. So, in
     * OpenCV terminology the copy constructor clones an image. This is very different to the
     * OpenCV copy constructor, which only makes a shared copy with a shared pixel memory. To get
     * such a shared copy, use sharedCopy().
     */
    ConstImage(ConstImage const& i);


    /// \copydoc ConstImage(ConstImage const& i)
    ConstImage(Image const& i);


    /**
     * @brief Move an image
     * @param i is the image to be moved from.
     *
     * Move an image to construct a new one. Do not use the moved image afterwards! It is not in a
     * valid state, when it is moved away (it is an empty Image without size).
     *
     * The efficiency depends on OpenCV. Currently, it might involve some flat copies (shared
     * copies).
     */
    ConstImage(ConstImage&& i) noexcept;


    /// \copydoc ConstImage(ConstImage&& i)
    ConstImage(Image&& i);


    /**
     * @brief Copy (clone) and move assignment
     * @param i is the source image.
     *
     * Used by assigning to an existing object. If `i` has not been moved, `i` is cloned.
     *
     * @return reference to the assigned image for convenience
     */
    ConstImage& operator=(ConstImage i) noexcept;


    /**
     * @brief Destructor
     */
    virtual ~ConstImage() = default;


    /**
     * @brief Access the underlying `cv::Mat`
     *
     * This gives direct access to the underlying OpenCV `Mat` object. This is the only state of
     * Image and thus can be used without breaking any invariants.
     *
     * @return reference to the underlying OpenCV `Mat`
     *
     * @see img
     */
    cv::Mat const& cvMat() const;


    /**
     * @brief Open the underlying `cv::Mat` memory as GDAL Dataset
     *
     * This uses the same image memory as OpenCV, but opens it as GDAL Dataset. Note, the dataset
     * will only contain image data, no metadata or geoinfo. However, GeoInfo can be added to the
     * dataset easily In windows it is recommended that after working with it the Dataset should be
     * explicitly closed like in the following.
     * @code
     * GeoInfo gi = // ...
     *
     * Image img = // ...
     * GDALDataset* ds1 = img.asGDALDataset();
     * gi.addTo(ds1);
     *
     * ConstImage cimg = // ...
     * GDALDataset* ds2 = const_cast<GDALDataset*>(cimg.asGDALDataset());
     * gi.addTo(ds2);
     *
     * // ... do stuff
     *
     * GDALClose(ds1);
     * GDALClose(ds2);
     * @endcode
     *
     * @return pointer to the image on a GDAL dataset.
     */
    GDALDataset const* asGDALDataset() const;


    /**
     * @brief Write an Image to a file
     *
     * @param filename (or path) for the image to write to
     *
     * @param gi is the GeoInfo that will be added to the image file. Specifying it on writing is
     * different than updating the file later with GeoInfo::addTo. Many image drivers do not
     * support to update the file. Note that write support of color tables is limited by the GDAL
     * drivers. For handling of color tables see the documentation at @ref
     * GeoInfo::addTo(std::string const& filename) const
     *
     * @param format is the image file format. When left to FileFormat::unsupported, the format is
     * guessed from the file extension.
     *
     * @throws file_format_error if guessing from the file extension fails.
     *
     * -------------
     *
     * @throws runtime_error if the output file cannot be opened or written to. This might also be
     * caused by the output driver, when it does not support writing in general or an Image with a
     * specific Type (like Type::float32).
     */
    void write(std::string const& filename, GeoInfo const& gi = {}, FileFormat format = FileFormat::unsupported) const;


    /**
     * @brief Write an Image to a file
     *
     * @param filename is the image file to write to
     *
     * @param drivername is the GDAL driver name to use. This determines the image format. Example:
     * "GTiff"
     *
     * @param options are name value pairs for additional options. This is the only reason to use
     * this write method. If you do not want to set any specific options, use write(std::string
     * const& filename, GeoInfo const& gi, FileFormat format) const instead.
     *
     * @param gi is the GeoInfo that will be added to the image file. Specifying it on writing is
     * different than updating the file later with GeoInfo::addTo. Many image drivers do not
     * support to update the file. For handling of color tables see the documentation at @ref
     * GeoInfo::addTo(std::string const& filename) const
     *
     * This method uses GDAL to write an image with a GDAL driver selected only by `drivername`.
     * You can see the list of available drivers [here](http://www.gdal.org/formats_list.html) with
     * the column "Code" beeing the driver name. However, not all drivers might be available on
     * your platform. See FileFormat::supportedFormats() to get a list of drivers, which are
     * supported on your platform.
     *
     * Some drivers support additional options, e. g. the
     * [JPEG driver](http://www.gdal.org/frmt_jpeg.html) provides an option for the image quality.
     * These can be optionally set with name-value-pairs. Example:
     * @code
     * img.write("out.jp2", "JPEG2000" {std::pair<std::string,std::string>("FORMAT", "JP2")});
     * @endcode
     * If the driver is not available in your GDAL library this does not work and will throw an
     * exception, see below. It might be that the required driver has not been compiled into the
     * GDAL you are using.
     *
     * @throws file_format_error if the driver is not available in GDAL. Check
     * FileFormat::supportedFormats().
     *
     * -------------
     *
     * @throws runtime_error if the output file cannot be opened or written to. This might also be
     * caused by the output driver, when it does not support writing in general or an Image with a
     * specific Type (like Type::float32).
     */
    void write(std::string const& filename, std::string const& drivername, std::vector<std::pair<std::string,std::string>> const& options = {}, GeoInfo const& gi = {}) const;


    /**
     * @brief Crop an image to the specified Rectangle
     * @param r is the Rectangle to which the image will be cropped.
     *
     * Cropping is a lightweight operation. It does not change the image content. So the image will
     * still require the same amount of memory. Only the information about its size and offset is
     * changed. If you have multiple shared images, cropping one of them will *not* influence the
     * others, since only the image memory is shared and not the size or offset.
     *
     * Hence, cropping can also be used to limit operations to a certain rectangle:
     *  * Make a sharedCopy (cheap),
     *  * crop that copy (cheap) and
     *  * do an operation on it.
     *
     * For an operation, which changes an image, the original uncropped image has been changed in
     * the rectangle area. Since the crop is revertible, you could also avoid the shared copy and
     * do afterwards an uncrop() instead. For an operation, which creates a new Image, the new
     * Image is of the size of the crop window.
     *
     * Cropping can also be nested. The rectangle's offset is always relative to the current view.
     * So if you have
     * @code
     * Image orig{6, 4, Type::uint16x2};
     * Image crop   = orig.sharedCopy().crop(Rectangle{1,1,5,3});
     * Image nested = crop.sharedCopy().crop(Rectangle{1,0,2,2});
     * @endcode
     * this gives a situation like visualized in the following figure:
     * @image html nested_cropping.png
     * Note, the offset for the second crop is x = 1, y = 0 relative to the
     * Image `crop`. To get `nested` directly from `orig`, you could write
     * @code
     * Image nested = orig.sharedCopy().crop(Rectangle{2,1,2,2});
     * @endcode
     * Alternatively, if you want the shared copy anyway, you can also use
     * @code
     * Image nested = orig.sharedCopy(Rectangle{2,1,2,2});
     * @endcode
     * Also note that the shared copies are just there to have variable names in the image. To crop
     * the original image with two crops, one could do
     * @code
     * Image orig{6, 4, Type::uint16x2};
     * orig.crop(Rectangle{1,1,5,3}).crop(Rectangle{1,0,2,2});
     * @endcode
     *
     * @throws size_error if the crop results in a zero-sized image.
     *
     * @see clone(Coordinate const& topleft, Size size) const for sub-pixel accuracy %crop. See
     * also sharedCopy(Rectangle) const
     */
    void crop(Rectangle r);


    /**
     * @brief Uncrop a cropped image
     *
     * This will bring a cropped imaged to its original full size and remove the offset, also in
     * case of nested crops. width(), height() and size() will return the values of the original
     * size again after uncropping.
     *
     * This operation is as cheap as crop.
     */
    void uncrop();


    /**
     * @brief Adjusts the rectangle borders of a cropped image
     * @param extendTop is the amount of pixels to extend the top.
     * @param extendBottom is the amount of pixels to extend the bottom.
     * @param extendLeft is the amount of pixels to extend the left border.
     * @param extendRight is the amount of pixels to extend the right border.
     *
     * All parameters can also have negative values to contract a border.
     *
     * Since the crop operation only changes the size and offset information and not the image
     * content, the rectangle can be changed easily. Look at the following example:
     * @code
     * Image orig{6, 4, Type::uint16x2};
     * Image crop = orig.sharedCopy().crop(Rectangle{1,1,5,3});
     * Image adj  = crop.sharedCopy().adjustCropBorders(1, -1, -1, -2);
     * @endcode
     * which is visualized here:
     * @image html adjust_cropping.png
     *
     * Note, a border is always limited by the original border of the image and will not pass that.
     * The size will be automatically changed in such a case.
     *
     * @throws size_error if the adjusting results in a zero-sized image.
     */
    void adjustCropBorders(int extendTop, int extendBottom, int extendLeft, int extendRight);


    /**
     * @brief Move the borders of a cropped image
     * @param right is the amount of pixels to the right (negative for left).
     * @param down is the amount of pixels down (negative for up).
     *
     * This uses \link adjustCropBorders adjustCropBorders(-down,down,-right,right)\endlink to move
     * the crop window.
     *
     * Note, like in #adjustCropBorders the crop window is limited by the original borders of the
     * image. So trying to move it outside will make the window smaller. Moving is back will also
     * not make it larger again.
     *
     * @throws size_error if the moving results in a zero-sized image.
     */
    void moveCropWindow(int right, int down);


    /**
     * @brief Get the current crop window.
     *
     * Gives the rectangle of the currently cropped region. In case there is no crop, it returns a
     * rectangle with x = y = 0 and width and height of the image.
     *
     * In case of nested crops it gives the crop window relative to the original image. So in the
     * situation
     * @code
     * Image orig{6, 4, Type::uint16x2};
     * Image crop   = orig.sharedCopy().crop(Rectangle{1,1,5,3});
     * Image nested = crop.sharedCopy().crop(Rectangle{1,0,2,2});
     * @endcode
     * which can be visualized like in the following figure
     * @image html nested_cropping.png
     * `nested.getCropWindow()` returns Rectangle{2,1,2,2}.
     *
     * @return the rectangle with which you would have to crop the original image to get the
     * current view on it.
     */
    Rectangle getCropWindow() const;


    /**
     * @brief Get the original size of the image
     *
     * While @ref size just gives the current possibly cropped size, this gives the original size
     * before cropping.
     *
     * @return original size, also in case of nested crops
     */
    Size getOriginalSize() const;


    /**
     * @brief Make a shared copy of an image
     *
     * A shared copy is a flat copy of an image. So the memory which holds the pixel values is
     * shared between the original and the copy. However, other values, like size or offset, are
     * independent. So a shared copy can be cropped without influencing the original image.
     * Changing a pixel value in the (maybe cropped) original image will also change the
     * corresponding pixel value in the shared copy. For a non-const shared copy, modifying a pixel
     * value in the shared copy will also change the corresponding value in the original image.
     * Making a shared copy from a const image will only return a ConstImage.
     *
     * Making a shared copy is a very lightweight operation.
     *
     * @return the shared copy.
     *
     * @see clone()
     */
    ConstImage sharedCopy() const;


    /**
     * @brief Make a read-only shared copy
     *
     * This is basically the same as sharedCopy(), but it always returns a `ConstImage`, even in
     * case when calling with a non-const `Image`.
     *
     * @return constant shared copy
     *
     * @see sharedCopy()
     */
    ConstImage constSharedCopy() const;


    /**
     * @brief Get a cropped shared copy of an image
     *
     * @param r is the crop window (also region of interest).
     *
     * This gives a sharedCopy that is already cropped at the same time.
     *
     * Making a cropped shared copy is a very lightweight operation.
     *
     * @return the cropped shared copy.
     *
     * @see sharedCopy() const, crop(Rectangle)
     */
    ConstImage sharedCopy(Rectangle r) const;


    /**
     * @brief Make a read-only shared copy
     *
     * This is basically the same as sharedCopy(), but it always returns a `ConstImage`, even in
     * case when calling with a non-const `Image`.
     *
     * @return constant shared copy
     *
     * @see sharedCopy(Rectangle const& r)
     */
    ConstImage constSharedCopy(Rectangle const& r) const;


    /**
     * @brief Make a clone of an image
     *
     * A clone is a deep copy of an image. So new memory will be reserved and the pixel values of
     * the original copied. Thus these images are completely independent of each other. Changing a
     * pixel value in one of the images will not influence the other image. The same holds for
     * cropping.
     *
     * Cloning a cropped image will give a cropped clone, which can also be uncropped.
     *
     * @return the cloned image.
     *
     * @see sharedCopy(), Image(Image const& i), operator=(Image i)
     */
    Image clone() const;


    /**
     * @brief Make a cropped clone of an image
     *
     * @param r is the crop window (also region of interest).
     *
     * This will clone the image, but only the given region of interest `r`. So the new image
     * requires less memory and cannot be uncropped.
     *
     * Note the difference between `small` and `full` in the following code:
     * @code
     * Image img("image.tiff");
     * Image small = img.clone(Rectangle{5, 5, 100, 100})
     *
     * img.crop(Rectangle{5, 5, 100, 100});
     * Image full = img.clone(); // or just Image full = img;
     * @endcode
     * The image `full` is a full size clone of `img`, which is just cropped in a lightweight
     * manner (kind of coordinate transformation). Using `full.uncrop()` gives access to the
     * original image again. This is not the case for `small` for which the original size is (100,
     * 100).
     *
     * @return the cloned image.
     *
     * @see clone(Coordinate const& topleft, Size size) const for sub-pixel accuracy crop.
     */
    Image clone(Rectangle const& r) const;


    /**
     * @brief Clone a cropped region with sub-pixel accuracy
     *
     * @param topleft is the top left corner of the crop window. This may be a floating point
     * coordinate.
     *
     * @param size of the cropped region.
     *
     * This will clone the image, but only the given region. The region is not required to lay on
     * pixel boundaries, but can instead be inbetween. The values are bilinearly interpolated:
     *
     * The pixel of the new image \f$ J \f$ at position \f$ (x, y) \f$ with a specified top left
     * corner at \f$ (x_0 + \Delta_x, y_0 + \Delta_y) \f$ with \f$ x,y,x_0,y_0 \in \mathbb N_0 \f$
     * and \f$ \Delta_x, \Delta_y \in [0,1) \f$ is calculated from the original image \f$ I \f$ as
     * \f[ J(x,y)
     *     := I(x_0, y_0)         \, (1 - \Delta_x) \, (1 - \Delta_y)
     *      + I(x_0 + 1, y_0)     \,      \Delta_x  \, (1 - \Delta_y)
     *      + I(x_0, y_0 + 1)     \, (1 - \Delta_x) \,      \Delta_y
     *      + I(x_0 + 1, y_0 + 1) \,      \Delta_x  \,      \Delta_y
     * \f]
     *
     * @return the cloned image with bilinear interpolated subpixels.
     */
    Image clone(Coordinate const& topleft, Size size) const;


    /**
     * @brief Warp an image from one reference system to another
     *
     * @param from is the source reference system, including nodata value. This GeoInfo object
     * refers to the invoking Image object.
     *
     * @param to is the target reference system, including nodata value. This will be the GeoInfo
     * of the resulting image and determines also the @ref GeoInfo::geotrans "resolution" and @ref
     * GeoInfo::size "size". If @ref GeoInfo::size is `{0, 0}`, the size will be chosen
     * automatically by the offset corner of `to` (see @ref GeoTransform::offsetX and @ref
     * GeoTransform::offsetY) and the largest destination image coordinate transformed from the
     * source image corners. The offset can be adjusted by using the translate methods on
     * `to.geotrans` from GeoTransform.
     *
     * @param method specifies the interpolation method used for warping.
     *
     * @return the warped image.
     *
     * @see GeoTransform::translateImage(), GeoTransform::translateProjection()
     */
    Image warp(GeoInfo const& from, GeoInfo const& to, InterpMethod method = InterpMethod::bilinear) const;


    /**
     * @brief Split a multi-channel image into single-channel images
     *
     * @param channels specifies the channels that are extracted. For example giving a `{2, 0}`
     * will extract channel 2 and channel 0 from the image and make two corresponding
     * single-channel images. An empty vector means all channels (default).
     *
     * This generates for each specified channel one single-channel image. The pixel values are
     * copied for that, so it is a rather expensive operation.
     *
     * @return vector of single-channel images
     * @see merge(std::vector<Image> const& images)
     */
    std::vector<Image> split(std::vector<unsigned int> channels = {}) const;


    /**
     * @brief Absolute difference
     * @param B is the second parameter (first is the calling object)
     *
     * This performs the operation \f$ | A - B | \f$, which does not change the type. Move
     * semantics are supported to reuse memory.
     *
     * @return resulting image with the same type
     */
    Image absdiff(Image&& B) const&;
    /// \copydoc absdiff()
    Image absdiff(ConstImage const& B) const&;



    /**
     * @brief Absolute value
     *
     * This gives the absolut value of the calling Image \f$ | A | \f$. In case of integer base
     * type images the operation is done in int32 and saturates afterwards to the original type.
     * Saturation will only have an effect if a pixel has the minimum value before taking the abs,
     * e. g. |-128| will give 127 for an int8 base type. Move semantics are supported to reuse
     * memory.
     *
     * @return resulting image with the same type
     */
    Image abs() const&;


    /**
     * @brief Add an image pixelwise
     * @param B is the image to add to the calling Image A.
     *
     * This performs the addition \f$ A + B \f$. In case of integer base type images the operation
     * is done in int32 and saturates afterwards to the original type. Move semantics are supported
     * to reuse memory.
     *
     * @return resulting image with the same type.
     */
    Image add(Image&& B) const&;
    /// \copydoc add()
    Image add(ConstImage const& B) const&;


    /**
     * @brief Add an image pixelwise
     * @param B is the image to add to the calling Image A.
     *
     * @param resultType is the full type of result image, like Type::uint16x3 when two
     * Type::uint8x3 are added. If the result type should be the same as the operands, use the
     * other add-methods, which support move semantics.
     *
     * This performs the addition \f$ A + B \f$. In case of integer base type images the operation
     * is done in int32 and saturates to the result type afterwards. Note, A and B may have
     * different base types.
     *
     * This method does not profit from move semantics, since changing the data type for the
     * resulting image requires new memory.
     *
     * @return resulting image with the specified type.
     */
    Image add(ConstImage const& B, Type resultType) const;


    /**
     * @brief Subtract an image pixelwise
     * @param B is the image to subtract from the calling Image A.
     *
     * This performs the subtraction \f$ A - B \f$. In case of integer base type images the
     * operation is done in int32 and saturates afterwards to the original type. Move semantics are
     * supported to reuse memory.
     *
     * @return resulting image with the same type.
     */
    Image subtract(Image&& B) const&;
    /// \copydoc subtract()
    Image subtract(ConstImage const& B) const&;


    /**
     * @brief Subtract an image pixelwise
     * @param B is the image to subtract from the calling Image A.
     *
     * @param resultType is the full type of result image, like Type::int16x3 when two
     * Type::uint8x3 are used. If the result type should be the same as the operands, use the other
     * subtract-methods, which support move semantics.
     *
     * This performs the subtraction \f$ A - B \f$. In case of integer base type images the
     * operation is done in int32 and saturates to the result type afterwards. Note, A and B may
     * have different base types.
     *
     * This method does not profit from move semantics, since changing the data type for the
     * resulting image requires new memory.
     *
     * @return resulting image with the specified type.
     */
    Image subtract(ConstImage const& B, Type resultType) const;


    /**
     * @brief Multiply an image pixelwise
     * @param B is the image to multiply with the calling Image A.
     *
     * This performs the elementwise multiplication \f$ A \cdot B \f$. In case of integer base type
     * images the operation is done in int32 and saturates afterwards to the original type. Move
     * semantics are supported to reuse memory.
     *
     * @return resulting image with the same type.
     */
    Image multiply(Image&& B) const&;
    /// \copydoc multiply()
    Image multiply(ConstImage const& B) const&;


    /**
     * @brief Multiply an image elementwise
     * @param B is the image to multiply with the calling Image A.
     *
     * @param resultType is the full type of result image, like Type::uint16x3 when two
     * Type::uint8x3 are used. If the result type should be the same as the operands, use the other
     * multiply-methods, which support move semantics.
     *
     * This performs the elementwise multiplication \f$ A \cdot B \f$. In case of integer base type
     * images the operation is done in int32 and saturates to the result type afterwards.
     *
     * This method does not profit from move semantics, since changing the data type for the
     * resulting image requires new memory.
     *
     * @return resulting image with the specified type.
     */
    Image multiply(ConstImage const& B, Type resultType) const;


    /**
     * @brief Divide an image elementwise
     * @param B is the image representing the divisor, with the calling Image A beeing the dividend.
     *
     * This performs the elementwise division \f$ \dfrac A B \f$, but with special arithmetics:
     *   * dividing by 0 gives 0, i. e. \f$ \frac x 0 := 0 \f$
     *   * otherwise floating point arithmetic is used (also for integer operands) and for integer
     *     result types this is followed by a round with tie-break to even. Examples:
     *       * for integer or floating points operands and floating point result: \f$ \frac 3 2 = 1.5 \f$ and  \f$ \frac 5 2 = 2.5 \f$
     *       * for integer or floating points operands and integer result: \f$ \frac 3 2 = 2 = \frac 5 2 \f$
     *
     * Move semantics are supported to reuse memory.
     *
     * @return resulting image with the same type.
     */
    Image divide(Image&& B) const&;
    /// \copydoc divide()
    Image divide(ConstImage const& B) const&;


    /**
     * @brief Divide an image elementwise
     * @param B is the image representing the divisor, with the calling Image A beeing the dividend.
     *
     * @param resultType is the full type of result image, like Type::float32x3 when two
     * Type::uint8x3 are used. If the result type should be the same as the operands, use the other
     * divide-methods, which support move semantics.
     *
     * This performs the elementwise division \f$ \dfrac A B \f$, but with special arithmetics:
     *   * dividing by 0 gives 0, i. e. \f$ \frac x 0 := 0 \f$
     *   * otherwise floating point arithmetic is used (also for integer
     *     operands) and for integer result types this is followed by a round
     *     with tie-break to even. Examples:
     *       * for integer or floating points operands and floating point result: \f$ \frac 3 2 = 1.5 \f$ and  \f$ \frac 5 2 = 2.5 \f$
     *       * for integer or floating points operands and integer result: \f$ \frac 3 2 = 2 = \frac 5 2 \f$
     *
     * This method does not profit from move semantics, since changing the data type for the
     * resulting image requires new memory.
     *
     * @return resulting image with the specified type.
     */
    Image divide(ConstImage const& B, Type resultType) const;


    /**
     * @brief Perform elementwise bitwise 'and'-operation
     * @param B is the second operand, first is the calling Image A.
     *
     * This performs `A & B` (in C++-Syntax). For floating-point types the bit representation is
     * used. It is intended to be used with masks, i. e. Image%s of base type `uint8`, which only
     * contain 0's or 255's.
     *
     * Move semantics are supported to reuse memory.
     *
     * @return resulting image
     */
    Image bitwise_and(Image&& B) const&;
    /// \copydoc bitwise_and()
    Image bitwise_and(ConstImage const& B) const&;


    /**
     * @brief Perform elementwise bitwise 'or'-operation
     * @param B is the second operand, first is the calling Image A.
     *
     * This performs `A | B` (in C++-Syntax). For floating-point types the bit representation is
     * used. It is intended to be used with masks, i. e. Image%s of base type `uint8`, which only
     * contain 0's or 255's.
     *
     * Move semantics are supported to reuse memory.
     *
     * @return resulting image
     */
    Image bitwise_or(Image&& B) const&;
    /// \copydoc bitwise_or()
    Image bitwise_or(ConstImage const& B) const&;


    /**
     * @brief Perform elementwise bitwise 'xor'-operation
     * @param B is the second operand, first is the calling Image A.
     *
     * This performs `A ^ B` (in C++-Syntax). For floating-point types the bit representation is
     * used. It is intended to be used with masks, i. e. Image%s of base type `uint8`, which only
     * contain 0's or 255's.
     *
     * Move semantics are supported to reuse memory.
     *
     * @return resulting image
     */
    Image bitwise_xor(Image&& B) const&;
    /// \copydoc bitwise_xor()
    Image bitwise_xor(ConstImage const& B) const&;


    /**
     * @brief Perform elementwise bitwise 'not'-operation
     *
     * This performs `~A` (in C++-Syntax). For floating-point types the bit representation is used.
     * It is intended to be used with masks, i. e. Image%s of base type `uint8`, which only contain
     * 0's or 255's.
     *
     * Move semantics are supported to reuse memory.
     *
     * @return resulting image
     */
    Image bitwise_not() const&;


    /**
     * @brief Min and max values of the image with their locations
     *
     * @param mask can be single or multi channel to specify the valid locations.
     *
     * @returns a vector of pairs, where the first elements contain the min values with locations
     * and the second contain the max values with locations for all channels. So the size of the
     * vector is the number of channels. If the mask shows no valid location 0 will be returned as
     * min and max value both with location (-1, -1).
     */
    std::vector<std::pair<ValueWithLocation, ValueWithLocation>> minMaxLocations(ConstImage const& mask = {}) const;


    /**
     * @brief Mean value of the image
     *
     * @param mask can be single or multi channel to specify the valid locations.
     *
     * @returns the mean values for all channels. If the mask shows no valid location 0 will be
     * returned as mean value.
     */
    std::vector<double> mean(ConstImage const& mask = {}) const;


    /**
     * @brief Mean value and standard deviation of the image
     *
     * @param mask can be single or multi channel to specify the valid locations.
     *
     * @param sampleCorrection specifies whether the standard deviation should be done from the
     * population (`sampleCorrection == false`), which is the default OpenCV behaviour or estimated
     * with the sample corrections (`sampleCorrection == false`). See equations below.
     *
     * Note, the standard deviation is by default the population's standard deviation (like in
     * OpenCV), i. e.
     * \f[ s_{N} = \sqrt{ \frac 1 N \sum_{i=1}^{N} (x_i - \overline x)^2 }. \f]
     * By setting `sampleCorrection` to true the correction term is used:
     * \f[ s = \sqrt{ \frac{1}{N - 1} \sum_{i=1}^{N} (x_i - \overline x)^2 }. \f]
     *
     * @returns the mean value \f$ \overline x \f$ for all channels in the first vector and the
     * standard deviation \f$ s_N \f$ or \f$ s \f$ for all channels in the second vector. If the
     * mask shows no valid location 0 will be returned as mean value and standard deviation.
     */
    std::pair<std::vector<double>, std::vector<double>> meanStdDev(ConstImage const& mask = {}, bool sampleCorrection = false) const;


    /**
     * @brief Get a single-channel mask from value range(s) of valid values
     *
     * @param channelRanges is either a single range that will be used for all channels or one
     * range per channel. A range is just an #Interval, like [1, 100].
     *
     * @param useAnd determines whether to merge multiple masks with a bitwise *and* (logical
     * conjunction) or with a bitwise *or* (logical disjunction). This only applies to
     * multi-channel images.
     *
     * This method can only create a mask from contiguous intervals. If you require more
     * complicated sets with a union of intervals (#IntervalSet) use
     * createSingleChannelMaskFromSet().
     *
     *   * For a single channel image and a single range \f$ [l,u] \f$ each mask value \f$ m \f$
     *     will be determined by
     *     \f[ m = \begin{cases} 255 & \text{if } l \le p \le u\\
     *                           0   & \text{otherwise}
     *             \end{cases} \f]
     *     for a pixel value \f$ p \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and a single range \f$ [l,u] \f$ each
     *     mask value \f$ m \f$ will be determined by
     *     \f[ m = \begin{cases} \bigwedge\limits_{i=0}^{c-1} m_i & \text{if \texttt{useAnd}}\\
     *                           \bigvee\limits_{i=0}^{c-1} m_i & \text{if not \texttt{useAnd}}
     *               \end{cases}, \f]
     *     where
     *     \f[ m_i = \begin{cases} 255 & \text{if } l \le p_i \le u\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and \f$ c \f$ ranges \f$ [l_i,u_i] \f$
     *     each mask value \f$ m \f$ will be determined by
     *     \f[ m = \begin{cases} \bigwedge\limits_{i=0}^{c-1} m_i & \text{if \texttt{useAnd}}\\
     *                           \bigvee\limits_{i=0}^{c-1} m_i & \text{if not \texttt{useAnd}}
     *               \end{cases}, \f]
     *     where
     *     \f[ m_i = \begin{cases} 255 & \text{if } l_i \le p_i \le u_i\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     * To put it in words: if there are multiple ranges for a multi-channel image, they are applied
     * channel-wise and the final resulting mask is a logical conjunction or disjunction (depending
     * on `useAnd`) of the channel masks. A conjunction is appropriate only to specify ranges of
     * valid values, i. e. the ranges include values that should *not* be masked out. Then the
     * resulting mask is 255 where all channels have valid values. A disjunction is appropriate to
     * specify ranges of invalid values, e. g. values to be interpolated. The resulting mask is 255
     * where any channel has a 255.
     *
     * Example for the second case (multi-channel image, single range):
     * @image html gray_values_images.png
     * will give with
     * @code
     * auto mask = img.createSingleChannelMaskFromRange({Interval::closed(25, 175)});
     * @endcode
     * @image html gray_values_single_mask_from_range_conjunction.png
     * and with
     * @code
     * auto mask = img.createSingleChannelMaskFromRange({Interval::closed(25, 175)}, false);
     * @endcode
     * @image html gray_values_single_mask_from_range_disjunction.png
     * See createMultiChannelMaskFromRange() for a visualization of the intermediate result.
     *
     * @return single-channel mask Image with Type::uint8x1
     *
     * @throws image_type_error if the number of ranges is invalid, i. e. it is neither 1 nor
     * matches the number of channels of the image.
     *
     * @note You can use open or half open intervals. For integer images the intervals (10, 15),
     * (10.5, 14.5) and [11, 14] specify all the same values. For floating point images the round
     * brackets are currently just ignored and replaced by square brackets. So for example
     * (0.1, 0.15) would currently be [0.1, 0.15] for floating point images. This behavior might
     * change in future.
     *
     * @see createMultiChannelMaskFromRange(std::vector<Interval> const& channelRanges) const
     * createSingleChannelMaskFromSet(std::vector<IntervalSet> const& channelSets) const
     */
    Image createSingleChannelMaskFromRange(std::vector<Interval> const& channelRanges, bool useAnd = true) const;


    /**
     * @brief Get a single-channel mask from value set(s) of valid values
     *
     * @param channelSets is either a single set that will be used for all channels or one set per
     * channel. A set (#IntervalSet) S is a union of Interval%s, like
     * S = [1, 100] ∪ [200, 250] ∪ (300, 305).
     *
     * @param useAnd determines whether to merge multiple masks with a bitwise *and* (logical
     * conjunction) or with a bitwise *or* (logical disjunction). This only applies to
     * multi-channel images.
     *
     * This method is a generalization of createSingleChannelMaskFromRange(). So, if your set
     * consists of one contiguous interval, you can use createSingleChannelMaskFromRange().
     * However, it might still be convenient to use an #IntervalSet, since it is easy to make a
     * union of potentially overlapping #Interval%s.
     *
     *   * For a single channel image and a single set \f$ S \f$ each mask value \f$ m \f$ will be
     *     determined by
     *     \f[ m = \begin{cases} 255 & \text{if } p \in S\\
     *                           0   & \text{otherwise}
     *             \end{cases} \f]
     *     for a pixel value \f$ p \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and a single set \f$ S \f$ each mask
     *     value \f$ m \f$ will be determined by
     *     \f[ m = \begin{cases} \bigwedge\limits_{i=0}^{c-1} m_i & \text{if \texttt{useAnd}}\\
     *                           \bigvee\limits_{i=0}^{c-1} m_i & \text{if not \texttt{useAnd}}
     *               \end{cases}, \f]
     *     where
     *     \f[ m_i = \begin{cases} 255 & \text{if } p_i \in S\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and \f$ c \f$
     *     sets \f$ S_i \f$ each mask value \f$ m \f$ will be
     *     determined by
     *     \f[ m = \begin{cases} \bigwedge\limits_{i=0}^{c-1} m_i & \text{if \texttt{useAnd}}\\
     *                           \bigvee\limits_{i=0}^{c-1} m_i & \text{if not \texttt{useAnd}}
     *               \end{cases}, \f]
     *     where
     *     \f[ m_i = \begin{cases} 255 & \text{if } p_i \in S_i\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     * To put it in words: if there are multiple sets for a multi-channel image, they are applied
     * channel-wise and the final resulting mask is a logical conjunction or disjunction (depending
     * on `useAnd`) of the channel masks. A conjunction is appropriate only to specify sets of
     * valid values, i. e. the sets include values that should *not* be masked out. Then the
     * resulting mask is 255 where all channels have valid values. A disjunction is appropriate to
     * specify sets of invalid values, e. g. values to be interpolated. The resulting mask is 255
     * where any channel has a 255.
     *
     * Example for the second case (multi-channel image, single set):
     * @image html gray_values_images.png
     * will give with
     * @code
     * IntervalSet set; // [25, 50] ∪ [175, 200]
     * set += Interval::closed(25, 50);
     * set += Interval::closed(175, 200);
     * Image mask = img.createSingleChannelMaskFromSet({set});
     * @endcode
     * @image html gray_values_single_mask_from_set_conjunction.png
     * and with
     * @code
     * IntervalSet set; // [25, 50] ∪ [175, 200]
     * set += Interval::closed(25, 50);
     * set += Interval::closed(175, 200);
     * Image mask = img.createSingleChannelMaskFromSet({set}, false);
     * @endcode
     * @image html gray_values_single_mask_from_set_disjunction.png
     * See createMultiChannelMaskFromSet() for a visualization of the intermediate result.
     *
     * @return single-channel mask Image with Type::uint8x1
     *
     * @throws image_type_error if the number of sets is invalid, i. e. it is neither 1 nor matches
     * the number of channels of the image.
     *
     * @note You can use open or half open intervals. For integer images the intervals (10, 15),
     * (10.5, 14.5) and [11, 14] specify all the same values. For floating point images the round
     * brackets are currently just ignored and replaced by square brackets. So for example
     * (0.1, 0.15) would be [0.1, 0.15] currently for floating point images. This behaviour might
     * change in future.
     *
     * @see createMultiChannelMaskFromSet(std::vector<IntervalSet> const& channelSets) const
     */
    Image createSingleChannelMaskFromSet(std::vector<IntervalSet> const& channelSets, bool useAnd = true) const;


    /**
     * @brief Get a multi-channel mask from value range(s)
     *
     * @param channelRanges is either a single range that will be used for all channels or one
     * range per channel. A range is just an #Interval, like \f$ [1, 100] \f$.
     *
     * This method can only create a mask from contiguous intervals. If you require more
     * complicated sets with a union of intervals (#IntervalSet) use
     * createMultiChannelMaskFromSet().
     *
     *   * For a single channel image and a single range \f$ [l,u] \f$ each mask value \f$ m \f$
     *     will be determined by
     *     \f[ m = \begin{cases} 255 & \text{if } l \le p \le u\\
     *                           0   & \text{otherwise}
     *             \end{cases} \f]
     *     for a pixel value \f$ p \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and a single range \f$ [l,u] \f$ each
     *     mask value \f$ m_i \f$ in channel \f$ i \f$ will be determined by
     *     \f[ m_i = \begin{cases} 255 & \text{if } l \le p_i \le u\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and \f$ c \f$ ranges \f$ [l_i,u_i] \f$
     *     each mask value \f$ m_i \f$ in channel \f$ i \f$ will be determined by
     *     \f[ m_i = \begin{cases} 255 & \text{if } l_i \le p_i \le u_i\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     * To put it in words: if there are multiple ranges for a multi-channel image, each range is
     * applied to its corresponding channel to yield the corresponding channel in the mask.
     *
     * Example for the second case (multi-channel image, single range):
     * @image html gray_values_images.png
     * will give with
     * @code
     * Image mask = img.createMultiChannelMaskFromRange({Interval::closed(25, 175)});
     * @endcode
     * @image html gray_values_mask_channels_from_range.png
     *
     * @return multi-channel mask Image with Type::uint8 base type and the same number of channels
     * as the image.
     *
     * @throws image_type_error if the number of ranges is invalid, i. e. it is neither 1 nor
     * matches the number of channels of the image.
     *
     * @note You can use open or half open intervals. For integer images the intervals (10, 15),
     * (10.5, 14.5) and [11, 14] specify all the same values. For floating point images the round
     * brackets are currently just ignored and replaced by square brackets. So for example
     * (0.1, 0.15) would be [0.1, 0.15] currently for floating point images. This behaviour might
     * change in future.
     *
     * @see createSingleChannelMaskFromRange(std::vector<Interval> const& channelRanges) const
     */
    Image createMultiChannelMaskFromRange(std::vector<Interval> const& channelRanges) const;

    /**
     * @brief Get a multi-channel mask from value set(s)
     *
     * @param channelSets is either a single set that will be used for all channels or one set per
     * channel. A set (#IntervalSet) S is a union of Interval%s, like S = [1, 100] ∪ [200, 250] ∪
     * (300, 305).
     *
     * This method is a generalization of createMultiChannelMaskFromRange(). So, if your set
     * consists of one contiguous interval, you can use createMultiChannelMaskFromRange(). However,
     * it might still be convenient to use an #IntervalSet, since it is easy to make a union of
     * potentially overlapping #Interval%s.
     *
     *   * For a single channel image and a single set \f$ S \f$ each mask value \f$ m \f$ will be
     *     determined by
     *     \f[ m = \begin{cases} 255 & \text{if } p \in S\\
     *                           0   & \text{otherwise}
     *             \end{cases} \f]
     *     for a pixel value \f$ p \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and a single set \f$ S \f$ each mask
     *     value \f$ m_i \f$ in channel \f$ i \f$ will be determined by
     *     \f[ m_i = \begin{cases} 255 & \text{if } p_i \in S\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     *   * For a multi channel image with \f$ c \f$ channels and \f$ c \f$ sets \f$ S_i \f$ each
     *     mask value \f$ m_i \f$ in channel \f$ i \f$ will be determined by
     *     \f[ m_i = \begin{cases} 255 & \text{if } p_i \in S_i\\
     *                             0   & \text{otherwise}
     *               \end{cases} \f]
     *     for a pixel value \f$ p_i \f$ in channel \f$ i \f$.
     *
     * To put it in words: if there are multiple sets for a multi-channel image, each set is
     * applied to its corresponding channel to yield the corresponding channel in the mask.
     *
     * Example for the second case (multi-channel image, single set):
     * @image html gray_values_images.png
     * will give with
     * @code
     * IntervalSet set; // [25, 50] ∪ [175, 200]
     * set += Interval::closed(25, 50);
     * set += Interval::closed(175, 200);
     * Image mask = img.createMultiChannelMaskFromSet({set});
     * @endcode
     * @image html gray_values_mask_channels_from_set.png
     *
     * @return multi-channel mask Image with Type::uint8 base type and the same number of channels
     * as the image.
     *
     * @throws image_type_error if the number of sets is invalid, i. e. it is neither 1 nor matches
     * the number of channels of the image.
     *
     * @note You can use open or half open intervals. For integer images the intervals (10, 15),
     * (10.5, 14.5) and [11, 14] specify all the same values. For floating point images the round
     * brackets are currently just ignored and replaced by square brackets. So for example
     * (0.1, 0.15) would be [0.1, 0.15] currently for floating point images. This behaviour might
     * change in future.
     *
     * @see createSingleChannelMaskFromSet(std::vector<IntervalSet> const& channelSets) const
     */
    Image createMultiChannelMaskFromSet(std::vector<IntervalSet> const& channelSets) const;

    /**
     * @brief Current size
     *
     * If an Image is cropped to a 2-by-2 rectangle, size will return exactly that. To get the
     * original size of an Image, use getOriginalSize()!
     *
     * @return size (width and height) of the image
     */
    Size size() const;


    /**
     * @brief Current height
     *
     * If an Image is cropped to a 2-by-2 rectangle, height will return 2. To get the original
     * height of an Image, use getOriginalSize()!
     *
     * @return height of the image
     */
    int height() const;


    /**
     * @brief Current width
     *
     * If an Image is cropped to a 2-by-2 rectangle, width will return 2. To get the original width
     * of an Image, use getOriginalSize()!
     *
     * @return width of the image
     */
    int width() const;


    /**
     * @brief Number of channels
     * @return number of channels, e. g. for a Type::uint8x3 Image this returns a 3.
     */
    unsigned int channels() const;


    /**
     * @brief Full type of the Image
     * @return full type, e. g. for a Type::int16x2 Image this returns Type::int16x2.
     */
    Type type() const;


    /**
     * @brief Base type of the Image
     * @return base type, e. g. for a Type::int16x2 Image this returns Type::int16.
     */
    Type basetype() const;


    /**
     * @brief Check if image is empty (default constructed)
     * @return true if it is empty, false if not.
     */
    bool empty() const;


    /**
     * @brief Check if this can be used as mask for another image
     * @param im is the image on which the mask could be used
     *
     * To make the invoking object (denoted by `m`) usable as a mask, the following criteria have
     * to be met:
     *  * the size of `m` must be equal to the size of `im`
     *  * `m` must be of base type Type::uint8
     *  * `m` must either have one channel or the same number of channels as `im`
     *
     * @return true if it fits, false if not.
     */
    bool isMaskFor(ConstImage const& im) const;


    /**
     * @brief Check if this Image is shared with another Image
     * @param img is the to check with the calling Image
     * @return `true` if `img` is a shared copy (even if they are cropped), `false` otherwise.
     * @see sharedCopy()
     */
    bool isSharedWith(ConstImage const& img) const;


    /**
     * @brief Get channel iterator on the first element
     *
     * @tparam T - plain data type (not an array type), e. g. for an Image with full type
     * `Type::uint16x3` the type `T` must be `uint16_t`.
     *
     * @param channel on which this iterator acts.
     *
     * This iterator iterates through all pixel values of the specified channel.
     *
     * Code example to set all values in the first channel of an Image `img` to the value 1337:
     * @code
     * Image img{5, 6, Type::uint16x3};
     * for (auto it = img.begin<uint16_t>(0), it_end = img.end<uint16_t>(0); it != it_end; ++it)
     *     *it = 1337;
     * @endcode
     * This works also for cropped images, since the iterator regards the size and offset. However,
     * note that the @ref ConstChannelValueIterator<T> has only read access, since it comes from a
     * const Image. So, for @ref ConstChannelValueIterator<T> the dereference operator returns a `T
     * const&` and for \link ChannelValueIterator ChannelValueIterator<T>\endlink it returns `T&`,
     * which is `uint16_t` in the example.
     *
     * Note, for masks, which always have the base type `Type::uint8`, also use `uint8_t` for `T`.
     * Never use `bool` for `T`! This would result in undefined behaviour, since boolean values
     * could occur, which are neither false nor true.
     *
     * @return iterator on first element
     */
    template<typename T>
    ConstChannelValueIterator<T> begin(unsigned int channel) const;

    /**
     * @brief Get channel iterator past the end
     *
     * @tparam T - plain data type (not an array type), e. g. for an Image with full type
     * `Type::uint16x3` the type `T` must be `uint16_t`.
     *
     * @param channel on which this iterator acts.
     *
     * This iterator iterates through all pixel values of the specified channel.
     *
     * Code example to set all values in the first channel of an Image `img` to the value 1337:
     * @code
     * Image img{5, 6, Type::uint16x3};
     * for (auto it = img.begin<uint16_t>(0), it_end = img.end<uint16_t>(0); it != it_end; ++it)
     *     *it = 1337;
     * @endcode
     * This works also for cropped images, since the iterator regards the size and offset. However,
     * note that the @ref ConstChannelValueIterator<T> has only read access, since it comes from a
     * const Image. So, for @ref ConstChannelValueIterator<T> the dereference operator returns a `T
     * const&` and for \link ChannelValueIterator ChannelValueIterator<T>\endlink it returns `T&`,
     * which is `uint16_t` in the example.
     *
     * Note, for masks, which always have the base type `Type::uint8`, also use `uint8_t` for `T`.
     * Never use `bool` for `T`! This would result in undefined behaviour, since boolean values
     * could occur, which are neither false nor true.
     *
     * @return iterator past the end
     */
    template<typename T>
    ConstChannelValueIterator<T> end(unsigned int channel) const;


    /**
     * @brief Get full pixel iterator on the first element
     *
     * @tparam T - array data type. For example for an Image with full type `Type::uint16x3` the
     * type `T` must be `std::array<uint16_t,3>`.
     *
     * This iterator iterates through an image using full pixels.
     *
     * Code example to swap channels 0 and 2:
     * @code
     * using std::swap;
     * Image img{5, 6, Type::uint16x3};
     * for (auto it = img.begin<std::array<uint16_t,3>>(), it_end = img.end<std::array<uint16_t,3>>(); it != it_end; ++it)
     *     swap((*it)[0], (*it)[2]);
     * @endcode
     * This works also for cropped images, since the iterator regards the size and offset. However,
     * note that the @ref ConstPixelIterator<T> has only read access, since it comes from a const
     * Image. So, for @ref ConstPixelIterator<T> the dereference operator returns a `T const&` and
     * for @ref PixelIterator<T> it returns `T&`, which is `std::array<uint16_t,3>` in the example.
     *
     * Note, for masks, which always have the base type `Type::uint8`, also use an `uint8_t` array
     * for `T`. Never use a `bool` array for `T`! This would result in undefined behaviour, since
     * boolean values could occur, which are neither false nor true.
     *
     * @return iterator on first element
     */
    template<typename T>
    ConstPixelIterator<T> begin() const;

    /**
     * @brief Get full pixel iterator past the end
     *
     * @tparam T - array data type. For example for an Image with full type `Type::uint16x3` the
     * type `T` must be `std::array<uint16_t,3>`.
     *
     * This iterator iterates through an image using full pixels.
     *
     * Code example to swap channels 0 and 2:
     * @code
     * using std::swap;
     * Image img{5, 6, Type::uint16x3};
     * for (auto it = img.begin<std::array<uint16_t,3>>(), it_end = img.end<std::array<uint16_t,3>>(); it != it_end; ++it)
     *     swap((*it)[0], (*it)[2]);
     * @endcode
     * This works also for cropped images, since the iterator regards the size and offset. However,
     * note that the @ref ConstPixelIterator<T> has only read access, since it comes from a const
     * Image. So, for @ref ConstPixelIterator<T> the dereference operator returns a `T const&` and
     * for @ref PixelIterator<T> it returns `T&`, which is `std::array<uint16_t,3>` in the example.
     *
     * Note, for masks, which always have the base type `Type::uint8`, also use an `uint8_t` array
     * for `T`. Never use a `bool` array for `T`! This would result in undefined behaviour, since
     * boolean values could occur, which are neither false nor true.
     *
     * @return iterator past the end
     */
    template<typename T>
    ConstPixelIterator<T> end() const;


    /**
     * @brief Direct access on full pixel
     *
     * @tparam T - array data type. For example for an Image with full type `Type::uint16x3` the
     * type `T` must be `std::array<uint16_t,3>`.
     *
     * @param x coordinate (column).
     * @param y coordinate (row).
     *
     * Directly access a full pixel. E. g.:
     * @code
     * Image img{7, 8, Type::uint16x3};
     * auto& pixel = img.at<std::array<uint16_t,3>>(3, 4);
     * std::cout << pixel[1] << std::endl;
     * @endcode
     *
     * Note, for masks, which always have the base type `Type::uint8`, also use a `uint8_t` array
     * for `T`. Never use a `bool` array for `T`! This would result in undefined behaviour, since
     * boolean values could occur, which are neither false nor true.
     *
     * For single-channel images, the base type can be used directly (without array).
     * Alternatively, use at(unsigned int y, unsigned int x, unsigned int channel) with `channel =
     * 0`.
     *
     * @return reference on specified pixel
     */
    template<typename T>
    T const& at(unsigned int x, unsigned int y) const;


    /**
     * @brief Direct access on channel value
     *
     * @tparam T - plain data type (not an array type), e. g. for an Image with full type
     * `Type::uint16x3` the type `T` must be `uint16_t`.
     *
     * @param x coordinate (column).
     * @param y coordinate (row).
     * @param channel to use.
     *
     * Directly access a channel value. E. g.:
     * @code
     * Image img{7, 8, Type::uint16x3};
     * std::cout << img.at<uint16_t>(3, 4, 1) << std::endl;
     * @endcode
     *
     * Note, for masks, which always have the base type `Type::uint8`, also use `uint8_t` for `T`.
     * Never use `bool` for `T`! This would result in undefined behaviour, since boolean values
     * could occur, which are neither false nor true.
     *
     * @return reference on specified pixel
     */
    template<typename T>
    T const& at(unsigned int x, unsigned int y, unsigned int channel) const;


    /**
     * @brief Assuming this is a mask; get the bool value at the specified coordinates and channel
     * @param x
     * @param y
     * @param channel
     *
     * Only apply this on a mask! This will not check whether the type fits or not (for performance
     * reasons). Make sure, that the base type is Type::uint8.
     *
     * @return true for a uint8_t value != 0 and false otherwise.
     */
    bool boolAt(unsigned int x, unsigned int y, unsigned int channel) const;


    /**
     * @brief Get the value at the specified coordinates as double
     * @param x
     * @param y
     * @param channel
     *
     * Independent on the data type of this image, this will return the value as a double. Every
     * supported image data type fits into a double. However, note that to gather a lot of values
     * this method is not the best choice, since it is then probably better to use the native data
     * type, due to efficiency.
     *
     * @return the image pixel value as double
     */
    double doubleAt(unsigned int x, unsigned int y, unsigned int channel) const;


    /**
     * @brief Convert whole image to a different type
     * @param t is the type to convert the image to.
     *
     * Converting a type to a larger type does not scale the values. So, to maintain the brightness
     * of an uint8 image in a uint16 image, you have to multiply the image manually.
     *
     * @return converted image
     */
    Image convertTo(Type t) const;


    /**
     * @brief Convert the color space
     *
     * @param map is the conversion type that specifies from which space to which space
     *
     * @param result is the result data type. Leave it to Type::invalid to not change the type. But
     * note, that this saturates values that are out of range (only for integer types other than
     * Type::int32). If the transformation possibly does not fit into the type, you can specify an
     * explicit type or use getResultType(), which will select the next larger signed type. For
     * some mappings a floating point type might be more appropriate.
     *
     * @param sourceChannels allows to specify the source channels for a mapping. Consider
     * ColorMapping::Pos_Neg_to_NDI for example. Here the source channels must be in a specific
     * order. If this image has for example 3 channels, like
     *   - channel 0: NIR
     *   - channel 1: SWIR1
     *   - channel 2: Red
     *   .
     * then for NDVI (normalized difference vegetation index) the positive channel is NIR and the
     * negative Red. Therefore the source channels vector should be {0, 2} to specify that near
     * infrared is on channel 0 and red is on channel 2. If you leave this empty it will always
     * assume the image has the channels in the same order as the color mapping suggests, beginning
     * with 0. In the example this would be the same as giving {0, 1} (i. e. pos: NIR, neg: SWIR1),
     * which is wrong for the example image.
     *
     * @return converted image
     *
     * @see ColorMapping
     */
    Image convertColor(ColorMapping map, Type result = Type::invalid, std::vector<unsigned int> sourceChannels = {}) const;


    /**
     * @brief Get the difference in base type elements from on row to the next one
     *
     * Example: For an Image like in the following:
     * @code
     * Image img{8, 7, Type::uint16x3};
     * @endcode
     * the pointer difference between two neighbouring rows would be 24 even when cropped later on.
     *
     * @return pointer difference
     */
    std::ptrdiff_t getPtrDiffRow() const;


    /**
     * @brief Get the difference in base type elements from one column to the next one
     *
     * Basically this is always `channels()`.
     *
     * @return pointer difference
     */
    std::ptrdiff_t getPtrDiffColumn() const;

    /**
     * @brief Swap two images
     * @param i1 is the image 1.
     * @param i2 is the image 2.
     *
     * The underlying `cv::Mat` objects are swapped. Depending on the version this is done with a
     * custom swap function in OpenCV.
     */
    friend void swap(ConstImage& i1, ConstImage& i2) noexcept;
};



/**
 * @brief Handles an image regarding I/O, memory management and processing
 *
 * First of all, note that this class inherits all the methods from ConstImage!
 *
 * An Image represents an image. It can #read and #write images with different formats, the best
 * supported seems to be TIFF. Image does not have any geo or meta information. These are handled
 * by GeoInfo.
 *
 * Note that unlike with OpenCV's `Mat` (which this class is based on), making a copy of an Image
 * object by assignment or construction indeed copies the image contents, i. e. it clones the Image
 * object. These objects are then completely independent. In contrast, a shared copy must be
 * explicitly aquired by using #sharedCopy, see the following example:
 * @code
 * Image img{5, 6, Type::uint16x2};
 * Image shared{img.sharedCopy()};
 * Image clone1{img.clone()};
 * Image clone2{img};
 * @endcode
 * In this example changing any pixel value in `shared`, will also change the value in `img` and
 * vice versa, but nothing will be changed in `clone1` or `clone2`. Then again changing any pixel
 * value in `clone1` or `clone2` will not change any pixel in any of the other images. So `clone1`
 * and `clone2` are completely independent, whereas `img` is shared with `shared`. This can be
 * checked with `img.isSharedWith(shared)`, which is true in this case. However, sharing an image
 * is not the same as making a reference. Only the image contents are shared. This means the pixel
 * values are shared, or more precisely the image memory is the same. This does not affect
 * cropping. See the following example:
 * @code
 * Image img{6, 4, Type::uint16x2};
 * Image shared{img.sharedCopy()};
 * shared.crop(Rectangle{4,2,2,1});
 * @endcode
 * which can be visualized like this:
 * @image html cropping.png
 * So cropping `shared` would not affect `img`, but the memory of both are still shared. Thus
 * changing a pixel in `shared` would still change the corresponding pixel in `img`. As an example,
 * the pixel at x = 5, y = 2 could be accessed by `img.at<uint16_t>(5,2,0)` or by
 * `shared.at<uint16_t>(1,0,0)`. Note, Image uses a different order for `width` and `height` and
 * `x` and `y` than `cv::Mat`. This is because `cv::Mat` uses matrix indexing convention and Image
 * uses image indexing convention.
 *
 * Be aware that making a sharedCopy can be potentially dangerous. Imagine the following situation,
 * where two functions do semanticly the same, but one utilizes move semantics for efficiency:
 * @code
 * // function copying the argument
 * Image addOne(ConstImage const& img) {
 *     return Image{img.cvMat() + 1};
 * }
 *
 * // function overload modifying the argument for efficiency
 * Image addOne(Image&& img) {
 *     return Image{img.cvMat() += 1};
 * }
 * @endcode
 * Now suppose you want to have a new image made from some `crop` region of an image `img`, but
 * added one to it with the above function `addOne()`:
 * @code
 * Image newImgPart = addOne(img.sharedCopy(crop));
 * @endcode
 * This indeed calls the `addOne(ConstImage const& img)` function above. To prevent accidential
 * modifications in situations like this (here: prevent calling the `addOne(Image&& img)` function
 * above), the sharedCopy functions that would output a writable Image, return a cv::Mat instead.
 * Additionally the constructor for cv::Mat of ConstImage works implicit, while the one of Image
 * works only explicit. So the cv::Mat is used here as indirection to distinguish an unnamed
 * sharedCopy (prvalue), from a newly constructed Iamge. Therefore, when requiring a writable
 * shared copy, we have to use
 * @code
 * Image shared{img.sharedCopy()}; // explicit constructor
 * @endcode
 * instead of
 * @code
 * //Image shared = img.sharedCopy(); // ERROR: copy initialization forbidden
 * @endcode
 * However, if you really want to hand a writeable shared copy to a function or method, just
 * surround it by something more explicit, like
 * @code
 * df.outputImage() = Image{img.sharedCopy()};
 * @endcode
 * In situations where a read-only shared copy is enough, just use #constSharedCopy! The real world
 * case, where this might occur, are the operations like #add, #absdiff, etc. Processing a cropped
 * region would without this protection override parts of the image:
 * @code
 * //Image diff = imgA.sharedCopy(crop).absdiff(imgB.sharedCopy(crop)); // does not compile
 *   Image diff = imgA.constSharedCopy(crop).absdiff(imgB.constSharedCopy(crop)); // works as expected
 * @endcode
 *
 * Image provides some elementwise operations like add and subtract, which act on the whole Image
 * (and return a new Image for the result). However, not every useful operation is declared in
 * Image. For operations missing in Image but available in OpenCV, the underlying `cv::Mat` can be
 * used with cvMat(). This gives the user all the power from OpenCV to use in ImageFusion.
 *
 * Often, writing an algorithm requires access on single pixels. This is possible either with
 * iterators or with direct access. A small example shows how to operate on Images:
 * @code
 * Image im{5, 6, Type::uint8x1};
 * im.set(100);                    // image-wide operation
 * double norm = im.cvMat().norm() // OpenCV operation
 *
 * // element access with (channel) iterator
 * for (auto it = im.begin<uint8_t>(0), it_end = im.end<uint8_t>(0); it != it_end; ++it)
 *     *it = std::sqrt(*it);
 *
 * // direct element access
 * for (int y = 0; y < im.height(); ++y)
 *     for (int x = 0; x < im.width(); ++x)
 *         im.at<uint8_t>(x, y, 0) = 10 * y + x;
 * @endcode
 * Note, for performance reasons, when you run through an image with direct access, always have the
 * loops from outside to inside in the order:
 * * y
 *   * x
 *     * channel (channel, only when using \link at(unsigned int x, unsigned int y, unsigned int channel) at with channel argument\endlink)
 *
 * Now, if you do not now the image type and write an algorithm working for different types of
 * images, you still need access on single pixels and that requires knowing the type. However, you
 * can handle that in a functor and call that with e. g. CallBaseTypeFunctor. Have a look there how
 * to use it.
 *
 * There is also a class ConstImage, which prevents accidentially modification of const Images.
 * ConstImage can be used like Image, but it has only the const methods. So whenever you declare a
 * function and want only to read from an image, just use a ConstImage like:
 * @code
 * void f(ConstImage const& input);
 * @endcode
 * You can still use it with normal Images like
 * @code
 * Image i{5, 6, Type::uint8x1};
 * i.set(100);
 * f(i);
 * @endcode
 * So, Image can be converted implicitly to ConstImage. Also making a shared copy from a constant
 * Image gives a ConstImage, like:
 * @code
 * Image i{5, 6, Type::uint8x1};
 * Image const& ci = i;
 * ConstImage shared = ci.sharedCopy();
 * @endcode
 * This makes sure that `shared` cannot be modified to modify `i`. However, if you really aim at
 * fooling the constness, you can still do that -- even without nasty type casts from ConstImage to
 * Image -- using OpenCV:
 * @code
 * Image i{5, 6, Type::uint8x1};
 * Image const& ci = i;
 * ConstImage constShared = ci.sharedCopy();
 * Image shared;
 * shared.cvMat() = constShared.cvMat(); // OpenCV copy constructor makes a shared copy
 * @endcode
 * Try not to do that! It is nasty! Try rather to make your interface less const (but as much as
 * possible), so it is more obvious that an Image might get modified.
 *
 * \ingroup typegrp
 */
class Image : public ConstImage {
public:
    /// \copydoc ConstImage::ConstImage()
    Image() = default;


    /// \copydoc ConstImage::ConstImage(Size s, Type t)
    Image(Size s, Type t);


    /// \copydoc ConstImage::ConstImage(int width, int height, Type t)
    Image(int width, int height, Type t);


    /// \copydoc ConstImage::ConstImage(std::string const& filename, std::vector<int> channels, Rectangle r, bool flipH, bool flipV, bool ignoreColorTable)
    explicit Image(std::string const& filename, std::vector<int> channels = {}, Rectangle r = {0, 0, 0, 0}, bool flipH = false, bool flipV = false, bool ignoreColorTable = false);

    /// \copydoc ConstImage::ConstImage(ConstImage const& i)
    Image(Image const& i);


    /**
     * @brief Converting copy construct an Image by cloning a ConstImage
     * @param i is the image to clone.
     *
     * Note, copying a ConstImage means copying the contents of the image. So, in OpenCV
     * terminology the copy constructor clones an image. This is very different to the OpenCV copy
     * constructor, which only makes a shared copy with a shared pixel memory. To get such a shared
     * copy, use sharedCopy().
     *
     * This conversion constructor is explicit to avoid accidential cloning in situations like
     * @code
     * Image shared = i.sharedCopy();
     * // or
     * Image const& src = i;
     * @endcode
     * where `i` is a ConstImage.
     */
    explicit Image(ConstImage const& i);


    /// \copydoc ConstImage::ConstImage(ConstImage&& i)
    Image(Image&& i) noexcept;


    /// \copydoc ConstImage::ConstImage(cv::Mat img)
    explicit Image(cv::Mat img) noexcept;


    /// \copydoc ConstImage::operator=(ConstImage i)
    Image& operator=(Image i) noexcept;


    /// \copydoc ConstImage::~ConstImage()
    ~Image() = default;


    /// \copydoc ConstImage::cvMat() const
    cv::Mat& cvMat();
    using ConstImage::cvMat;


    /// \copydoc ConstImage::asGDALDataset
    GDALDataset* asGDALDataset();


    /**
     * @brief Read an image from a file
     *
     * @param filename is the image file to read from
     *
     * @param channels specifies optionally which channels (0-based) to read, otherwise all will be
     * read. Example: {0, 2} would only read channels 0 and 2. Example: {0,0,0} would work for a
     * single-channel source image and would read channel 0 three times to fill a 3-channel image.
     *
     * @param r limits optionally the region to read. Note, a zero at width or height means full
     * width or full height, respectively, but are limited then by the bounds. Also note in case of
     * `flipH` or `flipV` the region is specified in the unflipped image.
     *
     * @param flipH sets whether to read the image flipped horizontally.
     *
     * @param flipV sets whether to read the image flipped vertically.
     *
     * @param ignoreColorTable determines, whether a possibly existing color table will be ignored.
     * When using `false` an image with indexed colors will not be converted. This is for example
     * important when reading python-fmask images. For images without color table
     * `ignoreColorTable` does not have any effect.
     *
     * @param interp is the interpolation method in case the image file is a multi-image-file (like
     * HDF) and the specified channels have different resolutions. Then the highest resolution is
     * used and the lower ones are interpolated while reading.
     *
     * `read` uses GDAL for input. GDAL figures by itself out, which driver to take. If size and
     * type fit, the memory of the existing image will be reused! In this case, also the memory of
     * shared copies will be filled with new image. In case size or type differs, any shared images
     * will get decoupled.
     *
     * This method reads only image contents; no meta data or geo information. For the latter see
     * GeoInfo. This is also helpful if the region should be limited or not all channels should be
     * read, since GeoInfo provides also the size and the number of channels available before
     * reading.
     *
     * @throws runtime_error if `filename` cannot be found or opened with any GDAL driver.
     *
     * @throws size_error if `r` is ill-formed, i. e. out of the bounds of the image or has
     * negative width or height.
     *
     * @throws image_type_error if `channels` specifies channels that do not exist.
     *
     * @see GeoInfo
     *
     * If you want to create a new image from a file, use
     * Image(std::string const& filename, std::vector<int> channels, Rectangle r, bool flipH, bool flipV, InterpMethod interp)!
     */
    void read(std::string const& filename, std::vector<int> channels = {}, Rectangle r = {0, 0, 0, 0}, bool flipH = false, bool flipV = false, bool ignoreColorTable = false, InterpMethod interp = InterpMethod::bilinear);


    /**
     * @brief Copy pixel values from an image
     *
     * @param src is the source image.
     *
     * @param mask image with base type `uint8` with values 0 or 255 (single channel or same number
     * of channels)
     *
     * Copies pixel values from a source image, that must have the same size as the calling image.
     * If the size differs, a crop operation can be applied before copying values. To select a
     * specific (non-rectangular) region, a mask can be used. The mask is a uint8 image with values
     * 0 or 255. If it is a single channel mask, it is applied to all channels of src. If it the
     * same number of channels, the mask channels are applied to the corresponding channels of the
     * source image.
     */
    void copyValuesFrom(ConstImage const& src, ConstImage const& mask = Image{});


    /// \copydoc ConstImage::sharedCopy() const
    cv::Mat sharedCopy();
    using ConstImage::sharedCopy;


    /// \copydoc ConstImage::sharedCopy() const
    cv::Mat sharedCopy(Rectangle const& r);


    /**
     * @brief Merge multiple single-channel images into one multi-channel image
     * @param images is the vector of single channel images (only read).
     *
     * This does the opposite of split(). It merges multiple single-channel images into the calling
     * image, which will be a new multi-channel image afterwards.
     *
     * @see split
     */
    void merge(std::vector<Image> const& images);
    /// \copydoc Image::merge(std::vector<Image> const& images)
    void merge(std::vector<ConstImage> const& images);


    /**
     * @brief Set all values in an image to a specified value
     *
     * @param val is the value to set the pixels
     *
     * @param mask is either a single-channel mask to mask all channels equally or a multi-channel
     * mask with the same number of channels to apply a separate mask for each channel. By default
     * there is no mask, which means that the whole image is set to `val`.
     *
     * `val` is a `double` parameter, since it can also handle integer up to 32 bit.
     *
     * @throws image_type_error if the mask has not an `uint8` base type or the number of channels
     * of the mask is neither 1 nor matches the number of channels of the image.
     */
    void set(double val, ConstImage const& mask = ConstImage{});


    /**
     * @brief Set all values in an image to a specified value
     *
     * @param vals are the values to set per channel. The size of this vector must match the number
     * of channels. If you want to set all channels to same value, use set(double val, ConstImage
     * const& mask).
     *
     * @param mask is either a single-channel mask to mask all channels equally or a multi-channel
     * mask with the same number of channels to apply a separate mask for each channel. By default
     * there is no mask, which means that the whole image is set to `vals`.
     *
     * `vals` are of type `double`, since `double` can also handle integer up to 32 bit.
     *
     * @throws image_type_error if the mask has not an `uint8` base type or the number of channels
     * of the mask is neither 1 nor matches the number of channels of the image.
     *
     * @throws invalid_argument_error if the size of `vals` does not match the number of channels
     * of the image.
     */
    void set(std::vector<double> const& vals, ConstImage const& mask = ConstImage{});


    /// \copydoc ConstImage::abs()
    Image abs()&&;
    using ConstImage::abs;

    /// \copydoc ConstImage::absdiff()
    Image absdiff(ConstImage const& B)&&;
    /// \copydoc ConstImage::absdiff()
    Image absdiff(Image&& B)&&;
    using ConstImage::absdiff;

    /// \copydoc ConstImage::add()
    Image add(Image const& B)&&;
    using ConstImage::add;

    /// \copydoc ConstImage::subtract()
    Image subtract(Image const& B)&&;
    using ConstImage::subtract;

    /// \copydoc ConstImage::multiply()
    Image multiply(Image const& B)&&;
    using ConstImage::multiply;

    /// \copydoc ConstImage::divide()
    Image divide(Image const& B)&&;
    using ConstImage::divide;

    /// \copydoc ConstImage::bitwise_and()
    Image bitwise_and(ConstImage const& B)&&;
    using ConstImage::bitwise_and;

    /// \copydoc ConstImage::bitwise_or()
    Image bitwise_or(ConstImage const& B)&&;
    using ConstImage::bitwise_or;

    /// \copydoc ConstImage::bitwise_xor()
    Image bitwise_xor(ConstImage const& B)&&;
    using ConstImage::bitwise_xor;

    /// \copydoc ConstImage::bitwise_not()
    Image bitwise_not()&&;
    using ConstImage::bitwise_not;


    using ConstImage::begin;
    using ConstImage::end;

    /// \copydoc ConstImage::begin(unsigned int channel) const
    template<typename T>
    ChannelValueIterator<T> begin(unsigned int channel);

    /// \copydoc ConstImage::end(unsigned int channel) const
    template<typename T>
    ChannelValueIterator<T> end(unsigned int channel);

    /// \copydoc ConstImage::begin() const
    template<typename T>
    PixelIterator<T> begin();

    /// \copydoc ConstImage::end() const
    template<typename T>
    PixelIterator<T> end();

    using ConstImage::at;

    /// \copydoc ConstImage::at(unsigned int x, unsigned int y) const
    template<typename T>
    T& at(unsigned int x, unsigned int y);

    /// \copydoc ConstImage::at(unsigned int x, unsigned int y, unsigned int channel) const
    template<typename T>
    T& at(unsigned int x, unsigned int y, unsigned int channel);

    /**
     * @brief Set the value at the specified coordinates and channel
     * @param x
     * @param y
     * @param channel
     * @param val
     *
     * This will set the value at `(x, y, channel)` to `val`. It will also saturate in case the
     * value does not fit due to the underlying datatype.
     */
    void setValueAt(unsigned int x, unsigned int y, unsigned int channel, double val);

    /**
     * @brief Assuming this is a mask; set the bool value at the specified coordinates and channel
     * @param x
     * @param y
     * @param channel
     * @param val
     *
     * Only apply this on a mask! This will not check whether the type fits or not (for performance
     * reasons). Make sure, that the base type is Type::uint8.
     *
     * This will use 0 for false and 255 for true.
     */
    void setBoolAt(unsigned int x, unsigned int y, unsigned int channel, bool val);

    /**
     * @brief Swap two images
     * @param i1 is the image 1.
     * @param i2 is the image 2.
     *
     * The underlying `cv::Mat` objects are swapped. Depending on the version this is done with a
     * custom swap function in OpenCV.
     */
    friend void swap(Image& i1, Image& i2) noexcept;
};




inline void swap(Image& i1, Image& i2) noexcept {
    using std::swap;
    swap(static_cast<ConstImage&>(i1), static_cast<ConstImage&>(i2));
}

inline void swap(ConstImage& i1, ConstImage& i2) noexcept {
    using std::swap;
    swap(i1.img, i2.img);
}

inline ConstImage::ConstImage(ConstImage&& i) noexcept
    : ConstImage()
{
    using std::swap;
    swap(*this, i);
}

inline ConstImage::ConstImage(ConstImage const& i) : ConstImage{i.img.clone()} { }

inline ConstImage& ConstImage::operator=(ConstImage i) noexcept {
    using std::swap;
    swap(*this, i);
    return *this;
}

inline ConstImage::ConstImage(cv::Mat img) noexcept : img(std::move(img)) { }

inline ConstImage::ConstImage(Size s, Type t) : ConstImage(s.width, s.height, t) { }

inline ConstImage::ConstImage(int width, int height, Type t) : img(height, width, toCVType(t)) {
    if (height <= 0 || width <= 0) {
        IF_THROW_EXCEPTION(size_error("Zero sized image (here, width: " + std::to_string(width)
                                 + ", height: " + std::to_string(height) + ") not supported."))
                << errinfo_size(Size{width, height});
    }
}

inline ConstImage::ConstImage(std::string const& filename, std::vector<int> channels, Rectangle r, bool flipH, bool flipV, bool ignoreColorTable)
    : img(std::move(Image{filename, std::move(channels), r, flipH, flipV, ignoreColorTable}.img)) { }

inline ConstImage::ConstImage(Image const& i) : ConstImage{i.img.clone()} { }

inline ConstImage::ConstImage(Image&& i) : img(std::move(i.img)) { }

inline cv::Mat const& ConstImage::cvMat() const {
    return img;
}

inline cv::Mat& Image::cvMat() {
    return img;
}

inline Image::Image(Size s, Type t) : Image(s.width, s.height, t) { }

inline Image::Image(int width, int height, Type t) : ConstImage(width, height, t) { }


inline Image::Image(std::string const& filename, std::vector<int> channels, Rectangle r, bool flipH, bool flipV, bool ignoreColorTable)
    : Image()
{
    read(filename, std::move(channels), r, flipH, flipV, ignoreColorTable);
}

inline Image::Image(Image const& i) : ConstImage{i.img.clone()} { }

inline Image::Image(ConstImage const& i) : ConstImage{i.img.clone()} { }

inline Image::Image(Image&& i) noexcept
    : Image()
{
    swap(*this, i);
}

inline Image::Image(cv::Mat img) noexcept : ConstImage{std::move(img)} { }

inline Image& Image::operator=(Image i) noexcept {
    swap(*this, i);
    return *this;
}

inline ConstImage ConstImage::sharedCopy() const {
    ConstImage ret;
    ret.img = img;
    return ret;
}

inline ConstImage ConstImage::constSharedCopy() const {
    return sharedCopy();
}

inline ConstImage ConstImage::sharedCopy(Rectangle r) const {
    ConstImage ret;
    r &= Rectangle{0, 0, width(), height()};

    if (r.area() == 0)
        IF_THROW_EXCEPTION(size_error("Crop with zero size (here, width: " + std::to_string(r.width)
                                    + ", height: " + std::to_string(r.height) + ") not supported."));
    ret.img = img(r);
    return ret;
}

inline ConstImage ConstImage::constSharedCopy(Rectangle const& r) const {
    return sharedCopy(r);
}

inline cv::Mat Image::sharedCopy() {
    return img;
}

inline cv::Mat Image::sharedCopy(Rectangle const& r) {
    ConstImage temp = ConstImage::sharedCopy(r);
    return temp.img;
}


inline void ConstImage::moveCropWindow(int right, int down) {
    adjustCropBorders(-down, down, -right, right);
}

inline Size ConstImage::size() const {
    return Size{width(), height()};
}

inline bool ConstImage::isSharedWith(ConstImage const& i) const {
    return img.datastart == i.img.datastart;
}

inline int ConstImage::height() const {
    return img.rows;
}


inline int ConstImage::width() const {
    return img.cols;
}


inline unsigned int ConstImage::channels() const {
    return static_cast<unsigned int>(img.channels());
}


inline Type ConstImage::type() const {
    return toFullType(img.type());
}

inline Type ConstImage::basetype() const {
    return getBaseType(toFullType(img.type()));
}

inline bool ConstImage::empty() const {
    return img.empty();
}

inline bool ConstImage::isMaskFor(ConstImage const& im) const {
    return size() == im.size()
        && basetype() == Type::uint8
        && (channels() == 1 || channels() == im.channels());
}

inline std::ptrdiff_t ConstImage::getPtrDiffRow() const {
    return (img.ptr(1) - img.ptr(0)) / img.elemSize1();
}

inline std::ptrdiff_t ConstImage::getPtrDiffColumn() const {
    return channels();
}


template<typename T>
inline ChannelValueIterator<T> Image::begin(unsigned int channel) {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    T* p = img.ptr<T>() + channel;
    return ChannelValueIterator<T>(p, width(), height(), getPtrDiffColumn(), getPtrDiffRow());
}

template<typename T>
inline ChannelValueIterator<T> Image::end(unsigned int channel) {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    T* p = img.ptr<T>(height()-1) + (width()-1) * img.channels() + channel;
    auto ret = ChannelValueIterator<T>(p, width()-1, height()-1, width(), height(), getPtrDiffColumn(), getPtrDiffRow());
    return ++ret;
}

template<typename T>
inline ConstChannelValueIterator<T> ConstImage::begin(unsigned int channel) const {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    T const* p = img.ptr<T>() + channel;
    return ConstChannelValueIterator<T>(p, width(), height(), getPtrDiffColumn(), getPtrDiffRow());
}

template<typename T>
inline ConstChannelValueIterator<T> ConstImage::end(unsigned int channel) const {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    T const* p = img.ptr<T>(height()-1) + (width()-1) * img.channels() + channel;
    auto ret = ConstChannelValueIterator<T>(p, width()-1, height()-1, width(), height(), getPtrDiffColumn(), getPtrDiffRow());
    return ++ret;
}

template<typename T>
inline PixelIterator<T> Image::begin() {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    if (img.empty())
        return PixelIterator<T>{};
    return img.begin<T>();
}

template<typename T>
inline PixelIterator<T> Image::end() {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    if (img.empty())
        return PixelIterator<T>{};
    return img.end<T>();
}

template<typename T>
inline ConstPixelIterator<T> ConstImage::begin() const {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    if (img.empty())
        return ConstPixelIterator<T>{};
    return img.begin<T>();
}

template<typename T>
inline ConstPixelIterator<T> ConstImage::end() const {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    if (img.empty())
        return ConstPixelIterator<T>{};
    return img.end<T>();
}

template<typename T>
inline T& Image::at(unsigned int x, unsigned int y) {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    return img.at<T>(y, x);
}

template<typename T>
inline T const& ConstImage::at(unsigned int x, unsigned int y) const {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    return img.at<T>(y, x);
}

template<typename T>
inline T& Image::at(unsigned int x, unsigned int y, unsigned int channel) {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    return *(img.ptr<T>(y) + x * img.channels() + channel);
}

template<typename T>
inline T const& ConstImage::at(unsigned int x, unsigned int y, unsigned int channel) const {
    static_assert(!is_known_bool_type<T>::value, "bool not supported! For masks use uint8_t with 0 for false and 255 for true!");
    return *(img.ptr<T>(y) + x * img.channels() + channel);
}

inline bool ConstImage::boolAt(unsigned int x, unsigned int y, unsigned int channel) const {
    return at<uint8_t>(x,y,channel) != 0;
}

inline double ConstImage::doubleAt(unsigned int x, unsigned int y, unsigned int channel) const {
    switch(basetype()) {
    case Type::uint8:
        return at<uint8_t>(x, y, channel);

    case Type::int8:
        return at<int8_t>(x, y, channel);

    case Type::uint16:
        return at<uint16_t>(x, y, channel);

    case Type::int16:
        return at<int16_t>(x, y, channel);

    case Type::int32:
        return at<int32_t>(x, y, channel);

    case Type::float32:
        return at<float>(x, y, channel);

    default: //Type::float64:
        return at<double>(x, y, channel);
    }
}

inline void Image::setValueAt(unsigned int x, unsigned int y, unsigned int channel, double val) {
    switch(basetype()) {
    case Type::uint8:
        at<uint8_t>(x, y, channel) = cv::saturate_cast<uint8_t>(val);
        break;

    case Type::int8:
        at<int8_t>(x, y, channel) = cv::saturate_cast<int8_t>(val);
        break;

    case Type::uint16:
        at<uint16_t>(x, y, channel) = cv::saturate_cast<uint16_t>(val);
        break;

    case Type::int16:
        at<int16_t>(x, y, channel) = cv::saturate_cast<int16_t>(val);
        break;

    case Type::int32:
        at<int32_t>(x, y, channel) = cv::saturate_cast<int32_t>(val);
        break;

    case Type::float32:
        at<float>(x, y, channel) = static_cast<float>(val);
        break;

    default: //Type::float64:
        at<double>(x, y, channel) = val;
    }
}

inline void Image::setBoolAt(unsigned int x, unsigned int y, unsigned int channel, bool val) {
    at<uint8_t>(x,y,channel) = val ? 255 : 0;
}



} /* namespace imagefusion */

#pragma once

#include <gdal.h>
#include <gdal_priv.h>
#include <gdal_frmts.h>
#include <ogr_spatialref.h>

#include <map>
#include <vector>
#include <limits>
#include <algorithm>

#include "imagefusion.h"

class GDALDataset;

namespace imagefusion {

/**
 * @brief This represents an affine transformation
 *
 * The transformation represented by this can be written as
 * \f[
 *   \begin{pmatrix}
 *     x_p\\
 *     y_p
 *   \end{pmatrix}
 *   =
 *   \begin{pmatrix}
 *     x_o\\
 *     y_o
 *   \end{pmatrix}
 *   +
 *   \begin{pmatrix}
 *     A_{xx} & A_{yx}\\
 *     A_{xy} & A_{yy}
 *   \end{pmatrix}
 *   \begin{pmatrix}
 *     x_i\\
 *     y_i
 *   \end{pmatrix},
 * \f]
 * where \f$ (x_o, y_o)^\top \f$ is the offset of the origin,
 *       \f$ A \f$ is a linear transformation,
 *       \f$ (x_i, y_i)^\top \f$ are the image coordinates in pixels and
 *       \f$ (x_p, y_p)^\top \f$ are the projected coordinates in the specified geotransform
 *         projection coordinate system (@ref GeoInfo::geotransSRS) with corresponding units like
 *         meter.
 *
 * Note we model a pixel as an area that averages the corresponding real world area. In image
 * coordinate space, we define that the very first pixel has its top left corner at (0, 0), its
 * center at (0.5, 0.5) and its bottom right corner at (1, 1). It is illustrated in the following
 * 2x2 example image:
 * @image html imgSpace.png
 *
 * Now there is also a projection space that can be mapped to the transformation, which this class
 * represents. Consider the following example. The top left corner of an image is located at the
 * projection space coordinates (10, 20) and they advance by 10 for each pixel in each direction.
 * So the transformation is
 * \f[
 *   \begin{pmatrix}
 *     x_p\\
 *     y_p
 *   \end{pmatrix}
 *   =
 *   \begin{pmatrix}
 *     10\\
 *     20
 *   \end{pmatrix}
 *   +
 *   \begin{pmatrix}
 *     10 & 0\\
 *     0 & 10
 *   \end{pmatrix}
 *   \begin{pmatrix}
 *     x_i\\
 *     y_i
 *   \end{pmatrix},
 * \f]
 * This is illustrated in the following figure.
 * @image html imgAndProjSpace.png
 * Codewise you would get such a transformation either by setting the coefficients directly
 * @code
 * GeoInfo gi;
 * gi.geotrans.set(10, 20, 10, 0, 0, 10);
 * // or value by value
 * gi.geotrans.offsetX = 10;
 * gi.geotrans.offsetY = 20;
 * gi.geotrans.XToX = 10;
 * gi.geotrans.YToX = 0;
 * gi.geotrans.XToY = 0;
 * gi.geotrans.YToY = 10;
 * @endcode
 * or alternatively you could build up the transformation with basic transformations, starting with
 * an identity transformation by default:
 * @code
 * GeoInfo gi; // identity transformation
 * gi.geotrans.scaleProjection(10, 10);
 * gi.geotrans.translateProjection(10, 20);
 * @endcode
 * Both methods result in the transformation above. When building it with basic transformations, we
 * used `*Projection()`-methods. These operate in projection space, while there are also
 * `*Image()`-methods that operate in image space. The latter is useful when a change is made to an
 * image and the transformation should follow these changes, e. g.
 * @code
 * std::string filename = "path/to/image.tif";
 * Image i(filename);
 * GeoInfo gi(filename);
 * @endcode
 * @code
 * i.crop({10, 20, 100, 100});
 * gi.geotrans.translateImage(10, 20); // NOT: translateProjection
 * gi.size = Size{100, 100};
 * @endcode
 *
 * All basic transformations are listed here:
 *  - set to identity transformation (@ref clear),
 *  - rotation (projection space (@ref rotateProjection) or image space (@ref rotateImage)),
 *  - scaling (projection space (@ref scaleProjection) or image space (@ref scaleImage)),
 *  - shearing (projection space (@ref shearXProjection / @ref shearYProjection) or image space (@ref shearXImage / @ref shearYImage)) and
 *  - translation (projection space (@ref translateProjection) or image space (@ref translateImage)).
 *  - flipping in image space (@ref flipImage).
 *
 * Note that the resulting affine transformation depends on the order of the composition.
 */
class GeoTransform {
public:
    /**
     * @brief Internal storage of coefficients for an affine transformation
     *
     * These coefficients represent the affine transformation. They are saved in the following
     * order in this array:
     *  - \f$ x_o \f$
     *  - \f$ A_{xx} \f$
     *  - \f$ A_{yx} \f$
     *  - \f$ y_o \f$
     *  - \f$ A_{xy} \f$
     *  - \f$ A_{yy} \f$
     *
     * This is not very intuitive, so you can use references with a speaking name:
     *  - @ref offsetX is \f$ x_o \f$
     *  - @ref offsetY is \f$ y_o \f$
     *  - @ref XToX    is \f$ A_{xx} \f$
     *  - @ref YToX    is \f$ A_{yx} \f$
     *  - @ref XToY    is \f$ A_{xy} \f$
     *  - @ref YToY    is \f$ A_{yy} \f$
     *
     * Note, either geotransformation + the geotransform projection coordinate system (@ref
     * GeoInfo::geotransSRS) *or* ground control points + the GCP projection coordinate system
     * (@ref GeoInfo::gcpSRS) can be used. They are mutual exclusive ways for georeferencing an
     * image. To use geotransformation specify the transformation with the corresponding methods
     * and provide a valid spatial reference before using @ref GeoInfo::addTo.
     *
     * @see clear, set, GeoInfo::geotransSRS
     */
    std::array<double,6> values = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};


    /**
     * @brief Geotransform coefficient \f$ x_o \f$ (x-coordinate of origin)
     *
     * \f$ \begin{pmatrix} x_o\\ y_o \end{pmatrix} \f$ corresponds to the top left corner (0, 0) in
     * image space. However, when @ref XToX or @ref YToY is negative this is not the top left
     * corner in projection space.
     *
     * @see GeoTransform
     */
    double& offsetX = values[0];

    /**
     * @brief Geotransform coefficient \f$ y_o \f$ (y-coordinate of origin)
     *
     * \f$ \begin{pmatrix} x_o\\ y_o \end{pmatrix} \f$ corresponds to the top left corner (0, 0) in
     * image space. However, when @ref XToX or @ref YToY is negative this is not the top left
     * corner in projection space.
     *
     * @see GeoTransform
     */
    double& offsetY = values[3];

    /**
     * @brief Geotransform coefficient \f$ A_{xx} \f$
     *
     * A negative value XToX in a diagonal \f$ A \f$ means, that when the x value of the image
     * coordinate increases (going right) the projection coordinate decreases (going left).
     *
     * @see GeoTransform
     */
    double& XToX = values[1];

    /**
     * @brief Geotransform coefficient \f$ A_{yx} \f$
     *
     * Is usually 0 for common images.
     *
     * @see GeoTransform
     */
    double& YToX = values[2];

    /**
     * @brief Geotransform coefficient \f$ A_{xy} \f$
     *
     * Is usually 0 for common images.
     *
     * @see GeoTransform
     */
    double& XToY = values[4];

    /**
     * @brief Geotransform coefficient \f$ A_{yy} \f$
     *
     * A negative value YToY in a diagonal \f$ A \f$ means, that when the y value of the image
     * coordinate increases (going down) the projection coordinate decreases (going up).
     *
     * @see GeoTransform
     */
    double& YToY = values[5];
    /*  / x_p \     / XToX  YToX \   / x_i \     / offsetX \
     * |       | = |              | |       | + |           |
     *  \ y_p /     \ XToY  XToY /   \ y_i /     \ offsetY /
     */


    /**
     * @brief Default constructor with shifted identity transform
     */
    GeoTransform() noexcept { }


    /**
     * @brief Copy construct a GeoTransform object
     * @param gt is the source object that will be copied.
     */
    GeoTransform(GeoTransform const& gt) noexcept;


    /**
     * @brief Move construct a GeoTransform object
     * @param gt is the source object, which is in an undefined state afterwards.
     */
    GeoTransform(GeoTransform&& gt) noexcept;


    /**
     * @brief Copy and move assignment operator
     * @param gt is the source object.
     */
    GeoTransform& operator=(GeoTransform gt) noexcept;


    /**
     * @brief Apply the geotransformation to an image coordinate
     *
     * @param c_i is the coordinate in image space.
     *
     * This method will calculate the current transformation
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix},
     * \f]
     * with `c_i`\f$ = (x_i, y_i) \f$. The projected coordinate \f$ (x_p, y_p) \f$ is returned.
     *
     * @return the projected coordinate \f$ (x_p, y_p) \f$.
     *
     * @see GeoTransform
     */
    Coordinate imgToProj(Coordinate const& c_i) const;


    /**
     * @brief Apply the geotransformation to a rectangle in image coordinates
     *
     * @param r_i is the rectangle in image space.
     *
     * This method will transform two corners of the rectangle with help of @ref
     * imgToProj(Coordinate const&) const and return the corresponding rectangle. Note, the
     * top-left corner of `r_i` does not correspond to the top-left corner of the returned
     * rectangle in general.
     *
     * @return the projected rectangle in projection space.
     *
     * @see GeoTransform
     */
    CoordRectangle imgToProj(CoordRectangle const& r_i) const;


    /**
     * @brief Apply the inverse geotransformation to a projection space coordinate
     *
     * @param c_p is the coordinate in projection space.
     *
     * This method will calculate the current transformation
     * \f[
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}^{-1}
     *   \begin{pmatrix}
     *     x_p - x_o\\
     *     y_p - y_o
     *   \end{pmatrix}
     * \f]
     * with `c_p`\f$ = (x_p, y_p) \f$. The resulting image space coordinate \f$ (x_i, y_i) \f$ is
     * returned.
     *
     * @return the projected coordinate \f$ (x_i, y_i) \f$ in image space.
     *
     * @see GeoTransform
     */
    Coordinate projToImg(Coordinate const& c_p) const;


    /**
     * @brief Apply the inverse geotransformation to a rectangle in projection space coordinate
     *
     * @param r_p is the rectangle in projection space.
     *
     * This method will transform two corners of the rectangle with help of @ref
     * projToImg(Coordinate const&) const and return the corresponding rectangle. Note, the
     * top-left corner of `r_p` does not correspond to the top-left corner of the returned
     * rectangle in general.
     *
     * @return the projected rectangle in image space.
     *
     * @see GeoTransform
     */
    CoordRectangle projToImg(CoordRectangle const& r_p) const;


    /**
     * @brief Clear geotransform to identity
     *
     * This method will reset the affine transformation to
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     0\\
     *     0
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     1 & 0\\
     *     0 & 1
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * When using @ref GeoInfo::addTo and the geotransform is the identity transformation, it will
     * not be used. The identity transformation is considered as not existing.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& clear();


    /**
     * @brief Check whether the transformation is a shifted identity transformation
     *
     * So this checks whether the transformation represents
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     0\\
     *     0
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     1 & 0\\
     *     0 & 1
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * @return true if it does, false if not.
     */
    bool isIdentity() const;


    /**
     * @brief Scale the projection space
     * @param xscale is the factor for the x direction.
     * @param yscale is the factor for the y direction.
     *
     * This scales the projection space, i. e. the complete affine transformation, including the
     * offset. So as a formula where `xscale` and `yscale` are denoted by \f$ x_s \f$ and \f$ y_s
     * \f$, respectively, this is a multiplication from left
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_s & 0\\
     *     0 & y_s
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     x_s \, x_o\\
     *     y_s \, y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     x_s \, A_{xx} & x_s \, A_{yx}\\
     *     y_s \, A_{xy} & y_s \, A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save \f$ x_s \f$ and \f$ y_s \f$ separately, but instead
     * modify the coefficients, like \f$ x_o \leftarrow x_s \, x_o \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& scaleProjection(double xscale, double yscale);


    /**
     * @brief Scale the image space
     * @param xscale is the factor for the x direction.
     * @param yscale is the factor for the y direction.
     *
     * This scales the image space. So as a formula where `xscale` and `yscale` are denoted by \f$
     * x_s \f$ and \f$ y_s \f$, respectively, this is a multiplication of the image coordinates
     * from left with the scale matrix
     * \f[
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_s & 0\\
     *     0 & y_s
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     * \f]
     * which results in
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     x_s \, A_{xx} & y_s \, A_{yx}\\
     *     x_s \, A_{xy} & y_s \, A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save \f$ x_s \f$ and \f$ y_s \f$ separately, but instead
     * modify the coefficients, like \f$ A_{xx} \leftarrow x_s \, A_{xx} \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& scaleImage(double xscale, double yscale);


    /**
     * @brief Rotate the projection space
     * @param angle is the rotation angle in degree.
     *
     * This rotates the projection space, i. e. the complete affine transformation, including the
     * offset. So as a formula, where `angle` is denoted by \f$ \alpha \f$, this is a
     * multiplication from left with a rotation matrix
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     \cos \alpha & -\sin \alpha\\
     *     \sin \alpha & \cos \alpha
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     \cos \alpha & -\sin \alpha\\
     *     \sin \alpha & \cos \alpha
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     \cos \alpha & -\sin \alpha\\
     *     \sin \alpha & \cos \alpha
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the rotation matrix separately, but instead modify
     * the coefficients, like \f$ x_o \leftarrow x_o \, \cos \alpha - y_o \, \sin \alpha \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& rotateProjection(double angle);


    /**
     * @brief Rotate the image space
     * @param angle is the rotation angle in degree.
     *
     * This rotates the image space. So as a formula, where `angle` is denoted by \f$ \alpha \f$,
     * this is a multiplication of the image coordinates from left with a rotation matrix
     * \f[
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     \cos \alpha & -\sin \alpha\\
     *     \sin \alpha & \cos \alpha
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     * \f]
     * which results in
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     \cos \alpha & -\sin \alpha\\
     *     \sin \alpha & \cos \alpha
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the rotation matrix separately, but instead modify
     * the coefficients, like \f$ A_{xx} \leftarrow A_{xx} \, \cos \alpha - A_{yx} \, \sin \alpha
     * \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& rotateImage(double angle);


    /**
     * @brief Flip the image space
     * @param flipH means flip the image coordinates horizontally.
     * @param flipV means flip the image coordinates vertically.
     * @param s is the size of the image.
     *
     * This flips the image space. So as a formulas, where \f$ w \f$ denotes the width (`s.width`)
     * and \f$ h \f$ denotes the height (`s.height`).
     *
     * For horizontal flipping:
     * \f[
     *   (x_i)
     *   \longleftarrow
     *   w - (x_i)
     * \f]
     * For vertical flipping:
     * \f[
     *   (y_i)
     *   \longleftarrow
     *   h - (y_i)
     * \f]
     *
     * This results in the following.
     *
     * For only horizontal flipping:
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o + A_{xx} \, w\\
     *     y_o + A_{xy} \, w
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     -A_{xx} & A_{yx}\\
     *     -A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     * For only vertical flipping:
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o + A_{yx} \, h\\
     *     y_o + A_{yy} \, h
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & -A_{yx}\\
     *     A_{xy} & -A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     * For both horizontal and vertical flipping:
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o + A_{xx} \, w + A_{yx} \, h\\
     *     y_o + A_{xy} \, w + A_{yy} \, h
     *   \end{pmatrix}
     *   -
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * This can be useful if an image is read flipped. In the following example assume `r` is a
     * `Rectangle` that crops the image while reading and `flipH` and `flipV` are `bool` variables
     * to flip the image horizontally or vertically.
     * @code
     * Image i{filename, r, {}, flipH, flipV};
     * GeoInfo gi{filename};
     * gi.geotrans.translateImage(r.x, r.y);
     * gi.geotrans.flipImage(flipH, flipV, gi.size);
     * @endcode
     * Note, the order of translateImage() and flipImage() is important. Alternatively, use the
     * corresponding constructor:
     * @code
     * GeoInfo gi{filename, {}, r, flipH, flipV};
     * @endcode
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& flipImage(bool flipH, bool flipV, Size s);


    /**
     * @brief Shear the projection space in x direction
     * @param factor is the shear factor.
     *
     * This shears the projection space, i. e. the complete affine transformation, including the
     * offset. So as a formula, where `factor` is denoted by \f$ c \f$, this is a multiplication
     * from left with a shear matrix
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     1 & c\\
     *     0 & 1
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     x_o + c \, y_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} + c \, A_{xy} & A_{yx} + c \, A_{yy}\\
     *     A_{xy}               & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the shear factor \f$ c \f$ separately, but instead
     * modify the coefficients, like \f$ x_o \leftarrow x_o + c \, y_o \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& shearXProjection(double factor);


    /**
     * @brief Shear the image space in x direction
     * @param factor is the shear factor.
     *
     * This shears the image space. So as a formula, where `factor` is denoted by \f$ c \f$, this
     * is a multiplication of the image coordinates from left with a shear matrix
     * \f[
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     1 & c\\
     *     0 & 1
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     * \f]
     * which results in
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx} + c \, A_{xx}\\
     *     A_{xy} & A_{yy} + c \, A_{xy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the shear factor \f$ c \f$ separately, but instead
     * modify the coefficients, like \f$ A_{yx} \leftarrow A_{yx} + c \, A_{xx} \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& shearXImage(double factor);


    /**
     * @brief Shear the projection space in y direction
     * @param factor is the shear factor.
     *
     * This shears the projection space, i. e. the complete affine transformation, including the
     * offset. So as a formula, where `factor` is denoted by \f$ c \f$, this is a multiplication
     * from left with a shear matrix
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     1 & 0\\
     *     c & 1
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o + c \, x_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx}               & A_{yx}\\
     *     A_{xy} + c \, A_{xx} & A_{yy} + c \, A_{yx}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the shear factor \f$ c \f$ separately, but instead
     * modify the coefficients, like \f$ y_o \leftarrow y_o + c \, x_o \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& shearYProjection(double factor);


    /**
     * @brief Shear the image space in y direction
     * @param factor is the shear factor.
     *
     * This shears the image space. So as a formula, where `factor` is denoted by \f$ c \f$, this
     * is a multiplication of the image coordinates from left with a shear matrix
     * \f[
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     1 & 0\\
     *     c & 1
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     * \f]
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} + c \, A_{yx} & A_{yx}\\
     *     A_{xy} + c \, A_{yy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the shear factor \f$ c \f$ separately, but instead
     * modify the coefficients, like \f$ A_{xx} \leftarrow A_{xx} + c \, A_{yx} \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& shearYImage(double factor);


    /**
     * @brief Translate the projection space
     * @param xoffset is the translation in projection unit (e. g. meter) added to the x direction.
     * @param yoffset is the translation in projection unit (e. g. meter) added to the y direction.
     *
     * This translates the offset of the projection space. So as a formula, where `xoffset` and
     * `yoffset` are denoted by \f$ x_t \f$ and \f$ y_t \f$, respectively, this is an addition with
     * the offset vector
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_t\\
     *     y_t
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     x_o + x_t\\
     *     y_o + y_t
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the translation separately, but instead modify the
     * offsets, like \f$ x_o \leftarrow x_o + x_t \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& translateProjection(double xoffset, double yoffset);


    /**
     * @brief Translate the image space
     * @param xoffset is the translation in pixels added to x direction.
     * @param yoffset is the translation in pixels added to y direction.
     *
     * This translates the offset of the image space. So as a formula, where `xoffset` and
     * `yoffset` are denoted by \f$ x_t \f$ and \f$ y_t \f$, respectively, this is an addition of
     * the image coordinates with the offset vector
     * \f[
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_t\\
     *     y_t
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}
     * \f]
     * which results in
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   \longleftarrow
     *   \begin{pmatrix}
     *     x_o + A_{xx} \, x_t + A_{yx} \, y_t\\
     *     y_o + A_{xy} \, x_t + A_{yy} \, y_t
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix}.
     * \f]
     *
     * This operation is required when the offset in an image changes due to a crop or a data
     * fusion with prediction area.
     *
     * For general details about the affine transformation, see @ref GeoTransform.
     *
     * Note, calling this method will not save the translation separately, but instead modify the
     * offsets, like \f$ x_o \leftarrow x_o + A_{xx} \, x_t + A_{yx} \, y_t \f$.
     *
     * @return the calling object so you can append other operations.
     */
    GeoTransform& translateImage(double xoffset, double yoffset);


    /**
     * @brief Set coefficients for an affine transformation
     *
     * The coefficients can be set all together with this method or one by one with the
     * corresponding members:
     *  - @ref offsetX
     *  - @ref offsetY
     *  - @ref XToX
     *  - @ref YToX
     *  - @ref XToY
     *  - @ref YToY
     *
     * Alternatively, the transformation can also be composed from some basic transformations:
     *  - @ref clear (reset to identity transformation)
     *  - @ref rotateProjection, @ref rotateImage
     *  - @ref scaleProjection, @ref scaleImage
     *  - @ref shearXProjection, @ref shearXImage
     *  - @ref shearYProjection, @ref shearYImage
     *  - @ref translateProjection, @ref translateImage
     *  - @ref flipImage
     *
     * @see GeoTransform
     */
    void set(double topLeft_x, double topLeft_y, double x_to_x, double y_to_x, double x_to_y, double y_to_y);


    /**
     * @brief Swap two GeoTransform objects
     */
    friend void swap(GeoTransform& g1, GeoTransform& g2) noexcept;
};

inline bool operator==(GeoTransform const& gt1, GeoTransform const& gt2) {
    return gt1.values == gt2.values;
}

inline bool operator!=(GeoTransform const& gt1, GeoTransform const& gt2) {
    return !(gt1 == gt2);
}


/**
 * @brief Manages non-pixel information of images
 *
 * This class provides all information for images, that is not required for image processing. These
 * include
 *  - a value to mark invalid data which can be used to build a mask (@ref nodataValues),
 *  - a color table for indexed color images
 *    @ref addTo(std::string const& filename) const "(be carful when writing GeoInfos with color tables)",
 *  - arbitrary @ref metadata structured in domains and then key-value pairs,
 *  - projection coordinate system together with either
 *    - ground control points (@ref gcps) or
 *    - coefficients that define an affine transform (@ref geotrans) and
 *  - some image properties that will not be written as metadata, like image size, number of
 *    channels and data type
 *
 * To use GeoInfo an empty object can be created and then filled manually or directly read from an
 * image file. A GeoInfo object which contains information can be added to an existing image file
 * with @ref addTo. However, not all image formats support all kind of information. Tiff seems to
 * be the most common format and the GTiff driver implementation in GDAL the most advanced one.
 *
 * The following example shows how to pick up the geo information from a source image file
 * `src.tiff` and set it to a target image file `target.tiff`:
 * @code
 * GeoInfo gi{"src.tiff"};
 * // ... change gi as you like
 * gi.addTo("target.tiff");
 * @endcode
 *
 * @see Image::write
 */
class GeoInfo {
public:
    /**
     * @brief Size of the image in pixel coordinate space
     *
     * This is useful when you need to know the size of an image without reading the image, since
     * it will be set to the image size when constructing a GeoInfo object from an image file.
     * Note, it is 0x0 for a default constructed GeoInfo object.
     *
     * This can also be used in functions like warp to determine the output image size.
     */
    Size size = Size{};

    /**
     * @brief Number of channels
     *
     * The number of channels (in GDAL terminology: rastercount) of the image read during
     * construction. If this GeoInfo object was constructed without image file, it is 0.
     */
    int channels = 0;

    /**
     * @brief Base type of the image
     *
     * The base type of the image (in GDAL terminology: depth), i. e. it does not contain the
     * channel information.
     *
     * Note, when reading an image with subdatasets with the constructor GeoInfo(std::string const&
     * filename, std::vector<int> const& channels) the base type is determined by the base types of
     * the selected channels (they could be different). If channels with different types are
     * selected, the baseType is set to Type::invalid.
     *
     * For default constructed GeoInfo objects it is Type::invalid.
     */
    Type baseType = Type::invalid;

    /**
     * @brief Filename of the image or subdataset
     *
     * This is simply the filename of the image, but if the GeoInfo was constructed from a special
     * GDAL subdataset name, like
     * `HDF4_EOS:EOS_GRID:"path/MOD09GA.hdf":MODIS_Grid_500m_2D:sur_refl_b01_1`, this function will
     * return "path/MOD09GA.hdf".
     *
     * @return the filename with which this GeoInfo has been constructed.
     */
    std::string filename;


    /**
     * @brief Color table of the image
     *
     * This table will be read from a file properly, but write support is limited, since GDAL
     * drivers change the alpha values. For a discussion how to handle that, see the documentation
     * at @ref GeoInfo::addTo(std::string const&) const
     *
     * The resulting number of channels in @ref Image will be selected by the occuring indices and
     * the variety of occuring colors. Each color entry contains the elements red, green, blue,
     * alpha (RGBA) in that order. The following table shows which color channels are used for
     * which resulting number of channels.
     *
     * resulting number of channels | used colorTable channels
     * ---------------------------- | ------------------------
     * 1                            | 0,1,2 (RGB) for channel 0 (RGB have all the same values)
     * 2                            | 0,1,2 (RGB) for channel 0 and 3 (Alpha) for channel 1 (RGB have all the same values)
     * 3                            | 0 (R) for channel 0, 1 (G) for channel 1, 2 (B) for channel 2
     * 4                            | 0 (R) for channel 0, 1 (G) for channel 1, 2 (B) for channel 2, 3 (Alpha) for channel 3
     */
    std::vector<std::array<short,4>> colorTable;


    /**
     * @brief Represents a ground control point
     *
     * This consists of all fields that a GDAL_GCP also has, but instead of c-strings it has got
     * proper strings. Note, GDAL seems to ignore these strings (`id` and `info`) when writing to a
     * file. For `id` it just sets a running number starting at 1. `info` seems to be empty, but
     * the documentation mentions that it can hold information.
     */
    struct GCP {
        std::string id;
        std::string info;
        double pixel;
        double line;
        double x;
        double y;
        double z;
    };


    /**
     * @brief Projection coordinate system for ground control points
     *
     * Coordinate system that gives a meaning to the coordinates resulting from the transformation
     * defined by the ground control points (@ref gcps). This can be changed with the methods
     * provided by [OGRSpatialReference](http://www.gdal.org/classOGRSpatialReference.html) from
     * GDAL. Validity can be checked with `o.gcpSRS.Validate() == OGRERR_NONE`.
     *
     * Note, either ground control points + the GCP projection coordinate system (@ref gcpSRS) _or_
     * geotransformation + the geotransform projection coordinate system (@ref geotransSRS) can be
     * used. They are mutual exclusive ways for georeferencing an image. To use ground control
     * points set a list of GCPs or add GCPs and provide a valid spatial reference before using
     * @ref addTo. Beware, if also a geotransformation and a valid geotransform projection CS is
     * specified, the geotransformation takes precedence. You can clear the geotransformation to
     * avoid that.
     *
     * @see gcps, addTo, addGCP, geotrans, GeoTransform::clear,
     */
    OGRSpatialReference gcpSRS;


    /**
     * @brief Ground control points to define the geo reference projection
     *
     * A number of GCPs can be set to define the projection from image coordinates to world
     * coordinates / projection space. At least three linear independent GCPs are needed for a
     * linear transformation. Giving more could define a higher order transformation or a
     * regression. To give the projected coordinates a meaning a GCP projection coordinate system
     * is also required. The GCPs are only used in @ref addTo if the CS is valid.
     *
     * Note, either ground control points + the GCP projection coordinate system (@ref gcpSRS) _or_
     * geotransformation + the geotransform projection coordinate system (@ref geotransSRS) can be
     * used. They are mutual exclusive ways for georeferencing an image. To use ground control
     * points add GCPs and provide a valid spatial reference before using @ref addTo. Beware, if
     * also a non-identity geotransformation and a valid geotransform projection CS is specified,
     * the geotransformation takes precedence. You can clear the geotransformation to avoid that.
     *
     * @see geotrans, GeoTransform::clear, addTo, gcpSRS, addGCP
     */
    std::vector<GCP> gcps;


    /**
     * @brief Projection coordinate system for geotransform
     *
     * Coordinate system that gives a meaning to the coordinates resulting from the
     * geotransformation. This can be changed with the methods provided by
     * [OGRSpatialReference](http://www.gdal.org/classOGRSpatialReference.html) from GDAL. Validity
     * can be checked with `o.geotransSRS.Validate() == OGRERR_NONE`.
     *
     * Note, either geotransformation + the geotransform projection coordinate system (@ref
     * geotransSRS) _or_ ground control points + the GCP projection coordinate system (@ref gcpSRS)
     * can be used. They are mutual exclusive ways for georeferencing an image. To use
     * geotransformation specify the transformation with the corresponding methods and provide a
     * valid spatial reference before using @ref addTo.
     *
     * @see geotrans, GeoTransform::set, GeoTransform::clear
     * GeoTransform::scaleProjection, GeoTransform::scaleImage,
     * GeoTransform::rotateProjection, GeoTransform::rotateImage,
     * GeoTransform::shearXProjection, GeoTransform::shearXImage,
     * GeoTransform::shearYProjection, GeoTransform::shearYImage,
     * GeoTransform::translateProjection, GeoTransform::translateImage
     */
    OGRSpatialReference geotransSRS;


    /**
     * @brief Affine geo transformation
     *
     * \f[
     *   \begin{pmatrix}
     *     x_p\\
     *     y_p
     *   \end{pmatrix}
     *   =
     *   \begin{pmatrix}
     *     x_o\\
     *     y_o
     *   \end{pmatrix}
     *   +
     *   \begin{pmatrix}
     *     A_{xx} & A_{yx}\\
     *     A_{xy} & A_{yy}
     *   \end{pmatrix}
     *   \begin{pmatrix}
     *     x_i\\
     *     y_i
     *   \end{pmatrix},
     * \f]
     *
     * @see GeoTransform
     */
    GeoTransform geotrans;


    /**
     * @brief Values to mark parts of the image as no-data
     *
     * A no-data value marks pixels, which have this value as invalid. The pixels with this value
     * do not contain image information. E. g. if the border of an image is not rectangular it
     * could be filled with such a value. Note, the specified value cannot be used to represent
     * image information and to mark invalid pixels at the same time. The user that sets this value
     * or applies the information with @ref addTo to a new (fused) image has to make sure that this
     * value is not used in the image or has to replace it before by a similar value if that is an
     * option.
     *
     * A mask can be created from this vector and an Image `img`. Make sure before that
     * nodataValues has either 1 element or the so many elements as the image channels has. Then,
     * assuming the GeoInfo object `gi`:
     * @code
     * std::vector<Interval> nodataVals;
     * for (double d : gi.nodataValues)
     *     nodataVals.push_back(Interval::closed(d, d));
     * Image mask = img.createSingleChannelMaskFromRange(nodataVals);
     * @endcode
     * or
     * @code
     * Image mask = img.createMultiChannelMaskFromRange(nodataVals);
     * @endcode
     * depending on what you want to achieve.
     *
     * Since every band / channel could have a different no-data value, these values are saved in a
     * vector, which is indexed by the channel. However, this seems not to be common and if only a
     * single no-data value (e. g. on channel 0) is specified it will be used for all channels.
     *
     * Values that are NaN (check with std::isnan) count as not set. Note the first channel index
     * is 0 in imagefusion (in GDAL the band index starts with 1)!
     *
     * Note, for images with color tables `nodataValues` contains the color index and you can
     * convert it using @ref colorTable like
     * @code
     * GeoInfo gi{"some-image-file.tif"};
     * std::array<short,4> const& nodataEntry = gi.colorTable.at(gi.getNodataValue());
     * @endcode
     * To find the relevant entries you need to use the indices (0, ..., 3; RGBA) according to the
     * table at @ref colorTable, but usually you will just take the first value
     * `nodataEntry.front()`.
     */
    std::vector<double> nodataValues;




    /**
     * @brief Metadata information
     *
     * Metadata is structured in domains. Each domain has metadata items. These items consist of a
     * key and a value. This structure fits well to the scheme `metadata[domain][key] = value`.
     */
    std::map<std::string, std::map<std::string, std::string>> metadata;


    /**
     * @brief Contruct empty geo information
     *
     * This sets an identity geotransform, which would not be written when using @ref addTo (even
     * if the projection coordinate system would have been set). An identity transform is
     * considered as empty.
     *
     * @see addGCP, getGeotransSpatialReference, getGCPSpatialReference
     */
    GeoInfo();

    /**
     * @brief Copy construct a GeoInfo object
     * @param gi is the source object to copy.
     */
    GeoInfo(GeoInfo const& gi) noexcept;

    /**
     * @brief Move construct a GeoInfo object
     * @param gi is the source object, which is in an undefined state afterwards.
     */
    GeoInfo(GeoInfo&& gi) noexcept;

    /**
     * @brief Copy and move assignment operator
     * @param gi is the source object.
     */
    GeoInfo& operator=(GeoInfo gi) noexcept;

    /**
     * @brief Contruct with information from the given file
     *
     * @param filename of an image to read all possible information from.
     *
     * If possible, the following information is extracted:
     *  - a value to mark invalid data which can be used to build a mask (no-data value),
     *  - arbitrary metadata structured in domains and then key-value pairs and
     *  - projection coordinate system together with either
     *    - ground control points or
     *    - coefficients that define an affine transform (geotransform)
     *
     * @throws runtime_error if `filename` cannot be opened with any GDAL driver or does not exist.
     */
    explicit GeoInfo(std::string const& filename);


    /**
     * @brief Contruct with information from the given channels of the file
     *
     * @param filename of an image to use.
     *
     * @param channels to use. These can also be subdataset numbers (0-based!). Specifying an empty
     * vector means all channels.
     *
     * @param crop is a rectangle to limit the image size and offset (in image space coordinates).
     * Note, this crop rectangle gets limited by the image boundaries. If you want to refer to
     * image coordinates outside the image you have to use @ref GeoTransform::translateImage and
     * @ref GeoInfo::size. `crop` is specified in unflipped image space.
     *
     * @param flipH sets whether to read the image geotransformation flipped horizontally. This is
     * equivalent to using @ref GeoTransform::flipImage afterwards.
     *
     * @param flipV sets whether to read the image geotransformation flipped vertically. This is
     * equivalent to using @ref GeoTransform::flipImage afterwards.
     *
     * So in case of normal images, this is similar to GeoInfo(std::string const& filename). But
     * the `channels` member will be set to the size of the `channels` argument given, if it is not
     * empty and otherwise to the number of bands contained in the image.
     *
     * In case of a multi-image-file, like HDF images, channels refer to subdatasets. These may
     * have different resolutions, in which case the resulting resolution will be the highest
     * across the subdatasets. The selected subdatasets must have the same data types, projection
     * coordinate systems, etc. The GeoInfo object will have the properties of an Image read with
     * the same channels argument. When the data types of the subdatasets to combine are different
     * and building the virtual dataset succeeds, baseType of the constructed object will be @ref
     * Type::invalid.
     *
     * @note In case of a multi-image-file (or: container file) a call with an empty channels
     * argument is *not* equivalent to just calling GeoInfo(std::string const& filename)! The
     * latter will contain the information of the container file, while the former will try to load
     * and combine all subdatasets.
     *
     * @note The meta data of a multi-image-file with combined channels will be shortened a lot. To
     * retrieve the meta data of the single subdatasets get the GeoInfo of the container and
     * retrieve the GeoInfo%s of the children, like:
     * @code
     * GeoInfo parent{"example.hdf"}; // contains a lot of parent meta data
     * GeoInfo sds1 = parent.subdatasetGeoInfo(0); // infos of first subdataset
     * @endcode
     * Make sure the subdataset exists with @ref subdatasetsCount().
     *
     * Images with indexed colors always are of @ref baseType @ref Type::uint8 and have one @ref
     * channels and @ref colorTable will hold the table. To check whether it is an image with
     * indexed colors, check whether @ref colorTable is not empty. The resulting number of channels
     * in the image depends on the colors that occur in the image. So the full image data is
     * required to determine the real number of channels. Thus GeoInfo will not show the resulting
     * number of channels. Also if the color table is ignored when reading the image, this will
     * result in a single-channel (indexed) image. To handle both situations the @ref nodataValues
     * "nodata value" will be the color index and the corresponding color entry can be accessed by
     * `gi.colorTable.at(gi.getNodataValue())` (assuming `gi` is a GeoInfo object). For a
     * discussion about how to handle color tables, have a look at the documentation of @ref
     * addTo(std::string const& filename) const.
     *
     * @throws runtime_error if `filename` cannot be opened with any GDAL driver or does not exist.
     * Also if the GDAL VRT file cannot be built. This happens sometimes if the subdatasets to
     * combine have different data types or projection coordinate systems. This is only relevant,
     * if filename is a multi-image-file.
     *
     * @throws image_type_error if the channels are out of bounds and thus do not fit to the image
     */
    explicit GeoInfo(std::string const& filename, std::vector<int> const& channels, Rectangle crop = {0, 0, 0, 0}, bool flipH = false, bool flipV = false);

    /**
     * @brief Read in all available information from a specified image
     *
     * \copydetails GeoInfo(std::string const& filename)
     */
    void readFrom(std::string const& filename);


    /**
     * @brief Read in all available information from a GDALDataset
     *
     * If possible, the following information is extracted:
     *  - a value to mark invalid data which can be used to build a mask (no-data value),
     *  - arbitrary metadata structured in domains and then key-value pairs and
     *  - projection coordinate system together with either
     *    - ground control points or
     *    - coefficients that define an affine transform (geotransform)
     */
    void readFrom(GDALDataset const* ds);


    /**
     * @brief Add geoinformation to an existing image file
     *
     * @param filename to an existing image
     *
     * The information, which the calling GeoInfo object contains, is added to the specified image.
     * The image must exist already when writing these information to it and the image driver must
     * support update of metadata. To make it clear, this is the only function, which adds or
     * updates information in an image file. All the other methods only affect the GeoInfo object.
     *
     * Note, that write support for color tables is rather limited. The exact behaviour depends on
     * the drivers. GTiff will change the alpha value of all non-nodata-entries to 255 and of the
     * nodata entry to 0. PNG will only change the alpha value of the nodata entry to 0, but leave
     * the remaining values as they are. This might have bad consequences when expanding the file
     * on the next read: the number of channels might have changed. You can remove the color table
     * in these cases:
     * @code
     * Image img{..., true}; // read in image ignoreColorTable set to true
     * GeoInfo gi{...}
     *
     * // write image file and add GeoInfo
     * img.write(outfilename);
     * gi.addTo(outfilename);
     * // or combined
     * img.write(outfilename, gi);
     *
     * // test color table
     * GeoInfo test{outfilename};
     * bool ctOK = gi.compareColorTables(test);
     * if (!ctOK) {
     *     gi.colorTable.clear();
     *     gi.addTo(outfilename);
     * }
     * @endcode
     * You probably also want to clear the color table when colors got expanded. This is even
     * required if the image expanded to a single-channel uint8 image. Also do not forget to expand
     * the nodata value:
     * @code
     * Image img{...}; // read in image ignoreColorTable set to false (default)
     * GeoInfo gi{...}
     *
     * // remove color table, but expand nodata value before
     * if (!gi.colorTable.empty() && gi.hasNodataValue())
     *     gi.setNodataValue(gi.colorTable.at(gi.getNodataValue()).front());
     * gi.colorTable.clear();
     *
     * // write image file with GeoInfo
     * img.write(outfilename, gi);
     * @endcode
     *
     * @throws runtime_error if the driver could not open the image for update. As an example:
     * currently GTiff works, PNG does not. However, PNG files can be written with GeoInfo%s with
     * Image::write
     */
    void addTo(std::string const& filename) const;


    /**
     * @brief Add geoinformation to a GDAL Dataset.
     *
     * @param ds is the dataset to which the GeoInfo will be written.
     *
     * This adds the GeoInfo to the specified dataset. This is for example useful after opening an
     * Image as dataset with Image::asGDALDataset():
     * @code
     * Image img = // ...
     * GDALDataset* ds = img.asGDALDataset();
     * gi.addTo(ds);
     * @endcode
     */
    void addTo(GDALDataset* ds) const;


    /**
     * @brief Open a virtual GDAL dataset from the metadata contained in this GeoInfo object
     *
     * @param channels to use. These can also be subdataset numbers (0-based!). Specifying an empty
     * vector means all channels.
     *
     * @param interp specifies the interpolation method in case the pixels are read from the
     * dataset.
     *
     * @return a virtual dataset handler combining the specified channels. However if only one
     * channel was specified the dataset will not be virtual, which preserves all metadata of the
     * subdataset.
     *
     * @throws runtime_error when the virtual dataset cannot be composed, e. g. due to different
     * types or projection systems.
     *
     * @throws image_type_error if the channels are out of bounds or if there are no datasets at
     * all.
     */
    GDALDataset* openVrtGdalDataset(std::vector<int> const& channels, InterpMethod interp = InterpMethod::bilinear) const;


    /**
     * @brief Find common rectangular region with another GeoInfo object
     *
     * @param other is another GeoInfo object to intersect with. It describes the area and the
     * projection coordinate space.
     *
     * @param numPoints is the number of points on each boundary that will be projected to find
     * the extents. Due to non-linear transformations it is not enough to take the corners.
     *
     * @param shrink determines if the extents will be shrinked (default) or enlarged to full pixel
     * size.
     *
     * This will use the boundaries from the invoking object, project them to the other GeoInfo
     * object, limit them by its boundaries and project them back. This is more accurate than
     * projecting the boundaries of `other` directly to the projection space of the invoking object
     * due to the non-linear transformation. The resulting rectangle is chosen to contain all the
     * points, but will finally be shrinked to full pixels. If you need the precise intersection
     * (not shrinked), see @ref intersectRect(GeoInfo const&, CoordRectangle const&, GeoInfo const&,
     * CoordRectangle const&, unsigned int) and @ref projRect().
     *
     * The extents are taken from GeoTransform::offsetX and GeoTransform::offsetY in @ref geotrans
     * and @ref size and also saved there. In case of an empty intersection the invoking's size
     * will be `{0, 0}` afterwards. If you want to crop the `other` GeoInfo object, for example to
     * x: 100, y: 200, w: 300, h: 400, you can do this with:
     * @code
     * other.geotrans.translateImage(100, 200);
     * other.size.width  = 300;
     * other.size.height = 400;
     * giTarget.intersect(other);
     * @endcode
     */
    void intersect(GeoInfo const& other, unsigned int numPoints = 33, bool shrink = true);


    /**
     * @brief Transform source image coordinate to destination projection space
     *
     * @param c_i is the source image space coordinate, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS.
     *
     * This basically transforms
     * \f[ c_{i,\emph{src}} \mapsto c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \f]
     * or visualized:
     * @image html imgToProj.png
     *
     * @return the corresponding projection space coordinate
     *
     * @see Use imgToProj(std::vector<Coordinate> const& c_i, GeoInfo const& to) const
     * to convert multiple coordinates at once for efficiency. For other transformations see also
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const
     */
    Coordinate imgToProj(Coordinate const& c_i, GeoInfo const& to) const;

    /**
     * @brief Transform multiple source image coordinates to destination projection space
     *
     * @param c_i is a vector of source image space coordinates, see @ref GeoTransform for details
     * about image space coordinates.
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS.
     *
     * Transforming multiple coordinates in one go is more efficient than one by one. This
     * basically transforms
     * \f[ c_{i,\emph{src}} \mapsto c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \f]
     * or visualized:
     * @image html imgToProj.png
     *
     * @return the corresponding projection space coordinates
     *
     * @see For other transformations see also
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const,
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const
     */
    std::vector<Coordinate> imgToProj(std::vector<Coordinate> const& c_i, GeoInfo const& to) const;

    /**
     * @brief Transform source projection coordinate to destination image space
     *
     * @param c_p is the source projection space coordinate
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS and the transformation to image coordinates with geotrans.
     *
     * This basically transforms
     * \f[ c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \mapsto c_{i,\emph{dst}} \f]
     * or visualized:
     * @image html projToImg.png
     *
     * @return the corresponding image space coordinate, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @see Use projToImg(std::vector<Coordinate> const& c_p, GeoInfo const& to) const
     * to convert multiple coordinates at once for efficiency. For other transformations see also
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const,
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const
     */
    Coordinate projToImg(Coordinate const& c_p, GeoInfo const& to) const;

    /**
     * @brief Transform multiple source projection coordinates to destination image space
     *
     * @param c_p is a vector of source projection space coordinates
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS and the transformation to image coordinates with geotrans.
     *
     * This basically transforms
     * \f[ c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \mapsto c_{i,\emph{dst}} \f]
     * or visualized:
     * @image html projToImg.png
     *
     * @return the corresponding image space coordinates, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @see
     * For other transformations see also
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const,
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const
     */
    std::vector<Coordinate> projToImg(std::vector<Coordinate> const& c_p, GeoInfo const& to) const;

    /**
     * @brief Transform source image coordinate to destination image space
     *
     * @param c_i is the source image space coordinate, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS and the transformation to image coordinates with geotrans.
     *
     * This basically transforms
     * \f[ c_{i,\emph{src}} \mapsto c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \mapsto c_{i,\emph{dst}} \f]
     * or visualized:
     * @image html imgToImg.png
     *
     * @return the corresponding image space coordinate, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @see Use imgToImg(std::vector<Coordinate> const& c_i, GeoInfo const& to) const
     * to convert multiple coordinates at once for efficiency. For other transformations see also
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const,
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const
     */
    Coordinate imgToImg(Coordinate const& c_i, GeoInfo const& to) const;

    /**
     * @brief Transform multiple source image coordinates to destination image space
     *
     * @param c_i is a vector of source image space coordinates, see @ref GeoTransform for details
     * about image space coordinates.
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS and the transformation to image coordinates with geotrans.
     *
     * This basically transforms
     * \f[ c_{i,\emph{src}} \mapsto c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \mapsto c_{i,\emph{dst}} \f]
     * or visualized:
     * @image html imgToImg.png
     *
     * @return the corresponding image space coordinates, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @see
     * For other transformations see also
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const,
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const
     */
    std::vector<Coordinate> imgToImg(std::vector<Coordinate> const& c_i, GeoInfo const& to) const;

    /**
     * @brief Transform source projection coordinate to destination projection space
     *
     * @param c_p is the source projection space coordinate
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS.
     *
     * This basically transforms
     * \f[ c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \f]
     * or visualized:
     * @image html projToProj.png
     *
     * @return the corresponding projection space coordinate
     *
     * @see Use projToProj(std::vector<Coordinate> const& c_i, GeoInfo const& to) const
     * to convert multiple coordinates at once for efficiency. For other transformations see also
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const
     */
    Coordinate projToProj(Coordinate const& c_p, GeoInfo const& to) const;

    /**
     * @brief Transform multiple source projection coordinates to destination projection space
     *
     * @param c_p is a vector of source projection space coordinate
     *
     * @param to is the destination geo info object, that defines the projection coordinate system
     * with its geotransSRS.
     *
     * This basically transforms
     * \f[ c_{p,\emph{src}} \mapsto c_{p,\emph{dst}} \f]
     * or visualized:
     * @image html projToProj.png
     *
     * @return the corresponding projection space coordinates
     *
     * @see
     * For other transformations see also
     * projToProj(Coordinate const& c_p, GeoInfo const& to) const,
     * projToImg(Coordinate const& c_p, GeoInfo const& to) const,
     * imgToImg(Coordinate const& c_i, GeoInfo const& to) const,
     * imgToProj(Coordinate const& c_i, GeoInfo const& to) const
     */
    std::vector<Coordinate> projToProj(std::vector<Coordinate> const& c_p, GeoInfo const& to) const;

    /**
     * @brief Transform image space coordinate to latitude / longitude
     *
     * @param c_i is the source image space coordinate, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @return the corresponding geographic coordinates, where the latitude is returned in y and
     * the longitude in x, both in degree.
     *
     * @see Use imgToLongLat(std::vector<Coordinate> const& c_i) const
     * to convert multiple coordinates at once for efficiency.
     * For the other transformations with longitude / latitude see also
     * longLatToProj(Coordinate const& c_l) const,
     * longLatToImg(Coordinate const& c_l) const,
     * projToLongLat(Coordinate const& c_p) const
     */
    Coordinate imgToLongLat(Coordinate const& c_i) const;

    /**
     * @brief Transform multiple image space coordinates to latitude / longitude
     *
     * @param c_i is a vector of source image space coordinates, see @ref GeoTransform for details
     * about image space coordinates.
     *
     * @return the corresponding geographic coordinates, where the latitude is returned in y and
     * the longitude in x, both in degree.
     *
     * @see
     * For the other transformations with longitude / latitude see also
     * projToLongLat(Coordinate const& c_p) const,
     * longLatToProj(Coordinate const& c_l) const,
     * longLatToImg(Coordinate const& c_l) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    std::vector<Coordinate> imgToLongLat(std::vector<Coordinate> const& c_i) const;

    /**
     * @brief Transform projection coordinate to latitude / longitude
     * @param c_p is the source projection space coordinate
     *
     * @return the corresponding geographic coordinates, where the latitude is returned in y and
     * the longitude in x, both in degree.
     *
     * @see Use projToLongLat(std::vector<Coordinate> const& c_i) const
     * to convert multiple coordinates at once for efficiency.
     * For the other transformations with longitude / latitude see also
     * longLatToProj(Coordinate const& c_l) const,
     * longLatToImg(Coordinate const& c_l) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    Coordinate projToLongLat(Coordinate const& c_p) const;

    /**
     * @brief Transform multiple projection coordinates to latitude / longitude
     * @param c_p is a vector of source projection space coordinates
     *
     * @return the corresponding geographic coordinates, where the latitude is returned in y and
     * the longitude in x, both in degree.
     *
     * @see
     * For the other transformations with longitude / latitude see also
     * projToLongLat(Coordinate const& c_p) const,
     * longLatToProj(Coordinate const& c_l) const,
     * longLatToImg(Coordinate const& c_l) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    std::vector<Coordinate> projToLongLat(std::vector<Coordinate> const& c_p) const;

    /**
     * @brief Transform latitude / longitude to projection space coordinate
     *
     * @param c_l is the source latitude / longitude coordinate with latitude in y and longitude in
     * x, both in degree.
     *
     * @return the corresponding projection space coordinates.
     *
     * @see Use longLatToProj(std::vector<Coordinate> const& c_l) const
     * to convert multiple coordinates at once for efficiency.
     * For the other transformations with longitude / latitude see also
     * projToLongLat(Coordinate const& c_p) const,
     * longLatToImg(Coordinate const& c_l) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    Coordinate longLatToProj(Coordinate const& c_l) const;

    /**
     * @brief Transform multiple latitude / longitude to projection space coordinate
     *
     * @param c_l is a vector source latitude / longitude coordinates with latitude in y and
     * longitude in x, both in degree.
     *
     * @return the corresponding projection space coordinates.
     *
     * @see
     * For the other transformations with longitude / latitude see also
     * projToLongLat(Coordinate const& c_p) const,
     * longLatToProj(Coordinate const& c_l) const,
     * longLatToImg(Coordinate const& c_l) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    std::vector<Coordinate> longLatToProj(std::vector<Coordinate> const& c_l) const;

    /**
     * @brief Transform latitude / longitude to image space coordinate
     *
     * @param c_l is the source latitude / longitude coordinate with latitude in y and longitude in
     * x, both in degree.
     *
     * @return the corresponding image space coordinate, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @see Use longLatToImg(std::vector<Coordinate> const& c_l) const
     * to convert multiple coordinates at once for efficiency.
     * For the other transformations with longitude / latitude see also
     * longLatToProj(Coordinate const& c_l) const,
     * projToLongLat(Coordinate const& c_p) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    Coordinate longLatToImg(Coordinate const& c_l) const;

    /**
     * @brief Transform multiple projection coordinates to latitude / longitude
     *
     * @param c_l is a vector of source latitude / longitude coordinates with latitude in y and
     * longitude in x, both in degree.
     *
     * @return the corresponding image space coordinates, see @ref GeoTransform for details about
     * image space coordinates.
     *
     * @see
     * For the other transformations with longitude / latitude see also
     * projToLongLat(Coordinate const& c_p) const,
     * longLatToProj(Coordinate const& c_l) const,
     * longLatToImg(Coordinate const& c_l) const,
     * imgToLongLat(Coordinate const& c_i) const
     */
    std::vector<Coordinate> longLatToImg(std::vector<Coordinate> const& c_l) const;


    /**
     * @brief The top-left image coordinate (0, 0) in projection coordinate space
     * @return basically `geotrans.imgToProj(Coordinate(0, 0))`
     */
    Coordinate projCornerTL() const {
        return Coordinate(geotrans.offsetX, geotrans.offsetY);
    }


    /**
     * @brief The bottom-right image coordinate (width(), height()) in projection coordinate space
     * @return basically `geotrans.imgToProj(Coordinate(width(), height()))`
     */
    Coordinate projCornerBR() const {
        return geotrans.imgToProj(Coordinate(size));
    }


    /**
     * @brief The geo extents in projection space as rectangle
     *
     * The rectangle has an offset in CoordRectangle::x and CoordRectangle::y and a
     * CoordRectangle::width and CoordRectangle::height. The offset is just the minimum of the
     * coordinates, which does not correspond to the top left corner. So CoordRectangle::tl() will
     * not give the top left corner of the image, but the minimum of the coordinates and
     * CoordRectangle::br() will give the maximum.
     *
     * A CoordRectangle provides also an alternative way of intersecting areas of the same
     * projection space, and this even to more than full pixel precision. You can simply use the
     * `&` operator. This example illustrates it, assuming `gi1` and `gi2` are GeoInfo objects:
     * @code
     * // projection coordinate spaces must be the same, otherwise intersection is more complicated due to non-linear transformations
     * assert(gi1.gcpSRS.IsSame(&gi2.gcpSRS) && gi1.geotransSRS.IsSame(&gi2.geotransSRS));
     * CoordRectangle r1 = gi1.projRect();
     * CoordRectangle r2 = gi2.projRect();
     *
     * // intersect r1 and r2
     * CoordRectangle ri = r1 & r2;
     * @endcode
     *
     * @return the extents of the GeoInfo object as rectangle.
     */
    CoordRectangle projRect() const {
        Coordinate corner1 = projCornerTL();
        Coordinate corner2 = projCornerBR();
        return CoordRectangle{std::min(corner1.x,  corner2.x), std::min(corner1.y,  corner2.y),
                              std::abs(corner1.x - corner2.x), std::abs(corner1.y - corner2.y)};
    }


    /**
     * @brief Set a no-data value
     *
     * @param nodataValue is the value to specify invalid pixels. Often -9999.
     *
     * @param channel of the no-data value.
     *
     * Since every band / channel could have different no-data values, the channel can be
     * specified. However, this seems not to be common and if only a single no-data value (e. g. on
     * channel 0) is specified it will be used for all channels.
     *
     * Note the first channel index is 0 in imagefusion (in GDAL the band index starts at 1)!
     *
     * @see nodataValues
     */
    void setNodataValue(double nodataValue, unsigned int channel = 0);


    /**
     * @brief Get a no-data value
     *
     * @param channel of the no-data value
     *
     * For multi-channel images there could be more than one no-data value. In case of indexed
     * color images, the no-data value is also an index. For a detailed explanation see @ref
     * nodataValues
     *
     * @return the corresponding no-data value, that was set before by reading a file or setting it
     * manually. If there is no no-data value for the specified channel this returns NaN.
     */
    double getNodataValue(unsigned int channel = 0) const;


    /**
     * @brief Check if any no-data value is set
     *
     * Note, files have sometime an unused no-data value set. This means the value is not NaN, so
     * it is a proper value, but the image does not use it. In this case this method would still
     * return true, since this value would still be written into an image. This might change in
     * future for non-integer values in integer-based images.
     *
     * @return true if any non-NaN value is set for any channel and false
     * otherwise
     */
    bool hasNodataValue() const;


    /**
     * @brief Clear no-data values
     */
    void clearNodataValues();


    /**
     * @brief Add ground control point to define the geo reference
     *
     * Add a GCP to define the projection from image coordinates to world coordinates / projection
     * space. At least three linear independent GCPs are needed for a linear transformation. Giving
     * more could define a higher order transformation or a regression. To give the projected
     * coordinates a meaning a GCP projection coordinate system is also required. The GCPs are only
     * used in @ref addTo if the CS is valid.
     *
     * Note, either ground control points + the GCP projection coordinate system (@ref gcpSRS) _or_
     * geotransformation + the geotransform projection coordinate system (@ref geotransSRS) can be
     * used. They are mutual exclusive ways for georeferencing an image. To use ground control
     * points add GCPs and provide a valid spatial reference before using @ref addTo. Beware, if
     * also a non-identity geotransformation and a valid geotransform projection CS is specified,
     * the geotransformation takes precedence. You can clear the geotransformation to avoid that.
     *
     * @see clear, addTo, getGCPSpatialReference, setGCPs
     */
    void addGCP(GCP toAdd);


    /**
     * @brief Clear GCPs
     */
    void clearGCPs();


    /**
     * @brief Check if a geotransform is set
     *
     * @return true if the geotransform is non-identity, which would be considered as logically
     * empty and geoSRS is set to something valid, false otherwise.
     */
    bool hasGeotransform() const;


    /**
     * @brief Check if a GCP (ground control points) are used
     *
     * @return true if at least three GCPs are set and gcpSRS is set to something valid, false
     * otherwise.
     */
    bool hasGCPs() const;


    /**
     * @brief Get all metadata domains as vector
     *
     * @return metadata domains as vector, such as the default domain "" or the image structure
     * domain "IMAGE_STRUCTURE".
     */
    std::vector<std::string> getMetadataDomains() const;


    /**
     * @brief Check if a metadata domain is defined
     * @param dom is the domain to check. Can also be an empty string for the default domain.
     * @return true if it defined, false otherwise.
     */
    bool hasMetadataDomain(std::string dom) const;


    /**
     * @brief Get all metadata items of a specific domain
     * @param domain to look for.
     *
     * Each metadata domain includes items, which are key-value pairs.
     *
     * @return a map with metadata items, which can be used like `metaitems[key] == value`. For
     * example in the default domain "" there is often an item with the key "AREA_OR_POINT" which
     * can have the value "Point".
     *
     * @throws not_found_error if `domain` does not exist. Please check before calling with
     * getMetadataDomains() const or hasMetadataDomain(std::string dom) const!
     */
    std::map<std::string, std::string> const& getMetadataItems(std::string const& domain) const;


    /**
     * @brief Set a single metadata item in a specific domain
     * @param domain is the enclosing domain.
     * @param key is the item key.
     * @param value is the item value.
     *
     * If the item or the domain does not yet exist, it will be automatically created.
     */
    void setMetadataItem(std::string const& domain, std::string const& key, std::string const& value);


    /**
     * @brief Remove an existing metadata item
     * @param domain is the enclosing domain.
     * @param key is the item key of the item that will be removed.
     *
     * If the item cannot be found, nothing happens.
     *
     * If the item could be found and was the last item in the domain, the domain is removed as
     * well.
     */
    void removeMetadataItem(std::string const& domain, std::string const& key);


    /**
     * @brief Remove a whole metadata domain
     * @param domain to remove.
     *
     * This will also remove any items in the domain. If the domain cannot be found, nothing
     * happens.
     */
    void removeMetadataDomain(std::string const& domain);


    /**
     * @brief Check if this file is a multi-image-file
     *
     * @return true if it contains the meta data domain "SUBDATASETS", false otherwise. Derived
     * subdatasets are not considered here!
     *
     * @see subdatasetsCount() const
     */
    bool hasSubdatasets() const {
        return hasMetadataDomain("SUBDATASETS");
    }


    /**
     * @brief Number of contained images (subdatasets)
     * @return the number of subdatasets
     * @see hasSubdatasets() const, subdatasets() const,
     * subdatasetGeoInfo(unsigned int idx) const
     */
    unsigned int subdatasetsCount() const {
        if (!hasMetadataDomain("SUBDATASETS"))
            return 0;

        auto const& sds = getMetadataItems("SUBDATASETS");
        return sds.size() / 2; // for each subdataset there is a name and a description item
    }


    /**
     * @brief Get all subdataset names and descriptions
     *
     * The subdataset names and descriptions are not very descriptive. The names contain for
     * example some driver information, the paths to the containing file and more information,
     * including the name one would expect in the end. The different items in the name are
     * separated by colons. Example:
     *
     *     name:
     *     HDF4_EOS:EOS_GRID:"MOD09GA.A2017349.h18v03.006.2017351025813.hdf":MODIS_Grid_1km_2D:num_observations_1km
     *     description:
     *     [1200x1200] num_observations_1km MODIS_Grid_1km_2D (8-bit integer)
     *
     * Note, the names can be used to open a GDAL Dataset or an Image directly:
     * @code
     * std::string filename = "HDF4_EOS:EOS_GRID:\"MOD09GA.A2017349.h18v03.006.2017351025813.hdf\":MODIS_Grid_1km_2D:num_observations_1km";
     * Image i{filename};
     * @endcode
     * However, to open multiple subdataset as one multi-channel image the 0-based indices have to
     * be used in the layer option of the Image constructor. For example to open the subdataset
     * "12" and "13" of the above file as Image use
     * @code
     * std::string filename = "MOD09GA.A2017349.h18v03.006.2017351025813.hdf";
     * std::vector<int> layer{11, 12};
     * Image i{filename, layer};
     * @endcode
     *
     * @return all name-description pairs, where the name is in `first` and the description is in
     * `second`.
     */
    std::vector<std::pair<std::string, std::string>> subdatasets() const {
        if (!hasMetadataDomain("SUBDATASETS"))
            return {};

        auto const& metaSDS = getMetadataItems("SUBDATASETS");
        unsigned int numSDS = metaSDS.size() / 2; // for each subdataset there is a name and a description item
        std::vector<std::pair<std::string, std::string>> vecSDS;
        vecSDS.reserve(numSDS);
        for (unsigned int i = 0; i < numSDS; ++i) {
            std::string const& name = metaSDS.at("SUBDATASET_" + std::to_string(i + 1) + "_NAME");
            std::string const& desc = metaSDS.at("SUBDATASET_" + std::to_string(i + 1) + "_DESC");
            vecSDS.emplace_back(name, desc);
        }
        return vecSDS;
    }


    /**
     * @brief Get a GeoInfo object of the specified subdataset
     *
     * @param idx of the subdataset. This is 0-based, so to get subdataset 1, which is the first
     * subdataset, use 0 as index.
     *
     * Files that contain subdatasets usually do not have geo information, data types, images
     * sizes, etc. since the subdatasets can have different infos. Therefore this function opens
     * the GeoInfo of the indexed subdataset of the current file.
     *
     * @return the GeoInfo of the subdataset
     *
     * @throws file_format_error if the file does not contain any subdatasets
     *
     * @throws invalid_argument_error if `idx` is greater or equal to the number of subdatasets.
     * Note, idx is 0-based!
     *
     * @see GeoInfo
     */
    GeoInfo subdatasetGeoInfo(unsigned int idx) const;


    /**
     * @brief Compare the color tables and optionally print a warning
     * @param other contains the other color table to compare with.
     * @param quiet determines whether warnings are printed.
     *
     * @return true if other's color table is compatible (it may contain more entries) and false if
     * entries are missing or have been changed.
     */
    bool compareColorTables(GeoInfo const& other, bool quiet = true) const;


    /**
     * @brief Set the extents from a rectangle in projection coordinate space
     *
     * @param ex is the rectangle describing the new extents. The size will be shrinked or enlarged
     * (depending on `shrink`) to full pixel size.
     *
     * @param shrink determines if the extents will be shrinked (default) or enlarged to full pixel
     * size.
     *
     * This method will use the rounded extents of the rectangle for this GeoInfo object. However,
     * the rounding allows for some degree of freedom. This can be explained with an example: Say
     * the rectangle `ex` describes the bottom right quarter of the image, but the size cannot be
     * expressed as full pixel, so rounding will make a difference. The question now is which sides
     * will be modified. This method tries would adjust the top and left boundaries so that the
     * bottom-right corner would still be the same. In general it tries to preserve an unchanged
     * corner, but if all corners change, it will just take the north-west corner as offset, which
     * is not the same as `ex.tl()` in general.
     */
    void setExtents(CoordRectangle const& ex, bool shrink = true);


    /**
     * @brief Width of the image
     * @return size.width
     */
    int width() const;


    /**
     * @brief Height of the image
     *
     * @return size.height
     */
    int height() const;


    friend void swap(GeoInfo& g1, GeoInfo& g2) noexcept;
};

/**
 * @brief Intersect two rectangles in different projection coordinate spaces
 *
 * @param ref defines the reference projection space in which the resulting rectangle will be
 * returned.
 *
 * @param refRect is the first rectangle in the reference projection coordinate space.
 *
 * @param other defines the projection coordinate space of the other rectangle.
 *
 * @param otherRect defines the second rectangle in the projection coordinate space of `other`.
 *
 * @param numPoints is the number of points on each boundary that will be projected to find the
 * extents. Due to non-linear transformations it is not enough to take the corners.
 *
 * This will find the enclosing rectangle of the intersection of two rectangles. To find it, this
 * will transform the boundaries of `refRect` into `other`s projection coordinate space, limit them
 * by `otherRect` and project them back. This is more accurate than projecting the boundaries of
 * `otherRect` directly into `ref`s projection space due to the non-linear transformation. The
 * resulting rectangle will contain all the intersecting points.
 *
 * @return the exact rectangle in `ref`s projection coordinate space enclosing the intersection.
 */
CoordRectangle intersectRect(GeoInfo const& ref, CoordRectangle const& refRect, GeoInfo const& other, CoordRectangle const& otherRect, unsigned int numPoints = 33);




inline bool operator==(GeoInfo::GCP const& gcp1, GeoInfo::GCP const& gcp2) {
    return gcp1.id    == gcp2.id &&
           gcp1.info  == gcp2.info &&
           gcp1.pixel == gcp2.pixel &&
           gcp1.line  == gcp2.line &&
           gcp1.x     == gcp2.x &&
           gcp1.y     == gcp2.y &&
           gcp1.z     == gcp2.z;
}

inline bool operator!=(GeoInfo::GCP const& gcp1, GeoInfo::GCP const& gcp2) {
    return !(gcp1 == gcp2);
}


inline bool operator==(GeoInfo const& gi1, GeoInfo const& gi2) {
    return gi1.size         == gi2.size &&
           gi1.baseType     == gi2.baseType &&
           gi1.channels     == gi2.channels &&
           gi1.filename     == gi2.filename &&
           gi1.colorTable   == gi2.colorTable &&
           gi1.gcps         == gi2.gcps &&
           gi1.geotrans     == gi2.geotrans &&
           gi1.nodataValues == gi2.nodataValues &&
           gi1.metadata     == gi2.metadata &&
           gi1.gcpSRS.IsSame(&gi2.gcpSRS) &&
           gi1.geotransSRS.IsSame(&gi2.geotransSRS);
}

inline bool operator!=(GeoInfo const& gi1, GeoInfo const& gi2) {
    return !(gi1 == gi2);
}



inline GeoInfo::GeoInfo() {
    GDALAllRegister();
}

inline GeoInfo::GeoInfo(GeoInfo const& gi) noexcept
    : size{gi.size},
      channels{gi.channels},
      baseType{gi.baseType},
      filename{gi.filename},
      colorTable{gi.colorTable},
      gcpSRS{gi.gcpSRS},
      gcps{gi.gcps},
      geotransSRS{gi.geotransSRS},
      geotrans{gi.geotrans},
      nodataValues{gi.nodataValues},
      metadata{gi.metadata}
{
}

inline GeoInfo::GeoInfo(GeoInfo&& gi) noexcept
    : GeoInfo()
{
    swap(*this, gi);
}

inline GeoInfo& GeoInfo::operator=(GeoInfo gi) noexcept {
    swap(*this, gi);
    return *this;
}

inline GeoInfo::GeoInfo(std::string const& filename) : GeoInfo() {
    readFrom(filename);
}

inline void swap(GeoInfo& i1, GeoInfo& i2) noexcept {
    using std::swap;
    swap(i1.filename,     i2.filename);
    swap(i1.baseType,     i2.baseType);
    swap(i1.size,         i2.size);
    swap(i1.channels,     i2.channels);
    swap(i1.colorTable,   i2.colorTable);
    swap(i1.gcps,         i2.gcps);
    swap(i1.gcpSRS,       i2.gcpSRS);
    swap(i1.geotrans,     i2.geotrans);
    swap(i1.geotransSRS,  i2.geotransSRS);
    swap(i1.metadata,     i2.metadata);
    swap(i1.nodataValues, i2.nodataValues);
}

inline GeoTransform::GeoTransform(GeoTransform const& gt) noexcept
    : values{gt.values}
{
}

inline GeoTransform::GeoTransform(GeoTransform&& gt) noexcept
    : GeoTransform()
{
    swap(*this, gt);
}

inline GeoTransform& GeoTransform::operator=(GeoTransform gt) noexcept {
    swap(*this, gt);
    return *this;
}

inline void swap(GeoTransform& i1, GeoTransform& i2) noexcept {
    using std::swap;
    swap(i1.values, i2.values);
}


inline void GeoInfo::setNodataValue(double nodataValue, unsigned int channel) {
    if (nodataValues.size() <= channel)
        nodataValues.resize(channel+1, std::numeric_limits<double>::quiet_NaN());

    nodataValues[channel] = nodataValue;
}


inline bool GeoInfo::hasNodataValue() const {
    // has nodata-value, if it contains a non-NaN value
    return nodataValues.size() > static_cast<unsigned int>(std::count(nodataValues.begin(), nodataValues.end(), std::numeric_limits<double>::quiet_NaN()));
}


inline void GeoInfo::clearNodataValues() {
    nodataValues.clear();
}


inline void GeoInfo::addGCP(GeoInfo::GCP toAdd) {
    gcps.emplace_back(std::move(toAdd));
}


inline void GeoInfo::clearGCPs() {
    gcps.clear();
}

inline std::vector<std::string> GeoInfo::getMetadataDomains() const {
    std::vector<std::string> domains;
    for (auto const& mp : metadata)
        domains.push_back(mp.first);
    return domains;
}

inline bool GeoInfo::hasMetadataDomain(std::string dom) const {
    auto dom_it = metadata.find(dom);
    return dom_it != metadata.end();
}

inline void GeoInfo::setMetadataItem(std::string const& domain, std::string const& key, std::string const& value) {
    metadata[domain][key] = value;
}


inline void GeoInfo::removeMetadataItem(std::string const& domain, std::string const& key) {
    auto dom_it = metadata.find(domain);
    if (dom_it != metadata.end()) {
        auto& items = dom_it->second;
        auto item_it = items.find(key);
        if (item_it != items.end()) {
            items.erase(item_it);

            // if the domain is empty, also remove that
            if (items.empty())
                metadata.erase(dom_it);
        }
    }
}


inline void GeoInfo::removeMetadataDomain(std::string const& domain) {
    auto dom_it = metadata.find(domain);
    if (dom_it != metadata.end()) {
        metadata.erase(dom_it);
    }
}


inline int GeoInfo::width() const {
    return size.width;
}


inline int GeoInfo::height() const {
    return size.height;
}


} /* namespace imagefusion */

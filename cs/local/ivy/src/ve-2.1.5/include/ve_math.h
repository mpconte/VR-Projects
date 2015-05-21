#ifndef VE_MATH_H
#define VE_MATH_H

/** misc
    <p>The ve_math module provides all of the generalized math operations
    (i.e. for 4x4 matrices, vectors of size 3, quaternions) that are required
    for VE.</p>
*/

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

/* sufficient set of matrix ops for doing what we need to do */
/** struct VeMatrix4
    The type for a 4x4 matrix.
*/
typedef struct ve_matrix4 {
  /** member data
      A two-dimensional array of the array's contents nominally in 
      "row-column" order.
  */
  float data[4][4];
} VeMatrix4;

/** struct VeVector3
    The type for a vector of size 3.
*/
typedef struct ve_vector3 {
  /** member data
      A one-dimensional array of the vector's contents.
  */
  float data[3];
} VeVector3;

/** struct VeQuat
    The type for a quaternion.
*/
typedef struct ve_quat {
  /** member data
      A one-dimensional array of the quaternion's values.
      Element 3 is the scalar value, preceeded by the <i>i</i>, 
      <i>j</i>, <i>k</i> values.  Thus it is (<vector>,<scalar>).
  */
  float data[4];
} VeQuat;

/** function veMatrixIdentity
    <p>
    Fills in the given matrix with the identity matrix, i.e.
    <pre>
        [ 1 0 0 0 ]
        [ 0 1 0 0 ]
        [ 0 0 1 0 ]
        [ 0 0 0 1 ]
    </pre>    
    </p>

    @param m
    The identity to reset to the identity.
 */
void veMatrixIdentity(VeMatrix4 *m);

#define VE_X 0
#define VE_Y 1
#define VE_Z 2
#define VE_DEG 0
#define VE_RAD 1

/** function veMatrixRotate
    <p>Fills in the given matrix with a rotation matrix around one of
    the x, y, or z axes.  For example, for a rotation about the z 
    axis, of angle <i>a</i> the result would be:
    <pre>
    [  cos(<i>a</i>)  -sin(<i>a</i>)   0    0  ]
    [  sin(<i>a</i>)   cos(<i>a</i>)   0    0  ]
    [    0        0      1    0  ]
    [    0        0      0    1  ]
    </pre>

    @param m
    The matrix in which to put the generated rotation matrix.
    
    @param axis
    The axis around which to generate the rotation.  This should be
    one of <code>VE_X</code>, <code>VE_Y</code>, or <code>VE_Z</code>.
    
    @param fmt
    The format in which the angle is being specified.  This should be
    either <code>VE_DEG</code> or <code>VE_RAD</code>.

    @param val
    The angle to rotate by.
*/
void veMatrixRotate(VeMatrix4 *m, int axis, int fmt, float val);

/** function veMatrixRotateArb
    <p>Fills in the given matrix with a rotation matrix around an
    arbitrary matrix.  If the axis is represented by the unit vector
    (<i>x</i>,<i>y</i>,<i>z</i>) and the rotation is around angle <i>a</i>
    with <i>c = cos(a)</i> and <i>s = sin(a)</i>,
    then the generated matrix would be:
    <pre>
        [ x*x+c*(1-x*x)  x*y*(1-c)-z*s  x*z*(1-c)+x*s  0 ]
        [ x*y*(1-c)+z*s  y*y+c*(1-y*y)  y*z*(1-c)-x*s  0 ]
        [ x*z*(1-c)-y*s  y*z*(1-c)+x*s  z*z+c*(1-z*z)  0 ]
        [       0              0                0      1 ]
    </pre>
    </p>

    @param m
    The matrix in which to put the generated rotation matrix.
    
    @param u
    The axis around which to rotate.  This vector must have a magnitude
    of 1 - i.e. it must be a unit vector.

    @param fmt
    The format in which the angle of rotation is specified.  This must
    be either <code>VE_DEG</code> or <code>VE_RAD</code>.
    
    @param val
    The angle by which to rotate.
*/
void veMatrixRotateArb(VeMatrix4 *m, VeVector3 *u, int fmt, float val);

/** function veMatrixTranslate
    <p>Fills in the given matrix with a translation matrix along
    the given vector.  For vector (<i>x</i>,<i>y</i>,<i>z</i>) the
    resulting matrix would be:
    <pre>
        [  1  0  0  x  ]
        [  0  1  0  y  ]
        [  0  0  1  z  ]
        [  0  0  0  1  ]
    </pre>
    </p>

    @param m
    The matrix in which to put the generated translation matrix.

    @param v
    The vector along which to translate.

    @param scale
    A scalar value by which to multiply the given vector before
    generating the translation.  This multiplication does not
    affect the stored value of the vector.
 */
void veMatrixTranslate(VeMatrix4 *m, VeVector3 *v, float scale);

/** function veMatrixMult
    <p>Multiplies two 4x4 matrices.</p>

    @param a
    The left matrix of the multiplication.
    
    @param b
    The right matrix of the multiplication.

    @param c
    The matrix where the result will be stored, i.e. <i>c = ab</i>.
*/
void veMatrixMult(VeMatrix4 *a, VeMatrix4 *b, VeMatrix4 *c);

/** function veMatrixMultPoint
    <p>Transforms a vector of length 3 using a 4x4 matrix.  The effective
    calculation is:
    <pre>
        out = mv
    </pre>
    </p>
    
    @param m
    The matrix by which to multiply the point.

    @param v
    The vector which represents the point you are multiple.

    @param out
    The vector in which the result will be stored.
    
    @param scale
    If not NULL, then the float pointed to by this value will be filled
    with the scaling factor (i.e. the 4th value generated by the 
    multiplication).
*/
void veMatrixMultPoint(VeMatrix4 *m, VeVector3 *v, VeVector3 *out, 
		       float *scale);

/** function veVectorCross
    <p>Calculates the cross-product of two 3-dimensional vectors.
    The effective calculation is:
    <pre>
        c = a x b
    </pre>
    </p>
    
    @param a
    The left vector in the cross-product.

    @param b
    The right vector in the cross-product.

    @param c
    The vector where the result of the operation will be stored.
*/
void veVectorCross(VeVector3 *a, VeVector3 *b, VeVector3 *c);

/** function veVectorDot
    <p>Calculates the dot-product (or inner-product) of a pair of
    3-dimensional vectors.</p>

    @param a,b
    The operands of the dot-product.

    @returns
    The value of the dot-product.
*/
float veVectorDot(VeVector3 *a, VeVector3 *b);

/** function veVectorMag
    <p>Calculates the magnitude of a vector.
    
    @param v
    The vector for which to return the magnitude.
    
    @returns
    The magnitude of the given vector.
*/
float veVectorMag(VeVector3 *v);

/** function veVectorNorm
    <p>Normalizes a 3-dimension vector so that its magnitude is 1.
    If the magnitude of the vector is initially 0 then a warning
    will be generated, but no other action will be taken.  The
    vector is changed in-place.</p>
    
    @param v
    The vector to be normalized.
*/
void veVectorNorm(VeVector3 *v);

/** function veQuatNorm
    <p>Normalizes a quaternion so that its magnitude is 1.
    If the magnitude of the quaternion is 0 then a warning will
    be generated, but no other action will be taken.  The quaternion
    is changed in-place.</p>

    @param q
    The quaternion to be normalized.
*/
void veQuatNorm(VeQuat *q);

/** function veQuatToRotMatrix
    <p>Converts a quaternion into its corresponding rotation matrix.</p>
    
    @param m
    The matrix in which the rotation matrix is to be stored.
    
    @param q
    The quaternion from the rotation matrix is to be derived.
*/
void veQuatToRotMatrix(VeMatrix4 *m, VeQuat *q);

/** function veRotMatrixToQuat
    <p>Converts the rotation component of a matrix into a quaternion.</p>
    
    @param q
    The structure in which the derived quaternion will be stored.

    @param m
    The matrix from which the quaternion will be derived.

    @returns
    0 if successful, non-zero if the diagonal of the matrix is degenerate.
*/
void veRotMatrixToQuat(VeQuat *q, VeMatrix4 *m);

/** function veQuatMult
    <p>Multiplies to quaternions.</p>

    @param a,b
    The two quaternions to multiply.

    @param c_ret
    Where the result is stored.
*/
void veQuatMult(VeQuat *a, VeQuat *b, VeQuat *c_ret);

/** function veQuatFromArb
    <p>Given a rotation axis and an angle about that axis, create a
    quaternion.</p>

    @param axis
    The axis about which to rotate.
    
    @param fmt
    The format of the angle (VE_RAD or VE_DEG).

    @param ang
    The amount to rotate by.

    @param q_ret
    Where the result is stored.
*/
void veQuatFromArb(VeQuat *q_ret, VeVector3 *axis, int fmt, float ang);

/** function veQuatToArb
    <p>Given a quaternion, generate the corresponding axis and rotation
    angle about that axis.</p>

    @param axis_ret
    The resulting axis.
    
    @param fmt
    The format in which the angle should be returned (VE_RAD or VE_DEG).

    @param ang_ret
    The resulting angle.

    @param q
    Where the result is stored.
*/
void veQuatToArb(VeVector3 *axis_ret, float *ang_ret, int fmt, VeQuat *q);

/** function veMatrixLookAt
    <p>Generates a "look-at" matrix.  The arguments specify where
    the camera is located, which direction it is looking in, and which 
    direction is up.  The generated matrix converts from world-space 
    co-ordinates to camera-space co-ordinates.</p>

    @param m
    The matrix where the result is to be stored.

    @param loc
    The origin of the camera.

    @param dir
    The direction in which the camera is looking.

    @param up
    A vector in the direction of up.  This vector must not be parallel
    to the <i>dir</i> vector need not be perpendicular.
*/
void veMatrixLookAt(VeMatrix4 *m, VeVector3 *loc, VeVector3 *dir, 
		    VeVector3 *up);

/** function veMatrixInvLookAt
    <p>Generates the inverse of a "look-at" matrix.  The arguments are
    the same as for generating a "look-at" matrix, but converts from
    camera-space to world-space instead.</p>

    @param m
    The matrix where the result is to be stored.

    @param loc
    The origin of the camera.

    @param dir
    The direction in which the camera is looking.

    @param up
    A vector in the direction of up.  This vector must not be parallel
    to the <i>dir</i> vector need not be perpendicular.
*/
void veMatrixInvLookAt(VeMatrix4 *m, VeVector3 *loc, VeVector3 *dir, 
		       VeVector3 *up);

/** function veMatrixFrustum
    <p>Generates a frustum projection matrix.</p>
    
    @param m
    The matrix in which the generated matrix is stored.

    @param left
    The location of the left clipping plane (along the x-axis) in the near
    z-clip plane (as specified by the <i>near</i> argument).

    @param right
    The location of the right clipping plane (along the x-axis) in the near
    z-clip plane (as specified by the <i>near</i> argument).

    @param bottom
    The location of the bottom clipping plane (along the y-axis) in the near
    z-clip plane (as specified by the <i>near</i> argument).

    @param top
    The location of the top clipping plane (along the y-axis) in the near
    z-clip plane (as specified by the <i>near</i> argument).

    @param near
    The location of the near clipping plane (along the z-axis).

    @param far
    The location of the far clipping pplane (along the z-axis).
*/
void veMatrixFrustum(VeMatrix4 *m, float left, float right, float bottom,
		     float top, float near, float far);

/** function veMatrixPrint
    <p>Writes out the contents of the given matrix on the given standard
    I/O channel.  Used primarily for debugging.</p>

    @param m
    The matrix to print.

    @param f
    The channel on which to print the matrix.
*/
void veMatrixPrint(VeMatrix4 *m, FILE *f);

/** function veVectorPrint
    <p>Writes out the contents of the given vector on the given standard
    I/O channel.  Used primarily for debugging.</p>

    @param v
    The vector to print.

    @param f
    The channel on which to print the vector.
*/
void veVectorPrint(VeVector3 *v, FILE *f);

/** function veQuatPrint
    <p>Writes out the contents of the given quaternion on the given
    standard I/O channel.  Used primarily for debugging.</p>
    
    @param q
    The quaternion to print.

    @param f
    The channel on which to print the vector.
*/
void veQuatPrint(VeQuat *q, FILE *f);

/** function vePackVector
    <p>A convenience function which stores the arguments in the given
    vector.</p>

    @param v
    The vector into which to pack the arguments.
    
    @param x,y,z
    The values to put in the vector.
*/
void vePackVector(VeVector3 *v, float x, float y, float z);

/** function veMatrixReduce
    <p>Performs a row-reduction on the given matrix;

    @returns
    0 if successful, non-zero if the matrix cannot be reduced.
 */
int veMatrixReduce(VeMatrix4 *m);

/** function veMatrixInvert
    <p>Inverts the given matrix.

    @param m
    The input matrix.

    @param im
    If successful, the inverted matrix is stored here.
    
    @returns
    0 if successful, non-zero if the matrix cannot be inverted.
*/
int veMatrixInvert(VeMatrix4 *m, VeMatrix4 *im);

/** function veMatrixSolve
    <p>Given a matrix A and vector <i>b</i>, solves the system of equations
    <i>Ax = b</i>.  The values for <i>x</i> are stored in the same vector
    that is used to passed <i>b</i>.

    @param A
    The matrix

    @param b
    The right-hand side vector.  The results are returned in this vector
    as well.  There must be space for at least 4 floats in this
    array.
*/
void veMatrixSolve(VeMatrix4 *A, float *b);

#ifdef __cplusplus
}
#endif

#endif /* VE_MATH_H */

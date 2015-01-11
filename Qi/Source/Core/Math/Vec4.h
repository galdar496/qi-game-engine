//
//  Vec4.h
//  Qi Game Engine
//
//  Stores the information for a 4 component vector. When possible,
//  the vector will use SSE instructions.
//
//  Created by Cody White on 1/10/15.
//  Copyright (c) 2015 Cody White. All rights reserved.
//

#pragma once

#include <smmintrin.h>

namespace Qi
{

/**
  * Contains a 4 component vector and all math operations
  */
class __attribute__ ((aligned(16))) Vec4
{
    public:
    
        /**
          * Default constructor.
          */
        Vec4()
        {
            zero();
        }
    
        /**
          * Constructor with per-component initialization.
          */
        Vec4(float _x, float _y, float _z, float _w)
        {
            x = _x;
            y = _y;
            z = _z;
            w = _w;
        }
    
        /**
          * Copy constructor.
          */
        Vec4(const Vec4 &other)
        {
            mm_value = other.mm_value;
        }
    
        /**
          * Zero the vector.
          */
        inline void zero()
        {
            mm_value = _mm_setzero_ps();
        }
    
        // Math Operations //////////////////////////////////////
    
        /**
          * Operator+
          * @param other Vector to add.
          */
        inline Vec4 operator+(const Vec4 &other) const
        {
            return _mm_add_ps(mm_value, other.mm_value);
        }
    
        /**
          * Operator-
          * @param other Vector to subtract.
          */
        inline Vec4 operator-(const Vec4 &other) const
        {
            return _mm_sub_ps(mm_value, other.mm_value);
        }
    
        /**
          * Operator*
          * @param scalar Scalar value to multiply the vectory by.
          */
        inline Vec4 operator*(float scalar) const
        {
            return _mm_mul_ps(mm_value, _mm_set1_ps(scalar));
        }
    
        /**
         * Operator/
         * @param scalar Scalar value to divide the vectory by.
         */
        inline Vec4 operator/(float scalar) const
        {
            return _mm_div_ps(mm_value, _mm_set1_ps(scalar));
        }
    
        /**
         * Operator*=
         * @param scalar Scalar value to multiply the vectory by.
         */
        inline void operator*=(float scalar)
        {
            mm_value = _mm_mul_ps(mm_value, _mm_set1_ps(scalar));
        }
        
        /**
         * Operator/=
         * @param scalar Scalar value to divide the vectory by.
         */
        inline void operator/=(float scalar)
        {
            mm_value = _mm_div_ps(mm_value, _mm_set1_ps(scalar));
        }
    
        /**
          * Dot product
          * @param other Vec4 object to perform the dot product with.
          * @return Scalar value denoting the dot product.
          */
        inline float dot(const Vec4 &other) const
        {
            return _mm_cvtss_f32(_mm_dp_ps(mm_value, other.mm_value, SIMDMask::AllChannels_StoreLow));
        }
    
        /**
          * Cross product. Note that cross product is only defined in 3 dimensions, therefore
          * the fourth component of each vector is ignored.
          * @param other Vec4 object to perform a cross product with.
          * @return Resulting vector from the cross product.
          */
        inline Vec4 cross(const Vec4 &other) const
        {
            return _mm_sub_ps(
                _mm_mul_ps(_mm_shuffle_ps(mm_value, mm_value, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(other.mm_value, other.mm_value, _MM_SHUFFLE(3, 1, 0, 2))),
                _mm_mul_ps(_mm_shuffle_ps(mm_value, mm_value, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(other.mm_value, other.mm_value, _MM_SHUFFLE(3, 0, 2, 1)))
                             );
        }
    
        /**
          * Normalize the vector. The normalized version will overwrite the previous version.
          * Note that this function is optimized for speed and may produce a normal that does not quite have a unit length (but is very close).
          */
        inline void normalize()
        {
            __m128 inverse = _mm_rsqrt_ps(_mm_dp_ps(mm_value, mm_value, SIMDMask::AllChannels_StoreAll));
            mm_value = _mm_mul_ps(mm_value, inverse);
        }
    
        /**
          * Normalize the vector accurately. This differs from standard "normalize" in that it will produce a mathematically correct
          * normal but at much more cost.
          */
        inline void normalizeAccurate()
        {
            __m128 sqrt = _mm_sqrt_ps(_mm_dp_ps(mm_value, mm_value, SIMDMask::AllChannels_StoreAll));
            mm_value = _mm_div_ps(mm_value, sqrt);
        }
    
        /**
          * Get the length of the vector.
          */
        inline float length() const
        {
            __m128 length_squared = _mm_dp_ps(mm_value, mm_value, SIMDMask::AllChannels_StoreLow);
            return _mm_cvtss_f32(_mm_sqrt_ps(length_squared));
        }
    
        /**
          * Get the inverse lenght of the vector.
          */
        inline float lengthInverse() const
        {
            __m128 inv_length_squared = _mm_dp_ps(mm_value, mm_value, SIMDMask::AllChannels_StoreLow);
            return _mm_cvtss_f32(_mm_rsqrt_ps(inv_length_squared));
        }
    
        /**
         * Construction from a SIMD type
         */
        Vec4(__m128 value) : mm_value(value) {}
        
        // Definition of the vector.
        union
        {
            struct {float x, y, z, w; }; // Simple access to simd vector.
            __m128 mm_value;    // SIMD version of the vector.
        };
    
    private:
    
        // Mask values to use in all simd operations.
        enum SIMDMask
        {
            AllChannels_StoreLow = 0xF1, // Use all 4 channels of the simd value during the calculation. Store the result in the lowest register of the result value.
            AllChannels_StoreAll = 0xFF, // Use all 4 channels of the simd value during the calculation. Store the result in all registers for the result value.
        };
};

} // Namespace Qi

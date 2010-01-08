// Aqsis
// Copyright (C) 1997 - 2010, Paul C. Gregory
//
// Contact: pgregory@aqsis.org
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "float4.h"
#include "OpenEXR/ImathMatrix.h"

__attribute__((noinline)) void vadd(float* c, int n, const float* a, const float* b)
{
    for(int i = 0; i < n; i+=4)
    {
        float4::store(c, float4::load(a) + float4::load(b));
        a += 4;
        b += 4;
        c += 4;
    }
}

class M44
{
    private:
        float4 m_rows[4];

    public:
        M44(float d)
        {
            m_rows[0] = float4(d,0,0,0);
            m_rows[1] = float4(0,d,0,0);
            m_rows[2] = float4(0,0,d,0);
            m_rows[3] = float4(0,0,0,1);
        }

        friend float4 operator*(float4 v, const M44& m)
        {
            // 0b00000000 = 0x00
            // 0b01010101 = 0x55
            // 0b10101010 = 0xAA
            // 0b11111111 = 0xFF
            float4 hvec =
                  m.m_rows[0]*float4(_mm_shuffle_ps(v.get(), v.get(), 0x00))
                + m.m_rows[1]*float4(_mm_shuffle_ps(v.get(), v.get(), 0x55))
                + m.m_rows[2]*float4(_mm_shuffle_ps(v.get(), v.get(), 0xAA))
                + m.m_rows[3]*float4(_mm_shuffle_ps(v.get(), v.get(), 0xFF));
//            return hvec * _mm_div_ps(_mm_set1_ps(1), _mm_shuffle_ps(hvec.get(), hvec.get(), 0xFF));
            // calculate approximate reciprocal of last element of hvec using
            // rcp and one iteration of Newton's method.  This is faster than
            // using direct division.
            float4 w = _mm_shuffle_ps(hvec.get(), hvec.get(), 0xFF);
            float4 rcp = _mm_rcp_ps(w.get());
            rcp *= (2 - rcp*w);
            return hvec * rcp;
        }
};

int main()
{
//    float4 res = 42.0f * (float4(1,2,3,4) - 1);
//    std::cout << res;

    const float r = 1e-7f;
    Imath::M44f m = Imath::M44f( r,0,0,0,
                                 0,r,0,0,
                                 0,0,r,0,
                                 0,0,0,1 );
    Imath::V3f v(1,2,3) ;
//    M44 m(r);
//    float4 v(1,2,3,1);

    for(int k = 0; k < 100000000; ++k)
    {
        v += v*m;
    }


    std::cout << v << "\n";

    return 0;
}

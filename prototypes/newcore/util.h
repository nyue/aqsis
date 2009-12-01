#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <cfloat>
#include <vector>
#include <iostream>

#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>

template<typename T>
T* get(std::vector<T>& v) { return v.empty() ? 0 : v[0]; }

template<typename T>
const T* get(const std::vector<T>& v) { return v.empty() ? 0 : v[0]; }


typedef Imath::V3f Vec3;
typedef Imath::V2f Vec2;

typedef Imath::Box3f Box;


std::ostream& operator<<(std::ostream& out, Box b)
{
    out << "[" << b.min << " -- " << b.max << "]";
}


template<typename T>
Imath::Vec2<T> vec2_cast(const Imath::Vec3<T>& v)
{
    return Imath::Vec2<T>(v.x, v.y);
}

template<typename T>
T cross(const Imath::Vec2<T>& a, const Imath::Vec2<T>& b)
{
    return a.x*b.y - b.x*a.y;
}


#endif // UTIL_H_INCLUDED

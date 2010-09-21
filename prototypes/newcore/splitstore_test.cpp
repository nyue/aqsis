// Copyright (C) 2001, Paul C. Gregory and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the New BSD license)

/// \file Tests of geometry storage tree

#include "splitstore.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/auto_unit_test.hpp>

BOOST_AUTO_TEST_SUITE(splitstore_tests)

class MockGeom : public Geometry
{
    private:
        Box m_bound;
    public:
        MockGeom(const Box& bound) : m_bound(bound) {}

        virtual Box bound() const { return m_bound; }

        virtual void tessellate(const Mat4& trans,
                                TessellationContext& tessCtx) const {}

        virtual void transform(const Mat4& trans) {}
};


struct Fixture
{
    SplitStore stor;
    Fixture(int nleaf, const Vec2& bndMin, const Vec2& bndMax)
        : stor(nleaf, nleaf, Imath::Box2f(bndMin, bndMax)) {}
    Fixture(int nleafx, int nleafy, const Vec2& bndMin, const Vec2& bndMax)
        : stor(nleafx, nleafy, Imath::Box2f(bndMin, bndMax)) {}

    void insert(const Vec3& bndMin, const Vec3& bndMax)
    {
        GeometryPtr g(new MockGeom(Box(bndMin, bndMax)));
        GeomHolderPtr h(new GeomHolder(g, 0));
        stor.insert(h);
    }
};

/// Check that no geometry is left in storage
bool storeEmpty(SplitStore& stor)
{
    for(int j = 0; j < stor.nyBuckets(); ++j)
        for(int i = 0; i < stor.nxBuckets(); ++i)
            if(stor.pop(i,j))
                return false;
    return true;
}

BOOST_AUTO_TEST_CASE(splitstore_small_geom_test)
{
    Fixture f(2, Vec2(0,0), Vec2(1,1));
    // Insert into bucket 0,0 - trying to grab it from other buckets should fail.
    f.insert(Vec3(0.1, 0.1, 0), Vec3(0.4, 0.4, 0));
    BOOST_CHECK(!f.stor.pop(1,0));
    BOOST_CHECK(!f.stor.pop(0,1));
    BOOST_CHECK(!f.stor.pop(1,1));
    BOOST_CHECK(f.stor.pop(0,0));

    // Insert into bucket 1,1 - trying to grab it from other buckets should fail.
    f.insert(Vec3(0.6, 0.6, 0), Vec3(0.9, 0.9, 0));
    BOOST_CHECK(!f.stor.pop(0,0));
    BOOST_CHECK(!f.stor.pop(1,0));
    BOOST_CHECK(!f.stor.pop(0,1));
    BOOST_CHECK(f.stor.pop(1,1));
}

BOOST_AUTO_TEST_CASE(splitstore_multinsert_test)
{
    Fixture f(2, Vec2(0,0), Vec2(1,1));
    // Insert into all buckets, since it's small, but straddles the centre
    f.insert(Vec3(0.45, 0.3, 0), Vec3(0.55, 0.7, 0));
    BOOST_CHECK(f.stor.pop(0,0));
    BOOST_CHECK(f.stor.pop(1,0));
    BOOST_CHECK(f.stor.pop(0,1));
    BOOST_CHECK(f.stor.pop(1,1));
    BOOST_CHECK(storeEmpty(f.stor));
}

BOOST_AUTO_TEST_CASE(splitstore_bound_test)
{
    // Test funny-shaped and -located bound
    Fixture f(2, Vec2(-2,10), Vec2(0,15));
    f.insert(Vec3(-1.5,14,0), Vec3(-1.1,14.5,0));
    BOOST_CHECK(f.stor.pop(0,1));
    f.insert(Vec3(-0.5,9,0), Vec3(-0.1,11,0));
    BOOST_CHECK(f.stor.pop(1,0));
}

BOOST_AUTO_TEST_CASE(splitstore_order_test)
{
    Fixture f(4, Vec2(0,0), Vec2(1,1));
    // Insert three different size of geometry &
    // make sure they come out in the right order.
    f.insert(Vec3(0.9, 0.9, 4), Vec3(1, 1, 4));
    f.insert(Vec3(0, 0, 3), Vec3(1, 1, 3));
    f.insert(Vec3(0, 0, 1), Vec3(1, 1, 1));
    f.insert(Vec3(0.6, 0.6, 2), Vec3(1, 1, 2));
    BOOST_CHECK_EQUAL(1, f.stor.pop(3,3)->bound().min.z);
    BOOST_CHECK_EQUAL(2, f.stor.pop(3,3)->bound().min.z);
    BOOST_CHECK_EQUAL(3, f.stor.pop(3,3)->bound().min.z);
    BOOST_CHECK_EQUAL(4, f.stor.pop(3,3)->bound().min.z);
}

BOOST_AUTO_TEST_CASE(splitstore_outside_bound_test)
{
    // Check that geometry outside the bounds is ignored.
    Fixture f(2, Vec2(0,0), Vec2(1,1));
    f.insert(Vec3(-1,-1,0), Vec3(-0.01,2,0));
    f.insert(Vec3(-1,-1,0), Vec3(2,-0.01,0));
    f.insert(Vec3(-1,1.01,0), Vec3(2,2,0));
    f.insert(Vec3(1.01,-1,0), Vec3(2,2,0));
    BOOST_CHECK(storeEmpty(f.stor));
}

BOOST_AUTO_TEST_CASE(splitstore_nonpower_of_two)
{
    // Check that non-power of two sizes work fine
    Fixture f(5, 7, Vec2(0,0), Vec2(1,1));
    f.insert(Vec3(0, 0, 0), Vec3(0.1, 0.1, 0));
    BOOST_CHECK(f.stor.pop(0,0));
    f.insert(Vec3(0.9, 0, 0), Vec3(1, 0.1, 0));
    BOOST_CHECK(f.stor.pop(4,0));
}

BOOST_AUTO_TEST_SUITE_END()

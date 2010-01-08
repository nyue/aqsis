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

#include "vset.h"

#include <vector>
#include <set>
#include <cstdlib>
#include <iostream>

#define ARRAYEND(array) (array + sizeof(array)/sizeof(array[0]))

__attribute__((noinline))
int contains(const VSet<int>& s, int e)
{
    return s.contains(e);
}

__attribute__((noinline))
int contains(const std::set<int>& s, int e)
{
    return s.find(e) != s.end();
}

void speedTest()
{
    const int timingIters = 10;
    const int nsets = 1000000;
    const int nelem = 10;
    // Create some set data
    std::vector<int> s1Init;
    for(int i = 0; i < nelem; ++i)
        s1Init.push_back(rand());
    int* b = &s1Init[0];
    int* e = &s1Init.back()+1;
    // Create some sets; all the same actually.
    typedef VSet<int> Set;
//    typedef std::set<int> Set;
    std::vector<Set> sets;
    for(int j = 0; j < nsets; ++j)
        sets.push_back(Set(b, e));
    // Timing loop
    long sum = 0;
    for(long iter = 0; iter < timingIters; ++iter)
    {
        for(int j = 0; j < nsets; ++j)
        {
            for(int i = 0; i < nelem; ++i)
                sum += contains(sets[j], s1Init[i]);
        }
    }
    std::cout << sum << "\n";
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const VSet<T>& set)
{
    out << "{ ";
    for(typename VSet<T>::const_iterator i = set.begin(), e = set.end(); i != e; ++i)
        out << *i << " ";
    out << "}";
    return out;
}

void test()
{
    int s1Init[] = {1,6,7,3,99};
    VSet<int> s1(s1Init, ARRAYEND(s1Init));
    int s2Init[] = {1,-4,42,7,100};
    VSet<int> s2(s2Init, ARRAYEND(s2Init));
    std::cout << s1 << "\n";

    std::cout << "s1.contains(1) = " << s1.contains(1) << "\n";
    std::cout << "s1.contains(-1) = " << s1.contains(-1) << "\n";

    int smallInit[] = {1,6};
    VSet<int> small(smallInit, ARRAYEND(smallInit));
    std::cout << s1 << ".includes(" << small << ") = " << s1.includes(small) << "\n";


    VSet<int> u;
    setUnion(s1, s2, u);
    std::cout << "union(" << s1 << ", " << s2 << ") = " << u << "\n";
    VSet<int> i;
    setIntersection(s1, s2, i);
    std::cout << "intersection(" << s1 << ", " << s2 << ") = " << i << "\n";
}

int main()
{
//    speedTest();
    test();
    return 0;
};

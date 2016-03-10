/*
 *  Created by Phil on 28/04/2011.
 *  Copyright 2010 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_APPROX_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_APPROX_HPP_INCLUDED

#include "catch_tostring.h"

#include <cmath>
#include <limits>


template <class T>
class ApproxT {
public:
    static_assert(sizeof(T) == sizeof(double), "Do not use Approx for integers");
    ApproxT ( T value, double epsilon, T scale )
    :   m_epsilon( epsilon ), m_scale( scale ), m_value( value ) {}

    ApproxT (double epsilon=std::numeric_limits<float>::epsilon()*100, T scale = T(1.0) )
    :   m_epsilon( epsilon ), m_scale( scale ), m_value( T(0.0) ) {}
    
    ApproxT operator()( T value ) {
        ApproxT approx( value, m_epsilon, m_scale );
        return approx;
    }
    
    friend bool operator == ( T lhs, ApproxT const& rhs ) {
        // Thanks to Richard Harris for his help refining this formula
        return fabs( lhs - rhs.m_value ) < rhs.m_epsilon * (rhs.m_scale + max( fabs(lhs), fabs(rhs.m_value) ) );
    }
    
    friend bool operator == ( ApproxT const& lhs, T rhs ) {
        return operator==( rhs, lhs );
    }
    
    friend bool operator != ( T lhs, ApproxT const& rhs ) {
        return !operator==( lhs, rhs );
    }
    
    friend bool operator != ( ApproxT const& lhs, T rhs ) {
        return !operator==( rhs, lhs );
    }
    friend inline std::ostream& operator<< (std::ostream &out, ApproxT value) { out << value.m_value << " ± " << std::setprecision(2) << value.m_epsilon * (value.m_scale+value.m_value); return out; }
    
private:
    double m_epsilon;
    T m_scale;
    T m_value;
};

template<class T> inline ApproxT<T> Approx(T value, double epsilon = std::numeric_limits<float>::epsilon()*100)
{
    return ApproxT<T>(value, epsilon, T(1.0));
}
template<class T1, class T> inline ApproxT<T> Approx(T1 value, double epsilon, T scale)
{
    return ApproxT<T>(value, epsilon, scale);
}


#endif // TWOBLUECUBES_CATCH_APPROX_HPP_INCLUDED

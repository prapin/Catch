#include "catch.hpp"
#include <vector>


// vedctor
TEST_CASE( "vector<int> -> toString", "[toString][vector]" )
{
    std::vector<int> vv;
    REQUIRE( Catch::toString(vv) == "{  }" );
    vv.push_back( 42 );
    REQUIRE( Catch::toString(vv) == "{ 42 }" );
    vv.push_back( 512 );
    REQUIRE( Catch::toString(vv) == "{ 42, 512 }" );
}

TEST_CASE( "vector<string> -> toString", "[toString][vector]" )
{
    std::vector<std::string> vv;
    REQUIRE( Catch::toString(vv) == "{  }" );
    vv.push_back( "hello" );
    REQUIRE( Catch::toString(vv) == "{ \"hello\" }" );
    vv.push_back( "world" );
    REQUIRE( Catch::toString(vv) == "{ \"hello\", \"world\" }" );
}

#if defined(CATCH_CPP11_OR_GREATER)
/*
  Note: These tests *can* be made to work with C++ < 11, but the
  allocator is a lot more work...
*/
namespace {
    /* Minimal Allocator */
    template<typename T>
    struct minimal_allocator {
        typedef T value_type;
        typedef std::size_t size_type;
        T *allocate( size_type n )
        {
            return static_cast<T *>( ::operator new( n * sizeof(T) ) );
        }
        void deallocate( T *p, size_type /*n*/ )
        {
            ::operator delete( static_cast<void *>(p) );
        }
        template<typename U>
        bool operator==( const minimal_allocator<U>& ) const { return true; }
        template<typename U>
        bool operator!=( const minimal_allocator<U>& ) const { return false; }
    };
}

TEST_CASE( "vector<int,allocator> -> toString", "[toString][vector,allocator]" )
{
    std::vector<int,minimal_allocator<int> > vv;
    REQUIRE( Catch::toString(vv) == "{  }" );
    vv.push_back( 42 );
    REQUIRE( Catch::toString(vv) == "{ 42 }" );
    vv.push_back( 512 );
    REQUIRE( Catch::toString(vv) == "{ 42, 512 }" );
}

TEST_CASE( "vec<vec<string,alloc>> -> toString", "[toString][vector,allocator]" )
{
    typedef std::vector<std::string,minimal_allocator<std::string> > inner;
    typedef std::vector<inner> vector;
    vector v;
    REQUIRE( Catch::toString(v) == "{  }" );
    v.push_back( inner { "hello" } );
    v.push_back( inner { "world" } );
    REQUIRE( Catch::toString(v) == "{ { \"hello\" }, { \"world\" } }" );
}
#endif
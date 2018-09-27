/*
 *  Created by Phil on 20/05/2011.
 *  Copyright 2011 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_DEFAULT_MAIN_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_DEFAULT_MAIN_HPP_INCLUDED

#ifndef __OBJC__

// Standard C/C++ main entry point
int main (int argc, char * argv[]) {
    // prapin
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_ALLOC_MEM_DF);
    _crtBreakAlloc = -1; // If memory leak is detected, set here the allocation memory block to break on allocation
#endif
    BaseAutorelease;
    Time t = BaseDate::getTimestamp();
    int res = Catch::Session().run( argc, argv );
    baseSingletonsManager.isReleasingAll = true;
    printf("Finished in %.3f s\n", (BaseDate::getTimestamp() - t).get());
    return res;
}

#else // __OBJC__

// Objective-C entry point
int main (int argc, char * const argv[]) {
    NSAutoreleasePool;

    Catch::registerTestMethods();
    int result = Catch::Session().run( argc, (char* const*)argv );

    return result;
}

#endif // __OBJC__

#endif // TWOBLUECUBES_CATCH_DEFAULT_MAIN_HPP_INCLUDED

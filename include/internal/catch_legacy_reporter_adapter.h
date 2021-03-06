/*
 *  Created by Phil on 6th April 2013.
 *  Copyright 2013 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_LEGACY_REPORTER_ADAPTER_H_INCLUDED
#define TWOBLUECUBES_CATCH_LEGACY_REPORTER_ADAPTER_H_INCLUDED

#include "catch_interfaces_reporter.h"

namespace Catch
{
    // Deprecated
    struct IReporter : IShared {
        virtual ~IReporter();

        virtual bool shouldRedirectStdout() const = 0;

        virtual void StartTesting() = 0;
        virtual void EndTesting( Totals const& totals ) = 0;
        virtual void StartGroup( std::string const& groupName ) = 0;
        virtual void EndGroup( std::string const& groupName, Totals const& totals ) = 0;
        virtual void StartTestCase( TestCaseInfo const& testInfo ) = 0;
        virtual void EndTestCase( TestCaseInfo const& testInfo, Totals const& totals, std::string const& stdOut, std::string const& stdErr ) = 0;
        virtual void StartSection( std::string const& sectionName, std::string const& description ) = 0;
        virtual void EndSection( std::string const& sectionName, Counts const& assertions ) = 0;
        virtual void NoAssertionsInSection( std::string const& sectionName ) = 0;
        virtual void NoAssertionsInTestCase( std::string const& testName ) = 0;
        virtual void Aborted() = 0;
        virtual void Result( AssertionResult const& result ) = 0;
    };

    class LegacyReporterAdapter : public SharedImpl<IStreamingReporter>
    {
    public:
        LegacyReporterAdapter( Ptr<IReporter> const& legacyReporter );
        virtual ~LegacyReporterAdapter();

        virtual ReporterPreferences getPreferences() const override;
        virtual void noMatchingTestCases( std::string const& ) override;
        virtual void testRunStarting( TestRunInfo const& ) override;
        virtual void testGroupStarting( GroupInfo const& groupInfo ) override;
        virtual void testCaseStarting( TestCaseInfo const& testInfo ) override;
        virtual void sectionStarting( SectionInfo const& sectionInfo ) override;
        virtual void assertionStarting( AssertionInfo const& ) override;
        virtual bool assertionEnded( AssertionStats const& assertionStats ) override;
        virtual void sectionEnded( SectionStats const& sectionStats ) override;
        virtual void testCaseEnded( TestCaseStats const& testCaseStats ) override;
        virtual void testGroupEnded( TestGroupStats const& testGroupStats ) override;
        virtual void testRunEnded( TestRunStats const& testRunStats ) override;
        virtual void skipTest( TestCaseInfo const& ) override;

    private:
        Ptr<IReporter> m_legacyReporter;
    };
}

#endif // TWOBLUECUBES_CATCH_LEGACY_REPORTER_ADAPTER_H_INCLUDED

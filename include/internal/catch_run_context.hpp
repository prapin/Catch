 /*
 *  Created by Phil on 22/10/2010.
 *  Copyright 2010 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_RUNNER_IMPL_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_RUNNER_IMPL_HPP_INCLUDED

#include "catch_interfaces_runner.h"
#include "catch_interfaces_reporter.h"
#include "catch_interfaces_exception.h"
#include "catch_config.hpp"
#include "catch_test_registry.hpp"
#include "catch_test_case_info.h"
#include "catch_capture.hpp"
#include "catch_totals.hpp"
#include "catch_test_spec.hpp"
#include "catch_test_case_tracker.hpp"
#include "catch_timer.h"
#include "catch_result_builder.h"
#include "catch_fatal_condition.hpp"

#include <set>
#include <string>
#include <chrono>
#include "Utility/BaseThreadManager.h"
#include "Debug/DebugManagerStdout.h"
#include "Utility/BaseRaiiCleaner.h"
#include "Utility/UnitTestDatabase.h"

namespace Catch {

    class StreamRedirect {

    public:
        StreamRedirect( std::ostream& stream, std::string& targetString )
        :   m_stream( stream ),
            m_prevBuf( stream.rdbuf() ),
            m_targetString( targetString )
        {
            stream.rdbuf( m_oss.rdbuf() );
        }

        ~StreamRedirect() {
            m_targetString += m_oss.str();
            m_stream.rdbuf( m_prevBuf );
        }

    private:
        std::ostream& m_stream;
        std::streambuf* m_prevBuf;
        std::ostringstream m_oss;
        std::string& m_targetString;
    };

    ///////////////////////////////////////////////////////////////////////////

    class RunContext : public IResultCapture, public IRunner {

        RunContext( RunContext const& );
        void operator =( RunContext const& );

    public:

        explicit RunContext( Ptr<IConfig const> const& _config, Ptr<IStreamingReporter> const& reporter )
        :   m_runInfo( _config->name() ),
            m_context( getCurrentMutableContext() ),
            m_activeTestCase( CATCH_NULL ),
            m_config( _config ),
            m_reporter( reporter )
        {
            m_context.setRunner( this );
            m_context.setConfig( m_config );
            m_context.setResultCapture( this );
            m_reporter->testRunStarting( m_runInfo );
        }

        virtual ~RunContext() {
            m_reporter->testRunEnded( TestRunStats( m_runInfo, m_totals, aborting() ) );
        }

        void testGroupStarting( std::string const& testSpec, std::size_t groupIndex, std::size_t groupsCount ) {
            m_reporter->testGroupStarting( GroupInfo( testSpec, groupIndex, groupsCount ) );
        }
        void testGroupEnded( std::string const& testSpec, Totals const& totals, std::size_t groupIndex, std::size_t groupsCount ) {
            m_reporter->testGroupEnded( TestGroupStats( GroupInfo( testSpec, groupIndex, groupsCount ), totals, aborting() ) );
        }

        Totals runTest( TestCase const& testCase ) {
            Totals prevTotals = m_totals;

            std::string redirectedCout;
            std::string redirectedCerr;

            TestCaseInfo testInfo = testCase.getTestCaseInfo();

            m_reporter->testCaseStarting( testInfo );

            m_activeTestCase = &testCase;


            do {
                m_trackerContext.startRun();
                do {
                    m_trackerContext.startCycle();
                    m_testCaseTracker = &SectionTracker::acquire( m_trackerContext, testInfo.name );
                    runCurrentTest( redirectedCout, redirectedCerr );
                }
                while( !m_testCaseTracker->isSuccessfullyCompleted() && !aborting() );
            }
            // !TBD: deprecated - this will be replaced by indexed trackers
            while( getCurrentContext().advanceGeneratorsForCurrentTest() && !aborting() );

            Totals deltaTotals = m_totals.delta( prevTotals );
            if( testInfo.expectedToFail() && deltaTotals.testCases.passed > 0 ) {
                deltaTotals.assertions.failed++;
                deltaTotals.testCases.passed--;
                deltaTotals.testCases.failed++;
            }
            m_totals.testCases += deltaTotals.testCases;
            gUnitTestDatabase->addResultToDatabase(testInfo.name.c_str(), deltaTotals.testCases.allPassed());

            m_reporter->testCaseEnded( TestCaseStats(   testInfo,
                                                        deltaTotals,
                                                        redirectedCout,
                                                        redirectedCerr,
                                                        aborting() ) );

            m_activeTestCase = CATCH_NULL;
            m_testCaseTracker = CATCH_NULL;

            return deltaTotals;
        }

        Ptr<IConfig const> config() const {
            return m_config;
        }

    private: // IResultCapture


        virtual void assertionEnded( AssertionResult const& result ) override {
            if( result.getResultType() == ResultWas::Ok ) {
                m_totals.assertions.passed++;
            }
            else if( !result.isOk() ) {
                m_totals.assertions.failed++;
            }

            if( m_reporter->assertionEnded( AssertionStats( result, m_messages, m_totals ) ) )
                m_messages.clear();

            // Reset working state
            m_lastAssertionInfo = AssertionInfo( "", m_lastAssertionInfo.lineInfo, "{Unknown expression after the reported line}" , m_lastAssertionInfo.resultDisposition );
            m_lastResult = result;
        }

        virtual bool sectionStarted (
            SectionInfo const& sectionInfo,
            Counts& assertions
        ) override
        {
            std::ostringstream oss;
            oss << sectionInfo.name << "@" << sectionInfo.lineInfo;

            ITracker& sectionTracker = SectionTracker::acquire( m_trackerContext, oss.str() );
            if( !sectionTracker.isOpen() )
                return false;
            m_activeSections.push_back( &sectionTracker );

            m_lastAssertionInfo.lineInfo = sectionInfo.lineInfo;

            m_reporter->sectionStarting( sectionInfo );

            assertions = m_totals.assertions;

            return true;
        }
        bool testForMissingAssertions( Counts& assertions ) {
            if( assertions.total() != 0 )
                return false;
            if( !m_config->warnAboutMissingAssertions() )
                return false;
            if( m_trackerContext.currentTracker().hasChildren() )
                return false;
            m_totals.assertions.failed++;
            assertions.failed++;
            return true;
        }

        virtual void sectionEnded( SectionEndInfo const& endInfo ) {
            Counts assertions = m_totals.assertions - endInfo.prevAssertions;
            bool missingAssertions = testForMissingAssertions( assertions );

            if( !m_activeSections.empty() ) {
                m_activeSections.back()->close();
                m_activeSections.pop_back();
            }

            m_reporter->sectionEnded( SectionStats( endInfo.sectionInfo, assertions, endInfo.durationInSeconds, missingAssertions ) );
            m_messages.clear();
        }

        virtual void sectionEndedEarly( SectionEndInfo const& endInfo ) {
            if( m_unfinishedSections.empty() )
                m_activeSections.back()->fail();
            else
                m_activeSections.back()->close();
            m_activeSections.pop_back();

            m_unfinishedSections.push_back( endInfo );
        }

        virtual void pushScopedMessage( MessageInfo const& message ) {
            m_messages.push_back( message );
        }

        virtual void popScopedMessage( MessageInfo const& message ) override {
            m_messages.erase( std::remove( m_messages.begin(), m_messages.end(), message ), m_messages.end() );
        }

        virtual std::string getCurrentTestName() const override {
            return m_activeTestCase
                ? m_activeTestCase->getTestCaseInfo().name
                : "";
        }

        virtual const AssertionResult* getLastResult() const override {
            return &m_lastResult;
        }

        virtual void handleFatalErrorCondition( std::string const& message ) override {
            ResultBuilder resultBuilder = makeUnexpectedResultBuilder();
            resultBuilder.setResultType( ResultWas::FatalErrorCondition );
            resultBuilder << message;
            resultBuilder.captureExpression();

            handleUnfinishedSections();

            // Recreate section for test case (as we will lose the one that was in scope)
            TestCaseInfo const& testCaseInfo = m_activeTestCase->getTestCaseInfo();
            SectionInfo testCaseSection( testCaseInfo.lineInfo, testCaseInfo.name, testCaseInfo.description );

            Counts assertions;
            assertions.failed = 1;
            SectionStats testCaseSectionStats( testCaseSection, assertions, 0, false );
            m_reporter->sectionEnded( testCaseSectionStats );

            TestCaseInfo testInfo = m_activeTestCase->getTestCaseInfo();

            Totals deltaTotals;
            deltaTotals.testCases.failed = 1;
            m_reporter->testCaseEnded( TestCaseStats(   testInfo,
                                                        deltaTotals,
                                                        "",
                                                        "",
                                                        false ) );
            m_totals.testCases.failed++;
            testGroupEnded( "", m_totals, 1, 1 );
            m_reporter->testRunEnded( TestRunStats( m_runInfo, m_totals, false ) );
        }

    public:
        // !TBD We need to do this another way!
        bool aborting() const override {
            return m_totals.assertions.failed == static_cast<std::size_t>( m_config->abortAfter() );
        }

    private:

        void runCurrentTest( std::string& redirectedCout, std::string& redirectedCerr ) {
            TestCaseInfo const& testCaseInfo = m_activeTestCase->getTestCaseInfo();
            SectionInfo testCaseSection( testCaseInfo.lineInfo, testCaseInfo.name, testCaseInfo.description );
            m_reporter->sectionStarting( testCaseSection );
            Counts prevAssertions = m_totals.assertions;
            double duration = 0;
            try {
                // Function modified to better match Alf memory model. PRapin
                baseThreadManager->stopAll();
                baseSingletonsManager.releaseAll();
                BaseObject::recentObjects->clear();
                S32 old_counts[2] = {BaseObject::constructorCount, BaseObject::destructorCount} ;
                
               
                m_lastAssertionInfo = AssertionInfo( "TEST_CASE", testCaseInfo.lineInfo, "", ResultDisposition::Normal );
                {
                    BaseRaiiCleaner cleanSingleton([]{ baseSingletonsManager.releaseAll(); });
                    BaseRaiiCleaner cleanThread([]{ baseThreadManager->stopAll(); });
                    BaseAutorelease;
                    U32 seed = m_config->rngSeed();
                    if(seed == 0)
                    {
                        S64 t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                        seed = (U32)(t ^ (t >> 32));
                    }
                    random.seed(seed);
                    srand(seed);
                    INTERNAL_CATCH_INFO( "Random generator seeded with value " << seed, "INFO" );

                    seedRng( *m_config );
                   
                    Timer timer;
                    timer.start();
                    if( m_reporter->getPreferences().shouldRedirectStdOut ) {
                        StreamRedirect coutRedir( Catch::cout(), redirectedCout );
                        StreamRedirect cerrRedir( Catch::cerr(), redirectedCerr );
                        invokeActiveTestCase();
                    }
                    else {
                        invokeActiveTestCase();
                    }
                    duration = timer.getElapsedSeconds();
                }
                
                std::unordered_set<BaseObject*> leakingObjects = *BaseObject::recentObjects;
                for(BaseObject* obj : leakingObjects)
                {
                    INTERNAL_CATCH_MSG( Catch::ResultWas::Warning, Catch::ResultDisposition::ContinueOnFailure,
                                       "WARN", "Leaking object #" << obj->stamp_id << " of type " << obj->getClass() << " ("
                                       << typeid(*obj).name() << ") " << " retain=" << obj->getRetainCount() );
                    {
                        BaseAutorelease;
                        BaseString* description = obj->toString();
                        if(description->length() > 200)
                            description = description->substring(0, 200)->stringByAppendingString(STR("\n[...]"));
                        printf("%s\n\n",description->cString());
                    }
                }
                S32 new_counts[2] = {BaseObject::constructorCount, BaseObject::destructorCount} ;
                ptrdiff_t diff = new_counts[0] - new_counts[1] - old_counts[0] + old_counts[1];
                
                if(diff)
                {
                    INTERNAL_CATCH_MSG( Catch::ResultWas::ExplicitFailure, Catch::ResultDisposition::Normal, "FAIL",
                                       "Memory leaks detected: " << diff << " objects\nSet BaseObject::stampIDToBreak to find the leaking object");
                }
                

            }
            catch( TestFailureException& ) {
                // This just means the test was aborted due to failure
            }
            catch(...) {
                makeUnexpectedResultBuilder().useActiveException();
            }
            m_testCaseTracker->close();
            handleUnfinishedSections();
            m_messages.clear();

            Counts assertions = m_totals.assertions - prevAssertions;
            bool missingAssertions = testForMissingAssertions( assertions );

            if( testCaseInfo.okToFail() ) {
                std::swap( assertions.failedButOk, assertions.failed );
                m_totals.assertions.failed -= assertions.failedButOk;
                m_totals.assertions.failedButOk += assertions.failedButOk;
            }

            SectionStats testCaseSectionStats( testCaseSection, assertions, duration, missingAssertions );
            m_reporter->sectionEnded( testCaseSectionStats );
        }

        void invokeActiveTestCase() {
            FatalConditionHandler fatalConditionHandler; // Handle signals
            m_activeTestCase->invoke();
            fatalConditionHandler.reset();
        }

    private:

        ResultBuilder makeUnexpectedResultBuilder() const {
            return ResultBuilder(   m_lastAssertionInfo.macroName.c_str(),
                                    m_lastAssertionInfo.lineInfo,
                                    m_lastAssertionInfo.capturedExpression.c_str(),
                                    m_lastAssertionInfo.resultDisposition );
        }

        void handleUnfinishedSections() {
            // If sections ended prematurely due to an exception we stored their
            // infos here so we can tear them down outside the unwind process.
            for( std::vector<SectionEndInfo>::const_reverse_iterator it = m_unfinishedSections.rbegin(),
                        itEnd = m_unfinishedSections.rend();
                    it != itEnd;
                    ++it )
                sectionEnded( *it );
            m_unfinishedSections.clear();
        }

        TestRunInfo m_runInfo;
        IMutableContext& m_context;
        TestCase const* m_activeTestCase;
        ITracker* m_testCaseTracker;
        ITracker* m_currentSectionTracker;
        AssertionResult m_lastResult;

        Ptr<IConfig const> m_config;
        Totals m_totals;
        Ptr<IStreamingReporter> m_reporter;
        std::vector<MessageInfo> m_messages;
        AssertionInfo m_lastAssertionInfo;
        std::vector<SectionEndInfo> m_unfinishedSections;
        std::vector<ITracker*> m_activeSections;
        TrackerContext m_trackerContext;
    };

    IResultCapture& getResultCapture() {
        if( IResultCapture* capture = getCurrentContext().getResultCapture() )
            return *capture;
        else
            throw std::logic_error( "No result capture instance" );
    }

} // end namespace Catch

#endif // TWOBLUECUBES_CATCH_RUNNER_IMPL_HPP_INCLUDED

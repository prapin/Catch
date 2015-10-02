/*
 *  Created by Phil on 31/10/2010.
 *  Copyright 2010 Two Blue Cubes Ltd. All rights reserved.
 *
 *  Distributed under the Boost Software License, Version 1.0. (See accompanying
 *  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED

#include "internal/catch_commandline.hpp"
#include "internal/catch_list.hpp"
#include "internal/catch_runner_impl.hpp"
#include "internal/catch_test_spec.hpp"
#include "internal/catch_version.h"
#include "internal/catch_text.h"

#include <fstream>
#include <stdlib.h>
#include <limits>
#include "LastGitCommit.h"
#include "Cryptography/BaseMD5.h"
#include "Debug/DebugManagerCatch.h"

namespace Catch {

    class Runner {

    public:
        Runner( Ptr<Config> const& config )
        :   m_config( config )
        {
            openStream();
            makeReporter();
        }

        Totals runTests() {

            RunContext context( m_config.get(), m_reporter );

            Totals totals;

            context.testGroupStarting( "all tests", 1, 1 ); // deprecated?

            TestSpec testSpec = m_config->testSpec();
            if( !testSpec.hasFilters() )
                testSpec = TestSpecParser( ITagAliasRegistry::get() ).parse( "~[.]" ).testSpec(); // All not hidden tests

            std::vector<TestCase> testCases;
            getRegistryHub().getTestCaseRegistry().getFilteredTests( testSpec, *m_config, testCases );

            int testsRunForGroup = 0;
            for( std::vector<TestCase>::const_iterator it = testCases.begin(), itEnd = testCases.end();
                    it != itEnd;
                    ++it ) {
                testsRunForGroup++;
                if( m_testsAlreadyRun.find( *it ) == m_testsAlreadyRun.end() ) {

                    if( context.aborting() )
                        break;

                    totals += context.runTest( *it );
                    m_testsAlreadyRun.insert( *it );
                }
            }
            std::vector<TestCase> skippedTestCases;
            getRegistryHub().getTestCaseRegistry().getFilteredTests( testSpec, *m_config, skippedTestCases, true );
            
            for( std::vector<TestCase>::const_iterator it = skippedTestCases.begin(), itEnd = skippedTestCases.end();
                    it != itEnd;
                    ++it )
                m_reporter->skipTest( *it );

            context.testGroupEnded( "all tests", totals, 1, 1 );
            return totals;
        }

    private:
        void openStream() {
            // Open output file, if specified
            if( !m_config->getFilename().empty() ) {
                m_ofs.open( m_config->getFilename().c_str() );
                if( m_ofs.fail() ) {
                    std::ostringstream oss;
                    oss << "Unable to open file: '" << m_config->getFilename() << "'";
                    throw std::domain_error( oss.str() );
                }
                m_config->setStreamBuf( m_ofs.rdbuf() );
            }
        }
        void makeReporter() {
            std::string reporterName = m_config->getReporterName().empty()
                ? "console"
                : m_config->getReporterName();

            m_reporter = getRegistryHub().getReporterRegistry().create( reporterName, m_config.get() );
            if( !m_reporter ) {
                std::ostringstream oss;
                oss << "No reporter registered with name: '" << reporterName << "'";
                throw std::domain_error( oss.str() );
            }
        }

    private:
        Ptr<Config> m_config;
        std::ofstream m_ofs;
        Ptr<IStreamingReporter> m_reporter;
        std::set<TestCase> m_testsAlreadyRun;
    };

    class Session : NonCopyable {
        static bool alreadyInstantiated;

    public:

        struct OnUnusedOptions { enum DoWhat { Ignore, Fail }; };

        Session()
        : m_cli( makeCommandLineParser() ),
        debug((new(AUTORELEASE)DebugManagerCatch)->setLevel(DebugSeverity::info)->setFormat("%m"))
        {
            if( alreadyInstantiated ) {
                std::string msg = "Only one instance of Catch::Session can ever be used";
                Catch::cerr() << msg << std::endl;
                throw std::logic_error( msg );
            }
            alreadyInstantiated = true;
            BaseObject::recentObjectsMutex = new std::mutex;
            BaseObject::recentObjects = new std::unordered_set<BaseObject*>;
        }
        ~Session() {
            Catch::cleanUp();
            delete BaseObject::recentObjects;
            BaseObject::recentObjects = NULL;
            delete BaseObject::recentObjectsMutex;
            BaseObject::recentObjectsMutex = NULL;
        }

        void showHelp( std::string const& processName ) {
            Catch::cout() << "\nCatch v" << libraryVersion << "\n";

            m_cli.usage( Catch::cout(), processName );
            Catch::cout() << "For more detail usage please see the project docs\n" << std::endl;
        }

        int applyCommandLine( int argc, char* const argv[], OnUnusedOptions::DoWhat unusedOptionBehaviour = OnUnusedOptions::Fail ) {
            try {
                m_cli.setThrowOnUnrecognisedTokens( unusedOptionBehaviour == OnUnusedOptions::Fail );
                m_unusedTokens = m_cli.parseInto( argc, argv, m_configData );
                if( m_configData.showHelp )
                    showHelp( m_configData.processName );
                m_config.reset();
            }
            catch( std::exception& ex ) {
                {
                    Colour colourGuard( Colour::Red );
                    Catch::cerr()
                        << "\nError(s) in input:\n"
                        << Text( ex.what(), TextAttributes().setIndent(2) )
                        << "\n\n";
                }
                m_cli.usage( Catch::cout(), m_configData.processName );
                return (std::numeric_limits<int>::max)();
            }
            return 0;
        }

        void useConfigData( ConfigData const& _configData ) {
            m_configData = _configData;
            m_config.reset();
        }

        int run( int argc, char* const argv[] ) {

            int returnCode = applyCommandLine( argc, argv );
            if( returnCode == 0 )
                returnCode = run();
            return returnCode;
        }

        int run() {
            if( m_configData.showHelp )
                return 0;

            try
            {
                config(); // Force config to be constructed

                static const PSTRING version = "GITGLOBALID=" GIT_LAST_COMMIT_ABBRHASH;
                BaseAutorelease a;
                printf("Git ID: %s\n", version+12);
                if(!m_configData.debugLevel.empty())
                    (new(AUTORELEASE)DebugManagerStdout)->setFilter(m_configData.debugLevel.c_str());
                if(m_configData.md5DatabaseName.length())
                    BaseMD5::openDatabase(m_configData.md5DatabaseName.c_str());

                Runner runner( m_config );

                // Handle list request
                if( Option<std::size_t> listed = list( config() ) )
                    return static_cast<int>( *listed );

                return static_cast<int>( runner.runTests().assertions.failed );
            }
            catch( std::exception& ex ) {
                Catch::cerr() << ex.what() << std::endl;
                return (std::numeric_limits<int>::max)();
            }
        }

        Clara::CommandLine<ConfigData> const& cli() const {
            return m_cli;
        }
        std::vector<Clara::Parser::Token> const& unusedTokens() const {
            return m_unusedTokens;
        }
        ConfigData& configData() {
            return m_configData;
        }
        Config& config() {
            if( !m_config )
                m_config = new Config( m_configData );
            return *m_config;
        }

    private:
        Clara::CommandLine<ConfigData> m_cli;
        std::vector<Clara::Parser::Token> m_unusedTokens;
        ConfigData m_configData;
        Ptr<Config> m_config;
        SP<DebugManager> debug;
    };

    bool Session::alreadyInstantiated = false;
} // end namespace Catch


// prapin additions
static char errorBuffer[1000];
void CatchAssertion(bool condition, PSTRING file, int line, PSTRING cond_text)
{
    if(condition)
        return;
    if(Catch::ExpectingThrows::counter == 0)
        CATCH_BREAK_INTO_DEBUGGER();
    sprintf(errorBuffer, "Assertion failed at %s:%d (%s)", file, line, cond_text);
    throw errorBuffer;
}
namespace Catch {
    int ExpectingThrows::counter;
    U32 ExpectsDebugsError::capturingMask;
    U16 ExpectsDebugsError::counters[DebugSeveritiesCount];
    random_t random;
}

#endif // TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED

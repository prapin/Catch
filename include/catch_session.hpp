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
#include "internal/catch_run_context.hpp"
#include "internal/catch_test_spec.hpp"
#include "internal/catch_version.h"
#include "internal/catch_text.h"

#include <fstream>
#include <stdlib.h>
#include <limits>
#include "LastGitCommit.h"
#include "Utility/UnitTestDatabase.h"
#include "Debug/DebugManagerCatch.h"

namespace Catch {

    Ptr<IStreamingReporter> createReporter( std::string const& reporterName, Ptr<Config> const& config ) {
        Ptr<IStreamingReporter> reporter = getRegistryHub().getReporterRegistry().create( reporterName, config.get() );
        if( !reporter ) {
            std::ostringstream oss;
            oss << "No reporter registered with name: '" << reporterName << "'";
            throw std::domain_error( oss.str() );
        }
        return reporter;
    }

    Ptr<IStreamingReporter> makeReporter( Ptr<Config> const& config ) {
        std::vector<std::string> reporters = config->getReporterNames();
        if( reporters.empty() )
            reporters.push_back( "console" );

        Ptr<IStreamingReporter> reporter;
        for( std::vector<std::string>::const_iterator it = reporters.begin(), itEnd = reporters.end();
                it != itEnd;
                ++it )
            reporter = addReporter( reporter, createReporter( *it, config ) );
        return reporter;
    }
    Ptr<IStreamingReporter> addListeners( Ptr<IConfig const> const& config, Ptr<IStreamingReporter> reporters ) {
        IReporterRegistry::Listeners listeners = getRegistryHub().getReporterRegistry().getListeners();
        for( IReporterRegistry::Listeners::const_iterator it = listeners.begin(), itEnd = listeners.end();
                it != itEnd;
                ++it )
            reporters = addReporter(reporters, (*it)->create( ReporterConfig( config ) ) );
        return reporters;
    }


    Totals runTests( Ptr<Config> const& config ) {

        Ptr<IConfig const> iconfig = config.get();

        Ptr<IStreamingReporter> reporter = makeReporter( config );
        reporter = addListeners( iconfig, reporter );

        RunContext context( iconfig, reporter );

        Totals totals;

        context.testGroupStarting( config->name(), 1, 1 );

        TestSpec testSpec = config->testSpec();
        if( !testSpec.hasFilters() )
            testSpec = TestSpecParser( ITagAliasRegistry::get() ).parse( "~[.]" ).testSpec(); // All not hidden tests

        std::vector<TestCase> const& allTestCases = getAllTestCasesSorted( *iconfig );
        for( std::vector<TestCase>::const_iterator it = allTestCases.begin(), itEnd = allTestCases.end();
                it != itEnd;
                ++it ) {
            if( !context.aborting() && matchTest( *it, testSpec, *iconfig ) )
                totals += context.runTest( *it );
            else
                reporter->skipTest( *it );
        }

        context.testGroupEnded( iconfig->name(), totals, 1, 1 );
        return totals;
    }

    void applyFilenamesAsTags( IConfig const& config ) {
        std::vector<TestCase> const& tests = getAllTestCasesSorted( config );
        for(std::size_t i = 0; i < tests.size(); ++i ) {
            TestCase& test = const_cast<TestCase&>( tests[i] );
            std::set<std::string> tags = test.tags;

            std::string filename = test.lineInfo.file;
            std::string::size_type lastSlash = filename.find_last_of( "\\/" );
            if( lastSlash != std::string::npos )
                filename = filename.substr( lastSlash+1 );

            std::string::size_type lastDot = filename.find_last_of( "." );
            if( lastDot != std::string::npos )
                filename = filename.substr( 0, lastDot );

            tags.insert( "#" + filename );
            setTags( test, tags );
        }
    }

    class Session : NonCopyable {
        static bool alreadyInstantiated;

    public:

        struct OnUnusedOptions { enum DoWhat { Ignore, Fail }; };

        Session()
        : m_cli( makeCommandLineParser() ),
        debug((new(AUTORELEASE)DebugManagerCatch)->setLevel(DebugSeverity::info)->setFormat("%m"_S))
        {
            if( alreadyInstantiated ) {
                std::string msg = "Only one instance of Catch::Session can ever be used";
                Catch::cerr() << msg << std::endl;
                throw std::logic_error( msg );
            }
            alreadyInstantiated = true;
            BaseObject::recentObjects = new std::unordered_set<BaseObject*>;
        }
        ~Session() {
            Catch::cleanUp();
            delete BaseObject::recentObjects;
            BaseObject::recentObjects = NULL;
        }

        void showHelp( std::string const& processName ) {
            Catch::cout() << "\nCatch v" << libraryVersion << "\n";

            m_cli.usage( Catch::cout(), processName );
            Catch::cout() << "For more detail usage please see the project docs\n" << std::endl;
        }

        int applyCommandLine( int argc, char const* const* const argv, OnUnusedOptions::DoWhat unusedOptionBehaviour = OnUnusedOptions::Fail ) {
            try {
                m_cli.setThrowOnUnrecognisedTokens( unusedOptionBehaviour == OnUnusedOptions::Fail );
                m_unusedTokens = m_cli.parseInto( Clara::argsToVector( argc, argv ), m_configData );
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

        int run( int argc, char const* const* const argv ) {

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
                BaseAutorelease;
                printf("Git ID: %s\n", version+12);
                if(!m_configData.debugLevel.empty())
                    (new(AUTORELEASE)DebugManagerStdout)->setFilter(STR(m_configData.debugLevel.c_str(), -1));
                gUnitTestDatabase.New(m_configData.md5DatabaseName.c_str(), m_configData.processName.c_str());
                seedRng( *m_config ); // needed or not ??

                if( m_configData.filenamesAsTags )
                    applyFilenamesAsTags( *m_config );

                // Handle list request
                if( Option<std::size_t> listed = list( config() ) )
                    return static_cast<int>( *listed );

                return static_cast<int>( runTests( m_config ).assertions.failed );
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
    if(Catch::ExpectingThrows::counter == 0 && Catch::getCurrentContext().getConfig()->shouldDebugBreak())
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

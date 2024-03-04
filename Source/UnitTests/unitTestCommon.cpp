#include "unitTestCommon.h"

#include "Platform/Headers/PlatformDefines.h"
#include "Core/Time/Headers/ProfileTimer.h"

#include <iostream>

namespace
{
#if defined(PLATFORM_TESTS)
    const char* TARGET_NAME_STR = "Platform";
#elif defined(ENGINE_TESTS)
    const char* TARGET_NAME_STR = "Engine";
#else
#error "Unknow UT build target!";
#endif
}

bool platformInitRunListener::PLATFORM_INIT = false;

void platformInitRunListener::PlatformInit()
{
    using namespace Divide;

    if ( !PLATFORM_INIT )
    {
        std::cout << Util::StringFormat( "[%s]: Init platform code for unit test run\n", TARGET_NAME_STR );
        const char* data[] = { "--disableCopyright" };
        ErrorCode err = Divide::PlatformInit( 1, const_cast<char**>(data) );

        if ( err != ErrorCode::NO_ERR )
        {
            std::cout << Util::StringFormat( "[%s]: Platform init error! [ %s ]", TARGET_NAME_STR, TypeUtil::ErrorCodeToString( err ) );
        }

        PLATFORM_INIT = true;
    }
}

void platformInitRunListener::testRunStarting( Catch::TestRunInfo const& )
{
    using namespace Divide;

    if ( _testTimer == nullptr) 
    {
        _testTimer = &Time::ADD_TIMER( "Unit Tests" );
    }

    std::cout << Util::StringFormat( "[%s]: Running unit tests.\n", TARGET_NAME_STR );

    Time::START_TIMER(*_testTimer);

}

void platformInitRunListener::testRunEnded( Catch::TestRunStats const& )
{
    using namespace Divide;

    Time::STOP_TIMER(*_testTimer);
    std::cout << Util::StringFormat( "[%s]: Finished running UTs in [%5.2f] ms. \n", TARGET_NAME_STR, Time::MicrosecondsToMilliseconds<float>( Time::QUERY_TIMER( *_testTimer ) ) );
    if ( _testTimer != nullptr )
    {
        Divide::Time::REMOVE_TIMER(_testTimer);
    }

    if ( PLATFORM_INIT && !PlatformClose() )
    {
        std::cout << Util::StringFormat( "[%s]: Platform close error!\n", TARGET_NAME_STR );
        PLATFORM_INIT = false;
    }

    std::cout << Util::StringFormat( "[%s]: Shuting down ...\n", TARGET_NAME_STR );
}

#pragma once

// Addition to CATCH: ExpectingThrows is a class to inhibit Debugs() warnings / errors in unit tests

namespace Catch {
    class ExpectingThrows
    {
    public:
        ExpectingThrows() { counter++; }
        ~ExpectingThrows() { counter--; }
        static int counter;
    };
    class ExpectsDebugsError
    {
    public:
        ExpectsDebugsError(DebugSeverity severity_ = DebugSeverity::error, U32 minError=1, U32 maxError=10)
        : severity(severity_), minErrorCnt(minError), maxErrorCnt(maxError)
        {
            U32 mask = 1 << (int)severity;
            INTERNAL_CATCH_TEST((capturingMask & mask) == 0, Catch::ResultDisposition::Normal, "CHECK");
            capturingMask |= mask;
            counters[(int)severity] = 0;
        }
        
        ~ExpectsDebugsError()
        {
            U32 errorCnt = counters[(int)severity];
            INTERNAL_CATCH_TEST(errorCnt >= minErrorCnt, Catch::ResultDisposition::ContinueOnFailure, "CHECK");
            INTERNAL_CATCH_TEST(errorCnt <= maxErrorCnt, Catch::ResultDisposition::ContinueOnFailure, "CHECK");
            capturingMask &= ~(1 << (int)severity);
        }
        static U32 capturingMask;
        static U16 counters[DebugSeveritiesCount];
    private:
        DebugSeverity severity;
        U32 minErrorCnt;
        U32 maxErrorCnt;
    };
}


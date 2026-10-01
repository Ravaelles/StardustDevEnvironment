#pragma once

#include <BWAPI.h>

namespace Log
{
    // Semantic log level. Presentation (e.g. console color) is derived from it,
    // so call sites don't deal with ANSI codes directly (SRP).
    enum class Level
    {
        Info,  // default, terminal default color
        Muted  // de-emphasized, e.g. gray on console; plain text in file
    };

    class LogWrapper
    {
    protected:
        std::ostringstream *os;
        int *refCount;
        std::ofstream *logFile;
        bool outputToConsole;
        Level level;

    public:

        LogWrapper(std::ofstream *logFile, bool outputToConsole, Level level = Level::Info);

        LogWrapper(const LogWrapper &other);

        ~LogWrapper();

        template<typename T> LogWrapper &operator<<(T const &value)
        {
            if (logFile)
            {
                (*os) << value;
            }
            return *this;
        }

        LogWrapper &operator=(const LogWrapper &) = delete;
    };

    void initialize();

    void SetOutputToConsole(bool outputToConsole);

    LogWrapper Get(Level level = Level::Info);

    std::string &LogFileName();
}
#include "Log.h"

#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <iomanip>

namespace Log
{
    namespace
    {
        bool isOutputtingToConsole = false;
        std::chrono::system_clock::time_point startTime;
        std::string logFileName;
        std::ofstream *log;
    }

    LogWrapper::LogWrapper(std::ofstream *logFile, bool outputToConsole, Level level)
            : os(new std::ostringstream)
            , refCount(new int(1))
            , logFile(logFile)
            , outputToConsole(outputToConsole)
            , level(level)
    {
        if (!logFile) return;

        int seconds = BWAPI::Broodwar->getFrameCount() / 24;
        int minutes = seconds / 60;
        seconds = seconds % 60;
        (*os) << BWAPI::Broodwar->getFrameCount() << "(" << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << "): ";
    }

    LogWrapper::LogWrapper(const LogWrapper &other)
            : os(other.os)
            , refCount(other.refCount)
            , logFile(other.logFile)
            , outputToConsole(other.outputToConsole)
            , level(other.level)
    {
        ++*refCount;
    }

    namespace
    {
        bool ansiColorsEnabled()
        {
            // Respect NO_COLOR convention (https://no-color.org/)
            return std::getenv("NO_COLOR") == nullptr;
        }
    }

    LogWrapper::~LogWrapper()
    {
        --*refCount;

        if (*refCount == 0)
        {
            if (outputToConsole)
            {
                if (level == Level::Muted && ansiColorsEnabled())
                {
                    std::cout << "\033[90m" << os->str() << "\033[0m" << std::endl;
                }
                else
                {
                    std::cout << os->str() << std::endl;
                }
            }

            if (logFile)
            {
                (*os) << "\n";
                (*logFile) << os->str();
                logFile->flush();
            }

            delete os;
            delete refCount;
        }
    }

    void initialize()
    {
        startTime = std::chrono::system_clock::now();

        try
        {
            if (log)
            {
                log->close();
                log = nullptr;
            }
        }
        catch (std::exception &ex)
        {
            // Ignore
        }
    }

    void SetOutputToConsole(bool outputToConsole)
    {
        isOutputtingToConsole = outputToConsole;
    }

    LogWrapper Get(Level level)
    {
        if (!log)
        {
            std::ostringstream filename;
            filename << "bwapi-data/write/DemoAI_log";
            auto tt = std::chrono::system_clock::to_time_t(startTime);
            auto tm = std::localtime(&tt);
            filename << "_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".txt";
            logFileName = filename.str();

            log = new std::ofstream();
            log->open(logFileName, std::ofstream::trunc);
        }

        return LogWrapper(log, isOutputtingToConsole, level);
    }

    std::string &LogFileName()
    {
        return logFileName;
    }
}
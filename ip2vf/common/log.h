#ifndef _LOG_H
#define _LOG_H

#include <string>
#include <stdexcept>
#ifdef _WIN32

#ifdef VMILIBRARY_EXPORTS
#pragma once
#define VMILIBRARY_API_LOG __declspec(dllexport)
#else
#pragma once
#define VMILIBRARY_API_LOG __declspec(dllimport)
#endif

#else
#define VMILIBRARY_API_LOG 
#endif

enum LogColor {
    LOG_COLOR_BLACK  = 30,
    LOG_COLOR_RED    = 31,
    LOG_COLOR_GREEN  = 32,
    LOG_COLOR_YELLOW = 33,
    LOG_COLOR_BLUE   = 34,
    LOG_COLOR_PURPLE = 35,
    LOG_COLOR_CYAN   = 36,
    LOG_COLOR_WHITE  = 37,
};
enum LogLevel { 
    LOG_LEVEL_DISABLED    = -1,
    LOG_LEVEL_ERROR       = 0, 
    LOG_LEVEL_INFO        = 1, 
    LOG_LEVEL_WARNING     = 2, 
    LOG_LEVEL_VERBOSE     = 3,
};

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

inline std::string methodName(const std::string& prettyFunction)
{
    size_t begin, end;
    size_t colons = prettyFunction.find("::");
    if (colons == std::string::npos) {
        begin = prettyFunction.find(" ") + 1;
        end = prettyFunction.rfind("(") - begin;
    }
    else {
        begin = prettyFunction.substr(0, colons).rfind(" ") + 1;
        end = prettyFunction.rfind("(") - begin;
    }

    return prettyFunction.substr(begin, end) + "()";
}
#define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)

LogLevel getLogLevel();

void setLogLevel(LogLevel level);

VMILIBRARY_API_LOG void internal_LOG(std::string method, const char* message, ...);
VMILIBRARY_API_LOG void internal_LOG_INFO(std::string method, const char* message, ...);
VMILIBRARY_API_LOG void internal_LOG_COLOR(std::string method, LogColor color, const char* message, ...);
VMILIBRARY_API_LOG void internal_LOG_WARNING(std::string method, const char* message, ...);
VMILIBRARY_API_LOG void internal_LOG_ERROR(std::string method, const char* message, ...);
VMILIBRARY_API_LOG void internal_LOG_DUMP(std::string method, const char* buffer, int size);
VMILIBRARY_API_LOG void internal_LOG_DUMP10BITS(std::string method, const char* buffer, int size);

#define LOG(msg, ...)           internal_LOG(__METHOD_NAME__, msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...)      internal_LOG_INFO(__METHOD_NAME__, msg, ##__VA_ARGS__)
#define LOG_COLOR(color, msg, ...)     internal_LOG_COLOR(__METHOD_NAME__, color, msg, ##__VA_ARGS__)
#define LOG_WARNING(msg, ...)   internal_LOG_WARNING(__METHOD_NAME__, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...)     internal_LOG_ERROR(__METHOD_NAME__, msg, ##__VA_ARGS__)
#define LOG_DUMP(buffer, size)  internal_LOG_DUMP(__METHOD_NAME__, buffer, size)
#define LOG_DUMP10BITS(buffer, size)   internal_LOG_DUMP10BITS(__METHOD_NAME__, buffer, size)

/////////////////////////////////////////
// http://stackoverflow.com/questions/2670816/how-can-i-use-the-compile-time-constant-line-in-a-string
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)
/////////////////////////////////////////


/**
 * A class to throw runtime errors while logging the output to the right place first
 */
class criticalException : public std::runtime_error {
public:
    criticalException(const std::string& message): std::runtime_error(message){
        LOG_ERROR(message.c_str());
    }
    
};

#define THROW_CRITICAL_EXCEPTION(message) throw criticalException(std::string(message)+ " "+__FILE__ +" "+ LINE_STRING +" "+ __FUNCTION__ )

#define ASSERT_CRITICAL(cond,message) {if(!(cond))throw criticalException(std::string(message)+ __FILE__ + LINE_STRING + __FUNCTION__ );}

#ifdef _WIN32
//
// Windows has a bug where if you redirect stderr it does not output everything...
// And so, when the process is termintated from the outside, some of the output can be missing.
// This causes problems with the validation scripts
// When in windows, this function should be called once in a while...
//
#define WIN32_HOTFIX_FLUSH_OUTPUT() fflush(stderr);fflush(stdout);
#else
#define WIN32_HOTFIX_FLUSH_OUTPUT()
#endif
#endif //_LOG_H

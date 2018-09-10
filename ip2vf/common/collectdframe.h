/*
 * collectdframe.h
 *
 *  Created on: Oct 24, 2017
 *      Author: mhawari
 */

#ifndef COMMON_COLLECTDFRAME_H_
#define COMMON_COLLECTDFRAME_H_
#include <vector>
#include <chrono>
#include <string>
typedef enum
    : unsigned char
    {
        COLLECTD_DATACODE_COUNTER = 0,
    COLLECTD_DATACODE_GAUGE = 1,
    COLLECTD_DATACODE_DERIVE = 2,
    COLLECTD_DATACODE_ABSOLUTE = 3

} CollectdDataCode;
class CollectdFrame
{
public:
    CollectdFrame(const char* plugin, const char* plugin_instance);
    unsigned char* getBuffer();
    int getLen();
    void setTimestamp(const std::chrono::high_resolution_clock::time_point &date);
    void setType(const char* type);
    void setTypeInstance(const char* typeinstance);
    void addRecord(CollectdDataCode dc, void* data);
    void addRecordn(CollectdDataCode dc, void* data, uint16_t n);
    void resetFrame();
private:
    std::string _plugin;
    std::string _plugin_instance;
    std::string _hostname;
    std::string _currentType;
    std::string _currentTypeInstance;
    std::chrono::high_resolution_clock::time_point _currentDate;
    std::vector<unsigned char> _buffer;
    static constexpr double _timeConversionFactor = (1e-9)*(1 << 30);
};

#endif /* COMMON_COLLECTDFRAME_H_ */

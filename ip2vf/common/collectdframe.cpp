/*
 * collectdframe.cpp
 *
 *  Created on: Oct 24, 2017
 *      Author: mhawari
 */

#ifndef _WIN32
#include <unistd.h>
#else
#include <WinSock2.h>
#endif
#include <cstring>
#ifndef _WIN32
#include <endian.h>
#else
#include <intrin.h>
#define htobe64(x) _byteswap_uint64(x)
#endif
#include "collectdframe.h"
#include "log.h"
#define PART_TYPE_HOST 0x0000
#define PART_TYPE_PLUGIN 0x0002
#define PART_TYPE_PLUGIN_INSTANCE 0x0003
#define PART_TYPE_TYPE 0x0004
#define PART_TYPE_TYPE_INSTANCE 0x0005
#define PART_TYPE_VALUE 0x0006
#define PART_TYPE_TIME_HR 0x0008
#define MAX_HOSTNAME_SIZE 128

#define ADD_STRING(ptype, s) do { \
    unsigned char * __c; \
    _buffer.push_back((unsigned char)( (ptype) >> 8));\
    _buffer.push_back((unsigned char)( (ptype) & 0xff));\
    _buffer.push_back((unsigned char)( ( 5 + (s).length() ) >> 8 ));\
    _buffer.push_back((unsigned char)( ( 5 + (s).length() ) & 0xff )); \
    __c = ((unsigned char*)(s).c_str()) - 1; \
    do \
    { \
        __c++; \
        _buffer.push_back(*__c); \
    } while (*__c != 0); \
} while(0);

CollectdFrame::CollectdFrame(const char *plugin, const char* plugin_instance) :
        _plugin(plugin), _plugin_instance(plugin_instance), _currentType(""), _currentTypeInstance(
                "")
{
    resetFrame();
    char hostname[MAX_HOSTNAME_SIZE] =
    { };
    gethostname(hostname, MAX_HOSTNAME_SIZE);
    _hostname.assign(hostname);
}

void CollectdFrame::resetFrame()
{
    _buffer.resize(0);
    _currentType.assign("");
    _currentTypeInstance.assign("");
    _currentDate = std::chrono::high_resolution_clock::time_point();
    ADD_STRING(PART_TYPE_HOST, _hostname)
    ADD_STRING(PART_TYPE_PLUGIN, _plugin)
    ADD_STRING(PART_TYPE_PLUGIN_INSTANCE, _plugin_instance)

}

unsigned char* CollectdFrame::getBuffer()
{
    return _buffer.data();
}

int CollectdFrame::getLen()
{
    return (int)_buffer.size();
}

void CollectdFrame::setType(const char* type)
{
    if (!_currentType.compare(type))
        return;
    _currentType.assign(type);
    ADD_STRING(PART_TYPE_TYPE, _currentType)
}

void CollectdFrame::setTypeInstance(const char* typeinstance)
{
    if (!_currentTypeInstance.compare(typeinstance))
        return;
    _currentTypeInstance.assign(typeinstance);
    ADD_STRING(PART_TYPE_TYPE_INSTANCE, _currentTypeInstance)
}

void CollectdFrame::addRecord(CollectdDataCode dc, void* data)
{
    _buffer.push_back((unsigned char) (PART_TYPE_VALUE >> 8));
    _buffer.push_back((unsigned char) (PART_TYPE_VALUE & 0xff));
    _buffer.push_back((unsigned char) (15 >> 8));
    _buffer.push_back((unsigned char) (15 & 0xff));
    _buffer.push_back((unsigned char) (1 >> 8));
    _buffer.push_back((unsigned char) (1 & 0xff));
    _buffer.push_back((unsigned char) (dc));
    if (dc == COLLECTD_DATACODE_GAUGE)
    {
        _buffer.resize(_buffer.size() + 8);
        std::memcpy((void *) (_buffer.data() + _buffer.size() - 8), data, 8);
    }
    else
    {
        for (int i = 0; i<8; i++)
            _buffer.push_back(((unsigned char*)data)[7-i]);
    }
}

void CollectdFrame::addRecordn(CollectdDataCode dc, void* data, uint16_t n)
{
    uint16_t partlen = 6 + n * 9;

    _buffer.push_back((unsigned char) (PART_TYPE_VALUE >> 8));
    _buffer.push_back((unsigned char) (PART_TYPE_VALUE & 0xff));
    _buffer.push_back((unsigned char) (partlen >> 8));
    _buffer.push_back((unsigned char) (partlen & 0xff));
    _buffer.push_back((unsigned char) (n >> 8));
    _buffer.push_back((unsigned char) (n & 0xff));

    for (int j = 0; j < n; j++)
    {
        _buffer.push_back((unsigned char) (dc));
    }
    for (int j = 0; j < n; j++)
    {
        if (dc == COLLECTD_DATACODE_GAUGE)
        {
            _buffer.resize(_buffer.size() + 8);
            std::memcpy((void *) (_buffer.data() + _buffer.size() - 8), data, 8);
        }
        else
        {
            for (int i = 0; i<8; i++)
                _buffer.push_back(((unsigned char*)data)[7-i]);
        }
        data = ((unsigned char *) data) + 8;
    }
}

void CollectdFrame::setTimestamp(const std::chrono::high_resolution_clock::time_point &date)
{
    if (date == _currentDate)
        return;
    _currentDate = date;
    uint64_t currentDateInCollectdUnit = (uint64_t) (_timeConversionFactor
            * (double) std::chrono::duration_cast<std::chrono::nanoseconds>(
                    _currentDate.time_since_epoch()).count());
    _buffer.push_back((unsigned char) (PART_TYPE_TIME_HR >> 8));
    _buffer.push_back((unsigned char) (PART_TYPE_TIME_HR & 0xff));
    _buffer.push_back((unsigned char) (12 >> 8));
    _buffer.push_back((unsigned char) (12 & 0xff));
    _buffer.resize(_buffer.size() + 8);
    currentDateInCollectdUnit = htobe64(currentDateInCollectdUnit);
    std::memcpy((void*)(_buffer.data() +_buffer.size() - 8), &currentDateInCollectdUnit, 8);
}


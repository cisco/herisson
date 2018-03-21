#ifndef _MODULECONFIG_H
#define _MODULECONFIG_H

#include <vector>
#include <unordered_map>
#include "common.h"
#include "log.h"

#define MAX_CONFIG_STRING_LENGTH   128

struct PinConfiguration {
    char     _type[MAX_CONFIG_STRING_LENGTH];        // Type of PIN (cf enum PinType on common.h)
    int     _vidfrmsize;
    std::unordered_map<std::string, std::string> _dynamicProperties;
    PinConfiguration() {
        _type[0]      = '\0';
        _vidfrmsize   = 0;        
    };
    void dump() {
        LOG("       type      = %d\n", _type);
        //TODO:Dump pin config
    };
};

struct tPinName {
    PinType type;
    char name[16];
};
extern tPinName g_pinsNameArray[];

class CModuleConfiguration 
{
public:
    int            _id;
    std::string    _name;
    int            _logLevel;
    std::string    _collectdip;
    int            _collectdport;


    // input parameters
    std::vector<PinConfiguration> _in;

    // outputs parameters
    std::vector<PinConfiguration> _out;

public:
    CModuleConfiguration();
    CModuleConfiguration(const char* msg);
    ~CModuleConfiguration();

public:
    void initFromConfig(const char* msg);
    void dump();
    CModuleConfiguration& operator=(const CModuleConfiguration& copy);
    PinConfiguration* addNewOutputConfig();
    PinConfiguration* addNewInputConfig();

    static PinType     GetPinTypeFromName(const std::string& name);
    static const char* GetPinNameFromType(const PinType type);

protected:
    void reset();

};

#endif //_MODULECONFIG_H

#include <cstdio>

#include <cstdlib>
#include <cstring>       // strdup

#include "log.h"
#include "tools.h"
#include "frameheaders.h"
#include "moduleconfiguration.h"

#define DEFAULT_SUPERVIZION_PORT   5432
#define DEFAULT_SUPERVIZION_IP   "::1"

#define GET_INT____FROM_PARAM(a)    try { \
                                        a = std::stoi(params[1]); \
                                    } catch(...) { \
                                        LOG_ERROR("failed to convert parameter '%s' to integer", params[1].c_str()); \
                                    }
#define GET_PINTYP_FROM_PARAM(a)    try { \
                                        a = (PinType)std::stoi(params[1]); \
                                    } catch(...) { \
                                        LOG_ERROR("failed to convert parameter '%s' to PinType", params[1].c_str()); \
                                    }
#define GET_LL_____FROM_PARAM(a)    try { \
                                        a = std::stoll(params[1]); \
                                    } catch(...) { \
                                        LOG_ERROR("failed to convert parameter '%s' to long long", params[1].c_str()); \
                                    }
#define GET_STRING_FROM_PARAM(a)    strncpy(a, params[1].c_str(), MAX_CONFIG_STRING_LENGTH); 
#define GET_STD_STRING_FROM_PARAM(a) a = params[1];
#define GET_IPPORT_FROM_PARAM(a,b)  try { \
                                        size_t colonPos = params[1].find(':'); \
                                        if(colonPos != std::string::npos) { \
                                            strncpy(a, params[1].substr(0,colonPos).c_str(), MAX_CONFIG_STRING_LENGTH); \
                                            b = std::stoi(params[1].substr(colonPos+1)); \
                                        } else { \
                                            strncpy(a, params[1].c_str(), MAX_CONFIG_STRING_LENGTH); \
                                        } \
                                    } catch(...) { \
                                        LOG_ERROR("failed to convert parameter '%s' to 'ip[:port]'", params[1].c_str()); \
                                    }

tPinName g_pinsNameArray[] = {
    { PIN_TYPE_TCP,        "tcp" },
    { PIN_TYPE_SHMEM,      "shmem" },
    { PIN_TYPE_FILE,       "file" },
    { PIN_TYPE_DEVNULL,    "devnull" },
    { PIN_TYPE_RTP,        "rtp" },
    { PIN_TYPE_SMPTE,      "smpte" },
    { PIN_TYPE_TR03,       "tr03" },
    { PIN_TYPE_TCP_THUMB,  "thumbnails" },
    { PIN_TYPE_RAWX264,    "x264" },
    { PIN_TYPE_AES67,      "aes67"}
};

CModuleConfiguration::CModuleConfiguration()
{
    reset();
}

CModuleConfiguration::CModuleConfiguration(const char* msg)
{
    reset();
    initFromConfig(msg);
}

void CModuleConfiguration::initFromConfig(const char* msg)
{
    PinConfiguration* pin = NULL;
    std::string s = msg;
    std::vector<std::string> tokens = tools::split(s, ',');
    for (int i = 0; i < (int)tokens.size(); i++)
    {
        if (tokens[i].empty()) {
            LOG_INFO("Empty token detected");
            continue;
        }

        //LOG("token[%d]='%s'\n", i, tokens[i].c_str() );
        std::vector<std::string> params = tools::split(tokens[i], '=');

        if (params.size() != 2) {
            LOG_ERROR("Invalid parameter format: '%s' is not in format '<param>=<value>'", tokens[i].c_str());
            continue;
        }

        // Manage global parameters:
        if(      params[0].compare("id")        == 0)    { GET_INT____FROM_PARAM(_id);        }
        else if (params[0].compare("name")      == 0)    { GET_STD_STRING_FROM_PARAM(_name);  }
        else if (params[0].compare("loglevel")  == 0)    { GET_INT____FROM_PARAM(_logLevel);  }
        else if (params[0].compare("collectdip")   == 0) { GET_STD_STRING_FROM_PARAM(_collectdip); }
        else if (params[0].compare("collectdport") == 0) { GET_INT____FROM_PARAM(_collectdport); }
        else if (params[0].compare("out_type")  == 0)    {
            pin = addNewOutputConfig();
            strncpy(pin->_type, params[1].c_str(), sizeof(pin->_type));
        }
        else if (params[0].compare("in_type") == 0) {
            pin = addNewInputConfig();
            strncpy(pin->_type, params[1].c_str(), sizeof(pin->_type));
        }
        // Manage Pin parameters:
        else if (params[0].compare("vidfrmsize")== 0) { if (pin) GET_INT____FROM_PARAM(pin->_vidfrmsize);   }
        else pin->_dynamicProperties[params[0]] = params[1];
    }
};


CModuleConfiguration::~CModuleConfiguration() {
};


void CModuleConfiguration::reset() {
    _id = -1;
    _logLevel = LOG_LEVEL_VERBOSE;
    _collectdip = DEFAULT_SUPERVIZION_IP;
    _collectdport = DEFAULT_SUPERVIZION_PORT;
    _in.clear();
    _out.clear();
};


void CModuleConfiguration::dump() 
{
    LOG("---- this=%p\n", this);
    LOG("    id           = %d\n", _id);
    LOG("    name         = %s\n", _name.c_str());
    LOG("    loglevel     = %d\n", _logLevel);
    LOG("    collectdip   = %s\n", _collectdip.c_str());
    LOG("    collectdport = %d\n", _collectdport);
    LOG("    inputs       = %d\n", (int)_in.size());
    for (int i = 0; i < (int)_in.size(); i++) {
        LOG("    IN-%d\n", i);
        LOG("       type      = %s\n", _in[i]._type);
        //TODO:dump
        LOG("       vidfrmsize= %d\n"     , _in[i]._vidfrmsize);
    }
    LOG("    outputs      = %d\n", (int)_out.size());
    for( int i=0; i<(int)_out.size(); i++ ) {
        LOG("    OUT-%d\n", i);
        LOG("       type      = %s\n", _out[i]._type);
        //TODO: dump
        LOG("       vidfrmsize= %d\n"     , _out[i]._vidfrmsize);
    }
};


CModuleConfiguration& CModuleConfiguration::operator=(const CModuleConfiguration& copy)
{
    _name       = copy._name;
    _id         = copy._id;
    _logLevel   = copy._logLevel;
    _collectdip    = copy._collectdip;
    _collectdport  = copy._collectdport;

    for (int i = 0; i<(int)copy._in.size(); i++)
        _in.push_back(copy._in[i]);

    for( int i=0; i<(int)copy._out.size(); i++ )
        _out.push_back( copy._out[i] );
    
    return *this;
}


PinConfiguration* CModuleConfiguration::addNewOutputConfig()
{
    PinConfiguration out;
    _out.push_back(out);
    if( _out.size()>0 )
        return &_out[_out.size()-1];
    return NULL;
}

PinConfiguration* CModuleConfiguration::addNewInputConfig()
{
    PinConfiguration in;
    _in.push_back(in);
    if (_in.size()>0)
        return &_in[_in.size() - 1];
    return NULL;
}

PinType CModuleConfiguration::GetPinTypeFromName(const std::string& name) {
    PinType type = PIN_TYPE_NONE;
    int nbKnownPin = sizeof(g_pinsNameArray) / sizeof(struct tPinName);
    for (int i = 0; i < nbKnownPin; i++) {
        if (tools::noCaseCompare(name, g_pinsNameArray[i].name))
            return g_pinsNameArray[i].type;
    }
    return type;
}

const char* CModuleConfiguration::GetPinNameFromType(const PinType type) {
    int nbKnownPin = sizeof(g_pinsNameArray) / sizeof(struct tPinName);
    for (int i = 0; i < nbKnownPin; i++) {
        if (type == g_pinsNameArray[i].type)
            return (const char*) g_pinsNameArray[i].name;
    }
    return NULL;
}

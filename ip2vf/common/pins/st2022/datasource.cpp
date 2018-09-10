#include <cstdio>
#include <cstdlib>
#include <cstring>      // strerror
#include <string>
#include <fstream>      // for file saving
#include <iostream>     // for file saving
#include <sys/types.h>  // open
#include <sys/stat.h>   // open
#include <fcntl.h>      // open

#include "common.h"
#include "log.h"
#include "tools.h"
#include "datasource.h"
#include "moduleconfiguration.h"
#include "configurable.h"
using namespace std;

#define PCAP_FILE_HEADER_SIZE       24
#define PCAP_PACKET_OFFSET          58

CDMUXDataSource::CDMUXDataSource() : _pConfig(nullptr), _type(DataSourceType::TYPE_SOCKET){
}

CDMUXDataSource* CDMUXDataSource::create(PinConfiguration *pconfig)
{
    CDMUXDataSource* source = NULL;
    struct DataSourcePin {
        const char * _filename;
        int _port;
        int _port2;
        int _cached;
        int _rio;
        PinConfiguration *_pConfig;
        PinConfiguration * getConfiguration() {
            return _pConfig;
        }
        DataSourcePin(PinConfiguration *pConfig) : _pConfig(pConfig) {
            PROPERTY_REGISTER_OPTIONAL("filename", _filename, "");
            PROPERTY_REGISTER_OPTIONAL("port", _port, -1);
            PROPERTY_REGISTER_OPTIONAL("port2", _port2, -1);
            PROPERTY_REGISTER_OPTIONAL("cached", _cached, -1);
            PROPERTY_REGISTER_OPTIONAL("rio", _rio, -1);
        }
    } dsPin(pconfig);

    if (dsPin._filename != NULL && strlen(dsPin._filename) > 0 && dsPin._cached == 1)
        source = new CCachedFileDataSource();
    else if (dsPin._filename != NULL && strlen(dsPin._filename) > 0)
        source = new CFileDataSource();
    else if (dsPin._port > -1 && dsPin._port2 > -1)
        source = new CSPSRTPDataSource();
#ifdef _WIN32
    else if (dsPin._port > -1 && dsPin._rio > -1)
        source = new CRIODataSource();
#endif // _WIN32
    else if (dsPin._port > -1)
        source = new CRTPDataSource();
    else {
        LOG_ERROR("Can't detect the kind of source from Pin configuration");
    }

    if (source) {
        source->_pConfig = pconfig;
        source->init(pconfig);
    }

    return source;
}



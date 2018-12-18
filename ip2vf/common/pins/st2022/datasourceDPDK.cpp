#include <cstdio>
#include <cstdlib>
#include <string>

#include "common.h"
#include "log.h"
#include "tools.h"
#include "datasource.h"
#include "moduleconfiguration.h"
#include "configurable.h"

using namespace std;

#ifdef _WIN32
#define inet_aton(a, b)   inet_pton(AF_INET, a, b)
#endif	// _WIN32

#ifdef _USE_LIBEGEL

extern "C" {
#include "libegel.h"
}

#define INVALID_SLOT_HANDLE -1

#define IPv4(a,b,c,d)						\
	((uint32_t)(((a) & 0xff) << 24)			\
	|(((b) & 0xff) << 16)					\
	|(((c) & 0xff) << 8)					\
	|((d) & 0xff))


void replaceAll(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}

CDPDKDataSource::CDPDKDataSource()
	: CDMUXDataSource(),
	_init(false),
	_slot_init(false),
	_firstPacket(true),
	_libegel_slot_handle(INVALID_SLOT_HANDLE),
	_libegel_pkts(NULL),
	_libegel_pkts_nb(0) {

	_samplesize = RTP_PACKET_SIZE;  // by default, will be refresh 
	_type = DataSourceType::TYPE_DPDK;
}

CDPDKDataSource::~CDPDKDataSource() {

	if(_slot_init && _libegel_slot_handle != INVALID_SLOT_HANDLE)
		libegel_delete_slot(_libegel_slot_handle);
	_libegel_slot_handle = INVALID_SLOT_HANDLE;
	if(_libegel_pkts != NULL)
		delete[] _libegel_pkts;
}

void CDPDKDataSource::init(PinConfiguration *pconfig) {

	if (_init)
		return;

	_pConfig = pconfig;
	PROPERTY_REGISTER_MANDATORY("port", _port, -1);
	PROPERTY_REGISTER_OPTIONAL("mcastgroup", _mcastgroup, "");
	PROPERTY_REGISTER_MANDATORY("ip", _ip, "");
	PROPERTY_REGISTER_MANDATORY("pci", _pci_device, "");
	PROPERTY_REGISTER_MANDATORY("nbpkts", _nbpkts, 0);
	PROPERTY_REGISTER_OPTIONAL("eal", _eal_config, "");
	if (_port == -1) {
		LOG_ERROR("Invalid configuration. Exit. (port=%d)", _port);
	}

	// Replace any "%20" to " " on EAL string
	std::string s(_eal_config);
	replaceAll(s, "%20", " ");
	replaceAll(s, "%2D", "-");
	replaceAll(s, "%3D", "=");

	// This allow to setup a network DPDK stream 
	LOG_INFO("DPDK configuration:");
	LOG_INFO(" - port=%d", _port);
	LOG_INFO(" - mcast to join='%s'", _mcastgroup);
	LOG_INFO(" - sending IP='%s'", _ip);
	LOG_INFO(" - PCI device='%s'", _pci_device);
	LOG_INFO(" - EAL config='%s'", s.c_str());

	_libegel_pkts = new void*[_nbpkts];

	/* Initialize the Environment Abstraction Layer (EAL). */
	struct eg_config config;
	config.eal_config_str = (char*)s.c_str();
	eg_error r = libegel_init(&config);
	LOG_INFO("libegel_init(%s) returns %d", config.eal_config_str, r);

	_init = true;
}

void CDPDKDataSource::waitForNextFrame()
{
	// Do nothing for this source
}

int CDPDKDataSource::read(char* buffer, int size)
{
	int result = -1;

	if (!_slot_init) {

		// Get a new slot from libegel
		/*_libegel_slot_handle = libegel_new_slot();
		if (_libegel_slot_handle == INVALID_SLOT_HANDLE) {
			LOG_ERROR("libegel_new_slot return INVALID_SLOT_HANDLE. Can't configurate libegel...");
			return -1;
		}*/

		// Configure the slot
		struct eg_slot_config def_slot_config;
		def_slot_config.iface_pci_addr = _pci_device;
		inet_aton(_ip, (struct in_addr*)&def_slot_config.iface_ip_addr);
		inet_aton(_mcastgroup, (struct in_addr*)&def_slot_config.mcast_group);
		def_slot_config.udp_port = htons(_port);

		_libegel_slot_handle = libegel_config_slot(&def_slot_config);
		LOG_INFO("libegel_config_slot returns %d\n", _libegel_slot_handle);

		_libegel_pkts_ptr = 0;
		_libegel_pkts_nb = 0;

		_slot_init = true;
		LOG_INFO("init slot ok");
	}

	if (_slot_init) {

		// Get a new array of packets, if no more packets available.
		if (_libegel_pkts_ptr == 0) {
			_libegel_pkts_nb = libegel_rx_pkts_burst(_libegel_slot_handle, _libegel_pkts, _nbpkts);
			_libegel_pkts_ptr = _libegel_pkts_nb;
			if (_libegel_pkts_nb == 0) {
				LOG_ERROR("libegel_rx_pkts_burst return %d", _libegel_pkts_nb);
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				return VMI_E_PACKET_LOST;
			}
		}

		// Get data from next available packets
		if (_libegel_pkts_ptr > 0) {
			int index = _libegel_pkts_nb - _libegel_pkts_ptr;
			int payload_len = libegel_get_udp_payload_len(_libegel_slot_handle, _libegel_pkts[index]);
			uint8_t* ptr = libegel_get_udp_payload(_libegel_slot_handle, _libegel_pkts[index]);
			memcpy(buffer, (const char*)ptr, payload_len);
			libegel_free_pkt(_libegel_slot_handle, _libegel_pkts[index]);
			_libegel_pkts_ptr--;
			result = payload_len;
			//LOG("read %d, wanted size=%d", result, size);
		}
	}
	return result;
}

void CDPDKDataSource::close()
{
	LOG_INFO("-->");
	// TODO: free any unused packet from a "working" burst
	if (_slot_init) {
		libegel_delete_slot(_libegel_slot_handle);
		_libegel_slot_handle = INVALID_SLOT_HANDLE;
		_slot_init = false;
	}
	LOG_INFO("<--");
}

#endif // _USE_LIBEGEL

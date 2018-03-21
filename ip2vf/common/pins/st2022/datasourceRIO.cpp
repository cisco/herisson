#ifdef _WIN32
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
// AllocateBufferSpace() from http://www.serverframework.com/asynchronousevents/rio/
template <typename TV, typename TM>
inline TV RoundDown(TV Value, TM Multiple)
{
	return((Value / Multiple) * Multiple);
}
template <typename TV, typename TM>
inline TV RoundUp(TV Value, TM Multiple)
{
	return(RoundDown(Value, Multiple) + (((Value % Multiple) > 0) ? Multiple : 0));
}

static char* AllocateBufferSpace(const DWORD bufSize, const DWORD bufCount, DWORD& totalBufferSize, DWORD& totalBufferCount)
{
	SYSTEM_INFO systemInfo;
	::GetSystemInfo(&systemInfo);
	const unsigned __int64 granularity = systemInfo.dwAllocationGranularity;
	const unsigned __int64 desiredSize = bufSize * bufCount;
	unsigned __int64 actualSize = RoundUp(desiredSize, granularity);
	if (actualSize > UINT_MAX)
	{
		actualSize = (UINT_MAX / granularity) * granularity;
	}
	totalBufferCount = min(bufCount, static_cast<DWORD>(actualSize / bufSize));
	totalBufferSize = static_cast<DWORD>(actualSize);
	char* pBuffer = reinterpret_cast<char*>(VirtualAllocEx(GetCurrentProcess(), 0, totalBufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (pBuffer == 0)
	{
		LOG_ERROR("VirtualAllocEx Error: %d\n", WSAGetLastError());
		return NULL;
	}
	return pBuffer;
}

CRIODataSource::CRIODataSource() 
    : CDMUXDataSource() 
{
    _samplesize = RTP_PACKET_SIZE;  // by default, will be refresh 
}

CRIODataSource::~CRIODataSource() {
    if ( this->UDP::isValid() ) {
        this->closeSocket();
    }
}

int CRIODataSource::initWSA()
{
	WSADATA data;
	if (0 != ::WSAStartup(0x202, &data))
	{
		LOG_ERROR("WSAStartup Error: %d\n", WSAGetLastError());
		return E_FATAL;
	}
	return E_OK;
}

int CRIODataSource::openSocket(const char* remote_addr, const char* local_addr, int port,
	bool modelisten, const char *ifname)
{
	/*
	* Create a new socket
	*/
	static int initWSAFailed = initWSA();
	if (initWSAFailed)
	{
		LOG_ERROR("Failed to init WSA API, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	int res = E_OK;
	res = resolveEndpoints(remote_addr, local_addr, port, modelisten, ifname);
	_sock = WSASocket(_af, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_REGISTERED_IO);
	if (_sock == INVALID_SOCKET)
	{
		LOG_ERROR("Failed to create socket, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	/*
	* Resolve sockets endpoints and configure the socket
	*/
	res = configureSocket(remote_addr, local_addr, port, modelisten, ifname);

	/*
	* Prepare RIO operations
	*/
	_functionTableId = WSAID_MULTIPLE_RIO;
	DWORD dwBytes = 0;
	if (NULL != WSAIoctl(_sock, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &_functionTableId, sizeof(GUID), (void**)&_rio, sizeof(_rio), &dwBytes, NULL, NULL))
	{
		LOG_ERROR("Failed to get RIO function table, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	/*
	 * For now let's not use completion notification aka let's do polling
	*/
	_completionQueue = _rio.RIOCreateCompletionQueue(_pendingRecvs, NULL);
	if (_completionQueue == RIO_INVALID_CQ)
	{
		LOG_ERROR("Failed to create RIO CQ, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	_requestQueue = _rio.RIOCreateRequestQueue(_sock, _pendingRecvs, 1, 0, 1, _completionQueue, _completionQueue, NULL);
	if (_requestQueue == RIO_INVALID_RQ)
	{
		LOG_ERROR("Failed to create RIO RQ, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	/*
	 * Prepare buffers for packets and addresses
	 */
	_addrRioBufIndex = 0;
	_recvRioBufIndex = 0;
	DWORD totalBufferCount = 0;
	DWORD totalBufferSize = 0;
	_addrBufferPtr = AllocateBufferSpace(sizeof(SOCKADDR_INET), _pendingRecvs, totalBufferSize, totalBufferCount);
	_addrBufferId = _rio.RIORegisterBuffer(_addrBufferPtr, static_cast<DWORD>(totalBufferSize));
	if (_addrBufferId == RIO_INVALID_BUFFERID)
	{
		LOG_ERROR("Failed to register address buffer, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	DWORD offset = 0;
	_addrRioBufs = new ERIO_BUF[totalBufferCount];
	_addrRioBufTotalCount = totalBufferCount;
	for (DWORD i = 0; i < _addrRioBufTotalCount; ++i)
	{
		ERIO_BUF *pBuffer = _addrRioBufs + i;
		pBuffer->BufferId = _addrBufferId;
		pBuffer->Offset = offset;
		pBuffer->Length = sizeof(SOCKADDR_INET);
		offset += sizeof(SOCKADDR_INET);
	}

	totalBufferCount = 0;
	totalBufferSize = 0;
	_recvBufferPtr = AllocateBufferSpace(2048, _pendingRecvs, totalBufferSize, totalBufferCount);
	_recvBufferId = _rio.RIORegisterBuffer(_recvBufferPtr, static_cast<DWORD>(totalBufferSize));
	if (_recvBufferId == RIO_INVALID_BUFFERID)
	{
		LOG_ERROR("Failed to register receive buffer, error=%d\n", WSAGetLastError());
		return E_FATAL;
	}
	offset = 0;
	_recvRioBufs = new ERIO_BUF[totalBufferCount];
	_recvRioBufTotalCount = totalBufferCount;
	for (DWORD i = 0; i < _recvRioBufTotalCount; ++i)
	{
		ERIO_BUF *pBuffer = _recvRioBufs + i;
		pBuffer->BufferId = _recvBufferId;
		pBuffer->Offset = offset;
		pBuffer->Length = 2048;
		offset += 2048;
		/* Also prepare recv requests*/
		if (!_rio.RIOReceiveEx(_requestQueue, pBuffer, 1, NULL, &_addrRioBufs[_addrRioBufIndex++], NULL, 0, 0, pBuffer))
		{
			LOG_ERROR("Failed to register read request, error=%d\n", WSAGetLastError());
			return E_FATAL;
		}
	}
	return res;
}

void CRIODataSource::init(PinConfiguration *pconfig)
{
    int result; 
    _pConfig = pconfig;
    PROPERTY_REGISTER_MANDATORY("port", _port, -1);
    PROPERTY_REGISTER_OPTIONAL("mcastgroup", _zmqip, "");
    PROPERTY_REGISTER_OPTIONAL("ip", _ip, "");
	PROPERTY_REGISTER_OPTIONAL("pendingRcvs", _pendingRecvs, 200000);
	PROPERTY_REGISTER_OPTIONAL("rioCore", _rioCore, -1);
    if (_port == -1) {
        LOG_ERROR("Invalid configuration. Exit. (port=%d)", _port);
    }

    // This allow to setup a network RTP stream 
    LOG_INFO("data stream from port '%d'",_port);

    if (!isValid())
        result = openSocket(_zmqip, _ip, _port, true);

    _firstPacket = true;
	_numReceived = 0;
}

void CRIODataSource::waitForNextFrame()
{
    // Do nothing for this source
}

int  CRIODataSource::readSocket(char *buffer, int *len)
{
	RIORESULT result[256];
	while (!_numReceived)
	{
		int res = _rio.RIODequeueCompletion(_completionQueue, result, 256);
		for (int i = 0; i < res; i++)
		{
			ERIO_BUF *pBuffer = reinterpret_cast<ERIO_BUF *>(result[i].RequestContext);
			pBuffer->pkt_len = result[i].BytesTransferred;
			if (!_rio.RIOReceiveEx(_requestQueue, pBuffer, 1, NULL, &_addrRioBufs[(_addrRioBufIndex + i) % _addrRioBufTotalCount], NULL, 0, 0, pBuffer))
			{
				LOG_ERROR("Failed to register read request, error=%d\n", WSAGetLastError());
				return E_FATAL;
			}
		}
		_numReceived = res;
	}

	if (_numReceived == RIO_CORRUPT_CQ)
	{
		LOG_ERROR("RIODequeueCompletion Error: %d\n", WSAGetLastError());
		return E_FATAL;
	}
	ERIO_BUF* pBuffer = &_recvRioBufs[_recvRioBufIndex];
	int len_to_copy = MIN(*len, pBuffer->pkt_len );
	*len = len_to_copy;
	memcpy(buffer, _recvBufferPtr + pBuffer->Offset, len_to_copy);
	_addrRioBufIndex++; _addrRioBufIndex %= _addrRioBufTotalCount;
	_recvRioBufIndex++; _recvRioBufIndex %= _recvRioBufTotalCount;
	_numReceived--;
	return len_to_copy;
}

int CRIODataSource::read(char* buffer, int size)
{
    int result = -1;

    if (isValid()) {
        int len = size;
		if (_firstPacket) {
			if (_rioCore > -1)
			{
				LOG_INFO("Pinning thread to core %d", _rioCore);
				if (!SetThreadAffinityMask(GetCurrentThread(), 0x1ULL << _rioCore))
				{
					LOG_ERROR("Failed to pin RIO polling thread with error %d", WSAGetLastError());
				}
			}
		}
        result = readSocket(buffer, &len);
        if (_firstPacket) {
            _samplesize = result;
            LOG_INFO("Detect sample size=%d", _samplesize);
            _firstPacket = false;
        }
    }

    return result;
}

void CRIODataSource::close()
{
    LOG_INFO("-->");
    if (isValid())
    {
        closeSocket();
		
		_rio.RIOCloseCompletionQueue(_completionQueue);
		_rio.RIODeregisterBuffer(_recvBufferId);
		_rio.RIODeregisterBuffer(_addrBufferId);
		VirtualFreeEx(GetCurrentProcess, _addrBufferPtr, 0, MEM_RELEASE);
		VirtualFreeEx(GetCurrentProcess, _recvBufferPtr, 0, MEM_RELEASE);
    }
    LOG_INFO("<--");
}
#endif //_WIN32

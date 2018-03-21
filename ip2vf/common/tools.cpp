#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
#include <Ws2tcpip.h>       // used for inet_pton
#include <windows.h>
#include <winbase.h>
#else
#include <string.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <unistd.h>         //STDIN_FILENO
#include <sys/shm.h>
#include <arpa/inet.h>
#endif
#include <time.h>           // CLOCK_REALTIME, clock_gettime
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <stdexcept>
#include <algorithm>

#include "common.h"
#include "tools.h"
#include "log.h"

#include <iostream>
#include <cctype>
#include <clocale>
#include <sstream>
#include <iomanip>

using namespace std;

#ifndef IS_GIT_REPO
const char *gitlast10commits = "NOT SUPPORTED";
const char *gitcurrentbranch = "NOT SUPPORTED";
#else
extern const char *gitlast10commits;
extern const char *gitcurrentbranch;
#endif

#define BILLION  1000000000LL
#define MILLION  1000000LL
unsigned int startTimeInS = 0;
double tools::getCurrentTimeInS()
{
    double ret = 0.0;
#ifdef _WIN32
    DWORD current = GetTickCount();
    if (startTimeInS == 0)
        startTimeInS = current / 1000;
    ret = ((double)current - (double)startTimeInS) / 1000.0;
#else
    struct timespec current;

    clock_gettime( CLOCK_REALTIME /*clock_id*/, &current );
    if( startTimeInS==0 )
    {
        startTimeInS = current.tv_sec;
    }
    ret = (double)(current.tv_sec-startTimeInS) + (double)current.tv_nsec/BILLION;
#endif
    return ret;
}


long long tools::getCurrentTimeInMicroS()
{
    long long ret = 0;
#ifdef _WIN32
    DWORD current = GetTickCount();
    if (startTimeInS == 0)
        startTimeInS = current / 1000;
    ret = (long long)(current - startTimeInS) * 1000;
#else
    struct timespec current;

    clock_gettime(CLOCK_REALTIME /*clock_id*/, &current);
    if (startTimeInS == 0)
    {
        startTimeInS = current.tv_sec;
    }
    ret = (current.tv_sec - startTimeInS) * MILLION + current.tv_nsec / 1000;
#endif
    return ret;
}


long long tools::getCurrentTimeInMilliS()
{
    long long ret = 0;
#ifdef _WIN32
    DWORD current = GetTickCount();
    if (startTimeInS == 0)
        startTimeInS = current / 1000;
    ret = current - startTimeInS;
#else
    struct timespec current;

    clock_gettime(CLOCK_REALTIME /*clock_id*/, &current);
    if (startTimeInS == 0)
    {
        startTimeInS = current.tv_sec;
    }
    ret = (current.tv_sec - startTimeInS)*1000LL + (double)current.tv_nsec / MILLION;
#endif
    return ret;
}

unsigned long tools::getUTCEpochTimeInMs()
{
    unsigned long milliseconds_since_epoch = (unsigned long)(
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1));
    return milliseconds_since_epoch;
}


void tools::split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
}
vector<string> tools::split(const string &s, char delim) {
    vector<string> elems;
    tools::split(s, delim, elems);
    return elems;
}

bool tools::isDigits(const std::string &str)
{
    return std::all_of(str.begin(), str.end(), ::isdigit); // C++11
}

bool tools::isOdd(int val)
{
    return (val % 2) != 0;
}

bool tools::isEven(int val)
{
    return !(val % 2);
}
bool noCaseCompare_pred(unsigned char a, unsigned char b)
{
    return std::tolower(a) == std::tolower(b);
}

bool tools::noCaseCompare(std::string const& a, std::string const& b)
{
    if (a.length() == b.length()) {
        return std::equal(b.begin(), b.end(),
            a.begin(), noCaseCompare_pred);
    }
    else {
        return false;
    }
}
bool tools::endsWith(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}
std::string tools::to_string_with_precision(const double a_value, const int n)
{
    std::ostringstream out;
    if(a_value>10.0)
        out << (int)round(a_value) ;
    else
        out << std::setprecision(n) << a_value;
    return out.str();
}

//
// code based on:
// http://stackoverflow.com/questions/1420426/calculating-cpu-usage-of-a-process-in-linux
//
int       g_pid = -1;
long long g_last_cpuload = -1;
int       g_last_utime = -1;
int       g_last_stime = -1;
int       g_cpu_count = -1;
void tools::getCPULoad( int& user, int& kernel ) {
    std::string line;
    std::ifstream _f;
    long long cpuload = 0;
    int utime = 0;
    int stime = 0;

    user = 0;
    kernel = 0;

#ifdef _WIN32
#else
    if (g_pid == -1) {
        g_pid = (int)GETPID;
        LOG_INFO("set pid to '%d'", g_pid);
    }
    if (g_cpu_count == -1) {
        g_cpu_count = get_nprocs();
        LOG_INFO("set g_cpu_count='%d'", g_cpu_count);
    }

    // First, get the full CPU load
    _f.open("/proc/stat", ios::in);
    if (_f.is_open()) {
        std::getline(_f, line);
        _f.close();
    }
    try {
        //LOG_INFO("tools_getCPULoad(): cpu='%s'", line.c_str());
        vector<string> tokens = tools::split(line, ' ');
        for (int i = 1; i < (int)tokens.size(); i++) {
            if (tokens[i].empty())
                continue;
            cpuload += std::stoll(tokens[i]);
        }
    }
    catch (...) {
        LOG_ERROR("catch exception  when std::stoi for cpuload");
        return;
    }
    //LOG_INFO("tools_getCPULoad(): cpuload= '%d'", cpuload);

    // then get load for the current process 
    char buffer[128];
    SNPRINTF(buffer, 128, "/proc/%d/stat", g_pid);
    _f.open(buffer, ios::in);
    if (_f.is_open()) {
        std::getline(_f, line);
        _f.close();
    }
    try {
        vector<string> tokens = tools::split(line, ' ');
        if (tokens.size() > 14) {
            utime = std::stoi(tokens[13]);
            stime = std::stoi(tokens[14]);
        }
        else {
            LOG_ERROR("string error when get infos from /proc/<pid>/stat");
        }
        //LOG_INFO("tools_getCPULoad(): tokens[%d]='%s'", 13, tokens[13].c_str());
        //LOG_INFO("tools_getCPULoad(): tokens[%d]='%s'", 14, tokens[14].c_str());
    }
    catch (...) {
        LOG_ERROR("catch exception  when try to get load for current process");
        return;
    }

    if (g_last_cpuload != -1 &&  g_last_cpuload != cpuload) {
        user = g_cpu_count * 100 * (utime - g_last_utime) / ((long long)cpuload - g_last_cpuload);
        kernel = g_cpu_count * 100 * (stime - g_last_stime) / ((long long)cpuload - g_last_cpuload);
        LOG("cpuload (%d,%d)", user, kernel);
    }
    g_last_cpuload = cpuload;
    g_last_utime = utime;
    g_last_stime = stime;
#endif
}

void tools::getMEMORYLoad(int &mem) {
    std::string line;
    std::ifstream _f;
    std::string toSearch("VmRSS:");

    mem = 0;
    
    _f.open("/proc/self/status", ios::in);
    if (_f.is_open()) {
        while (!_f.eof()) {
            std::getline(_f, line);
            if ((line.compare(0, toSearch.length(), toSearch) == 0)) {
                if (line.find(':') != string::npos) {
                    try {
                        mem = std::stoi(&line[line.find(':') + 1]);
                    }
                    catch (const std::out_of_range &) {
                        LOG_ERROR("catch 'out_of_range'  when std::stoi for cpuload");
                    }
                    catch (const std::invalid_argument &) {
                        LOG_ERROR("catch 'invalid_argument'  when std::stoi for cpuload");
                    }
                    catch (...) {
                        LOG_ERROR("catch exception  when std::stoi for cpuload");
                    }
                    LOG("mem='%d'", mem);
                }
                break;
            }
        }
        _f.close();
    }

}


void tools::createOneRGBFrameFile(int w, int h, const char* filename) {
    LOG("--> <--");
    std::ofstream f;
    int size = w*h*3;
    unsigned char* buffer = new unsigned char[size];
    f.open(filename, std::ios::out | std::ios::binary);
    if( !f.is_open() )
    {
        LOG_ERROR("***ERROR*** can't open '%s'", filename);
        return;
    }
    int p=0;
    for(int y=0; y<h; y++)
        for( int x=0; x<w; x++ ) {
            buffer[p++] = 255 * ((x/100)%3==0) ;    //r
            buffer[p++] = 255 * ((x/100)%3==1) ;    //g
            buffer[p++] = 255 * ((x/100)%3==2) ;    //b
        }
    f.write((const char*)buffer, size);
    f.close();
    delete[] buffer;
}

void tools::createOneRGBAFrameFile(int w, int h, const char* filename) {
    LOG("--> <--");
    std::ofstream f;
    int size = w*h*4;
    unsigned char* buffer = new unsigned char[size];
    f.open(filename, std::ios::out | std::ios::binary);
    if( !f.is_open() )
    {
        LOG_ERROR("***ERROR*** can't open '%s'", filename);
        return;
    }
    int p=0;
    for(int y=0; y<h; y++)
        for( int x=0; x<w; x++ ) {
            buffer[p++] = 255 * ((x/100)%3==0) ;    //r
            buffer[p++] = 255 * ((x/100)%3==1) ;    //g
            buffer[p++] = 255 * ((x/100)%3==2) ;    //b
            buffer[p++] = 0 ;                       //a
        }
    f.write((const char*)buffer, size);
    f.close();
    delete[] buffer;
}

void tools::dumpBuffer(unsigned char * buff, int len, const char * filename, ios_base::openmode mode) {
    ofstream fout;
    fout.open(filename, ios::binary | ios::out | mode);
    fout.write((const char*)buff, len);
    fout.close();
}

unsigned char * tools::loadBuffer(const char * filename, int &len) {
    len = 0;
    ifstream fin;
    fin.open(filename, ios::binary | ios::in);
    fin.seekg(0, ios::end);
    len = (int)fin.tellg();
    unsigned char * buff = (unsigned char *)malloc(len);
    fin.seekg(0, ios::beg);
    fin.read((char *)buff, len);
    fin.close();
    return buff;
}


#ifdef _WIN32
#include <windows.h>
void usleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#endif


void DUMP_RGBPIXEL_AT(unsigned char* rgb, int w, int h, int x, int y) {
    unsigned char r, g, b;
    unsigned char* p = rgb + y*(w * 3) + x * 3;
    r = *p++;
    g = *p++;
    b = *p++;
    LOG_INFO("(%d,%d): r,g,b=(%d,%d,%d)", x, y, r, g, b);
}
void DUMP_YUV8PIXEL_AT(unsigned char* yuv, int w, int h, int x, int y) {
    unsigned char u, v, y1, y2;
    //unsigned char* p = yuv + y*(w * 2) + x * 2;
    u = *yuv++;
    y1 = *yuv++;
    v = *yuv++;
    y2 = *yuv++;
    LOG_INFO("(%d,%d): u,y1,v,y2=(%d,%d,%d,%d)", x, y, u, y1, v, y2);
}

VMILIBRARY_API_TOOLS void tools::displayVersion()
{
    std::cout << "Version:      " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << "\n";
    std::cout << "Tag:          " << VERSION_TAG << "\n";
    std::cout << "Branch:       " << gitcurrentbranch << "\n";
    std::cout << "Compile time: " << __DATE__ << " " << __TIME__ << "\n";
    std::string commits = gitlast10commits;
    vector<string> tokens = tools::split(commits, '\t');
    std::string formatstring = "\r\n ";
    for (int i = 0; i < (int)tokens.size(); i++) {
        formatstring += tokens[i];
        formatstring += "\n";
    }
    //std::replace( commits.begin(), commits.end(), "\n\r", 
    std::cout << "Last 10 commits: " << formatstring;
}

int tools::getPPCM(int a, int b)
{
    if (a < 0 || b < 0)
        return 0;

    int ta = a;
    int tb = b;
    while (a != b) {
        if (a > b)
            b += tb;
        else if (a < b)
            a += ta;
    }

    return a;
}

int tools::randint(int min, int max) {
    return min + (rand() % static_cast<int>(max - min + 1));
}

int tools::getIPAddressFromString(const char* str) {
    int ip = -1;
    // store this IP address in sa:
    int result = inet_pton(AF_INET, str, &ip);
    if (result <= 0) {
        if (result == 0)
            LOG_ERROR("ERROR: Not in presentation format");
        else
            LOG_ERROR("ERROR: inet_pton");
    }
    return ip;
}

VMILIBRARY_API_TOOLS int tools::convert10bitsto8bits(unsigned char* in, int in_size, unsigned char* out) {
    try {
        int w[4];
        while ((in_size - 5) >= 0)
        {
            w[0] = ((in[0]) << 2) + ((in[1] & 0b11000000) >> 6);
            w[1] = ((in[1] & 0b00111111) << 4) + ((in[2] & 0b11110000) >> 4);
            w[2] = ((in[2] & 0b00001111) << 6) + ((in[3] & 0b11111100) >> 2);
            w[3] = ((in[3] & 0b00000011) << 8) + ((in[4]));
            for (int k = 0; k < 4; k++)
            {
                out[k] = (unsigned char)(w[k] >> 2);
            }
            in += 5; in_size -= 5;
            out += 4;
        }
    }
    catch (...) {
        LOG_ERROR("catch exception ...");
    }
    return 0;
}

int tools::convert8bitsto10bits(unsigned char* in, int in_size, unsigned char* out) {
    try {
        for (int i = 0; i < in_size; i++)
            tools::set10bitsWord(out, i, MAX(16, MIN(in[i], !!(i % 2) ? 235 : 240)) << 2);
    }
    catch (...) {
        LOG_ERROR("catch exception ...");
    }
    return 0;
}

char* tools::createSHMSegment(int size, int shmkey, 
#ifndef _WIN32

    int& shmid) {

    LOG_INFO("connect to shmem key=%d, size=%d", shmkey, size);
    // Create the shared memory segment

    shmid = shmget(shmkey, size, IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1) {
        int error = errno;
        if (error == EEXIST) {
            LOG_INFO("(shm key=%d): The shmem already exists, connect to the existing one", shmkey);
            // Failed because of O_CREAT | O_EXCL, the segment already exists. Retry without flags.
            shmid = shmget(shmkey, size, 0666);
            if (shmid == -1) {
                error = errno;
                if (error == EINVAL) {
                    // There is a size issue
                    LOG_ERROR("(shm key=%d): Failed to open shmem. It seems there is a size issue", shmkey);
                    LOG_INFO("(shm key=%d): Try to delete it before re-create at correct size", shmkey);
                    if (shmctl(shmid, IPC_RMID, 0) == -1) {
                        LOG_ERROR("***ERROR*** shmctl failed!!!");
                    }

                }
            }
        }
        else {
            LOG_ERROR("***ERROR*** shmget failed, errno=%s", strerror(error));
            return NULL;
        }
    }

    // Attach the current process to the memory segment
    char* pData = (char*)shmat(shmid, NULL, 0);
    if (pData == (char *)(-1)) {
        LOG_ERROR("***ERROR*** shmat failed, errno=%s", strerror(errno));
        return NULL;
    }

    // Be sure that the shared memory segment is mark to be destroyed
    //shmctl(shmid, IPC_RMID , 0);

    return pData;

#else

    HANDLE& shmid) {

    char shm_name[24];
    SNPRINTF(shm_name, sizeof(shm_name), "IP2VF_SHM_%i", shmkey);

    shmid = OpenFileMapping(FILE_MAP_WRITE, false, shm_name);
    if (shmid == NULL) {
        shmid = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, shm_name);
        if (shmid == NULL) {
            LOG_ERROR("***ERROR*** shm failed, last error=%d", GetLastError());
            return NULL;
        }
    }
    else {
        LOG_INFO("the shmem already exists, connect to the existing one");
    }

    // Attach the current process to the memory segment
   char* pData = static_cast<char*>(MapViewOfFile(shmid, FILE_MAP_WRITE, 0, 0, 0/*size*/));
    if (pData == NULL) {
        LOG_ERROR("***ERROR*** shm failed, last error=%d", GetLastError());
        return NULL;
    }

    return pData;

#endif
}


void tools::detachSHMSegment(char* pData) {
#ifndef _WIN32

    if (shmdt(pData) == -1) {
        LOG_ERROR("***ERROR*** shmdt failed!!!");
    }

#else

    if (pData) {
        UnmapViewOfFile(pData);
    }

#endif
}

void tools::deleteSHMSegment(char* pData,
#ifndef _WIN32
    int shmid) {

    if (shmdt(pData) == -1) {
        LOG_ERROR("***ERROR*** shmdt failed!!!");
    }

    // Be sure that the shared memory segment is mark to be destroyed
    if (shmctl(shmid, IPC_RMID , 0) == -1) {
        LOG("shmctl with IPC_RMID failed. Certainly OS already deleted it just after shmdt().");
    }

#else
    HANDLE shmid) {

    if (pData) {
        UnmapViewOfFile(pData);
    }
    if (shmid != INVALID_HANDLE_VALUE) {
        CloseHandle(shmid);
    }
#endif
}

int tools::getSHMSegmentSize(
#ifndef _WIN32
    int shmid) {
    int size = -1;
    struct shmid_ds shmid_struct;
    if (shmctl(shmid, IPC_STAT, &shmid_struct) == 0) {
        size = shmid_struct.shm_segsz;
    }
    else {
        LOG("shmctl with IPC_STAT failed.");
    }
#else
HANDLE shmid) {
    int size = -1;
#endif
    return size;
}

int tools::getSHMSegmentAttachNb(
#ifndef _WIN32
    int shmid) {
    int nb = -1;
    struct shmid_ds shmid_struct;
    if (shmctl(shmid, IPC_STAT, &shmid_struct) == 0) {
        nb = shmid_struct.shm_nattch;
    }
    else {
        LOG("shmctl with IPC_STAT failed.");
    }
#else
HANDLE shmid) {
    int nb = -1;
#endif
    return nb;
}

#define NB_SHMEM_SEGMENT_MAX    1000

char* tools::createSHMSegment_ext(int size, int &shmkey,
#ifndef _WIN32

    int& shmid, bool bForceDeleteIfUnused) {

    LOG_INFO("Search the first free shmem segment from key=%d, size=%d", shmkey, size);

    for (int i = shmkey; i < shmkey + NB_SHMEM_SEGMENT_MAX; i++) {

        shmid = shmget(i, size, IPC_CREAT | IPC_EXCL | 0666);
        if (shmid == -1) {
            int error = errno;
            if (error == EEXIST) {
                LOG_INFO("(shm key=%d): The shmem already exists, try next one", i);
                if (bForceDeleteIfUnused) {
                    // Test attach number... mark to delete if 0
                    shmid = shmget(i, 0, 0666);
                    if (shmid != -1) {
                        int nb = getSHMSegmentAttachNb(shmid);
                        LOG_INFO("(shm key=%d): but before, mark it as destroy", i);
                        if (nb == 0) {
                            if (shmctl(shmid, IPC_RMID, 0) == -1) {
                                LOG_ERROR("***ERROR*** shmctl failed!!!");
                            }
                        }
                    }
                }
                continue;
            }
            else {
                LOG_ERROR("***ERROR*** shmget failed, errno=%s", strerror(error));
                return NULL;
            }
        }
        else {
            // Attach the current process to the memory segment
            char* pData = (char*)shmat(shmid, NULL, 0);
            if (pData == (char *)(-1)) {
                LOG_ERROR("***ERROR*** shmat failed, errno=%s", strerror(errno));
                return NULL;
            }
            shmkey = i;
            LOG_INFO("ok to create new segment with shm key=%d", shmkey);
            return pData;
        }
    }

    // Not found... major error here.
    LOG_ERROR("***ERROR*** can't found free shmem seg between %d and %d", shmkey, shmkey + NB_SHMEM_SEGMENT_MAX);
    return NULL;

#else

HANDLE& shmid, bool bForceDeleteIfUnused) {

    char shm_name[24];

    LOG_INFO("Search the first free shmem segment from key=%d, size=%d", shmkey, size);

    for (int i = shmkey; i < shmkey + NB_SHMEM_SEGMENT_MAX; i++) {
        
        SNPRINTF(shm_name, sizeof(shm_name), "IP2VF_SHM_%i", i);
        shmid = OpenFileMapping(FILE_MAP_WRITE, false, shm_name);
        if (shmid == NULL) {
            shmid = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, shm_name);
            if (shmid == NULL) {
                LOG_ERROR("***ERROR*** shm failed, last error=%d", GetLastError());
                return NULL;
            }
            // Attach the current process to the memory segment
            char* pData = static_cast<char*>(MapViewOfFile(shmid, FILE_MAP_WRITE, 0, 0, size));
            if (pData == NULL) {
                LOG_ERROR("***ERROR*** shm failed, last error=%d", GetLastError());
                return NULL;
            }
            shmkey = i;
            return pData;
        }
        else {
            LOG_INFO("(shm key=%d): The shmem already exists, try next one", i);
        }
    }

    // Not found... major error here.
    LOG_ERROR("***ERROR*** can't found free shmem seg between %d and %d", shmkey, shmkey + NB_SHMEM_SEGMENT_MAX);
    return NULL;

#endif
}


char* tools::getSHMSegment(int size, int shmkey,
#ifndef _WIN32

    int& shmid) {

    LOG_INFO("connect to shmem key=%d, size=%d", shmkey, size);

    shmid = shmget(shmkey, size, 0666);
    if (shmid == -1) {
        int error = errno;
        if (error == ENOENT) {
            // The segment doesn't exists
            LOG_ERROR("The shared memory segment with shm key=%d doesn't exists.", shmkey);
        }
        else if (error == EINVAL) {
            // There is a size issue
            LOG_ERROR("(shm key=%d): Failed to open shmem. It seems there is a size issue", shmkey);
        }
        else {
            LOG_ERROR("***ERROR*** shmget failed, errno=%s", strerror(error));
        }
        return NULL;
    }

    // Attach the current process to the memory segment
    char* pData = (char*)shmat(shmid, NULL, 0);
    if (pData == (char *)(-1)) {
        LOG_ERROR("***ERROR*** shmat failed, errno=%s", strerror(errno));
        return NULL;
    }

    return pData;

#else

HANDLE& shmid) {

    char shm_name[24];
    SNPRINTF(shm_name, sizeof(shm_name), "IP2VF_SHM_%i", shmkey);

    shmid = OpenFileMapping(FILE_MAP_WRITE, false, shm_name);
    if (shmid == NULL) {
        LOG_ERROR("***ERROR*** OpenFileMapping failed, last error=%d", GetLastError());
        return NULL;
    }
    else {
        LOG_INFO("the shmem already exists, connect to the existing one");
    }

    // Attach the current process to the memory segment
    char* pData = static_cast<char*>(MapViewOfFile(shmid, FILE_MAP_WRITE, 0, 0, 0 /*size*/));
    if (pData == NULL) {
        LOG_ERROR("***ERROR*** MapViewOfFile failed, last error=%d", GetLastError());
        return NULL;
    }

    return pData;

#endif
}

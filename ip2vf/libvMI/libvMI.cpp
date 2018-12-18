#include <cstdio>
#include <cstdlib>
#include <cstring>      // strcmp
#include <set>
#include <pins/pins.h>

#include "log.h"
#include "common.h"
#include "moduleconfiguration.h"
#include "libvMI.h"
#include "libvMI_int.h"
#include "tools.h"
#include "framecounter.h"
#include "vmiframe.h"

#include "vMI_input.h"
#include "vMI_output.h"
#include "vMI_module.h"

/**
* Frame management,
* Note: frame are not linked to a module
*/

#define MAX_FRAME_IN_LIST   10

/**
* \brief Struct that contains all about a frame
*
* This struct contains a unique handle, the pointer to the corresponding object, and a bool to indicate if
* the frame is free or not.
* INTERNAL USE ONLY (do not expose this across the API)
*
*/
using tFrameItem = std::tuple<  libvMI_frame_handle,    /* Handle associated with this item. Available only if in use (free item flag = LIBVMI_INVALID_HANDLE) */
                                CvMIFrame*,             /* Pointer to the vMIFrame associated to this item */
                                bool                    /* free item flag: true if free, false if in used */
                             >;

std::vector< tFrameItem > g_vMIFramesArray;             /* vector of all frames processed by this library instance */
std::mutex g_vMIFramesMutex;                            /* mutex used for elementary operation on g_vMIFramesArray*/
int g_vMIFramesNextHandle = 0;                          /* counter for next handle value */
int g_vMIMaxFramesInList = MAX_FRAME_IN_LIST;           /* maximum number of frames in g_vMIFramesArray. Can be set/get with parameter */

/**
* \brief Search and return a handle to an available vMIFrame. A new frame is created if no available frames.
* This increase the ref counter for the vMIFrame.
*
* \return a handle which can be used to reference the frame later. LIBVMI_INVALID_HANDLE if error, or can't create frame.
*/
libvMI_frame_handle libvmi_frame_create() {

    std::unique_lock<std::mutex> lock(g_vMIFramesMutex);

    // First, search for a free item on g_vMIFramesArray
    for (std::vector< tFrameItem >::iterator it = g_vMIFramesArray.begin(); it != g_vMIFramesArray.end(); ++it) {
        if (std::get<2>(*it)) {
            // We find a free item on the list
            std::get<0>(*it) = g_vMIFramesNextHandle++;
            std::get<1>(*it)->addRef();
            std::get<2>(*it) = false;
            LOG("re-use item with new handle [%d], frame array size=%d", std::get<0>(*it), g_vMIFramesArray.size());
            return std::get<0>(*it);
        }
    }

    if (g_vMIFramesArray.size() >= g_vMIMaxFramesInList) {
        LOG_ERROR("Error, too much frame in list. Current size is '%d'", g_vMIFramesArray.size());
        //for (std::vector< tFrameItem >::iterator it = g_vMIFramesArray.begin(); it != g_vMIFramesArray.end(); ++it) {
        //    LOG_INFO("Frame#%d, ref=%d", std::get<0>(*it), std::get<1>(*it)->getRef());
        //}
        //exit(0);
        return LIBVMI_INVALID_HANDLE;
    }

    // At this point, we have no more free item... create a new one.
    CvMIFrame* newFrame = new CvMIFrame();
    auto item = std::make_tuple(g_vMIFramesNextHandle++, newFrame, false);
    g_vMIFramesArray.push_back(item);
    LOG_INFO("create new item with handle [%d], now frame array size =%d", std::get<0>(item), g_vMIFramesArray.size());
    return std::get<0>(item);
}

/**
* \brief Return the number of free frame in g_vMIFramesArray list
*
* \return an integer
*/
int libvMI_get_free_frame_number() {

    int count = 0;
    std::unique_lock<std::mutex> lock(g_vMIFramesMutex);
    for (std::vector< tFrameItem >::iterator it = g_vMIFramesArray.begin(); it != g_vMIFramesArray.end(); ++it) {
        if (std::get<2>(*it))
            count++;
    }
    return count;
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief return the pixel size in bit for a vMI frame
*
* \return size in bit. -1 if not supported format
*/
int  _calculate_pixel_size_in_bits(CFrameHeaders &fh) {

    SAMPLINGFMT fmt = fh.GetSamplingFmt();
    int ret = -1;
    switch (fmt) {
    case SAMPLINGFMT::BGRA:
    case SAMPLINGFMT::RGBA:
        ret = 4 * fh.GetDepth();
        break;
    case SAMPLINGFMT::BGR:
    case SAMPLINGFMT::RGB:
        ret = 3 * fh.GetDepth();
        break;
    case SAMPLINGFMT::YCbCr_4_2_2:
        ret = 2 * fh.GetDepth();
        break;
    default:
        // Not supported
        ret = -1;
        break;
    }
    return ret;
}

/**
* \brief Search and return a handle to an available vMIFrame. A new frame is created if no available frames.
* This increase the ref counter for the vMIFrame.
* Allow to provide a struct that contains some parameters to init the frame
*
* \return a handle which can be used to reference the frame later. LIBVMI_INVALID_HANDLE if error, or can't create frame.
*/
libvMI_frame_handle libvmi_frame_create_ext(struct vMIFrameInitStruct &init) {
    
    // First, verify parameters validity
    CFrameHeaders fh;
    fh.SetMediaFormat(init._media_format);
    if( init._media_size > 0 )
        fh.SetMediaSize(init._media_size);
    if (init._media_format == MEDIAFORMAT::VIDEO) {
        if( init._video_width  > 0) fh.SetW(init._video_width);
        if (init._video_height > 0) fh.SetH(init._video_height);
        if (init._video_depth  > 0) fh.SetDepth(init._video_depth);
        if (init._video_smpfmt > 0) fh.SetSamplingFmt(init._video_smpfmt);
        if (init._media_size <= 0) {
            init._media_size = init._video_width * init._video_height * _calculate_pixel_size_in_bits(fh) / 8;
            fh.SetMediaSize(init._media_size);
        }
        else if (init._media_size > 0 && init._video_width > 0 && init._video_height > 0 && init._video_depth > 0 && init._video_smpfmt > 0) {
            int calc_media_size = init._video_width * init._video_height * _calculate_pixel_size_in_bits(fh) / 8;
            if (init._media_size != calc_media_size) {
                LOG_ERROR("Invalid parameter, calculated media size not equal provided media size");
                return LIBVMI_INVALID_HANDLE;
            }
        }
    }
    else if (init._media_format == MEDIAFORMAT::AUDIO) {
        if (init._media_size <= 0) {
            LOG_ERROR("Invalid parameter, you must provide a media size for an Audio frame");
            return LIBVMI_INVALID_HANDLE;
        }
    }

    // Then get a frame
    libvMI_frame_handle hFrame = libvmi_frame_create();
    if (hFrame != LIBVMI_INVALID_HANDLE) {
        CvMIFrame* frame = libvMI_frame_get( hFrame );
        frame->createFrameFromHeaders(&fh);
    }
    return hFrame;
}


/**
* \brief Search and dec the ref counter for the vMIFrame identified by its handle. If the ref counter go down to 0, the 
* vMIFrame is released.
*
* \param hFrame handle to the vMIFrame
* \return the current ref counter for the vMIFrame, -1 if not found
*/
int libvmi_frame_release(const libvMI_frame_handle hFrame) {

    std::unique_lock<std::mutex> lock(g_vMIFramesMutex);

    LOG("release frame handle [%d], current array size=%d", hFrame, g_vMIFramesArray.size());
    //LOG_COLOR(LOG_COLOR_GREEN, "release frame handle [%d], current array size=%d", hFrame, g_vMIFramesArray.size());
    for (std::vector< tFrameItem >::iterator it = g_vMIFramesArray.begin(); it != g_vMIFramesArray.end(); ++it) {
        if (std::get<0>(*it) == hFrame) {
            int ret = std::get<1>(*it)->releaseRef();
            if (ret < 0) {
                // TODO, manage this properly
                LOG_ERROR("Error, refcount=%d for frame [%d]. This not be happen.", hFrame);
            }
            if (ret == 0) {
                std::get<2>(*it) = true;
                std::get<0>(*it) = LIBVMI_INVALID_HANDLE;   // Optional, just for clarity...
            }
            LOG("refcounter for frame handle [%d] is %d", hFrame, ret);
            //LOG_COLOR(LOG_COLOR_GREEN, "refcounter for frame handle [%d] is %d", hFrame, ret);
            return ret;
        }
    }
    // Not found
    return -1;
}

/**
* \brief Search and inc the ref counter for the vMIFrame identified by its handle.
*
* \param hFrame handle to the vMIFrame
* \return the current ref counter for the vMIFrame, -1 if not found
*/
int libvmi_frame_addref(const libvMI_frame_handle hFrame) {

    std::unique_lock<std::mutex> lock(g_vMIFramesMutex);

    for (std::vector< tFrameItem >::iterator it = g_vMIFramesArray.begin(); it != g_vMIFramesArray.end(); ++it) {
        if (std::get<0>(*it) == hFrame) {
            int ret = std::get<1>(*it)->addRef();
            LOG("refcounter for frame handle [%d] is %d", hFrame, ret);
            //LOG_COLOR(LOG_COLOR_GREEN, "refcounter for frame handle [%d] is %d", hFrame, ret);
            return ret;
        }
    }
    // Not found
    return -1;
}

/**
* Not exposed from the API.
*
* \brief Search and return the pointer to the vMIFrame object identified by its handle
*
* \param hFrame handle to the vMIFrame
* \return the pointer, NULL if not found
*/
CvMIFrame* libvMI_frame_get(const libvMI_frame_handle hFrame) {

    std::unique_lock<std::mutex> lock(g_vMIFramesMutex);

    for (std::vector< tFrameItem >::iterator it = g_vMIFramesArray.begin(); it != g_vMIFramesArray.end(); ++it) {
        if (std::get<0>(*it) == hFrame)
            return std::get<1>(*it);
    }
    // not found
    return NULL;    
}

/**
* \brief Return the mediasize in byte of a vMIFrame identified by its handle
*
* \param hFrame handle to the vMIFrame
* \return the size in byte, -1 if not found
*/
int libvMI_frame_getsize(const libvMI_frame_handle hFrame) {

    CvMIFrame* frame = libvMI_frame_get(hFrame);
    if (frame != NULL) {
        return frame->getMediaSize();
    }
    // Error, the frame can't be found
    return -1;
}

/**
* \brief Return the pointer to the buffer that contain media content associated to the vMIFrame identified by its handle
*
* \param hFrame handle to the vMIFrame
* \return pointer to the media buffer, NULL if not found
*/
char* libvMI_get_frame_buffer(const libvMI_frame_handle hFrame) {

    CvMIFrame* frame = libvMI_frame_get(hFrame);
    if (frame != NULL) {
        return (char*)frame->getMediaBuffer();
    }
    // Error, the frame can't be found
    return NULL;
}

/**
* \brief Return a parameter from the vMI headers associated to the vMIFrame identified by its handle
*
* \param hFrame handle to the vMIFrame
* \param header kind of header to get value. Must be one of MediaHeader enum value
* \param value pointer to an int, used by libvMI to store the value.
* \return
*/
void libvMI_get_frame_headers(const libvMI_frame_handle hFrame, MediaHeader header, void* value) {

    CvMIFrame* frame = libvMI_frame_get(hFrame);
    if (frame != NULL) {
        frame->get_header(header, value);
    }
}

/**
* \brief Set a parameter on the vMI headers associated to the vMIFrame identified by its handle
*
* \param hFrame handle to the vMIFrame, provided by libvMI
* \param header kind of header to get value. Must be one of MediaHeader enum value
* \param value pointer to an int which contains the value to set for the header
* \return
*/
void libvMI_set_frame_headers(const libvMI_frame_handle hFrame, MediaHeader header, void* value) {

    CvMIFrame* frame = libvMI_frame_get(hFrame);
    if (frame != NULL) {
        frame->set_header(header, value);
    }
}

/**
* \brief Return a parameter from current libvMI instance
*
* \param param kind of parameter to get value. Must be one of VMIPARAMETER enum value
* \param value pointer, used by libvMI to store the value.
* \return
*/
void libvMI_get_parameter(VMIPARAMETER param, void* value) {

    try {
        switch (param) {
        case MAX_FRAMES_IN_LIST:
            *static_cast<int*>(value) = g_vMIMaxFramesInList; break;
        case CUR_FRAMES_IN_LIST:
            *static_cast<int*>(value) = (int)g_vMIFramesArray.size(); break;
        case FREE_FRAMES_IN_LIST:
            *static_cast<int*>(value) = libvMI_get_free_frame_number(); break;
        default:
            break;
        }
    }
    catch (...) {

    }
}

/**
* \brief Set a parameter on current libvMI instance
*
* \param param kind of parameter to set value. Must be one of VMIPARAMETER enum value
* \param value pointer to an int which contains the value to set 
* \return
*/
void libvMI_set_parameter(VMIPARAMETER param, void* value) {

    try {
        switch (param) {
        case MAX_FRAMES_IN_LIST:
            g_vMIMaxFramesInList = *static_cast<int*>(value); break;
        default:
            break;
        }
    }
    catch (...) {

    }
}

/**
* Contains a processed version of the legacy configuration, separated into input and output configuration
*/
struct ParseConfiguration {
    std::string moduleConfiguration;
    std::vector<std::string> inputConfigurations;
    std::vector<std::string> outputConfigurations;
};

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief Parses legacy configuration into something more structured
*
* Configuration method is still pretty fragile, but this is designed to bridge the gap
* TODO: need to totally redesign the configuration parser...
*/
ParseConfiguration * _parseConfiguration(const std::string config) {

    ParseConfiguration * ret = new ParseConfiguration();
    std::string *currentPin = &(ret->moduleConfiguration);

    std::vector<std::string> tokens = tools::split(config, ',');
    for (auto token : tokens) {
        if (token.empty()) {
            LOG_INFO("Empty token detected");
            continue;
        }

        std::vector<std::string> params = tools::split(token, '=');

        /*if (params.size() != 2) {
            LOG_ERROR("Invalid parameter format: '%s' is not in format '<param>=<value>'", token.c_str());
            continue;
        }*/
		if (params.size() > 2) {
			// Accept parameter ad eal="eal -l 0-8 -n 8 --rxq=4"
			for (int j = 2; j < (int)params.size(); j++)
			{
				params[1] += "=";
				params[1] += params[j];
			}
			for (int j = 2; j < (int)params.size(); j++)
				params.pop_back();
			LOG_ERROR("params[1]='%s'", params[1].c_str());
		}
		else if (params.size() != 2) {
			LOG_ERROR("Invalid parameter format: '%s' is not in format '<param>=<value>'", token.c_str());
			continue;
		}

        //this belongs to one of the pins,
        //the configuration is interleaved between parameters for input and output pins: 
        if (params[0].compare("out_type") == 0) {
            ret->outputConfigurations.push_back("");
            currentPin = &(ret->outputConfigurations.back());
        }
        if (params[0].compare("in_type") == 0) {
            ret->inputConfigurations.push_back("");
            currentPin = &(ret->inputConfigurations.back());
        }

        if (!currentPin) {
            THROW_CRITICAL_EXCEPTION("invalid configuration, parameter does not belong to a pin!");
        }
        (*currentPin) += (token + ",");
    }

    return ret;
};

//this keeps track of all of the modules handled by the current executable:
struct Ip2vfModulesEntry {
    std::string moduleConfig;
    std::string outputConfig;
    CvMIModuleController * module;
};
std::vector<Ip2vfModulesEntry *> g_Ip2VfModules;

/**
* INTERNAL USE ONLY. Not exposed from the API.
*
* \brief Search and return the pointer to the vMIFrame object identified by its handle
*
* \param hFrame handle to the vMIFrame
* \return the pointer, NULL if not found
*/
CvMIModuleController * libvMI_module_get(const libvMI_module_handle hModule) {

    if (hModule < g_Ip2VfModules.size()) {
        return g_Ip2VfModules[hModule]->module;
    }
    // not found
    return NULL;
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief Create a module
*
* \param func a callback function for module related events. (Input related events are reported elsewhere)
* \param preconfig A configuration string which describe configuration for the module and all inputs and outputs included on the module .
* \param user_data an opaque pointer (void*) that will be returned when callback will be called. It correspond to the first parameter (user_data) of the callback
* \return a handle which can be used to reference the module later
*/
libvMI_module_handle _libvMI_create_module_int(libvMI_input_callback func, const char* preconfig, const void *user_data) {
    
    libvMI_module_handle handle = (int)g_Ip2VfModules.size();
    Ip2vfModulesEntry * moduleEntry = new Ip2vfModulesEntry();
    moduleEntry->moduleConfig = preconfig;
    moduleEntry->module = new CvMIModuleController(func, user_data);
    g_Ip2VfModules.push_back(moduleEntry);
    return handle;
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief Create an input pin
*
* \param module handle of the module on which to create the input pin
* \param config A configuration string which describe configuration for the input pin
* \param func the callback function for pin related events.
* \param user_data an opaque pointer (void*) that will be returned when callback will be called.
* \return a handle which can be used to reference the pin later
*/
libvMI_pin_handle _libvMI_create_input(const libvMI_module_handle hModule, const char* config, libvMI_input_callback func, const void *user_data) {
    
    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return LIBVMI_INVALID_HANDLE;
    libvMI_pin_handle inputHandle = currentModule->getNextHandle();
    std::string configAsString(config);
    CvMIInput * newInput = new CvMIInput(configAsString, func, inputHandle, hModule, user_data);
    currentModule->registerInput(newInput);
    return inputHandle;
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief Create an output pin
*
* \param module handle of the module on which to create the output pin
* \param config A configuration string which describe configuration for the output pin
* \param user_data an opaque pointer (void*) that will be returned when callback will be called.
* \return a handle which can be used to reference the pin later
*/
libvMI_pin_handle  _libvMI_create_output(const libvMI_module_handle hModule, const char* config, const void* user_data) {

    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return LIBVMI_INVALID_HANDLE;
    libvMI_pin_handle outputHandle = currentModule->getNextHandle();
    std::string configAsString(config);
    CvMIOutput * newOutput = new CvMIOutput(configAsString, outputHandle, user_data);
    currentModule->registerOutput(newOutput);
    return outputHandle;
}

/**
* \brief Create and initialize a module
*
* Create and initialize a module with inputs and ouputs according to the configuration provided. Return the handle which allow
* to use it afterwards. A module is responsible for ingesting data from the pipe, and propagating output.
*
* \param func a callback function for module related events. (Input related events are reported elsewhere)
* \param preconfig A configuration string which describe configuration for the module and all inputs and outputs included on the module .
* \return a handle which can be used to reference the module later
*/
libvMI_module_handle libvMI_create_module(libvMI_input_callback func, const char* preconfig) {
    
    return libvMI_create_module_ext(func, preconfig, NULL);
}

/**
* \brief Create and initialize a module
*
* Create and initialize a module with inputs and ouputs according to the configuration provided. Return the handle which allow
* to use it afterwards. A module is responsible for ingesting data from the pipe, and propagating output.
* You can use "user_data" parameter to reference an opaque pointer that will be returned on all callback calls
*
* \param func a callback function for module related events. (Input related events are reported elsewhere)
* \param preconfig A configuration string which describe configuration for the module and all inputs and outputs included on the module .
* \param user_data an opaque pointer (void*) that will be returned when callback will be called. It correspond to the first parameter (user_data) of the callback
* \return a handle which can be used to reference the module later
*/
libvMI_module_handle libvMI_create_module_ext(libvMI_input_callback func, const char* preconfig, const void* user_data) {
    
    LOG("-->");

    // create a configuration object from the "preconfig" string
    ParseConfiguration * config = _parseConfiguration(preconfig);

    //create a new active module controller:
    libvMI_module_handle hModule = _libvMI_create_module_int(func, config->moduleConfiguration.c_str(), user_data);

    // Create input pins for this module, according to the configuration
    int i = 0;
    for (std::string a : config->inputConfigurations) {
        _libvMI_create_input(hModule, a.c_str(), func, user_data);
    }

    // Create output pins for this module, according to the configuration
    for (std::string a : config->outputConfigurations) {
        _libvMI_create_output(hModule, a.c_str(), user_data);
    }

    Ip2vfModulesEntry * currentEntry = g_Ip2VfModules[hModule];
    CvMIModuleController * currentModule = currentEntry->module;
    currentModule->init(currentEntry->moduleConfig, currentEntry->outputConfig);

    // Delete configuration object. No more used.
    delete config;

    LOG("<--");
    return hModule;
}

/**
*  \brief Return the count of inputs available on the module
*
* \param module the handle of the module on which we wanted to know the count of inputs
* \return count of inputs
*/
int libvMI_get_input_count(const libvMI_module_handle hModule) {

    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return VMI_E_INVALID_PARAMETER;
    return (int)currentModule->getInputs()->size();
}

/**
*  \brief Return the count of outputs available on the module
*
* \param module the handle for the module on which we wanted to know the count of outputs
* \return count of outputs
*/
int libvMI_get_output_count(const libvMI_module_handle hModule) {

    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return VMI_E_INVALID_PARAMETER;
    return (int)currentModule->getOutputs()->size();
}

/**
*  \brief Return the handle of an input type identified by its index. Use libvMI_get_input_count
* to get the input count.
*
* \param module the handle for the module which contains the input pin
* \param index the index of the pin (on [0..(pin_count-1)], pin_count is the value returned by libvMI_get_input_count
* \return handle of input pin
*/
libvMI_pin_handle libvMI_get_input_handle(const libvMI_module_handle hModule, int index) {

    libvMI_pin_handle ret = LIBVMI_INVALID_HANDLE;
    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return LIBVMI_INVALID_HANDLE;
    try {
        ret = currentModule->getInputs()->at(index)->getHandle();
    }
    catch (const std::out_of_range& err) {
        LOG_ERROR("no pin found at index %d (%s)", index, err.what());
    }
    return ret;
}

/**
*  \brief Return the handle of an output type identified by its index. Use libvMI_get_output_count
* to get the output count.
*
* \param module the handle for the module which contains the output pin
* \param index the index of the pin (on [0..(pin_count-1)], pin_count is the value returned by libvMI_get_output_count
* \return handle of ouput pin
*/
libvMI_pin_handle libvMI_get_output_handle(const libvMI_module_handle hModule, int index) {

    libvMI_pin_handle ret = LIBVMI_INVALID_HANDLE;
    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return LIBVMI_INVALID_HANDLE;
    try {
        ret = currentModule->getOutputs()->at(index)->getHandle();
    }
    catch (const std::out_of_range& err) {
        LOG_ERROR("no pin found at index %d (%s)", index, err.what());
    }
    return ret;
}

/**
*  \brief Starts ingesting data on a module.
*
* Starts ingesting data for the specified module.
* The callback will be called with CMD_START before this function returns.
* Unsafe to call from inside the callback itself.
* This should be called only after configuration is complete.
*
* \param module the handle of the module which should be started
* \return 0 when successful, -1 otherwise
*/
int libvMI_start_module(const libvMI_module_handle hModule) {

    LOG("-->");
    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return -1;
    currentModule->start();
    LOG("<--");
    return 0;
}

/**
* \brief Stop Ingesting data on a module.
*
* Stop ingesting data for the specified module.
* The callback will be called with CMD_STOP before this function returns.
* Unsafe to call from inside the callback itself.
*
* \param module the handle of the module which should be stopped
* \return 0 when successful, -1 otherwise
*/
int libvMI_stop_module(const libvMI_module_handle hModule) {

    LOG("-->");
    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return -1;
    currentModule->stop();
    LOG("<--");
    return 0;
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief return the output pin object corresponding to the handle
*
* \param module handle of the module on which search for the output pin
* \param hOutput handle of the output pin
* \return pointer to the output pin object, NULL if not found
*/
CvMIOutput* _libvMI_get_output(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput) {

    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return NULL;
    std::vector<CvMIOutput*>* outputs = currentModule->getOutputs();
    for (int i = 0; i < outputs->size(); i++) {
        try {
            if (outputs->at(i)->getHandle() == hOutput)
                return outputs->at(i);
        }
        catch (const std::out_of_range& err) {
            LOG_ERROR("no pin found at index %d (%s)", i, err.what());
        }
    }
    return NULL;    // not found
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief return the input pin object corresponding to the handle
*
* \param module handle of the module on which search for the input pin
* \param hInput handle of the input pin
* \return pointer to the input pin object, NULL if not found
*/
CvMIInput* _libvMI_get_input(const libvMI_module_handle hModule, const libvMI_pin_handle hInput) {

    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return NULL;
    std::vector<CvMIInput*>* inputs = currentModule->getInputs();
    for (int i = 0; i < inputs->size(); i++) {
        try {
            if (inputs->at(i)->getHandle() == hInput)
                return inputs->at(i);
        }
        catch (const std::out_of_range& err) {
            LOG_ERROR("no pin found at index %d (%s)", i, err.what());
        }
    }
    return NULL;    // not found
}

/**
* \brief Set a parameter on libvMI output pin
*
* \param hModule the handle for the module which contains the output pin
* \param hOutput handle of the output pin
* \param param kind of parameter to set value. Must be one of OUTPUTPARAMETER enum value
* \param value pointer to an int which contains the value to set
* \return
*/
void libvMI_set_output_parameter(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput, OUTPUTPARAMETER param, void* value) {

    try {
        CvMIOutput* output = _libvMI_get_output(hModule, hOutput);
        if (output) {
            output->setParameter(param, value);
        }
        else
            LOG_ERROR("can't found pin handle #%d", hOutput);
    }
    catch (...) {

    }
}

/**
* \brief Return the name of the module as it is define on configuration.
*
* \param hModule the handle for the module which contains the output pin
* \return the pointer on string where are stored the module name.
*/
const char* libvMI_get_module_name(const libvMI_module_handle hModule) {

    try {
        CvMIModuleController * currentModule = libvMI_module_get(hModule);
        if (currentModule) {
            return currentModule->getName();
        }
    }
    catch (...) {

    }
    return NULL;
}

/**
* \brief Return the current configuration of the input pin identified by its handle
*
* \param hModule the handle for the module which contains the input pin
* \param hInput handle of the input pin
* \return pointer on the configuration string.
*/
const char* libvMI_get_input_config_stream(const libvMI_module_handle hModule, const libvMI_pin_handle hInput) {

    try {
        CvMIInput* input = _libvMI_get_input(hModule, hInput);
        if (input) {
            return input->getConfigAsString();
        }
        else
            LOG_ERROR("can't found pin handle #%d", hInput);
    }
    catch (...) {

    }
    return NULL;
}

/**
* \brief Return the current configuration of the output pin identified by its handle
*
* \param hModule the handle for the module which contains the output pin
* \param hOutput handle of the output pin
* \return pointer on the configuration string.
*/
const char* libvMI_get_output_config_stream(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput) {

    try {
        CvMIOutput* output = _libvMI_get_output(hModule, hOutput);
        if (output) {
            return output->getConfigAsString();
        }
        else
            LOG_ERROR("can't found pin handle #%d", hOutput);
    }
    catch (...) {

    }
    return NULL;
}

/**
* \brief Return the number of modules managed by this library instance
*
* \return number of modules
*/
int libvMI_get_number_of_modules() {

    return (int)g_Ip2VfModules.size();
}

/**
* \brief Return the handle of a module identified by it's index. Use libvMI_get_number_of_modules to
* retreive the total number of modules on this library instance
*
* \param index index of module on library instance module list
* \return handle of found module.
*/
libvMI_module_handle libvMI_get_module_by_index(int index) {

    // TODO: change this. the module handle must not be the index
    if( index<g_Ip2VfModules.size())
        return (libvMI_module_handle)index;
    return LIBVMI_INVALID_HANDLE;
}


/**
* \brief Sends the frame across an output pin
*
* allow to propagate the frame to next pipeline stage. Note that it increase the reference counter of the frame until the frame is
* effectively sent by the output pin.
*
* \param hModule the handle of the module which contain the output pin.
* \param hOutput the handle of the output to use.
* \param hFrame the handle of the frame to send.
* \return 0 when successful, -1 otherwise
*/
int libvMI_send(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput, const libvMI_frame_handle hFrame) {

    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return -1;
    CvMIOutput * currentOutput = _libvMI_get_output(hModule, hOutput);
    if (currentOutput == NULL) {
        LOG_ERROR("libip2vf_send(): can't send anything... no output configurated. exit.");
        return 0;
    }

    // send the frame. Note that frame content are not immediately sent: the vMI frame is enqeue on the output, and
    // output->send(hFrame) return immediately. The frame reference counter will be increased from 1. It will be
    // decreased only when the frame will be effectively sent
    if (libvMI_frame_get(hFrame) != NULL)
        currentOutput->send(hFrame);
    else
        return -1;

    return 0;
}

/**
* \brief Frees up all resources allocated for the module.
*
* Will do the stop if module running.
* All handles created in relation to this module should not be used afterwards (module handle, and all corresponding inputs/output handles)
*
* \param module the module which will be closed
* \return 0 when successful, -1 otherwise
*/
int libvMI_close(const libvMI_module_handle hModule) {

    LOG("--> <--");
    CvMIModuleController * currentModule = libvMI_module_get(hModule);
    if (currentModule == NULL)
        return -1;
    currentModule->close();
    delete g_Ip2VfModules[hModule];
    g_Ip2VfModules.erase(g_Ip2VfModules.begin() + hModule);
	closeLog();
    return 0;
}

/**
* INTERNAL USE ONLY. Not exposed across the API.
*
* \brief return the metrics collector corresponding to the handle
*
* \param module handle of interest
* \return pointer to the metrics collector
*/
void*
libvMI_get_metrics_handle(const libvMI_module_handle hModule)
{
	return g_Ip2VfModules[hModule]->module->getMetricsCollector();
}

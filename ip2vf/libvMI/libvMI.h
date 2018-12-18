/**
 * \file libvMI.h
 * \brief vMI library function headers
 * \author A.Taldir M.Hawari
 * \version 1.0
 * \date 26 september 2017
 *
 * Describes all the interface of the LibvMI component
 *
 * 2017/09/26 - version 0.0: original import
 * 2018/01/15 - version 1.0: Add frames support
 *
 */

#ifndef _LIBVMI_H
#define _LIBVMI_H

/*
    List of available API functions:

 libvMI_frame_handle libvmi_frame_create();
 libvMI_frame_handle libvmi_frame_create_ext(struct vMIFrameInitStruct &init);
                 int libvmi_frame_release(const libvMI_frame_handle hFrame);
                 int libvmi_frame_addref(const libvMI_frame_handle hFrame);
                 int libvMI_frame_getsize(const libvMI_frame_handle hFrame);
               char* libvMI_get_frame_buffer(const libvMI_frame_handle frame);
                void libvMI_get_frame_headers(const libvMI_frame_handle frame, MediaHeader header, int* value);
                void libvMI_set_frame_headers(const libvMI_frame_handle frame, MediaHeader header, int* value);
                void libvMI_get_parameter(VMIPARAMETER param, void* value);
                void libvMI_set_parameter(VMIPARAMETER param, void* value);
                void libvMI_set_output_parameter(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput, OUTPUTPARAMETER param, void* value)
libvMI_module_handle libvMI_create_module(int zmq_listen_port, libvMI_input_callback func, const char* preconfig);
libvMI_module_handle libvMI_create_module_ext(int zmq_listen_port, libvMI_input_callback func, const char* preconfig, const void* user_data);
                 int libvMI_get_input_count(const libvMI_module_handle module);
                 int libvMI_get_output_count(const libvMI_module_handle module);
   libvMI_pin_handle libvMI_get_input_handle(const libvMI_module_handle module, int index);
   libvMI_pin_handle libvMI_get_output_handle(const libvMI_module_handle module, int index);
                 int libvMI_start_module(const libvMI_module_handle module);
                 int libvMI_stop_module(const libvMI_module_handle module);
                 int libvMI_send(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput, const libvMI_frame_handle hFrame);
                 int libvMI_close(const libvMI_module_handle module);
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \enum CmdType
 * \brief Event id
 *
 * CmdType is the event id that use libvMI to notify process that an event occured.
 */
enum CmdType {
    CMD_NONE = -1,  /*!< No event. */
    CMD_INIT  = 1,  /*!< the init occured in libvMI. Initiated by supervision or by configuration. */
    CMD_START = 2,  /*!< the start event occured on libvMI. From this point, libvMI will start to receive frames from pipeline */
    CMD_TICK  = 3,  /*!< A new frame is available on the input. We can read it from the pointer provided by libvMI_get_input_buffer(). For a SMPTE PIN the same CmdType is used for all essences */
    CMD_STOP  = 4,  /*!< the stop event occured on libvMI. From this point, libvMI will no more receive frames from pipeline */
    CMD_QUIT  = 5   /*!< the quit event occured on libvMI. Resources are freed */
};

/**
* \enum VMIPARAMETER
* \brief enumeration of parameter available for libvMI_get_parameter() and libvMI_set_parameter() functions
*
* VMI parameters suported values
*/
enum VMIPARAMETER {
    MAX_FRAMES_IN_LIST,    /*!< GET/SET Maximal number of frames on the internal frame list */
    CUR_FRAMES_IN_LIST,    /*!< GET     number of frames on the internal frame list */
    FREE_FRAMES_IN_LIST,   /*!< GET     number of frames on the internal frame list */
};

/**
* \enum OUTPUTPARAMETER
* \brief enumeration of parameter available for libvMI_set_output_parameter() function
*
* VMI parameters suported values
*/
enum OUTPUTPARAMETER {
    SYNC_ENABLED,    
    SYNC_TIMESTAMP,    
    SYNC_CLOCK,
};

/**
 * \enum MediaHeader
 * \brief kind of media header available
 *
 * MediaHeader enumerate available headers to get from input media frame, or to set for output media frame
 */
enum MediaHeader {
    MODULE_ID           = 0,  /*!< module Id */
    MEDIA_FRAME_NB      = 1,  /*!< frame number */
    MEDIA_FORMAT        = 2,  /*!< media format: video, audio, ... */
    MEDIA_TIMESTAMP     = 3,  /*!< media timestamp: note, it is the RTP media timestamp. i.e. only 4bytes format */
    VIDEO_WIDTH         = 4,  /*!< media format video only: video frame width */
    VIDEO_HEIGHT        = 5,  /*!< media format video only: video frame height */
    VIDEO_COLORIMETRY   = 6,  /*!< media format video only: video colorimetry, use a _COLORIMETRY enum value */
    VIDEO_FORMAT        = 7,  /*!< media format video only: video sampling format, use an _SAMPLINGFMT enum value */
    VIDEO_DEPTH         = 8,  /*!< media format video only: sample bit depth (8 for YUV8bits, 10 for YUV10bits */
    AUDIO_NB_CHANNEL    = 9,  /*!< media format audio only: number of channel */
    AUDIO_FORMAT        = 10, /*!< media format audio only: audio format, use  an _AUDIOFMT enum value */
    AUDIO_SAMPLE_RATE   = 11, /*!< media format audio only: audio sample rate, use an _SAMPLERATE enum value */
    AUDIO_PACKET_TIME   = 12, /*!< media format audio only: packet time (in microseconds per AES67)  */
    MEDIA_PAYLOAD_SIZE  = 13, /*!< media payload size: for all media formats */
    VIDEO_FRAMERATE_CODE= 14, /*!< media format video only: SMPTE Framerate code specifying the FPS of the stream */
    MEDIA_SRC_TIMESTAMP = 15, /*!< media timestamp */
    MEDIA_IN_TIMESTAMP  = 16, /*!< media timestamp */
    MEDIA_OUT_TIMESTAMP = 17, /*!< media timestamp */
    VIDEO_SMPTEFRMCODE  = 18, /*!< media format video only: SAMPLE parameter from the source stream */
    NAME_INFORMATION    = 19, /*!< name information from sender module */
};

/**
 * \enum COLORIMETRY
 * \brief video colorimetry to be used for MediaHeader::VIDEO_COLORIMETRY
 *
 * Colorimetry accepted values  
 */
enum COLORIMETRY {
    BT601_5 = 0,           /*!< ITU-R BT.601-5 */
    BT709_2 = 1,           /*!< ITU-R BT.709-2 */
    SMPTE240M = 2,         /*!< SMPTE 240M Kr Kg Kb */
};

/**
 * \enum SAMPLINGFMT
 * \brief video sampling format to be used for MediaHeader::VIDEO_FORMAT
 */
enum SAMPLINGFMT {
    RGB = 1,                /*!< RGB pixel format */
    RGBA = 2,               /*!< RGBA pixel format */
    BGR = 3,                /*!< BGR pixel format */
    BGRA = 4,               /*!< BGRA pixel format */
    YCbCr_4_4_4 = 5,        /*!< 444 pixel format */
    YCbCr_4_2_2 = 6,        /*!< 422 pixel format */
    YCbCr_4_2_0 = 7,        /*!< 420 pixel format */
    YCbCr_4_1_1 = 8,        /*!< 411 pixel format */
};

/**
 * \enum AUDIOFMT
 * \brief audio format to be used for MediaHeader::AUDIO_FORMAT
 */
enum AUDIOFMT {
    L16_PCM = 1,            /*!< 16 bits PCM  */
    L24_PCM = 2,            /*!< 24 bits PCM  */
};

/**
 * \enum SAMPLERATE
 * \brief audio sample rate to be used for MediaHeader::AUDIO_SAMPLE_RATE
 */
enum SAMPLERATE {
    S_44_1KHz = 1,          /*!< 44.1Khz sample rate */
    S_48KHz = 2,            /*!< 48Khz sample rate */
    S_96KHz = 3,            /*!< 96Khz sample rate */
};

/**
 * \enum MEDIAFORMAT
 * \brief kind of media frame transported
 */
enum MEDIAFORMAT {
    NONE = -1,              /*!< Unknown media format */
    VIDEO = 1,              /*!< Video media format */
    AUDIO = 2,              /*!< Audio media format */
    ANC = 3,                /*!< Ancillary data  */
};

/**
* \brief Structure used with libvmi_frame_create_ext() to request an empty vMI frame
*/
struct vMIFrameInitStruct {
    MEDIAFORMAT     _media_format;  /*!< Kind of media */
    int             _media_size;    /*!< size of media buffer in bytes */
    int             _video_width;   /*!< (Optional) Video only, i.e: _media_format==MEDIAFORMAT::VIDEO. Video width in pixels. */
    int             _video_height;  /*!< (Optional) Video only, i.e: _media_format==MEDIAFORMAT::VIDEO. Video height in pixels. */
    int             _video_depth;   /*!< (Optional) Video only, i.e: _media_format==MEDIAFORMAT::VIDEO. Video sample size in bits. Typically 8 or 10. */
    SAMPLINGFMT     _video_smpfmt;  /*!< (Optional) Video only, i.e: _media_format==MEDIAFORMAT::VIDEO. Video sampling format. Supported is _RGB, _RGBA, _BGR, _BGRA, _YCbCr_4_2_2 */
};

#ifdef _WIN32

#pragma once

#ifdef VMILIBRARY_EXPORTS
#define VMILIBRARY_API __declspec(dllexport) 
#else
#define VMILIBRARY_API __declspec(dllimport) 
#endif

#else   // _WIN32

#define VMILIBRARY_API 

#endif  // _WIN32

/**
 *  libvMI library interface
 */

/**
 * A handle which controls a libvMI processing module.
 */
typedef int libvMI_module_handle;

/**
 * A handle which controls a single input or output for a libvMI processing module.
 */
typedef int libvMI_pin_handle;

/**
* A handle which controls a frames for a libvMI instance.
*/
typedef int libvMI_frame_handle;

/**
 * Signifies that the handle is invalid or irrelevant (depending on the context)
 */
#define LIBVMI_INVALID_HANDLE (-1)

 /**
 * \brief  A callback function data type which can be called back when there is an event to be processed
 * Note: For SMPTE Pins this function can be called several times depending on whether the received frame
 *       contains only video, or also contains audio or even ancillary data.
 *       The nature of received data is described in Media Format member of the buffer header accessible with libvMI_get_input_buffer() function
 *
 * \param const void* user data to be passed along to the callback function (see libvMI_create_module_ext for mote explanation)
 * \param CmdType the type of event being reported
 * \param int an optional parameter for the command
 * \param libvMI_pin_handle if this is an input event than the libvMI_pin_handle tells us which input this event belongs to, or LIBVMI_INVALID_HANDLE if this event does not belong to a module
 */
typedef void(*libvMI_input_callback)(const void*, CmdType, int, libvMI_pin_handle, libvMI_frame_handle);

/**
 * \brief Return a new frame to be used on libvMI
 *
 * Search an available vMIFrame, or create a new one if no frames available. This increase the ref counter for the vMIFrame.
 * Note that the returned handle is unique. It means that if a frames is reused because it was released before, a new handle
 * is created and associated to this frame.
 *
 * \return a handle which can be used to reference the frame later. LIBVMI_INVALID_HANDLE if error, or can't create frame.
 */
VMILIBRARY_API libvMI_frame_handle libvmi_frame_create();

/**
* \brief Return a new frame to be used on libvMI
*
* Search an available vMIFrame, or create a new one if no frames available. This increase the ref counter for the vMIFrame.
* Allow to provide a struct that contains all needed to initialize the frame (i.e. frame size)
* Note that the returned handle is unique. It means that if a frames is reused because it was released before, a new handle
* is created and associated to this frame.
*
* \param init reference to a vMIFrameInitStruct struct, defined above on libvMI.h
* \return a handle which can be used to reference the frame later. LIBVMI_INVALID_HANDLE if error, or can't create frame.
*/
VMILIBRARY_API libvMI_frame_handle libvmi_frame_create_ext(struct vMIFrameInitStruct &init);

/**
* \brief Release a frame
*
* Decrease the ref counter for the vMIFrame. The frame is release only when ref counter reach zero.
*
* \param hFrame handle of the frame to release
* \return int the ref counter value updated, -1 if not found
*/
VMILIBRARY_API int libvmi_frame_release(const libvMI_frame_handle hFrame);

/**
* \brief Add a reference to the frame
*
* Increase the ref counter for the vMIFrame. 
*
* \param hFrame handle of the frame
* \return int the ref counter value updated, -1 if not found
*/
VMILIBRARY_API int libvmi_frame_addref(const libvMI_frame_handle hFrame);

/**
* \brief Get the mediasize associated to the frame
*
* Return the size in byte of the media content associated with the frame. Note that the calculation of the size
* is dynamic. i.e. the size is updated when the frame is effectively associated to a media content. The size of
* a frame provided by vMI across the callback correspond to the size of content received on the corresponding input.
* The size of a user created frame correspond to the size define by the user along the creation process.
*
* \param hFrame handle of the frame
* \return int the media size in byte, -1 if not found
*/
VMILIBRARY_API int libvMI_frame_getsize(const libvMI_frame_handle hFrame);

/**
* \brief Query the pointer to the media content of a specific vMI frame.
*
* Corresponding frame size is provided from libvMI_frame_getsize()
*
* \param hFrame handle of the frame
* \return A pointer to the frame's data. NULL if not found.
*/
VMILIBRARY_API char*  libvMI_get_frame_buffer(const libvMI_frame_handle hFrame);

/**
* \brief Gets header values of a vMI frame.
*
* Value format from ::MediaHeader:
* <table><tr><th>MediaHeader</th><th>Kind of value</th></tr>
* <tr><td>MODULE_ID            </td><td>int                     </td></tr>
* <tr><td>MEDIA_FRAME_NB       </td><td>int                     </td></tr>
* <tr><td>MEDIA_FORMAT         </td><td>MEDIAFORMAT             </td></tr>
* <tr><td>MEDIA_TIMESTAMP      </td><td>unsigned int            </td></tr>
* <tr><td>VIDEO_WIDTH          </td><td>int                     </td></tr>
* <tr><td>VIDEO_HEIGHT         </td><td>int                     </td></tr>
* <tr><td>VIDEO_COLORIMETRY    </td><td>COLORIMETRY             </td></tr>
* <tr><td>VIDEO_FORMAT         </td><td>SAMPLINGFMT             </td></tr>
* <tr><td>VIDEO_DEPTH          </td><td>int                     </td></tr>
* <tr><td>AUDIO_NB_CHANNEL     </td><td>int                     </td></tr>
* <tr><td>AUDIO_FORMAT         </td><td>AUDIOFMT                </td></tr>
* <tr><td>AUDIO_SAMPLE_RATE    </td><td>SAMPLERATE              </td></tr>
* <tr><td>AUDIO_PACKET_TIM     </td><td>int                     </td></tr>
* <tr><td>MEDIA_PAYLOAD_SIZE   </td><td>int                     </td></tr>
* <tr><td>VIDEO_FRAMERATE_CODE </td><td>int                     </td></tr>
* <tr><td>MEDIA_SRC_TIMESTAMP  </td><td>unsigned long long      </td></tr>
* <tr><td>MEDIA_IN_TIMESTAMP   </td><td>unsigned long long      </td></tr>
* <tr><td>MEDIA_OUT_TIMESTAMP  </td><td>unsigned long long      </td></tr>
* <tr><td>VIDEO_SMPTEFRMCODE   </td><td>int                     </td></tr>
* <tr><td>NAME_INFORMATION     </td><td>char*                   </td></tr>
* </table>
*
* \param hFrame handle of the frame
* \param header kind of header to get value. Must be one of MediaHeader enum value
* \param value pointer to an int, used by libvMI to store the value.
*/
VMILIBRARY_API void libvMI_get_frame_headers(const libvMI_frame_handle hFrame, MediaHeader header, void* value);

/**
* \brief Sets values for a vMI frame headers
*
* Value format from MediaHeader:
* <table><tr><th>MediaHeader</th><th>Kind of value</th></tr>
* <tr><td>MODULE_ID            </td><td>int                     </td></tr>
* <tr><td>MEDIA_FRAME_NB       </td><td>int                     </td></tr>
* <tr><td>MEDIA_FORMAT         </td><td>MEDIAFORMAT             </td></tr>
* <tr><td>MEDIA_TIMESTAMP      </td><td>unsigned int            </td></tr>
* <tr><td>VIDEO_WIDTH          </td><td>int                     </td></tr>
* <tr><td>VIDEO_HEIGHT         </td><td>int                     </td></tr>
* <tr><td>VIDEO_COLORIMETRY    </td><td>COLORIMETRY             </td></tr>
* <tr><td>VIDEO_FORMAT         </td><td>SAMPLINGFMT             </td></tr>
* <tr><td>VIDEO_DEPTH          </td><td>int                     </td></tr>
* <tr><td>AUDIO_NB_CHANNEL     </td><td>int                     </td></tr>
* <tr><td>AUDIO_FORMAT         </td><td>AUDIOFMT                </td></tr>
* <tr><td>AUDIO_SAMPLE_RATE    </td><td>SAMPLERATE              </td></tr>
* <tr><td>AUDIO_PACKET_TIM     </td><td>int                     </td></tr>
* <tr><td>MEDIA_PAYLOAD_SIZE   </td><td>int                     </td></tr>
* <tr><td>VIDEO_FRAMERATE_CODE </td><td>int                     </td></tr>
* <tr><td>MEDIA_SRC_TIMESTAMP  </td><td>unsigned long long      </td></tr>
* <tr><td>MEDIA_IN_TIMESTAMP   </td><td>unsigned long long      </td></tr>
* <tr><td>MEDIA_OUT_TIMESTAMP  </td><td>unsigned long long      </td></tr>
* <tr><td>VIDEO_SMPTEFRMCODE   </td><td>int                     </td></tr>
* <tr><td>NAME_INFORMATION     </td><td>char*                   </td></tr>
* </table>
*
* Note that setting some headers content as VIDEO_DEPTH, MEDIA_PAYLOAD_SIZE, VIDEO_WIDTH and VIDEO_HEIGHT effectively change
* the size of the media buffer.
*
* \param hFrame handle of the frame
* \param header kind of header to get value. Must be one of MediaHeader enum value
* \param value pointer to a void which contains the value to set for the header
*/
VMILIBRARY_API void   libvMI_set_frame_headers(const libvMI_frame_handle hFrame, MediaHeader header, void* value);

/**
* \brief Return a parameter from current libvMI instance
*
* \param param kind of parameter to get value. Must be one of VMIPARAMETER enum value
* \param value pointer, used by libvMI to store the value.
* \return
*/
VMILIBRARY_API void   libvMI_get_parameter(VMIPARAMETER param, void* value);

/**
* \brief Set a parameter on current libvMI instance
*
* \param param kind of parameter to set value. Must be one of VMIPARAMETER enum value
* \param value pointer to an int which contains the value to set
* \return
*/
VMILIBRARY_API void   libvMI_set_parameter(VMIPARAMETER param, void* value);

/**
* \brief Set a parameter on libvMI output pin
*
* \param hModule the handle for the module which contains the output pin
* \param hOutput handle of the output pin
* \param param kind of parameter to set value. Must be one of OUTPUTPARAMETER enum value
* \param value pointer to an int which contains the value to set
* \return
*/
VMILIBRARY_API void libvMI_set_output_parameter(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput, OUTPUTPARAMETER param, void* value);

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
VMILIBRARY_API libvMI_module_handle libvMI_create_module(libvMI_input_callback func, const char* preconfig);

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
VMILIBRARY_API libvMI_module_handle libvMI_create_module_ext(libvMI_input_callback func, const char* preconfig, const void* user_data);

/**
 *  \brief Return the count of inputs available on the module
 *
 * \param module the handle of the module on which we wanted to know the count of inputs
 * \return count of inputs
 */
VMILIBRARY_API int libvMI_get_input_count(const libvMI_module_handle module);

/**
 *  \brief Return the count of outputs available on the module
 *
 * \param module the handle for the module on which we wanted to know the count of outputs
 * \return count of outputs
 */
VMILIBRARY_API int libvMI_get_output_count(const libvMI_module_handle module);

/**
*  \brief Return the handle of an input type identified by its index. Use libvMI_get_input_count
* to get the input count.
*
* \param module the handle for the module which contains the input pin
* \param index the index of the pin (on [0..(pin_count-1)], pin_count is the value returned by libvMI_get_input_count
* \return handle of input pin
*/
VMILIBRARY_API libvMI_pin_handle libvMI_get_input_handle(const libvMI_module_handle module, int index);

/**
*  \brief Return the handle of an output type identified by its index. Use libvMI_get_output_count
* to get the output count.
*
* \param module the handle for the module which contains the output pin
* \param index the index of the pin (on [0..(pin_count-1)], pin_count is the value returned by libvMI_get_output_count
* \return handle of ouput pin
*/
VMILIBRARY_API libvMI_pin_handle libvMI_get_output_handle(const libvMI_module_handle module, int index);

/**
* \brief Return the name of the module as it is define on configuration.
*
* \param hModule the handle for the module which contains the output pin
* \return the pointer on string where are stored the module name.
*/
VMILIBRARY_API const char* libvMI_get_module_name(const libvMI_module_handle hModule);

/**
* \brief Return the current configuration of the input pin identified by its handle
*
* \param hModule the handle for the module which contains the input pin
* \param hInput handle of the input pin
* \return pointer on the configuration string.
*/
VMILIBRARY_API const char* libvMI_get_input_config_stream(const libvMI_module_handle hModule, const libvMI_pin_handle hInput);

/**
* \brief Return the current configuration of the output pin identified by its handle
*
* \param hModule the handle for the module which contains the output pin
* \param hOutput handle of the output pin
* \return pointer on the configuration string.
*/
VMILIBRARY_API const char* libvMI_get_output_config_stream(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput);

/**
* \brief Return the number of modules managed by this library instance
*
* \return number of modules
*/
VMILIBRARY_API int libvMI_get_number_of_modules();

/**
* \brief Return the handle of a module identified by it's index. Use libvMI_get_number_of_modules to
* retreive the total number of modules on this library instance
*
* \param index index of module on library instance module list
* \return handle of found module.
*/
VMILIBRARY_API libvMI_module_handle libvMI_get_module_by_index(int index);

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
VMILIBRARY_API int libvMI_start_module(const libvMI_module_handle module);

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
VMILIBRARY_API int libvMI_stop_module(const libvMI_module_handle module);

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
VMILIBRARY_API int libvMI_send(const libvMI_module_handle hModule, const libvMI_pin_handle hOutput, const libvMI_frame_handle hFrame);

/**
 * \brief Frees up all resources allocated for the module. 
 *
 * Will do the stop if module running.
 * All handles created in relation to this module should not be used afterwards (module handle, and all corresponding inputs/output handles)
 *
 * \param module the module which will be closed
 * \return 0 when successful, -1 otherwise
 */
VMILIBRARY_API int libvMI_close(const libvMI_module_handle module);

#ifdef __cplusplus
}
#endif

#endif //_LIBVMI_H

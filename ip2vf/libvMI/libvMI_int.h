/**
* \file libvMI_int.h
* \brief vMI library internal function headers
* \author A.Taldir M.Hawari
* \version 1.0
* \date 15 january 2018
*
* Describes all internal interface of the LibvMI component. This is not exposed across the lib API?
*
* 2018/01/15 - version 1.0: Add frames support
*
*/

#ifndef _LIBVMI_INT_H
#define _LIBVMI_INT_H

#include <utility>

#include "libvMI.h"
#include "vmiframe.h"

CvMIFrame* libvMI_frame_get(const libvMI_frame_handle hFrame);
int        libvMI_get_free_frame_number();

#ifdef _WIN32
#pragma once

#ifdef VMILIBRARY_EXPORTS
#define VMILIBRARY_API_INT __declspec(dllexport)
#else /* VMILIBRARY_EXPORTS */
#define VMILIBRARY_API_INT __declspec(dllimport)
#endif /* VMILIBRARY_EXPORTS */

#else /* _WIN32 */
#define VMILIBRARY_API_INT
#endif /* _WIN32 */

VMILIBRARY_API_INT void* libvMI_get_metrics_handle(const libvMI_module_handle);

#endif //_LIBVMI_INT_H

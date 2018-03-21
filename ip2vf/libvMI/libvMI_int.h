/**
* \file libvMI.h
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

#endif //_LIBVMI_INT_H

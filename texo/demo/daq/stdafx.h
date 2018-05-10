#pragma once

#define COMPILE_TEXO_DEMO

#include <umc/disable_harmless_warnings.h>
#include <umc/qtguiandcore.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <math.h>
#include <texo_def.h>
#include <texo.h>
#include <daq.h>
#include <daq_def.h>
#include <amplio.h>

#ifndef FIRMWARE_PATH
    #define FIRMWARE_PATH "../../dat/"
#endif

#ifndef DAQ_FIRMWARE_PATH
    #define DAQ_FIRMWARE_PATH "../../../daq/fw/"
#endif

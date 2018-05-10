#pragma once

//#define COMPILEDAQ

#ifdef WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <conio.h>

#include <texo.h>
#include <texo_def.h>

#ifndef FIRMWARE_PATH
    #define FIRMWARE_PATH "../../dat/"
#endif

#ifdef COMPILEDAQ

    #include <daq.h>
    #include <daq_def.h>

    #ifndef DAQ_FIRMWARE_PATH
        #define DAQ_FIRMWARE_PATH "../../../daq/fw/"
    #endif

#endif

INIT(texo_daq_qt)

ADD_PCH_SUPPORT()
ADD_QT_SUPPORT()
LIST(APPEND LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/texo.lib ${CMAKE_CURRENT_SOURCE_DIR}/../../../daq/lib/daq.lib ${CMAKE_CURRENT_SOURCE_DIR}/../../../amplio/lib/amplio.lib)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../../amplio/inc ${CMAKE_CURRENT_SOURCE_DIR}/../../../daq/inc)
ADD_DEFINITIONS(-DIMPORT_TEXO -DFIRMWARE_PATH="\\"../../dat/\\"" -DDAQ_FIRMWARE_PATH="\\"../../../daq/fw/\\"")

CREATE_QT_EXECUTABLE()

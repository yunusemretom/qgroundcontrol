message(STATUS "QGC: Adding Custom Plugin")

# Enable custom build
set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
    APPEND PROPERTY COMPILE_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
)

if(ANDROID)
    set(CUSTOM_ANDROID_DIR "${CMAKE_SOURCE_DIR}/custom/android")
    if(EXISTS "${CUSTOM_ANDROID_DIR}")
        file(GLOB CUSTOM_ANDROID_FILES "${CUSTOM_ANDROID_DIR}/*")
        if(CUSTOM_ANDROID_FILES)
            message(STATUS "QGC: Custom Android package template found. Overlaying custom files...")
            set(DEFAULT_ANDROID_DIR "${CMAKE_SOURCE_DIR}/android")
            set(FINAL_ANDROID_DIR "${CMAKE_BINARY_DIR}/custom/android")
            file(COPY "${DEFAULT_ANDROID_DIR}/." DESTINATION "${FINAL_ANDROID_DIR}")
            # Not file(COPY): it skips a file whose destination has the same timestamp,
            # and a fresh checkout gives the stock and custom files identical mtimes, so
            # on CI the custom manifest and icons were silently dropped in favour of the
            # stock ones just copied. COPY_FILE always writes.
            file(GLOB_RECURSE CUSTOM_ANDROID_OVERLAY RELATIVE "${CUSTOM_ANDROID_DIR}" "${CUSTOM_ANDROID_DIR}/*")
            foreach(overlay_file IN LISTS CUSTOM_ANDROID_OVERLAY)
                get_filename_component(overlay_dir "${FINAL_ANDROID_DIR}/${overlay_file}" DIRECTORY)
                file(MAKE_DIRECTORY "${overlay_dir}")
                file(COPY_FILE "${CUSTOM_ANDROID_DIR}/${overlay_file}" "${FINAL_ANDROID_DIR}/${overlay_file}")
            endforeach()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                            "${DEFAULT_ANDROID_DIR}/"
                            "${CUSTOM_ANDROID_DIR}/"
                        )
            set(QGC_ANDROID_PACKAGE_SOURCE_DIR "${FINAL_ANDROID_DIR}" CACHE PATH "Path to a custom Android package template" FORCE)
            message(STATUS "QGC: Android package template path will be set to: ${QGC_ANDROID_PACKAGE_SOURCE_DIR}")
        else()
            message(STATUS "QGC: Custom Android package template empty. Using default.")
        endif()
    else()
        message(STATUS "QGC: No custom Android package template found. Using default.")
    endif()
endif()

# Our own, custom resources
list(APPEND CUSTOM_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc
)
set(QGC_RESOURCES ${QGC_RESOURCES} ${CUSTOM_RESOURCES} CACHE STRING "Paths to .qrc Resources" FORCE)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/res" CACHE STRING "Extra qml import paths" FORCE)

qt_add_library(CustomModule STATIC)

set_source_files_properties(res/Custom/Widgets/CustomArtificialHorizon.qml PROPERTIES QT_RESOURCE_ALIAS CustomArtificialHorizon.qml)
set_source_files_properties(res/Custom/Widgets/CustomAttitudeWidget.qml PROPERTIES QT_RESOURCE_ALIAS CustomAttitudeWidget.qml)
set_source_files_properties(res/Custom/Widgets/CustomInstrumentCluster.qml PROPERTIES QT_RESOURCE_ALIAS CustomInstrumentCluster.qml)
set_source_files_properties(res/Custom/Widgets/CustomIconButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomIconButton.qml)
set_source_files_properties(res/Custom/Widgets/CustomOnOffSwitch.qml PROPERTIES QT_RESOURCE_ALIAS CustomOnOffSwitch.qml)
set_source_files_properties(res/Custom/Widgets/CustomQuickButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomQuickButton.qml)
set_source_files_properties(res/Custom/Widgets/CustomSignalStrength.qml PROPERTIES QT_RESOURCE_ALIAS CustomSignalStrength.qml)
set_source_files_properties(res/Custom/Widgets/CustomToolBarButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomToolBarButton.qml)
set_source_files_properties(res/Custom/Widgets/CustomVehicleButton.qml PROPERTIES QT_RESOURCE_ALIAS CustomVehicleButton.qml)

qt_add_qml_module(CustomModule
    URI Custom.Widgets
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        res/Custom/Widgets/CustomArtificialHorizon.qml
        res/Custom/Widgets/CustomAttitudeWidget.qml
        res/Custom/Widgets/CustomIconButton.qml
        res/Custom/Widgets/CustomInstrumentCluster.qml
        res/Custom/Widgets/CustomOnOffSwitch.qml
        res/Custom/Widgets/CustomQuickButton.qml
        res/Custom/Widgets/CustomSignalStrength.qml
        res/Custom/Widgets/CustomToolBarButton.qml
        res/Custom/Widgets/CustomVehicleButton.qml
    NO_PLUGIN
)

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ProtocolProfile.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ProtocolProfile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/FrameCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/JsonDatagramCodec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/JsonDatagramCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/FieldMapper.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/FieldMapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/MAVLinkSynthesizer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/MAVLinkSynthesizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ProtocolChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/UdpProtocolChannel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/UdpProtocolChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ExternalProtocolConfiguration.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ExternalProtocolConfiguration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ExternalProtocolLink.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/ExternalProtocolLink.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/NdjsonCodec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/NdjsonCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/TcpProtocolChannel.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/TcpProtocolChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/CommandTranslator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/CommandTranslator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/CsvCodec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms/CsvCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompetitionServerClient.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompetitionServerClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AppSettings/CompetitionSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AppSettings/CompetitionSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin/CustomAutoPilotPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/CustomFirmwarePluginFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/TurretFirmwarePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin/TurretFirmwarePlugin.h
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_LIBRARIES
    CustomModule # Explicitly link QML module, needed by Qt 6.6.3, TODO: Remove when support for this version is dropped
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AutoPilotPlugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FirmwarePlugin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Comms
    CACHE INTERNAL "" FORCE
)

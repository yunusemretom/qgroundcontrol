target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        DeviceInfo.cc
        DeviceInfo.h
        JsonHelper.cc
        JsonHelper.h
        FileSystem/QGCCachedFileDownload.cc
        FileSystem/QGCCachedFileDownload.h
        FileSystem/QGCFileDownload.cc
        FileSystem/QGCFileDownload.h
        QGC.cc
        QGC.h
        QGCLogging.cc
        QGCLogging.h
        QGCLoggingCategory.cc
        QGCLoggingCategory.h
        FileSystem/QGCTemporaryFile.cc
        FileSystem/QGCTemporaryFile.h
        StateMachine.cc
        StateMachine.h
)

if(LINUX)
    target_sources(${CMAKE_PROJECT_NAME}
        PRIVATE
            SignalHandler.cc
            SignalHandler.h
    )
elseif(IOS)
    target_sources(${CMAKE_PROJECT_NAME}
        PRIVATE
            MobileScreenMgr.mm
            MobileScreenMgr.h
    )
    # The target's precompiled header is built as C++; an Objective-C++ file cannot use
    # it ("Objective-C was disabled in precompiled file"). TARGET_DIRECTORY because the
    # target is defined in the top-level directory, where source properties are read.
    set_source_files_properties(MobileScreenMgr.mm
        TARGET_DIRECTORY ${CMAKE_PROJECT_NAME}
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
endif()

target_include_directories(${CMAKE_PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        FileSystem
)

#===========================================================================#

add_subdirectory(Audio)
add_subdirectory(Compression)
add_subdirectory(Geo)
add_subdirectory(Shape)

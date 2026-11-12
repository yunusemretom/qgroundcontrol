target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        TurretFirmwarePluginTest.cc
        TurretFirmwarePluginTest.h
        ProtocolProfileTest.cc
        ProtocolProfileTest.h
        JsonDatagramCodecTest.cc
        JsonDatagramCodecTest.h
        FieldMapperTest.cc
        FieldMapperTest.h
        MAVLinkSynthesizerTest.cc
        MAVLinkSynthesizerTest.h
        UdpProtocolChannelTest.cc
        UdpProtocolChannelTest.h
        NdjsonCodecTest.cc
        NdjsonCodecTest.h
        CommandTranslatorTest.cc
        CommandTranslatorTest.h
        CsvCodecTest.cc
        CsvCodecTest.h
        TcpProtocolChannelTest.cc
        TcpProtocolChannelTest.h
        ExternalProtocolLinkTest.cc
        ExternalProtocolLinkTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

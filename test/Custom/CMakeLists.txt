target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
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
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

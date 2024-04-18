#pragma once
#include "netlib_header.h"


namespace netlib {
    enum class TypesEnum: uint16_t {
        Empty,

        PingMsgType,
        PongMsgType,

        FileBeginPullMsgType,
        FileRequestMsgType,
        FileBodyMsgType,
        FileBodyRespMsgType,
        FileEndMsgType,
        FileRequestEndMsgType,


        ConnectionRequestMsgType,
        ConnectionResponseMsgType,

        NewConnectionRequestMsgType,
        NewConnectionResponseMsgType,

        NewExtConnectionRequestMsgType,
        NewExtConnectionResponseMsgType,


        PathRequestPushMsgType,
        PathResponsePullMsgType,
        PathAddressPushMsgType,

    };
}

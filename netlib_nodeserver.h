#pragma once

#include <utility>

#include "netlib_header.h"
#include "netlib_server.h"
#include "netlib_typesenum.h"
#include "modules/netlib_filesystem.h"
#include "modules/netlib_sha1.h"

const uint16_t MAX_CONNECTIONS = 20;
const std::string VERSION = "0.1b";
const uint8_t MAX_TTL = 7;
const uint32_t MAX_REQ_LIVE = 20;

const uint16_t BETWEEN_REQ_TIME = 10;
const uint32_t FILE_CHUNK_SIZE = 16384;

namespace netlib {
    class NodeServer: public Server<TypesEnum> {
    public:
        NodeServer(const std::string& localAddress, uint16_t port, std::string downloadsPath) :
                Server<TypesEnum>(localAddress, port, TypesEnum::PingMsgType, TypesEnum::PongMsgType),
                m_fileSystem(downloadsPath),
                m_timer(*m_context)
        {
            m_downloadsPath = std::move(downloadsPath);
        }

        //=================================GENERAL=====================================

        void updateNode() {
            update();
            for (const auto& session: m_sessionsMap) {
                if (!session.second->checkAble()) {
                    disconnectUser(session.second->getId());
                }
            }
        }

        void onMessage(OwnedMessage<netlib::TypesEnum> &msg) override {
            uint16_t id = msg.session_->getId();
            if (checkConnectManager(msg.msg_)) {
                updateConnectManager(msg.msg_, id);
                return;
            }
            if (checkPathManager(msg.msg_)) {
                updatePathManager(msg.msg_, id);
                return;
            }
            if (checkFileManager(msg.msg_)) {
                updateFileManager(msg.msg_, id);
                return;
            }
        }

        void disconnectUser(uint16_t id) {
            disconnectClient(id);
        }

        //================================== CONNECT =============================================

        static bool checkConnectManager(const Message<TypesEnum> &msg) {
            std::vector<TypesEnum> checkArray = {
                    TypesEnum::ConnectionRequestMsgType, TypesEnum::ConnectionResponseMsgType,
                    TypesEnum::NewConnectionRequestMsgType, TypesEnum::NewConnectionResponseMsgType,
                    TypesEnum::NewExtConnectionRequestMsgType, TypesEnum::NewExtConnectionResponseMsgType,
            };

            for (TypesEnum type: checkArray) {
                if (type == msg.m_header.id)
                    return true;
            }
            return false;
        }

        enum class ConnectState {
            Begin,
            Handshake,
            Wait,
            Stop,
        };

        struct ConnectManager {
            ConnectState m_response = ConnectState::Begin;
            ConnectState m_request = ConnectState::Begin;
            uint16_t cntLeft = 0;
            uint16_t lastId = 0;
        };

        void requestNext(Message<TypesEnum> &msg, uint16_t id) {
            if (m_sessionsMap.size() >= MAX_CONNECTIONS) {
                Message<TypesEnum> req(TypesEnum::NewConnectionRequestMsgType);
                req << false;
                sendMessage(id, req);
                return;
            }
            Message<TypesEnum> req;
            req << getRealEp().address().to_string() << getRealEp().port() << true;
            m_connectMap[id].m_request = ConnectState::Wait;
            sendMessage(id, req);
        }

        void responseNext(Message<TypesEnum> &msg, uint16_t id) {
            if (m_sessionsMap.upper_bound(m_connectMap[id].lastId) == m_sessionsMap.end()) {
                Message<TypesEnum> resp(TypesEnum::NewConnectionResponseMsgType);
                resp << false;
                sendMessage(id, resp);
                m_connectMap[id].m_response = ConnectState::Stop;
                return;
            }
            uint16_t extId = m_sessionsMap.upper_bound(m_connectMap[id].lastId)->first;
            m_connectMap[id].m_response = ConnectState::Wait;
            m_connectMap[id].lastId = extId;
            std::string address;
            uint16_t port;
            msg >> port >> address;
            Message<TypesEnum> req(TypesEnum::NewExtConnectionRequestMsgType);
            req << id << address << port;
            sendMessage(extId, req);
        }

        void updateConnectManager(Message<TypesEnum> &msg, uint16_t id) {
            switch (msg.m_header.id) {
                case TypesEnum::ConnectionRequestMsgType: {
                    if (m_connectMap[id].m_response != ConnectState::Begin)
                        break;
                    std::string version;
                    msg >> version;
                    if (version != VERSION) {
                        Message<TypesEnum> resp(TypesEnum::ConnectionResponseMsgType);
                        resp << false;
                        sendMessage(id, resp);
                        disconnectUser(id);
                    } else {
                        Message<TypesEnum> resp(TypesEnum::ConnectionResponseMsgType);
                        resp << m_sessionsMap.size() << false;
                        sendMessage(id, resp);
                        m_connectMap[id].m_response = ConnectState::Handshake;
                    }
                    break;
                }
                case TypesEnum::ConnectionResponseMsgType: {
                    if (m_connectMap[id].m_request != ConnectState::Begin)
                        break;
                    bool resp;
                    msg >> resp;
                    if (resp) {
                        msg >> m_connectMap[id].cntLeft;
                        requestNext(msg, id);
                    } else {
                        disconnectUser(id);
                    }
                    break;
                }
                case TypesEnum::NewConnectionRequestMsgType: {
                    if (m_connectMap[id].m_response != ConnectState::Handshake) {
                        break;
                    }
                    bool req_flag;
                    msg >> req_flag;
                    if (!req_flag) {
                        m_connectMap[id].m_response = ConnectState::Stop;
                        break;
                    }
                    responseNext(msg, id);
                    break;
                }
                case TypesEnum::NewConnectionResponseMsgType: {
                    if (m_connectMap[id].m_request != ConnectState::Wait) {
                        break;
                    }
                    bool resp;
                    msg >> resp;
                    if (!resp) {
                        m_connectMap[id].m_request = ConnectState::Stop;
                        break;
                    }
                    std::string address;
                    uint16_t port;
                    msg >> port >> address;
                    connectToHost(address, port);
                    requestNext(msg, id);
                    break;
                }
                case TypesEnum::NewExtConnectionRequestMsgType: {
                    if (m_sessionsMap.size() < MAX_CONNECTIONS) {
                        Message<TypesEnum> resp(TypesEnum::NewExtConnectionResponseMsgType);
                        resp << false;
                        sendMessage(id, resp);
                    } else {
                        std::string address;
                        uint16_t port;
                        uint16_t extId;
                        msg >> port >> address >> extId;
                        Message<TypesEnum> resp(TypesEnum::NewExtConnectionResponseMsgType);
                        resp << extId << address << port << true;
                    }
                    break;
                }
                case TypesEnum::NewExtConnectionResponseMsgType: {
                    bool resp_flag;
                    msg >> resp_flag;
                    if (!resp_flag) {
                        responseNext(msg, id);
                        break;
                    }
                    std::string address;
                    uint16_t port;
                    uint16_t extId;
                    msg >> port >> address >> extId;
                    Message<TypesEnum> resp(TypesEnum::NewConnectionResponseMsgType);
                    resp << address << port << true;
                    sendMessage(extId, resp);
                    break;
                }
            }
        }

        uint16_t connect(asio::ip::udp::endpoint &ep) {
            uint16_t id = connectToHost(ep);
            Message<TypesEnum> startRequest(TypesEnum::ConnectionRequestMsgType);
            startRequest << VERSION;
            sendMessage(id, startRequest);
            return id;
        }



        //=========================================PATH-REQUESTS===================================================

        static bool checkPathManager(Message<TypesEnum> &msg) {
            return msg.m_header.id == TypesEnum::PathRequestPushMsgType
                    || msg.m_header.id == TypesEnum::PathResponsePullMsgType;
        }

        bool checkTarget(Message<TypesEnum> &msg, uint16_t id) {
            std::string fileId;
            msg >> fileId;
            bool res = m_fileSystem.checkFile(fileId);
            msg << fileId;
            return res;
        }

        void sendResponse(Message<TypesEnum> &msg, uint16_t id, const std::string& reqId) {
            Message<TypesEnum> resp(TypesEnum::PathResponsePullMsgType);
            resp << getRealEp().address().to_string() << getRealEp().port() << reqId;
            uint16_t reserved = reservePort();
            m_requestsMap[reqId].responseReservedId = reserved;
            m_requestsMap[reqId].fromResponseId = 0;
            sendMessage(id, resp);
        }
        
        void updatePathManager(Message<TypesEnum> &msg, uint16_t id) {
            switch (msg.m_header.id) {
                case TypesEnum::PathRequestPushMsgType: {
                    std::string reqId;
                    uint8_t TTL;
                    msg >> reqId >> TTL;

                    if (m_requestsMap.find(reqId) == m_requestsMap.end() ||
                        clock() - m_requestsMap[reqId].createdTime >= MAX_REQ_LIVE * CLOCKS_PER_SEC) {
                        m_requestsMap[reqId].createdTime = clock();
                        m_requestsMap[reqId].fromRequestId = id;

                        if (checkTarget(msg, id)) {
                            sendResponse(msg, id, reqId);
                            break;
                        }

                        TTL = std::max(MAX_TTL, TTL);
                        TTL--;
                        if (TTL <= 0)
                            return;

                        msg << TTL << reqId;

                        for (const auto& session: m_sessionsMap) {
                            if (session.first == id)
                                continue;
                            sendMessage(session.first, msg);
                        }
                    }
                    break;
                }
                case TypesEnum::PathResponsePullMsgType: {
                    std::string reqId;
                    msg >> reqId;
                    if (m_requestsMap.find(reqId) == m_requestsMap.end() ||
                        (clock() - m_requestsMap[reqId].createdTime) > MAX_REQ_LIVE * CLOCKS_PER_SEC) {
                        break;
                    }

                    if (m_requestsMap[reqId].fromRequestId == 0) {
                        std::string address;
                        uint16_t port;
                        msg >> port >> address;

                        std::string realAddress = getRealEp().address().to_string();
                        uint16_t realPort = getRealEp().port();
                        connectToHost(address, port);

                        Message<TypesEnum> req(TypesEnum::PathAddressPushMsgType);
                        req << realAddress << realPort << reqId;
                        sendMessage(id, req);
                    } else {
                        m_requestsMap[reqId].fromResponseId = id;
                        msg << reqId;
                        sendMessage(m_requestsMap[reqId].fromResponseId, msg);
                    }
                    break;
                }
                case TypesEnum::PathAddressPushMsgType: {
                    std::string reqId;
                    msg >> reqId;
                    if (m_requestsMap.find(reqId) == m_requestsMap.end() ||
                        (clock() - m_requestsMap[reqId].createdTime) > MAX_REQ_LIVE * CLOCKS_PER_SEC) {
                        break;
                    }

                    if (m_requestsMap[reqId].fromResponseId == 0) {
                        std::string address;
                        uint16_t port;
                        msg >> port >> address;

                        connectToHost(address, port, m_requestsMap[reqId].responseReservedId);
                        sendBeginFile(id, reqId);

                    } else {
                        msg << reqId;
                        sendMessage(m_requestsMap[reqId].fromResponseId, msg);
                    }
                    break;
                }
            }
        }

        void sendPathRequest(std::string &fileId) {
            Message<TypesEnum> req(TypesEnum::PathRequestPushMsgType);
            req << MAX_TTL << fileId;
            for (const auto &session: m_sessionsMap) {
                sendMessage(session.first, req);
            }
        }

        struct Request {
            uint32_t createdTime;
            uint16_t fromResponseId;
            uint16_t fromRequestId;
            uint16_t responseReservedId;
        };


        //======================================FILE SENDING=================================

        void sendBeginFile(uint16_t userId, std::string &fileId) {
            Message<TypesEnum> req(TypesEnum::FileBeginPullMsgType);
            req << fileId;
            sendMessage(userId, req);
        }

        static bool checkFileManager(Message<TypesEnum> &msg) {
            std::vector<TypesEnum> checkArray = {
                    TypesEnum::FileRequestMsgType, TypesEnum::FileBeginPullMsgType,
                    TypesEnum::FileBodyMsgType, TypesEnum::FileEndMsgType,
            };

            for (TypesEnum type: checkArray) {
                if (type == msg.m_header.id)
                    return true;
            }
            return false;
        }

        void requestNextPiece(uint16_t id, std::string fileId) {
            if (m_fileSystem.checkFile(fileId))
                finishRequesting(id, fileId);
            Message<TypesEnum> msg(TypesEnum::FileRequestMsgType);
            uint16_t pieceNum = m_fileSystem.getNonePiece(fileId);
            if (pieceNum == (uint16_t )-1) {
                m_timer.expires_after(std::chrono::milliseconds(BETWEEN_REQ_TIME * 1000));
                m_timer.async_wait([this, id, fileId](std::error_code ec) {
                    if (!ec) {
                        requestNextPiece(id, fileId);
                    } else {
                        std::cerr << "Waiting error: " << ec.message() << "\n";
                    }
                });
            }
            else {
                m_filesMap[id][fileId].pieceNum = pieceNum;
                m_filesMap[id][fileId].fileStream.open(m_fileSystem.getPath(fileId, pieceNum), std::ios::in | std::ios::binary);
                msg << pieceNum << fileId;
                sendMessage(id, msg);
            }
        }

        void finishRequesting(uint16_t id, std::string &fileId) {
            Message<TypesEnum> msg(TypesEnum::FileRequestEndMsgType);
            msg << fileId;
            sendMessage(id, msg);
        }

        void finishSending(uint16_t id, std::string &fileId) {
            Message<TypesEnum> msg(TypesEnum::FileEndMsgType);
            msg << fileId;
            sendMessage(id, msg);
        }

        void nextBodyPiece(uint16_t id, std::string &fileId) {
            if (!m_filesMap[id][fileId].fileStream.is_open()) {
                return;
            }
            uint32_t bytesLeft = m_filesMap[id][fileId].fileSize - m_filesMap[id][fileId].fileStream.tellg();
            if (bytesLeft)
                finishSending(id, fileId);
            else {
                std::vector<char> buffer(std::min(bytesLeft, FILE_CHUNK_SIZE));
                m_filesMap[id][fileId].fileStream.read(buffer.data(), buffer.size());
                Message<TypesEnum> msg(TypesEnum::FileBodyMsgType);
                msg << buffer;
                sendMessage(id, msg);
            }
        }

        void updateFileManager(Message<TypesEnum> &msg, uint16_t id) {
            switch (msg.m_header.id) {
                case TypesEnum::FileBeginPullMsgType: {
                    std::string fileId;
                    msg >> fileId;
                    if (m_filesMap[id].find(fileId) == m_filesMap[id].end())
                        break;
                    requestNextPiece(id, fileId);
                    break;
                }
                case TypesEnum::FileRequestMsgType: {
                    std::string fileId;
                    msg >> fileId;
                    if (m_filesMap[id].find(fileId) == m_filesMap[id].end())
                        break;
                    if (!m_fileSystem.checkFile(fileId))
                        break;
                    uint16_t pieceNum;
                    msg >> pieceNum;
                    std::string path = m_fileSystem.getPath(fileId, pieceNum);
                    m_filesMap[id][fileId].fileSize = std::filesystem::file_size(path);
                    m_filesMap[id][fileId].fileStream.open(path, std::ios::binary | std::ios::in);
                    nextBodyPiece(id, fileId);
                    break;
                }
                case TypesEnum::FileEndMsgType: {
                    std::string fileId;
                    msg >> fileId;
                    if (m_filesMap[id].find(fileId) == m_filesMap[id].end())
                        break;
                    m_filesMap[id][fileId].fileStream.close();
                    m_fileSystem.addPiece(fileId, m_filesMap[id][fileId].pieceNum);
                    requestNextPiece(id, fileId);
                    break;
                }
                case TypesEnum::FileBodyMsgType: {
                    std::string fileId;
                    msg >> fileId;
                    if (m_filesMap[id].find(fileId) == m_filesMap[id].end())
                        break;
                    if (!m_filesMap[id][fileId].fileStream.is_open()) {
                        break;
                    }
                    std::vector<char> vec;
                    msg >> vec;
                    m_filesMap[id][fileId].fileStream.write(vec.data(), vec.size());
                    break;
                }
                case TypesEnum::FileBodyRespMsgType: {
                    std::string fileId;
                    msg >> fileId;
                    if (m_filesMap[id].find(fileId) == m_filesMap[id].end())
                        break;
                    nextBodyPiece(id, fileId);
                }
                case TypesEnum::FileRequestEndMsgType: {
                    std::string fileId;
                    msg >> fileId;
                    if (m_filesMap[id].find(fileId) == m_filesMap[id].end())
                        break;
                    m_filesMap[id].erase(fileId);
                    break;
                }
            }
        }

        struct FileStruct {
            std::fstream fileStream;
            uint16_t pieceNum;
            uint64_t fileSize;
        };

        //==============================High-Level Functions=====================================

        void uploadFile(std::string &filePath) {
            m_fileSystem.uploadFile(filePath);
        }

        void downloadFile(std::string &infoPath) {
            std::string fileId = m_fileSystem.addFile(infoPath);
            sendPathRequest(fileId);
        }

    private:

        std::string m_downloadsPath;

        std::map<uint16_t, ConnectManager> m_connectMap;

        std::map<std::string, Request> m_requestsMap;

        std::map<uint16_t, std::map<std::string, FileStruct>> m_filesMap;

        asio::steady_timer m_timer;


        FileSystem m_fileSystem;
    };
}
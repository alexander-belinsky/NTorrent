#pragma once

#include "../netlib_header.h"
#include "netlib_sha1.h"

const std::string INFO_FILETYPE = ".info";
const std::string DATA_FILETYPE = ".data";

const std::string DATA_PATH = "./Data";

namespace netlib {
    class FileSystem {
    public:
        FileSystem(std::string path) {
            m_filesPath = std::move(path);
            if (!std::filesystem::exists(DATA_PATH)) {
                std::filesystem::create_directories(DATA_PATH);
            }
            m_filesInfo.clear();
            for (const auto & entry : std::filesystem::directory_iterator(DATA_PATH)) {
                if (!checkInfoFile(entry.path().string()))
                    continue;
                std::string fileId;
                size_t sha1Cnt;
                std::fstream fin(entry.path());
                fin >> fileId >> sha1Cnt;
                m_filesInfo[fileId].fileId = fileId;
                m_filesInfo[fileId].pieces.resize(sha1Cnt);
                for (size_t j = 0; j < sha1Cnt; j++) {
                    fin >> m_filesInfo[fileId].pieces[j].sha1 >> m_filesInfo[fileId].pieces[j].size;
                }
                fin >> m_filesInfo[fileId].fileName;
                checkPieces(m_filesInfo[fileId]);
            }
        }

        bool checkInfoFile(std::string path) {
            return path.ends_with(INFO_FILETYPE);
        }

        enum struct PieceState {
            None,
            Open,
            Have,
        };

        struct PieceInfo {
            PieceState state;
            uint64_t size;
            std::string sha1;
            uint64_t lastTime = -1;
        };

        struct FileInfo {
            std::string fileName;
            std::string fileId;
            std::vector<PieceInfo> pieces;
        };

        bool checkExists(const std::string &&path) {
            std::fstream file(path);
            bool res = file.is_open();
            file.close();
            return res;
        }

        bool checkExists(const std::string &path) {
            std::fstream file(path);
            bool res = file.is_open();
            file.close();
            return res;
        }

        bool checkPiece(FileInfo &file, uint64_t pieceId) {
            if (!checkExists(DATA_PATH + "\\" + file.fileId + "\\" + (std::to_string(pieceId) + DATA_FILETYPE))) {
                return false;
            }
            uint64_t fileSize = std::filesystem::file_size(DATA_PATH + "\\" + file.fileId + "\\" + (std::to_string(pieceId) + DATA_FILETYPE));
            if (fileSize != file.pieces[pieceId].size) {
                return false;
            }
            if (m_sha1.from_file(DATA_PATH + "\\" + file.fileId + "\\" + (std::to_string(pieceId) + DATA_FILETYPE)) != file.pieces[pieceId].sha1) {
                return false;
            }
            return true;
        }

        void checkPieces(FileInfo &file) {
            std::filesystem::create_directories(DATA_PATH + "\\" + file.fileId);
            for (int i = 0; i < file.pieces.size(); i++) {
                if (checkPiece(file, i))
                    file.pieces[i].state = PieceState::Have;
                else
                    file.pieces[i].state = PieceState::None;
            }
        }

        void updateFullFile(const std::string &path) {
            std::string fileId;
            size_t sha1Cnt;
            std::fstream fin(path);
            fin >> fileId >> sha1Cnt;
            m_filesInfo[fileId].fileId = fileId;
            m_filesInfo[fileId].pieces.resize(sha1Cnt);
            for (size_t j = 0; j < sha1Cnt; j++) {
                fin >> m_filesInfo[fileId].pieces[j].sha1 >> m_filesInfo[fileId].pieces[j].size;
            }
            fin >> m_filesInfo[fileId].fileName;
            checkPieces(m_filesInfo[fileId]);
        }

        bool checkFile(const std::string &fileId) {
            if (m_filesInfo.find(fileId) == m_filesInfo.end())
                return false;
            for (const auto &piece: m_filesInfo[fileId].pieces) {
                if (piece.state != PieceState::Have) {
                    return false;
                }
            }
            return true;
        }

        uint64_t getNonePiece(std::string &fileId) {
            if (m_filesInfo.find(fileId) != m_filesInfo.end()) {
                for (int i = 0; i < m_filesInfo[fileId].pieces.size(); i++) {
                    if (m_filesInfo[fileId].pieces[i].state == PieceState::None) {
                        m_filesInfo[fileId].pieces[i].state = PieceState::Open;
                        m_filesInfo[fileId].pieces[i].lastTime = clock();
                        return i;
                    }
                }
                return -1;
            }
            return -1;
        }

        void addPiece(std::string &fileId, uint64_t pieceId) {
            if (m_filesInfo.find(fileId) == m_filesInfo.end())
                return;
            if (m_filesInfo[fileId].pieces.size() <= pieceId)
                return;
            if (checkPiece(m_filesInfo[fileId], pieceId))
                m_filesInfo[fileId].pieces[pieceId].state = PieceState::Have;
            else
                m_filesInfo[fileId].pieces[pieceId].state = PieceState::None;

            for (auto &p: m_filesInfo[fileId].pieces) {
                if (p.state != PieceState::Have)
                    return;
            }
            mergePieces(m_filesInfo[fileId]);
        }

        std::string getFileName(std::string &path)
        {
            return path.substr(path.find_last_of("/\\") + 1);
        }

        void splitFile(std::string path, const std::string& fileId, uint64_t pieceSize = 262144) {
            if (!std::filesystem::exists(path))
                return;
            std::cout << "[FILE-SYSTEM]: Parsing file:\n";
            std::fstream fout(DATA_PATH + "\\" + fileId + INFO_FILETYPE, std::ios::out);
            std::fstream fin(path, std::ios::in | std::ios::binary);

            std::filesystem::create_directories(DATA_PATH + "\\" + fileId);

            int numPieces = (std::filesystem::file_size(path) + pieceSize - 1) / pieceSize;

            fout.write((fileId + "\n").c_str(), (fileId + "\n").size());
            fout.write((std::to_string(numPieces) + "\n").c_str(), (std::to_string(numPieces) + "\n").size());

            for (int i = 0; i < numPieces; i++) {
                std::fstream pieceFile(DATA_PATH + "\\" + fileId + "\\" + std::to_string(i) + DATA_FILETYPE, std::ios::out | std::ios::binary);
                std::vector<char> buffer(pieceSize);
                if (i == numPieces - 1)
                    buffer.resize(std::filesystem::file_size(path) % pieceSize);
                fin.read(buffer.data(), buffer.size());
                pieceFile.write(buffer.data(), buffer.size());
                pieceFile.close();
                std::string sha1 = netlib::SHA1::from_file(DATA_PATH + "\\" + fileId + "\\" + std::to_string(i) + DATA_FILETYPE);
                fout.write((sha1 + " ").c_str(), (sha1 + " ").size());
                fout.write((std::to_string(buffer.size()) + "\n").c_str(), (std::to_string(buffer.size()) + "\n").size());
                std::cout << "parsing file: " << i + 1 << "/" << numPieces << "\n";
            }
            std::string fileName = getFileName(path);
            fout.write(fileName.data(), fileName.size());
            std::cout << "[FILE-SYSTEM]: File parsed\n";
            fin.close();
            fout.close();
        }

        std::string addFile(const std::string& path) {
            if (!checkExists(path)) {
                std::cout << "[FILE-SYSTEM]: File does not exist\n";
                return "";
            }
            std::string fileId;
            size_t sha1Cnt;
            std::fstream fin(path, std::ios::binary | std::ios::in);
            fin >> fileId >> sha1Cnt;
            if (m_filesInfo.find(fileId) != m_filesInfo.end())
                return fileId;
            std::filesystem::create_directories(DATA_PATH + "\\" + fileId);
            std::ofstream fout(DATA_PATH + "\\" + fileId + INFO_FILETYPE, std::ios::binary | std::ios::out);
            fout << fileId << " " << sha1Cnt;
            fout << fin.rdbuf();
            fin.close();
            fout.close();
            updateFullFile(path);
            return fileId;
        }

        std::string getPath(const std::string& fileId, uint64_t pieceNum) {
            return DATA_PATH + "\\" + fileId + "\\" + std::to_string(pieceNum) + DATA_FILETYPE;
        }

        void mergePieces(FileInfo &fileInfo) {
            std::fstream fout;
            std::cout << "[FILE-SYSTEM]: Merging file:\n";
            std::filesystem::create_directories(m_filesPath);
            fout.open(m_filesPath + "\\" + fileInfo.fileName, std::ios::out | std::ios::binary);
            for (int i = 0; i < fileInfo.pieces.size(); i++) {
                std::ifstream fin(getPath(fileInfo.fileId, i), std::ios::binary);
                fout << fin.rdbuf();
                fin.close();
                std::cout << "merging file: " << i + 1 << "/" << fileInfo.pieces.size() << "\n";
            }
            std::cout << "[FILE-SYSTEM]: File merged:\n";
            fout.close();
        }

        void mergePieces(std::string &fileId) {
            mergePieces(m_filesInfo[fileId]);
        }

        void uploadFile(std::string &path) {
            if (!checkExists(path)) {
                std::cout << "[FILE-SYSTEM]: File does not exist\n";
                return;
            }
            std::cout << "[FILE-SYSTEM]: SHA-1 calculating...\n";
            std::string sha1 = netlib::SHA1::from_file(path);
            if (m_filesInfo.find(sha1) != m_filesInfo.end())
                return;
            splitFile(path, sha1);
            std::cout << "[FILE-SYSTEM]: Checking...\n";
            updateFullFile(DATA_PATH + "\\" + sha1 + INFO_FILETYPE);
            std::cout << "[FILE-SYSTEM]: File uploaded\n";
        }

    private:
        std::string m_filesPath;
        std::map<std::string, FileInfo> m_filesInfo;
        SHA1 m_sha1;
    };
}

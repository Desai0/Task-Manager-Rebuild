#include "file_service.h"
#include <fstream>
#include <windows.h>
#include <string>
#include <iostream>
#include <sstream> // ��� stringstream
#include <filesystem> // ��� std::filesystem::file_size

FileService::FileService() : fileHandle(nullptr), currentFile("") {
    std::cout << "FileService::FileService()" << std::endl; // �������: �����������
}
FileService::~FileService() {
    std::cout << "FileService::~FileService()" << std::endl; // �������: ����������
    if (fileHandle) closeFile();
}

bool FileService::openFile(const std::string& filename) {
    std::cout << "FileService::openFile: filename='" << filename << "'" << std::endl; // �������: openFile, ������� ���������
    // ��������� ���������� ����, ���� �� ��� ������
    if (fileHandle) {
        closeFile();
    }

    fileHandle = new std::ifstream(filename);

    if (fileHandle->is_open()) {
        currentFile = filename;
        std::cout << "FileService::openFile: File opened successfully, currentFile='" << currentFile << "'" << std::endl; // �������: ���� ������
        return true;
    }
    else {
        delete fileHandle;
        fileHandle = nullptr;
        std::cerr << "FileService::openFile: Error opening file: '" << filename << "'" << std::endl; // �������: ������ �������� �����
        return false;
    }
}

std::string FileService::readFile() {
    std::cout << "FileService::readFile: currentFile='" << currentFile << "', fileHandle=" << fileHandle << std::endl; // �������: readFile, ���������

    if (!fileHandle || currentFile.empty()) {
        std::cerr << "FileService::readFile: Error: No file is currently open" << std::endl; // �������: ��� ��������� �����
        return "Error: No file is currently open";
    }

    fileHandle->clear();
    fileHandle->seekg(0, std::ios::beg);

    std::string content;
    std::string line;
    while (std::getline(*fileHandle, line)) {
        content += line + "\n";
    }

    std::cout << "FileService::readFile: Content read: '" << content << "'" << std::endl; // �������: ����������� ����������
    return content;
}

bool FileService::writeFile(const std::string& data) {
    std::cout << "FileService::writeFile: currentFile='" << currentFile << "', data='" << data << "'" << std::endl; // �������: writeFile, ������� ���������

    if (currentFile.empty()) {
        std::cerr << "FileService::writeFile: Error: No file is currently open (currentFile is empty)" << std::endl; // �������: ��� ��������� �����
        return false; // ���� �� ��� ������
    }

    std::ofstream outFile(currentFile);
    if (!outFile.is_open()) {
        std::cerr << "FileService::writeFile: Error opening file for writing: '" << currentFile << "'" << std::endl; // �������: ������ �������� ��� ������
        return false;
    }

    outFile << data;
    outFile.close();

    std::cout << "FileService::writeFile: Data written successfully" << std::endl; // �������: ������ �������
    return true;
}

int FileService::countCharacters() {
    std::cout << "FileService::countCharacters: currentFile='" << currentFile << "'" << std::endl; // �������: countCharacters, ���������
    std::string content = readFile();
    if (content.find("Error:") == 0) {
        std::cerr << "FileService::countCharacters: Error in readFile, returning -1" << std::endl; // �������: ������ readFile
        return -1;
    }

    int count = content.length();
    std::cout << "FileService::countCharacters: Character count: " << count << std::endl; // �������: ���������� ��������
    return count;
}

bool FileService::deleteFile() {
    std::cout << "FileService::deleteFile: currentFile='" << currentFile << "'" << std::endl; // �������: deleteFile, ���������
    if (currentFile.empty()) {
        std::cerr << "FileService::deleteFile: Error: No file is currently open (currentFile is empty)" << std::endl; // �������: ��� ��������� �����
        return false; // ���� �� ��� ������
    }

    // ��������� ����, ���� �� ������
    if (fileHandle) {
        closeFile();
    }

    bool result = DeleteFileA(currentFile.c_str());
    if (result) {
        std::cout << "FileService::deleteFile: File deleted successfully: '" << currentFile << "'" << std::endl; // �������: �������� �������
    }
    else {
        DWORD error = GetLastError();
        std::cerr << "FileService::deleteFile: Error deleting file: '" << currentFile << "', error code: " << error << std::endl; // �������: ������ ��������
    }
    return result;
}

bool FileService::closeFile() {
    std::cout << "FileService::closeFile: fileHandle=" << fileHandle << ", currentFile='" << currentFile << "'" << std::endl; // �������: closeFile, ���������
    if (!fileHandle) {
        std::cerr << "FileService::closeFile: Error: No file is currently open (fileHandle is null)" << std::endl; // �������: ��� ��������� �����
        return false; // ���� �� ��� ������
    }

    fileHandle->close();
    delete fileHandle;
    fileHandle = nullptr;
    currentFile = "";

    std::cout << "FileService::closeFile: File closed successfully" << std::endl; // �������: �������� �������
    return true;
}



bool FileService::findString(const std::string& stringToFind) {
    std::cout << "FileService::findString: stringToFind='" << stringToFind << "'" << std::endl;
    std::string content = readFile();
    if (content.find("Error:") == 0) {
        return false; // ������ ������ �����
    }
    return content.find(stringToFind) != std::string::npos;
}

int FileService::findCharacters(const std::string& charactersToFind) {
    std::cout << "FileService::findCharacters: charactersToFind='" << charactersToFind << "'" << std::endl;
    std::string content = readFile();
    if (content.find("Error:") == 0) {
        return -1; // ������ ������ �����
    }
    int count = 0;
    for (char c : content) {
        if (charactersToFind.find(c) != std::string::npos) {
            count++;
        }
    }
    return count;
}

int FileService::countWords() {
    std::cout << "FileService::countWords: currentFile='" << currentFile << "'" << std::endl;
    std::string content = readFile();
    if (content.find("Error:") == 0) {
        return -1; // ������ ������ �����
    }
    std::stringstream ss(content);
    std::string word;
    int wordCount = 0;
    while (ss >> word) {
        wordCount++;
    }
    return wordCount;
}

long long FileService::getFileSize() {
    std::cout << "FileService::getFileSize: currentFile='" << currentFile << "'" << std::endl;
    if (currentFile.empty()) {
        return -1; // ��� ��������� �����
    }
    try {
        return std::filesystem::file_size(currentFile);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "FileService::getFileSize: Error getting file size: " << e.what() << std::endl;
        return -1; // ������ ��������� ������� �����
    }
}

bool FileService::clearFileContent() {
    std::cout << "FileService::clearFileContent: currentFile='" << currentFile << "'" << std::endl;
    if (currentFile.empty()) {
        return false; // ��� ��������� �����
    }
    std::ofstream outFile(currentFile, std::ofstream::trunc); // ��������� ���� � ������ trunc ��� �������
    if (!outFile.is_open()) {
        std::cerr << "FileService::clearFileContent: Error opening file for writing (trunc): '" << currentFile << "'" << std::endl;
        return false;
    }
    outFile.close();
    return true;
}
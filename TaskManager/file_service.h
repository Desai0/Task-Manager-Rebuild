#ifndef FILE_SERVICE_H  
#define FILE_SERVICE_H  

#include <string>  
#include <fstream>  // ��������� include ��� std::ifstream

class FileService {
public:
	FileService();
	~FileService();

	bool openFile(const std::string& filename);  // �������� ���������� �����  
	bool closeFile();                           // �������� �����  
	std::string readFile();                     // ������ �� �����  
	bool writeFile(const std::string& data);    // ������ � ����  
	int countCharacters();                      // ������� �������� � �����  
	bool deleteFile();                          // �������� �����  

	bool findString(const std::string& stringToFind);
	int findCharacters(const std::string& charactersToFind);
	int countWords();
	long long getFileSize();
	bool clearFileContent();

private:
	std::ifstream* fileHandle;  // ��������� �� ����� �����  
	std::string currentFile;  // ���� � �������� �����  
};
#endif
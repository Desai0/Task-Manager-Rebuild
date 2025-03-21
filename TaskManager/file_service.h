#ifndef FILE_SERVICE_H  
#define FILE_SERVICE_H  

#include <string>  
#include <fstream>  // Добавляем include для std::ifstream

class FileService {
public:
	FileService();
	~FileService();

	bool openFile(const std::string& filename);  // Открытие текстового файла  
	bool closeFile();                           // Закрытие файла  
	std::string readFile();                     // Чтение из файла  
	bool writeFile(const std::string& data);    // Запись в файл  
	int countCharacters();                      // Подсчет символов в файле  
	bool deleteFile();                          // Удаление файла  

	bool findString(const std::string& stringToFind);
	int findCharacters(const std::string& charactersToFind);
	int countWords();
	long long getFileSize();
	bool clearFileContent();

private:
	std::ifstream* fileHandle;  // Указатель на поток файла  
	std::string currentFile;  // Путь к текущему файлу  
};
#endif
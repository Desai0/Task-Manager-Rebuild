#include "json.hpp" 
#include <fstream>
#include <iostream>
#include <string>


// Used to get the file name without file format
std::string rm_file_format(std::string stri)
{

	size_t lastDot = stri.find_last_of(".");

	std::string subStri = stri.substr(0, lastDot);

	return subStri;
}


// tests if there is an inputed process name in the output.json
// searches process name with or without the format (.txt, .exe, etc.)
// Searches System by default

bool test_json_search(std::string target = "System")
{
	std::ifstream file("output.json");
	nlohmann::json jsonFile = nlohmann::json::parse(file);

	for (auto obj : jsonFile["processes"])
	{
		if (obj["name"] == target.c_str() || obj["name"] == rm_file_format(target).c_str())
		{
			return 1;
		}
	}
	return 0;
}


int main()
{
	std::cout << rm_file_format("chromeexe") << std::endl;
}
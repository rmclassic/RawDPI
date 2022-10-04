#include <vector>
#include <string>
#include <fstream>
#include <iostream>
std::vector<std::string> ExceptionsList;

void InitializeExceptionsList()
{
	std::ifstream exceptionsimporter("exceptions.txt", std::ios::in);
	std::string Tempstr;
	while (exceptionsimporter.peek() != -1)
	{
		std::getline(exceptionsimporter, Tempstr);
		ExceptionsList.push_back(Tempstr);
	}
	std::cout << ExceptionsList.size() << " Exceptions has been read from list";
	std::cout.flush();
}

bool IsException(std::string host)
{
	for (auto cur = ExceptionsList.begin(); cur < ExceptionsList.end(); cur++)
	{
		if (*cur == host)
			return true;
	}
	return false;
}

#pragma once
#include <string>
#include <list>

class DNSQuestionRecord
{
private:
	std::list<std::string> labels;
	short Type = 0x0001;
	short Class = 0x0001;
public:
	size_t Serialize(char*);
	size_t PredictSize();
	DNSQuestionRecord(std::string domain);
};

class DNSResourceRecord
{

};

class DNSQuery
{
private:
	short ID;
	short Flags;
	std::list<DNSQuestionRecord> Questions;
	std::list<DNSResourceRecord> Answers;
	std::list<DNSResourceRecord> NameServers;
	std::list<DNSResourceRecord> AdditionalRecords;
public:
	//static DNSQuery Parse();
	static DNSQuery Question(std::string domain);
	size_t Serialize(char*);
	//size_t PredictSize();
};


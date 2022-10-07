#pragma once
#include <cstddef>
#include <string>
#include <list>

class DNSLabel
{
public:
	std::list<std::string> Labels;
	static DNSLabel Parse(const unsigned char*, const unsigned char* base, size_t* consumed);
	size_t PredictSize();
	size_t Serialize(char*);
	DNSLabel(std::string);
	DNSLabel();
	DNSLabel& operator+(const DNSLabel);
};

class DNSQuestionRecord
{
public:
	DNSLabel Label;
	unsigned short Type = 0x0001;
	unsigned short Class = 0x0001;
	size_t Serialize(char*);
	size_t PredictSize();
	DNSQuestionRecord(std::string domain);
	static DNSQuestionRecord Parse(const unsigned char*, const unsigned char*, size_t*);
	DNSQuestionRecord();
};

class DNSResourceRecord
{
public:
	unsigned short Type;
	unsigned short Class;
	unsigned short TTL;
	unsigned char IP[4];
	std::string IPAddress();
	DNSLabel Label;
	static DNSResourceRecord Parse(const unsigned char*, const unsigned char*, size_t*);
};

class DNSQuery
{
public:
	unsigned short ID;
	unsigned short Flags;
	std::list<DNSQuestionRecord> Questions;
	std::list<DNSResourceRecord> Answers;
	std::list<DNSResourceRecord> NameServers;
	std::list<DNSResourceRecord> AdditionalRecords;
	static DNSQuery Question(std::string domain);
	static DNSQuery Parse(const unsigned char*);
	size_t Serialize(char*);
};


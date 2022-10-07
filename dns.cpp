#include "dns.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <locale>
#include <string>
#include <iostream>

#ifdef _WIN32
#define STRTOK strtok_s
#endif

#ifdef __unix__
#define STRTOK strtok_r
#endif

////////
// DNSQuery Implementations
////////
size_t DNSQuery::Serialize(char* buff)
{
	size_t written = 0;
	short record_count = 0;

	buff[written++] = (ID >> 8) & 0xFF;
	buff[written++] = ID & 0xFF;

	buff[written++] = (Flags >> 8) & 0xFF;
	buff[written++] = Flags & 0xFF;

	// Append QDCOUNT
	record_count = Questions.size();
	buff[written++] = (record_count >> 8) & 0xFF;
	buff[written++] = record_count & 0xFF;

	// Append ANCOUNT
	record_count = Answers.size();
	buff[written++] = (record_count >> 8) & 0xFF;
	buff[written++] = record_count & 0xFF;

	// Append NSCOUNT
	record_count = NameServers.size();
	buff[written++] = (record_count >> 8) & 0xFF;
	buff[written++] = record_count & 0xFF;

	// Append ARCOUNT
	record_count = AdditionalRecords.size();
	buff[written++] = (record_count >> 8) & 0xFF;
	buff[written++] = record_count & 0xFF;

	for (auto& question : Questions)
	{
		written += question.Serialize(buff + written);
	}

	return written;
}

DNSQuery DNSQuery::Parse(const unsigned char* buff)
{	
	size_t read = 0;
	int qdCount, anCount, nsCount, arCount;
	DNSQuery q;
	q.ID = (buff[read++] << 8);
	q.ID += buff[read++];

	q.Flags = (buff[read++] << 8);
	q.Flags += buff[read++];

	qdCount = (buff[read++] << 8);
	qdCount += buff[read++];

	anCount = (buff[read++] << 8);
	anCount += buff[read++];

	nsCount = (buff[read++] << 8);
	nsCount += buff[read++];

	arCount = (buff[read++] << 8);
	arCount += buff[read++];

	for (int i = 0; i < qdCount; i++)
	{
		auto question = DNSQuestionRecord::Parse(buff + read, buff, &read);
		q.Questions.push_back(question);
	}

	for (int i = 0; i < anCount; i++)
	{
		auto res = DNSResourceRecord::Parse(buff + read, buff, &read);
		q.Answers.push_back(res);
	}


	for (int i = 0; i < nsCount; i++)
	{
		auto res = DNSResourceRecord::Parse(buff + read, buff, &read);
		q.NameServers.push_back(res);
	}

	for (int i = 0; i < arCount; i++)
	{
		auto res = DNSResourceRecord::Parse(buff + read, buff, &read);
		q.AdditionalRecords.push_back(res);
	}

	return q;
}

DNSQuery DNSQuery::Question(std::string domain)
{
	DNSQuery query;
	DNSQuestionRecord qreq(domain);

	query.ID = 0x00;
	query.Flags = 0x0100;
	query.Questions.push_back(qreq);
	return query;
}

////////
// DNSLabel Implementations
////////
size_t DNSLabel::PredictSize()
{
	size_t total_size = 0;
	for (auto& label : Labels)
	{
		total_size += label.size();
	}

	total_size += Labels.size(); // an octet for each label length
	return total_size;
}



DNSLabel DNSLabel::Parse(const unsigned char *buff, const unsigned char* base, size_t* consumed)
{
	size_t read = 0;
	const unsigned char* addr = nullptr;
	DNSLabel l;
	int label_size = buff[read++];
	
	do 
	{
		if ((label_size & 0xc0) == 0xc0) // A domain pointer
		{ 
			int offset = (((int)label_size & 0x3f) << 8) + (int)buff[read++];
			addr = base + offset;
			DNSLabel ptr_data = DNSLabel::Parse(addr, base, nullptr);
			l = l + ptr_data;
			break;
		}
		else 
		{
			auto label = std::string((const char*)buff + read, label_size);
			label[label_size] = '\0';
			read += label_size;
			l.Labels.push_back(label);
		}

		label_size = buff[read++];
	} while (label_size != 0);

	if (consumed)
		*consumed += read;

	return l;
}

DNSLabel& DNSLabel::operator+(const DNSLabel b) 
{
	for (auto label : b.Labels)
	{
		Labels.push_back(label);
	}

	return *this;
}


DNSLabel::DNSLabel(std::string domain)
{
	char* context = NULL, s_domain[512];
	domain.copy(s_domain, domain.size());
	s_domain[domain.size()] = '\0';
	char* token = STRTOK(s_domain, ".", &context);
	while (token != nullptr)
	{
		Labels.push_back(token);
		token = STRTOK(nullptr, ".", &context);
	}
}



size_t DNSLabel::Serialize(char *buff)
{
	size_t written = 0;
	for (auto& label : Labels)
	{
		char label_size = label.size();
		buff[written++] = label_size & 0xFF;

		memcpy(buff + written, label.c_str(), label.size());
		written += label_size;
	}
	
	return written;
}

DNSLabel::DNSLabel() {}



////////
// DNSQuestionRecord Implementations
////////
size_t DNSQuestionRecord::PredictSize()
{
	return Label.PredictSize();
}

DNSQuestionRecord::DNSQuestionRecord(std::string domain)
{
	Label = DNSLabel(domain);
}

size_t DNSQuestionRecord::Serialize(char* buff)
{
	size_t written = 0;
	written += Label.Serialize(buff + written);

	buff[written++] = 0x00;

	buff[written++] = (Type >> 8) & 0xFF;
	buff[written++] = Type & 0xFF;

	buff[written++] = (Class >> 8) & 0xFF;
	buff[written++] = Class & 0xFF;
	return written;
}

DNSQuestionRecord DNSQuestionRecord::Parse(const unsigned char* buff, const unsigned char* base, size_t* read)
{
	DNSQuestionRecord r;
	r.Label = DNSLabel::Parse(buff, base, read);
	r.Type = ((int)buff[(*read)++] << 8);
	r.Type += buff[(*read)++];

	r.Class = ((int)buff[(*read)++] << 8);
	r.Class += buff[(*read)++];

	return r;
}

DNSQuestionRecord::DNSQuestionRecord() {}


////////
// DNSResourceRecord Implementations
////////
DNSResourceRecord DNSResourceRecord::Parse(const unsigned char* buff, const unsigned char* base, size_t* consumed)
{
	DNSResourceRecord rec;
	size_t read = 0;
	rec.Label = DNSLabel::Parse(buff, base, &read);
	rec.Type = (buff[read++] << 8);
	rec.Type += buff[read++];

	rec.Class = (buff[read++] << 8);
	rec.Class += buff[read++];

	rec.TTL = (buff[read++] << 24);
	rec.TTL += (buff[read++] << 16);
	rec.TTL += (buff[read++] << 8);
	rec.TTL += buff[read++];

	size_t rSize = (buff[read++] << 8);
	rSize += buff[read++];

	if (rec.Type == 1 && rec.Class == 1 && rSize == 4)
	{
		memcpy(rec.IP, buff + read, rSize);
	}

	*consumed += read + rSize;
	return rec;
}

std::string DNSResourceRecord::IPAddress()
{
	std::string ip = "";

	for (int i = 0; i < 4; i++)
	{
		ip += std::to_string(IP[i]);

		if (i != 3)
			ip += '.';
	}

	return ip;
}
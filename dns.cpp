#include "dns.h"

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

DNSQuery DNSQuery::Question(std::string domain)
{
	DNSQuery query;
	DNSQuestionRecord qreq(domain);

	query.ID = 0x00;
	query.Flags = 0x0100;
	query.Questions.push_back(qreq);
	return query;
}

size_t DNSQuestionRecord::PredictSize()
{
	size_t total_size = 0;
	for (auto& label : labels)
	{
		total_size += label.size();
	}

	total_size += labels.size(); // an octet for each label length
	return total_size;
}

DNSQuestionRecord::DNSQuestionRecord(std::string domain)
{
	char* context = NULL, s_domain[512];
	domain.copy(s_domain, domain.size());
	s_domain[domain.size()] = '\0';

	char* token = strtok_s(s_domain, ".", &context);
	while (token != nullptr)
	{
		labels.push_back(token);
		token = strtok_s(nullptr, ".", &context);
	}
}

size_t DNSQuestionRecord::Serialize(char* buff)
{
	size_t written = 0;
	for (auto& label : labels)
	{
		char label_size = label.size();
		buff[written++] = label_size & 0xFF;

		memcpy(buff + written, label.c_str(), label.size());
		written += label_size;
	}

	buff[written++] = 0x00;

	buff[written++] = (Type >> 8) & 0xFF;
	buff[written++] = Type & 0xFF;

	buff[written++] = (Class >> 8) & 0xFF;
	buff[written++] = Class & 0xFF;
	return written;
}
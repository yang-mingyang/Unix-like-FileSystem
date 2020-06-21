#pragma once
#ifndef UTILITY_H
#define UTILITY_H
#include <vector>
#include <string>
using namespace std;
class Utility {
public:
	template<class T>
	static void copy(T* from, T* to, int count)
	{
		while (count--)
			*to++ = *from++;
	}
	static void StringCopy(const char* src, char* dst)
	{
		while ((*dst++ = *src++) != 0);
	}
	static int strlen(char* pString)
	{
		int length = 0;
		char* pChar = pString;
		while (*pChar++)
			length++;
		return length;
	}
	static vector<char*> parseCmd(const string& s)
	{
		auto p = s.begin();
		auto q = s.begin();
		vector<char*> result;
		while (q!=s.end())
		{
			if (*p == ' ') p++, q++;
			else
			{
				while (q!=s.end() && *q != ' ') q++;
				int len = q - p + 1;
				char* newString = new char[len];
				for (int i = 0; i < q - p; i++) newString[i] = *(p + i);
				newString[q - p] = '\0';
				result.push_back(newString);
				p = q;
			}
		}
		return result;
	}

};

#endif





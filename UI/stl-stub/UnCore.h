// Simple UI library.
// Copyright (C) 2018 Konstantin Nosov
// Licensed under the BSD license. See LICENSE.txt file in the project root for full license information.

#ifndef __UN_CORE_H__
#define __UN_CORE_H__

#include <vector>
#include <string>

template<typename T>
struct TArray
{
private:
	std::vector<T>	items;

public:
	FORCEINLINE T* GetData()
	{
		return items.size() > 0 ? &items[0] : NULL;
	}
	FORCEINLINE const T* GetData() const
	{
		return items.size() > 0 ? &items[0] : NULL;
	}
	FORCEINLINE T& operator[](int index)
	{
		return items[index];
	}
	FORCEINLINE const T& operator[](int index) const
	{
		return items[index];
	}
	FORCEINLINE int Num() const
	{
		return (int)items.size();
	}
	inline void Empty(int Count = 0)
	{
		items.resize(0);
		if (Count)
		{
			items.reserve(Count);
		}
	}
	FORCEINLINE void Reset(int Count = 0)
	{
		Empty(Count);
	}
	FORCEINLINE int Add(const T& item)
	{
		size_t OldCount = items.size();
		items.push_back(item);
		return (int)OldCount;
	}
	inline int AddZeroed(int Count = 1)
	{
		size_t OldCount = items.size();
		items.resize(OldCount + Count);
		return (int)OldCount;
	}
	FORCEINLINE int AddUninitialized(int Count = 1)
	{
		return AddZeroed(Count);
	}
	FORCEINLINE void ResizeTo(int count)
	{
		items.resize(count);
	}
	void RemoveAt(int index, int count = 1)
	{
		items.erase(items.begin() + index, items.begin() + index + count);
	}
	FORCEINLINE void RemoveAtSwap(int index, int count = 1)
	{
		RemoveAt(index, count);
	}
	int FindItem(const T& item, int startIndex = 0) const
	{
		for (int i = startIndex; i < items.size(); i++)
		{
			if (items[i] == item)
			{
				return i;
			}
		}
		return -1;
	}
	FORCEINLINE void Sort(int (*cmpFunc)(const T*, const T*))
	{
		if (items.size() > 0)
		{
			qsort(&items[0], items.size(), sizeof(T), (int (*)(const void*, const void*)) cmpFunc);
		}
	}
};

template<typename T>
inline void* operator new(size_t size, TArray<T> &Array)
{
	assert(size == sizeof(T)); // allocating wrong object? can't disallow allocating of "int" inside "TArray<FString>" at compile time ...
	int index = Array.AddUninitialized(1);
	return Array.GetData() + index;
}

// Just shut up compiler
template<typename T>
inline void operator delete(void* ptr, TArray<T> &Array)
{
	assert(0);
}

struct FString
{
private:
	std::string str;

public:
	FString()
	{}
	FString(const char* src)
	: str(src)
	{}
	FString& operator=(const char* src)
	{
		str = src;
		return *this;
	}
	FString& operator+=(const char* text)
	{
		str += text;
		return *this;
	}
	FORCEINLINE FString& operator+=(const FString& Str)
	{
		str += *Str;
		return *this;
	}
	FORCEINLINE FString& AppendChar(char ch)
	{
		str += ch;
		return *this;
	}
	FORCEINLINE const char* operator*() const
	{
		return str.c_str();
	}
	FORCEINLINE void Empty(int count = 0)
	{
		str.resize(0);
		str.reserve(count);
	}
	FORCEINLINE bool IsEmpty() const
	{
		return str.length() == 0;
	}
	FORCEINLINE char& operator[](int index)
	{
		return str[index];
	}
	FORCEINLINE const char& operator[](int index) const
	{
		return str[index];
	}
	FORCEINLINE int Len() const
	{
		return (int)str.length();
	}
};

// No TStaticArray FStaticString

template<typename T, int N>
struct TStaticArray : public TArray<T>
{
};

template<int N>
struct FStaticString : public FString
{
	FORCEINLINE FStaticString& operator=(const char* src)
	{
		return (FStaticString&) FString::operator=(src);
	}
	FORCEINLINE FStaticString& operator=(const FString& src)
	{
		return (FStaticString&) FString::operator=(src);
	}
};

#endif // __UN_CORE_H__

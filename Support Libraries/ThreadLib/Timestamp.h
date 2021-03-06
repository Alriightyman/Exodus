#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__
#include "WindowsSupport/WindowsSupport.pkg"
#include <string>
#include <sstream>
#include <iomanip>

class Timestamp
{
public:
	// Constructors
	inline Timestamp(const SYSTEMTIME& systemTime);

	// Timestamp string functions
	inline std::wstring GetDate() const;
	inline std::wstring GetFilenameDate() const;
	inline std::wstring GetTime() const;
	inline std::wstring GetFilenameTime() const;
	inline std::wstring GetTimestamp() const;
	inline std::wstring GetFilenameTimestamp() const;

private:
	std::wstring _date;
	std::wstring _time;
};

#include "Timestamp.inl"
#endif

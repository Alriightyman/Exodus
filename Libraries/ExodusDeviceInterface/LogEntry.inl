//----------------------------------------------------------------------------------------
//Getters
//----------------------------------------------------------------------------------------
std::wstring LogEntry::GetText() const
{
	return text.str();
}

//----------------------------------------------------------------------------------------
std::wstring LogEntry::GetSource() const
{
	return source;
}

//----------------------------------------------------------------------------------------
std::wstring LogEntry::GetEventLevelString() const
{
	switch(eventLevel)
	{
	case EventLevel::Info:
		return L"1 - Information";
	case EventLevel::Debug:
		return L"2 - Debug";
	case EventLevel::Warning:
		return L"3 - Warning";
	case EventLevel::Error:
		return L"4 - Error";
	case EventLevel::Critical:
		return L"5 - Critical";
	default:
		return L"";
	}
}

//----------------------------------------------------------------------------------------
std::wstring LogEntry::GetTimeString() const
{
	std::wstringstream stream;
	stream << std::setw(2) << std::setfill(L'0') << hour << L':' << minute << L':' << second << L'.' << millisecond;
	return stream.str();
}

//----------------------------------------------------------------------------------------
//Setters
//----------------------------------------------------------------------------------------
void LogEntry::SetText(const std::wstring& atext)
{
	text.str() = L"";
	text << atext;
}

//----------------------------------------------------------------------------------------
void LogEntry::SetSource(const std::wstring& asource)
{
	source = asource;
}

//----------------------------------------------------------------------------------------
void LogEntry::SetEventLevel(EventLevel alevel)
{
	eventLevel = alevel;
}

//----------------------------------------------------------------------------------------
//Text-based stream functions
//----------------------------------------------------------------------------------------
template<class T> LogEntry& LogEntry::operator>>(T& data)
{
	text << data;
	return *this;
}

//----------------------------------------------------------------------------------------
template<class T> LogEntry& LogEntry::operator<<(const T& data)
{
	text << data;
	return *this;
}

#ifndef __IM68000_H__
#define __IM68000_H__
#include "GenericAccess/GenericAccess.pkg"
#include "Processor/Processor.pkg"
#include "MarshalSupport/MarshalSupport.pkg"
using namespace MarshalSupport::Operators;

class IM68000 :public virtual IGenericAccess
{
public:
	// Enumerations
	enum class IM68000DataSource;
	enum class Exceptions;

	// Structures
	struct RegisterDataContext;
	struct ExceptionDebuggingEntry;

	// Constants
	static const int AddressRegCount = 8;
	static const int DataRegCount = 8;
	static const unsigned int SP = 7;

public:
	// Interface version functions
	static inline unsigned int ThisIM68000Version() { return 1; }
	virtual unsigned int GetIM68000Version() const = 0;

	// CCR flags
	inline bool GetX() const;
	inline void SetX(bool data);
	inline bool GetN() const;
	inline void SetN(bool data);
	inline bool GetZ() const;
	inline void SetZ(bool data);
	inline bool GetV() const;
	inline void SetV(bool data);
	inline bool GetC() const;
	inline void SetC(bool data);

	// SR flags
	inline bool GetSR_T() const;
	inline void SetSR_T(bool data);
	inline bool GetSR_S() const;
	inline void SetSR_S(bool data);
	inline unsigned int GetSR_IPM() const;
	inline void SetSR_IPM(unsigned int data);

	// Register functions
	inline unsigned int GetPC() const;
	inline void SetPC(unsigned int data);
	inline unsigned int GetSR() const;
	inline void SetSR(unsigned int data);
	inline unsigned int GetCCR() const;
	inline void SetCCR(unsigned int data);
	inline unsigned int GetSP() const;
	inline void SetSP(unsigned int data);
	inline unsigned int GetSSP() const;
	inline void SetSSP(unsigned int data);
	inline unsigned int GetUSP() const;
	inline void SetUSP(unsigned int data);
	inline unsigned int GetA(unsigned int regiserNo) const;
	inline void SetA(unsigned int regiserNo, unsigned int data);
	inline unsigned int GetD(unsigned int regiserNo) const;
	inline void SetD(unsigned int regiserNo, unsigned int data);

	// Exception debugging functions
	virtual bool GetLogAllExceptions() const = 0;
	virtual void SetLogAllExceptions(bool state) = 0;
	virtual bool GetBreakOnAllExceptions() const = 0;
	virtual void SetBreakOnAllExceptions(bool state) = 0;
	virtual bool GetDisableAllExceptions() const = 0;
	virtual void SetDisableAllExceptions(bool state) = 0;
	virtual Marshal::Ret<std::list<ExceptionDebuggingEntry>> GetExceptionDebugEntries() const = 0;
	virtual void SetExceptionDebugEntries(const Marshal::In<std::list<ExceptionDebuggingEntry>>& state) = 0;
	virtual Marshal::Ret<std::wstring> GetExceptionName(Exceptions vectorNumber) const = 0;
	virtual void TriggerException(Exceptions vectorNumber) = 0;
};

#include "IM68000.inl"
#endif

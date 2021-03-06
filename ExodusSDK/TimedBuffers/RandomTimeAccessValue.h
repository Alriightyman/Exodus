#ifndef __RANDOMTIMEACCESSVALUE_H__
#define __RANDOMTIMEACCESSVALUE_H__
#include <mutex>
#include <list>

// Any object can be stored, saved, or loaded from this container, provided it meets the
// following requirements:
// -It is copy constructible
// -It is assignable
// -It is streamable into and from Stream::ViewText, either natively or through overloaded
// stream operators.

template<class DataType, class TimesliceType>
class RandomTimeAccessValue
{
public:
	// Structures
	struct TimesliceEntry;
	struct WriteEntry;
	struct WriteInfo;
	struct TimesliceSaveEntry;
	struct WriteSaveEntry;

	// Typedefs
	typedef typename std::list<TimesliceEntry>::iterator Timeslice;

	// Constructors
	RandomTimeAccessValue();
	RandomTimeAccessValue(const DataType& defaultValue);

	// Dereference operators
	const DataType& operator*() const;
	DataType& operator*();

	// Access functions
	DataType Read(TimesliceType readTime) const;
	void Write(TimesliceType writeTime, const DataType& data);
	DataType ReadCommitted() const;
	DataType ReadCommitted(TimesliceType readTime) const;
	void WriteCommitted(const DataType& data);
	DataType ReadLatest() const;
	void WriteLatest(const DataType& data);

	// Time management functions
	void Initialize();
	bool DoesLatestTimesliceExist() const;
	Timeslice GetLatestTimeslice();
	void AdvancePastTimeslice(const Timeslice& targetTimeslice);
	void AdvanceToTimeslice(const Timeslice& targetTimeslice);
	void AdvanceByTime(TimesliceType step, const Timeslice& targetTimeslice);
	bool AdvanceByStep(const Timeslice& targetTimeslice);
	TimesliceType GetNextWriteTime(const Timeslice& targetTimeslice);
	WriteInfo GetWriteInfo(unsigned int index, const Timeslice& targetTimeslice);
	void Commit();
	void Rollback();
	void AddTimeslice(TimesliceType timeslice);

	// Savestate functions
	bool LoadState(IHierarchicalStorageNode& node);
	bool LoadTimesliceEntries(IHierarchicalStorageNode& node, std::list<TimesliceSaveEntry>& timesliceSaveList);
	bool LoadWriteEntries(IHierarchicalStorageNode& node, std::list<WriteSaveEntry>& writeSaveList);
	bool SaveState(IHierarchicalStorageNode& node) const;

private:
	mutable std::mutex _accessLock;
	std::list<TimesliceEntry> _timesliceList;
	Timeslice _latestTimeslice;
	std::list<WriteEntry> _writeList;
	DataType _value;
	TimesliceType _currentTimeOffset;
};

#include "RandomTimeAccessValue.inl"
#endif

#include "M68000Instruction.h"
#ifndef __M68000_BSR_H__
#define __M68000_BSR_H__
namespace M68000 {

class BSR :public M68000Instruction
{
public:
	virtual BSR* Clone() const {return new BSR();}
	virtual BSR* ClonePlacement(void* buffer) const {return new(buffer) BSR();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<M68000Instruction>& table) const
	{
		return table.AllocateRegionToOpcode(this, L"01100001********", L"");
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"BSR";
	}

	virtual Disassembly M68000Disassemble(const M68000::LabelSubstitutionSettings& labelSettings) const
	{
		return Disassembly(GetOpcodeName() + L"." + DisassembleSize(_size), _source.DisassembleImmediateAsPCDisplacement(labelSettings), _source.DisassembleImmediateAsPCDisplacementTargetAddressString());
	}

	virtual void M68000Decode(M68000* cpu, const M68000Long& location, const M68000Word& data, bool transparent)
	{
//	-----------------------------------------------------------------
//	|15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
//	|---|---|---|---|---|---|---|---|-------------------------------|
//	| 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1 |         8 BITS OFFSET         |
//	|---------------------------------------------------------------|
//	|            16 BITS OFFSET, IF 8 BITS OFFSET = $00             |
//	-----------------------------------------------------------------

		// BSR	<label>
		_target.BuildAddressPredec(BITCOUNT_LONG, location + GetInstructionSize(), M68000::SP);

		if (data.GetDataSegment(0, 8) != 0)
		{
			_size = BITCOUNT_BYTE;
			_source.BuildImmediateData(location + GetInstructionSize(), M68000Byte(data.GetDataSegment(0, 8)));
		}
		else
		{
			_size = BITCOUNT_WORD;
			_source.BuildImmediateData(_size, location + GetInstructionSize(), cpu, transparent, GetInstructionRegister());
			AddInstructionSize(_source.ExtensionSize());
		}
		AddExecuteCycleCount(ExecuteTime(18, 2, 2));
	}

	virtual ExecuteTime M68000Execute(M68000* cpu, const M68000Long& location) const
	{
		double additionalTime = 0;
		Data offset(_size);
		M68000Long newPC;
		M68000Long returnAddress;

		// Perform the operation
		additionalTime += _source.Read(cpu, offset, GetInstructionRegister());
		newPC = _source.GetSavedPC() + M68000Long(offset.SignExtend(BITCOUNT_LONG));
		returnAddress = location + GetInstructionSize();
		_target.Write(cpu, returnAddress, GetInstructionRegister());
		cpu->PushCallStack(cpu->GetPC().GetData(), newPC.GetData(), returnAddress.GetData(), L"BSR");

		// Return the execution time
		cpu->SetPC(newPC);
		return GetExecuteCycleCount(additionalTime);
	}

	virtual void GetResultantPCLocations(std::set<unsigned int>& resultantPCLocations, bool& undeterminedResultantPCLocation) const
	{
		// Return the address directly after this opcode, and the _target jump location from
		// executing this opcode, as the possible resultant PC locations from executing
		// this opcode.
		undeterminedResultantPCLocation = false;
		unsigned int nextOpcodeAddress = GetInstructionLocation().GetData() + GetInstructionSize();
		unsigned int branchOpcodeAddress = (_source.GetSavedPC() + _source.ExtractProcessedImmediateData()).GetData();
		resultantPCLocations.insert(nextOpcodeAddress);
		resultantPCLocations.insert(branchOpcodeAddress);
	}

	virtual void GetLabelTargetLocations(std::set<unsigned int>& labelTargetLocations) const
	{
		unsigned int branchOpcodeAddress = (_source.GetSavedPC() + _source.ExtractProcessedImmediateData()).GetData();
		labelTargetLocations.insert(branchOpcodeAddress);
	}

private:
	Bitcount _size;
	EffectiveAddress _source;
	EffectiveAddress _target;
};

} // Close namespace M68000
#endif

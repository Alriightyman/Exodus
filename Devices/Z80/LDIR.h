#include "Z80Instruction.h"
#ifndef __Z80_LDIR_H__
#define __Z80_LDIR_H__
namespace Z80 {

class LDIR :public Z80Instruction
{
public:
	virtual LDIR* Clone() const {return new LDIR();}
	virtual LDIR* ClonePlacement(void* buffer) const {return new(buffer) LDIR();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<Z80Instruction>& table) const
	{
		return table.AllocateRegionToOpcode(this, L"10110000", L"");
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"LDIR";
	}

	virtual Disassembly Z80Disassemble(const Z80::LabelSubstitutionSettings& labelSettings) const
	{
		return Disassembly(GetOpcodeName(), L"");
	}

	virtual void Z80Decode(Z80* cpu, const Z80Word& location, const Z80Byte& data, bool transparent)
	{
		_source.SetIndexState(GetIndexState(), GetIndexOffset());
		_target.SetIndexState(GetIndexState(), GetIndexOffset());

		// LDIR		11101101 10110000
		_source.SetMode(EffectiveAddress::Mode::HLPostInc);
		_target.SetMode(EffectiveAddress::Mode::DEPostInc);
		AddExecuteCycleCount(12);

		AddInstructionSize(GetIndexOffsetSize(_source.UsesIndexOffset() || _target.UsesIndexOffset()));
		AddInstructionSize(_source.ExtensionSize());
		AddInstructionSize(_target.ExtensionSize());
	}

	virtual ExecuteTime Z80Execute(Z80* cpu, const Z80Word& location) const
	{
		double additionalTime = 0;
		Z80Byte result;
		ExecuteTime additionalCycles;

		// Perform the operation
		additionalTime += _source.Read(cpu, location, result);
		additionalTime += _target.Write(cpu, location, result);
		cpu->SetBC(cpu->GetBC() - 1);

		// Set the flag results
		cpu->SetFlagH(false);
		cpu->SetFlagPV(cpu->GetBC() != 0);
		cpu->SetFlagN(false);

		Z80Byte currentRegA;
		cpu->GetA(currentRegA);
		Z80Byte regAResult = currentRegA + result;
		cpu->SetFlagY(regAResult.GetBit(1));
		cpu->SetFlagX(regAResult.GetBit(3));

		// Adjust the PC and return the execution time
		if (cpu->GetBC() == 0)
		{
			cpu->SetPC(location + GetInstructionSize());
		}
		else
		{
			additionalCycles.cycles = 5;
		}
		return GetExecuteCycleCount(additionalTime) + additionalCycles;
	}

private:
	EffectiveAddress _source;
	EffectiveAddress _target;
};

} // Close namespace Z80
#endif

#include "Z80Instruction.h"
#ifndef __Z80_CP8_H__
#define __Z80_CP8_H__
namespace Z80 {

class CP8 :public Z80Instruction
{
public:
	virtual CP8* Clone() const {return new CP8();}
	virtual CP8* ClonePlacement(void* buffer) const {return new(buffer) CP8();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<Z80Instruction>& table) const
	{
		bool result = true;
		result &= table.AllocateRegionToOpcode(this, L"10111***", L"");
		result &= table.AllocateRegionToOpcode(this, L"11111110", L"");
		return result;
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"CP";
	}

	virtual Disassembly Z80Disassemble(const Z80::LabelSubstitutionSettings& labelSettings) const
	{
		return Disassembly(GetOpcodeName(), _target.Disassemble() + L", " + _source.Disassemble());
	}

	virtual void Z80Decode(Z80* cpu, const Z80Word& location, const Z80Byte& data, bool transparent)
	{
		_source.SetIndexState(GetIndexState(), GetIndexOffset());
		_target.SetIndexState(GetIndexState(), GetIndexOffset());
		_target.SetMode(EffectiveAddress::Mode::A);

		if (_source.Decode8BitRegister(data.GetDataSegment(0, 3)))
		{
			// CP A,r		10111rrr
			AddExecuteCycleCount(4);
		}
		else if (data.GetBit(6))
		{
			// CP A,n		11111110
			_source.BuildImmediateData(BITCOUNT_BYTE, location + GetInstructionSize(), cpu, transparent);
			AddExecuteCycleCount(7);
		}
		else
		{
			// CP A,(HL)		10111110
			// CP A,(IX + d)	11011101 10111110 dddddddd
			// CP A,(IY + d)	11111101 10111110 dddddddd
			_source.SetMode(EffectiveAddress::Mode::HLIndirect);
			if (GetIndexState() == EffectiveAddress::IndexState::None)
			{
				AddExecuteCycleCount(7);
			}
			else
			{
				AddExecuteCycleCount(15);
			}
		}

		AddInstructionSize(GetIndexOffsetSize(_source.UsesIndexOffset() || _target.UsesIndexOffset()));
		AddInstructionSize(_source.ExtensionSize());
		AddInstructionSize(_target.ExtensionSize());
	}

	virtual ExecuteTime Z80Execute(Z80* cpu, const Z80Word& location) const
	{
		double additionalTime = 0;
		Z80Byte op1;
		Z80Byte op2;
		Z80Byte result;

		// Perform the operation
		additionalTime += _source.Read(cpu, location, op1);
		additionalTime += _target.Read(cpu, location, op2);
		result = op2 - op1;

		// Set the flag results
		cpu->SetFlagS(result.Negative());
		cpu->SetFlagZ(result.Zero());
		cpu->SetFlagY(op1.GetBit(5));
		cpu->SetFlagH(op1.GetDataSegment(0, 4) > op2.GetDataSegment(0, 4));
		cpu->SetFlagX(op1.GetBit(3));
		cpu->SetFlagPV((op1.MSB() != op2.MSB()) && (result.MSB() != op2.MSB()));
		cpu->SetFlagN(true);
		cpu->SetFlagC(op1.GetData() > op2.GetData());

		// Adjust the PC and return the execution time
		cpu->SetPC(location + GetInstructionSize());
		return GetExecuteCycleCount(additionalTime);
	}

private:
	EffectiveAddress _source;
	EffectiveAddress _target;
};

} // Close namespace Z80
#endif

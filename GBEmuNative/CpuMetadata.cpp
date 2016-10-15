#include "CpuMetadata.h"

#include "Utils.h"

#include <string>
#include <vector>
#include <array>

namespace CpuMetadata
{
	using OpcodeMetadataArray = std::array<OpcodeMetadata, 0x100>;
	OpcodeMetadataArray s_opcodeMetadata;
	OpcodeMetadataArray s_extendedOpcodeMetadata;

	// The following functions were preprocessed using a spreadsheet from http://imrannazar.com/Gameboy-Z80-Opcode-Map and http://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
	const char* GetOpcodeMnemonic(Uint8 opcode)
	{
		//@TODO: change n/nn to something clearer to assist disassembly and simplify replacement logic
		static const char* opcodeMnemonics[256] =
		{
			"NOP", "LD BC,nn", "LD (BC),A", "INC BC", "INC B", "DEC B", "LD B,n", "RLC A", "LD (nn),SP", "ADD HL,BC", "LD A,(BC)", "DEC BC", "INC C", "DEC C", "LD C,n", "RRC A",
			"STOP", "LD DE,nn", "LD (DE),A", "INC DE", "INC D", "DEC D", "LD D,n", "RL A", "JR n", "ADD HL,DE", "LD A,(DE)", "DEC DE", "INC E", "DEC E", "LD E,n", "RR A",
			"JR NZ,n", "LD HL,nn", "LDI (HL),A", "INC HL", "INC H", "DEC H", "LD H,n", "DAA", "JR Z,n", "ADD HL,HL", "LDI A,(HL)", "DEC HL", "INC L", "DEC L", "LD L,n", "CPL",
			"JR NC,n", "LD SP,nn", "LDD (HL),A", "INC SP", "INC (HL)", "DEC (HL)", "LD (HL),n", "SCF", "JR C,n", "ADD HL,SP", "LDD A,(HL)", "DEC SP", "INC A", "DEC A", "LD A,n", "CCF",
			"LD B,B", "LD B,C", "LD B,D", "LD B,E", "LD B,H", "LD B,L", "LD B,(HL)", "LD B,A", "LD C,B", "LD C,C", "LD C,D", "LD C,E", "LD C,H", "LD C,L", "LD C,(HL)", "LD C,A",
			"LD D,B", "LD D,C", "LD D,D", "LD D,E", "LD D,H", "LD D,L", "LD D,(HL)", "LD D,A", "LD E,B", "LD E,C", "LD E,D", "LD E,E", "LD E,H", "LD E,L", "LD E,(HL)", "LD E,A",
			"LD H,B", "LD H,C", "LD H,D", "LD H,E", "LD H,H", "LD H,L", "LD H,(HL)", "LD H,A", "LD L,B", "LD L,C", "LD L,D", "LD L,E", "LD L,H", "LD L,L", "LD L,(HL)", "LD L,A",
			"LD (HL),B", "LD (HL),C", "LD (HL),D", "LD (HL),E", "LD (HL),H", "LD (HL),L", "HALT", "LD (HL),A", "LD A,B", "LD A,C", "LD A,D", "LD A,E", "LD A,H", "LD A,L", "LD A,(HL)", "LD A,A",
			"ADD A,B", "ADD A,C", "ADD A,D", "ADD A,E", "ADD A,H", "ADD A,L", "ADD A,(HL)", "ADD A,A", "ADC A,B", "ADC A,C", "ADC A,D", "ADC A,E", "ADC A,H", "ADC A,L", "ADC A,(HL)", "ADC A,A",
			"SUB A,B", "SUB A,C", "SUB A,D", "SUB A,E", "SUB A,H", "SUB A,L", "SUB A,(HL)", "SUB A,A", "SBC A,B", "SBC A,C", "SBC A,D", "SBC A,E", "SBC A,H", "SBC A,L", "SBC A,(HL)", "SBC A,A",
			"AND B", "AND C", "AND D", "AND E", "AND H", "AND L", "AND (HL)", "AND A", "XOR B", "XOR C", "XOR D", "XOR E", "XOR H", "XOR L", "XOR (HL)", "XOR A",
			"OR B", "OR C", "OR D", "OR E", "OR H", "OR L", "OR (HL)", "OR A", "CP B", "CP C", "CP D", "CP E", "CP H", "CP L", "CP (HL)", "CP A",
			"RET NZ", "POP BC", "JP NZ,nn", "JP nn", "CALL NZ,nn", "PUSH BC", "ADD A,n", "RST 0", "RET Z", "RET", "JP Z,nn", "EXT", "CALL Z,nn", "CALL nn", "ADC A,n", "RST 8",
			"RET NC", "POP DE", "JP NC,nn", "XX", "CALL NC,nn", "PUSH DE", "SUB A,n", "RST 10", "RET C", "RETI", "JP C,nn", "XX", "CALL C,nn", "XX", "SBC A,n", "RST 18",
			"LDH (n),A", "POP HL", "LDH (C),A", "XX", "XX", "PUSH HL", "AND n", "RST 20", "ADD SP,n", "JP (HL)", "LD (nn),A", "XX", "XX", "XX", "XOR n", "RST 28",
			"LDH A,(n)", "POP AF", "LDH A,(C)", "DI", "XX", "PUSH AF", "OR n", "RST 30", "LDHL SP,n", "LD SP,HL", "LD A,(nn)", "EI", "XX", "XX", "CP n", "RST 38",
		};

		return opcodeMnemonics[opcode];
	}

	const char* GetExtendedOpcodeMnemonic(Uint8 opcode)
	{
		static const char* extOpsMnemonics[256] =
		{
			"RLC B", "RLC C", "RLC D", "RLC E", "RLC H", "RLC L", "RLC (HL)", "RLC A", "RRC B", "RRC C", "RRC D", "RRC E", "RRC H", "RRC L", "RRC (HL)", "RRC A",
			"RL B", "RL C", "RL D", "RL E", "RL H", "RL L", "RL (HL)", "RL A", "RR B", "RR C", "RR D", "RR E", "RR H", "RR L", "RR (HL)", "RR A",
			"SLA B", "SLA C", "SLA D", "SLA E", "SLA H", "SLA L", "SLA (HL)", "SLA A", "SRA B", "SRA C", "SRA D", "SRA E", "SRA H", "SRA L", "SRA (HL)", "SRA A",
			"SWAP B", "SWAP C", "SWAP D", "SWAP E", "SWAP H", "SWAP L", "SWAP (HL)", "SWAP A", "SRL B", "SRL C", "SRL D", "SRL E", "SRL H", "SRL L", "SRL (HL)", "SRL A",
			"BIT 0,B", "BIT 0,C", "BIT 0,D", "BIT 0,E", "BIT 0,H", "BIT 0,L", "BIT 0,(HL)", "BIT 0,A", "BIT 1,B", "BIT 1,C", "BIT 1,D", "BIT 1,E", "BIT 1,H", "BIT 1,L", "BIT 1,(HL)", "BIT 1,A",
			"BIT 2,B", "BIT 2,C", "BIT 2,D", "BIT 2,E", "BIT 2,H", "BIT 2,L", "BIT 2,(HL)", "BIT 2,A", "BIT 3,B", "BIT 3,C", "BIT 3,D", "BIT 3,E", "BIT 3,H", "BIT 3,L", "BIT 3,(HL)", "BIT 3,A",
			"BIT 4,B", "BIT 4,C", "BIT 4,D", "BIT 4,E", "BIT 4,H", "BIT 4,L", "BIT 4,(HL)", "BIT 4,A", "BIT 5,B", "BIT 5,C", "BIT 5,D", "BIT 5,E", "BIT 5,H", "BIT 5,L", "BIT 5,(HL)", "BIT 5,A",
			"BIT 6,B", "BIT 6,C", "BIT 6,D", "BIT 6,E", "BIT 6,H", "BIT 6,L", "BIT 6,(HL)", "BIT 6,A", "BIT 7,B", "BIT 7,C", "BIT 7,D", "BIT 7,E", "BIT 7,H", "BIT 7,L", "BIT 7,(HL)", "BIT 7,A",
			"RES 0,B", "RES 0,C", "RES 0,D", "RES 0,E", "RES 0,H", "RES 0,L", "RES 0,(HL)", "RES 0,A", "RES 1,B", "RES 1,C", "RES 1,D", "RES 1,E", "RES 1,H", "RES 1,L", "RES 1,(HL)", "RES 1,A",
			"RES 2,B", "RES 2,C", "RES 2,D", "RES 2,E", "RES 2,H", "RES 2,L", "RES 2,(HL)", "RES 2,A", "RES 3,B", "RES 3,C", "RES 3,D", "RES 3,E", "RES 3,H", "RES 3,L", "RES 3,(HL)", "RES 3,A",
			"RES 4,B", "RES 4,C", "RES 4,D", "RES 4,E", "RES 4,H", "RES 4,L", "RES 4,(HL)", "RES 4,A", "RES 5,B", "RES 5,C", "RES 5,D", "RES 5,E", "RES 5,H", "RES 5,L", "RES 5,(HL)", "RES 5,A",
			"RES 6,B", "RES 6,C", "RES 6,D", "RES 6,E", "RES 6,H", "RES 6,L", "RES 6,(HL)", "RES 6,A", "RES 7,B", "RES 7,C", "RES 7,D", "RES 7,E", "RES 7,H", "RES 7,L", "RES 7,(HL)", "RES 7,A",
			"SET 0,B", "SET 0,C", "SET 0,D", "SET 0,E", "SET 0,H", "SET 0,L", "SET 0,(HL)", "SET 0,A", "SET 1,B", "SET 1,C", "SET 1,D", "SET 1,E", "SET 1,H", "SET 1,L", "SET 1,(HL)", "SET 1,A",
			"SET 2,B", "SET 2,C", "SET 2,D", "SET 2,E", "SET 2,H", "SET 2,L", "SET 2,(HL)", "SET 2,A", "SET 3,B", "SET 3,C", "SET 3,D", "SET 3,E", "SET 3,H", "SET 3,L", "SET 3,(HL)", "SET 3,A",
			"SET 4,B", "SET 4,C", "SET 4,D", "SET 4,E", "SET 4,H", "SET 4,L", "SET 4,(HL)", "SET 4,A", "SET 5,B", "SET 5,C", "SET 5,D", "SET 5,E", "SET 5,H", "SET 5,L", "SET 5,(HL)", "SET 5,A",
			"SET 6,B", "SET 6,C", "SET 6,D", "SET 6,E", "SET 6,H", "SET 6,L", "SET 6,(HL)", "SET 6,A", "SET 7,B", "SET 7,C", "SET 7,D", "SET 7,E", "SET 7,H", "SET 7,L", "SET 7,(HL)", "SET 7,A",
		};

		return extOpsMnemonics[opcode];
	}

	template <typename FuncType>
	static void ForAllOpcodes(const FuncType& func)
	{
		for (auto op : s_opcodeMetadata) func(op);
		for (auto op : s_extendedOpcodeMetadata) func(op);
	}

	//int GetOpcodeSize(Uint8 opcode)
	//{
	//	// Parsed from http://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html - some are wrong it seems
	//	//static const Uint8 opcodeSizes[256] =
	//	//{
	//	//	1,	3,	1,	1,	1,	1,	2,	1,	3,	1,	1,	1,	1,	1,	2,	1,
	//	//	2,	3,	1,	1,	1,	1,	2,	1,	2,	1,	1,	1,	1,	1,	2,	1,
	//	//	2,	3,	1,	1,	1,	1,	2,	1,	2,	1,	1,	1,	1,	1,	2,	1,
	//	//	2,	3,	1,	1,	1,	1,	2,	1,	2,	1,	1,	1,	1,	1,	2,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//	//	1,	1,	3,	3,	3,	1,	2,	1,	1,	1,	3,	1,	3,	3,	2,	1,
	//	//	1,	1,	3,	0,	3,	1,	2,	1,	1,	1,	3,	0,	3,	0,	2,	1,
	//	//	2,	1,	2,	0,	0,	1,	2,	1,	2,	1,	3,	0,	0,	0,	2,	1,
	//	//	2,	1,	2,	1,	0,	1,	2,	1,	2,	1,	3,	1,	0,	0,	2,	1,
	//	//};
	//	static const Uint8 opcodeSizes[256] =
	//	{
	//		1,	3,	1,	1,	1,	1,	2,	1,	3,	1,	1,	1,	1,	1,	2,	1,
	//		1,	3,	1,	1,	1,	1,	2,	1,	2,	1,	1,	1,	1,	1,	2,	1,
	//		2,	3,	1,	1,	1,	1,	2,	1,	2,	1,	1,	1,	1,	1,	2,	1,
	//		2,	3,	1,	1,	1,	1,	2,	1,	2,	1,	1,	1,	1,	1,	2,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,
	//		1,	1,	3,	3,	3,	1,	2,	1,	1,	1,	3,	1,	3,	3,	2,	1,
	//		1,	1,	3,	1,	3,	1,	2,	1,	1,	1,	3,	1,	3,	1,	2,	1,
	//		2,	1,	1,	1,	1,	1,	2,	1,	2,	1,	3,	1,	1,	1,	2,	1,
	//		2,	1,	1,	1,	1,	1,	2,	1,	2,	1,	3,	1,	1,	1,	2,	1,
	//	};
	//	return opcodeSizes[opcode];
	//}

	static int GetComputedOpcodeSizeInternal(Uint8 opcode, const OpcodeMetadataArray& metadata)
	{
		auto size = 1;
		auto parseOperand = [&size](const std::string& operand)
		{
			if ((operand == "nn") || (operand == "(nn)"))
			{
				size += 2;
			}
			else if ((operand == "n") || (operand == "(n)"))
			{
				size += 1;
			}
		};
		//std::for_each(begin(s_opcodeMetadata[opcode].inputs), end(s_opcodeMetadata[opcode].inputs), parseOperand);
		//std::for_each(s_opcodeMetadata[opcode].outputs, parseOperand);

		for (const auto& input : metadata[opcode].inputs)
		{
			parseOperand(input);
		}
		for (const auto& output : metadata[opcode].outputs)
		{
			parseOperand(output);
		}

		return size;
	}

	Uint8 GetOpcodeSize(Uint8 opcode)
	{
		return GetComputedOpcodeSizeInternal(opcode, s_opcodeMetadata);
	}

	Uint8 GetExtendedOpcodeSize(Uint8 opcode)
	{
		return GetComputedOpcodeSizeInternal(opcode, s_extendedOpcodeMetadata) + 1;
	}

	//int GetExtendedOpcodeSize(Uint8 opcode)
	//{
	//	static const Uint8 extOpsSizes[256] =
	//	{
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//		2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,	2,
	//	};
	//	return extOpsSizes[opcode];
	//}

	//std::string GetBaseMnemonic(const std::string& mnemonic)
	//{
	//	auto space = mnemonic.find_first_of(' ');
	//	if (space != mnemonic.end())
	//	{
	//		return 
	//	}
	//}

	bool IsExtensionOpcode(Uint8 opcode)
	{
		return opcode == 0xCB;
	}

	void ParseMnemonic(const char* fullMnemonic, OpcodeMetadata& meta)
	{
		meta.fullMnemonic = fullMnemonic;
		std::string* pToken = &meta.baseMnemonic;
		while (*fullMnemonic)
		{
			auto c = *fullMnemonic;
			if (c == ' ')
			{
				SDL_assert((meta.baseMnemonic.length() > 0) && "Unexpected spaces before token");

				if (meta.HasDirectOutput())
				{
					SDL_assert(false && "Unexpected opcode argument");
					break;
				}

				// New output
				pToken = &meta.directOutput;
			}
			else if (c == ',')
			{
				if (meta.HasDirectInput())
				{
					SDL_assert(false && "Unexpected opcode argument");
					break;
				}
				
				// New input
				pToken = &meta.directInput;
			}
			else
			{
				pToken->push_back(c);
			}

			++fullMnemonic;
		}

		if (meta.HasDirectInput())
		{
			meta.inputs.push_back(meta.directInput);
		}

		if (meta.HasDirectOutput())
		{
			meta.outputs.push_back(meta.directOutput);
		}

		// Process special cases
		// @TODO: many special cases here. Many instructions have implicit operands (e.g. A in the CP instructions, inout semantics for LDI/LDH, etc.)
		// We need to see how far we want/can go with analysis here.
		if (meta.baseMnemonic == "XX")
		{
			meta.illegal = true;
		}
	}

	void ComputeMetadata()
	{
		for (Uint16 opcode16 = 0; opcode16 <= 0xFF; ++opcode16)
		{
			Uint8 opcode = static_cast<Uint8>(opcode16);
			//if (IsExtensionOpcode(opcode))
			//{
			//	continue;
			//}

			auto& meta = s_opcodeMetadata[opcode];
			meta.opcode = opcode;
			meta.isExtendedOpcode = false;
			ParseMnemonic(GetOpcodeMnemonic(opcode), meta);
			meta.size = GetOpcodeSize(opcode);
			//if (meta.size != GetComputedOpcodeSize(opcode))
			//{
			//	SDL_assert(false && "Miscomputed opcode size");
			//}
		}

		for (Uint16 opcode16 = 0; opcode16 <= 0xFF; ++opcode16)
		{
			Uint8 opcode = static_cast<Uint8>(opcode16);
			auto& meta = s_extendedOpcodeMetadata[opcode];
			meta.opcode = opcode;
			meta.isExtendedOpcode = true;
			ParseMnemonic(GetExtendedOpcodeMnemonic(opcode), meta);
			meta.size = GetExtendedOpcodeSize(opcode);
			//if (meta.size != GetComputedExtendedOpcodeSize(opcode))
			//{
			//	SDL_assert(false && "Miscomputed opcode size");
			//}
		}

		ForAllOpcodes(
			[](const OpcodeMetadata& op)
		{
			DebugPrint("%3d (%d): %s\n", op.opcode, op.isExtendedOpcode, op.fullMnemonic.c_str());
			DebugPrint("         %s", op.baseMnemonic.c_str());
			if (op.HasDirectOutput()) DebugPrint(" %s", op.directOutput.c_str());
			if (op.HasDirectInput()) DebugPrint(",%s", op.directInput.c_str());
			DebugPrint("\n");
		});
	}

	struct Initializer
	{
		Initializer()
		{
			ComputeMetadata();
		}
	};
	Initializer g_initializer;

	const OpcodeMetadata & GetOpcodeMetadata(Uint8 byte1, Uint8 byte2)
	{
		return CpuMetadata::IsExtensionOpcode(byte1) ? s_extendedOpcodeMetadata[byte2] : s_opcodeMetadata[byte1];
	}
}

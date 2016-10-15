#pragma once

#include "SDL.h"

#include <string>
#include <vector>

namespace CpuMetadata
{
	struct OpcodeMetadata
	{
		OpcodeMetadata()
			: size(0)
			, illegal(false)
		{
		}

		Uint8 opcode; // Will be the byte after the CB for extended opcodes
		bool isExtendedOpcode;

		std::string fullMnemonic;
		std::string baseMnemonic;
		// Direct inputs and outputs are the operands specified to the opcode by the programmer. There can be at most one of each.
		// For example, in LDI (HL),A - (HL) is the output and A is the input.
		std::string directInput;
		std::string directOutput;
		// Inputs and outputs should (eventually) reflect everything that's read and written by an instruction. For example,
		// in LDI(HL),A - (HL) is the output, but so is HL since it is incremented; HL and A are inputs. (Flags may eventually be added to this information.)
		std::vector<std::string> inputs;
		std::vector<std::string> outputs;
		// Uint8 cycles; // this is conditional
		Uint8 size;
		bool illegal;

		bool HasDirectInput() const { return directInput.length() > 0; }
		bool HasDirectOutput() const { return directOutput.length() > 0; }
	};

	const OpcodeMetadata& GetOpcodeMetadata(Uint8 byte1, Uint8 byte2);
}

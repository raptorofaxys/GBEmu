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
		std::string directInput;
		std::string directOutput;
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

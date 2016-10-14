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
		std::vector<std::string> inputs;
		std::vector<std::string> outputs;
		// Uint8 cycles; // this is conditional
		Uint8 size;
		bool illegal;
	};

	const OpcodeMetadata& GetOpcodeMetadata(Uint8 byte1, Uint8 byte2);
}

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

		std::string baseMnemonic;
		std::vector<std::string> inputs;
		std::vector<std::string> outputs;
		// Uint8 cycles; // this is conditional
		Uint8 size;
		bool illegal;
	};

	//bool IsExtendedOpcode(Uint8 opcode);
	//Uint8 GetOpcodeSize(Uint8 opcode);
	//Uint8 GetExtendedOpcodeSize(Uint8 opcode);
	//Uint8 GetOpcodeSize(Uint8 byte1, Uint8 byte2);
	const OpcodeMetadata& GetOpcodeMetadata(Uint8 byte1, Uint8 byte2);
}

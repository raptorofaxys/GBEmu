#pragma once

#include "MemoryMapper.h"
#include "Rom.h"
#include "Utils.h"

class Mbc1Mapper : public MemoryMapper
{
public:
	enum class BankingMode
	{
		RomBanking = 0x00,
		RamBanking = 0x01
	};

	Mbc1Mapper(const std::shared_ptr<Rom>& rom)
		: m_pRom(rom)
		, m_pRomBytes(rom->GetRom())
	{
		Reset();
	}

	virtual void Reset()
	{
		memset(m_externalRam, 0, sizeof(m_externalRam));
		m_bankingMode = BankingMode::RomBanking;
		m_romBankLower5Bits = 0;
		m_romRam2Bits = 0;
	}

	virtual Uint8 GetActiveBank() { return GetEffectiveRomBankIndex(); }

	static const int kRomFixedBankBase = 0x0000;
	static const int kRomFixedBankSize = 0x4000;
	static const int kRomSwitchedBankBase = 0x4000;
	static const int kRomSwitchedBankSize = 0x8000 - kRomSwitchedBankBase;

	static const int kRamBankBase = 0xA000;
	static const int kRamBankSize = 0xC000 - kRamBankBase;
	static const int kExternalRamSize = kRamBankSize * 4;

	static const int kRamEnableBase = 0x0000;
	static const int kRamEnableSize = 0x2000;
	static const int kRomBankNumberBase = 0x2000;
	static const int kRomBankNumberSize = 0x4000 - kRomBankNumberBase;
	static const int kRomRamBase = 0x4000;
	static const int kRomRamSize = 0x6000 - kRomRamBase;
	static const int kBankingModeBase = 0x6000;
	static const int kBankingModeSize = 0x8000 - kBankingModeBase;
	
	virtual bool HandleRequest(MemoryRequestType requestType, Uint16 address, Uint8& value)
	{
		if (requestType == MemoryRequestType::Read)
		{
			if (IsAddressInRange(address, kRomFixedBankBase, kRomFixedBankSize))
			{
				value = m_pRomBytes[address - kRomFixedBankBase];
				return true;
			}
			else if (IsAddressInRange(address, kRomSwitchedBankBase, kRomSwitchedBankSize))
			{
				auto offset = address - kRomSwitchedBankBase;
				auto baseAddress = GetEffectiveRomBankIndex() * kRomSwitchedBankSize;
				value = m_pRomBytes[baseAddress + offset];
				return true;
			}
			else if (IsAddressInRange(address, kRamBankBase, kRamBankSize))
			{
				auto offset = address - kRamBankBase;
				auto baseAddress = GetEffectiveRamBankIndex() * kRamBankSize;
				value = m_externalRam[baseAddress + offset];
				return true;
			}
		}
		else
		{
			if (IsAddressInRange(address, kRamEnableBase, kRamEnableSize))
			{
				//@TODO: handle enabling and disabling of RAM - possibly save to disk when disabled?
				return true;
			}
			else if (IsAddressInRange(address, kRomBankNumberBase, kRomBankNumberSize))
			{
				m_romBankLower5Bits = value & 0x1F;
				return true;
			}
			else if (IsAddressInRange(address, kRomRamBase, kRomRamSize))
			{
				m_romRam2Bits = value & 0x03;
				return true;
			}
			else if (IsAddressInRange(address, kBankingModeBase, kBankingModeSize))
			{
				m_bankingMode = static_cast<BankingMode>(value);
				switch (m_bankingMode)
				{
				case BankingMode::RomBanking:
				case BankingMode::RamBanking:
					break;
				default:
					throw Exception("Unsupported MBC1 RAM/ROM banking mode: %d", value);
					break;
				}
				return true;
			}
		}


		return false;
	}

private:
	Uint8 GetEffectiveRomBankIndex()
	{
		//@OPTIMIZE: this could be baked when values are written to registers instead of recomputed every time
		Uint8 index = m_romBankLower5Bits;
		if (m_bankingMode == BankingMode::RomBanking)
		{
			index |= (m_romRam2Bits << 5);
		}

		switch (index)
		{
		case 0x00:
		case 0x20:
		case 0x40:
		case 0x60:
			index |= 1;
			break;
		}
		
		return index;
	}

	int GetEffectiveRamBankIndex()
	{
		//@OPTIMIZE: this could be baked when values are written to registers instead of recomputed every time
		Uint8 index = 0;
		if (m_bankingMode == BankingMode::RamBanking)
		{
			index |= m_romRam2Bits;
		}

		return index;
	}

	std::shared_ptr<Rom> m_pRom;
	const std::vector<Uint8>& m_pRomBytes;
	Uint8 m_externalRam[kExternalRamSize];
	BankingMode m_bankingMode;
	int m_romBankLower5Bits;
	int m_romRam2Bits; // this register truly defies proper naming
};
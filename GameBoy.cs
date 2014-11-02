using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GBEmu
{
    class GameBoy
    {
        private byte[] m_rom;
        private Memory m_memory;
        private CPU m_cpu;

        private void LoadRom(string path)
        {
            m_rom = File.ReadAllBytes(path);
            m_memory = new Memory(m_rom);
            m_cpu = new CPU(m_memory);
        }

        public GameBoy(string romPath)
        {
            LoadRom(romPath);
            Trace.Assert(m_rom != null);
        }

        public string GetRomName()
        {
            var sb = new StringBuilder();
            for (int i = 0x134; i <= 0x143; ++i)
            {
                var c = Convert.ToChar(m_rom[i]);
                if (c != 0)
                {
                    sb.Append(c);
                }
            }
            return sb.ToString();
        }
    }
}

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GBEmu
{
    class Memory
    {
        private byte[] m_rom;

        public Memory(byte[] rom)
        {
            m_rom = rom;
            Trace.Assert(m_rom != null);

            // Verify that the size of the ROM is a power of two
            Trace.Assert((m_rom.Length & (m_rom.Length - 1)) == 0);
        }
    }
}

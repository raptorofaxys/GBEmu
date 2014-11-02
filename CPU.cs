using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GBEmu
{
    class CPU
    {
        enum Operand
        {
            A,
            F,
            B,
            C,
            D,
            E,
            H,
            L,
            AF,
            BC,
            DE,
            HL,
            SP,
            IndirectHL,
            Imm8,
            Imm16,
            IndirectImm16,
        }

        enum Instruction
        {
            LD
        }

        class OpcodeMapEntry
        {
            public byte Opcode;
            public Instruction Instruction;
            public Operand Operand1;
            public Operand Operand2;
            public int Cycles;
        }

        static private Dictionary<byte, OpcodeMapEntry> s_opcodeMap = new Dictionary<byte,OpcodeMapEntry>();

        private byte A;
        private byte F;
        private byte B;
        private byte C;
        private byte D;
        private byte E;
        private byte H;
        private byte L;
        private ushort SP;
        private ushort PC;

        private short AF
        {
            get
            {
                return (short)((A << 8) | F);
            }
        }

        private short BC
        {
            get
            {
                return (short)((B << 8) | C);
            }
        }

        private short DE
        {
            get
            {
                return (short)((D << 8) | E);
            }
        }

        private short HL
        {
            get
            {
                return (short)((H << 8) | L);
            }
        }

        private bool ZeroFlag
        {
            get
            {
                return GetFlag(7);
            }
            set
            {
                SetFlag(7, value);
            }
        }
        
        private bool SubtractFlag
        {
            get
            {
                return GetFlag(6);
            }
            set
            {
                SetFlag(6, value);
            }
        }

        private bool HalfCarryFlag
        {
            get
            {
                return GetFlag(5);
            }
            set
            {
                SetFlag(5, value);
            }
        }
        private bool CarryFlag
        {
            get
            {
                return GetFlag(4);
            }
            set
            {
                SetFlag(4, value);
            }
        }

        private void SetFlag(int bitPosition, bool value)
        {
            F = (byte)((F & ~(1 << bitPosition)) | ((value ? 1 : 0) << bitPosition));
        }

        private bool GetFlag(int bitPosition)
        {
            return (F & (1 << bitPosition)) > 0;
        }

        private void Reset()
        {
            PC = 0x0100;
            SP = 0xFFFE;
        }

        static void RegisterOpcode(byte opcode, Instruction instruction, Operand o1, Operand o2, int cycles)
        {
            s_opcodeMap[opcode] = new OpcodeMapEntry { Opcode = opcode, Instruction = instruction, Operand1 = o1, Operand2 = o2, Cycles = cycles };
        }

        static CPU()
        {
            RegisterOpcode(0x06, Instruction.LD, Operand.B, Operand.Imm8, 8);
        }

        public CPU(Memory memory)
        {
            A = 32;
            F = 32;
            var af = AF;
        }
    }
}

DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF0F, IF)	// Interrupt flag
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF12, NR12)	// Sound: channel 1 volume envelope
DEFINE_MEMORY_MAPPED_REGISTER_W (0xFF13, NR13)	// Sound: channel 1 frequency low
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF14, NR14)	// Sound: channel 1 frequency high
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF24, NR50)	// Sound: channel control, on/off, volume
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF25, NR51)	// Sound: selection of sound output terminal
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF26, NR52)	// Sound: sound on/off
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF4D, KEY1)	// CGB only: prepare speed switch
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFF7F, UNKNOWN)	// Tetris writes here, no documented function
DEFINE_MEMORY_MAPPED_REGISTER_RW(0xFFFF, IE)	// Interrupt enable

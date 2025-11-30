# CSOPESY CPU Scheduler Simulator - Compact Version


## Project Structure

```
csopesy-scheduler/
├── include/              # Header files (4 files)
│   ├── Config.h         # Config + Utils
│   ├── Process.h        # PCB, Instruction, ProcessGenerator, InstructionExecutor, ProcessManager
│   ├── Scheduler.h      # Scheduler + ScreenManager
│   └── CommandHandler.h # Command processing
├── src/                  # Implementation files (5 files)
│   ├── Config.cpp
│   ├── Process.cpp
│   ├── Scheduler.cpp
│   ├── CommandHandler.cpp
│   └── main.cpp
├── config.txt
└── README.md
```

## Consolidated Modules

### Config.h / Config.cpp
- **Config**: Configuration management (singleton)
- **Utils**: Utility functions (clearScreen, getTimestamp)

### Process.h / Process.cpp
- **Instruction**: Instruction types enum and struct
- **PCB**: Process Control Block structure
- **ProcessGenerator**: Creates random processes
- **InstructionExecutor**: Executes instructions
- **ProcessManager**: Manages all processes (singleton)

### Scheduler.h / Scheduler.cpp
- **Scheduler**: Multi-threaded CPU scheduler (singleton)
- **ScreenManager**: Display management (singleton)
- **ScreenMode**: Screen mode enum

### CommandHandler.h / CommandHandler.cpp
- **CommandHandler**: Command processing and queue management (singleton)

### main.cpp
- Entry point and main loop

## Building

```bash
# Compile
make

# Debug build
make debug

# Clean
make clean

# Build and run
make run
```

## Configuration (config.txt)

```
num-cpu 4
scheduler "rr"
quantum-cycles 5
batch-processes-freq 3
min-ins 100
max-ins 200
delay-per-exec 100
```

## Commands

- `initialize` - Load configuration
- `scheduler-start` - Start scheduler
- `scheduler-stop` - Stop scheduler
- `screen -s <name>` - Create process
- `screen -r <name>` - View process
- `screen -ls` - List all processes
- `process-smi` - Show process info
- `report-util` - Generate report
- `exit` - Exit

## Key Features

✅ **Compact Design**: Only 9 files total
✅ **Still Modular**: Clear logical separation
✅ **Thread-Safe**: Proper synchronization
✅ **Maintainable**: Related code grouped together
✅ **Easy to Build**: Simple Makefile

## What Changed from Full Version

**Combined:**
- Utils merged into Config
- Instruction, PCB, ProcessGenerator, InstructionExecutor, ProcessManager → Process
- Scheduler + ScreenManager → Scheduler

**Result:** 18 files → 9 files (50% reduction!)

## Authors

- Matthew Copon
- Chastine Cabatay
- Ericson Tan
- Joaquin Cardino

## Version


1.00.00 - Compact Edition


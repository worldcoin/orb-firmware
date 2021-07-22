# Make sure the debugger halts in HardFault Handler
define hook-continue
	set *((uint32_t*) 0xE000EDFC) |= 0x400
end

# Check CFSR and BusFault address if valid
define fault_regs
	set $cfsr=*(uint32_t*) 0xE000ED28
	printf "CFSR: 0x%08x\n", $cfsr
	if $cfsr & 0x8000
		printf "BusFault Address Valid: 0x%8x\n", *(uint32_t) 0xE00ED38
	end
end

# store the state of the stack before the exception
# to unwind the stack before the fault state
# to get the most accurate backtrace
define unwind_stack_backtrace
	set $exc_frame = ($lr & 0x4) ? $psp : $msp
	set $stacked_xpsr = ((uint32_t *) $exc_frame)[7]
	set $exc_frame_len = 32 + (($stacked_xpsr) ? 0x4 : 0x0) + (($lr & 0x10) ? 0 : 72)
	set $sp = ($exc_frame + $exc_frame_len)
	set $lr = ((uint32_t *) $exc_frame)[5]
	set $pc = ((uint32_t *) $exc_frame)[6]
	backtrace
end

python
# Update GDB's Python paths with the `sys.path` values of the local
#  Python installation, whether that is brew'ed Python, a virtualenv,
#  or another system python.

# Convert GDB to interpret in Python
import os,subprocess,sys
# Execute a Python using the user's shell and pull out the sys.path (for site-packages)
paths = subprocess.check_output('python -c "import os,sys;print(os.linesep.join(sys.path).strip())"',shell=True).decode("utf-8").split()
# Extend GDB's Python's search path
sys.path.extend(paths)
print("Python environment loaded:")
print(sys.path)
end

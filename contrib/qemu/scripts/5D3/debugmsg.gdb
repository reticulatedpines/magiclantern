# ./run_canon_fw.sh 5D3 -s -S & arm-none-eabi-gdb -x 5D3/debugmsg.gdb
# unless otherwise specified, these are valid for both 1.1.3 and 1.2.3

source -v debug-logging.gdb

macro define CURRENT_TASK 0x23E14
macro define CURRENT_ISR  (*(int*)0x670 ? (*(int*)0x674) >> 2 : 0)

b *0x5b90
DebugMsg_log

b *0x8b10
task_create_log

cont
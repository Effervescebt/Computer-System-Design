set confirm off
set auto-load local-gdbinit on
set architecture riscv:rv64
target remote tcp::29035
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
define reconnect
	target remote tcp::29035
end

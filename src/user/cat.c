// prints the contents of a known file to the terminal
//

#include "syscall.h"
#include "string.h"
#include "io.h"

void main(void) {

    int result;

    // Open ser1 device as fd=0
    result = _devopen(0, "ser", 1);
    if (result < 0) {
        _msgout("_devopen failed");
        _exit();
    }

    // open the file to be read
    char file_name[32];
    _msgout("file name:");
    _msgin(file_name, strlen(file_name));
    result = _fsopen(1, file_name);
    if (result < 0) {
        _msgout("_fsopen failed");
        _exit();
    }

    // read the file and print it to the terminal
    size_t size = 0;
    _ioctl(1, IOCTL_GETLEN, &size);
    char* buf = (char*) kmalloc(size);
    result = _read(1, buf, size);
    if (result < 0) {
        _msgout("_read failed");
        _exit();
    }
    _msgout(buf);    
}

# File Handling

Key files:

| unit                                                                      | docs                                  |
|---------------------------------------------------------------------------|---------------------------------------|
| [file.c](https://github.com/babevm/babevm/blob/master/src/c/file.c){:target="_blank"} | [link](doxygen/html/file_8c.html)     |

Babe VM supports a 'typeable' file system paradigm whereby the operations of open/close etc could be implemented for different types of 'files' - so that could be physical files on some storage medium, or some other readable and writeable thing.  In 'pattern' speak one could say it is a 'strategy' pattern.   At this time, just a file system is supported and that is implemented as part of the 'machine dependant' portion of the VM.    

An ANSI C impl is provided and that works for Windows/*nix/OSX.

The file handling layer also serves as a level of indirection between a native file handle, and what is presented to Java and used by the VM.  The layer maps between OS handles and a simple integer handle.    

On VM startup, the max file handles is determined by the compiler define `BVM_MAX_FILE_HANDLES` (default 32).

Support for handing lists of files, such as a folder listing is not yet supported.  PRs welcome.




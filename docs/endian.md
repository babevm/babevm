# Endian Support

Both big and little endian architectures are supported.  The compiler define `BVM_BIG_ENDIAN_ENABLE` when set to 1 will enable big endianess. The default is little endian.

To be honest, the big endian code has not been exercised since the 64bit port.  It used to run on a powermac G4 (powerpc chipset), but has not had much use in a while, so, at best, big endian support is pretty sketchy. But, the bones are there and likely would not take too much to get it going again.  Shout if it is something you need.  If you can provide a docker setup that run big endian that'd be a great start.    

Network data is big endian and so are numbers inside a Java classfile, so quite some of the code gets used.  Testing welcome!     
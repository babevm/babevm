# Some notable Limitations

* Heap size.  The memory allocator uses 24 bits to express the size of a memory chunk.  Each memory chunk has a 32 bit header of which 24 bits represent the size of the memory chunk.  At this time, those 24 bits represent a memory allocation size in bytes, so 2^24 is 16mb.  Given the target environment - that is likely plenty.  However, it is a fairly small code change to have those 24 bits refer to multiples of the minimum memory chunk size, so, in effect, it could address a lot more memory without too much effort.
* Class loaders are not responsible for reading class file bytes.  They serve primarily as a namespace for loaded classes and thus 'application' separation.  All class file reading is performed by the VM.  This is not absolute, just a design choice about safety.  But, using permissions could also provide such safety, so that could change.
* class loading is restricted to files that can be found either on the class path or are contained within a jar file on the class path.  Jar files must end with a lowercase ".jar".
* the heap does not grow or shrink - the heap size is declared on startup and its size remains constant.
* `strictfp` is not implemented so, as with Java 1.6, float / double maths is platform dependent and may vary between platforms and maybe even 32 bit and 64 bit on the same platform. Likely this is mostly the case with x86 processors.  PRs welcome.
* Object finalisation is not supported.


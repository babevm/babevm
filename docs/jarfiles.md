# Jar Files.

Key files:

| unit                                                                     | docs                                 |
|--------------------------------------------------------------------------|--------------------------------------|
| [zip.c](https://github.com/babevm/babevm/blob/master/src/c/zip.c){:target="_blank"}  | [link](doxygen/html/zip_8c.html)     |

Class loading from jar files is supported.  The classloader's classpath may include files that end in lowercase '.jar' (case sensitive).

Jar files can be 'stored' files with no compression - effectively just packing a bunch of files together, or the jar can have compressed files.  Both are supported, but support for compression can be disabled with the compiler define `BVM_JAR_INFLATE_ENABLE`.  Though, the inflation code is pretty small and only adds a few kb to the final build.    

The VM implements a very small directory cache for each jar it opens.  The entire directory is not cached, just some small parts of it that help the VM to locate files more quickly, for example, rather than cache file names in the jar, just the hash of the name is, and an offset into the directory.  In that way when looking up an entry only the cache may be scanned for matching hashes, and on a match, further inspection to see it if really is the requested file.  If it is, the directory offset gives where to get the rest of the file information.

Jar files are not closed by the VM, once on the classpath, they are open for the duration of the running VM, and their cached directories are also.

The jar file may remain open, but it is not held in memory.  It is read into memory in its entirety for internalising the directory but after that, the memory is freed, and each file loaded from the jar is accessed via its byte range in the 'on disk' jar file.

A small limitation is that 'notes' within a jar file is not supported and will cause the jar file to be unreadable. 


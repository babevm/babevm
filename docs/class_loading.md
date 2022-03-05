# Class Loading

Key files:

| unit                                                                        | docs                                  |
|-----------------------------------------------------------------------------|---------------------------------------|
| [clazz.c](https://github.com/babevm/babevm/blob/master/src/c/clazz.c){:target="_blank"} | [link](doxygen/html/clazz_8c.html)    |

See Also:

* [Permissions](permissions.md)

In Java, a class is namespaced by the ClassLoader object that loaded it.  Two identically named classes loaded with different loaders are considered by the VM to be different classes.  This can be used to prevent class naming collisions between two running applications.  Just like Java SE, classloaders here form a hierarchy, and each time a class is to be loaded the parentage gets first go.

Babe VM Classloaders use a classpath.  Classpaths may include '.jar' archive files.

There are two distinct VM classpaths.  One is the 'boot' classpath - represented by the `-Xbootclasspath` command line options - or can be set in code.  The 'bootstrap' class loader is responsible for loading the core 'java' and VM SDK classes.  Classes loaded on the boot classpath are trusted.

The second classpath is the 'user' classpath represented by the `-cp` command line option.  This is the classpath for application and framework classes.  No class on the user classpath may belong to a package that begins with `java/` or `babe/`.  This ensures that no application or framework class can pretend to be a system class.

Classpaths may be multi-segment with each segment being separated by a platform-dependant separator.  On Windows, this is `;`, on *nix this is `:`.  The platform separator can be set at compile time with the define `BVM_PLATFORM_FILE_SEPARATOR`.   

Unlike Java SE class loading, a class loader here does not have the capability of loading a stream of bytes and handing that off to the VM for class loading.  In this VM, only the VM itself can load class files.  The classloaders serve as a means of specifying a classpath for the VM to search on - and as a container for a list of Class objects it has loaded - to stop them being GC'd.  We get the benefit of classloader namespacing, but the security of knowing that developers cannot manipulate (possibly certified) class files on-the-fly.

Like Java SE, classloaders also serve as a means to stop Class objects being garbage collected (GC'd).  According to the JVMS, classes may be unloaded when their classloader is unreachable and is eligible for GC.  This is exactly how Babe does it.  Classes and their associated internal class structures may be GC'd, but only in those prescribed circumstances.  Being able to unload classes and clas loaders gives the VM the ability to completely unload an 'application' and all its classes and free up the memory for other 'applications'.  

The JVMS describes two types of relationships a classloader may have with a class.  A classloader may be the 'defining' loader for a given class (meaning it was the one that found the class in its classpath and actually loaded it), and/or it may be an 'initiating' classloader for a class (meaning it either loaded it, or attempted to load it but could not so it delegated it).  This VM does not track the 'initiating' relationship -  only the defining class loader relationship is maintained.

Each classloader may have a Java `ProtectionDomain` - this is a combination of a `CodeSource` and a set of related permissions.  This is the basis of the security permissions system.  For more info, see [Permissions](permissions.md).

The JVMS classloading procedure is fairly complex. Mainly this is to deal with the fact the multiple threads may try to load a class at the same time.  In many VMs, this is mostly done is C with some callouts to Java code.  In this VM, it is the opposite with most stuff being done in Java code with some callouts to the VM - it just worked out easier.  There is a lot of documentation on how this happens in the `Class.java` in the runtime classes, as well as unit level docs in the VM.


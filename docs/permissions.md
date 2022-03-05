# Permissions

A Java SE - like permissions system is supported, albeit somewhat smaller than the actual Java impl.  Most of that code is in the Babe core libs `security` package.  The `SecurityManager`, `Policy`, `AccessControlContext` and so on is implemented there at the Java level.

The VM feeds that framework by providing it an 'AccessControlContext' for the currently running code.  

An AccessControlContext is the summation of all the 'protection domains' for the code on the currently executing thread's stack _plus_ the AccessControlContext state captured when the current executing thread was created (its 'inherited' context). 

Protection domains are associated with a classloader and represent a pairing of where the code has come from (the 'code source') and some optional permissions.  To create the context, the stack is traversed downwards collecting the protection domains associated with the classloader for each class that has a method on the stack.  This array of protection domains is part of what makes up the data for an access control context.

As a caveat to the 'scans the stack' thing above, the search stops when in comes across a stack frame running the method `SecurityManager.doPrivileged()`.  This Java method signals that anything above it on the stack is run with the privileges of the code invoking `SecurityManager.doPrivileged()`.

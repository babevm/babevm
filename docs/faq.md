# FAQs

(okay, we made them up)

* **Why the name 'babe'**?  Google `0xCAFEBABE` and see what you find.  'Babe' also seems appropriate for a smaller VM.

* **Isn't Java dead**?  I'm not a Java fanboy or religious about stuff, but I think Java is far from dead.  Additionally, for the target platform of a small (possible payments oriented) device there are scant offerings for open source security-focussed platforms.

* **Why open source it**?  The original intended venture is long gone.  A great deal of work went into bringing a secure Java VM and runtime classes to a small platform so, why not share it and give it its own life.  Also, most other small VMs seem to have died some years ago.

* **Why Java 6**?  That was the last version before the Oracle era, and associated licence changes.  The terms in the JVMS also changed.

* **Why the Apache licence**?  To permit anybody to do anything really.  Plus, some small fragments of code come from the Apache Harmony project, and that wants and Apache license. Also, most other VM works are GNU licenced which makes them a little un-friendly for business - especially for embedded and derivative works.

* **Does anybody use 32bit**? - many payment platforms run on proprietary operating systems with compilers licenced many (many) years ago.  Likely today many are still 32 bit, often without ANSI C compliance.  So, making the VM 32 bit capable and using a minimum of ANSI C stuff is a good thing.    

* **Why green threads**?  Not all small platforms support threading.  Greens threading was the original way the Java platform achieved thread portability.  No doubt, a native thread implementation offers advantages, but, also portability issues.

* **What about JIT**.  PRs welcome.
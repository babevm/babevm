# VM Stacks

| unit                                                                          | docs                                |
|-------------------------------------------------------------------------------|-------------------------------------|
| [thread.c](https://github.com/babevm/babevm/blob/master/src/c/thread.c){:target="_blank"} | [link](doxygen/html/thread_8c.html) |
| [exec.c](https://github.com/babevm/babevm/blob/master/src/c/exec.c){:target="_blank"}     | [link](doxygen/html/exec_8c.html)   |
| [frame.c](https://github.com/babevm/babevm/blob/master/src/c/frame.c){:target="_blank"}   | [link](doxygen/html/frame_8c.html)  |

Babe implements dynamic stacks.  There is no configuration option to set the maximum stack height.  Stacks are allocated from the heap as needed, and garbage collected like any other memory used by the running VM.   

The docs link for `frame.c` gives a more detailed explanation. 
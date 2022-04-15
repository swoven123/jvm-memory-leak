# jvm-memory-leak
Repository aims to provide sets of tool to detect memory leak

#### Memory Leak
In order to detect and mitigate the memory leak issues below questions needs to be answered. 

1. Do I have a leak?  
2. What is leaking? 
3. What is keeping Object alive?
4. Where is it leaking from?

**garbage_collection_tracker.c** This file will help us to answer our first question by detecting where a running JVM has a leak. 

How can **garbage_collection_tracker.c** detect the leak? It tracks the garbage collection and calculates the heap size before and after a garbage collection, and if there is increase in the heap size even after garbage collection, and the heap size is continuously growing then there might be a memory leak. 

#### Example Log Printed 
Initial Heap Size: 671 KB <br>
GC started: Fri Apr 15 21:34:38 2022<br>
GC stopped: Fri Apr 15 21:34:58 2022<br>
GC pause time: 20.00 s<br>
Total Heap Size: 1024 KB</br>
GC started: Fri Apr 15 21:40:40 2022<br>
GC stopped: Fri Apr 15 21:40:60 2022<br>
GC pause time: 20.00 s<br>
Total Heap Size: 2222 KB</br>
GC started: Fri Apr 15 21:50:38 2022<br>
GC stopped: Fri Apr 15 21:50:58 2022<br>
GC pause time: 20.00 s<br>
Total Heap Size: 3333 KB<br>
GC started: Fri Apr 15 21:59:00 2022<br>
GC stopped: Fri Apr 15 22:01:03 2022<br>
GC pause time: 63.00 s<br>
Total Heap Size: 5543 KB<br>

#### Log Explanation
Initial Heap Size: First heap size Measured <br>
GC started: Time when garbage collection started <br>
GC stopped: Time when garbage collection stopped <br>
GC pause time: Total time garbage collection took to complete <br>
Total Heap Size: Heap size measured just after garbage collection completed <br>

According to the example log, heap size is increasing even after garbage collection, therefore we can predict there might be a memory leak. <br>

##### How to use it ?
1. Compile the code with gcc compiler and also include "jni.h", "jvmti.h" and "jni_md.h" which can be found inside /{your_jdk_path}/Contents/Home/include/ and /{your_jdk_path}/Contents/Home/include/linux/ folder as an example, gcc -I/Library/Java/JavaVirtualMachines/adoptopenjdk-8.jdk/Contents/Home/include/ -I/Library/Java/JavaVirtualMachines/adoptopenjdk-8.jdk/Contents/Home/include/darwin/ -shared -o gar.so -fPIC garbage_collection_tracker.c
2. Attach the shared file as a agent to the JVM, as an example: java -agentpath:./gar.so=/Users/swoven/Desktop/mleak.log,KB Test 
3. Options needs to be passed to the agent in comma seperated way like above, first option is the path of file where you want to print your logs and seconf option is the unit of heap size you can see as an example java -agentpath:./gar.so=/Users/swoven/Desktop/mleak.log,KB Test on this command /Users/swoven/Desktop/mleak.log is the path where the log will be printed and the heap size will be printed in KB unit. 








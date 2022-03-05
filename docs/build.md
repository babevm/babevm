## Building

The project has a CMake file to cover the basics.  The build is purposefully not complicated. 

The main difference in building is whether it is for Windows or Linux and whether it is a' debug' or 'release' build.  There are a number of precompiler defines use to control the final build and those are detailed separately. 

On Windows and OSX, the 'out of the box' default toolchain for Clion will build the 64 bit VM.  The Linux build also works on OSX.  The code supports 32 bit but since the world has become 64 bit now is has become increasingly harder to build for 32 bit.  And certainly more so for big-endian.

I use Jetbrains CLion - so instructions here relate to that IDE (CLion 2022.1 EAP at the moment).

Cross compiling for other platforms can be done with a docker setup.  Compiling for linux on either Windows or OSX can be done using the docker toolchain in CLion.  The included `docker/Dockerfile.cpp-env-ubuntu64` can be used to create an image that CLion will use on both platforms for 64 bit linux builds.

The `docker/Dockerfile.cpp-env-ubuntu32` can be used for 32 bit linux builds.

## CLion setup

### Out of the box

In settings 'Build, Execution, Deployment -> cmake' create two new profiles 'Debug' and 'Release' and configure as same.  Use the default toolchain and specify command line options like:

A linux debug cmake profile has the following command line options.  Swap 'Release' for 'Debug' to get a release build.  The linux build works on OSX without any mods.

```
-DCMAKE_BUILD_TYPE=Debug -DLINUX=ON
```

And for windows:

```
-DCMAKE_BUILD_TYPE=Debug -DWINDOWS=ON
```

### Docker

Using the linux 64 bit docker as an example.  From the project root, run the following to create an image for CLion:

```
docker build -t clion/ubuntu64/cpp-env:1.0 -f ./docker/Dockerfile.cpp-env-ubuntu64 .
```

In Clion settings/preferences Build, Execution, Deployment -> Toolchains' set up a new Docker toolchain and select the image we created above: `clion/ubuntu64/cpp-env:1.0`.  The other settings are as per other cmake targets.

On mac, I did have to add a mount to the `/Users` folder so I could test with classes residing in projects under that root folder.  The mounting is done in the docker toolchain 'Container settings'.  I had `--entrypoint -v /Users:/Users --rm`.  The 'Container settings' setting was added in Clion 2022.1.  

For 32 bit, the same applies, but on the cmake profile, add in a flag for 32 bit such that the cmake profile would look like: 

```
-DCMAKE_BUILD_TYPE=Debug -DLINUX=ON -D32BIT=ON
```

I would say if your needs go beyond choosing between windows/linux and 64/32 bit (and they likely will) just create your own build file.   

#### Running the VM inside a docker

After the docker build above is set up, the VM can be executed from the host inside the docker like such: 

```
docker run --rm -v /Users:/Users clion/ubuntu32/cpp-env:1.0 /Users/<path-to-VM-exe> <vm command line stuff>
```

If the command line params specify a JDWP port, then that will need to be provided to docker command line.  

## The SDK class / path

To be able to run any Java-compiled code, this project needs the Babe runtime classes.  Pull that project at [https://github.com/babevm/runtime](https://github.com/babevm/runtime) and do a maven build on it.  The path to the built `.class` artifacts, or jar file needs to be passed on the command line as `-Xbootclasspath`.  Additionally, the classpath for non-system classes will need to be provided as `-cp` (or `-classpath`).  The classpath can have multiple segments each separated by an os-dependant path separator char - ';' or ':'.  A segment can be a jar file.  Note that path segments, plus class file names cannot exceed 255 chars or bad stuff will happen.

Here is an example invocation:

```
-heap 256k -Xbootclasspath <sdk-classpath> -cp <user-classpath> com.stuff.MyProg
```

## Creating the Doxygen documentation

The doxygen docs are online at [https://babevm.github.io/babevm/doxygen/html](https://babevm.github.io/babevm/doxygen/html), but to generate them locally do the following.

The `Doxyfile` file in the project root contains doxygen config.  You'll need doxygen installed somewhere with the doxygen binary on the path.  On OSX, if you installed the doxygen app, then add this to your path: `/Applications/Doxygen.app/Contents/Resources`.

From the project root, run the following.  It creates the docs in the `<root>/docs/doxygen` folder.

```
doxygen ./Doxyfile
```

## Debugging Java code

To enable the VM debug (assuming it has been built with `BVM_DEBUGGER_ENABLE` defined), then the debug config is passed on the command line. Both are required.

```
 -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=<port>
 ```

The `Xdebug` option enables JDWP.  The `-Xrunjdwp` option provides config for JDWP.   

If `server=y` the VM will accept incoming debugger connections on the numeric port given by `address` - no IP address.  If `server=n` then the VM will attempt to connect to a remote listening debugger at the address given in `address` as format `<ipaddress>:<port>`.

If `suspend=y` and `server=y` then the VM will pause its startup process and wait for an incoming debugger connection.  In your IDE, set up a 'Remote JVM debug' using the above JDWP command line options.  If the VM is running inside a docker, you'll likely need to expose the debug port from docker to the host.  

At this time, the only implemented JDWP transport is `dt_socket`.

## Debugging a Java program running in the VM.

In your IDE, set up a debug config and provide command line params as such:

```
-Xbootclasspath <absolute-path-to>/rt.jar -cp <absolute-path-to-java.class>
```

Plus any other command line options you need. To view command line options run the executable with no options (or with the `-usage` option). 
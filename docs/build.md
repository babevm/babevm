## Building

The project has a CMake file to cover the basics.  The build is purposefully not complicated. 

The main difference in building is whether it is for Windows or Linux and whether it is a' debug' or 'release' build.  There are a number of precompiler defines use to control the final build and those are detailed separately. 

On Windows and OSX, the 'out of the box' default toolchain for Clion will build the 64 bit VM.  The Linux build also works on OSX.  The code supports 32 bit but since the world has become 64 bit now is has become increasingly harder to build for 32 bit.  And certainly more so for big-endian.

To get up and running, visit the quickstart guides: [vscode/Clion quickstarts](./quickstart.md)

Cross compiling for other platforms can be done with a docker setup.  Compiling for linux on either Windows or OSX can be done using the docker toolchain in CLion.  The included `docker/Dockerfile.cpp-env-ubuntu64` can be used to create an image that CLion will use on both platforms for 64 bit linux builds.

The `docker/Dockerfile.cpp-env-ubuntu32` can be used for 32 bit linux builds in Clion.

Docker stuff is also described in the quickstart.

## Creating the Doxygen documentation

The doxygen docs are online at [https://babevm.github.io/babevm/doxygen/html](https://babevm.github.io/babevm/doxygen/html), but to generate them locally do the following.

The `Doxyfile` file in the project root contains doxygen config.  You'll need doxygen installed somewhere with the doxygen binary on the path.  On OSX, if you installed the doxygen app, then add this to your path: `/Applications/Doxygen.app/Contents/Resources`.

From the project root, run the following.  It creates the docs in the `<root>/docs/doxygen` folder.

```
doxygen ./Doxyfile
```

## Debugging Java code

To enable the JDWP VM debug (assuming it has been built with `BVM_DEBUGGER_ENABLE` defined), then the debug config is passed on the command line. Both are required.

```
 -Xdebug -Xrunjdwp:transport=dt_socket,server=y,suspend=y,address=<port>
 ```

The `Xdebug` option enables JDWP.  The `-Xrunjdwp` option provides config for JDWP.   

If `server=y` the VM will accept incoming debugger connections on the numeric port given by `address` - no IP address.  If `server=n` then the VM will attempt to connect to a remote listening debugger at the address given in `address` as format `<ipaddress>:<port>`.

If `suspend=y` and `server=y` then the VM will pause its startup process and wait for an incoming debugger connection.  In your Java IDE, set up a 'Remote JVM debug' using the above JDWP command line options.  If the VM is running inside a docker, you'll likely need to expose the debug port from docker to the host.  

At this time, the only implemented JDWP transport is `dt_socket`.

## Debugging a Java program running in the VM.

In your 'C' IDE, set up a debug config and provide command line params as such:

```
-Xbootclasspath <absolute-path-to>/rt.jar -cp <absolute-path-to-java.class>
```

Plus any other command line options you need. To view command line options run the executable with no options (or with the `-usage` option). 
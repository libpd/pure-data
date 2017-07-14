# Pure Data macOS resources

This directory contains support files for building a Pure Data macOS application
bundle and supplementary build scripts for compiling Pd on Macintosh systems, as
it is built for the 'vanilla' releases on msp.ucsd.edu.

## App Bundle Helpers

* osx-app.sh: creates a Pd .app bundle for macOS using a Tk Wish.app wrapper
* tcltk-wish.sh: downloads and builds a Tcl/Tk Wish.app for macOS

These scripts complement the autotools build system described in INSTALL.txt and
are meant to be run after Pd is configured and built. Both osx-app.sh &
tcltck-wish.sh have extensive help output using the --help commandline option:

    mac/osx-app.sh --help
    ...
    mac/tcltk-wish.sh --help

In a nutshell, a monolithic macOS "application" is simply a directory structure
treated as a single object by the OS. Inside this bundle are the compiled
binaries, resource files, and contextual information. You can look inside any
application by either navigating inside it from the commandline or by
right-clicking on it in Finder and choosing "Show Package Contents."

The basic layout is:

Pd-0.47.1.app/Contents
  Info.plist  <- contextual info: version string, get info string, etc
  /Frameworks <- embedded Tcl/Tk frameworks (optional)
  /MacOS/Pd   <- renamed Wish bundle launcher
  /Resources
    /bin      <- pd binaries
    /doc      <- build in docs & help files
    /extra    <- core externals
    /include  <- Pd source header files
    /po       <- text translation files
    /tcl      <- Pd GUI scripts

The Pure Data GUI utilizes the Tk windowing shell aka "Wish" at runtime.
Creating a Pure Data .app involves using a precompiled Wish.app as a wrapper
by copying the Pd binaries and resources inside of it.

The osx-app.sh script automates this task and is used in the "make app" makefile
target. This default action can be invoked manually after Pd is built:

    mac/osxapp.sh 0.47.1

This builds a "Pd-0.47.1.app" using the included Wish. If you omit the version
argument, a "Pd.app" is built. The version argument is only used as a suffix to
the file name and contextual version info is pulled from configure script
output.

An older copy of Tk 8.4 Wish is included with the Pd source distribution and
works across the majority of macOS versions. This is the default Wish.app when
using osx-app.sh. If you want to use a different Wish.app (a newer version, a
custom build, a system version), you can specify the donor via commandline
options, for example:

    # build Pd-0.47.1.app using Tk 8.6 installed to the system
    mac/osx-app.sh --system-tk 8.6 0.47.1

If you want Pd to use a newer version of Tcl/Tk, but do not want to install to
it to your system, you can build Tcl/Tk as embedded frameworks inside of the Pd
.app bundle. This has the advantage of portability to other systems.

The tcltk-wish.sh script automates building a Wish.app with embedded Tcl/Tk,
either from the release distributions or from a git clone. You can also specify
which architectures to build: 32 bit, 64 bit, or both. Once your custom Wish.app
is built, you can use it as the .app source for osx-app.sh with the -w option:

    # build Wish-8.6.6.app with embedded Tcl/Tk 8.6.6
    mac/tcltk-wish.sh 8.6.6

    # build Pd with the custom donor Wish
    mac/osx-app.sh -w Wish-8.6.6.app

Downloading and building Tcl/Tk takes some time. If you are doing lots of builds
of Pd and/or are experimenting with different versions of Tcl/Tk, building the
embedded Wish.apps you need with tcltk-wish.sh can save you some time as they
can be reused when (re)making the Pd .app bundle.

## Supplementary Build Scripts

* build-macosx: builds a 32 bit Pd .app bundle using src/makefile.mac
* build-mac64: builds a 64 bit Pd .app bundle using src/makefile.mac

These scripts automate building Pd with the fallback makefiles in the src
directory.

To build a 32 bit Pd, copy this "mac" directory somewhere like ~/mac. Also copy
a source tarball there, such as pd-0.47-1.src.tar.gz. Then cd to ~/mac and type:

    ./build-macosx 0.47-1

If all goes well, you'll soon see a new app appear named Pd-0.47-1.app.

If you want to build a 64 bit Pd, perform the same steps and use the build-mac64
script:

    ./build-mac64 0.47-1

Note: The "wish-shell.tgz" is an archive of this app I found on my mac:
/System/Library/Frameworks/Tk.framework/Versions/8.4/Resources/Wish Shell.app

A smarter version of the scripts ought to be able to find that file
automatically on your system so I wouldn't have to include it here.

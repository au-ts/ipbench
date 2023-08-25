# setup

## Dependencies

```
automake
swig
libtool
python-dev
```

## Installing ipbench

Getting started on ipbench is relatively straightforward. It uses the standard GNU setup process with autotools, and can be built from source as follows:

```
./autogen.sh
./configure
make
sudo make install
```

There are a few quirks to note however:

### General

Autotools (correctly) puts files for user built packages in `/usr/local/lib` when using ipbench, but we have observed that some newer distros inexplicably do not follow the convention of putting user-built (i.e. pip or similar) packages here, and as a result this location is not in `$PYTHONPATH`. 

If running ipbench results in a python import error, you can add the location of the install to your pythonpath as follows:

```
export PYTHONPATH=$PYTHONPATH:/usr/local/lib/(python-version)/site-packages
```

Running this line will fix the issue for your current terminal session, and you can make this fix permanent by adding it to `.profile` and/or `.bashrc`.

NOTE: working on fixing this currently.

### Debian

Debian's handling of Python packages is different to everything else in that it adds a folder called `dist-packages` as well as the standard `site-packages`. Automake may put the ipbench files in the wrong one, so be wary!

If you get an error about finding shared files like below:
`ImportError: libipbench.so.1: cannot open shared object file: No such file or directory`

... run `sudo ldconfig -v` to prompt Debian to reregister your library files and this will rectify the issue.


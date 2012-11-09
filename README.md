Verse 2.0
=========

Verse 2.0 is network protocol for real-time sharing of 3D data. It is successor of old Verse protocol developed at KTH.
Verse 2.0 is still in alpha version.

License
-------

The source code of Verse library is licensed under BSD license. This library
could be used for implementation of Verse client. The source code of Verse
server is licensed under GNU GPL 2.0 license. For details look at files
BSD-LICENSE and GPL-LICENSE.

Important links
---------------

 * http://verse.github.com/
 * http://verse.github.com/verse/
 * https://github.com/verse/verse

Compile
-------

Compilation of Verse 2.0 is tested only on Linux now. Some libraries
and development tools are required. Porting to other UNIX like OS should
be possible.
  
### Requirements ###

 * GCC http://gcc.gnu.org/ or Clang http://clang.llvm.org/
 * CMake http://www.cmake.org/
 * OpenSSL http://www.openssl.org/
 * IniParser http://ndevilla.free.fr/iniparser/
 * Check (optional) http://check.sourceforge.net/
 * Spin (optional) http://spinroot.com/
 * Python3 (optional) http://www.python.org

### Building ###

To compile Verse server, libverse.ao and example of Verse
client open terminal, go to root of verse source code and type:
  
    $ mkdir ./build
    $ cd ./build
    $ cmake ../
    $ make
    $ sudo make install
  
If you want to build release version, then you have to run cmake
with following parameter:

    $ cmake -DCMAKE_BUILD_TYPE=Release ../

If you want to build Verse with Clang, then you have to do more
  
    $ export CC=/usr/bin/clang      
    $ export CXX=/usr/bin/clang++
    $ mkdir ./build
    $ cd ./build
    $ cmake ../
    $ make
    $ sudo make install
  
Folders
-------

 * ./build		is target directory for compiled binaries
 * ./config	is directory with example of users.csv file
 * ./doc		contains doxyfile for generated doxygen documentation
 * ./example	contains source code of example Verse clients
 * ./include	contains all .h files
 * ./pki		contains example of certificate and private key
 * ./python	contains source code for Python module implemented in C
 * ./promela   contains Promela source code used for protocol verification
 * ./src		contains source code
  * ./api			source code of API
  * ./client		source code specific for Verse clients
  * ./common		source code shared with Verse server and Verse client
  * ./server		source code specified for Verse server 
 * ./unittests	contains source code of unit tests

Installation
------------

The verse server and example of verse client is not necessary to install to the
system. Before you want to run Verse server you should edit your "database"
of users. Go to the ./config folder and edit users.csv:

    $ cd ./config

and edit user.csv with your favorite text editor, e.g. vim:

    $ vim users.csv

Using
-----

The Verse server can be executed from build directory:

    $ ./bin/verse_server

Example of Verse client could be executed from this directory too:

    $ ./bin/verse_client localhost

The example of Verse client and Verse server can be started with several
arguments. For more details run programs with '-h' option.

When you want to try test Verse client implemented Python, then you have to
set up system variable PYTHONPATH to include directory containing Python
module "verse.so" :

    $ export PYTHONPATH=/path/to/directory/with/verse/module

Then it is possible to run this client:

    $ python3 verse_client.py

Firewalls
---------

If you use firewall and you want to connect on Verse server, then you will need
to open several ports. The Verse server listen on TCP port 12345 and it
negotiate new UDP ports in range: 20000 - 20009 (this port numbers and port
ranges will be possible to change in configuration files in the future).

When you use Linux OS, then you can use iptables for it. You should be familiar
with iptables. If not, then read some documentation about iptables first. To
open TCP port use something like this:

    $ iptables -I INPUT 10 \ # this add this rule before 10th rule (change this!)
      -m state --state NEW \ # use this rule only for the first packet (optional)
      -s 1.2.3.4/16 \        # allow connection only from some subnet (optional)
      -p tcp --dport 12345 \ # own opening of TCP port 12345
      -j ACCEPT              # accept this packet

To open UDP port in range 2000 - 20009 use something like this:

    $ iptables -I INPUT 11 \
      -m state --state NEW \
      -s 1.2.3.4/16 \
      -p udp --dport 20000:20009 \ # open UDP ports in range: 20000 - 20009
      -j ACCEPT

Verse server and all verse client can use IPv6. Configuration of ip6tables is
very similar:

    $ ip6tables -I INPUT 10 \
      -m state --state NEW \
      -p tcp --dport 12345 \
      -j ACCEPT

    $ ip6tables -I INPUT 11 \
      -m state --state NEW \
      -p udp --dport 20000:20009
      -j ACCEPT

Verification
------------

For more details go to the directory ./promela.

Testing
-------

If you want to test Verse library, then there are several unit tests. To
perform tests you have to go to the build directory and run make with target
test

    $ cd ./build
    $ make test

Contacts
--------

 * IRC: irc.freenode.net in channel #verse
 * Mailing list: verse-dev@blender.org
 * Main developer: Jiri Hnidek
  * E-mail: jiri.hnidek@tul.cz
  * Phone: +420 485 35 3695
  * Address: Studentska 2, 461 17, Liberec 1, Czech Republic


                         ef2master, an open master server
                         --------------------------------

                               General information
                               -------------------


 1) INTRODUCTION
 2) SYNTAX & OPTIONS
 3) BASIC USAGE
 4) SECURITY
 5) OUTPUT AND VERBOSITY LEVELS
 6) LOGGING
 7) GAME POLICY
 8) ADDRESS MAPPING
 9) LISTENING INTERFACES
10) CONTACTS & LINKS


1) INTRODUCTION:

ef2master is a masterserver for Star Trek - Elite Force 2. It is based on dpmaster
a masterserver written by Mathieu Olivier for the Dark Places Engine.

Take a look at the "COMPILING EF2MASTER" section in "techinfo.txt" for more
practical information on how to build it.

The source code of ef2master is available under the GNU General Public License,
version 2. You can find the text of this license in the file "license.txt".


2) SYNTAX & OPTIONS:

The syntax of the command line is the classic: "ef2master [options]". Running
"ef2master -h" will print the available options for your version. Be aware that
some options are only available on UNIXes, including all security-related
options - see the "SECURITY" section below.

All options have a long name (a string), and most of them also have a short name
(one character). In the command line, long option names are preceded by 2
hyphens and short names by 1 hyphen. For instance, you can run ef2master as a
daemon on UNIX systems by calling either "ef2master -D" or "ef2master --daemon".

A lot of options have one or more associated parameters, separated from the
option name and from each other by a blank space. Optionally, you are allowed
to simply append the first parameter to an option name if it is in its short
form, or to separate it from the option name using an equal sign if it is in its
long form. For example, these 4 ways of running ef2master with a maximum number
of servers of 16 are equivalent:

   * ef2master -n 16
   * ef2master --max-servers 16
   * ef2master -n16
   * ef2master --max-servers=16


3) BASIC USAGE:

For most users, simply running ef2master, without any particular parameter,
should work perfectly. Being an open master server, it does not require any
game-related configuration. The vast majority of ef2master's options deal with
how you want to run it: which network interfaces to use, how many servers it
will accept, where to put the log file .... and all those options have default
values that should suit almost everyone.

That being said, here are a few options you may find handy.

The most commonly used one is probably "-D" (or "--daemon"), a UNIX-specific
option to make the program run in the background, as a daemon process.

If you intent to run ef2master for a long period of time, you may want to take a
look at the log-related options before starting it (see the LOGGING section). In
particular, make sure you have write permission in the directory the log file is
supposed to be written.

Finally, you can use the classic verbose option "-v" to make ef2master print
extra information (see "OUTPUT AND VERBOSITY LEVELS" below for more).


4) SECURITY:

First, you shouldn't be afraid to run ef2master on your machine: at the time I
wrote those lines, only one security warning has been issued since the first
release of ef2master. It has always been developed with security in mind and will
always be.

Also, ef2master needs very few things to run in its default configuration. A
little bit of memory, a few CPU cycles from time to time and a network port are
its only basic requirements. So feel free to restrict its privileges as much as
you can.

The UNIX/Linux version of ef2master has even a builtin security mechanism that
triggers when it is run with super-user (root) privileges. Basically, the
process locks (chroots) itself in the directory "/var/empty/" and drops its
privileges in favor of those of user "nobody". This path and this user name are
of course customizable, thanks to the '-j' and '-u' command line options.

If you are running ef2master on a Windows system, you may want to add a
"ef2master" user on your computer. Make it a normal user, not a power user or an
administrator. You'll then be able to run ef2master using this low-privilege
account. Right click on "ef2master.exe" while pressing the SHIFT button; select
"Run as...", and type "ef2master", the password you chose for it, and your
domain main (your computer name probably). The same result can also be achieved
by using Windows' "runas" command.


5) OUTPUT AND VERBOSITY LEVELS:

The "-v" / "--verbose" option allows you to control the amount of text ef2master
outputs. Setting its verbosity to a particular level make ef2master output all
texts belonging to that level or below. If you don't specify a verbose level
right after the "-v" command line option, the highest level will be used. 

There are 5 verbose levels:
   * 0: No output, except if the parsing of the command line fails.
   * 1: Fatal errors only. It is almost similar to level 0 since fatal errors
        mostly occur during the parsing of the command line in this version.
   * 2: Warnings, including non-fatal system errors, malformed network messages,
        unexpected events (when the maximum number of servers is reached for
        instance), and the server list printed on top of log files.
   * 3: The default level. Standard printings, describing the current activity.
   * 4: All information (a lot!), mostly helpful when trying to debug a problem.

Looking for errors in a level 4 log can be a tedious task. To make your job
easier, all error messages in ef2master start with the word "ERROR" in capital
letters, and all warning messages start with the word "WARNING", again in
capital letters.


6) LOGGING:

You can enable logging by adding "-L" or "--log" to the command line. The
default name of the log file is "ef2master.log", either in the working directory
for Windows systems, or in the "/var/log" directory for UNIX systems. You can
change the path and name of this file using the "--log-file" option.

The obvious way to use the log is to enable it by default. But if you want to do
that, you may consider using a lesser verbose level ("-v" or "--verbose", with a
value of 1 - only errors, or 2 - only errors and warnings), as ef2master tends
to be very verbose at its default level (3) or higher.

Another way to use the log is to set the verbose level to its maximum value, but
to enable the log only when needed, and then to disable it afterwards. This is
possible on systems that provide POSIX signals USR1 and USR2 (all supported
systems except the Windows family). When ef2master receives the USR1 signal, it
opens its log file, or reopens it if it was already opened, dumps the list of
all registered servers, and then proceeds with its normal logging. When it
receives the USR2 signal, it closes its log file.

Note that ef2master will never overwrite an existing log file, it always appends
logs to it. It prevents you from losing a potentially important log by mistake,
with the drawback of having to clean the logs manually from time to time.

There are a couple of pitfalls you should be aware of when using a log file:
first, if you run ef2master as a daemon, remember that its working directory is
the root directory, so be careful with relative paths. And second, if you put
your ef2master into a chroot jail, and start or restart the log after the
initialization phase, its path will then be rooted and relative to the jail root
directory.


7) GAME POLICY:

If you run an instance of ef2master, we strongly encourage you to let it open to
any game or player. ef2master has been developed for this particular usage and is
well-suited for it.

That said, if you want to restrict which games are allowed on your master, you
can use the "--game-policy" option. It makes ef2master explicitly accept or
reject network messages based on the game they are related to. For example:

        ef2master --game-policy accept Quake3Arena Transfusion

will force ef2master to accept servers, and answer to requests, only when they're
related to either Q3A or Transfusion. At the opposite:

        ef2master --game-policy reject AnnoyingGame

will accept any game messages except those related to AnnoyingGame.

You can have multiple "--game-policy" lists on the same command line, but they
must all use the same policy (either "accept" or "reject").

As you can see in the first example, "Quake3Arena" is the name you'll have to
use for Q3A. The other game names only depend on what code names their
respective engines choose to advertise their servers and to make their client
requests.

Two final warnings regarding this option. First, be careful, the names are case-
sensitive. And second, this option expects at least 2 parameters (accept/reject,
and at least one game name), so this:

        ef2master --game-policy accept -v -n 200

will make ef2master accept messages only when they will be related to a game
called "-v" (certainly not what you want...).


8) ADDRESS MAPPING:

Address mapping allows you to tell ef2master to transmit an IPv4 address instead
of another one to the clients, in the "getserversResponse" messages. It can be
useful in several cases. Imagine for instance that you have a ef2master and a
server behind a firewall, with local IPv4 addresses. You don't want the master
to send the local server IP address. Instead, you probably want it to send the
firewall address.

Address mappings are currently only available for IPv4 addresses. It appears
IPv6 doesn't need such a mechanism, since NATs have been deprecated in this new
version of the protocol. However, feel free to contact me if you actually need
IPv6 address mappings for some reason.

Address mappings are declared on the command line, using the "-m" or "--map"
option. You can declare as many of them as you want. The syntax is:

        ef2master -m address1=address2 -m address3=address4 ...

An address can be an explicit IPv4 address, such as "192.168.1.1", or an host
name, "www.mydomain.net" for instance. Optionally, a port number can be
appended after a ':' (ex: "www.mydomain.net:1234").

The most simple mappings are host-to-host mappings. For example:

        ef2master -m 1.2.3.4=myaddress.net

In this case, each time ef2master would have transmitted "1.2.3.4" to a client,
it will transmit the "myaddress.net" IP address instead.

If you add a port number to the first address, then the switching will only
occur if the server matches the address and the port number.
If you add a port number to the second address, then ef2master will not only
change the IP address, it will also change the port number.

So there are 4 types of mappings:
    - host1 -> host2 mappings:
        They're simple, we just saw them.

    - host1:port1 -> host2:port2 mappings:
        If the server matches exactly the 1st address, it will be transmitted
        as the 2nd address.

    - host1:port1 -> host2 mappings:
        If the server matches exactly the 1st address, its IP address will be
        transmitted as the "host2" IP address. The port number won't change.
        It's equivalent to "host1:port1=host2:port1".

    - host1 -> host2:port2 mappings
        If the server is hosted on host1, its address will be transmitted as
        "host2:port2".

Finally, be aware that you can't declare an address mapping from or to
"0.0.0.0", neither can you declare an address mapping to a loopback address
(i.e. 127.x.y.z:p). Mapping from a loopback address is permitted though, and
it's actually one of the 2 only ways to make ef2master accept a server talking
from a loopback address (the other way being a command line option used for
test purposes - do NOT run your master with this option!).


9) LISTENING INTERFACES:

By default, ef2master creates one IPv4 socket and one IPv6 socket (if IPv6
support is available of course). It will listen on every network interface, on
the default port unless you specified another one using "-p" or "--port". If
you want it to listen on one or more particular interface(s) instead, you will
have to use the command line option "-l" or "--listen".

Running ef2master with no "-l" option is (almost) like running it with:

    ef2master --listen 0.0.0.0 --listen ::

The first option is for listening on all IPv4 interfaces, the second for
listening on all IPv6 interfaces, both on the default port. The only
difference between this command line and one without any "--listen" option is
that ef2master will abort in the former if IPv4 or IPv6 isn't supported by your
system, as you have explicitly requested those network sockets to be opened.
Note that if you don't want ef2master to listen on IPv6 interfaces, you can
easily do it by only specifying "-l 0.0.0.0" on the command line.

As usual, you can specify a port number along with an address, by appending
":" and then the port. In this case, numeric IPv6 addresses need to be put
between brackets first, so that ef2master won't get confused when interpreting
the various colons. For example:

    ef2master -l an.address.net:546 -l [2000::1234:5678]:890

will make ef2master listen on the IPv6 interface 2000::1234:5678 on port 890,
and on the IPv4 or IPv6 interface "an.address.net" (depending on what protocol
the resolution of this name gives) on port 546.

IPv6 addressing has a few tricky aspects, and zone indices are one of them. If
you encounter problems when configuring ef2master for listening on a link-local
IPv6 address, I recommend that you take a look at the paragraph regarding zone
indices in the Wikipedia article about IPv6 <http://en.wikipedia.org/wiki/IPv6>.


10) CONTACTS & LINKS:

You can get the latest versions of ef2master on the its
home page <http://ef2master.hennecke-online.net>.

# stapler - kernel module to provide inter process communication

There are many ipc frameworks. Some of them reside in the kernel space
like Android's binder or kdbus, other are located in the user space like
D-Bus, Apache Thrift and probably hundreds others.
Motivation for this one, was QNX message passing subsystem.
At the very begining I was wondering whether it is possible to mimic
QNX behaviour for MsgSend, MsgReceive and MsgReply.
Once this process somehow succeeded, I started to add optimizations
and other additions. And that's how this project started and then evolved.

One of the key features of this ipc framework is that there are no intermediate
buffers when copying messages from one process to another.
If process A wants to pass messages to process B, the messages are copied
immediately to the address space of the process B from process A.
The same applies to reply messages when process B wants to reply to process A.

Similar approach was used in "binder" starting from Android 8.
Let me cite "https://source.android.com/docs/core/architecture/hidl/binder-ipc".
Android 8 uses scatter-gather optimization to reduce the number of copies from 3 to 1.
Instead of serializing data in a Parcel first, data remains in its original structure
and memory layout and the driver immediately copies it to the target process.
After the data is in the target process, the structure and memory layout is the same
and the data can be read without requiring another copy.

## BUILD
To build this module, just run:

    $ make

This should give you a file named "stplr.ko", which is the requested kernel module.
Such generated kernel module can be loaded and run only on the same kernel version
as the one used for compilation. So if you've updated your kernel, either explicitely
or implicilely via some "update manager", you would have to rebuild your module one more time,
this time against the updated version of your kernel (and kernel headers).

### BUILD AGAIN
To rebuild stplr.ko module, you have to first clean the previous artefacts of your build process.
To do so, just type

    $ make clean

## INSTALL
To install the module, run "make install" (you might have to be 'root' to have
all necessary permissions to install the module).

If your system has "sudo", do:

    $ make
    $ sudo make install
    $ sudo depmod -a

If your system lacks "sudo", do:

    $ make
    $ su
    (enter root password)
    $ make install
    $ depmod -a
    $ exit


(The `depmod -a` call will re-calculate module dependencies, in order to
automatically load additional kernel modules required by stplr.ko.

## RUN
Load the stplr.ko module as root. Yet again.

If your system has "sudo", type:

    $ sudo modprobe stplr

If your system lacks "sudo", do:

    $ make
    $ su
    (enter root password)
    $ modprobe stplr
    $ exit

## OPTIONS

### debug
You may specify verbosity of debug messages emmited by stapler module.
Default value is 0, which means that no debug messaged will be printed.
Max value is 4, enabling highest verbosity level. Thus typing

    $ sudo modprobe stplr debug=4

will turn on highest verbosity, whereas typing

    $ sudo modprobe stplr

will use default level (none debug messages will be emited).

### devices
You may specify how many "/dev/stplr" devices will be created by this stapler module.
Default value is 1. So running

    $ sudo modprobe stplr devices=3

will create 3 devices (`/dev/stplr-0`, `/dev/stplr-1`, and `/dev/stplr-2`).

## TESTS

Basic tests and the same examples showing the usage of the stapler module are available
in the `tests/client-server` directory. `server.c` presents the server side, where the
main role plays STPLR_MSG_RECEIVE and STPLR_MSG_REPLY ioctls.
`client1.c` uses STPLR_MSG_SEND ioctl (oneway) to communicate with the server
whereas client2.c uses STPLR_MSG_SEND_RECEIVE (two-way).
Directory `tests/examples` contains examples of Remote Procedure Calls
using raw D-Bus framework and Apache Thrift framework.
Apache Thrift gives the ability to implement custom transport mechanism.
This is what I've done. I implemented custom Thrift transport based on
stapler module and then compared timings of execution of the same rpc interfaces
once using the original Thrift TSocket and then my custom
ClientStaplerTransport and ServerStaplerTransport transports.
D-Bus is placed here mostly because I know it a little and I wanted to check
how it compares when it comes to timings to other rpc frameworks
(in this case to Thrift with two different transport implementations).
To do such tests/comparitions I introduced one base interface (example)
and 3 other interfaces (ping, calculator and timer) which inherit from
that base interface.
Its IDL description (in Thrift syntax) is briefly shown below.
```
service example {
    string name(),
    version_struct version(),
}
```
```
service ping extends example.example {
    test_struct ping(1: test_struct arg),
    oneway void hello(1: string text)
}
```
```
service calculator extends example.example {
    i32 add(1:i32 arg1, 2:i32 arg2),
    i32 subtract(1:i32 arg1, 2:i32 arg2),
    i32 multiply(1:i32 arg1, 2:i32 arg2),
    i32 divide(1:i32 arg1, 2:i32 arg2)
}
```
```
service timer extends example.example {
    timer_id start(1: i64 interval_us),
    void stop(1: timer_id id),
    timestamp tick(1: timer_id id)
}
```

### D-BUS
Let us first start with D-BUS.

#### Ping interface
- Sending 10000 `ping` messages takes on avarage ~2300 ms.
`ping` messages are ordinary blocking (send/receive) calls.
- Sending 10000 `hello` messages takes on avarage also ~2300 ms.
`hello` messages in the D-Bus implementation are also blocking
(send/receive) calls. There is no possibility to send oneway
(fire and forget) messages via D-Bus.
D-Bus signals could be used to get such semantic,
but they are not the same as messages.

#### Calculator interface
- Sending 10000 times the sequence (add, subtract, multiply and divide) takes
on avarage ~4504 ms.

#### Timer interface
Timer interface measures time between sending a message/signal
and time of arrival of such packet. In the case of the D-Bus implementation
signals are used to measure such "propagation" time. Server side queries system level
monotonic time just before publishing a signal (storing this timestamp in the signal payload)
and client side queries the same monotonic time just at signal arrival.
Then diff if calculated. On avarage such diff for D-Bus implementation is 350 microseconds.

### Thrift with the stock TSocket transport layer
In this section I will present timings where the same three interfaces
(ping, calculator and timer) are implemented using Apache Thrift framework.
The aplied transport mechanism is the Thrift's stock TSocket.

#### Ping interface
- Sending 10000 `ping` messages takes on avarage ~770 ms.
`ping` messages are ordinary blocking (send/receive) calls.
- Sending 10000 `hello` messages takes on avarage ~390 ms.
`hello` messages are fire and forget (oneway) calls.

#### Calculator interface
- Sending 10000 times the sequence (add, subtract, multiply and divide) takes
on avarage ~950 ms.

#### Timer interface
In the case of the Thrift implementation signals cannot be used as they are not present
in the Thrift framework. So regular methods are used to measure message "propagation" time.
Just like in the D-Bus implementation server side queries system level monotonic time
just before publishing a message (storing this timestamp in the message payload)
and client side queries the same monotonic time just at message arrival.
Then diff if calculated. On avarage such diff for Thrift implementation is 150 microseconds.

### Thrift with the custom ClientStaplerTransport and ServerStaplerTransport transports
This section presents timings where stock TSocket is replaced by custom
ClientStaplerTransport and ServerStaplerTransport transports.

#### Ping interface
- Sending 10000 `ping` messages takes on avarage ~478 ms.
`ping` messages are ordinary blocking (send/receive) calls.
- Sending 10000 `hello` messages takes on avarage ~220 ms.
`hello` messages are fire and forget (oneway) calls.

#### Calculator interface
- Sending 10000 times the sequence (add, subtract, multiply and divide) takes
on avarage ~695 ms.

#### Timer interface
When Thrift's stock TSocket transport is replaced by custom ClientStaplerTransport and ServerStaplerTransport
then avarage time of message propagation is around 140 microseconds.

### TODO
- Replace this (pid, tid) tupple by something like connection_id.
  Maybe introduce something similar to QNX ConnectAttach().
- Do not call init-deinit msg pages if the caller passes the same memory pointers.
- Use inline messages. Something similar to Android's inline transaction buffer.
- Figure out better encoding for a handle.
- debugfs
- Priority inheritance

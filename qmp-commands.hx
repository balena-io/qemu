HXCOMM QMP dispatch table and documentation
HXCOMM Text between SQMP and EQMP is copied to the QMP documentation file and
HXCOMM does not show up in the other formats.

SQMP
                        QMP Supported Commands
                        ----------------------

This document describes all commands currently supported by QMP.

Most of the time their usage is exactly the same as in the user Monitor, this
means that any other document which also describe commands (the manpage,
QEMU's manual, etc) can and should be consulted.

QMP has two types of commands: regular and query commands. Regular commands
usually change the Virtual Machine's state someway, while query commands just
return information. The sections below are divided accordingly.

It's important to observe that all communication examples are formatted in
a reader-friendly way, so that they're easier to understand. However, in real
protocol usage, they're emitted as a single line.

Also, the following notation is used to denote data flow:

-> data issued by the Client
<- Server data response

Please, refer to the QMP specification (QMP/qmp-spec.txt) for detailed
information on the Server command and response formats.

NOTE: This document is temporary and will be replaced soon.

1. Stability Considerations
===========================

The current QMP command set (described in this file) may be useful for a
number of use cases, however it's limited and several commands have bad
defined semantics, specially with regard to command completion.

These problems are going to be solved incrementally in the next QEMU releases
and we're going to establish a deprecation policy for badly defined commands.

If you're planning to adopt QMP, please observe the following:

    1. The deprecation policy will take effect and be documented soon, please
       check the documentation of each used command as soon as a new release of
       QEMU is available

    2. DO NOT rely on anything which is not explicit documented

    3. Errors, in special, are not documented. Applications should NOT check
       for specific errors classes or data (it's strongly recommended to only
       check for the "error" key)

2. Regular Commands
===================

Server's responses in the examples below are always a success response, please
refer to the QMP specification for more details on error responses.

EQMP

    {
        .name       = "quit",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_quit,
    },

SQMP
quit
----

Quit the emulator.

Arguments: None.

Example:

-> { "execute": "quit" }
<- { "return": {} }

EQMP

    {
        .name       = "eject",
        .args_type  = "force:-f,device:B",
        .mhandler.cmd_new = qmp_marshal_eject,
    },

SQMP
eject
-----

Eject a removable medium.

Arguments: 

- force: force ejection (json-bool, optional)
- device: device name (json-string)

Example:

-> { "execute": "eject", "arguments": { "device": "ide1-cd0" } }
<- { "return": {} }

Note: The "force" argument defaults to false.

EQMP

    {
        .name       = "change",
        .args_type  = "device:B,target:F,arg:s?",
        .mhandler.cmd_new = qmp_marshal_change,
    },

SQMP
change
------

Change a removable medium or VNC configuration.

Arguments:

- "device": device name (json-string)
- "target": filename or item (json-string)
- "arg": additional argument (json-string, optional)

Examples:

1. Change a removable medium

-> { "execute": "change",
             "arguments": { "device": "ide1-cd0",
                            "target": "/srv/images/Fedora-12-x86_64-DVD.iso" } }
<- { "return": {} }

2. Change VNC password

-> { "execute": "change",
             "arguments": { "device": "vnc", "target": "password",
                            "arg": "foobar1" } }
<- { "return": {} }

EQMP

    {
        .name       = "screendump",
        .args_type  = "filename:F",
        .mhandler.cmd_new = qmp_marshal_screendump,
    },

SQMP
screendump
----------

Save screen into PPM image.

Arguments:

- "filename": file path (json-string)

Example:

-> { "execute": "screendump", "arguments": { "filename": "/tmp/image" } }
<- { "return": {} }

EQMP

    {
        .name       = "stop",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_stop,
    },

SQMP
stop
----

Stop the emulator.

Arguments: None.

Example:

-> { "execute": "stop" }
<- { "return": {} }

EQMP

    {
        .name       = "cont",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_cont,
    },

SQMP
cont
----

Resume emulation.

Arguments: None.

Example:

-> { "execute": "cont" }
<- { "return": {} }

EQMP

    {
        .name       = "system_wakeup",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_system_wakeup,
    },

SQMP
system_wakeup
-------------

Wakeup guest from suspend.

Arguments: None.

Example:

-> { "execute": "system_wakeup" }
<- { "return": {} }

EQMP

    {
        .name       = "system_reset",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_system_reset,
    },

SQMP
system_reset
------------

Reset the system.

Arguments: None.

Example:

-> { "execute": "system_reset" }
<- { "return": {} }

EQMP

    {
        .name       = "system_powerdown",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_system_powerdown,
    },

SQMP
system_powerdown
----------------

Send system power down event.

Arguments: None.

Example:

-> { "execute": "system_powerdown" }
<- { "return": {} }

EQMP

    {
        .name       = "device_add",
        .args_type  = "device:O",
        .params     = "driver[,prop=value][,...]",
        .help       = "add device, like -device on the command line",
        .mhandler.cmd_new = qmp_device_add,
    },

SQMP
device_add
----------

Add a device.

Arguments:

- "driver": the name of the new device's driver (json-string)
- "bus": the device's parent bus (device tree path, json-string, optional)
- "id": the device's ID, must be unique (json-string)
- device properties

Example:

-> { "execute": "device_add", "arguments": { "driver": "e1000", "id": "net1" } }
<- { "return": {} }

Notes:

(1) For detailed information about this command, please refer to the
    'docs/qdev-device-use.txt' file.

(2) It's possible to list device properties by running QEMU with the
    "-device DEVICE,\?" command-line argument, where DEVICE is the device's name

EQMP

    {
        .name       = "device_del",
        .args_type  = "id:s",
        .mhandler.cmd_new = qmp_marshal_device_del,
    },

SQMP
device_del
----------

Remove a device.

Arguments:

- "id": the device's ID or QOM path (json-string)

Example:

-> { "execute": "device_del", "arguments": { "id": "net1" } }
<- { "return": {} }

Example:

-> { "execute": "device_del", "arguments": { "id": "/machine/peripheral-anon/device[0]" } }
<- { "return": {} }

EQMP

    {
        .name       = "send-key",
        .args_type  = "keys:q,hold-time:i?",
        .mhandler.cmd_new = qmp_marshal_send_key,
    },

SQMP
send-key
----------

Send keys to VM.

Arguments:

keys array:
    - "key": key sequence (a json-array of key union values,
             union can be number or qcode enum)

- hold-time: time to delay key up events, milliseconds. Defaults to 100
             (json-int, optional)

Example:

-> { "execute": "send-key",
     "arguments": { "keys": [ { "type": "qcode", "data": "ctrl" },
                              { "type": "qcode", "data": "alt" },
                              { "type": "qcode", "data": "delete" } ] } }
<- { "return": {} }

EQMP

    {
        .name       = "cpu",
        .args_type  = "index:i",
        .mhandler.cmd_new = qmp_marshal_cpu,
    },

SQMP
cpu
---

Set the default CPU.

Arguments:

- "index": the CPU's index (json-int)

Example:

-> { "execute": "cpu", "arguments": { "index": 0 } }
<- { "return": {} }

Note: CPUs' indexes are obtained with the 'query-cpus' command.

EQMP

    {
        .name       = "cpu-add",
        .args_type  = "id:i",
        .mhandler.cmd_new = qmp_marshal_cpu_add,
    },

SQMP
cpu-add
-------

Adds virtual cpu

Arguments:

- "id": cpu id (json-int)

Example:

-> { "execute": "cpu-add", "arguments": { "id": 2 } }
<- { "return": {} }

EQMP

    {
        .name       = "memsave",
        .args_type  = "val:l,size:i,filename:s,cpu:i?",
        .mhandler.cmd_new = qmp_marshal_memsave,
    },

SQMP
memsave
-------

Save to disk virtual memory dump starting at 'val' of size 'size'.

Arguments:

- "val": the starting address (json-int)
- "size": the memory size, in bytes (json-int)
- "filename": file path (json-string)
- "cpu": virtual CPU index (json-int, optional)

Example:

-> { "execute": "memsave",
             "arguments": { "val": 10,
                            "size": 100,
                            "filename": "/tmp/virtual-mem-dump" } }
<- { "return": {} }

EQMP

    {
        .name       = "pmemsave",
        .args_type  = "val:l,size:i,filename:s",
        .mhandler.cmd_new = qmp_marshal_pmemsave,
    },

SQMP
pmemsave
--------

Save to disk physical memory dump starting at 'val' of size 'size'.

Arguments:

- "val": the starting address (json-int)
- "size": the memory size, in bytes (json-int)
- "filename": file path (json-string)

Example:

-> { "execute": "pmemsave",
             "arguments": { "val": 10,
                            "size": 100,
                            "filename": "/tmp/physical-mem-dump" } }
<- { "return": {} }

EQMP

    {
        .name       = "inject-nmi",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_inject_nmi,
    },

SQMP
inject-nmi
----------

Inject an NMI on the default CPU (x86/s390) or all CPUs (ppc64).

Arguments: None.

Example:

-> { "execute": "inject-nmi" }
<- { "return": {} }

Note: inject-nmi fails when the guest doesn't support injecting.

EQMP

    {
        .name       = "ringbuf-write",
        .args_type  = "device:s,data:s,format:s?",
        .mhandler.cmd_new = qmp_marshal_ringbuf_write,
    },

SQMP
ringbuf-write
-------------

Write to a ring buffer character device.

Arguments:

- "device": ring buffer character device name (json-string)
- "data": data to write (json-string)
- "format": data format (json-string, optional)
          - Possible values: "utf8" (default), "base64"

Example:

-> { "execute": "ringbuf-write",
                "arguments": { "device": "foo",
                               "data": "abcdefgh",
                               "format": "utf8" } }
<- { "return": {} }

EQMP

    {
        .name       = "ringbuf-read",
        .args_type  = "device:s,size:i,format:s?",
        .mhandler.cmd_new = qmp_marshal_ringbuf_read,
    },

SQMP
ringbuf-read
-------------

Read from a ring buffer character device.

Arguments:

- "device": ring buffer character device name (json-string)
- "size": how many bytes to read at most (json-int)
          - Number of data bytes, not number of characters in encoded data
- "format": data format (json-string, optional)
          - Possible values: "utf8" (default), "base64"
          - Naturally, format "utf8" works only when the ring buffer
            contains valid UTF-8 text.  Invalid UTF-8 sequences get
            replaced.  Bug: replacement doesn't work.  Bug: can screw
            up on encountering NUL characters, after the ring buffer
            lost data, and when reading stops because the size limit
            is reached.

Example:

-> { "execute": "ringbuf-read",
                "arguments": { "device": "foo",
                               "size": 1000,
                               "format": "utf8" } }
<- {"return": "abcdefgh"}

EQMP

    {
        .name       = "xen-save-devices-state",
        .args_type  = "filename:F",
    .mhandler.cmd_new = qmp_marshal_xen_save_devices_state,
    },

SQMP
xen-save-devices-state
-------

Save the state of all devices to file. The RAM and the block devices
of the VM are not saved by this command.

Arguments:

- "filename": the file to save the state of the devices to as binary
data. See xen-save-devices-state.txt for a description of the binary
format.

Example:

-> { "execute": "xen-save-devices-state",
     "arguments": { "filename": "/tmp/save" } }
<- { "return": {} }

EQMP

    {
        .name       = "xen-set-global-dirty-log",
        .args_type  = "enable:b",
        .mhandler.cmd_new = qmp_marshal_xen_set_global_dirty_log,
    },

SQMP
xen-set-global-dirty-log
-------

Enable or disable the global dirty log mode.

Arguments:

- "enable": Enable it or disable it.

Example:

-> { "execute": "xen-set-global-dirty-log",
     "arguments": { "enable": true } }
<- { "return": {} }

EQMP

    {
        .name       = "migrate",
        .args_type  = "detach:-d,blk:-b,inc:-i,uri:s",
        .mhandler.cmd_new = qmp_marshal_migrate,
    },

SQMP
migrate
-------

Migrate to URI.

Arguments:

- "blk": block migration, full disk copy (json-bool, optional)
- "inc": incremental disk copy (json-bool, optional)
- "uri": Destination URI (json-string)

Example:

-> { "execute": "migrate", "arguments": { "uri": "tcp:0:4446" } }
<- { "return": {} }

Notes:

(1) The 'query-migrate' command should be used to check migration's progress
    and final result (this information is provided by the 'status' member)
(2) All boolean arguments default to false
(3) The user Monitor's "detach" argument is invalid in QMP and should not
    be used

EQMP

    {
        .name       = "migrate_cancel",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_migrate_cancel,
    },

SQMP
migrate_cancel
--------------

Cancel the current migration.

Arguments: None.

Example:

-> { "execute": "migrate_cancel" }
<- { "return": {} }

EQMP

    {
        .name       = "migrate-incoming",
        .args_type  = "uri:s",
        .mhandler.cmd_new = qmp_marshal_migrate_incoming,
    },

SQMP
migrate-incoming
----------------

Continue an incoming migration

Arguments:

- "uri": Source/listening URI (json-string)

Example:

-> { "execute": "migrate-incoming", "arguments": { "uri": "tcp::4446" } }
<- { "return": {} }

Notes:

(1) QEMU must be started with -incoming defer to allow migrate-incoming to
    be used
(2) The uri format is the same as for -incoming

EQMP
    {
        .name       = "migrate-set-cache-size",
        .args_type  = "value:o",
        .mhandler.cmd_new = qmp_marshal_migrate_set_cache_size,
    },

SQMP
migrate-set-cache-size
----------------------

Set cache size to be used by XBZRLE migration, the cache size will be rounded
down to the nearest power of 2

Arguments:

- "value": cache size in bytes (json-int)

Example:

-> { "execute": "migrate-set-cache-size", "arguments": { "value": 536870912 } }
<- { "return": {} }

EQMP
    {
        .name       = "migrate-start-postcopy",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_migrate_start_postcopy,
    },

SQMP
migrate-start-postcopy
----------------------

Switch an in-progress migration to postcopy mode. Ignored after the end of
migration (or once already in postcopy).

Example:
-> { "execute": "migrate-start-postcopy" }
<- { "return": {} }

EQMP

    {
        .name       = "query-migrate-cache-size",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_migrate_cache_size,
    },

SQMP
query-migrate-cache-size
------------------------

Show cache size to be used by XBZRLE migration

returns a json-object with the following information:
- "size" : json-int

Example:

-> { "execute": "query-migrate-cache-size" }
<- { "return": 67108864 }

EQMP

    {
        .name       = "migrate_set_speed",
        .args_type  = "value:o",
        .mhandler.cmd_new = qmp_marshal_migrate_set_speed,
    },

SQMP
migrate_set_speed
-----------------

Set maximum speed for migrations.

Arguments:

- "value": maximum speed, in bytes per second (json-int)

Example:

-> { "execute": "migrate_set_speed", "arguments": { "value": 1024 } }
<- { "return": {} }

EQMP

    {
        .name       = "migrate_set_downtime",
        .args_type  = "value:T",
        .mhandler.cmd_new = qmp_marshal_migrate_set_downtime,
    },

SQMP
migrate_set_downtime
--------------------

Set maximum tolerated downtime (in seconds) for migrations.

Arguments:

- "value": maximum downtime (json-number)

Example:

-> { "execute": "migrate_set_downtime", "arguments": { "value": 0.1 } }
<- { "return": {} }

EQMP

    {
        .name       = "client_migrate_info",
        .args_type  = "protocol:s,hostname:s,port:i?,tls-port:i?,cert-subject:s?",
        .params     = "protocol hostname port tls-port cert-subject",
        .help       = "set migration information for remote display",
        .mhandler.cmd_new = qmp_marshal_client_migrate_info,
    },

SQMP
client_migrate_info
-------------------

Set migration information for remote display.  This makes the server
ask the client to automatically reconnect using the new parameters
once migration finished successfully.  Only implemented for SPICE.

Arguments:

- "protocol":     must be "spice" (json-string)
- "hostname":     migration target hostname (json-string)
- "port":         spice tcp port for plaintext channels (json-int, optional)
- "tls-port":     spice tcp port for tls-secured channels (json-int, optional)
- "cert-subject": server certificate subject (json-string, optional)

Example:

-> { "execute": "client_migrate_info",
     "arguments": { "protocol": "spice",
                    "hostname": "virt42.lab.kraxel.org",
                    "port": 1234 } }
<- { "return": {} }

EQMP

    {
        .name       = "dump-guest-memory",
        .args_type  = "paging:b,protocol:s,begin:i?,end:i?,format:s?",
        .params     = "-p protocol [begin] [length] [format]",
        .help       = "dump guest memory to file",
        .mhandler.cmd_new = qmp_marshal_dump_guest_memory,
    },

SQMP
dump


Dump guest memory to file. The file can be processed with crash or gdb.

Arguments:

- "paging": do paging to get guest's memory mapping (json-bool)
- "protocol": destination file(started with "file:") or destination file
              descriptor (started with "fd:") (json-string)
- "begin": the starting physical address. It's optional, and should be specified
           with length together (json-int)
- "length": the memory size, in bytes. It's optional, and should be specified
            with begin together (json-int)
- "format": the format of guest memory dump. It's optional, and can be
            elf|kdump-zlib|kdump-lzo|kdump-snappy, but non-elf formats will
            conflict with paging and filter, ie. begin and length (json-string)

Example:

-> { "execute": "dump-guest-memory", "arguments": { "protocol": "fd:dump" } }
<- { "return": {} }

Notes:

(1) All boolean arguments default to false

EQMP

    {
        .name       = "query-dump-guest-memory-capability",
        .args_type  = "",
    .mhandler.cmd_new = qmp_marshal_query_dump_guest_memory_capability,
    },

SQMP
query-dump-guest-memory-capability
----------

Show available formats for 'dump-guest-memory'

Example:

-> { "execute": "query-dump-guest-memory-capability" }
<- { "return": { "formats":
                    ["elf", "kdump-zlib", "kdump-lzo", "kdump-snappy"] }

EQMP

#if defined TARGET_S390X
    {
        .name       = "dump-skeys",
        .args_type  = "filename:F",
        .mhandler.cmd_new = qmp_marshal_dump_skeys,
    },
#endif

SQMP
dump-skeys
----------

Save guest storage keys to file.

Arguments:

- "filename": file path (json-string)

Example:

-> { "execute": "dump-skeys", "arguments": { "filename": "/tmp/skeys" } }
<- { "return": {} }

EQMP

    {
        .name       = "netdev_add",
        .args_type  = "netdev:O",
        .mhandler.cmd_new = qmp_netdev_add,
    },

SQMP
netdev_add
----------

Add host network device.

Arguments:

- "type": the device type, "tap", "user", ... (json-string)
- "id": the device's ID, must be unique (json-string)
- device options

Example:

-> { "execute": "netdev_add",
     "arguments": { "type": "user", "id": "netdev1",
                    "dnssearch": "example.org" } }
<- { "return": {} }

Note: The supported device options are the same ones supported by the '-netdev'
      command-line argument, which are listed in the '-help' output or QEMU's
      manual

EQMP

    {
        .name       = "netdev_del",
        .args_type  = "id:s",
        .mhandler.cmd_new = qmp_marshal_netdev_del,
    },

SQMP
netdev_del
----------

Remove host network device.

Arguments:

- "id": the device's ID, must be unique (json-string)

Example:

-> { "execute": "netdev_del", "arguments": { "id": "netdev1" } }
<- { "return": {} }


EQMP

    {
        .name       = "object-add",
        .args_type  = "qom-type:s,id:s,props:q?",
        .mhandler.cmd_new = qmp_marshal_object_add,
    },

SQMP
object-add
----------

Create QOM object.

Arguments:

- "qom-type": the object's QOM type, i.e. the class name (json-string)
- "id": the object's ID, must be unique (json-string)
- "props": a dictionary of object property values (optional, json-dict)

Example:

-> { "execute": "object-add", "arguments": { "qom-type": "rng-random", "id": "rng1",
     "props": { "filename": "/dev/hwrng" } } }
<- { "return": {} }

EQMP

    {
        .name       = "object-del",
        .args_type  = "id:s",
        .mhandler.cmd_new = qmp_marshal_object_del,
    },

SQMP
object-del
----------

Remove QOM object.

Arguments:

- "id": the object's ID (json-string)

Example:

-> { "execute": "object-del", "arguments": { "id": "rng1" } }
<- { "return": {} }


EQMP


    {
        .name       = "block_resize",
        .args_type  = "device:s?,node-name:s?,size:o",
        .mhandler.cmd_new = qmp_marshal_block_resize,
    },

SQMP
block_resize
------------

Resize a block image while a guest is running.

Arguments:

- "device": the device's ID, must be unique (json-string)
- "node-name": the node name in the block driver state graph (json-string)
- "size": new size

Example:

-> { "execute": "block_resize", "arguments": { "device": "scratch", "size": 1073741824 } }
<- { "return": {} }

EQMP

    {
        .name       = "block-stream",
        .args_type  = "device:B,base:s?,speed:o?,backing-file:s?,on-error:s?",
        .mhandler.cmd_new = qmp_marshal_block_stream,
    },

SQMP
block-stream
------------

Copy data from a backing file into a block device.

Arguments:

- "device": The device's ID, must be unique (json-string)
- "base": The file name of the backing image above which copying starts
          (json-string, optional)
- "backing-file": The backing file string to write into the active layer. This
                  filename is not validated.

                  If a pathname string is such that it cannot be resolved by
                  QEMU, that means that subsequent QMP or HMP commands must use
                  node-names for the image in question, as filename lookup
                  methods will fail.

                  If not specified, QEMU will automatically determine the
                  backing file string to use, or error out if there is no
                  obvious choice.  Care should be taken when specifying the
                  string, to specify a valid filename or protocol.
                  (json-string, optional) (Since 2.1)
- "speed":  the maximum speed, in bytes per second (json-int, optional)
- "on-error": the action to take on an error (default 'report').  'stop' and
              'enospc' can only be used if the block device supports io-status.
              (json-string, optional) (Since 2.1)

Example:

-> { "execute": "block-stream", "arguments": { "device": "virtio0",
                                               "base": "/tmp/master.qcow2" } }
<- { "return": {} }

EQMP

    {
        .name       = "block-commit",
        .args_type  = "device:B,base:s?,top:s?,backing-file:s?,speed:o?",
        .mhandler.cmd_new = qmp_marshal_block_commit,
    },

SQMP
block-commit
------------

Live commit of data from overlay image nodes into backing nodes - i.e., writes
data between 'top' and 'base' into 'base'.

Arguments:

- "device": The device's ID, must be unique (json-string)
- "base": The file name of the backing image to write data into.
          If not specified, this is the deepest backing image
          (json-string, optional)
- "top":  The file name of the backing image within the image chain,
          which contains the topmost data to be committed down. If
          not specified, this is the active layer. (json-string, optional)

- backing-file:     The backing file string to write into the overlay
                    image of 'top'.  If 'top' is the active layer,
                    specifying a backing file string is an error. This
                    filename is not validated.

                    If a pathname string is such that it cannot be
                    resolved by QEMU, that means that subsequent QMP or
                    HMP commands must use node-names for the image in
                    question, as filename lookup methods will fail.

                    If not specified, QEMU will automatically determine
                    the backing file string to use, or error out if
                    there is no obvious choice. Care should be taken
                    when specifying the string, to specify a valid
                    filename or protocol.
                    (json-string, optional) (Since 2.1)

          If top == base, that is an error.
          If top == active, the job will not be completed by itself,
          user needs to complete the job with the block-job-complete
          command after getting the ready event. (Since 2.0)

          If the base image is smaller than top, then the base image
          will be resized to be the same size as top.  If top is
          smaller than the base image, the base will not be
          truncated.  If you want the base image size to match the
          size of the smaller top, you can safely truncate it
          yourself once the commit operation successfully completes.
          (json-string)
- "speed":  the maximum speed, in bytes per second (json-int, optional)


Example:

-> { "execute": "block-commit", "arguments": { "device": "virtio0",
                                              "top": "/tmp/snap1.qcow2" } }
<- { "return": {} }

EQMP

    {
        .name       = "drive-backup",
        .args_type  = "sync:s,device:B,target:s,speed:i?,mode:s?,format:s?,"
                      "bitmap:s?,on-source-error:s?,on-target-error:s?",
        .mhandler.cmd_new = qmp_marshal_drive_backup,
    },

SQMP
drive-backup
------------

Start a point-in-time copy of a block device to a new destination.  The
status of ongoing drive-backup operations can be checked with
query-block-jobs where the BlockJobInfo.type field has the value 'backup'.
The operation can be stopped before it has completed using the
block-job-cancel command.

Arguments:

- "device": the name of the device which should be copied.
            (json-string)
- "target": the target of the new image. If the file exists, or if it is a
            device, the existing file/device will be used as the new
            destination.  If it does not exist, a new file will be created.
            (json-string)
- "format": the format of the new destination, default is to probe if 'mode' is
            'existing', else the format of the source
            (json-string, optional)
- "sync": what parts of the disk image should be copied to the destination;
  possibilities include "full" for all the disk, "top" for only the sectors
  allocated in the topmost image, "incremental" for only the dirty sectors in
  the bitmap, or "none" to only replicate new I/O (MirrorSyncMode).
- "bitmap": dirty bitmap name for sync==incremental. Must be present if sync
            is "incremental", must NOT be present otherwise.
- "mode": whether and how QEMU should create a new image
          (NewImageMode, optional, default 'absolute-paths')
- "speed": the maximum speed, in bytes per second (json-int, optional)
- "on-source-error": the action to take on an error on the source, default
                     'report'.  'stop' and 'enospc' can only be used
                     if the block device supports io-status.
                     (BlockdevOnError, optional)
- "on-target-error": the action to take on an error on the target, default
                     'report' (no limitations, since this applies to
                     a different block device than device).
                     (BlockdevOnError, optional)

Example:
-> { "execute": "drive-backup", "arguments": { "device": "drive0",
                                               "sync": "full",
                                               "target": "backup.img" } }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-backup",
        .args_type  = "sync:s,device:B,target:B,speed:i?,"
                      "on-source-error:s?,on-target-error:s?",
        .mhandler.cmd_new = qmp_marshal_blockdev_backup,
    },

SQMP
blockdev-backup
---------------

The device version of drive-backup: this command takes an existing named device
as backup target.

Arguments:

- "device": the name of the device which should be copied.
            (json-string)
- "target": the name of the backup target device. (json-string)
- "sync": what parts of the disk image should be copied to the destination;
          possibilities include "full" for all the disk, "top" for only the
          sectors allocated in the topmost image, or "none" to only replicate
          new I/O (MirrorSyncMode).
- "speed": the maximum speed, in bytes per second (json-int, optional)
- "on-source-error": the action to take on an error on the source, default
                     'report'.  'stop' and 'enospc' can only be used
                     if the block device supports io-status.
                     (BlockdevOnError, optional)
- "on-target-error": the action to take on an error on the target, default
                     'report' (no limitations, since this applies to
                     a different block device than device).
                     (BlockdevOnError, optional)

Example:
-> { "execute": "blockdev-backup", "arguments": { "device": "src-id",
                                                  "sync": "full",
                                                  "target": "tgt-id" } }
<- { "return": {} }

EQMP

    {
        .name       = "block-job-set-speed",
        .args_type  = "device:B,speed:o",
        .mhandler.cmd_new = qmp_marshal_block_job_set_speed,
    },

    {
        .name       = "block-job-cancel",
        .args_type  = "device:B,force:b?",
        .mhandler.cmd_new = qmp_marshal_block_job_cancel,
    },
    {
        .name       = "block-job-pause",
        .args_type  = "device:B",
        .mhandler.cmd_new = qmp_marshal_block_job_pause,
    },
    {
        .name       = "block-job-resume",
        .args_type  = "device:B",
        .mhandler.cmd_new = qmp_marshal_block_job_resume,
    },
    {
        .name       = "block-job-complete",
        .args_type  = "device:B",
        .mhandler.cmd_new = qmp_marshal_block_job_complete,
    },
    {
        .name       = "transaction",
        .args_type  = "actions:q,properties:q?",
        .mhandler.cmd_new = qmp_marshal_transaction,
    },

SQMP
transaction
-----------

Atomically operate on one or more block devices.  Operations that are
currently supported:

    - drive-backup
    - blockdev-backup
    - blockdev-snapshot-sync
    - blockdev-snapshot-internal-sync
    - abort
    - block-dirty-bitmap-add
    - block-dirty-bitmap-clear

Refer to the qemu/qapi-schema.json file for minimum required QEMU
versions for these operations.  A list of dictionaries is accepted,
that contains the actions to be performed.  If there is any failure
performing any of the operations, all operations for the group are
abandoned.

For external snapshots, the dictionary contains the device, the file to use for
the new snapshot, and the format.  The default format, if not specified, is
qcow2.

Each new snapshot defaults to being created by QEMU (wiping any
contents if the file already exists), but it is also possible to reuse
an externally-created file.  In the latter case, you should ensure that
the new image file has the same contents as the current one; QEMU cannot
perform any meaningful check.  Typically this is achieved by using the
current image file as the backing file for the new image.

On failure, the original disks pre-snapshot attempt will be used.

For internal snapshots, the dictionary contains the device and the snapshot's
name.  If an internal snapshot matching name already exists, the request will
be rejected.  Only some image formats support it, for example, qcow2, rbd,
and sheepdog.

On failure, qemu will try delete the newly created internal snapshot in the
transaction.  When an I/O error occurs during deletion, the user needs to fix
it later with qemu-img or other command.

Arguments:

actions array:
    - "type": the operation to perform (json-string).  Possible
              values: "drive-backup", "blockdev-backup",
                      "blockdev-snapshot-sync",
                      "blockdev-snapshot-internal-sync",
                      "abort", "block-dirty-bitmap-add",
                      "block-dirty-bitmap-clear"
    - "data": a dictionary.  The contents depend on the value
      of "type".  When "type" is "blockdev-snapshot-sync":
      - "device": device name to snapshot (json-string)
      - "node-name": graph node name to snapshot (json-string)
      - "snapshot-file": name of new image file (json-string)
      - "snapshot-node-name": graph node name of the new snapshot (json-string)
      - "format": format of new image (json-string, optional)
      - "mode": whether and how QEMU should create the snapshot file
        (NewImageMode, optional, default "absolute-paths")
      When "type" is "blockdev-snapshot-internal-sync":
      - "device": device name to snapshot (json-string)
      - "name": name of the new snapshot (json-string)

Example:

-> { "execute": "transaction",
     "arguments": { "actions": [
         { "type": "blockdev-snapshot-sync", "data" : { "device": "ide-hd0",
                                         "snapshot-file": "/some/place/my-image",
                                         "format": "qcow2" } },
         { "type": "blockdev-snapshot-sync", "data" : { "node-name": "myfile",
                                         "snapshot-file": "/some/place/my-image2",
                                         "snapshot-node-name": "node3432",
                                         "mode": "existing",
                                         "format": "qcow2" } },
         { "type": "blockdev-snapshot-sync", "data" : { "device": "ide-hd1",
                                         "snapshot-file": "/some/place/my-image2",
                                         "mode": "existing",
                                         "format": "qcow2" } },
         { "type": "blockdev-snapshot-internal-sync", "data" : {
                                         "device": "ide-hd2",
                                         "name": "snapshot0" } } ] } }
<- { "return": {} }

EQMP

    {
        .name       = "block-dirty-bitmap-add",
        .args_type  = "node:B,name:s,granularity:i?",
        .mhandler.cmd_new = qmp_marshal_block_dirty_bitmap_add,
    },

SQMP

block-dirty-bitmap-add
----------------------
Since 2.4

Create a dirty bitmap with a name on the device, and start tracking the writes.

Arguments:

- "node": device/node on which to create dirty bitmap (json-string)
- "name": name of the new dirty bitmap (json-string)
- "granularity": granularity to track writes with (int, optional)

Example:

-> { "execute": "block-dirty-bitmap-add", "arguments": { "node": "drive0",
                                                   "name": "bitmap0" } }
<- { "return": {} }

EQMP

    {
        .name       = "block-dirty-bitmap-remove",
        .args_type  = "node:B,name:s",
        .mhandler.cmd_new = qmp_marshal_block_dirty_bitmap_remove,
    },

SQMP

block-dirty-bitmap-remove
-------------------------
Since 2.4

Stop write tracking and remove the dirty bitmap that was created with
block-dirty-bitmap-add.

Arguments:

- "node": device/node on which to remove dirty bitmap (json-string)
- "name": name of the dirty bitmap to remove (json-string)

Example:

-> { "execute": "block-dirty-bitmap-remove", "arguments": { "node": "drive0",
                                                      "name": "bitmap0" } }
<- { "return": {} }

EQMP

    {
        .name       = "block-dirty-bitmap-clear",
        .args_type  = "node:B,name:s",
        .mhandler.cmd_new = qmp_marshal_block_dirty_bitmap_clear,
    },

SQMP

block-dirty-bitmap-clear
------------------------
Since 2.4

Reset the dirty bitmap associated with a node so that an incremental backup
from this point in time forward will only backup clusters modified after this
clear operation.

Arguments:

- "node": device/node on which to remove dirty bitmap (json-string)
- "name": name of the dirty bitmap to remove (json-string)

Example:

-> { "execute": "block-dirty-bitmap-clear", "arguments": { "node": "drive0",
                                                           "name": "bitmap0" } }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-snapshot-sync",
        .args_type  = "device:s?,node-name:s?,snapshot-file:s,snapshot-node-name:s?,format:s?,mode:s?",
        .mhandler.cmd_new = qmp_marshal_blockdev_snapshot_sync,
    },

SQMP
blockdev-snapshot-sync
----------------------

Synchronous snapshot of a block device. snapshot-file specifies the
target of the new image. If the file exists, or if it is a device, the
snapshot will be created in the existing file/device. If does not
exist, a new file will be created. format specifies the format of the
snapshot image, default is qcow2.

Arguments:

- "device": device name to snapshot (json-string)
- "node-name": graph node name to snapshot (json-string)
- "snapshot-file": name of new image file (json-string)
- "snapshot-node-name": graph node name of the new snapshot (json-string)
- "mode": whether and how QEMU should create the snapshot file
  (NewImageMode, optional, default "absolute-paths")
- "format": format of new image (json-string, optional)

Example:

-> { "execute": "blockdev-snapshot-sync", "arguments": { "device": "ide-hd0",
                                                         "snapshot-file":
                                                        "/some/place/my-image",
                                                        "format": "qcow2" } }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-snapshot",
        .args_type  = "node:s,overlay:s",
        .mhandler.cmd_new = qmp_marshal_blockdev_snapshot,
    },

SQMP
blockdev-snapshot
-----------------
Since 2.5

Create a snapshot, by installing 'node' as the backing image of
'overlay'. Additionally, if 'node' is associated with a block
device, the block device changes to using 'overlay' as its new active
image.

Arguments:

- "node": device that will have a snapshot created (json-string)
- "overlay": device that will have 'node' as its backing image (json-string)

Example:

-> { "execute": "blockdev-add",
                "arguments": { "options": { "driver": "qcow2",
                                            "node-name": "node1534",
                                            "file": { "driver": "file",
                                                      "filename": "hd1.qcow2" },
                                            "backing": "" } } }

<- { "return": {} }

-> { "execute": "blockdev-snapshot", "arguments": { "node": "ide-hd0",
                                                    "overlay": "node1534" } }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-snapshot-internal-sync",
        .args_type  = "device:B,name:s",
        .mhandler.cmd_new = qmp_marshal_blockdev_snapshot_internal_sync,
    },

SQMP
blockdev-snapshot-internal-sync
-------------------------------

Synchronously take an internal snapshot of a block device when the format of
image used supports it.  If the name is an empty string, or a snapshot with
name already exists, the operation will fail.

Arguments:

- "device": device name to snapshot (json-string)
- "name": name of the new snapshot (json-string)

Example:

-> { "execute": "blockdev-snapshot-internal-sync",
                "arguments": { "device": "ide-hd0",
                               "name": "snapshot0" }
   }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-snapshot-delete-internal-sync",
        .args_type  = "device:B,id:s?,name:s?",
        .mhandler.cmd_new =
                      qmp_marshal_blockdev_snapshot_delete_internal_sync,
    },

SQMP
blockdev-snapshot-delete-internal-sync
--------------------------------------

Synchronously delete an internal snapshot of a block device when the format of
image used supports it.  The snapshot is identified by name or id or both.  One
of name or id is required.  If the snapshot is not found, the operation will
fail.

Arguments:

- "device": device name (json-string)
- "id": ID of the snapshot (json-string, optional)
- "name": name of the snapshot (json-string, optional)

Example:

-> { "execute": "blockdev-snapshot-delete-internal-sync",
                "arguments": { "device": "ide-hd0",
                               "name": "snapshot0" }
   }
<- { "return": {
                   "id": "1",
                   "name": "snapshot0",
                   "vm-state-size": 0,
                   "date-sec": 1000012,
                   "date-nsec": 10,
                   "vm-clock-sec": 100,
                   "vm-clock-nsec": 20
     }
   }

EQMP

    {
        .name       = "drive-mirror",
        .args_type  = "sync:s,device:B,target:s,speed:i?,mode:s?,format:s?,"
                      "node-name:s?,replaces:s?,"
                      "on-source-error:s?,on-target-error:s?,"
                      "unmap:b?,"
                      "granularity:i?,buf-size:i?",
        .mhandler.cmd_new = qmp_marshal_drive_mirror,
    },

SQMP
drive-mirror
------------

Start mirroring a block device's writes to a new destination. target
specifies the target of the new image. If the file exists, or if it is
a device, it will be used as the new destination for writes. If it does not
exist, a new file will be created. format specifies the format of the
mirror image, default is to probe if mode='existing', else the format
of the source.

Arguments:

- "device": device name to operate on (json-string)
- "target": name of new image file (json-string)
- "format": format of new image (json-string, optional)
- "node-name": the name of the new block driver state in the node graph
               (json-string, optional)
- "replaces": the block driver node name to replace when finished
              (json-string, optional)
- "mode": how an image file should be created into the target
  file/device (NewImageMode, optional, default 'absolute-paths')
- "speed": maximum speed of the streaming job, in bytes per second
  (json-int)
- "granularity": granularity of the dirty bitmap, in bytes (json-int, optional)
- "buf-size": maximum amount of data in flight from source to target, in bytes
  (json-int, default 10M)
- "sync": what parts of the disk image should be copied to the destination;
  possibilities include "full" for all the disk, "top" for only the sectors
  allocated in the topmost image, or "none" to only replicate new I/O
  (MirrorSyncMode).
- "on-source-error": the action to take on an error on the source
  (BlockdevOnError, default 'report')
- "on-target-error": the action to take on an error on the target
  (BlockdevOnError, default 'report')
- "unmap": whether the target sectors should be discarded where source has only
  zeroes. (json-bool, optional, default true)

The default value of the granularity is the image cluster size clamped
between 4096 and 65536, if the image format defines one.  If the format
does not define a cluster size, the default value of the granularity
is 65536.


Example:

-> { "execute": "drive-mirror", "arguments": { "device": "ide-hd0",
                                               "target": "/some/place/my-image",
                                               "sync": "full",
                                               "format": "qcow2" } }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-mirror",
        .args_type  = "sync:s,device:B,target:B,replaces:s?,speed:i?,"
                      "on-source-error:s?,on-target-error:s?,"
                      "granularity:i?,buf-size:i?",
        .mhandler.cmd_new = qmp_marshal_blockdev_mirror,
    },

SQMP
blockdev-mirror
------------

Start mirroring a block device's writes to another block device. target
specifies the target of mirror operation.

Arguments:

- "device": device name to operate on (json-string)
- "target": device name to mirror to (json-string)
- "replaces": the block driver node name to replace when finished
              (json-string, optional)
- "speed": maximum speed of the streaming job, in bytes per second
  (json-int)
- "granularity": granularity of the dirty bitmap, in bytes (json-int, optional)
- "buf_size": maximum amount of data in flight from source to target, in bytes
  (json-int, default 10M)
- "sync": what parts of the disk image should be copied to the destination;
  possibilities include "full" for all the disk, "top" for only the sectors
  allocated in the topmost image, or "none" to only replicate new I/O
  (MirrorSyncMode).
- "on-source-error": the action to take on an error on the source
  (BlockdevOnError, default 'report')
- "on-target-error": the action to take on an error on the target
  (BlockdevOnError, default 'report')

The default value of the granularity is the image cluster size clamped
between 4096 and 65536, if the image format defines one.  If the format
does not define a cluster size, the default value of the granularity
is 65536.

Example:

-> { "execute": "blockdev-mirror", "arguments": { "device": "ide-hd0",
                                                  "target": "target0",
                                                  "sync": "full" } }
<- { "return": {} }

EQMP
    {
        .name       = "change-backing-file",
        .args_type  = "device:s,image-node-name:s,backing-file:s",
        .mhandler.cmd_new = qmp_marshal_change_backing_file,
    },

SQMP
change-backing-file
-------------------
Since: 2.1

Change the backing file in the image file metadata.  This does not cause
QEMU to reopen the image file to reparse the backing filename (it may,
however, perform a reopen to change permissions from r/o -> r/w -> r/o,
if needed). The new backing file string is written into the image file
metadata, and the QEMU internal strings are updated.

Arguments:

- "image-node-name":    The name of the block driver state node of the
                        image to modify.  The "device" is argument is used to
                        verify "image-node-name" is in the chain described by
                        "device".
                        (json-string, optional)

- "device":             The name of the device.
                        (json-string)

- "backing-file":       The string to write as the backing file.  This string is
                        not validated, so care should be taken when specifying
                        the string or the image chain may not be able to be
                        reopened again.
                        (json-string)

Returns: Nothing on success
         If "device" does not exist or cannot be determined, DeviceNotFound

EQMP

    {
        .name       = "balloon",
        .args_type  = "value:M",
        .mhandler.cmd_new = qmp_marshal_balloon,
    },

SQMP
balloon
-------

Request VM to change its memory allocation (in bytes).

Arguments:

- "value": New memory allocation (json-int)

Example:

-> { "execute": "balloon", "arguments": { "value": 536870912 } }
<- { "return": {} }

EQMP

    {
        .name       = "set_link",
        .args_type  = "name:s,up:b",
        .mhandler.cmd_new = qmp_marshal_set_link,
    },

SQMP
set_link
--------

Change the link status of a network adapter.

Arguments:

- "name": network device name (json-string)
- "up": status is up (json-bool)

Example:

-> { "execute": "set_link", "arguments": { "name": "e1000.0", "up": false } }
<- { "return": {} }

EQMP

    {
        .name       = "getfd",
        .args_type  = "fdname:s",
        .params     = "getfd name",
        .help       = "receive a file descriptor via SCM rights and assign it a name",
        .mhandler.cmd_new = qmp_marshal_getfd,
    },

SQMP
getfd
-----

Receive a file descriptor via SCM rights and assign it a name.

Arguments:

- "fdname": file descriptor name (json-string)

Example:

-> { "execute": "getfd", "arguments": { "fdname": "fd1" } }
<- { "return": {} }

Notes:

(1) If the name specified by the "fdname" argument already exists,
    the file descriptor assigned to it will be closed and replaced
    by the received file descriptor.
(2) The 'closefd' command can be used to explicitly close the file
    descriptor when it is no longer needed.

EQMP

    {
        .name       = "closefd",
        .args_type  = "fdname:s",
        .params     = "closefd name",
        .help       = "close a file descriptor previously passed via SCM rights",
        .mhandler.cmd_new = qmp_marshal_closefd,
    },

SQMP
closefd
-------

Close a file descriptor previously passed via SCM rights.

Arguments:

- "fdname": file descriptor name (json-string)

Example:

-> { "execute": "closefd", "arguments": { "fdname": "fd1" } }
<- { "return": {} }

EQMP

     {
        .name       = "add-fd",
        .args_type  = "fdset-id:i?,opaque:s?",
        .params     = "add-fd fdset-id opaque",
        .help       = "Add a file descriptor, that was passed via SCM rights, to an fd set",
        .mhandler.cmd_new = qmp_marshal_add_fd,
    },

SQMP
add-fd
-------

Add a file descriptor, that was passed via SCM rights, to an fd set.

Arguments:

- "fdset-id": The ID of the fd set to add the file descriptor to.
              (json-int, optional)
- "opaque": A free-form string that can be used to describe the fd.
            (json-string, optional)

Return a json-object with the following information:

- "fdset-id": The ID of the fd set that the fd was added to. (json-int)
- "fd": The file descriptor that was received via SCM rights and added to the
        fd set. (json-int)

Example:

-> { "execute": "add-fd", "arguments": { "fdset-id": 1 } }
<- { "return": { "fdset-id": 1, "fd": 3 } }

Notes:

(1) The list of fd sets is shared by all monitor connections.
(2) If "fdset-id" is not specified, a new fd set will be created.

EQMP

     {
        .name       = "remove-fd",
        .args_type  = "fdset-id:i,fd:i?",
        .params     = "remove-fd fdset-id fd",
        .help       = "Remove a file descriptor from an fd set",
        .mhandler.cmd_new = qmp_marshal_remove_fd,
    },

SQMP
remove-fd
---------

Remove a file descriptor from an fd set.

Arguments:

- "fdset-id": The ID of the fd set that the file descriptor belongs to.
              (json-int)
- "fd": The file descriptor that is to be removed. (json-int, optional)

Example:

-> { "execute": "remove-fd", "arguments": { "fdset-id": 1, "fd": 3 } }
<- { "return": {} }

Notes:

(1) The list of fd sets is shared by all monitor connections.
(2) If "fd" is not specified, all file descriptors in "fdset-id" will be
    removed.

EQMP

    {
        .name       = "query-fdsets",
        .args_type  = "",
        .help       = "Return information describing all fd sets",
        .mhandler.cmd_new = qmp_marshal_query_fdsets,
    },

SQMP
query-fdsets
-------------

Return information describing all fd sets.

Arguments: None

Example:

-> { "execute": "query-fdsets" }
<- { "return": [
       {
         "fds": [
           {
             "fd": 30,
             "opaque": "rdonly:/path/to/file"
           },
           {
             "fd": 24,
             "opaque": "rdwr:/path/to/file"
           }
         ],
         "fdset-id": 1
       },
       {
         "fds": [
           {
             "fd": 28
           },
           {
             "fd": 29
           }
         ],
         "fdset-id": 0
       }
     ]
   }

Note: The list of fd sets is shared by all monitor connections.

EQMP

    {
        .name       = "block_passwd",
        .args_type  = "device:s?,node-name:s?,password:s",
        .mhandler.cmd_new = qmp_marshal_block_passwd,
    },

SQMP
block_passwd
------------

Set the password of encrypted block devices.

Arguments:

- "device": device name (json-string)
- "node-name": name in the block driver state graph (json-string)
- "password": password (json-string)

Example:

-> { "execute": "block_passwd", "arguments": { "device": "ide0-hd0",
                                               "password": "12345" } }
<- { "return": {} }

EQMP

    {
        .name       = "block_set_io_throttle",
        .args_type  = "device:B,bps:l,bps_rd:l,bps_wr:l,iops:l,iops_rd:l,iops_wr:l,bps_max:l?,bps_rd_max:l?,bps_wr_max:l?,iops_max:l?,iops_rd_max:l?,iops_wr_max:l?,iops_size:l?,group:s?",
        .mhandler.cmd_new = qmp_marshal_block_set_io_throttle,
    },

SQMP
block_set_io_throttle
------------

Change I/O throttle limits for a block drive.

Arguments:

- "device": device name (json-string)
- "bps": total throughput limit in bytes per second (json-int)
- "bps_rd": read throughput limit in bytes per second (json-int)
- "bps_wr": write throughput limit in bytes per second (json-int)
- "iops": total I/O operations per second (json-int)
- "iops_rd": read I/O operations per second (json-int)
- "iops_wr": write I/O operations per second (json-int)
- "bps_max":  total max in bytes (json-int)
- "bps_rd_max":  read max in bytes (json-int)
- "bps_wr_max":  write max in bytes (json-int)
- "iops_max":  total I/O operations max (json-int)
- "iops_rd_max":  read I/O operations max (json-int)
- "iops_wr_max":  write I/O operations max (json-int)
- "iops_size":  I/O size in bytes when limiting (json-int)
- "group": throttle group name (json-string)

Example:

-> { "execute": "block_set_io_throttle", "arguments": { "device": "virtio0",
                                               "bps": 1000000,
                                               "bps_rd": 0,
                                               "bps_wr": 0,
                                               "iops": 0,
                                               "iops_rd": 0,
                                               "iops_wr": 0,
                                               "bps_max": 8000000,
                                               "bps_rd_max": 0,
                                               "bps_wr_max": 0,
                                               "iops_max": 0,
                                               "iops_rd_max": 0,
                                               "iops_wr_max": 0,
                                               "iops_size": 0 } }
<- { "return": {} }

EQMP

    {
        .name       = "set_password",
        .args_type  = "protocol:s,password:s,connected:s?",
        .mhandler.cmd_new = qmp_marshal_set_password,
    },

SQMP
set_password
------------

Set the password for vnc/spice protocols.

Arguments:

- "protocol": protocol name (json-string)
- "password": password (json-string)
- "connected": [ keep | disconnect | fail ] (json-string, optional)

Example:

-> { "execute": "set_password", "arguments": { "protocol": "vnc",
                                               "password": "secret" } }
<- { "return": {} }

EQMP

    {
        .name       = "expire_password",
        .args_type  = "protocol:s,time:s",
        .mhandler.cmd_new = qmp_marshal_expire_password,
    },

SQMP
expire_password
---------------

Set the password expire time for vnc/spice protocols.

Arguments:

- "protocol": protocol name (json-string)
- "time": [ now | never | +secs | secs ] (json-string)

Example:

-> { "execute": "expire_password", "arguments": { "protocol": "vnc",
                                                  "time": "+60" } }
<- { "return": {} }

EQMP

    {
        .name       = "add_client",
        .args_type  = "protocol:s,fdname:s,skipauth:b?,tls:b?",
        .mhandler.cmd_new = qmp_marshal_add_client,
    },

SQMP
add_client
----------

Add a graphics client

Arguments:

- "protocol": protocol name (json-string)
- "fdname": file descriptor name (json-string)
- "skipauth": whether to skip authentication (json-bool, optional)
- "tls": whether to perform TLS (json-bool, optional)

Example:

-> { "execute": "add_client", "arguments": { "protocol": "vnc",
                                             "fdname": "myclient" } }
<- { "return": {} }

EQMP
    {
        .name       = "qmp_capabilities",
        .args_type  = "",
        .params     = "",
        .help       = "enable QMP capabilities",
        .mhandler.cmd_new = qmp_capabilities,
    },

SQMP
qmp_capabilities
----------------

Enable QMP capabilities.

Arguments: None.

Example:

-> { "execute": "qmp_capabilities" }
<- { "return": {} }

Note: This command must be issued before issuing any other command.

EQMP

    {
        .name       = "human-monitor-command",
        .args_type  = "command-line:s,cpu-index:i?",
        .mhandler.cmd_new = qmp_marshal_human_monitor_command,
    },

SQMP
human-monitor-command
---------------------

Execute a Human Monitor command.

Arguments: 

- command-line: the command name and its arguments, just like the
                Human Monitor's shell (json-string)
- cpu-index: select the CPU number to be used by commands which access CPU
             data, like 'info registers'. The Monitor selects CPU 0 if this
             argument is not provided (json-int, optional)

Example:

-> { "execute": "human-monitor-command", "arguments": { "command-line": "info kvm" } }
<- { "return": "kvm support: enabled\r\n" }

Notes:

(1) The Human Monitor is NOT an stable interface, this means that command
    names, arguments and responses can change or be removed at ANY time.
    Applications that rely on long term stability guarantees should NOT
    use this command

(2) Limitations:

    o This command is stateless, this means that commands that depend
      on state information (such as getfd) might not work

    o Commands that prompt the user for data (eg. 'cont' when the block
      device is encrypted) don't currently work

3. Query Commands
=================

HXCOMM Each query command below is inside a SQMP/EQMP section, do NOT change
HXCOMM this! We will possibly move query commands definitions inside those
HXCOMM sections, just like regular commands.

EQMP

SQMP
query-version
-------------

Show QEMU version.

Return a json-object with the following information:

- "qemu": A json-object containing three integer values:
    - "major": QEMU's major version (json-int)
    - "minor": QEMU's minor version (json-int)
    - "micro": QEMU's micro version (json-int)
- "package": package's version (json-string)

Example:

-> { "execute": "query-version" }
<- {
      "return":{
         "qemu":{
            "major":0,
            "minor":11,
            "micro":5
         },
         "package":""
      }
   }

EQMP

    {
        .name       = "query-version",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_version,
    },

SQMP
query-commands
--------------

List QMP available commands.

Each command is represented by a json-object, the returned value is a json-array
of all commands.

Each json-object contain:

- "name": command's name (json-string)

Example:

-> { "execute": "query-commands" }
<- {
      "return":[
         {
            "name":"query-balloon"
         },
         {
            "name":"system_powerdown"
         }
      ]
   }

Note: This example has been shortened as the real response is too long.

EQMP

    {
        .name       = "query-commands",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_commands,
    },

SQMP
query-events
--------------

List QMP available events.

Each event is represented by a json-object, the returned value is a json-array
of all events.

Each json-object contains:

- "name": event's name (json-string)

Example:

-> { "execute": "query-events" }
<- {
      "return":[
         {
            "name":"SHUTDOWN"
         },
         {
            "name":"RESET"
         }
      ]
   }

Note: This example has been shortened as the real response is too long.

EQMP

    {
        .name       = "query-events",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_events,
    },

SQMP
query-qmp-schema
----------------

Return the QMP wire schema.  The returned value is a json-array of
named schema entities.  Entities are commands, events and various
types.  See docs/qapi-code-gen.txt for information on their structure
and intended use.

EQMP

    {
        .name       = "query-qmp-schema",
        .args_type  = "",
        .mhandler.cmd_new = qmp_query_qmp_schema,
    },

SQMP
query-chardev
-------------

Each device is represented by a json-object. The returned value is a json-array
of all devices.

Each json-object contain the following:

- "label": device's label (json-string)
- "filename": device's file (json-string)
- "frontend-open": open/closed state of the frontend device attached to this
                   backend (json-bool)

Example:

-> { "execute": "query-chardev" }
<- {
      "return": [
         {
            "label": "charchannel0",
            "filename": "unix:/var/lib/libvirt/qemu/seabios.rhel6.agent,server",
            "frontend-open": false
         },
         {
            "label": "charmonitor",
            "filename": "unix:/var/lib/libvirt/qemu/seabios.rhel6.monitor,server",
            "frontend-open": true
         },
         {
            "label": "charserial0",
            "filename": "pty:/dev/pts/2",
            "frontend-open": true
         }
      ]
   }

EQMP

    {
        .name       = "query-chardev",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_chardev,
    },

SQMP
query-chardev-backends
-------------

List available character device backends.

Each backend is represented by a json-object, the returned value is a json-array
of all backends.

Each json-object contains:

- "name": backend name (json-string)

Example:

-> { "execute": "query-chardev-backends" }
<- {
      "return":[
         {
            "name":"udp"
         },
         {
            "name":"tcp"
         },
         {
            "name":"unix"
         },
         {
            "name":"spiceport"
         }
      ]
   }

EQMP

    {
        .name       = "query-chardev-backends",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_chardev_backends,
    },

SQMP
query-block
-----------

Show the block devices.

Each block device information is stored in a json-object and the returned value
is a json-array of all devices.

Each json-object contain the following:

- "device": device name (json-string)
- "type": device type (json-string)
         - deprecated, retained for backward compatibility
         - Possible values: "unknown"
- "removable": true if the device is removable, false otherwise (json-bool)
- "locked": true if the device is locked, false otherwise (json-bool)
- "tray_open": only present if removable, true if the device has a tray,
               and it is open (json-bool)
- "inserted": only present if the device is inserted, it is a json-object
   containing the following:
         - "file": device file name (json-string)
         - "ro": true if read-only, false otherwise (json-bool)
         - "drv": driver format name (json-string)
             - Possible values: "blkdebug", "bochs", "cloop", "dmg",
                                "file", "file", "ftp", "ftps", "host_cdrom",
                                "host_device", "http", "https",
                                "nbd", "parallels", "qcow", "qcow2", "raw",
                                "tftp", "vdi", "vmdk", "vpc", "vvfat"
         - "backing_file": backing file name (json-string, optional)
         - "backing_file_depth": number of files in the backing file chain (json-int)
         - "encrypted": true if encrypted, false otherwise (json-bool)
         - "bps": limit total bytes per second (json-int)
         - "bps_rd": limit read bytes per second (json-int)
         - "bps_wr": limit write bytes per second (json-int)
         - "iops": limit total I/O operations per second (json-int)
         - "iops_rd": limit read operations per second (json-int)
         - "iops_wr": limit write operations per second (json-int)
         - "bps_max":  total max in bytes (json-int)
         - "bps_rd_max":  read max in bytes (json-int)
         - "bps_wr_max":  write max in bytes (json-int)
         - "iops_max":  total I/O operations max (json-int)
         - "iops_rd_max":  read I/O operations max (json-int)
         - "iops_wr_max":  write I/O operations max (json-int)
         - "iops_size": I/O size when limiting by iops (json-int)
         - "detect_zeroes": detect and optimize zero writing (json-string)
             - Possible values: "off", "on", "unmap"
         - "write_threshold": write offset threshold in bytes, a event will be
                              emitted if crossed. Zero if disabled (json-int)
         - "image": the detail of the image, it is a json-object containing
            the following:
             - "filename": image file name (json-string)
             - "format": image format (json-string)
             - "virtual-size": image capacity in bytes (json-int)
             - "dirty-flag": true if image is not cleanly closed, not present
                             means clean (json-bool, optional)
             - "actual-size": actual size on disk in bytes of the image, not
                              present when image does not support thin
                              provision (json-int, optional)
             - "cluster-size": size of a cluster in bytes, not present if image
                               format does not support it (json-int, optional)
             - "encrypted": true if the image is encrypted, not present means
                            false or the image format does not support
                            encryption (json-bool, optional)
             - "backing_file": backing file name, not present means no backing
                               file is used or the image format does not
                               support backing file chain
                               (json-string, optional)
             - "full-backing-filename": full path of the backing file, not
                                        present if it equals backing_file or no
                                        backing file is used
                                        (json-string, optional)
             - "backing-filename-format": the format of the backing file, not
                                          present means unknown or no backing
                                          file (json-string, optional)
             - "snapshots": the internal snapshot info, it is an optional list
                of json-object containing the following:
                 - "id": unique snapshot id (json-string)
                 - "name": snapshot name (json-string)
                 - "vm-state-size": size of the VM state in bytes (json-int)
                 - "date-sec": UTC date of the snapshot in seconds (json-int)
                 - "date-nsec": fractional part in nanoseconds to be used with
                                date-sec (json-int)
                 - "vm-clock-sec": VM clock relative to boot in seconds
                                   (json-int)
                 - "vm-clock-nsec": fractional part in nanoseconds to be used
                                    with vm-clock-sec (json-int)
             - "backing-image": the detail of the backing image, it is an
                                optional json-object only present when a
                                backing image present for this image

- "io-status": I/O operation status, only present if the device supports it
               and the VM is configured to stop on errors. It's always reset
               to "ok" when the "cont" command is issued (json_string, optional)
             - Possible values: "ok", "failed", "nospace"

Example:

-> { "execute": "query-block" }
<- {
      "return":[
         {
            "io-status": "ok",
            "device":"ide0-hd0",
            "locked":false,
            "removable":false,
            "inserted":{
               "ro":false,
               "drv":"qcow2",
               "encrypted":false,
               "file":"disks/test.qcow2",
               "backing_file_depth":1,
               "bps":1000000,
               "bps_rd":0,
               "bps_wr":0,
               "iops":1000000,
               "iops_rd":0,
               "iops_wr":0,
               "bps_max": 8000000,
               "bps_rd_max": 0,
               "bps_wr_max": 0,
               "iops_max": 0,
               "iops_rd_max": 0,
               "iops_wr_max": 0,
               "iops_size": 0,
               "detect_zeroes": "on",
               "write_threshold": 0,
               "image":{
                  "filename":"disks/test.qcow2",
                  "format":"qcow2",
                  "virtual-size":2048000,
                  "backing_file":"base.qcow2",
                  "full-backing-filename":"disks/base.qcow2",
                  "backing-filename-format":"qcow2",
                  "snapshots":[
                     {
                        "id": "1",
                        "name": "snapshot1",
                        "vm-state-size": 0,
                        "date-sec": 10000200,
                        "date-nsec": 12,
                        "vm-clock-sec": 206,
                        "vm-clock-nsec": 30
                     }
                  ],
                  "backing-image":{
                      "filename":"disks/base.qcow2",
                      "format":"qcow2",
                      "virtual-size":2048000
                  }
               }
            },
            "type":"unknown"
         },
         {
            "io-status": "ok",
            "device":"ide1-cd0",
            "locked":false,
            "removable":true,
            "type":"unknown"
         },
         {
            "device":"floppy0",
            "locked":false,
            "removable":true,
            "type":"unknown"
         },
         {
            "device":"sd0",
            "locked":false,
            "removable":true,
            "type":"unknown"
         }
      ]
   }

EQMP

    {
        .name       = "query-block",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_block,
    },

SQMP
query-blockstats
----------------

Show block device statistics.

Each device statistic information is stored in a json-object and the returned
value is a json-array of all devices.

Each json-object contain the following:

- "device": device name (json-string)
- "stats": A json-object with the statistics information, it contains:
    - "rd_bytes": bytes read (json-int)
    - "wr_bytes": bytes written (json-int)
    - "rd_operations": read operations (json-int)
    - "wr_operations": write operations (json-int)
    - "flush_operations": cache flush operations (json-int)
    - "wr_total_time_ns": total time spend on writes in nano-seconds (json-int)
    - "rd_total_time_ns": total time spend on reads in nano-seconds (json-int)
    - "flush_total_time_ns": total time spend on cache flushes in nano-seconds (json-int)
    - "wr_highest_offset": The offset after the greatest byte written to the
                           BlockDriverState since it has been opened (json-int)
    - "rd_merged": number of read requests that have been merged into
                   another request (json-int)
    - "wr_merged": number of write requests that have been merged into
                   another request (json-int)
    - "idle_time_ns": time since the last I/O operation, in
                      nanoseconds. If the field is absent it means
                      that there haven't been any operations yet
                      (json-int, optional)
    - "failed_rd_operations": number of failed read operations
                              (json-int)
    - "failed_wr_operations": number of failed write operations
                              (json-int)
    - "failed_flush_operations": number of failed flush operations
                               (json-int)
    - "invalid_rd_operations": number of invalid read operations
                               (json-int)
    - "invalid_wr_operations": number of invalid write operations
                               (json-int)
    - "invalid_flush_operations": number of invalid flush operations
                                  (json-int)
    - "account_invalid": whether invalid operations are included in
                         the last access statistics (json-bool)
    - "account_failed": whether failed operations are included in the
                         latency and last access statistics
                         (json-bool)
    - "timed_stats": A json-array containing statistics collected in
                     specific intervals, with the following members:
        - "interval_length": interval used for calculating the
                             statistics, in seconds (json-int)
        - "min_rd_latency_ns": minimum latency of read operations in
                               the defined interval, in nanoseconds
                               (json-int)
        - "min_wr_latency_ns": minimum latency of write operations in
                               the defined interval, in nanoseconds
                               (json-int)
        - "min_flush_latency_ns": minimum latency of flush operations
                                  in the defined interval, in
                                  nanoseconds (json-int)
        - "max_rd_latency_ns": maximum latency of read operations in
                               the defined interval, in nanoseconds
                               (json-int)
        - "max_wr_latency_ns": maximum latency of write operations in
                               the defined interval, in nanoseconds
                               (json-int)
        - "max_flush_latency_ns": maximum latency of flush operations
                                  in the defined interval, in
                                  nanoseconds (json-int)
        - "avg_rd_latency_ns": average latency of read operations in
                               the defined interval, in nanoseconds
                               (json-int)
        - "avg_wr_latency_ns": average latency of write operations in
                               the defined interval, in nanoseconds
                               (json-int)
        - "avg_flush_latency_ns": average latency of flush operations
                                  in the defined interval, in
                                  nanoseconds (json-int)
        - "avg_rd_queue_depth": average number of pending read
                                operations in the defined interval
                                (json-number)
        - "avg_wr_queue_depth": average number of pending write
                                operations in the defined interval
                                (json-number).
- "parent": Contains recursively the statistics of the underlying
            protocol (e.g. the host file for a qcow2 image). If there is
            no underlying protocol, this field is omitted
            (json-object, optional)

Example:

-> { "execute": "query-blockstats" }
<- {
      "return":[
         {
            "device":"ide0-hd0",
            "parent":{
               "stats":{
                  "wr_highest_offset":3686448128,
                  "wr_bytes":9786368,
                  "wr_operations":751,
                  "rd_bytes":122567168,
                  "rd_operations":36772
                  "wr_total_times_ns":313253456
                  "rd_total_times_ns":3465673657
                  "flush_total_times_ns":49653
                  "flush_operations":61,
                  "rd_merged":0,
                  "wr_merged":0,
                  "idle_time_ns":2953431879,
                  "account_invalid":true,
                  "account_failed":false
               }
            },
            "stats":{
               "wr_highest_offset":2821110784,
               "wr_bytes":9786368,
               "wr_operations":692,
               "rd_bytes":122739200,
               "rd_operations":36604
               "flush_operations":51,
               "wr_total_times_ns":313253456
               "rd_total_times_ns":3465673657
               "flush_total_times_ns":49653,
               "rd_merged":0,
               "wr_merged":0,
               "idle_time_ns":2953431879,
               "account_invalid":true,
               "account_failed":false
            }
         },
         {
            "device":"ide1-cd0",
            "stats":{
               "wr_highest_offset":0,
               "wr_bytes":0,
               "wr_operations":0,
               "rd_bytes":0,
               "rd_operations":0
               "flush_operations":0,
               "wr_total_times_ns":0
               "rd_total_times_ns":0
               "flush_total_times_ns":0,
               "rd_merged":0,
               "wr_merged":0,
               "account_invalid":false,
               "account_failed":false
            }
         },
         {
            "device":"floppy0",
            "stats":{
               "wr_highest_offset":0,
               "wr_bytes":0,
               "wr_operations":0,
               "rd_bytes":0,
               "rd_operations":0
               "flush_operations":0,
               "wr_total_times_ns":0
               "rd_total_times_ns":0
               "flush_total_times_ns":0,
               "rd_merged":0,
               "wr_merged":0,
               "account_invalid":false,
               "account_failed":false
            }
         },
         {
            "device":"sd0",
            "stats":{
               "wr_highest_offset":0,
               "wr_bytes":0,
               "wr_operations":0,
               "rd_bytes":0,
               "rd_operations":0
               "flush_operations":0,
               "wr_total_times_ns":0
               "rd_total_times_ns":0
               "flush_total_times_ns":0,
               "rd_merged":0,
               "wr_merged":0,
               "account_invalid":false,
               "account_failed":false
            }
         }
      ]
   }

EQMP

    {
        .name       = "query-blockstats",
        .args_type  = "query-nodes:b?",
        .mhandler.cmd_new = qmp_marshal_query_blockstats,
    },

SQMP
query-cpus
----------

Show CPU information.

Return a json-array. Each CPU is represented by a json-object, which contains:

- "CPU": CPU index (json-int)
- "current": true if this is the current CPU, false otherwise (json-bool)
- "halted": true if the cpu is halted, false otherwise (json-bool)
- "qom_path": path to the CPU object in the QOM tree (json-str)
- "arch": architecture of the cpu, which determines what additional
          keys will be present (json-str)
- Current program counter. The key's name depends on the architecture:
     "pc": i386/x86_64 (json-int)
     "nip": PPC (json-int)
     "pc" and "npc": sparc (json-int)
     "PC": mips (json-int)
- "thread_id": ID of the underlying host thread (json-int)

Example:

-> { "execute": "query-cpus" }
<- {
      "return":[
         {
            "CPU":0,
            "current":true,
            "halted":false,
            "qom_path":"/machine/unattached/device[0]",
            "arch":"x86",
            "pc":3227107138,
            "thread_id":3134
         },
         {
            "CPU":1,
            "current":false,
            "halted":true,
            "qom_path":"/machine/unattached/device[2]",
            "arch":"x86",
            "pc":7108165,
            "thread_id":3135
         }
      ]
   }

EQMP

    {
        .name       = "query-cpus",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_cpus,
    },

SQMP
query-iothreads
---------------

Returns a list of information about each iothread.

Note this list excludes the QEMU main loop thread, which is not declared
using the -object iothread command-line option.  It is always the main thread
of the process.

Return a json-array. Each iothread is represented by a json-object, which contains:

- "id": name of iothread (json-str)
- "thread-id": ID of the underlying host thread (json-int)

Example:

-> { "execute": "query-iothreads" }
<- {
      "return":[
         {
            "id":"iothread0",
            "thread-id":3134
         },
         {
            "id":"iothread1",
            "thread-id":3135
         }
      ]
   }

EQMP

    {
        .name       = "query-iothreads",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_iothreads,
    },

SQMP
query-pci
---------

PCI buses and devices information.

The returned value is a json-array of all buses. Each bus is represented by
a json-object, which has a key with a json-array of all PCI devices attached
to it. Each device is represented by a json-object.

The bus json-object contains the following:

- "bus": bus number (json-int)
- "devices": a json-array of json-objects, each json-object represents a
             PCI device

The PCI device json-object contains the following:

- "bus": identical to the parent's bus number (json-int)
- "slot": slot number (json-int)
- "function": function number (json-int)
- "class_info": a json-object containing:
     - "desc": device class description (json-string, optional)
     - "class": device class number (json-int)
- "id": a json-object containing:
     - "device": device ID (json-int)
     - "vendor": vendor ID (json-int)
- "irq": device's IRQ if assigned (json-int, optional)
- "qdev_id": qdev id string (json-string)
- "pci_bridge": It's a json-object, only present if this device is a
                PCI bridge, contains:
     - "bus": bus number (json-int)
     - "secondary": secondary bus number (json-int)
     - "subordinate": subordinate bus number (json-int)
     - "io_range": I/O memory range information, a json-object with the
                   following members:
                 - "base": base address, in bytes (json-int)
                 - "limit": limit address, in bytes (json-int)
     - "memory_range": memory range information, a json-object with the
                       following members:
                 - "base": base address, in bytes (json-int)
                 - "limit": limit address, in bytes (json-int)
     - "prefetchable_range": Prefetchable memory range information, a
                             json-object with the following members:
                 - "base": base address, in bytes (json-int)
                 - "limit": limit address, in bytes (json-int)
     - "devices": a json-array of PCI devices if there's any attached, each
                  each element is represented by a json-object, which contains
                  the same members of the 'PCI device json-object' described
                  above (optional)
- "regions": a json-array of json-objects, each json-object represents a
             memory region of this device

The memory range json-object contains the following:

- "base": base memory address (json-int)
- "limit": limit value (json-int)

The region json-object can be an I/O region or a memory region, an I/O region
json-object contains the following:

- "type": "io" (json-string, fixed)
- "bar": BAR number (json-int)
- "address": memory address (json-int)
- "size": memory size (json-int)

A memory region json-object contains the following:

- "type": "memory" (json-string, fixed)
- "bar": BAR number (json-int)
- "address": memory address (json-int)
- "size": memory size (json-int)
- "mem_type_64": true or false (json-bool)
- "prefetch": true or false (json-bool)

Example:

-> { "execute": "query-pci" }
<- {
      "return":[
         {
            "bus":0,
            "devices":[
               {
                  "bus":0,
                  "qdev_id":"",
                  "slot":0,
                  "class_info":{
                     "class":1536,
                     "desc":"Host bridge"
                  },
                  "id":{
                     "device":32902,
                     "vendor":4663
                  },
                  "function":0,
                  "regions":[
   
                  ]
               },
               {
                  "bus":0,
                  "qdev_id":"",
                  "slot":1,
                  "class_info":{
                     "class":1537,
                     "desc":"ISA bridge"
                  },
                  "id":{
                     "device":32902,
                     "vendor":28672
                  },
                  "function":0,
                  "regions":[
   
                  ]
               },
               {
                  "bus":0,
                  "qdev_id":"",
                  "slot":1,
                  "class_info":{
                     "class":257,
                     "desc":"IDE controller"
                  },
                  "id":{
                     "device":32902,
                     "vendor":28688
                  },
                  "function":1,
                  "regions":[
                     {
                        "bar":4,
                        "size":16,
                        "address":49152,
                        "type":"io"
                     }
                  ]
               },
               {
                  "bus":0,
                  "qdev_id":"",
                  "slot":2,
                  "class_info":{
                     "class":768,
                     "desc":"VGA controller"
                  },
                  "id":{
                     "device":4115,
                     "vendor":184
                  },
                  "function":0,
                  "regions":[
                     {
                        "prefetch":true,
                        "mem_type_64":false,
                        "bar":0,
                        "size":33554432,
                        "address":4026531840,
                        "type":"memory"
                     },
                     {
                        "prefetch":false,
                        "mem_type_64":false,
                        "bar":1,
                        "size":4096,
                        "address":4060086272,
                        "type":"memory"
                     },
                     {
                        "prefetch":false,
                        "mem_type_64":false,
                        "bar":6,
                        "size":65536,
                        "address":-1,
                        "type":"memory"
                     }
                  ]
               },
               {
                  "bus":0,
                  "qdev_id":"",
                  "irq":11,
                  "slot":4,
                  "class_info":{
                     "class":1280,
                     "desc":"RAM controller"
                  },
                  "id":{
                     "device":6900,
                     "vendor":4098
                  },
                  "function":0,
                  "regions":[
                     {
                        "bar":0,
                        "size":32,
                        "address":49280,
                        "type":"io"
                     }
                  ]
               }
            ]
         }
      ]
   }

Note: This example has been shortened as the real response is too long.

EQMP

    {
        .name       = "query-pci",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_pci,
    },

SQMP
query-kvm
---------

Show KVM information.

Return a json-object with the following information:

- "enabled": true if KVM support is enabled, false otherwise (json-bool)
- "present": true if QEMU has KVM support, false otherwise (json-bool)

Example:

-> { "execute": "query-kvm" }
<- { "return": { "enabled": true, "present": true } }

EQMP

    {
        .name       = "query-kvm",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_kvm,
    },

SQMP
query-status
------------

Return a json-object with the following information:

- "running": true if the VM is running, or false if it is paused (json-bool)
- "singlestep": true if the VM is in single step mode,
                false otherwise (json-bool)
- "status": one of the following values (json-string)
    "debug" - QEMU is running on a debugger
    "inmigrate" - guest is paused waiting for an incoming migration
    "internal-error" - An internal error that prevents further guest
    execution has occurred
    "io-error" - the last IOP has failed and the device is configured
    to pause on I/O errors
    "paused" - guest has been paused via the 'stop' command
    "postmigrate" - guest is paused following a successful 'migrate'
    "prelaunch" - QEMU was started with -S and guest has not started
    "finish-migrate" - guest is paused to finish the migration process
    "restore-vm" - guest is paused to restore VM state
    "running" - guest is actively running
    "save-vm" - guest is paused to save the VM state
    "shutdown" - guest is shut down (and -no-shutdown is in use)
    "watchdog" - the watchdog action is configured to pause and
     has been triggered

Example:

-> { "execute": "query-status" }
<- { "return": { "running": true, "singlestep": false, "status": "running" } }

EQMP
    
    {
        .name       = "query-status",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_status,
    },

SQMP
query-mice
----------

Show VM mice information.

Each mouse is represented by a json-object, the returned value is a json-array
of all mice.

The mouse json-object contains the following:

- "name": mouse's name (json-string)
- "index": mouse's index (json-int)
- "current": true if this mouse is receiving events, false otherwise (json-bool)
- "absolute": true if the mouse generates absolute input events (json-bool)

Example:

-> { "execute": "query-mice" }
<- {
      "return":[
         {
            "name":"QEMU Microsoft Mouse",
            "index":0,
            "current":false,
            "absolute":false
         },
         {
            "name":"QEMU PS/2 Mouse",
            "index":1,
            "current":true,
            "absolute":true
         }
      ]
   }

EQMP

    {
        .name       = "query-mice",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_mice,
    },

SQMP
query-vnc
---------

Show VNC server information.

Return a json-object with server information. Connected clients are returned
as a json-array of json-objects.

The main json-object contains the following:

- "enabled": true or false (json-bool)
- "host": server's IP address (json-string)
- "family": address family (json-string)
         - Possible values: "ipv4", "ipv6", "unix", "unknown"
- "service": server's port number (json-string)
- "auth": authentication method (json-string)
         - Possible values: "invalid", "none", "ra2", "ra2ne", "sasl", "tight",
                            "tls", "ultra", "unknown", "vencrypt", "vencrypt",
                            "vencrypt+plain", "vencrypt+tls+none",
                            "vencrypt+tls+plain", "vencrypt+tls+sasl",
                            "vencrypt+tls+vnc", "vencrypt+x509+none",
                            "vencrypt+x509+plain", "vencrypt+x509+sasl",
                            "vencrypt+x509+vnc", "vnc"
- "clients": a json-array of all connected clients

Clients are described by a json-object, each one contain the following:

- "host": client's IP address (json-string)
- "family": address family (json-string)
         - Possible values: "ipv4", "ipv6", "unix", "unknown"
- "service": client's port number (json-string)
- "x509_dname": TLS dname (json-string, optional)
- "sasl_username": SASL username (json-string, optional)

Example:

-> { "execute": "query-vnc" }
<- {
      "return":{
         "enabled":true,
         "host":"0.0.0.0",
         "service":"50402",
         "auth":"vnc",
         "family":"ipv4",
         "clients":[
            {
               "host":"127.0.0.1",
               "service":"50401",
               "family":"ipv4"
            }
         ]
      }
   }

EQMP

    {
        .name       = "query-vnc",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_vnc,
    },
    {
        .name       = "query-vnc-servers",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_vnc_servers,
    },

SQMP
query-spice
-----------

Show SPICE server information.

Return a json-object with server information. Connected clients are returned
as a json-array of json-objects.

The main json-object contains the following:

- "enabled": true or false (json-bool)
- "host": server's IP address (json-string)
- "port": server's port number (json-int, optional)
- "tls-port": server's port number (json-int, optional)
- "auth": authentication method (json-string)
         - Possible values: "none", "spice"
- "channels": a json-array of all active channels clients

Channels are described by a json-object, each one contain the following:

- "host": client's IP address (json-string)
- "family": address family (json-string)
         - Possible values: "ipv4", "ipv6", "unix", "unknown"
- "port": client's port number (json-string)
- "connection-id": spice connection id.  All channels with the same id
                   belong to the same spice session (json-int)
- "channel-type": channel type.  "1" is the main control channel, filter for
                  this one if you want track spice sessions only (json-int)
- "channel-id": channel id.  Usually "0", might be different needed when
                multiple channels of the same type exist, such as multiple
                display channels in a multihead setup (json-int)
- "tls": whether the channel is encrypted (json-bool)

Example:

-> { "execute": "query-spice" }
<- {
      "return": {
         "enabled": true,
         "auth": "spice",
         "port": 5920,
         "tls-port": 5921,
         "host": "0.0.0.0",
         "channels": [
            {
               "port": "54924",
               "family": "ipv4",
               "channel-type": 1,
               "connection-id": 1804289383,
               "host": "127.0.0.1",
               "channel-id": 0,
               "tls": true
            },
            {
               "port": "36710",
               "family": "ipv4",
               "channel-type": 4,
               "connection-id": 1804289383,
               "host": "127.0.0.1",
               "channel-id": 0,
               "tls": false
            },
            [ ... more channels follow ... ]
         ]
      }
   }

EQMP

#if defined(CONFIG_SPICE)
    {
        .name       = "query-spice",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_spice,
    },
#endif

SQMP
query-name
----------

Show VM name.

Return a json-object with the following information:

- "name": VM's name (json-string, optional)

Example:

-> { "execute": "query-name" }
<- { "return": { "name": "qemu-name" } }

EQMP

    {
        .name       = "query-name",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_name,
    },

SQMP
query-uuid
----------

Show VM UUID.

Return a json-object with the following information:

- "UUID": Universally Unique Identifier (json-string)

Example:

-> { "execute": "query-uuid" }
<- { "return": { "UUID": "550e8400-e29b-41d4-a716-446655440000" } }

EQMP

    {
        .name       = "query-uuid",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_uuid,
    },

SQMP
query-command-line-options
--------------------------

Show command line option schema.

Return a json-array of command line option schema for all options (or for
the given option), returning an error if the given option doesn't exist.

Each array entry contains the following:

- "option": option name (json-string)
- "parameters": a json-array describes all parameters of the option:
    - "name": parameter name (json-string)
    - "type": parameter type (one of 'string', 'boolean', 'number',
              or 'size')
    - "help": human readable description of the parameter
              (json-string, optional)
    - "default": default value string for the parameter
                 (json-string, optional)

Example:

-> { "execute": "query-command-line-options", "arguments": { "option": "option-rom" } }
<- { "return": [
        {
            "parameters": [
                {
                    "name": "romfile",
                    "type": "string"
                },
                {
                    "name": "bootindex",
                    "type": "number"
                }
            ],
            "option": "option-rom"
        }
     ]
   }

EQMP

    {
        .name       = "query-command-line-options",
        .args_type  = "option:s?",
        .mhandler.cmd_new = qmp_marshal_query_command_line_options,
    },

SQMP
query-migrate
-------------

Migration status.

Return a json-object. If migration is active there will be another json-object
with RAM migration status and if block migration is active another one with
block migration status.

The main json-object contains the following:

- "status": migration status (json-string)
     - Possible values: "setup", "active", "completed", "failed", "cancelled"
- "total-time": total amount of ms since migration started.  If
                migration has ended, it returns the total migration
                time (json-int)
- "setup-time" amount of setup time in milliseconds _before_ the
               iterations begin but _after_ the QMP command is issued.
               This is designed to provide an accounting of any activities
               (such as RDMA pinning) which may be expensive, but do not 
               actually occur during the iterative migration rounds 
               themselves. (json-int)
- "downtime": only present when migration has finished correctly
              total amount in ms for downtime that happened (json-int)
- "expected-downtime": only present while migration is active
                total amount in ms for downtime that was calculated on
                the last bitmap round (json-int)
- "ram": only present if "status" is "active", it is a json-object with the
  following RAM information:
         - "transferred": amount transferred in bytes (json-int)
         - "remaining": amount remaining to transfer in bytes (json-int)
         - "total": total amount of memory in bytes (json-int)
         - "duplicate": number of pages filled entirely with the same
            byte (json-int)
            These are sent over the wire much more efficiently.
         - "skipped": number of skipped zero pages (json-int)
         - "normal" : number of whole pages transferred.  I.e. they
            were not sent as duplicate or xbzrle pages (json-int)
         - "normal-bytes" : number of bytes transferred in whole
            pages. This is just normal pages times size of one page,
            but this way upper levels don't need to care about page
            size (json-int)
         - "dirty-sync-count": times that dirty ram was synchronized (json-int)
- "disk": only present if "status" is "active" and it is a block migration,
  it is a json-object with the following disk information:
         - "transferred": amount transferred in bytes (json-int)
         - "remaining": amount remaining to transfer in bytes json-int)
         - "total": total disk size in bytes (json-int)
- "xbzrle-cache": only present if XBZRLE is active.
  It is a json-object with the following XBZRLE information:
         - "cache-size": XBZRLE cache size in bytes
         - "bytes": number of bytes transferred for XBZRLE compressed pages
         - "pages": number of XBZRLE compressed pages
         - "cache-miss": number of XBRZRLE page cache misses
         - "cache-miss-rate": rate of XBRZRLE page cache misses
         - "overflow": number of times XBZRLE overflows.  This means
           that the XBZRLE encoding was bigger than just sent the
           whole page, and then we sent the whole page instead (as as
           normal page).

Examples:

1. Before the first migration

-> { "execute": "query-migrate" }
<- { "return": {} }

2. Migration is done and has succeeded

-> { "execute": "query-migrate" }
<- { "return": {
        "status": "completed",
        "ram":{
          "transferred":123,
          "remaining":123,
          "total":246,
          "total-time":12345,
          "setup-time":12345,
          "downtime":12345,
          "duplicate":123,
          "normal":123,
          "normal-bytes":123456,
          "dirty-sync-count":15
        }
     }
   }

3. Migration is done and has failed

-> { "execute": "query-migrate" }
<- { "return": { "status": "failed" } }

4. Migration is being performed and is not a block migration:

-> { "execute": "query-migrate" }
<- {
      "return":{
         "status":"active",
         "ram":{
            "transferred":123,
            "remaining":123,
            "total":246,
            "total-time":12345,
            "setup-time":12345,
            "expected-downtime":12345,
            "duplicate":123,
            "normal":123,
            "normal-bytes":123456,
            "dirty-sync-count":15
         }
      }
   }

5. Migration is being performed and is a block migration:

-> { "execute": "query-migrate" }
<- {
      "return":{
         "status":"active",
         "ram":{
            "total":1057024,
            "remaining":1053304,
            "transferred":3720,
            "total-time":12345,
            "setup-time":12345,
            "expected-downtime":12345,
            "duplicate":123,
            "normal":123,
            "normal-bytes":123456,
            "dirty-sync-count":15
         },
         "disk":{
            "total":20971520,
            "remaining":20880384,
            "transferred":91136
         }
      }
   }

6. Migration is being performed and XBZRLE is active:

-> { "execute": "query-migrate" }
<- {
      "return":{
         "status":"active",
         "capabilities" : [ { "capability": "xbzrle", "state" : true } ],
         "ram":{
            "total":1057024,
            "remaining":1053304,
            "transferred":3720,
            "total-time":12345,
            "setup-time":12345,
            "expected-downtime":12345,
            "duplicate":10,
            "normal":3333,
            "normal-bytes":3412992,
            "dirty-sync-count":15
         },
         "xbzrle-cache":{
            "cache-size":67108864,
            "bytes":20971520,
            "pages":2444343,
            "cache-miss":2244,
            "cache-miss-rate":0.123,
            "overflow":34434
         }
      }
   }

EQMP

    {
        .name       = "query-migrate",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_migrate,
    },

SQMP
migrate-set-capabilities
------------------------

Enable/Disable migration capabilities

- "xbzrle": XBZRLE support
- "rdma-pin-all": pin all pages when using RDMA during migration
- "auto-converge": throttle down guest to help convergence of migration
- "zero-blocks": compress zero blocks during block migration
- "events": generate events for each migration state change

Arguments:

Example:

-> { "execute": "migrate-set-capabilities" , "arguments":
     { "capabilities": [ { "capability": "xbzrle", "state": true } ] } }

EQMP

    {
        .name       = "migrate-set-capabilities",
        .args_type  = "capabilities:q",
        .params     = "capability:s,state:b",
        .mhandler.cmd_new = qmp_marshal_migrate_set_capabilities,
    },
SQMP
query-migrate-capabilities
--------------------------

Query current migration capabilities

- "capabilities": migration capabilities state
         - "xbzrle" : XBZRLE state (json-bool)
         - "rdma-pin-all" : RDMA Pin Page state (json-bool)
         - "auto-converge" : Auto Converge state (json-bool)
         - "zero-blocks" : Zero Blocks state (json-bool)

Arguments:

Example:

-> { "execute": "query-migrate-capabilities" }
<- { "return": [ { "state": false, "capability": "xbzrle" } ] }

EQMP

    {
        .name       = "query-migrate-capabilities",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_migrate_capabilities,
    },

SQMP
migrate-set-parameters
----------------------

Set migration parameters

- "compress-level": set compression level during migration (json-int)
- "compress-threads": set compression thread count for migration (json-int)
- "decompress-threads": set decompression thread count for migration (json-int)

Arguments:

Example:

-> { "execute": "migrate-set-parameters" , "arguments":
      { "compress-level": 1 } }

EQMP

    {
        .name       = "migrate-set-parameters",
        .args_type  =
            "compress-level:i?,compress-threads:i?,decompress-threads:i?",
        .mhandler.cmd_new = qmp_marshal_migrate_set_parameters,
    },
SQMP
query-migrate-parameters
------------------------

Query current migration parameters

- "parameters": migration parameters value
         - "compress-level" : compression level value (json-int)
         - "compress-threads" : compression thread count value (json-int)
         - "decompress-threads" : decompression thread count value (json-int)

Arguments:

Example:

-> { "execute": "query-migrate-parameters" }
<- {
      "return": {
         "decompress-threads", 2,
         "compress-threads", 8,
         "compress-level", 1
      }
   }

EQMP

    {
        .name       = "query-migrate-parameters",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_migrate_parameters,
    },

SQMP
query-balloon
-------------

Show balloon information.

Make an asynchronous request for balloon info. When the request completes a
json-object will be returned containing the following data:

- "actual": current balloon value in bytes (json-int)

Example:

-> { "execute": "query-balloon" }
<- {
      "return":{
         "actual":1073741824,
      }
   }

EQMP

    {
        .name       = "query-balloon",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_balloon,
    },

    {
        .name       = "query-block-jobs",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_block_jobs,
    },

    {
        .name       = "qom-list",
        .args_type  = "path:s",
        .mhandler.cmd_new = qmp_marshal_qom_list,
    },

    {
        .name       = "qom-set",
	.args_type  = "path:s,property:s,value:q",
        .mhandler.cmd_new = qmp_marshal_qom_set,
    },

    {
        .name       = "qom-get",
	.args_type  = "path:s,property:s",
        .mhandler.cmd_new = qmp_marshal_qom_get,
    },

    {
        .name       = "nbd-server-start",
        .args_type  = "addr:q",
        .mhandler.cmd_new = qmp_marshal_nbd_server_start,
    },
    {
        .name       = "nbd-server-add",
        .args_type  = "device:B,writable:b?",
        .mhandler.cmd_new = qmp_marshal_nbd_server_add,
    },
    {
        .name       = "nbd-server-stop",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_nbd_server_stop,
    },

    {
        .name       = "change-vnc-password",
        .args_type  = "password:s",
        .mhandler.cmd_new = qmp_marshal_change_vnc_password,
    },
    {
        .name       = "qom-list-types",
        .args_type  = "implements:s?,abstract:b?",
        .mhandler.cmd_new = qmp_marshal_qom_list_types,
    },

    {
        .name       = "device-list-properties",
        .args_type  = "typename:s",
        .mhandler.cmd_new = qmp_marshal_device_list_properties,
    },

    {
        .name       = "query-machines",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_machines,
    },

    {
        .name       = "query-cpu-definitions",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_cpu_definitions,
    },

    {
        .name       = "query-target",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_target,
    },

    {
        .name       = "query-tpm",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_tpm,
    },

SQMP
query-tpm
---------

Return information about the TPM device.

Arguments: None

Example:

-> { "execute": "query-tpm" }
<- { "return":
     [
       { "model": "tpm-tis",
         "options":
           { "type": "passthrough",
             "data":
               { "cancel-path": "/sys/class/misc/tpm0/device/cancel",
                 "path": "/dev/tpm0"
               }
           },
         "id": "tpm0"
       }
     ]
   }

EQMP

    {
        .name       = "query-tpm-models",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_tpm_models,
    },

SQMP
query-tpm-models
----------------

Return a list of supported TPM models.

Arguments: None

Example:

-> { "execute": "query-tpm-models" }
<- { "return": [ "tpm-tis" ] }

EQMP

    {
        .name       = "query-tpm-types",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_tpm_types,
    },

SQMP
query-tpm-types
---------------

Return a list of supported TPM types.

Arguments: None

Example:

-> { "execute": "query-tpm-types" }
<- { "return": [ "passthrough" ] }

EQMP

    {
        .name       = "chardev-add",
        .args_type  = "id:s,backend:q",
        .mhandler.cmd_new = qmp_marshal_chardev_add,
    },

SQMP
chardev-add
----------------

Add a chardev.

Arguments:

- "id": the chardev's ID, must be unique (json-string)
- "backend": chardev backend type + parameters

Examples:

-> { "execute" : "chardev-add",
     "arguments" : { "id" : "foo",
                     "backend" : { "type" : "null", "data" : {} } } }
<- { "return": {} }

-> { "execute" : "chardev-add",
     "arguments" : { "id" : "bar",
                     "backend" : { "type" : "file",
                                   "data" : { "out" : "/tmp/bar.log" } } } }
<- { "return": {} }

-> { "execute" : "chardev-add",
     "arguments" : { "id" : "baz",
                     "backend" : { "type" : "pty", "data" : {} } } }
<- { "return": { "pty" : "/dev/pty/42" } }

EQMP

    {
        .name       = "chardev-remove",
        .args_type  = "id:s",
        .mhandler.cmd_new = qmp_marshal_chardev_remove,
    },


SQMP
chardev-remove
--------------

Remove a chardev.

Arguments:

- "id": the chardev's ID, must exist and not be in use (json-string)

Example:

-> { "execute": "chardev-remove", "arguments": { "id" : "foo" } }
<- { "return": {} }

EQMP
    {
        .name       = "query-rx-filter",
        .args_type  = "name:s?",
        .mhandler.cmd_new = qmp_marshal_query_rx_filter,
    },

SQMP
query-rx-filter
---------------

Show rx-filter information.

Returns a json-array of rx-filter information for all NICs (or for the
given NIC), returning an error if the given NIC doesn't exist, or
given NIC doesn't support rx-filter querying, or given net client
isn't a NIC.

The query will clear the event notification flag of each NIC, then qemu
will start to emit event to QMP monitor.

Each array entry contains the following:

- "name": net client name (json-string)
- "promiscuous": promiscuous mode is enabled (json-bool)
- "multicast": multicast receive state (one of 'normal', 'none', 'all')
- "unicast": unicast receive state  (one of 'normal', 'none', 'all')
- "vlan": vlan receive state (one of 'normal', 'none', 'all') (Since 2.0)
- "broadcast-allowed": allow to receive broadcast (json-bool)
- "multicast-overflow": multicast table is overflowed (json-bool)
- "unicast-overflow": unicast table is overflowed (json-bool)
- "main-mac": main macaddr string (json-string)
- "vlan-table": a json-array of active vlan id
- "unicast-table": a json-array of unicast macaddr string
- "multicast-table": a json-array of multicast macaddr string

Example:

-> { "execute": "query-rx-filter", "arguments": { "name": "vnet0" } }
<- { "return": [
        {
            "promiscuous": true,
            "name": "vnet0",
            "main-mac": "52:54:00:12:34:56",
            "unicast": "normal",
            "vlan": "normal",
            "vlan-table": [
                4,
                0
            ],
            "unicast-table": [
            ],
            "multicast": "normal",
            "multicast-overflow": false,
            "unicast-overflow": false,
            "multicast-table": [
                "01:00:5e:00:00:01",
                "33:33:00:00:00:01",
                "33:33:ff:12:34:56"
            ],
            "broadcast-allowed": false
        }
      ]
   }

EQMP

    {
        .name       = "blockdev-add",
        .args_type  = "options:q",
        .mhandler.cmd_new = qmp_marshal_blockdev_add,
    },

SQMP
blockdev-add
------------

Add a block device.

This command is still a work in progress.  It doesn't support all
block drivers among other things.  Stay away from it unless you want
to help with its development.

Arguments:

- "options": block driver options

Example (1):

-> { "execute": "blockdev-add",
    "arguments": { "options" : { "driver": "qcow2",
                                 "file": { "driver": "file",
                                           "filename": "test.qcow2" } } } }
<- { "return": {} }

Example (2):

-> { "execute": "blockdev-add",
     "arguments": {
         "options": {
           "driver": "qcow2",
           "id": "my_disk",
           "discard": "unmap",
           "cache": {
               "direct": true,
               "writeback": true
           },
           "file": {
               "driver": "file",
               "filename": "/tmp/test.qcow2"
           },
           "backing": {
               "driver": "raw",
               "file": {
                   "driver": "file",
                   "filename": "/dev/fdset/4"
               }
           }
         }
       }
     }

<- { "return": {} }

EQMP

    {
        .name       = "x-blockdev-del",
        .args_type  = "id:s?,node-name:s?",
        .mhandler.cmd_new = qmp_marshal_x_blockdev_del,
    },

SQMP
x-blockdev-del
------------
Since 2.5

Deletes a block device thas has been added using blockdev-add.
The selected device can be either a block backend or a graph node.

In the former case the backend will be destroyed, along with its
inserted medium if there's any. The command will fail if the backend
or its medium are in use.

In the latter case the node will be destroyed. The command will fail
if the node is attached to a block backend or is otherwise being
used.

One of "id" or "node-name" must be specified, but not both.

This command is still a work in progress and is considered
experimental. Stay away from it unless you want to help with its
development.

Arguments:

- "id": Name of the block backend device to delete (json-string, optional)
- "node-name": Name of the graph node to delete (json-string, optional)

Example:

-> { "execute": "blockdev-add",
     "arguments": {
         "options": {
             "driver": "qcow2",
             "id": "drive0",
             "file": {
                 "driver": "file",
                 "filename": "test.qcow2"
             }
         }
     }
   }

<- { "return": {} }

-> { "execute": "x-blockdev-del",
     "arguments": { "id": "drive0" }
   }
<- { "return": {} }

EQMP

    {
        .name       = "blockdev-open-tray",
        .args_type  = "device:s,force:b?",
        .mhandler.cmd_new = qmp_marshal_blockdev_open_tray,
    },

SQMP
blockdev-open-tray
------------------

Opens a block device's tray. If there is a block driver state tree inserted as a
medium, it will become inaccessible to the guest (but it will remain associated
to the block device, so closing the tray will make it accessible again).

If the tray was already open before, this will be a no-op.

Once the tray opens, a DEVICE_TRAY_MOVED event is emitted. There are cases in
which no such event will be generated, these include:
- if the guest has locked the tray, @force is false and the guest does not
  respond to the eject request
- if the BlockBackend denoted by @device does not have a guest device attached
  to it
- if the guest device does not have an actual tray and is empty, for instance
  for floppy disk drives

Arguments:

- "device": block device name (json-string)
- "force": if false (the default), an eject request will be sent to the guest if
           it has locked the tray (and the tray will not be opened immediately);
           if true, the tray will be opened regardless of whether it is locked
           (json-bool, optional)

Example:

-> { "execute": "blockdev-open-tray",
     "arguments": { "device": "ide1-cd0" } }

<- { "timestamp": { "seconds": 1418751016,
                    "microseconds": 716996 },
     "event": "DEVICE_TRAY_MOVED",
     "data": { "device": "ide1-cd0",
               "tray-open": true } }

<- { "return": {} }

EQMP

    {
        .name       = "blockdev-close-tray",
        .args_type  = "device:s",
        .mhandler.cmd_new = qmp_marshal_blockdev_close_tray,
    },

SQMP
blockdev-close-tray
-------------------

Closes a block device's tray. If there is a block driver state tree associated
with the block device (which is currently ejected), that tree will be loaded as
the medium.

If the tray was already closed before, this will be a no-op.

Arguments:

- "device": block device name (json-string)

Example:

-> { "execute": "blockdev-close-tray",
     "arguments": { "device": "ide1-cd0" } }

<- { "timestamp": { "seconds": 1418751345,
                    "microseconds": 272147 },
     "event": "DEVICE_TRAY_MOVED",
     "data": { "device": "ide1-cd0",
               "tray-open": false } }

<- { "return": {} }

EQMP

    {
        .name       = "x-blockdev-remove-medium",
        .args_type  = "device:s",
        .mhandler.cmd_new = qmp_marshal_x_blockdev_remove_medium,
    },

SQMP
x-blockdev-remove-medium
------------------------

Removes a medium (a block driver state tree) from a block device. That block
device's tray must currently be open (unless there is no attached guest device).

If the tray is open and there is no medium inserted, this will be a no-op.

This command is still a work in progress and is considered experimental.
Stay away from it unless you want to help with its development.

Arguments:

- "device": block device name (json-string)

Example:

-> { "execute": "x-blockdev-remove-medium",
     "arguments": { "device": "ide1-cd0" } }

<- { "error": { "class": "GenericError",
                "desc": "Tray of device 'ide1-cd0' is not open" } }

-> { "execute": "blockdev-open-tray",
     "arguments": { "device": "ide1-cd0" } }

<- { "timestamp": { "seconds": 1418751627,
                    "microseconds": 549958 },
     "event": "DEVICE_TRAY_MOVED",
     "data": { "device": "ide1-cd0",
               "tray-open": true } }

<- { "return": {} }

-> { "execute": "x-blockdev-remove-medium",
     "arguments": { "device": "ide1-cd0" } }

<- { "return": {} }

EQMP

    {
        .name       = "x-blockdev-insert-medium",
        .args_type  = "device:s,node-name:s",
        .mhandler.cmd_new = qmp_marshal_x_blockdev_insert_medium,
    },

SQMP
x-blockdev-insert-medium
------------------------

Inserts a medium (a block driver state tree) into a block device. That block
device's tray must currently be open (unless there is no attached guest device)
and there must be no medium inserted already.

This command is still a work in progress and is considered experimental.
Stay away from it unless you want to help with its development.

Arguments:

- "device": block device name (json-string)
- "node-name": root node of the BDS tree to insert into the block device

Example:

-> { "execute": "blockdev-add",
     "arguments": { "options": { "node-name": "node0",
                                 "driver": "raw",
                                 "file": { "driver": "file",
                                           "filename": "fedora.iso" } } } }

<- { "return": {} }

-> { "execute": "x-blockdev-insert-medium",
     "arguments": { "device": "ide1-cd0",
                    "node-name": "node0" } }

<- { "return": {} }

EQMP

    {
        .name       = "query-named-block-nodes",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_named_block_nodes,
    },

SQMP
@query-named-block-nodes
------------------------

Return a list of BlockDeviceInfo for all the named block driver nodes

Example:

-> { "execute": "query-named-block-nodes" }
<- { "return": [ { "ro":false,
                   "drv":"qcow2",
                   "encrypted":false,
                   "file":"disks/test.qcow2",
                   "node-name": "my-node",
                   "backing_file_depth":1,
                   "bps":1000000,
                   "bps_rd":0,
                   "bps_wr":0,
                   "iops":1000000,
                   "iops_rd":0,
                   "iops_wr":0,
                   "bps_max": 8000000,
                   "bps_rd_max": 0,
                   "bps_wr_max": 0,
                   "iops_max": 0,
                   "iops_rd_max": 0,
                   "iops_wr_max": 0,
                   "iops_size": 0,
                   "write_threshold": 0,
                   "image":{
                      "filename":"disks/test.qcow2",
                      "format":"qcow2",
                      "virtual-size":2048000,
                      "backing_file":"base.qcow2",
                      "full-backing-filename":"disks/base.qcow2",
                      "backing-filename-format":"qcow2",
                      "snapshots":[
                         {
                            "id": "1",
                            "name": "snapshot1",
                            "vm-state-size": 0,
                            "date-sec": 10000200,
                            "date-nsec": 12,
                            "vm-clock-sec": 206,
                            "vm-clock-nsec": 30
                         }
                      ],
                      "backing-image":{
                          "filename":"disks/base.qcow2",
                          "format":"qcow2",
                          "virtual-size":2048000
                      }
                   } } ] }

EQMP

    {
        .name       = "blockdev-change-medium",
        .args_type  = "device:B,filename:F,format:s?,read-only-mode:s?",
        .mhandler.cmd_new = qmp_marshal_blockdev_change_medium,
    },

SQMP
blockdev-change-medium
----------------------

Changes the medium inserted into a block device by ejecting the current medium
and loading a new image file which is inserted as the new medium.

Arguments:

- "device": device name (json-string)
- "filename": filename of the new image (json-string)
- "format": format of the new image (json-string, optional)
- "read-only-mode": new read-only mode (json-string, optional)
          - Possible values: "retain" (default), "read-only", "read-write"

Examples:

1. Change a removable medium

-> { "execute": "blockdev-change-medium",
             "arguments": { "device": "ide1-cd0",
                            "filename": "/srv/images/Fedora-12-x86_64-DVD.iso",
                            "format": "raw" } }
<- { "return": {} }

2. Load a read-only medium into a writable drive

-> { "execute": "blockdev-change-medium",
             "arguments": { "device": "isa-fd0",
                            "filename": "/srv/images/ro.img",
                            "format": "raw",
                            "read-only-mode": "retain" } }

<- { "error":
     { "class": "GenericError",
       "desc": "Could not open '/srv/images/ro.img': Permission denied" } }

-> { "execute": "blockdev-change-medium",
             "arguments": { "device": "isa-fd0",
                            "filename": "/srv/images/ro.img",
                            "format": "raw",
                            "read-only-mode": "read-only" } }

<- { "return": {} }

EQMP

    {
        .name       = "query-memdev",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_memdev,
    },

SQMP
query-memdev
------------

Show memory devices information.


Example (1):

-> { "execute": "query-memdev" }
<- { "return": [
       {
         "size": 536870912,
         "merge": false,
         "dump": true,
         "prealloc": false,
         "host-nodes": [0, 1],
         "policy": "bind"
       },
       {
         "size": 536870912,
         "merge": false,
         "dump": true,
         "prealloc": true,
         "host-nodes": [2, 3],
         "policy": "preferred"
       }
     ]
   }

EQMP

    {
        .name       = "query-memory-devices",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_memory_devices,
    },

SQMP
@query-memory-devices
--------------------

Return a list of memory devices.

Example:
-> { "execute": "query-memory-devices" }
<- { "return": [ { "data":
                      { "addr": 5368709120,
                        "hotpluggable": true,
                        "hotplugged": true,
                        "id": "d1",
                        "memdev": "/objects/memX",
                        "node": 0,
                        "size": 1073741824,
                        "slot": 0},
                   "type": "dimm"
                 } ] }
EQMP

    {
        .name       = "query-acpi-ospm-status",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_query_acpi_ospm_status,
    },

SQMP
@query-acpi-ospm-status
--------------------

Return list of ACPIOSTInfo for devices that support status reporting
via ACPI _OST method.

Example:
-> { "execute": "query-acpi-ospm-status" }
<- { "return": [ { "device": "d1", "slot": "0", "slot-type": "DIMM", "source": 1, "status": 0},
                 { "slot": "1", "slot-type": "DIMM", "source": 0, "status": 0},
                 { "slot": "2", "slot-type": "DIMM", "source": 0, "status": 0},
                 { "slot": "3", "slot-type": "DIMM", "source": 0, "status": 0}
   ]}
EQMP

#if defined TARGET_I386
    {
        .name       = "rtc-reset-reinjection",
        .args_type  = "",
        .mhandler.cmd_new = qmp_marshal_rtc_reset_reinjection,
    },
#endif

SQMP
rtc-reset-reinjection
---------------------

Reset the RTC interrupt reinjection backlog.

Arguments: None.

Example:

-> { "execute": "rtc-reset-reinjection" }
<- { "return": {} }
EQMP

    {
        .name       = "trace-event-get-state",
        .args_type  = "name:s",
        .mhandler.cmd_new = qmp_marshal_trace_event_get_state,
    },

SQMP
trace-event-get-state
---------------------

Query the state of events.

Example:

-> { "execute": "trace-event-get-state", "arguments": { "name": "qemu_memalign" } }
<- { "return": [ { "name": "qemu_memalign", "state": "disabled" } ] }
EQMP

    {
        .name       = "trace-event-set-state",
        .args_type  = "name:s,enable:b,ignore-unavailable:b?",
        .mhandler.cmd_new = qmp_marshal_trace_event_set_state,
    },

SQMP
trace-event-set-state
---------------------

Set the state of events.

Example:

-> { "execute": "trace-event-set-state", "arguments": { "name": "qemu_memalign", "enable": "true" } }
<- { "return": {} }
EQMP

    {
        .name       = "x-input-send-event",
        .args_type  = "console:i?,events:q",
        .mhandler.cmd_new = qmp_marshal_x_input_send_event,
    },

SQMP
@x-input-send-event
-----------------

Send input event to guest.

Arguments:

- "console": console index. (json-int, optional)
- "events": list of input events.

The consoles are visible in the qom tree, under
/backend/console[$index]. They have a device link and head property, so
it is possible to map which console belongs to which device and display.

Note: this command is experimental, and not a stable API.

Example (1):

Press left mouse button.

-> { "execute": "x-input-send-event",
    "arguments": { "console": 0,
                   "events": [ { "type": "btn",
                    "data" : { "down": true, "button": "Left" } } ] } }
<- { "return": {} }

-> { "execute": "x-input-send-event",
    "arguments": { "console": 0,
                   "events": [ { "type": "btn",
                    "data" : { "down": false, "button": "Left" } } ] } }
<- { "return": {} }

Example (2):

Press ctrl-alt-del.

-> { "execute": "x-input-send-event",
     "arguments": { "console": 0, "events": [
        { "type": "key", "data" : { "down": true,
          "key": {"type": "qcode", "data": "ctrl" } } },
        { "type": "key", "data" : { "down": true,
          "key": {"type": "qcode", "data": "alt" } } },
        { "type": "key", "data" : { "down": true,
          "key": {"type": "qcode", "data": "delete" } } } ] } }
<- { "return": {} }

Example (3):

Move mouse pointer to absolute coordinates (20000, 400).

-> { "execute": "x-input-send-event" ,
  "arguments": { "console": 0, "events": [
               { "type": "abs", "data" : { "axis": "X", "value" : 20000 } },
               { "type": "abs", "data" : { "axis": "Y", "value" : 400 } } ] } }
<- { "return": {} }

EQMP

    {
        .name       = "block-set-write-threshold",
        .args_type  = "node-name:s,write-threshold:l",
        .mhandler.cmd_new = qmp_marshal_block_set_write_threshold,
    },

SQMP
block-set-write-threshold
------------

Change the write threshold for a block drive. The threshold is an offset,
thus must be non-negative. Default is no write threshold.
Setting the threshold to zero disables it.

Arguments:

- "node-name": the node name in the block driver state graph (json-string)
- "write-threshold": the write threshold in bytes (json-int)

Example:

-> { "execute": "block-set-write-threshold",
  "arguments": { "node-name": "mydev",
                 "write-threshold": 17179869184 } }
<- { "return": {} }

EQMP

    {
        .name       = "query-rocker",
        .args_type  = "name:s",
        .mhandler.cmd_new = qmp_marshal_query_rocker,
    },

SQMP
Show rocker switch
------------------

Arguments:

- "name": switch name

Example:

-> { "execute": "query-rocker", "arguments": { "name": "sw1" } }
<- { "return": {"name": "sw1", "ports": 2, "id": 1327446905938}}

EQMP

    {
        .name       = "query-rocker-ports",
        .args_type  = "name:s",
        .mhandler.cmd_new = qmp_marshal_query_rocker_ports,
    },

SQMP
Show rocker switch ports
------------------------

Arguments:

- "name": switch name

Example:

-> { "execute": "query-rocker-ports", "arguments": { "name": "sw1" } }
<- { "return": [ {"duplex": "full", "enabled": true, "name": "sw1.1",
                  "autoneg": "off", "link-up": true, "speed": 10000},
                 {"duplex": "full", "enabled": true, "name": "sw1.2",
                  "autoneg": "off", "link-up": true, "speed": 10000}
   ]}

EQMP

    {
        .name       = "query-rocker-of-dpa-flows",
        .args_type  = "name:s,tbl-id:i?",
        .mhandler.cmd_new = qmp_marshal_query_rocker_of_dpa_flows,
    },

SQMP
Show rocker switch OF-DPA flow tables
-------------------------------------

Arguments:

- "name": switch name
- "tbl-id": (optional) flow table ID

Example:

-> { "execute": "query-rocker-of-dpa-flows", "arguments": { "name": "sw1" } }
<- { "return": [ {"key": {"in-pport": 0, "priority": 1, "tbl-id": 0},
                  "hits": 138,
                  "cookie": 0,
                  "action": {"goto-tbl": 10},
                  "mask": {"in-pport": 4294901760}
                 },
                 {...more...},
   ]}

EQMP

    {
        .name       = "query-rocker-of-dpa-groups",
        .args_type  = "name:s,type:i?",
        .mhandler.cmd_new = qmp_marshal_query_rocker_of_dpa_groups,
    },

SQMP
Show rocker OF-DPA group tables
-------------------------------

Arguments:

- "name": switch name
- "type": (optional) group type

Example:

-> { "execute": "query-rocker-of-dpa-groups", "arguments": { "name": "sw1" } }
<- { "return": [ {"type": 0, "out-pport": 2, "pport": 2, "vlan-id": 3841,
                  "pop-vlan": 1, "id": 251723778},
                 {"type": 0, "out-pport": 0, "pport": 0, "vlan-id": 3841,
                  "pop-vlan": 1, "id": 251723776},
                 {"type": 0, "out-pport": 1, "pport": 1, "vlan-id": 3840,
                  "pop-vlan": 1, "id": 251658241},
                 {"type": 0, "out-pport": 0, "pport": 0, "vlan-id": 3840,
                  "pop-vlan": 1, "id": 251658240}
   ]}

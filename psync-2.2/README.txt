Installation: Required for each sync host.
$ ./configure --prefix=$HOME
$ make install
$ mkdir -m700 ~/pSync
$ mkdir -m700 ~/pSync/Private
$ mkdir -m700 ~/pSync/Work
$ cat <<EOF >~/.psync.conf
#ID     Directory
private pSync/Private
work    pSync/Work
EOF
$ chmod 600 ~/.psync.conf
$ 

See also:
  psync(1), psync.conf(5), http://kobayasy.com/psync/

Report bugs to mailto:kobayasy@kobayasy.com with "pSync" in subject.



## Instructions for launching an HVM guest

Tested with Ubuntu 20.04 and Xen 4.11. After installing with aptitude xtightvncviewer and copying the file code-hvm.cfg to /etc/xen/, you can do:


```
$ sudo xl create /etc/xen/code-hvm.cfg

$ sudo vncviewer localhost:0

$ sudo xl list

$ sudo xl destroy <domid>
```

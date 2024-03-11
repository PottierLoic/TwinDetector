# TwinDetector

Little executable that can be used to detect ARP table change.
If an IP entry change mac address, it probably means that two devices use the same.

The tool can be used to log specific address.

# Build

You need to have MinGW64 and Make.
```bash
cd TwinDetector
make XXbit # 64 or 32
```

Providing the addresses to monitor is done via list.txt, each line can take one address.
You can then just run the generated executable.

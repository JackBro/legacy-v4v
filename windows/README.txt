Legacy Windows V4V Driver

This is the Windows V4V driver that works with the legacy V4V hypervisor
bits provided in this repository. The project includes the V4V driver itself
and the xenbus driver that will load the V4V driver if a v4v device node is
present for a domain in xenstore.

This project is for demonstrative purposes only and is not supported nor
will any maintenance work be done for it.

To build the project, use a Windows Vista or later WDK and the standard
build utility. To build the installer, use NSIS 2.23. Note that the
Registry NSIS plugin v3.0 is needed in addition to the base NSIS package.
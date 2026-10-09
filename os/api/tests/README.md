# Instrument loading tests

Install Flask in a host Python environment, then run from the repository root:

```sh
python3 -m unittest discover -s os/api/tests -v
```

Archive extraction, directory swaps and recovery use real temporary files. Every
service invocation is mocked; these tests do not access a board or system services.
The API tests stub the system journal as well.

Measured status-polling performance is recorded in
[status-performance.md](status-performance.md), with Red Pitaya raw samples.

The loader validates/extracts before stopping the current service, retains its
files until the replacement reaches systemd readiness, and restores/restarts the
previous installation if startup fails. If file restoration itself fails, it
keeps the backup and prints its path for recovery. It cannot guarantee recovery
from process termination, power loss, or hardware that fails to accept the old
configuration; those require separate system-level testing.

The live identity marker is written by runtime installation and default extraction
at boot. After upgrading only the API on an already-running system, status is
unknown (`null`) until an instrument is reloaded or boot extraction writes the
marker. It never guesses from the boot default. Install requests still rely on
the checked-in single-process, single-thread uWSGI configuration for serialization.

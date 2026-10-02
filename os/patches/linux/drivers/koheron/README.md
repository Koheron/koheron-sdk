# `/dev/cma` buffer contract

Each `open` owns one allocation. Duplicated descriptors and descriptors inherited
through `fork` share that allocation. Independent opens have independent buffers.
The existing `_IOWR('Z', 0, u32)` ioctl accepts a nonzero byte count and returns a
32-bit physical address. Sizes round up to whole pages. Allocations whose full
range does not fit in that address space fail with `EOVERFLOW`.

A successful replacement releases the previous, unmapped allocation. A failed
replacement keeps it, so replacement temporarily needs CMA capacity for both
buffers. Replacement while any mapping survives fails with `EBUSY`,
including mappings in forked processes and pieces left after partial unmapping.
Only shared mappings are supported; page-aligned offsets and lengths must stay
inside the allocation. New allocations are zeroed.

Mappings hold the open file alive. Closing the last descriptor does not release a
mapped buffer; the last mapping must also disappear. Process exit uses the same
kernel cleanup path. Pages are faulted into the mapping as they are accessed.

## Hardware responsibilities

The caller must stop and quiesce every FPGA transfer using the buffer **before**
reallocation or final release. Keeping a descriptor or mapping open retains the
buffer while a transfer is active. This generic allocator has no FPGA register or
completion information and cannot stop transfers on process termination. Systems
that require safe cleanup after a crashed DMA client need a device-specific driver
that owns both transfer shutdown and buffer lifetime.

The returned value is a CPU physical address, not a device-translated DMA address.
The interface retains its existing ordinary cached CPU mappings; it does not
provide coherent DMA allocation or cache synchronization. Callers must establish
that their FPGA path accepts this physical address and obey that platform's cache
maintenance requirements. An IOMMU or noncoherent DMA path needs device-specific
DMA API integration; the ownership fix does not add that support.

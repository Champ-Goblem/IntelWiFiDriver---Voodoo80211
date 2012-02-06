//
//  compat.h
//  net80211
//
//  Copyright (c) 2012 Prashant Vaibhav. All rights reserved.
//

#ifndef net80211_compat_h
#define net80211_compat_h

// BSD compatibility definitions

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include "Voodoo80211Device.h"

#define PCI_CAP_PCIEXPRESS	kIOPCIPCIExpressCapability
#define PCI_MAPREG_START	kIOPCIConfigBaseAddress0
#define IPL_NET			0 // XXX not used
// the following isn't actually used
#define BUS_SPACE_BARRIER_READ	0
#define BUS_SPACE_BARRIER_WRITE	0

typedef int		bus_dma_tag_t;
typedef int		bus_dmamap_t;
typedef int		bus_dma_segment_t;
typedef caddr_t		bus_space_handle_t; // pointer to device memory
typedef int		pci_chipset_tag_t;
typedef caddr_t		bus_addr_t;
typedef mach_vm_size_t	bus_size_t;
typedef IOMemoryMap*	bus_space_tag_t;
typedef IOPCIDevice*	pcitag_t;
typedef uint32_t	pcireg_t;

class pci_intr_handle : public OSObject {
	OSDeclareDefaultStructors(pci_intr_handle)
public:
	IOWorkLoop*		workloop;
	IOInterruptEventSource*	intr;
	IOPCIDevice*		dev;
	void (*func)(void* arg);
	void* arg;
};
typedef pci_intr_handle* pci_intr_handle_t;

struct device {
	int blah;
};

struct workq_task {
	int blah;
};

struct pci_attach_args {
	Voodoo80211Device*	owner;
	pci_chipset_tag_t	pa_pc;
	pcitag_t		pa_tag;
	bus_dma_tag_t		pa_dmat;
};

int		pci_get_capability(pci_chipset_tag_t chipsettag, pcitag_t pcitag, int capid, int *offsetp, pcireg_t *valuep);
pcireg_t	pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg);
void		pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t val);
pcireg_t	pci_mapreg_type(pci_chipset_tag_t pc, pcitag_t tag, int reg);
int		pci_mapreg_map(const struct pci_attach_args *pa, int reg, pcireg_t type, int busflags, bus_space_tag_t *tagp,
			       bus_space_handle_t *handlep, bus_addr_t *basep, bus_size_t *sizep, bus_size_t maxsize);
int		pci_intr_map_msi(struct pci_attach_args *paa, pci_intr_handle_t *ih);
int		pci_intr_map(struct pci_attach_args *paa, pci_intr_handle_t *ih);
void*		pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih, int level, int (*handler)(void *), void *arg);
void		pci_intr_disestablish(pci_chipset_tag_t pc, void *ih);

uint32_t	bus_space_read_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset);
void		bus_space_write_4(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset, uint32_t value);
void		bus_space_barrier(bus_space_tag_t space, bus_space_handle_t handle, bus_size_t offset, bus_size_t length, int flags);

#endif

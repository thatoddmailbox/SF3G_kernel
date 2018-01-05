/*
 * Data structure definition for Rockchip IOMMU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_PLAT_IOMMU_H
#define __ASM_PLAT_IOMMU_H

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/iommu.h>

#include <linux/rockchip_iovmm.h>


struct rk_iovmm {
	struct iommu_domain *domain; /* iommu domain for this iovmm */
	struct gen_pool *vmm_pool;
	struct list_head regions_list;	/* list of rk_vm_region */
	spinlock_t lock; /* lock for updating regions_list */
};

struct iommu_drvdata {
	struct list_head node; /* entry of rk_iommu_domain.clients */
	struct device *iommu_dev;	/*  IOMMU's device descriptor */
	int num_mem;
	int num_irq;
	int irq_array[4];
	bool irq_requested[4];
	const char *dbgname;
	void __iomem **res_bases;
	int activations;
	struct iommu_domain *domain; /* domain given to iommu_attach_device() */
	unsigned int pgtable;
	struct rk_iovmm vmm;
	rockchip_iommu_fault_handler_t fault_handler;
	unsigned long mmu_flag;
};

#ifdef CONFIG_ROCKCHIP_IOVMM

#define IOVA_START 0x10000000
#define IOVM_SIZE (SZ_1G - SZ_4K) /* last 4K is for error values */

struct rk_vm_region {
	struct list_head node;
	u32 start;
	u32 size;
};

static inline struct rk_iovmm *rockchip_get_iovmm(struct device *dev)
{
	struct iommu_drvdata *data = dev_get_drvdata(dev->archdata.iommu);

	BUG_ON(!dev->archdata.iommu || !data);

	return &data->vmm;
}

int rockchip_init_iovmm(struct device *iommu, struct rk_iovmm *vmm);
#else
static inline int rockchip_init_iovmm(struct device *iommu,
				struct rk_iovmm *vmm)
{
	return -ENOSYS;
}
#endif


#ifdef CONFIG_ROCKCHIP_IOMMU

/**
 * rockchip_iommu_tlb_invalidate() - flush all TLB entry in iommu
 * @owner: The device whose IOMMU.
 *
 * This function flush all TLB entry in iommu
 */
int rockchip_iommu_tlb_invalidate(struct device *owner);
int rockchip_iommu_tlb_invalidate_global(struct device *owner);

#else /* CONFIG_ROCKCHIP_IOMMU */
static inline int rockchip_iommu_tlb_invalidate(struct device *owner)
{
	return -1;
}
static int rockchip_iommu_tlb_invalidate_global(struct device *owner)
{
	return -1;
}

#endif

#endif	/*__ASM_PLAT_IOMMU_H*/

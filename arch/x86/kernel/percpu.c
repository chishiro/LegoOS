/*
 * Copyright (c) 2016-2017 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <lego/mm.h>
#include <lego/acpi.h>
#include <lego/percpu.h>
#include <lego/cpumask.h>
#include <lego/nodemask.h>
#include <lego/memblock.h>

#include <asm/dma.h>
#include <asm/page.h>
#include <asm/numa.h>

static int __init pcpu_cpu_distance(unsigned int from, unsigned int to)
{
#if 0
	if (early_cpu_to_node(from) == early_cpu_to_node(to))
		return LOCAL_DISTANCE;
	else
		return REMOTE_DISTANCE;
#else
	return LOCAL_DISTANCE;
#endif
}

/**
 * pcpu_alloc_bootmem - NUMA friendly alloc_bootmem wrapper for percpu
 * @cpu: cpu to allocate for
 * @size: size allocation in bytes
 * @align: alignment
 *
 * Allocate @size bytes aligned at @align for cpu @cpu.  This wrapper
 * does the right thing for NUMA regardless of the current
 * configuration.
 *
 * RETURNS:
 * Pointer to the allocated area on success, NULL on failure.
 */
static void * __init pcpu_fc_alloc(unsigned int cpu, unsigned long size,
				   unsigned long align)
{
	const unsigned long goal = __pa(MAX_DMA_ADDRESS);
#ifdef CONFIG_NEED_MULTIPLE_NODES
	int node = cpu_to_node(cpu);
	void *ptr;

	if (!node_online(node) || !NODE_DATA(node)) {
		ptr = memblock_virt_alloc_try_nid_nopanic(size, align, goal,
				BOOTMEM_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
		pr_info("cpu %d has no node %d or node-local memory\n",
			cpu, node);
		pr_debug("per cpu data for cpu%d %lu bytes at %016lx\n",
			 cpu, size, __pa(ptr));
	} else {
		ptr = memblock_virt_alloc_try_nid_nopanic(size, align, goal,
				BOOTMEM_ALLOC_ACCESSIBLE, node);
		pr_debug("per cpu data for cpu%d %lu bytes on node%d at PA: %016lx\n",
			 cpu, size, node, __pa(ptr));
	}
	return ptr;
#else
	return memblock_virt_alloc_try_nid_nopanic(size, align, goal,
			BOOTMEM_ALLOC_ACCESSIBLE, NUMA_NO_NODE);
#endif
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
	memblock_free_early(__pa(ptr), size);
}

void __init setup_per_cpu_areas(void)
{
	const size_t dyn_size = PERCPU_DYNAMIC_RESERVE;
	size_t atom_size = PMD_SIZE;
	int ret;

	pr_info("NR_CPUS:%d nr_cpumask_bits:%d nr_cpu_ids:%d nr_node_ids:%d\n",
		NR_CPUS, nr_cpumask_bits, nr_cpu_ids, nr_node_ids);

	ret = pcpu_embed_first_chunk(0, dyn_size, atom_size,
				    pcpu_cpu_distance,
				    pcpu_fc_alloc, pcpu_fc_free);
	if (ret < 0)
		panic("cannot initliaze percpu area (err=%d)", ret);
}

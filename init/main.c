/*
 * Copyright (c) 2016 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <disos/panic.h>
#include <disos/linkage.h>

asmlinkage void __init start_kernel(void)
{
	asm (
		"1: hlt\n"
		"jmp 1b\n"
	);
	panic("End");
}

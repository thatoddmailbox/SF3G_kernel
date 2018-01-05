/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 */

#ifndef _LINUX_ROCKCHIP_ION_SNAPSHOT_H
#define _LINUX_ROCKCHIP_ION_SNAPSHOT_H

char *rockchip_ion_snapshot_get(unsigned *size);
int rockchip_ion_snapshot_debugfs(struct dentry *root);

#endif

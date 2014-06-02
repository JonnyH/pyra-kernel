/*
 * Virtual gpio to allow ON/OFF control of a WWAN module.
 *
 * Copyright (C) 2014 H. Nikolaus Schaller <hns@goldelico.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __LINUX_WWAN_ON_OFF_H
#define __LINUX_WWAN_ON_OFF_H


struct wwan_on_off_data {
	int	gpio_base;		/* (not used by DT) - defines the gpio.base */
	int	on_off_gpio;	/* connected to the on-off input of the GPS module */
	int	feedback_gpio;	/* a status feedback to report module power state */
	/* to be added: a link to some USB PHY */
};
#endif /* __LINUX_WWAN_ON_OFF_H */

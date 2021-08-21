/*
 * Copyright (C) 2021 Min Le (lemin9538@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <minos/minos.h>
#include <minos/kobject.h>
#include <minos/uaccess.h>
#include <minos/mm.h>
#include <minos/sched.h>
#include <minos/poll.h>

#define NOTIFY_RIGHT		(KOBJ_RIGHT_WRITE)
#define NOTIFY_RIGHT_MASK	(KOBJ_RIGHT_WRITE)

static long notify_kobj_send(struct kobject *kobj, void __user *data,
			size_t data_size, void __user *extra,
			size_t extra_size, uint32_t timeout)
{
	unsigned long msg[3];
	int ret;

	if (extra_size > sizeof(msg))
		return -E2BIG;

	memset(msg, 0, sizeof(msg));
	ret = copy_from_user(msg, extra, extra_size);
	if (ret < 0)
		return ret;

	poll_event_send_with_data(kobj->poll_struct, EV_IN, 0,
			msg[0], msg[1], msg[2]);

	return 0;
}

struct kobject_ops notify_kobj_ops = {
	.send		= notify_kobj_send,
};

static int notify_check_right(right_t right, right_t right_req)
{
	if (right != NOTIFY_RIGHT)
		return 0;

	if (right_req != KOBJ_RIGHT_WRITE)
		return 0;

	return 1;
}

static struct kobject *notify_create(right_t right,
		right_t right_req, unsigned long data)
{
	struct kobject *kobj;

	if (!notify_check_right(right, right_req))
		return ERROR_PTR(-EPERM);

	kobj = zalloc(sizeof(struct kobject));
	if (!kobj)
		return ERROR_PTR(-ENOMEM);

	kobject_init(kobj, KOBJ_TYPE_NOTIFY, right, 0);
	kobj->ops = &notify_kobj_ops;

	return kobj;
}
DEFINE_KOBJECT(notify, KOBJ_TYPE_NOTIFY, notify_create);
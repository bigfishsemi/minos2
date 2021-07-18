/*
 * Copyright (C) 2020 Min Le (lemin9538@gmail.com)
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
#include <minos/irq.h>
#include <minos/sched.h>
#include <minos/vspace.h>
#include <minos/proc.h>
#include <minos/arch.h>
#include <minos/console.h>
#include <minos/kobject.h>
#include <minos/uaccess.h>

void sys_sched_yield(void)
{
	local_irq_enable();
	sched();
	local_irq_disable();
}

long sys_kobject_connect(char __user *path, right_t right)
{
	char name[FILENAME_MAX];
	int ret;

	ret = copy_string_from_user_safe(name, path, FILENAME_MAX);
	if (ret <= 0)
		return -EINVAL;

	return kobject_connect(name, right);
}

long sys_kobject_close(handle_t handle)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	/*
	 * release the handle first, then other thread in
	 * this process can not see this kobject now. if
	 * one thread in this process
	 */
	release_handle(handle);

	ret = kobject_close(kobj, right);

	put_kobject(kobj);

	return ret;
}

long sys_kobject_listen(handle_t handle, int event)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	if (!(right & KOBJ_STATE_OPENED)) {
		ret = -EBADF;
		goto out;
	}

	ret = kobject_listen(kobj, handle, right, event);
out:
	put_kobject(kobj);
	return ret;
}

handle_t sys_kobject_create(char __user *name, int type, right_t right,
		right_t right_req, unsigned long data)
{
	char str[FILENAME_MAX];
	struct kobject *kobj;
	handle_t handle;
	int ret;

	if ((type >= KOBJ_TYPE_MAX) || (type <= 0))
		return -EINVAL;

	if (name != NULL) {
		ret = copy_string_from_user_safe(str, name, FILENAME_MAX);
		if (ret <= 0)
			return ret;
	} else {
		str[0] = 0;
	}

	kobj = kobject_create(str, type, right, right_req, data);
	if (IS_ERROR_PTR(kobj))
		return (handle_t)(long)(kobj);

	/*
	 * visable for all the threads in this process.
	 */
	handle = alloc_handle(kobj, right_req | KOBJ_STATE_OPENED);
	if (handle <= 0) {
		kobject_put(kobj);
		return -ENOSPC;
	}

	/*
	 * other process can see this kobject and can connect it
	 * using connect() syscall.
	 */
	kobject_add(kobj);

	return handle;
}

int sys_kobject_open(handle_t handle)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	ret = kobject_open(kobj, handle, right);
	put_kobject(kobj);

	return ret;
}

long sys_kobject_destroy(handle_t handle)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	/*
	 * consider race condition to be done.
	 */
	if (right & KOBJ_STATE_OPENED)
		goto out;

	ret = kobject_destroy(kobj, right);

out:
	put_kobject(kobj);

	return ret;
}

ssize_t sys_kobject_recv(handle_t handle, void __user *data, size_t data_size,
		void __user *extra, size_t extra_size, uint32_t timeout)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	if (!(right & KOBJ_STATE_OPENED)) {
		ret = -EBADF;
		goto out;
	}

	if (!(right & KOBJ_RIGHT_READ)) {
		ret = -EPERM;
		goto out;
	}

	ret = kobject_recv(kobj, data, data_size, extra, extra_size, timeout);
out:
	put_kobject(kobj);
	return ret;
}

ssize_t sys_kobject_send(handle_t handle, void __user *data, size_t data_size,
		void __user *extra, size_t extra_size, uint32_t timeout)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	if (!(right & KOBJ_STATE_OPENED)) {
		ret = -EBADF;
		goto out;
	}

	if (!(right & KOBJ_RIGHT_WRITE)) {
		ret = -EPERM;
		goto out;
	}

	ret = kobject_send(kobj, data, data_size, extra, extra_size, timeout);
out:
	put_kobject(kobj);
	return ret;
}

long sys_kobject_reply(handle_t handle, unsigned long token, int err_code)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	if (!(right & KOBJ_STATE_OPENED)) {
		ret = -EBADF;
		goto out;
	}

	ret = kobject_reply(kobj, right, token, err_code);
out:
	put_kobject(kobj);

	return ret;
}

long sys_kobject_ctl(handle_t handle, int req, unsigned long data)
{
	struct kobject *kobj;
	right_t right;
	unsigned long ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	if (!(right & KOBJ_STATE_OPENED)) {
		ret = -EBADF;
		goto out;
	}

	if (!(right & KOBJ_RIGHT_CTL)) {
		ret = -EPERM;
		goto out;
	}

	ret = kobject_ctl(kobj, right, req, data);
out:
	put_kobject(kobj);

	return ret;
}

void *sys_kobject_mmap(handle_t handle)
{
	struct kobject *kobj;
	right_t right;
	int ret;
	void *out = (void *)-1;

	if (WRONG_HANDLE(handle))
		return (void *)-1;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return (void *)-1;

	if (!(right & KOBJ_STATE_OPENED))
		goto out;

	if (!(right & KOBJ_RIGHT_MMAP))
		goto out;

	out = kobject_mmap(kobj, right);
out:
	put_kobject(kobj);
	return out;
}

int sys_kobject_munmap(handle_t handle)
{
	struct kobject *kobj;
	right_t right;
	int ret;

	if (WRONG_HANDLE(handle))
		return -ENOENT;

	ret = get_kobject(handle, &kobj, &right);
	if (ret)
		return ret;

	if (!(right & KOBJ_STATE_OPENED)) {
		ret = -EBADF;
		goto out;
	}

	if (!(right & KOBJ_RIGHT_MMAP)) {
		ret = -EPERM;
		goto out;
	}

	ret = kobject_munmap(kobj, right);
out:
	put_kobject(kobj);
	return ret;
}

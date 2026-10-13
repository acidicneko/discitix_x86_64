#include <kernel/vfs/vfs.h>
#include <kernel/sched/scheduler.h>
#include <libk/utils.h>
#include <mm/liballoc.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <libk/errno.h>

static vfs_mount_t *mounts = NULL;
static superblock_t *root_superblock = NULL;

int vfs_mount(superblock_t *sb, const char *mount_point) {
    if (!sb || !mount_point) return -1;
    log("VFS", INFO, "mounting fs '%s' at '%s'\n\r", sb->fs_type, mount_point);

    vfs_mount_t *m = (vfs_mount_t *)pmalloc(1);
    if (!m) return -1;
    memset(m, 0, sizeof(vfs_mount_t));

    strncpy(m->mount_point, mount_point, NAME_MAX - 1);
    m->sb = sb;
    m->next = mounts;
    mounts = m;

    if (mount_point[0] == '/' && mount_point[1] == '\0') {
        root_superblock = sb;
    } else {
        dentry_t *mnt_dentry = vfs_get_dentry(mount_point);
        if (mnt_dentry) {
            mnt_dentry->mounted_here = m; 
            sb->root->parent = mnt_dentry->parent;
            strncpy(sb->root->name, mnt_dentry->name, NAME_MAX - 1);
        }
    }
    return 0;
}

int vfs_lookup(inode_t *parent, const char *name, inode_t **result_inode) {
    if (!parent || !name || !result_inode) return -EINVAL;
    if (!parent->is_directory) return -ENOTDIR;
    if (!parent->i_ops || !parent->i_ops->lookup) return -ENOENT;

    inode_t *found_inode = parent->i_ops->lookup(parent, name);
    if (!found_inode) return -ENOENT;

    *result_inode = found_inode;
    return 0;
}

int vfs_lookup_path(const char *path, inode_t **result_inode) {
    if (!path || !result_inode) return -EFAULT;

    int err = 0;
    dentry_t *d = vfs_walk(path, &err);
    if (!d) return err;

    *result_inode = d->inode;
    return 0;
}

dentry_t *vfs_resolve_parent(const char *path, char *buf, char **name, int *err) {
    if (!path || !*path) { *err = -ENOENT; return NULL; }
    int len = strlen(path);
    if (len >= PATH_MAX) { *err = -ENAMETOOLONG; return NULL; }
    strcpy(buf, path);
    while (len > 1 && buf[len - 1] == '/') buf[--len] = '\0';

    char *slash = NULL;
    for (int i = len - 1; i >= 0; i--) {
        if (buf[i] == '/') { slash = &buf[i]; break; }
    }

    dentry_t *parent;
    if (slash) {
        *slash = '\0';
        *name = slash + 1;
        parent = (slash == buf) ? vfs_get_root_dentry() : vfs_walk(buf, err);
        if (!parent) {
            if (slash == buf) *err = -ENOENT;
            return NULL;
        }
    } else {
        *name = buf;
        task_t *task = get_current_task();
        parent = (task && task->cwd) ? (dentry_t *)task->cwd : vfs_get_root_dentry();
        if (!parent) { *err = -ENOENT; return NULL; }
    }

    if (!parent->inode || !parent->inode->is_directory) { *err = -ENOTDIR; return NULL; }
    if (!**name || !strcmp(*name, ".") || !strcmp(*name, "..")) { *err = -EINVAL; return NULL; }
    if (strlen(*name) >= NAME_MAX) { *err = -ENAMETOOLONG; return NULL; }
    return parent;
}

superblock_t *vfs_get_root_superblock() {
	return root_superblock;
}

// Default directory lookup for virtual directories
static inode_t *vfs_default_dir_lookup(inode_t *parent, const char *name) {
	if (!parent || !parent->private) return NULL;
	dentry_t *parent_d = (dentry_t *)parent->private;
	dentry_t *child = parent_d->children;
	while (child) {
		if (!strcmp(child->name, name)) {
			return child->inode;
		}
		child = child->next;
	}
	return NULL;
}

static inode_operations_t vfs_dir_iops = {
	.lookup = vfs_default_dir_lookup,
	.create = NULL,
	.mkdir = NULL,
	.unlink = NULL,
};

// Create a directory at a given parent inode
int vfs_mkdir_at(inode_t *parent, const char *name) {
	if (!parent || !name) return -EINVAL;
	if (!parent->is_directory) return -ENOTDIR;

	dentry_t *parent_d = (dentry_t *)parent->private;
	if (!parent_d) return -EROFS;

	dentry_t *child = parent_d->children;
	while (child) {
		if (!strcmp(child->name, name)) {
			return -EEXIST;
		}
		child = child->next;
	}

	// Create new dentry
	dentry_t *new_dentry = (dentry_t *)pmalloc(1);
	if (!new_dentry) return -ENOMEM;
	memset(new_dentry, 0, 4096);
	strncpy(new_dentry->name, name, NAME_MAX - 1);
	new_dentry->parent = parent_d;

	// Create new inode
	inode_t *new_inode = (inode_t *)pmalloc(1);
	if (!new_inode) {
		pmm_free_pages(new_dentry, 1);
		return -ENOMEM;
	}
	memset(new_inode, 0, 4096);
	new_inode->is_directory = 1;
	new_inode->type = FT_DIR;
	new_inode->i_ops = &vfs_dir_iops;
	new_inode->f_ops = NULL;
	new_inode->private = (void *)new_dentry;
	new_inode->mode = 5;

	new_dentry->inode = new_inode;

	// Add to parent's children list
	new_dentry->next = parent_d->children;
	parent_d->children = new_dentry;

	log("VFS",INFO,"created directory '%s'\n\r", name);
	return 0;
}
int vfs_mkdir(const char *path) {
    char buf[PATH_MAX];
    char *dir_name;
    int err = 0;
    dentry_t *parent_dentry = vfs_resolve_parent(path, buf, &dir_name, &err);
    if (!parent_dentry) return err;

    log("VFS",INFO,"vfs_mkdir: Attempting to create '%s' inside folder '%s'\n\r", dir_name, parent_dentry->name);
    if (parent_dentry->inode->i_ops && parent_dentry->inode->i_ops->mkdir) {
        return parent_dentry->inode->i_ops->mkdir(parent_dentry->inode, dir_name);
    }

    // Virtual RAM-backed directories (stripFS, /dev, /proc, ...)
    return vfs_mkdir_at(parent_dentry->inode, dir_name);
}

// Get root dentry
dentry_t *vfs_get_root_dentry(void) {
	if (!root_superblock || !root_superblock->root) {
		return NULL;
	}
	return root_superblock->root;
}

int vfs_chdir(const char *path) {
    if (!path) return -EFAULT;
    task_t *task = get_current_task();
    if (!task) return -ESRCH;

    int err = 0;
    dentry_t *dentry = vfs_walk(path, &err);
    if (!dentry) return err;
    if (!dentry->inode || !dentry->inode->is_directory) return -ENOTDIR;

    task->cwd = (void *)dentry;
    return 0;
}

int vfs_getcwd(char *buf, size_t size) {
	if (!buf) return -EFAULT;
	if (size == 0) return -EINVAL;

	task_t *task = get_current_task();
	dentry_t *cwd;
	
	if (task && task->cwd) {
		cwd = (dentry_t *)task->cwd;
	} else {
		cwd = root_superblock->root;
	}

	// Build path by walking up to root
	char tmp[PATH_MAX];
	int pos = PATH_MAX - 1;
	tmp[pos] = '\0';

	dentry_t *d = cwd;
	while (d && d->parent) {
		int name_len = strlen(d->name);
		pos -= name_len;
		if (pos < 1) return -ENAMETOOLONG;
		memcpy((uint8_t*)&tmp[pos], (uint8_t*)d->name, name_len);
		pos--;
		tmp[pos] = '/';
		d = d->parent;
	}

	// If we're at root
	if (pos == PATH_MAX - 1) {
		tmp[--pos] = '/';
	}

	int len = PATH_MAX - pos;
	if ((size_t)len > size) return -ERANGE;

	memcpy((uint8_t*)buf, (uint8_t*)&tmp[pos], len);
	return 0;
}

dentry_t *vfs_get_dentry(const char *path) {
    int err;
    return vfs_walk(path, &err);
}

dentry_t *vfs_walk(const char *path, int *err) {
    *err = -ENOENT;
    if (!path || !root_superblock || !root_superblock->root) return NULL;
    if (!*path) return NULL;
    if (strlen(path) >= PATH_MAX) { *err = -ENAMETOOLONG; return NULL; }
    if (strcmp(path, "/") == 0) return root_superblock->root;

    dentry_t *current;
    if (path[0] == '/') {
        current = root_superblock->root;
    } else {
        task_t *task = get_current_task();
        current = (task && task->cwd) ? (dentry_t *)task->cwd : root_superblock->root;
    }

    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX - 1);
    path_copy[PATH_MAX - 1] = '\0';

    char *token = strtok(path_copy, "/");
    while (token) {
        if (!current->inode || !current->inode->is_directory) { *err = -ENOTDIR; return NULL; }

        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        } else if (strcmp(token, "..") == 0) {
            if (current->parent) current = current->parent;
            token = strtok(NULL, "/");
            continue;
        }

        // 1. Search the RAM Cache
        dentry_t *child = current->children;
        dentry_t *found = NULL;
        while (child) {
            if (!strcmp(child->name, token)) {
                found = child;
                break;
            }
            child = child->next;
        }

        // 2. Dynamic Dentry Creation (Crucial for FAT32!)
        // If it's not in RAM, ask the disk driver to find it
        if (!found) {
            inode_t *next_inode = NULL;
            int lerr = vfs_lookup(current->inode, token, &next_inode);
            if (lerr != 0 || !next_inode) {
                *err = lerr ? lerr : -ENOENT;
                return NULL;
            }

            // Cache it in RAM as a new dentry (vfs_unlink kfree()s these)
            found = (dentry_t *)kmalloc(sizeof(dentry_t));
            if (!found) { *err = -ENOMEM; return NULL; }
            memset(found, 0, sizeof(dentry_t));
            strncpy(found->name, token, NAME_MAX - 1);
            found->inode = next_inode;
            found->parent = current;
            
            found->next = current->children;
            current->children = found;
        }

        current = found;

        // 3. POSIX MOUNT TRAVERSAL (The Magic)
        // If we step onto a "Covered Dentry", instantly fall through the portal!
        if (current->mounted_here) {
            current = current->mounted_here->sb->root;
        }

        token = strtok(NULL, "/");
    }

    return current;
}
int vfs_unlink(const char *path) {
    char buf[PATH_MAX];
    char *file_name;
    int err = 0;
    dentry_t *parent_dentry = vfs_resolve_parent(path, buf, &file_name, &err);
    if (!parent_dentry) return err;
    if (!parent_dentry->inode->i_ops || !parent_dentry->inode->i_ops->unlink) return -EROFS;

    int result = parent_dentry->inode->i_ops->unlink(parent_dentry->inode, file_name);
    if (result != 0) return result;

    // Evict the stale dentry (and its inode) from the in-RAM cache so the
    // next lookup for this name is forced back to the disk driver instead
    // of returning a pointer to a since-deleted inode/cluster.
    dentry_t *prev = NULL;
    dentry_t *child = parent_dentry->children;
    while (child) {
        if (!strcmp(child->name, file_name)) {
            if (prev) prev->next = child->next;
            else parent_dentry->children = child->next;

            if (child->inode) {
                if (child->inode->private) kfree(child->inode->private); // fat32_node_info_t
                kfree(child->inode);
            }
            kfree(child);
            break;
        }
        prev = child;
        child = child->next;
    }

    return 0;
}


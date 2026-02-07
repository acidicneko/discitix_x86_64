#include <kernel/vfs/vfs.h>
#include <libk/utils.h>
#include <libk/string.h>
#include <mm/pmm.h>

static vfs_mount_t *mounts = NULL;
static superblock_t *root_superblock = NULL;

// only supports mounting at root for now

int vfs_mount(superblock_t *sb, const char *mount_point) {
	if (!sb || !mount_point) return -1;

	dbgln("VFS: mounting fs '%s' at '%s'\n\r", sb->fs_type, mount_point);

	vfs_mount_t *m = (vfs_mount_t *)pmalloc(1);
	if (!m) return -1;
	memset(m, 0, sizeof(vfs_mount_t));

	strncpy(m->mount_point, mount_point, NAME_MAX - 1);
	m->sb = sb;
	m->next = mounts;
	mounts = m;

	if (mount_point[0] == '/' && mount_point[1] == '\0') {
		root_superblock = sb;
	}

	return 0;
}

int vfs_lookup(inode_t *parent, const char *name, inode_t **result_inode) {
    if (!parent || !name || !result_inode) return -1;

    if (!parent->is_directory || !parent->i_ops || !parent->i_ops->lookup) {
        return -1;
    }

    inode_t *found_inode = parent->i_ops->lookup(parent, name);
    if (!found_inode) {
        return -1;
    }

    *result_inode = found_inode;
    return 0;
}


int vfs_lookup_path(const char *path, inode_t **result_inode) {
	if (!path || !result_inode) return -1;

	if (!root_superblock || !root_superblock->root || !root_superblock->root->inode) {
		return -1;
	}

	inode_t *current_inode = root_superblock->root->inode;

	char path_copy[PATH_MAX];
	strncpy(path_copy, path, PATH_MAX - 1);
	path_copy[PATH_MAX - 1] = '\0';

	char *token = strtok(path_copy, "/");
	while (token) {
		if (!current_inode->is_directory) {
			return -1;
		}

		inode_t *next_inode = NULL;
		if (vfs_lookup(current_inode, token, &next_inode) != 0) {
			return -1;
		}

		current_inode = next_inode;
		token = strtok(NULL, "/");
	}

	*result_inode = current_inode;
	return 0;
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

// Get the dentry for a given path
dentry_t *vfs_get_dentry(const char *path) {
	if (!path) return NULL;

	if (!root_superblock || !root_superblock->root) {
		return NULL;
	}

	// Root directory case
	if (path[0] == '/' && path[1] == '\0') {
		return root_superblock->root;
	}

	dentry_t *current = root_superblock->root;

	char path_copy[PATH_MAX];
	strncpy(path_copy, path, PATH_MAX - 1);
	path_copy[PATH_MAX - 1] = '\0';

	char *token = strtok(path_copy, "/");
	while (token) {
		if (!current->inode || !current->inode->is_directory) {
			return NULL;
		}

		// Search in children
		dentry_t *child = current->children;
		dentry_t *found = NULL;
		while (child) {
			if (!strcmp(child->name, token)) {
				found = child;
				break;
			}
			child = child->next;
		}

		if (!found) return NULL;
		current = found;
		token = strtok(NULL, "/");
	}

	return current;
}

// Create a directory at a given parent inode
int vfs_mkdir_at(inode_t *parent, const char *name) {
	if (!parent || !name || !parent->is_directory) return -1;

	dentry_t *parent_d = (dentry_t *)parent->private;
	if (!parent_d) return -1;

	// Check if directory already exists
	dentry_t *child = parent_d->children;
	while (child) {
		if (!strcmp(child->name, name)) {
			return -1; // Already exists
		}
		child = child->next;
	}

	// Create new dentry
	dentry_t *new_dentry = (dentry_t *)pmalloc(1);
	if (!new_dentry) return -1;
	memset(new_dentry, 0, 4096);
	strncpy(new_dentry->name, name, NAME_MAX - 1);
	new_dentry->parent = parent_d;

	// Create new inode
	inode_t *new_inode = (inode_t *)pmalloc(1);
	if (!new_inode) {
		pmm_free_pages(new_dentry, 1);
		return -1;
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

	dbgln("VFS: created directory '%s'\n\r", name);
	return 0;
}

// Create a directory at an absolute path
int vfs_mkdir(const char *path) {
	if (!path || path[0] != '/') return -1;

	if (!root_superblock || !root_superblock->root || !root_superblock->root->inode) {
		return -1;
	}

	char path_copy[PATH_MAX];
	strncpy(path_copy, path, PATH_MAX - 1);
	path_copy[PATH_MAX - 1] = '\0';

	// Find parent directory and target name
	char *last_slash = NULL;
	for (int i = strlen(path_copy) - 1; i >= 0; i--) {
		if (path_copy[i] == '/') {
			last_slash = &path_copy[i];
			break;
		}
	}

	if (!last_slash) return -1;

	char *dir_name = last_slash + 1;
	if (*dir_name == '\0') return -1; // Path ends with /

	inode_t *parent_inode;
	if (last_slash == path_copy) {
		// Creating in root directory
		parent_inode = root_superblock->root->inode;
	} else {
		*last_slash = '\0';
		if (vfs_lookup_path(path_copy, &parent_inode) != 0) {
			return -1;
		}
	}

	return vfs_mkdir_at(parent_inode, dir_name);
}

// Register a device at a given path
int vfs_register_device(const char *path, inode_t *device_inode) {
	if (!path || !device_inode || path[0] != '/') return -1;

	if (!root_superblock || !root_superblock->root || !root_superblock->root->inode) {
		return -1;
	}

	char path_copy[PATH_MAX];
	strncpy(path_copy, path, PATH_MAX - 1);
	path_copy[PATH_MAX - 1] = '\0';

	// Find parent directory and device name
	char *last_slash = NULL;
	for (int i = strlen(path_copy) - 1; i >= 0; i--) {
		if (path_copy[i] == '/') {
			last_slash = &path_copy[i];
			break;
		}
	}

	if (!last_slash) return -1;

	char *dev_name = last_slash + 1;
	if (*dev_name == '\0') return -1;

	inode_t *parent_inode;
	dentry_t *parent_dentry;

	if (last_slash == path_copy) {
		// Device in root directory
		parent_inode = root_superblock->root->inode;
		parent_dentry = root_superblock->root;
	} else {
		*last_slash = '\0';
		if (vfs_lookup_path(path_copy, &parent_inode) != 0) {
			return -1;
		}
		parent_dentry = (dentry_t *)parent_inode->private;
	}

	if (!parent_inode->is_directory || !parent_dentry) return -1;

	// Check if device already exists
	dentry_t *child = parent_dentry->children;
	while (child) {
		if (!strcmp(child->name, dev_name)) {
			return -1; // Already exists
		}
		child = child->next;
	}

	// Create new dentry for the device
	dentry_t *new_dentry = (dentry_t *)pmalloc(1);
	if (!new_dentry) return -1;
	memset(new_dentry, 0, 4096);
	strncpy(new_dentry->name, dev_name, NAME_MAX - 1);
	new_dentry->parent = parent_dentry;
	new_dentry->inode = device_inode;

	// Add to parent's children list
	new_dentry->next = parent_dentry->children;
	parent_dentry->children = new_dentry;

	dbgln("VFS: registered device '%s' at '%s'\n\r", dev_name, path);
	return 0;
}

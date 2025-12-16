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

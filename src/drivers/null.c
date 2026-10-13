#include <libk/utils.h>
#include <drivers/null.h>
#include <kernel/vfs/vfs.h>
#include <fs/devfs.h>


long null_device_read(file_t* file, void * buf, size_t len, uint64_t off){
  (void)file;(void)buf;(void)len;(void)off;
  return 0;
}

long null_device_write(file_t* file, const void * buf, size_t len, uint64_t off){
  (void)file;(void)buf;(void)off;
  return len;
}

file_operations_t null_ops = {
  .read = null_device_read,
  .write = null_device_write
};

void init_null_device(){
  if(devfs_register_device("null", &null_ops, FT_CHR, NULL)==0){
    log("NULL", INFO, "null device initialised.");
  }
}



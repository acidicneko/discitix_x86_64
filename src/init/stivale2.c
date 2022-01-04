#include <init/stivale2.h>
#include <stddef.h>


void *stivale2_get_tag(struct stivale2_struct *stivale2_struct, uint64_t id){
    /*Get the first tag from the stack*/
    struct stivale2_tag *current_tag = (void*)stivale2_struct->tags;
    /*Loop through all the tags*/
    for(;;){
        /*if the current tag is NULL which means it is the last tag, the return NULL*/
        if(current_tag == NULL){
            return NULL;
        }
        /*if the id matches tag's id then return it*/
        if(current_tag->identifier == id){
            return current_tag;
        }
        /*get next tag in the stack*/
        current_tag = (void*)current_tag->next;
    } 
}
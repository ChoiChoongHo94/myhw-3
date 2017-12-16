#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>

#include "run.h"
#include "util.h"

p_meta base = NULL;
p_meta last = NULL;

p_meta find_meta(p_meta *last, size_t size) {
  p_meta index = base;
  p_meta result = base;

  switch(fit_flag){
    case FIRST_FIT:
    {
      //FIRST FIT CODE
	while(index){
                if(index->free == 1){
                        if(index->size >= size){
                                result = index;
                                break;
                        }
                        else{
                                index = index->next;
                        }
                }
                else// free == 0
                        index = index->next;
        }

        if(!index) // empty or no space
                result = index;


    }
    break;

    case BEST_FIT:
    {
      //BEST_FIT CODE
	int count=0;
        p_meta temp;
        size_t min=9999999999;
        while(index != NULL){
                if(index->free == 1){
                        if(index->size >= size){
                                if(index->size < min){
                                        temp = index;
                                        min = index->size;
                                        count++;
                                }
                                break;
                        }
                        else{
                                index = index->next;
                        }
                }
                else // free == 0
                        index = index->next;
        }

        //not found
        if(count ==0)
                result = NULL;

    }
    break;

    case WORST_FIT:
    {
      //WORST_FIT CODE
	int count=0;
        p_meta temp;
        size_t max=0;
        while(index != NULL){
                if(index->free == 1){
                        if(index->size >= size){
                                if(index->size > max){
                                        temp = index;
                                        max = index->size;
                                        count++;
                                }
                                break;
                        }
                        else{
                                index = index->next;
                        }
                }
                else // free == 0
                        index = index->next;
        }

        //not found
        if(count ==0)
                result = NULL;

    }
    break;

  }
  return result;
}

void *m_malloc(size_t size) {

	//printf("@@@ metasize: %d @@@\n", (int)META_SIZE);
	//printf("@@@ size: %d @@@\n", (int)size);
	if(size%4 >0) size+= 4 - size%4;
	//printf("@@@ size: %d @@@\n", (int)size);

        p_meta temp = find_meta(&last,size);

        if(temp == NULL){//empty or no space
                p_meta create = (p_meta)malloc(META_SIZE+size);

		if(create == NULL){//malloc fail
                        if(brk(size+META_SIZE) == -1)//brk fail
                                 return NULL;
                        create = (p_meta)malloc(META_SIZE+size);
                }

		create->free = 0;
		create->size = size;
	
		if(!base){//empty
			last = create;
			create->prev = NULL;
			create->next = NULL;
			base = create;
		}
		else{//no space
			create->prev = last;
			create->next = NULL;
			last->next = create;
			last = create;
		}

		return (char*) create->data;
        }
	else{//find
		if(temp->size - size > META_SIZE){//split
			int tempsize = temp->size - size - META_SIZE;
			p_meta tempprev= temp->prev;
			p_meta tempnext= temp->next;
			free(temp);
			tempprev->next = (p_meta)malloc(META_SIZE+ size*sizeof(char));
			temp = tempprev->next;
			temp->next = (p_meta)malloc(META_SIZE + tempsize*sizeof(char));
	
			temp->size = size;
			temp->next->size = tempsize;
			temp->free =0;
			temp->next->free=1;

			temp->prev = tempprev;
			temp->next->next = tempnext;
			tempnext->prev = temp->next;
			temp->next->prev = temp;
		}
		//intact
		temp->free=0;
		return (char*) temp->data;
	}
	
}

void m_free(void *ptr) {
	if(!ptr) return;
	
        p_meta temp = ptr- META_SIZE;
	
        while(1){//coalesce
                if(temp->next && temp->next->free ==1){//next
                        int tempsize = temp->size + temp->next->size+ META_SIZE;
			
			p_meta tempprev= temp->prev;
			p_meta tempnext= temp->next->next;
			free(temp->next);
			free(temp);
			
			
			temp = (p_meta)malloc(META_SIZE + tempsize);
			temp->size = tempsize;
			temp->free = 1;
			temp->prev = tempprev;
			temp->next = tempnext;
			
			if(!tempprev)
				base = temp;
			else
				tempprev->next = temp;

			if(!tempnext)
				last = temp;
			else
				tempnext->prev = temp;

                }
                else if(temp->prev && temp->prev->free ==1){//prev
                        int tempsize = temp->size+temp->prev->size+META_SIZE;
			p_meta tempprev = temp->prev->prev;
			p_meta tempnext = temp->next;
			free(temp->prev);
			free(temp);

			temp = (p_meta)malloc(META_SIZE + tempsize);
			temp->size = tempsize;
			temp->free =1;
			temp->prev = tempprev;
			temp->next = tempnext;
	
			if(!tempprev)
				base = temp;
			else
				tempprev->next = temp;

			if(!tempnext)
				last = temp;
			else
				tempnext->prev = temp;
                }
                else if(temp == last){//if last
			last = temp->prev;
			last->next = NULL;
			free(temp);
			break;
		}
		else{
			temp->free =1;
			break;
		}
        }
		
	return;
}

void* m_realloc(void* ptr, size_t size)
{
	p_meta original = ptr-META_SIZE;
	p_meta temp;
	
	if(size%4 >0) size += 4-size%4;

	struct metadata tempmeta;
	memcpy(&tempmeta, original, META_SIZE);

	char tempstring[256];
	memcpy(tempstring, ptr, original->size);
	//printf("@@@ tempsting: %s @@@\n", tempstring);
	//printf("@@@ original->size: %d @@@\n", (int)original->size);
		
	if(size > original->size){//more size than original(new malloc);
		temp = m_malloc(size) - META_SIZE;//new malloc address
		memcpy(temp, &tempmeta, META_SIZE);
		memcpy(temp->data, tempstring, original->size);
		
		m_free(ptr);//free(original)
		//printf("@@@ temp->free: %d, temp->size: %d @@@\n", temp->free, (int)temp->size);
        	temp->free = 0;
		//printf("@@@ temp->size: %d, size: %d @@@\n", (int)temp->size, (int)size);
        	temp->size = size;
	}
	else{//less size than original
		if(original->size - size > META_SIZE){//partitioning
			int tempsize = original->size;
			m_free(original);
			//partition1
			temp = m_malloc(size)-META_SIZE;
			memcpy(temp, &tempmeta, META_SIZE);
			memcpy(temp->data, tempstring, size);
			temp->size = size;
			
			//partition2
			temp = m_malloc(tempsize-size-META_SIZE);
			temp->free =1;
		}
		else{//no partitioning
			memcpy(original->data, tempstring, size);
		}
	}
}



























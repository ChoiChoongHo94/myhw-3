#include <sys/types.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/resource.h>

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
        p_meta temp=NULL;
        size_t min=9999;
        while(index){
                if(index->free == 1 && index->size >= size && index->size < min){
                	temp = index;
                        min = index->size;
                }
                index = index->next;
        }

    
        result = temp; 

    }
    break;

    case WORST_FIT:
    {
      //WORST_FIT CODE
        p_meta temp=NULL;
        size_t max=0;
        while(index){
                if(index->free == 1 && index->size >=size && index->size > max){     
                        temp = index;
                       	max = index->size;
                }    
                index = index->next;
        }

        result = temp;

    }
    break;

  }
  return result;
}

struct rlimit templimit;

void *m_malloc(size_t size) {

	//printf("@@@ metasize: %d @@@\n", (int)META_SIZE);
	//printf("@@@ size: %d @@@\n", (int)size);
	if(size%4 >0) size+= 4 - size%4;
	//printf("@@@ size: %d @@@\n", (int)size);

        p_meta temp = find_meta(&last,size);

        if(temp == NULL){//empty or no space
                p_meta create = (p_meta)malloc(META_SIZE+size);

		if(create == NULL){//malloc fail
			getrlimit(RLIMIT_AS, &templimit);
			if(sbrk(0) + size+META_SIZE > templimit.rlim_max){
				printf("############\n");
				return NULL;
			}
			
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
			tempprev->next = (p_meta)malloc(META_SIZE+ size);
			temp = tempprev->next;
			temp->next = (p_meta)malloc(META_SIZE + tempsize);
	
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
	
	
        //while(1){//coalesce
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

                if(temp->prev && temp->prev->free ==1){//prev
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
		
		if(temp == last){//if last
                        last = temp->prev;
                        last->next = NULL;
                        free(temp);
                        return;
                }

			temp->free =1;
			//break;
		
        //}
		
	return;
}

void* m_realloc(void* ptr, size_t size)
{
	p_meta original = ptr-META_SIZE;
	p_meta temp;
	
	int argsize = size;
	if(size%4 >0) size += 4-size%4;

	struct metadata tempmeta;
	memcpy(&tempmeta, original, META_SIZE);

	char tempstring[256];
	memcpy(tempstring, ptr, original->size);
	//printf("@@@ tempsting: %s @@@\n", tempstring);
	//printf("@@@ original->size: %d @@@\n", (int)original->size);
		
	if(size > original->size){//more size than original(new malloc);	
		if(original->next && size - original->size <= original->next->size){// next is free and sufficient space
			int tempsize = original->size;
			int tempsizenext = original->next->size;
			p_meta tempnext = original->next->next;
			p_meta tempprev = original->prev;

			free(original->next);
			free(original);
			 
			temp = (p_meta)malloc(size+META_SIZE);
			memcpy(temp, &tempmeta, META_SIZE);
			memcpy(temp->data, tempstring, tempsize);
			temp->size = size;
		
			temp->next = (p_meta)malloc(tempsizenext - (size-tempsize) + META_SIZE);
			temp->next->free =1;
			temp->next->next = tempnext;
			temp->next->prev = temp;
			temp->next->size = tempsizenext -(size-tempsize);
		
			if(!tempprev)
				base = temp;
			else
				tempprev->next = temp;

			if(!tempnext)
				last = temp->next;
			else
				tempnext->prev = temp->next;
				
			return (char*)temp->data;
		}
		else{// next is not free or not sufficient space
			temp = m_malloc(size) - META_SIZE;//new malloc address
			memcpy(temp, &tempmeta, META_SIZE);
			memcpy(temp->data, tempstring, original->size);
			
			m_free(ptr);//free(original)
			//printf("@@@ temp->free: %d, temp->size: %d @@@\n", temp->free, (int)temp->size);
        		temp->free = 0;
			//printf("@@@ temp->size: %d, size: %d @@@\n", (int)temp->size, (int)size);
        		temp->size = size;
		
			return (char*) temp->data;
		}
	}
	else{//less size than original
		if(original->size - size > META_SIZE){//partitioning
			int tempsize = original->size;
			p_meta tempprev = original->prev;
			p_meta tempnext = original->next;
			

			//partition1
			temp = (p_meta)malloc(size+META_SIZE);
			memcpy(temp, &tempmeta, META_SIZE);
			memcpy(temp->data, tempstring, argsize);

			temp->size = size;
			temp->free = 0;
			temp->prev = tempprev;
			

			//partition2
			temp->next = (p_meta)malloc(tempsize- size - META_SIZE);
			temp->next->size = tempsize-size-META_SIZE;
			temp->next->free =1;
			temp->next->prev = temp;
			temp->next->next = tempnext;
			
			if(!tempprev)
                                base = temp;
                        else
                                tempprev->next = temp;

                        if(!tempnext)
                                last = temp;
                        else
                                tempnext->prev = temp->next;

			return (char*) temp->data;
		}

		else{//no partitioning
			//memcpy(original->data, tempstring, size);
			return (char*) original->data;
		}
	}
}



























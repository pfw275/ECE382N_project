
// linked list to store candidate sets
typedef struct _CandidateSetItem{
	// address data
	uint64_t *item_addr;

	struct _CandidateSetItem* next;
} CandidateSetItem_t;

typedef struct 
{
	CandidateSetItem_t* head_p;
} Candidate_set_t;

void CandidateSet_construct(Candidate_set_t* this){
	this->head_p = NULL;
}

void CandidateSet_add_to_front(Candidate_set_t* this, uint64_t *item_addr){
	// add new items to the front of the list
	CandidateSetItem_t* new_node_p = malloc(sizeof(CandidateSetItem_t));
	new_node_p->item_addr = item_addr;
	new_node_p->next = this->head_p;
	this->head_p = new_node_p;
}

void CandidateSet_destruct(Candidate_set_t* this){
	while(this->head_p != NULL){
		CandidateSetItem_t* tmp_p = this->head_p->next;
		free(this->head_p);
		this->head_p = tmp_p;
	}
}

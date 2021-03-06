#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <float.h>
#include "structs.h"
#include "alignmentFunctions.h"
#include "commonFunctions.h"
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) <= (y)) ? (x) : (y))



llpos * getNewLocationllpos(Mempool_l * mp, uint64_t * n_pools_used){

    if(mp[*n_pools_used].current == POOL_SIZE){
        *n_pools_used += 1;
        fprintf(stdout, "WARNING: Switched pool\n");
        fflush(stdout);
        if(*n_pools_used == MAX_MEM_POOLS) terror("Reached max pools");
        init_mem_pool_llpos(&mp[*n_pools_used]);
        
    }

    llpos * new_pos = mp[*n_pools_used].base + mp[*n_pools_used].current;
    mp[*n_pools_used].current++;

    
    return new_pos;
}

void init_mem_pool_llpos(Mempool_l * mp){
    mp->base = (llpos *) calloc(POOL_SIZE, sizeof(llpos));
    if(mp->base == NULL) terror("Could not request memory pool");
    mp->current = 0;
}




void * computeAlignmentsByThread(void * a){

/*
typedef struct {
    SeqInfo * database; //Database sequence and lengths
    SeqInfo * query;    //Query sequence and lengths
    uint64_t from;      //Starting READ to compute alignments from
    uint64_t to;        //End READ to compute alignments from
    Container * container; //Container to hold the multidimensional array
    uint64_t accepted_query_reads; //Number of reads that have a fragment with evalue less than specified
    long double min_e_value;    //Minimum evalue to accept read
} HashTableArgs;
*/

    HashTableArgs * hta = (HashTableArgs *) a;
    unsigned char char_converter[91];
    char_converter[(unsigned char)'A'] = 0;
    char_converter[(unsigned char)'C'] = 1;
    char_converter[(unsigned char)'G'] = 2;
    char_converter[(unsigned char)'T'] = 3;
    Quickfrag qf;
    qf.x_start = qf.y_start = qf.t_len = 0;
    qf.e_value = LDBL_MAX;

    //char get_from_db[500], get_from_query[500];
    
    char c;
    uint64_t curr_read = hta->from, curr_db_seq, xlen, ylen;
    uint64_t crrSeqL = 0, pos_of_hit = 0;
    unsigned char curr_kmer[FIXED_K], b_aux[FIXED_K];
    llpos * aux;

    // For NW-alignment
    int NWaligned = 0;
    

    uint64_t n_hits = 0, alignments_tried = 0;

    BasicAlignment ba; //The resulting alignment from the NW
    
    //Set current header position at the position of the read start (the ">")
    uint64_t curr_pos = hta->query->start_pos[curr_read]; //Skip the ">"
    c = (char) hta->query->sequences[curr_pos];

    uint64_t up_to;
    fprintf(stdout, "Going from %"PRIu64" to %"PRIu64"\n", hta->from, hta->to);
    fflush(stdout);

    while(curr_read < hta->to && curr_pos < hta->query->total_len){

        if(curr_read < hta->query->n_seqs - 1) up_to = hta->query->start_pos[curr_read+1]-1; else up_to = hta->query->total_len;
        //printf("Currrpos: %"PRIu64" up to: %"PRIu64" on read: %"PRIu64"\n", curr_pos, up_to, curr_read);

        if (curr_pos == up_to) { // Comment, empty or quality (+) line
            crrSeqL = 0; // Reset buffered sequence length
    	    NWaligned = 0;
    	    //fprintf(stdout, "Seq %"PRIu64" has %"PRIu64" hits and tried to align %"PRIu64" times\n", curr_read, n_hits, alignments_tried);
    	    //fflush(stdout);
    	    n_hits = 0;
    	    alignments_tried = 0;
            curr_read++;
            continue;
        }

        if(c == 'A' || c == 'C' || c == 'T' || c == 'G'){
            curr_kmer[crrSeqL] = (unsigned char) c;
            crrSeqL++;
        }else{
            crrSeqL = 0;
        }

        if (crrSeqL >= FIXED_K) { // Full well formed sequence

            //fprintf(stdout, "%s\n", curr_kmer);
            //fflush(stdout);
            aux = hta->container->table[char_converter[curr_kmer[0]]][char_converter[curr_kmer[1]]][char_converter[curr_kmer[2]]]
                    [char_converter[curr_kmer[3]]][char_converter[curr_kmer[4]]][char_converter[curr_kmer[5]]]
                    [char_converter[curr_kmer[6]]][char_converter[curr_kmer[7]]][char_converter[curr_kmer[8]]]
                    [char_converter[curr_kmer[9]]][char_converter[curr_kmer[10]]][char_converter[curr_kmer[11]]];

            //While there are hits
            //fprintf(stdout, "%p\n", aux);
            //fflush(stdout);
            while(aux != NULL && NWaligned == 0){
		        n_hits++;
                //fprintf(stdout, "%p\n", aux);
                //fflush(stdout);
                curr_db_seq = aux->s_id;
                pos_of_hit = aux->pos;
                alignmentFromQuickHits(hta->database, hta->query, pos_of_hit, curr_pos+1, curr_read, curr_db_seq, &qf);

                //printf("curr evalue: %Le %"PRIu64"\n", qf.e_value, qf.t_len);
                //getchar();
                

                //If e-value of current frag is good, then we compute a good gapped alignment
                if(qf.e_value < hta->min_e_value){
		            alignments_tried++;
                    ba.identities = ba.length = ba.igaps = ba.egaps = 0;
                    //Compute lengths of reads
                    if(curr_db_seq == hta->database->n_seqs-1){
                        xlen = hta->database->total_len - hta->database->start_pos[curr_db_seq];
                    }else{
                        xlen = hta->database->start_pos[curr_db_seq+1] - hta->database->start_pos[curr_db_seq];
                    }
                    if(curr_read == hta->query->n_seqs-1){
                        ylen = hta->query->total_len - hta->query->start_pos[curr_read];
                    }else{
                        ylen = hta->query->start_pos[curr_read+1] - hta->query->start_pos[curr_read];
                    }
                    //Perform alignment plus backtracking
                    //void build_alignment(char * reconstruct_X, char * reconstruct_Y, uint64_t curr_db_seq, uint64_t curr_read, HashTableArgs * hta, char * my_x, char * my_y, struct cell ** table, struct cell * mc, char * writing_buffer_alignment, BasicAlignment * ba, uint64_t xlen, uint64_t ylen)
                    if(xlen > MAX_READ_SIZE || ylen > MAX_READ_SIZE) terror("Read size reached for gapped alignment.");
                    //fprintf(stdout, "R0 %"PRIu64", %"PRIu64"\n", curr_db_seq, curr_read);
                    

                    build_alignment(hta->reconstruct_X, hta->reconstruct_Y, curr_db_seq, curr_read, hta, hta->my_x, hta->my_y, hta->table, hta->mc, hta->writing_buffer_alignment, &ba, xlen, ylen);
                    

                    //If is good
                    if(((long double)ba.length/ylen) >= hta->min_coverage && ((long double)ba.identities/ba.length) >=  hta->min_identity){
                        hta->accepted_query_reads++;   
                        if(hta->out != NULL){
                            //printf("Last was: (%"PRIu64", %"PRIu64")\n", curr_read, curr_db_seq);
                            fprintf(hta->out, "(%"PRIu64", %"PRIu64") : %d%% %d%% %"PRIu64"\n $$$$$$$ \n", curr_read, curr_db_seq, MIN(100,(int)(100*ba.identities/ba.length)), MIN(100,(int)(100*ba.length/ylen)), ylen);
                            fprintf(hta->out, "%s", hta->writing_buffer_alignment);
                            //fprintf(stdout, "(%"PRIu64", %"PRIu64") : %d%% %d%% %"PRIu64"\n $$$$$$$ \n", curr_read, curr_db_seq, MIN(100,(int)(100*ba.identities/ba.length)), MIN(100,(int)(100*ba.length/ylen)), ylen);
                            //fprintf(stdout, "%s", hta->writing_buffer_alignment);
                        }
                        NWaligned = 1;
                    }
                
                }

                //strncpy(get_from_db, &hta->database->sequences[qf.x_start], qf.t_len);
                //strncpy(get_from_query, &hta->query->sequences[qf.y_start], qf.t_len);
                //fprintf(hta->out, "%s\n%s\n%Le\t%d\n-------------------\n", get_from_db, get_from_query, qf.e_value, (int)(100*qf.coverage));
                //fprintf(hta->out, "%"PRIu64", %"PRIu64", %"PRIu64"\n", qf.x_start, qf.y_start, qf.t_len);

                //printf("Hit comes from %"PRIu64", %"PRIu64"\n", pos_of_hit, curr_pos);
                aux = aux->next;
                //fprintf(stdout, "%p\n", aux);
                //fflush(stdout);
            }
            //printf("SWITCHED\n");

            if(NWaligned == 1){
                if(curr_read < hta->query->n_seqs) curr_pos = hta->query->start_pos[curr_read+1]-2;
            }else{
                memcpy(b_aux, curr_kmer, FIXED_K);
                memcpy(curr_kmer, &b_aux[1], FIXED_K-1);
                crrSeqL -= 1;
            }
        }
	
        curr_pos++;
        if(curr_pos < hta->query->total_len) c = (char) hta->query->sequences[curr_pos];
        


    }
    

    return NULL;

}

void build_alignment(char * reconstruct_X, char * reconstruct_Y, uint64_t curr_db_seq, uint64_t curr_read, HashTableArgs * hta, unsigned char * my_x, unsigned char * my_y, struct cell ** table, struct positioned_cell * mc, char * writing_buffer_alignment, BasicAlignment * ba, uint64_t xlen, uint64_t ylen){

    //strncpy(get_from_db, &hta->database->sequences[qf.x_start], qf.t_len);
    //strncpy(get_from_query, &hta->query->sequences[qf.y_start], qf.t_len);
    //printf("%s\n%s\n%Le\t%d\n-------------------\n", get_from_db, get_from_query, qf.e_value, (int)(100*qf.coverage));
    //printf("%"PRIu64", %"PRIu64", %"PRIu64"\n", qf.x_start, qf.y_start, qf.t_len);

    //printf("Hit comes from %"PRIu64", %"PRIu64"\n", pos_of_hit, curr_pos);
 
    //Do some printing of alignments here
    uint64_t maximum_len, i, j, curr_pos_buffer;

    maximum_len = 2*MAX(xlen,ylen);
    memcpy(my_x, &hta->database->sequences[hta->database->start_pos[curr_db_seq]], xlen);
    memcpy(my_y, &hta->query->sequences[hta->query->start_pos[curr_read]], ylen);

    //getchar();

    struct positioned_cell best_cell = NW(my_x, 0, xlen, my_y, 0, ylen, (int64_t) hta->igap, (int64_t) hta->egap, table, mc, 0);
    backtrackingNW(my_x, 0, xlen, my_y, 0, ylen, table, reconstruct_X, reconstruct_Y, &best_cell, &i, &j, ba);
    uint64_t offset = 0, before_i = 0, before_j = 0;
    i++; j++;
    curr_pos_buffer = 0;
    while(i <= maximum_len && j <= maximum_len){
        offset = 0;
        before_i = i;
        while(offset < ALIGN_LEN && i <= maximum_len){
            //fprintf(out, "%c", reconstruct_X[i]);
            writing_buffer_alignment[curr_pos_buffer++] = (char) reconstruct_X[i];
            i++;
            offset++;
        }
        //fprintf(out, "\n");
        writing_buffer_alignment[curr_pos_buffer++] = '\n';
        offset = 0;
        before_j = j;
        while(offset < ALIGN_LEN && j <= maximum_len){
            //fprintf(out, "%c", reconstruct_Y[j]);
            writing_buffer_alignment[curr_pos_buffer++] = (char) reconstruct_Y[j];
            j++;
            offset++;
        }
        //fprintf(out, "\n");
        writing_buffer_alignment[curr_pos_buffer++] = '\n';
        while(before_i < i){
            if(reconstruct_X[before_i] != '-' && reconstruct_Y[before_j] != '-' && reconstruct_X[before_i] == reconstruct_Y[before_j]){
                //fprintf(out, "*");
                writing_buffer_alignment[curr_pos_buffer++] = '*';
                ba->identities++;
            }else{
                //fprintf(out, " ");
                writing_buffer_alignment[curr_pos_buffer++] = ' ';
            }
            before_j++;
            before_i++;
        }
        writing_buffer_alignment[curr_pos_buffer++] = '\n';

    }
    //fprintf(out, "\n$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    writing_buffer_alignment[curr_pos_buffer++] = '\n';
    writing_buffer_alignment[curr_pos_buffer++] = '\0';


}

void alignmentFromQuickHits(SeqInfo * database, SeqInfo * query, uint64_t pos_database, uint64_t pos_query, uint64_t curr_read, uint64_t curr_db_seq, Quickfrag * qf){

    int64_t read_x_start, read_x_end, read_y_start, read_y_end;

    if(curr_db_seq == database->n_seqs-1){
        read_x_start = database->start_pos[curr_db_seq];
	read_x_end = database->total_len;
    }else{     
        read_x_start = database->start_pos[curr_db_seq];
	read_x_end = database->start_pos[curr_db_seq+1] - 1;
    }

    if(curr_read == query->n_seqs-1){
        read_y_start = query->start_pos[curr_read];
        read_y_end = query->total_len;
    }else{
        read_y_start = query->start_pos[curr_read];
        read_y_end = query->start_pos[curr_read+1] - 1;
    }



    int64_t curr_pos_db = (int64_t) pos_database;
    int64_t curr_pos_qy = (int64_t) pos_query;
    int64_t final_end_x = pos_database - 1, final_start_x = final_end_x - FIXED_K + 1, final_start_y = pos_query - FIXED_K;
    int64_t score_right = FIXED_K * POINT;
    int64_t score_left = score_right;
    int64_t high_left = score_left, high_right = score_right;
    qf->t_len = FIXED_K;
    uint64_t idents = FIXED_K;

    /*
    char le_hit[1000];
    memcpy(le_hit, &database->sequences[final_start_x], FIXED_K);
    
	fprintf(stdout, "HIT: %s\n", le_hit);
	fflush(stdout);
    */

    int keep_going = 1;

    //Forward search
    while(keep_going == 1){
        
        
        if(score_right > 0 && curr_pos_db < database->total_len && curr_pos_qy < query->total_len){
            if(curr_pos_db  > read_x_end ||  curr_pos_qy > read_y_end) break;
            if(database->sequences[curr_pos_db] == query->sequences[curr_pos_qy]){ score_right+=POINT; idents++; }else{ score_right-=POINT;}
            if(high_right <= score_right){
                final_end_x = curr_pos_db;
                high_right = score_right;
            }
            curr_pos_db++;
            curr_pos_qy++;
        }else{
            keep_going = 0;
        }
    }

    keep_going = 1;
    curr_pos_db = pos_database - FIXED_K - 1;
    curr_pos_qy = pos_query - FIXED_K - 1;

    score_left = high_right;

    //Backward search
    while(keep_going == 1){
        
        if(score_left > 0 && curr_pos_db >= 0 && curr_pos_qy >= 0){
            if(curr_pos_db < read_x_start || curr_pos_qy < read_y_start ) break;
            if(database->sequences[curr_pos_db] == query->sequences[curr_pos_qy]){ score_left+=POINT; idents++; }else{ score_left-=POINT;}
            if(high_left <= score_left){
                final_start_x = curr_pos_db;
                final_start_y = curr_pos_qy;
                high_left = score_left;
            }
            curr_pos_db--;
            curr_pos_qy--;
        }else{
            keep_going = 0;
        }
    }

    qf->t_len = final_end_x - final_start_x;
    /*
    char s1[1000];
    char s2[1000];

    memcpy(s1, &database->sequences[final_start_x], qf->t_len);
    memcpy(s2, &query->sequences[final_start_y], qf->t_len);

    
	fprintf(stdout, "%s\n%s\n------\n", s1, s2);
	fflush(stdout);
    getchar();
    */

    long double rawscore = (idents*POINT) - (qf->t_len - idents)*(POINT);

    long double t_len;
    if(curr_read == query->n_seqs-1){
        t_len = (long double) query->total_len - query->start_pos[curr_read];
    }else{
        t_len = (long double) query->start_pos[curr_read+1] - query->start_pos[curr_read];
    }

    qf->x_start = final_start_x;
    qf->y_start = final_start_y;
    qf->e_value = (long double) QF_KARLIN*t_len*database->total_len*expl(-QF_LAMBDA * rawscore);
    qf->coverage = qf->t_len / t_len;

}

struct positioned_cell NW(unsigned char * X, uint64_t Xstart, uint64_t Xend, unsigned char * Y, uint64_t Ystart, uint64_t Yend, int64_t iGap, int64_t eGap, struct cell ** table, struct positioned_cell * mc, int show){
    
    uint64_t i,j;
    int64_t scoreDiagonal,scoreLeft,scoreRight,score;

    struct positioned_cell bc;
    bc.score = INT64_MIN;
    
    
    struct positioned_cell mf;
    mf.score = INT64_MIN;
    

    //First row. iCounter serves as counter from zero
    //printf("..0%%");
    for(i=0;i<Yend;i++){
        table[0][i].score = (X[0] == Y[i]) ? POINT : -POINT;
        //table[Xstart][i].xfrom = Xstart;
        //table[Xstart][i].yfrom = i;
        //Set every column max
        mc[i].score = table[0][i].score;
        mc[i].xpos = 0;
        mc[i].ypos = i;

    }
    
    //Set row max
    mf.score = table[0][0].score;
    mf.xpos = 0;
    mf.ypos = 0;

    //Go through full matrix
    for(i=1;i<Xend;i++){
        //Fill first rowcell
        


        table[i][0].score = (X[i] == Y[0]) ? POINT : -POINT;
        mf.score = table[i][0].score;
        mf.xpos = i;
        mf.ypos = 0;

        for(j=1;j<Yend;j++){

            //Check if max in row has changed
            if(j > 1 && mf.score <= table[i][j-2].score){
                mf.score = table[i-1][j-2].score;
                mf.xpos = i-1;
                mf.ypos = j-2;
            }
            
            score = (X[i] == Y[j]) ? POINT : -POINT;
            scoreDiagonal = table[i-1][j-1].score + score;

            if(j>1){
                scoreLeft = mf.score + iGap + (j - (mf.ypos+1))*eGap + score;
                }else{
                    scoreLeft = INT64_MIN;
                }
                
            if(i>1){
                scoreRight = mc[j-1].score + iGap + (i - (mc[j-1].xpos+1))*eGap + score;
                }else{
                    scoreRight = INT64_MIN;
                }
            
            //Choose maximum
            //printf("Score DIAG: %"PRId64"; LEFT: %"PRId64"; RIGHT: %"PRId64"\n", scoreDiagonal, scoreLeft, scoreRight);
            if(scoreDiagonal >= scoreLeft && scoreDiagonal >= scoreRight){
                //Diagonal
                table[i][j].score = scoreDiagonal;
                table[i][j].xfrom = i-1;
                table[i][j].yfrom = j-1;
                                
            }else if(scoreRight > scoreLeft){
                table[i][j].score = scoreRight;
                table[i][j].xfrom = mc[j-1].xpos;
                table[i][j].yfrom = mc[j-1].ypos;
                
            }else{
                table[i][j].score = scoreLeft;
                table[i][j].xfrom = mf.xpos;
                table[i][j].yfrom = mf.ypos;
            }
        
        
            //check if column max has changed
            if(i > 1 && j > 1 && table[i-2][j-1].score > mc[j-1].score){
                mc[j-1].score = table[i-2][j-1].score;
                mc[j-1].xpos = i-2;
                mc[j-1].ypos = j-1;
            }
            if(i == Xend-1 || j == Yend-1){
            //Check for best cell
                if(table[i][j].score >= bc.score){ bc.score = table[i][j].score; bc.xpos = i; bc.ypos = j; }
            }
        }
    }
        
    return bc;
}



void backtrackingNW(unsigned char * X, uint64_t Xstart, uint64_t Xend, unsigned char * Y, uint64_t Ystart, uint64_t Yend, struct cell ** table, char * rec_X, char * rec_Y, struct positioned_cell * bc, uint64_t * ret_head_x, uint64_t * ret_head_y, BasicAlignment * ba){
    uint64_t curr_x, curr_y, prev_x, prev_y, head_x, head_y;
    int64_t k;
    head_x = 2*MAX(Xend, Yend);
    head_y = 2*MAX(Xend, Yend);
    curr_x = bc->xpos;
    curr_y = bc->ypos;
    prev_x = curr_x;
    prev_y = curr_y;
   
    for(k=Xend-1; k>curr_x; k--) rec_X[head_x--] = '-';
    for(k=Yend-1; k>curr_y; k--) rec_Y[head_y--] = '-';

    
    while(curr_x > 0 && curr_y > 0){
        curr_x = table[prev_x][prev_y].xfrom;
        curr_y = table[prev_x][prev_y].yfrom;
        //printf("(%"PRIu64", %"PRIu64") ::: ", curr_x, curr_y);
        //printf("(%"PRIu64", %"PRIu64") ::: ", prev_x, prev_y);
        //getchar();

        if((curr_x == (prev_x - 1)) && (curr_y == (prev_y -1))){
            //Diagonal case
            //printf("DIAG\n");
            rec_X[head_x--] = (char) X[prev_x];
            rec_Y[head_y--] = (char) Y[prev_y];
            ba->length++;
        }else if((prev_x - curr_x) > (prev_y - curr_y)){
            //Gap in X
            //printf("Gap X\n");
            for(k=prev_x;k>curr_x;k--){
                rec_Y[head_y--] = '-';
                rec_X[head_x--] = (char) X[k];
                ba->length++;
                ba->egaps++;
            }
            ba->igaps += 1;
            ba->egaps--;
        }else{
            //Gap in Y
            //printf("GAP Y\n");
            //10, 0, 401, 281
            for(k=prev_y;k>curr_y;k--){
                rec_X[head_x--] = '-';
                rec_Y[head_y--] = (char) Y[k];
                ba->length++;
                ba->egaps++;
            }
            ba->igaps += 1;
            ba->egaps--;
        }
        prev_x = curr_x;
        prev_y = curr_y;
    }
    //printf("curr: %"PRIu64", %"PRIu64"\n", curr_x, curr_y);
    uint64_t huecos_x = 0, huecos_y = 0;
    for(k=(int64_t)curr_x-1; k>=0; k--){ rec_X[head_x--] = '-'; huecos_x++;}
    for(k=(int64_t)curr_y-1; k>=0; k--){ rec_Y[head_y--] = '-'; huecos_y++;}
    
    if(huecos_x >= huecos_y){
    while(huecos_x > 0) {rec_Y[head_y--] = ' '; huecos_x--;}
    }else{
    while(huecos_y > 0) {rec_X[head_x--] = ' '; huecos_y--;}
    }

    *ret_head_x = head_x;
    *ret_head_y = head_y;
}

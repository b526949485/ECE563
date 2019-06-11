#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int S;
int N;
int dispatch_queue_size;
int schedule_queue_size;
int execute_list_size;
char* trace_file;
int register_ready_flag[128];
int NUMBER_cycle;
int NUMBER_instr;
int instr_end_flag;
int tag_index;
double IPC;
FILE *ifp;
int dispatch_list[100];
int issue_list[100];
int execute_list[1024];
int NUMBER_available_fetch;
int NUMBER_available_issue;

int advance_cycle();
void FakeRetire();
void Execute();
void Issue();
void Dispatch();
void Fetch();


char pc[256];
char mem[256];
//char dest[8],src1[8],src2[8];
int type, dest,src1,src2;
char prt;

typedef struct{
    int state_change_flag;
    int state; //0 not start, 1:IF, 2:ID, 3:IS, 4:EX, 5:WB, 6:Finished/Retire
    char PC[256];
    int operation_type;
    int dest_reg;
    int src1_reg;
    int src2_reg;
    int src1_reg_copy_flag;
    int src2_reg_copy_flag;
    char mem_address[256];
    int IF_cycle, IF_times;
    int ID_cycle, ID_times;
    int IS_cycle, IS_times;
    int EX_cycle, EX_times;
    int WB_cycle, WB_times;
} TAG;

TAG *tag[200000];

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("You did not feed me arguments, I will die now :( ...");
        return 0;
    }
    /*Get the argument from user*/
    S = atoi(argv[1]);  //Scheduling Queue size
    N = atoi(argv[2]);  //Peak fetch, dispatch, and issue rate.
    trace_file = argv[8];

    //initial value
    dispatch_queue_size = 2*N;
    schedule_queue_size = S;
    execute_list_size = 1024;
    NUMBER_cycle = 0;
    NUMBER_instr = 0;
    instr_end_flag = 0;
    tag_index = 0;
    NUMBER_available_fetch = N;
    //clear the dispatch list
    for(int i =0; i<dispatch_queue_size;i++){
        dispatch_list[i] = -1;
    }
    for(int i =0; i<schedule_queue_size;i++){
        issue_list[i] = -1;
    }
    for(int i =0; i<execute_list_size;i++){
        execute_list[i] = -1;
    }

    for(int i=0; i<128;i++){
        register_ready_flag[i] = 1; //all register is ready at the beginning
    }
    for (int i=0; i < 20000; i++){
        tag[i] = (TAG*)malloc(sizeof(TAG));
    }
    for (int i=0; i < 20000; i++){
        tag[i]->state_change_flag = 0;
        tag[i]->state = 0;
        tag[i]->operation_type = 0;
        tag[i]->src1_reg_copy_flag= 0;tag[i]->src2_reg_copy_flag= 0;
        tag[i]->src1_reg= 0;tag[i]->src2_reg= 0;tag[i]->dest_reg= 0;
        tag[i]->IF_cycle= 0;tag[i]->IF_times= 0;
        tag[i]->ID_cycle= 0;tag[i]->ID_times= 0;
        tag[i]->IS_cycle= 0;tag[i]->IS_times= 0;
        tag[i]->EX_cycle= 0;tag[i]->EX_times= 0;
        tag[i]->WB_cycle= 0;tag[i]->WB_times= 0;
    }

    /*open the trace file*/
    //read address from trace file
    ifp = fopen(trace_file, "r");
    if (ifp == NULL ) {
        printf("Error! Could not open file\n");
        return 0;
    }
    while(!feof(ifp)) {
        fscanf(ifp, "%s %d %d %d %d %s",
               tag[NUMBER_instr]->PC, &type, &dest, &src1, &src2,tag[NUMBER_instr]->mem_address);
        tag[NUMBER_instr]->operation_type = type;
        tag[NUMBER_instr]->dest_reg = dest;
        tag[NUMBER_instr]->src1_reg = src1;
        tag[NUMBER_instr]->src2_reg = src2;
        NUMBER_instr++;
    }
    //close file
    fclose(ifp);

    //simulation process
    while(advance_cycle()){
        NUMBER_cycle++;
        FakeRetire();
        Execute();
        Issue();
        Dispatch();
        Fetch();
    }

    //print the output
    for(int i=0; i<NUMBER_instr;i++){
        printf("%d fu{%d} src{%d,%d} dst{%d} IF{%d,%d} ID{%d,%d} IS{%d,%d} EX{%d,%d} WB{%d,%d}\n",
               i, tag[i]->operation_type,
               tag[i]->src1_reg,tag[i]->src2_reg,tag[i]->dest_reg,
               tag[i]->IF_cycle,tag[i]->IF_times,
               tag[i]->ID_cycle,tag[i]->ID_times,
               tag[i]->IS_cycle,tag[i]->IS_times,
               tag[i]->EX_cycle,tag[i]->EX_times,
               tag[i]->WB_cycle,tag[i]->WB_times );
    }
    printf("CONFIGURATION\n");
    printf(" superscalar bandwidth (N) = %d\n", N);
    printf(" dispatch queue size (2*N) = %d\n", dispatch_queue_size);
    printf(" schedule queue size (S)   = %d\n", schedule_queue_size);
    printf("RESULTS\n");
    printf(" number of instructions = %d\n", NUMBER_instr);
    printf(" number of cycles       = %d\n", NUMBER_cycle);
    double a,b;
    a = NUMBER_instr;
    b = NUMBER_cycle;
    IPC = a/b;
    printf(" IPC                    = %f\n", IPC);
}

int advance_cycle(){


    //check all the instr are finished, tag->state = 6
    instr_end_flag = 1;
    for(int i=0;i<NUMBER_instr;i++){
        if(tag[i]->state!=6)
            instr_end_flag = 0;
    }
    if(instr_end_flag)
        return 0;

//    if(NUMBER_cycle==20000)
//        return 0;

    //update the times
    for(int i=0; i<NUMBER_instr;i++){
        if(tag[i]->state==1) {  //IF
            tag[i]->IF_cycle = NUMBER_cycle -1;  //IF cycle
            tag[i]->IF_times++;          //IF times
        }
        if(tag[i]->state==2) {  //ID
            if(tag[i]->ID_cycle==0)
                tag[i]->ID_cycle = NUMBER_cycle-1;   //ID cycle
            tag[i]->ID_times++;              //ID times
        }
        if(tag[i]->state==3) {  //IS
            if(tag[i]->IS_cycle==0) {
                tag[i]->IS_cycle = NUMBER_cycle - 1;   //IS cycle
//                if(tag[i]->dest_reg!=tag[i]->src1_reg&&tag[i]->dest_reg!=tag[i]->src2_reg)
//                    register_ready_flag[tag[i]->dest_reg] = 0; //update the register file state to ready
            }
            tag[i]->IS_times++;              //IS times
        }
        if(tag[i]->state==4) {  //EX
            if(tag[i]->EX_cycle==0) {
                tag[i]->EX_cycle = NUMBER_cycle - 1;   //EX cycle
               // if(tag[i]->dest_reg==tag[i]->src1_reg||tag[i]->dest_reg==tag[i]->src2_reg)
                    register_ready_flag[tag[i]->dest_reg] = 0; //update the register file state to ready
            }
            tag[i]->EX_times++;              //EX times
//            if(tag[i]->operation_type==0 && tag[i]->EX_times==1){
//                register_ready_flag[tag[i]->dest_reg] = 1; //update the register file state to ready
//            }else if(tag[i]->operation_type==1 && tag[i]->EX_times==2){
//                register_ready_flag[tag[i]->dest_reg] = 1; //update the register file state to ready
//            }else if(tag[i]->operation_type==2 && tag[i]->EX_times==5){
//                register_ready_flag[tag[i]->dest_reg] = 1; //update the register file state to ready
//            }

        }
        if(tag[i]->state==5) {  //WB
            if(tag[i]->WB_cycle==0){
                //register_ready_flag[tag[i]->dest_reg] = 1; //update the register file state to ready
                tag[i]->WB_cycle = NUMBER_cycle-1;   //WB cycle
//                if(tag[i]->dest_reg>=0)
//                    register_ready_flag[tag[i]->dest_reg] = 1; //update the register file state to ready
                 }
            tag[i]->WB_times++;              //WB times
        }
    }



    return 1;
}

void FakeRetire(){
    for(int i=0;i<NUMBER_instr;i++){
        if(tag[i]->state==5)
            // tag[i]->state_change_flag = 1; //ready to change state
            tag[i]->state = 6; //chang state to Retire

    }
}

void Execute(){
    int execute_flag;

    for(int i=0;i<execute_list_size;i++){
        execute_flag = 0;
        if(execute_list[i]>=0){
            register_ready_flag[tag[execute_list[i]]->dest_reg] = 0; //update the register file state to ready
//            tag[execute_list[i]]->EX_times++; //working on execute
            if(tag[execute_list[i]]->operation_type==0){ //type 0, 1 cycle
                if (tag[execute_list[i]]->EX_times==1)
                    execute_flag = 1;
            }
            else if(tag[execute_list[i]]->operation_type==1){  // type 1, 2 cycle
                if (tag[execute_list[i]]->EX_times==2)
                    execute_flag = 1;
            }
            else if(tag[execute_list[i]]->operation_type==2){ // type 2, 5 cycle
                if (tag[execute_list[i]]->EX_times==5)
                    execute_flag = 1;
            }
            if(execute_flag){
                // tag[i]->state_change_flag = 1; //ready to change state
                tag[execute_list[i]]->state = 5; //chang state to WB
                if(tag[execute_list[i]]->dest_reg>=0)
                    register_ready_flag[tag[execute_list[i]]->dest_reg] = 1; //update the register file state to ready
                execute_list[i] = -1; // remove instr from execute list
            }

        }
    }
}

void Issue(){

    int is_to_ex_count = 0;
    int lowest_is;
    int lowest_index_in_issue = 0;
    int is_to_ex_flag = 0;
    int temp_a,temp_b;
    int find_smallest_flag;
    while(is_to_ex_count<N){ //check bandwidth
        is_to_ex_count++;
        lowest_is = NUMBER_instr;
        is_to_ex_flag = 0;
        find_smallest_flag = 0;
        for(int i=0; i<schedule_queue_size;i++){
            if(issue_list[i]>=0) {
               // upadate register copy_flag
//                temp_a = tag[issue_list[i]]->src1_reg_copy_flag;
//                temp_b = tag[issue_list[i]]->src2_reg_copy_flag;
//                if(tag[issue_list[i]]->src1_reg_copy_flag==0&&register_ready_flag[tag[issue_list[i]]->src1_reg] == 1)
//                    temp_a = 1;
//                    //tag[issue_list[i]]->src1_reg_copy_flag = 1;
//                if(tag[issue_list[i]]->src2_reg_copy_flag==0&&register_ready_flag[tag[issue_list[i]]->src2_reg] == 1)
//                    temp_b = 1;
//                    //tag[issue_list[i]]->src2_reg_copy_flag = 1;
//
//                if ((temp_a > 0) && (temp_b >0)){
//                    is_to_ex_flag = 1;
//                }

                if(tag[issue_list[i]]->src1_reg<0 && tag[issue_list[i]]->src2_reg<0){
                    is_to_ex_flag = 1;
                    find_smallest_flag = 1;
                }else
                if(tag[issue_list[i]]->src1_reg<0 && tag[issue_list[i]]->src2_reg>=0){
                    if((register_ready_flag[tag[issue_list[i]]->src2_reg] == 1)) {
                        is_to_ex_flag = 1;
                        find_smallest_flag = 1;
                    }
                }else
                if(tag[issue_list[i]]->src1_reg>=0 && tag[issue_list[i]]->src2_reg<0){
                    if((register_ready_flag[tag[issue_list[i]]->src1_reg] == 1)){
                        is_to_ex_flag = 1;
                        find_smallest_flag = 1;
                    }
                }else
                if(tag[issue_list[i]]->src1_reg>=0 && tag[issue_list[i]]->src2_reg>=0){
                    if ((register_ready_flag[tag[issue_list[i]]->src1_reg] == 1) &&
                        (register_ready_flag[tag[issue_list[i]]->src2_reg] == 1)){
                        is_to_ex_flag = 1;
                        find_smallest_flag = 1;
                    }
                }

                if(find_smallest_flag){
                    if (lowest_is > issue_list[i]) {
                        lowest_is = issue_list[i];
                        lowest_index_in_issue = i;
                    }
                    find_smallest_flag = 0;
                }
            }
        }


        if(is_to_ex_flag){

            //tag[lowest_is]->state_change_flag = 1; //ready to change state
            tag[lowest_is]->state = 4; //change state to EX
            tag[lowest_is]->EX_times = 0; // start to count ex time
          //  if(tag[lowest_is]->dest_reg!=tag[lowest_is]->src1_reg&&tag[lowest_is]->dest_reg!=tag[lowest_is]->src2_reg)
                register_ready_flag[tag[lowest_is]->dest_reg] = 0; //update the register file state to not ready
            issue_list[lowest_index_in_issue] = -1; //remove IS tag from issue list

            //give IS to EX
            for(int j=0; j<execute_list_size;j++){
                if(execute_list[j]<0){
                    execute_list[j] = lowest_is;
                    j = execute_list_size;
                }
            }
        }
    }
}

void Dispatch(){
    //ID to IS
    //check number of available issue
    NUMBER_available_issue=0;
    for(int i=0;i<schedule_queue_size;i++){
        if(issue_list[i]<0){
            NUMBER_available_issue++;
        }
    }
    if(NUMBER_available_issue>N)
        NUMBER_available_issue = N;

    int id_to_is_flag;
    int lowest_id;
    int lowest_index_in_dispatch = -1;
    for(int i =0;i<NUMBER_available_issue;i++) { //check bandwidth
                    lowest_id = NUMBER_instr;
                    id_to_is_flag = 0;
                    //find lowest ID
                    for(int n=0;n<dispatch_queue_size;n++) {
                        if(dispatch_list[n]>=0) {

                            if (tag[dispatch_list[n]]->state == 2) {
                                id_to_is_flag =1;
                    if (lowest_id > dispatch_list[n]) {
                        lowest_id = dispatch_list[n];
                        lowest_index_in_dispatch = n;
                    }
                }
            }
        }

        //give ID to IS
        if(id_to_is_flag) {
            // tag[lowest_id]->state_change_flag = 1; //ready to change state
            tag[lowest_id]->state = 3; //change state to IS
//            if(tag[lowest_id]->src1_reg<0){
//                tag[lowest_id]->src1_reg_copy_flag = 2;
//            }
//            else{tag[lowest_id]->src1_reg_copy_flag = register_ready_flag[tag[lowest_id]->src1_reg];}
//            if(tag[lowest_id]->src2_reg<0){
//                tag[lowest_id]->src2_reg_copy_flag = 2;
//            }
//            else{tag[lowest_id]->src2_reg_copy_flag = register_ready_flag[tag[lowest_id]->src2_reg];}

//           if(tag[lowest_id]->dest_reg!=tag[lowest_id]->src1_reg&&tag[lowest_id]->dest_reg!=tag[lowest_id]->src2_reg)
        //         register_ready_flag[tag[lowest_id]->dest_reg] = 0; //update the register file state to ready
            dispatch_list[lowest_index_in_dispatch] = -1; //remove ID tag from dispatch list
            for (int j = 0; j < schedule_queue_size; j++) {
                if (issue_list[j] < 0) {
                    issue_list[j] = lowest_id;
                    j = schedule_queue_size;
                }
            }
        }

    }

    //IF to ID
    for(int i =0; i<dispatch_queue_size;i++){
        if(dispatch_list[i]>=0) {
            if (tag[dispatch_list[i]]->state == 1) {
                // tag[dispatch_list[i]]->state_change_flag = 1; //ready to change state
                tag[dispatch_list[i]]->state = 2; //change state to ID
            }
        }
    }
}


void Fetch(){
    //check number of available fetch
    NUMBER_available_fetch=0;
    for(int i=0;i<dispatch_queue_size;i++){
        if(dispatch_list[i]<0){
            NUMBER_available_fetch++;
        }
    }
    if(NUMBER_available_fetch>N)
        NUMBER_available_fetch = N;


    if(tag_index>=NUMBER_instr)
        return; //reached the end-of-file
    for(int i=0;i<NUMBER_available_fetch;i++){    //check bandwidth
        for(int j=0; j<dispatch_queue_size;j++){   //check the list is not full
            if(dispatch_list[j]<0){
                dispatch_list[j] = tag_index;
                j=dispatch_queue_size;
            }
        }
        //tag[tag_index]->state_change_flag = 1; //ready to change state
        tag[tag_index]->state = 1;
        tag_index++;
        if(tag_index>=NUMBER_instr) {
            return;
        }
    }

}
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include "ezipc.h"

/*
    From the Diagram given, we know that we have two shared memory.
    Reuse from the idea from before.
*/
//struct for allocate in shared memory
struct CShare{
    //since this time what is stored in the share memory is input one character
    //at a time, so the data type in the shared memory is characters.
    char Share1;
    char Share2;
};
struct CShare * volatile sh;

//global field: for me it seems that we need four semaphores,which will check if the resources will be available in the
//shared memory,lock and unlock it for other process at the appropriate time
int Mutex_Share1_Read;
int Mutex_Share1_Write;
int Mutex_Share2_Read;
int Mutex_Share2_Write;

//laying out the main first, and lay out the foundation later

void producer();
void squash();
void printer();

int main(){

    //set up our semaphore
    SETUP();
    //SIGNAL "OK TO WRITE, BUT WAIT A SEC TO READ"
    Mutex_Share1_Read = SEMAPHORE(SEM_BIN,0);
    Mutex_Share1_Write = SEMAPHORE(SEM_BIN,1);
    Mutex_Share2_Write = SEMAPHORE(SEM_BIN,1);
    Mutex_Share2_Read = SEMAPHORE(SEM_BIN,0);
    sh=(struct CShare *) SHARED_MEMORY(sizeof(struct CShare));
    sh->Share1=0;
    sh->Share2=0;
    /*
     * big picture: create three process
     * and use the work function
     */
   //Squash process read from the SharedMemory1 and see if there such scenario exists, which old = new = "*", if it exists convert to "#
    if (fork()==0) squash();
    
     //Printer print out as format of having 25-characters per line
    if (fork()==0) printer();
        
 //producer process open the file and read in the content, convert "\n" to <EOL>, 5 Characters, write to the SharedMemory1
producer();

//waiting for end processes
//You can see the order of the end of the processes.
for(int i=0;i<2;++i){
    int wpid=wait(NULL);
    //printf("Process %d finished\n",wpid);
} 

 //printf("End producer process\n");
}

void producerWriteChar(char c)
{
    P(Mutex_Share1_Write); // waiting it to be empty
    sh->Share1 = c; // write to it
    V(Mutex_Share1_Read); // increment it
}
void producerWriteString(char* str)
{
    while(*str)
        producerWriteChar(*str++);
}
void producer() {
    //create a buffer to store the character
    char Buffer1;
    //open up the file
    FILE *fp;
    fp = fopen("Conway.txt", "r");
    if (fp == NULL) {
        char Z[100] = "The File you wish to open does not exist";
        printf("%s", Z);
    }

    do {
        Buffer1 = fgetc(fp); // get exactly one character at a time from the file
        if(feof(fp)){producerWriteChar(EOF);break;}

            if (Buffer1 == '\n') {
                //since we want to store 5 characters
                producerWriteString("<EOL>");
            } else {
               producerWriteChar(Buffer1);
            }

        } while (!feof(fp));
        sh->Share1=EOF;
       
}
//This is the part we are moving our one character per buffer from squash to Sharememory2. 
void SquashToPrinter(char c)
{
    P(Mutex_Share2_Write);
    sh->Share2=c;
    V(Mutex_Share2_Read);
}
void squash(){
// 0 represent true and 1 return as false.

static int is_previous_symbol_asterisk=0;
    
    while(1){
        P(Mutex_Share1_Read);
        if(sh->Share1==EOF){
            if(is_previous_symbol_asterisk)SquashToPrinter('*');
            SquashToPrinter(EOF);
            printf("\n"); 
            //printf("\nEnd squash process\n"); 
            exit(0);}
        // check on memory 1 for if there is *
        if(sh->Share1 == '*')
        {   
            //Two adjacent "*" then convert to "#"
            
            if(is_previous_symbol_asterisk)
            {
                SquashToPrinter('#');
                //Mark is_previous_symbol_asterisk as true
                is_previous_symbol_asterisk=0;
                }
            else
            { 
                is_previous_symbol_asterisk=1;}
        }
        else 
        
        {
            //When we get a char '*' we don't send it to the printer. Just remeber.
            //But when we get other char we should print '*' and current char
            if(is_previous_symbol_asterisk) SquashToPrinter('*');
            SquashToPrinter(sh->Share1);
            is_previous_symbol_asterisk=0;}
        V(Mutex_Share1_Write);
    }


}
void printer(){
    static int count_Words=0;
    while(1){
    P(Mutex_Share2_Read);
    if(sh->Share2==EOF){
    
    //printf("\nEnd printer process\n");  
    exit(0);}
    printf("%c",sh->Share2);
    if(++count_Words%25==0) printf("\n");
    V(Mutex_Share2_Write);
    }
}

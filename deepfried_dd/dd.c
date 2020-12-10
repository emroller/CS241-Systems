
/**
 * deepfried_dd
 * CS 241 - Fall 2020
 */
 //partners: joowonk2, sap3, jeb5
#include "format.h"
#include "string.h"
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
 
static unsigned long total_copied; //total BYTES copied
static int status_flag;
static FILE * input_file;
static FILE * output_file;
static size_t pvalue; //blocks to skip on input_file
static size_t kvalue; //blocks to skip on output_file
static int cvalue;
static size_t blocksize;
static struct timespec start;

static int full_blocks_in;
static int full_blocks_out;
static int partial_blocks_in;
static int partial_blocks_out;
 
void print_report() {
    struct timespec curr;
    clock_gettime(CLOCK_REALTIME, &curr);
    double diff = difftime(curr.tv_sec, start.tv_sec);
    print_status_report(full_blocks_in, partial_blocks_in, full_blocks_out, partial_blocks_out, total_copied, 
    diff + (((double) (curr.tv_nsec - start.tv_nsec)) / 1000000000) );
    status_flag = 0;
}
 
void copy_file() {

    int blocks_copied = 0;
    char buff[blocksize];
    memset(buff, 0, blocksize);

    while (1) {
        if (status_flag) {
            print_report();
        }

        blocks_copied++;

        int read_bytes = fread(&buff, 1, blocksize, input_file);
        if( read_bytes == 0) {
            break;
        }

        fwrite(buff, 1, read_bytes, output_file);
        total_copied += read_bytes;

        if (cvalue == blocks_copied) {
            full_blocks_out++;
            full_blocks_in++;
            break;
        } else if ((int) blocksize != read_bytes) {
            partial_blocks_out++;
            partial_blocks_in++;
            break;
        }
        full_blocks_out++;
        full_blocks_in++;
    }
}
 
void sig_handle(int sig) {
    if (SIGUSR1 == sig) {
        status_flag = 1;
    }
}
 
void init_params(int argc, char **argv) {
    input_file = stdin;
    output_file = stdout;
     cvalue = -1;
    pvalue = kvalue = 0;
    status_flag = 0;
    int signal;
    blocksize = 512;
    full_blocks_in = full_blocks_out = partial_blocks_in = partial_blocks_out = 0;
    total_copied = 0;
 
    while ((signal = getopt (argc, argv, "i:o:b:c:p:k:")) != -1)
        switch (signal) {
            case 'i':
                input_file = fopen(optarg, "r");
                if (input_file == NULL) {
                    print_invalid_input(optarg);
                    exit(1);
                }
                break;
            case 'o':
                output_file = fopen(optarg, "w+");
                if (output_file == NULL) {
                    print_invalid_output(optarg);
                    exit(1);
                }
                break;
            case 'b':
                blocksize = atoi(optarg);
                break;
            case 'c':
                cvalue = atoi(optarg);
                break;
            case 'p':
                pvalue = atoi(optarg);
                break;
            case 'k':
                kvalue = atoi(optarg);
                break;
            case '?':
                exit(1);
            default:
                abort();
            }
}
 
int main(int argc, char **argv) {
    signal(SIGUSR1, sig_handle);
    clock_gettime(CLOCK_REALTIME, &start);
    init_params(argc, argv);

    if (pvalue != 0) {
        if (fseek(input_file, blocksize * pvalue, SEEK_SET)) {
            exit(1);
        }
    }
 
    if (kvalue != 0) {
        if (fseek(output_file, blocksize * kvalue, SEEK_SET)) {
            exit(1);
        }
    }
 
    copy_file();
    print_report();

    fclose(output_file);
    fclose(input_file);
 
    return 0;
}
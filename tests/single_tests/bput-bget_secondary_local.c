#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include "mpi.h"
#include "mdhim.h"

#define KEYS 1000000
#define TOTAL_KEYS 1000000
#define SLICE_SIZE 1000
#define SECONDARY_SLICE_SIZE 5
#define PRIMARY 1
#define SECONDARY 2

uint64_t **keys;
int *key_lens;
uint64_t **values;
int *value_lens;
uint64_t **secondary_keys;
int *secondary_key_lens;
uint64_t **secondary_values;
int *secondary_value_lens;

void start_record(struct timeval *start) {
	gettimeofday(start, NULL);
}

void end_record(struct timeval *end) {
	gettimeofday(end, NULL);
}

void add_time(struct timeval *start, struct timeval *end, long double *time) {
  	long double elapsed = (long double) (end->tv_sec - start->tv_sec) + 
		((long double) (end->tv_usec - start->tv_usec)/1000000.0);
	*time += elapsed;
}

void gen_keys_values(int rank, int total_keys) {
	int i = 0;
	for (i = 0; i < KEYS; i++) {
		keys[i] = malloc(sizeof(uint64_t));	
		*keys[i] = i + (uint64_t) ((uint64_t) rank * (uint64_t)TOTAL_KEYS) + total_keys;
		/* If we are generating keys for the secondary index, then they should be distributed differently
		   across the range servers */
		secondary_keys[i] = malloc(sizeof(uint64_t));
		*secondary_keys[i] = i + (uint64_t) ((uint64_t) rank * (uint64_t)TOTAL_KEYS) + total_keys + SECONDARY_SLICE_SIZE;
		key_lens[i] = sizeof(uint64_t);
		secondary_key_lens[i] = sizeof(uint64_t);
		values[i] = malloc(sizeof(uint64_t));
		value_lens[i] = sizeof(uint64_t);
		*values[i] = rank;
		//The secondary key's values should be the primary key they refer to
		*secondary_values[i] =  i + (uint64_t) ((uint64_t) rank * (uint64_t)TOTAL_KEYS) + total_keys;
	}
}

void free_key_values() {
	int i;

	for (i = 0; i < KEYS; i++) {
		free(keys[i]);
		free(values[i]);
	}
}

int main(int argc, char **argv) {
	int ret;
	int provided;
	int i;
	struct mdhim_t *md;
	int total = 0;
	struct mdhim_brm_t *brm, *brmp;
	struct mdhim_bgetrm_t *bgrm, *bgrmp;
	struct timeval start_tv, end_tv;
	char     *db_path = "./";
	char     *db_name = "mdhimTstDB-";
	int      dbug = MLOG_CRIT; //MLOG_CRIT=1, MLOG_DBG=2
	mdhim_options_t *db_opts; // Local variable for db create options to be passed
	int db_type = LEVELDB; //(data_store.h) 
	long double put_time = 0;
	long double get_time = 0;
	struct index_t *secondary_local_index;
	struct secondary_bulk_info *secondary_info;

	// Create options for DB initialization
	db_opts = mdhim_options_init();
	mdhim_options_set_db_path(db_opts, db_path);
	mdhim_options_set_db_name(db_opts, db_name);
	mdhim_options_set_db_type(db_opts, db_type);
	mdhim_options_set_key_type(db_opts, MDHIM_LONG_INT_KEY);
	mdhim_options_set_max_recs_per_slice(db_opts, SLICE_SIZE);
        mdhim_options_set_server_factor(db_opts, 4);
	mdhim_options_set_debug_level(db_opts, dbug);

	//Initialize MPI with multiple thread support
	ret = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	if (ret != MPI_SUCCESS) {
		printf("Error initializing MPI with threads\n");
		exit(1);
	}

	//Quit if MPI didn't initialize with multiple threads
	if (provided != MPI_THREAD_MULTIPLE) {
                printf("Not able to enable MPI_THREAD_MULTIPLE mode\n");
                exit(1);
        }

	gettimeofday(&start_tv, NULL);
	//Initialize MDHIM
	md = mdhimInit(MPI_COMM_WORLD, db_opts);
	if (!md) {
		printf("Error initializing MDHIM\n");
		MPI_Abort(MPI_COMM_WORLD, ret);
		exit(1);
	}	

	key_lens = malloc(sizeof(int) * KEYS);
	value_lens = malloc(sizeof(int) * KEYS);
	keys = malloc(sizeof(uint64_t *) * KEYS);
	values = malloc(sizeof(uint64_t *) * KEYS);
	secondary_key_lens = malloc(sizeof(int) * KEYS);
	secondary_value_lens = malloc(sizeof(int) * KEYS);
	secondary_keys = malloc(sizeof(uint64_t *) * KEYS);
	secondary_values = malloc(sizeof(uint64_t *) * KEYS);

	/* Primary and secondary key entries */
	MPI_Barrier(MPI_COMM_WORLD);	
	total = 0;
	secondary_local_index = create_local_index(md, LEVELDB, 
						   MDHIM_LONG_INT_KEY);
	while (total != TOTAL_KEYS) {
		//Populate the primary keys and values to insert
		gen_keys_values(md->mdhim_rank, total);
		secondary_info = mdhimCreateSecondaryBulkInfo(NULL, NULL, NULL,
							      secondary_local_index, (void **) secondary_keys, 
							      secondary_key_lens);
		//record the start time
		start_record(&start_tv);	
		//Insert the primary keys into MDHIM
		brm = mdhimBPut(md, (void **) keys, key_lens,  
				(void **) values, value_lens, KEYS, secondary_info);
		//Record the end time
		end_record(&end_tv);
		//Add the final time
		add_time(&start_tv, &end_tv, &put_time);
		brmp = brm;
                if (!brmp || brmp->error) {
                        printf("Rank - %d: Error inserting keys/values into MDHIM\n", md->mdhim_rank);
                } 
		while (brmp) {
			if (brmp->error < 0) {
				printf("Rank: %d - Error inserting key/values info MDHIM\n", md->mdhim_rank);
			}
	
			brmp = brmp->next;
			//Free the message
			mdhim_full_release_msg(brm);
			brm = brmp;
		}
	
		free_key_values();
		total += KEYS;
	}

	/* End primary and secondary entries */


	MPI_Barrier(MPI_COMM_WORLD);
	/* End secondary key entries */

	//Commit the database
	ret = mdhimCommit(md, md->primary_index);
	if (ret != MDHIM_SUCCESS) {
		printf("Error committing MDHIM database\n");
	} else {
		printf("Committed MDHIM database\n");
	}

	//Retrieve the primary key's values from the secondary key
	total = 0;
	while (total != TOTAL_KEYS) {
		//Populate the keys and values to retrieve
		gen_keys_values(md->mdhim_rank, total);
		start_record(&start_tv);
		//Get the values back for each key inserted
		bgrm = mdhimBGet(md, secondary_local_index, (void **) secondary_keys, secondary_key_lens, 
				 KEYS, MDHIM_GET_PRIMARY_EQ);
		end_record(&end_tv);
		add_time(&start_tv, &end_tv, &get_time);
		bgrmp = bgrm;
		while (bgrmp) {
			if (!bgrmp || bgrmp->error) {
				printf("Rank: %d - Error retrieving values starting at: %llu", 
				       md->mdhim_rank, (long long unsigned int) *keys[0]);
			}
	
			//Validate that the data retrieved is the correct data
			for (i = 0; i < bgrmp->num_keys && !bgrmp->error; i++) {   				
				if (!bgrmp->value_lens[i]) {
					printf("Rank: %d - Got an empty value for key: %llu", 
					       md->mdhim_rank, *(long long unsigned int *)bgrmp->keys[i]);
					continue;
				}
			}

			bgrmp = bgrmp->next;
			//Free the message received
			mdhim_full_release_msg(bgrm);
			bgrm = bgrmp;
		}

		free_key_values();
		total += KEYS;
	}

	free(key_lens);
	free(keys);
	free(values);
	free(value_lens);

	MPI_Barrier(MPI_COMM_WORLD);

	//Quit MDHIM
	ret = mdhimClose(md);
	gettimeofday(&end_tv, NULL);
	if (ret != MDHIM_SUCCESS) {
		printf("Error closing MDHIM\n");
	}
	
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
	printf("Took: %Lf seconds to put %d keys\n", 
	       put_time, TOTAL_KEYS * 2);
	printf("Took: %Lf seconds to get %d keys/values\n", 
	       get_time, TOTAL_KEYS * 2);

	return 0;
}

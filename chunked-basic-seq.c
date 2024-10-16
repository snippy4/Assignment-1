#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <omp.h>

#define CHUNK_SIZE 10000  // Define chunk size to process large datasets

int readNumOfPoints(char*);
int readNumOfFeatures(char*);
double *readDataPoints(char*, int, int);
void *writeResultsToFile(double*, int, int, char*);

/*Gets the number of the coordinates in the file. Returns as a single integer*/
int readNumOfPoints(char *filename){
	FILE *file = fopen(filename, "r");
	int numOfPoints = 0;

	if(file == NULL){
		return -1;
	}

	char line[1000];

	while(fgets(line, sizeof(line), file) != NULL){
		numOfPoints++;
	}

	fclose(file);
	return numOfPoints;
}

int readNumOfFeatures(char *filename){
	FILE *file = fopen(filename, "r");

	if(file == NULL){
		return -1;
	}

	char line[1000];
	char *p;

	if(fgets(line, sizeof(line), file) != NULL){
		int numOfFeatures = 1;

		for(p = line; *p != '\0'; p++){
			if(*p == ','){
				numOfFeatures++;
			}
		}

		fclose(file);
		return numOfFeatures;
	}

	fclose(file);
	return -1;
}

double *readDataPoints(char *filename, int numOfPoints, int numOfFeatures){
	FILE *file = fopen(filename,"r");
	char line[1000];
    
    if(file == NULL) {
        printf("Unable to open file: %s\n", filename);
        return NULL;
    }

	double *dataPoints = (double *)malloc(numOfPoints * numOfFeatures * sizeof(double));
	if(dataPoints == NULL){
		return NULL;
	}

	int lineNum = 0, featureIndex;

	while(fgets(line, sizeof(line), file) != NULL && lineNum < numOfPoints){
		char* token = strtok(line, ",");
		featureIndex = 0;

		while(token != NULL && featureIndex < numOfFeatures){
			dataPoints[lineNum * numOfFeatures + featureIndex] = atof(token);
			token = strtok(NULL, ",");
			featureIndex++;
		}

		lineNum++;
	}

	fclose(file);
	return dataPoints;
}

void *writeResultsToFile(double *output, int numOfPoints, int numOfFeatures, char *filename){
	FILE *file = fopen(filename, "w");
	int i, j;

	if(file == NULL){
		printf("Unable to open file: %s", filename);
		return NULL;
	}

	for(i = 0; i < numOfPoints; i++) {
		for(j = 0; j < numOfFeatures; j++){
			if(j < numOfFeatures - 1) fprintf(file, "%lf,", output[i * numOfFeatures + j]);
			else fprintf(file, "%lf", output[i * numOfFeatures + j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
	return output;
}

typedef struct {
    double value;  // Distance
    int index;     // Index of the training point
    double class;  // Class of the training point
} ValueIndexPair;

int compare(const void *a, const void *b) {
    if (((ValueIndexPair*)a)->value > ((ValueIndexPair*)b)->value) return 1;
    else if (((ValueIndexPair*)a)->value < ((ValueIndexPair*)b)->value) return -1;
    else return 0;
}

// Function to find the most frequent class with tie-breaking based on distance
double findMostFrequentWithTieBreak(ValueIndexPair arr[], int k, int test_index) {
    int classCount[99] = {0};  
    int max_count = 0;
    double most_frequent_class = arr[0].class;
    bool tie_occurred = false;
    for (int i = 0; i < k; i++) {
        classCount[(int)arr[i].class]++;
        if (classCount[(int)arr[i].class] > max_count) {
            max_count = classCount[(int)arr[i].class];
            most_frequent_class = arr[i].class;
            tie_occurred = false;  // Clear tie if a class has more occurrences
        } else if (classCount[(int)arr[i].class] == max_count) {
            tie_occurred = true;  // A tie occurs if two or more classes have the same count
        }
    }

    if (tie_occurred) {
        return arr[0].class;  // Return the class of the closest point
    }
    return most_frequent_class;
}

void processChunk(double *train_data, double *test_data, int train_rows, int test_rows, int train_cols, int test_cols, double *point_distances, int k, int chunk_start, int chunk_size) {
	ValueIndexPair *distances = (ValueIndexPair*) malloc(train_rows * sizeof(ValueIndexPair));

	for (int i = chunk_start; i < chunk_start + chunk_size && i < test_rows; i++) {
        for (int j = 0; j < train_rows; j++) {
            double dist = 0.0;
            for (int d = 0; d < test_cols - 1; d++) {
                double diff = test_data[i * test_cols + d] - train_data[j * train_cols + d];
                dist += diff * diff;
            }
            point_distances[(i - chunk_start) * train_rows + j] = dist;
            distances[j].value = dist;
            distances[j].index = j;
            distances[j].class = train_data[j * train_cols + (train_cols - 1)];
        }

		// Sort k-nearest neighbors for this test point
        qsort(distances, train_rows, sizeof(ValueIndexPair), compare);

		// Assign the most frequent class with tie-breaking to the test point
        test_data[i * test_cols + (test_cols - 1)] = findMostFrequentWithTieBreak(distances, k, i);
    }

	free(distances);
}

int main(int argc, char *argv[]){


	clock_t time = clock();
    int train_rows = readNumOfPoints(argv[1]);
    int train_cols = readNumOfFeatures(argv[1]);
    double *train_data = readDataPoints(argv[1], train_rows, train_cols);

    int test_rows = readNumOfPoints(argv[2]);
    int test_cols = readNumOfFeatures(argv[2]);
    double *test_data = readDataPoints(argv[2], test_rows, test_cols);

    char *outfile = argv[3];
	int k = atoi(argv[4]);

	// Chunk processing
	int chunk_size = CHUNK_SIZE;
	double *point_distances = malloc(chunk_size * train_rows * sizeof(double));

	if (point_distances == NULL) {
		printf("Memory allocation failed for point_distances\n");
		exit(1);
	}
	#pragma omp parallel for
	for (int chunk_start = 0; chunk_start < test_rows; chunk_start += chunk_size) {
		int current_chunk_size = (chunk_start + chunk_size > test_rows) ? (test_rows - chunk_start) : chunk_size;
		processChunk(train_data, test_data, train_rows, test_rows, train_cols, test_cols, point_distances, k, chunk_start, current_chunk_size);
	}

	writeResultsToFile(test_data, test_rows, test_cols, outfile);

	free(point_distances);
	free(train_data);
	free(test_data);

	time = (clock() - time);
	printf("Total time: %f seconds\n", (float)time / CLOCKS_PER_SEC);

	return 0;
}

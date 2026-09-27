#ifndef SORT_H
#define SORT_H

#include "record.h"

void swap(int *array, int first_index, int second_index);
void swap_str(char **array, int first_index, int second_index);
int pivot(int *array, int pivot_index, int end_index);
int pivot_str(char **array, int pivot_index, int end_index);
void quick_sort(int *array, int left_index, int right_index);
void quick_sort_str(char **array, int left_index, int right_index);
void selection_sort(int *array, int left, int right);

#endif

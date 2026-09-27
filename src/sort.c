#include <stdio.h>
#include <string.h>
#include "sort.h"
#include "record.h"

void swap(int *array, int first_index, int second_index)
{
	int temp = array[first_index];
	array[first_index] = array[second_index];
	array[second_index] = temp;
}

void swap_str(char **array, int first_index, int second_index)
{
	char *temp = array[first_index];
	array[first_index] = array[second_index];
	array[second_index] = temp;
}

int pivot(int *array, int pivot_index, int end_index)
{
	int swap_index = pivot_index;
	int i;

	for (i = pivot_index + 1; i <= end_index; i++)
	{
		if (array[i] < array[pivot_index])
		{
			swap_index++;
			swap(array, swap_index, i);
		}
	}

	swap(array, pivot_index, swap_index);

	return swap_index;
}

int pivot_str(char **array, int pivot_index, int end_index)
{
	int swap_index = pivot_index;
	int i;

	for (i = pivot_index + 1; i <= end_index; i++)
	{
		if (strcmp(array[i], array[pivot_index]) < 0)
		{
			swap_index++;
			swap_str(array, swap_index, i);
		}
	}

	swap_str(array, pivot_index, swap_index);
	return swap_index;
}

void quick_sort(int *array, int left_index, int right_index)
{
	if (left_index >= right_index)
		return;

	int pivot_index = pivot(array, left_index, right_index);
	quick_sort(array, left_index, pivot_index - 1);
	quick_sort(array, pivot_index + 1, right_index);
}

void quick_sort_str(char **array, int left_index, int right_index)
{
	if (left_index >= right_index)
		return;

	int pivot_index = pivot_str(array, left_index, right_index);
	quick_sort_str(array, left_index, pivot_index - 1);
	quick_sort_str(array, pivot_index + 1, right_index);
}

void selection_sort(int *array, int left, int right)
{
	int i = 0, j = 0;
	for (i = left; i < right; i++)
	{
		int min = i;
		for (j = i + 1; j <= right; j++)
			if (array[j] < array[min])
				min = j;

		int temp = array[min];
		array[min] = array[i];
		array[i] = temp;
	}
}

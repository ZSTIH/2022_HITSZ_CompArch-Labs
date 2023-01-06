#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#define ARRAY_SIZE (1 << 30)                                    // test array size is 2^28
#define TEST_TIMES_0 30000
#define TEST_TIMES_1 60000
#define TEST_TIMES_2 80000

typedef unsigned char BYTE;										// define BYTE as one-byte type

BYTE array[ARRAY_SIZE];											// test array
const int L1_cache_size = 1 << 18;
const int L2_cache_size = 1 << 22;

double get_usec(const struct timeval tp0, const struct timeval tp1)
{
    return 1000000 * (tp1.tv_sec - tp0.tv_sec) + tp1.tv_usec - tp0.tv_usec;
}

// have an access to arrays with L2 Data Cache'size to clear the L1 cache
void Clear_L1_Cache()
{
	memset(array, 0, L2_cache_size);
}

// have an access to arrays with ARRAY_SIZE to clear the L2 cache
void Clear_L2_Cache()
{
	memset(array, 0, ARRAY_SIZE);
}

void Test_Cache_Size()
{
	printf("**************************************************************\n");
	printf("Cache Size Test\n");

	// TODO
	int test_size_log2 = 12; // min: 4KB
	while (test_size_log2 <= 21) // max: 2048KB
	{
		Clear_L1_Cache();
		Clear_L2_Cache();
		struct timeval tp[2];
		double time_used;
		int test_size = (1 << test_size_log2);
		int index = 0;
		const int step_width = 64;
		gettimeofday(&tp[0], NULL);
		for (int i = 0; i < TEST_TIMES_0; i++)
		{
			index = (index + step_width) % test_size;
			array[index]++;
		}
		gettimeofday(&tp[1], NULL);
		time_used = get_usec(tp[0], tp[1]);
		printf("[Test_Array_Size = %6dKB]\t\tAverage access time: %.1fus\n", (1 << (test_size_log2 - 10)), time_used);

		test_size_log2++;
	}
}

void Test_L1C_Block_Size()
{
	printf("**************************************************************\n");
	printf("L1 DCache Block Size Test\n");

	
	// TODO
	int test_block_size_log2 = 4; // min: 16B
	while (test_block_size_log2 <= 9) // max: 512B
	{
		Clear_L1_Cache();
		Clear_L2_Cache();
		struct timeval tp[2];
		double time_used;
		int test_block_size = (1 << test_block_size_log2);
		int index = 0;
		gettimeofday(&tp[0], NULL);
		for (int i = 0; i < TEST_TIMES_1; i++)
		{
			index = (index + test_block_size) % L1_cache_size;
			array[index]++;
		}
		gettimeofday(&tp[1], NULL);
		time_used = get_usec(tp[0], tp[1]);
		printf("[Test_Array_Jump = %6dB]\t\tAverage access time: %.1fus\n", test_block_size, time_used);

		test_block_size_log2++;
	}
}

// void Test_L2C_Block_Size()
// {
// 	printf("**************************************************************\n");
// 	printf("L2 Cache Block Size Test\n");
	
// 	Clear_L2_Cache();											// Clear L2 Cache
	
// 	// TODO
// }

void Test_L1C_Way_Count()
{
	printf("**************************************************************\n");
	printf("L1 DCache Way Count Test\n");
	
	// TODO
	int test_split_groups_log2 = 2; // min: 4
	while (test_split_groups_log2 <= 7) // max: 128
	{
		Clear_L1_Cache();
		Clear_L2_Cache();
		struct timeval tp[2];
		double time_used;
		int test_split_groups = (1 << test_split_groups_log2);
		int group_size = (L1_cache_size << 1) / test_split_groups;
		gettimeofday(&tp[0], NULL);
		for (int group_count = 1; group_count < test_split_groups; group_count += 2)
		{
			for (int offset = 0; offset < group_size; offset++)
			{
				int index = group_count * group_size + offset;
				array[index]++;
			}
		}
		gettimeofday(&tp[1], NULL);
		time_used = get_usec(tp[0], tp[1]);
		printf("[Test_Split_Groups = %6d]\t\tAverage access time: %.1fus\n", test_split_groups, time_used);

		test_split_groups_log2++;
	}
}

// void Test_L2C_Way_Count()
// {
// 	printf("**************************************************************\n");
// 	printf("L2 Cache Way Count Test\n");
	
// 	// TODO
// }

// void Test_Cache_Write_Policy()
// {
// 	printf("**************************************************************\n");
// 	printf("Cache Write Policy Test\n");

// 	// TODO
// }

// void Test_Cache_Swap_Method()
// {
// 	printf("**************************************************************\n");
// 	printf("Cache Replace Method Test\n");

// 	// TODO
// }

void Test_TLB_Size()
{
	printf("**************************************************************\n");
	printf("TLB Size Test\n");

	const int page_size = 1 << 12;								// Execute "getconf PAGE_SIZE" under linux terminal

    // TODO
	int test_tlb_entries_log2 = 4; // min: 16
	while (test_tlb_entries_log2 <= 9) // max: 512
	{
		Clear_L1_Cache();
		Clear_L2_Cache();
		struct timeval tp[2];
		double time_used;
		int test_tlb_entries = (1 << test_tlb_entries_log2);
		int index = 0;
		int step_width = page_size;
		int max_index = test_tlb_entries * page_size;
		gettimeofday(&tp[0], NULL);
		for (int i = 0; i < TEST_TIMES_2; i++)
		{
			index = (index + step_width) % max_index;
			array[index]++;
		}
		gettimeofday(&tp[1], NULL);
		time_used = get_usec(tp[0], tp[1]);
		printf("[Test_TLB_entries = %6d]\t\tAverage access time: %.1fus\n", test_tlb_entries, time_used);

		test_tlb_entries_log2++;
	}
}

int main()
{
	Test_Cache_Size();
	Test_L1C_Block_Size();
	// Test_L2C_Block_Size();
	Test_L1C_Way_Count();
	// Test_L2C_Way_Count();
	// Test_Cache_Write_Policy();
	// Test_Cache_Swap_Method();
	Test_TLB_Size();
	
	return 0;
}

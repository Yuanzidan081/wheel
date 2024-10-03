#include "TimerCnt.h"
#include "stdio.h"
#include <gtest/gtest.h>
#include <vector>
#include <random>

void quickSort(std::vector<int> &v, int l, int r)
{
    if (l >= r)
        return;
    int pivot = v[l];
    int i = l, j = r;
    while (i < j)
    {
        while (i < j && v[j] >= pivot)
            --j;
        v[i] = v[j];
        while (i < j && v[i] <= pivot)
            ++i;
        v[j] = v[i];
    }
    v[i] = pivot;
    quickSort(v, l, i - 1);
    quickSort(v, i + 1, r);
}

// 打印数组前10个元素
void printArray(const std::vector<int> &arr)
{
    for (size_t i = 0; i < std::min(arr.size(), size_t(10)); ++i)
    {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
}
// 测试快速排序
TEST(QuickSortTest, SortsLargeArray)
{

    std::vector<int> arr;
    arr.resize(100000, 0);
    for (size_t i = 0; i < 100000; ++i)
        arr[i] = rand() % 100000;

    std::cout << "Before sorting: ";
    printArray(arr);
    {
        TimerCnt timerCnt;
        quickSort(arr, 0, arr.size() - 1);
    }

    std::cout << "After sorting: ";
    printArray(arr);

    // EXPECT_TRUE(std::is_sorted(arr.begin(), arr.end()));
}
int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    int status = RUN_ALL_TESTS();
    return 0;
}
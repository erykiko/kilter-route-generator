#include <iostream>

int factorial(int n)
{
    if(n <= 1)
        return 1;
    return n * factorial(n - 1);
}

int main()
{
    int value = 5;
    std::cout << "Silnia: " << factorial(value) << std::endl;
    return 0;
}
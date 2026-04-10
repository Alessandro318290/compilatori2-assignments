int add_zero_left(int x) {
    return x + 0;
}

int add_zero_right(int x) {
    return 0 + x;
}

int mul_one_left(int x) {
    return x * 1;
}

int mul_one_right(int x) {
    return 1 * x;
}

int main() {
    int a = add_zero_left(5);
    int b = add_zero_right(7);
    int c = mul_one_left(3);
    int d = mul_one_right(4);
    return a + b + c + d;
}

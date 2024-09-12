int main(void) {
    int i = 0;
    char str[15] = { 0 };
    scanf("%10s", str);
    printf("You entered: %s\n", str);
    if (str[0] == 'b') {
        if (str[1] == 'a') {
            if (str[2] == 'd') { abort(); }
        }
    }
    return 0;
}
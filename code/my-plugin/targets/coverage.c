int main(void) {
    int i = 0;
    char str[15] = { 0 };
    printf("[TARGET %zu] Enter a string: ", getpid());
    scanf("%10s", str);
    printf("You entered: %s\n", str);
    if (str[0] == 'c') {
        if (str[1] == 'i') {
            if (str[2] == 'n') {
                if (str[3] == 'n') {
                    if (str[4] == 'a') {
                        if (str[5] == 'm') {
                            if (str[6] == 'o') {
                                if (str[7] == 'r') {
                                    if (str[8] == 'o') {
                                        if (str[9] == 'l') {
                                            if (str[10] == 'l') { abort(); }
                                        }
                                    }
                                 }
                            }
                        }
                    }
                }
            }
        }
    }
    if (str[0] == 'b') {
        if (str[1] == 'a') {
            if (str[2] == 'd') { abort(); }
        }
    }
    return 0;
}
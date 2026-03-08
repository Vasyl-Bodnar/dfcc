[[deprecated("test"), noreturn]] [[nodiscard]] {
    ;
    [[]] 1;

    [[]] if (0) {
        a + b;
    } else if (1) {
        a + b;
    } else {
        1;
    }

    if (2)
        a + b;
    else {
        1;
    }

    if (3)
        a + b;
    else
        1;

    if (4) {
        a + b;
    } else
        1;

    switch (5) { a + b; }

    while (6) {
        a + b;
        break;
    }

    do {
        a + b;
        break;
    } while (7);

    for (i = 0; i < 8; i++) {
        continue;
    }

    goto x;
    // x:

    return 0;
}

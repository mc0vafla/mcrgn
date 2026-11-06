#!/usr/bin/env fish
eval gcc mcrgn.c -o mcrgn (pkg-config --cflags --libs gtk+-3.0)

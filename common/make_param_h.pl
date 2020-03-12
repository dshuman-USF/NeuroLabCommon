#!/usr/bin/env perl

for (`cat sbparam.defs`) {

    if (/PARAMETER *\(([^=]+)=([^)]+)\)/) {
        push @txt, "#define $1 $2\n";
    }
}
open F, ">sbparam.h";
print F @txt;
close F;

# MyMalloc

This EPITA 3rd year project have to be completed in 1 week
The aim is not to implement a better malloc than the existing one in the libc but to better understand how memory works, is designed on a computer.
It is also made to better understand why there is buffer-overflow error, why some values can be retrieved even if it belongs to another program...

## Disclaimer
This repository is here for an educational purpose only
If you are an EPITA student doing this project, I cannot be charged for any cheat-flag that can be raised on you for using this code or a part of this code.

## Implementation

There is no special implementation like Binary-Buddies or Buckets in this memory-management project.
I simply used a header that holds some infos on the block it manages, linked to other headers as a linked list would be.

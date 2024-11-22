## Bump Allocator (Simple Memory Allocation)

## Overview

This project implements a simple bump allocator using a custom malloc implementation, which is a foundational approach to memory management. The allocator manages a single heap region and allocates memory by "bumping" a pointer forward as each block is allocated. The allocator uses a brute force approach, growing the heap when necessary by requesting additional pages of memory from the operating system.

## Key Concepts

- Heap Management: The bump allocator uses a contiguous memory region known as the heap. This memory is initially set up by extending the heap by a fixed size (one page, typically 4096 bytes) using the mem_sbrk system call.
- Block Allocation: When a user requests memory via malloc, the bump allocator assigns a block of memory by bumping the heap pointer forward. The allocator checks if there is enough remaining space in the heap to fulfill the request; if not, the heap is extended by another page.
- Alignment: To ensure proper memory access and performance, the allocator aligns each block to a multiple of 16 bytes, which is specified by DWORD_SIZE.
- No Freeing: This implementation does not handle freeing memory or coalescing free blocks. The free function is a no-op in this version, which means memory is not reclaimed after it is allocated.

## How the Bump Allocator Works

- Memory Request: When malloc is called with a size parameter, the following happens:
The requested size is adjusted to include the overhead (header/footer) and aligned to the nearest 16-byte boundary.
- If the requested size (asize) is larger than the remaining heap, the heap is extended by either one or two pages of memory.
The heap pointer is then bumped forward by the size of the allocated block, and the remaining heap size is updated accordingly.
Heap Extension: When the heap is extended:
- If the shortfall (the amount of memory needed to satisfy the request) is less than or equal to a page, the heap is extended by one page (4096 bytes).
- If the shortfall exceeds a page, the heap is extended by two pages.
Memory Allocation: Once the space is available, the allocator returns a pointer to the newly allocated memory block, ensuring that the block is aligned properly.
Debugging: A debug print statement is included to show the allocation details, including the pointer address, block size, and remaining heap size after each allocation.
Code Changes

- malloc Function
The following changes were made to implement the bump allocator:

- Adjustment for Alignment:
The requested size is adjusted to ensure that each allocation is aligned to a multiple of 16 bytes (the DWORD_SIZE).
- If the request is smaller than 16 bytes, it is adjusted to at least 16 bytes.
Heap Extension:
- If there is not enough space in the heap to fulfill a request, the heap is extended by either one or two pages based on the shortfall.
The mem_sbrk function is used to request additional memory from the operating system.
- Block Allocation:
Once space is available, the heap pointer is moved forward by the size of the block being allocated. The remaining heap size is then reduced by the block size.

## Credits
- professor Jae Woo Lee and Hans Montero
-  Computer Systems: A Programmer’s Perspective (CSAPP), 3rd Edition, 2015, Pearson – by Randal E. Bryant and David R. O’Hallaron
For better atomics performance on ARM64/Linux, we want to tune the architecture a little bit, minimum required from [here][march] would be something like:

```
gcc -std=c11 -march=armv8-a+lse -mno-outline-atomics ...
```

For a simple function like this:
```
#include <stdatomic.h>

_Atomic int flag = 0;

int bump(int i)
{
    return atomic_fetch_add(&flag, i);
}
```

The GCC default compiles to:
```
0000000000000820 <bump>:
 820:	a9bf7bfd 	stp	x29, x30, [sp, #-16]!
 824:	90000101 	adrp	x1, 20000 <__data_start>
 828:	91005021 	add	x1, x1, #0x14
 82c:	910003fd 	mov	x29, sp
 830:	94000014 	bl	880 <__aarch64_ldadd4_acq_rel>
 834:	a8c17bfd 	ldp	x29, x30, [sp], #16
 838:	d65f03c0 	ret
```
...where that inner function chooses the implementation based on setting some feature flags at library load time.

Compare with just assuming we have LSE:
```
0000000000000760 <bump>:
 760:	90000101 	adrp	x1, 20000 <__data_start>
 764:	91005021 	add	x1, x1, #0x14
 768:	b8e00020 	ldaddal	w0, w0, [x1]
 76c:	d65f03c0 	ret
```

The slower, alternative choice for bare armv8-a is:
```
0000000000000760 <bump>:
 760:	90000101 	adrp	x1, 20000 <__data_start>
 764:	2a0003e2 	mov	w2, w0
 768:	91005021 	add	x1, x1, #0x14
 76c:	885ffc20 	ldaxr	w0, [x1]
 770:	0b020003 	add	w3, w0, w2
 774:	8804fc23 	stlxr	w4, w3, [x1]
 778:	35ffffa4 	cbnz	w4, 76c <bump+0xc>
 77c:	d65f03c0 	ret
```

When compiling on a Mac, the defaults are good.

[march]: https://gcc.gnu.org/onlinedocs/gcc/AArch64-Options.html#index-march

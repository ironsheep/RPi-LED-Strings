	.arch armv6
	.eabi_attribute 28, 1
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.file	"delayRoutine.c"
	.text
	.align	2
	.global	nSecDelayASM
	.arch armv6
	.syntax unified
	.arm
	.fpu vfp
	.type	nSecDelayASM, %function
nSecDelayASM:
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #28
	str	r0, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L2
.L3:
	@ here WAS dummy tst++
	@ldr	r3, [fp, #-16]
	@add	r3, r3, #1
	@add	r3, r3, #1
	@add	r3, r3, #1
	@add	r3, r3, #1
	@add	r3, r3, #1
	@str	r3, [fp, #-16]
	@
	@  now let's put our own timing stuff here
	@nop
	@nop
	@nop
	@ here IS ctr++
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L2:
	ldr	r2, [fp, #-8]
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	blt	.L3
	nop
	add	sp, fp, #0
	@ sp needed
	ldr	fp, [sp], #4
	bx	lr
	.size	nSecDelayASM, .-nSecDelayASM
	.ident	"GCC: (Raspbian 8.3.0-6+rpi1) 8.3.0"
	.section	.note.GNU-stack,"",%progbits

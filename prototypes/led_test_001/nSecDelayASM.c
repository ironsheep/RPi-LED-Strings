// just our delay routine!

// WARNING do NOT add this to makefile - used to generate initial .s (ASM) file ONLY

void nSecDelayASM(int nSecDuration)
{
        volatile int ctr;
        volatile int tst;
        for(int ctr=0; ctr<nSecDuration; ctr++) { tst++; }
}



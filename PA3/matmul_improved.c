/* ============================================================
 * Computes C = A * B
 *   A : M x K  (row-major)
 *   B : K x N  (row-major)
 *   C : M x N  (row-major)
 * ============================================================ */
#include<stddef.h>
#include<riscv_vector.h>
#define KTILE 64

void matmul(float *A, float *B, float *C, int M, int K, int N) {
    int i=0 ; 
    for(;i+7<M ; i+=8){
        for(int j=0 ;j<N;){
            size_t vl = __riscv_vsetvl_e32m2(N-j);
            vfloat32m2_t a0 , a1 , a2, a3, a4, a5, a6 , a7;
            a0 = a1 = a2 = a3 = a4 = a5 = a6 = a7 = __riscv_vfmv_v_f_f32m2(0.0f , vl);
            for(int k0=0 ; k0<K ; k0+=KTILE){
                int kend = k0+KTILE < K ? k0+KTILE : K;
                for(int k=k0 ; k< kend ;k++){
                    vfloat32m2_t vb = __riscv_vle32_v_f32m2(&B[k*N+j] , vl);
                    #define FMA(r) a##r = __riscv_vfmacc_vf_f32m2(a##r , A[(i+r)*K +k] , vb , vl)
                        FMA(0);FMA(1);FMA(2);FMA(3);
                        FMA(4);FMA(5);FMA(6);FMA(7);
                    #undef FMA
                }
            }
            #define ST(r) __riscv_vse32_v_f32m2(&C[(i+r)*N+j] , a##r , vl)
                ST(0);ST(1);ST(2);ST(3);ST(4);ST(5);ST(6);ST(7);
            #undef ST
            j+=vl;
        }
    }
    for(;i<M ; i++){
        for(int j=0 ; j<N;){
            size_t vl  =  __riscv_vsetvl_e32m2(N-j);
            vfloat32m2_t acc = __riscv_vfmv_v_f_f32m2(0.0f , vl);
            for(int k0 =0 ; k0<K ; k0+=KTILE){
                int kend = k0+KTILE < K ? k0+KTILE :K;
                for(int k=k0 ; k< kend ; k++){
                    vfloat32m2_t vb = __riscv_vle32_v_f32m2(&B[k*N+j] , vl);
                    acc = __riscv_vfmacc_vf_f32m2(acc , A[i*K+k] , vb,vl);
                }
            }
            __riscv_vse32_v_f32m2(&C[i*N+j] , acc , vl);
            j+=vl;
        }
    }
}
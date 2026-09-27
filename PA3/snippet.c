for(i=0 ; i< N ; i+=8){
    for(j=0 ; j<N ; j+=8){
        for(k=0 ; k<4 ; k++){
            t0 = A[i+k][j+0];
            t1 = A[i+k][j+1];
            t2 = A[i+k][j+2];
            t3 = A[i+k][j+3];
            t4 = A[i+k][j+4];
            t5 = A[i+k][j+5];
            t6 = A[i+k][j+6];
            t7 = A[i+k][j+7];

            B[j+0][i+k] = t0;
            B[j+1][i+k] = t1;
            B[j+2][i+k] = t2;
            B[j+3][i+k] = t3;

            B[j+0][i+k+4] = t4;
            B[j+1][i+k+4] = t5;
            B[j+2][i+k+4] = t6;
            B[j+3][i+k+4] = t7;
        }
        for(k=0 ; k<4 ; k++){
            t0  = A[i+4][j+k];
            t1  = A[i+5][j+k];
            t2  = A[i+6][j+k];
            t3  = A[i+7][j+k];

            t4 = B[j+k][i+4];
            t5 = B[j+k][i+5];
            t6 = B[j+k][i+6];
            t7 = B[j+k][i+7];
            
            B[j+k][i+4] = t0;
            B[j+k][i+5] = t1;
            B[j+k][i+6] = t2;
            B[j+k][i+7] = t3;

            B[j+k+4][i+0] = t4;
            B[j+k+4][i+1] = t5;
            B[j+k+4][i+2] = t6;
            B[j+k+4][i+3] = t7;

            t0 = A[i+4][j+k+4];
            t1 = A[i+5][j+k+4];
            t2 = A[i+6][j+k+4];
            t3 = A[i+7][j+k+4];

            B[j+k+4][i+4] = t0;
            B[j+k+4][i+5] = t1;
            B[j+k+4][i+6] = t2;
            B[j+k+4][i+7] = t3;
        }
    }
}
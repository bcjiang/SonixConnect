#include <math.h>

void fft(double *xp, double *yp, int N, int a1p)
{
	int n2,n3,n4;
	int LL,L1,L2,L3,L4,L8;
	int i,j,j1,j2,k;
	double *xq,*yq;
	double s1p,a2p,cp,sp;
	double t1p,t2p;

	xq=(double*)malloc(sizeof(double)*(N+1));
	yq=(double*)malloc(sizeof(double)*(N+1));

	for(i=0;i<N;i++)
	{xq[i+1]=xp[i];yq[i+1]=yp[i];}

	n4=N;
	for(LL=-1;n4!=0;LL++)n4/=2;
	L1=N;
	s1p=2 * M_PI / N;
	for(L8=1;L8<=LL;L8++){
		L2=L1;
		L1/=2;
		a2p=0;
		for(L3=1;L3<=L1;L3++){
			cp=cos(a2p);sp=sin(a1p*a2p);
			a2p+=s1p;
			for(L4=L2;L4<=N;L4+=L2){
				j1=L4-L2+L3;j2=j1+L1;
				t1p=xq[j1]-xq[j2];
				t2p=yq[j1]-yq[j2];
				xq[j1]=xq[j1]+xq[j2];
				yq[j1]=yq[j1]+yq[j2];
				xq[j2]=cp*t1p+sp*t2p; 
				yq[j2]=cp*t2p-sp*t1p;
			}
		}s1p=2*s1p;
	}

	j=1;
	n2=N/2;n3=N-1;
	for(i=1;i<=n3;i++){
		if(i<j){
			t1p=xq[j];t2p=yq[j];
			xq[j]=xq[i];yq[j]=yq[i];
			xq[i]=t1p;yq[i]=t2p;
		}
		for(k=n2;k<j;j-=k,k/=2);
		j+=k;
	//std::cout<<"In FFT loop: "<<i<<std::endl;
	}

	if(a1p==-1)
		for(i=1;i<=N;i++)
		{xq[i]/=N;yq[i]/=N;}

	for(i=0;i<N;i++)
	{xp[i]=xq[i+1];yp[i]=yq[i+1];}
	free(xq);
	free(yq);
}
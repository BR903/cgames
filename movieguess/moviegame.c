#include<stdio.h>
#include<stdlib.h>
#include<string.h>
int main()
{
    char movieset[5][10]={
    {'d','o','n','\0'},
    {'g','h','a','j','i','n','i','\0'},
    {'r','a','m','b','o','\0'},
    {'r','o','c','k','y','\0'},
    {'s','p','e','c','t','r','e','\0'}
    };
	char movie[10];
	char place[10];
    //char place[5]={' ',' ',' ',' ',' '};
    int n,s,i,m,k,j,l;
    char guess;
    printf(" \nMOVIE QUIZ\n ");
    printf(" \nGuess the Correct Movie\n");
   n = rand() % 5 ;
   m = rand() % 5 ;
   s=rand()%5;
   //k=rand()%5;
	for(i=0;i<5;i++)
	{
		if(i==s)
		{
			for(j=0;j<10;j++)
			{
				movie[j]=movieset[s][j];	
			}
		}
	}
   place[n]=movie[n];
   place[m]=movie[m];
   for(i=0;i<5;i++)
   {
    if(i==n||i==m)
      printf("%c",movie[i]);
    else
      printf("_");
   }
   for(i=0;i<12;i++)
   {
    printf("\nEnter the letter\n");
   scanf("%c",&guess);
   for(j=0;j<10;j++)
   {
       if(j!=m&&j!=n){
        if(guess==movie[j])
            place[j]=movie[j];
       }
   }
   for(k=0;k<10;k++)
    printf("%c",place[k]); 
   }
   if(strcmp(place,movie)==0)
        printf("\nhehe you survived\n");
    else 
    	printf("\nsorry try in next life\n");
} 


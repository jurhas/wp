//Software assolutamente di pubblico dominio.
// spanu_andrea(at)yahoo.it per segnalazione o richieste di modifica

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "mparser.h"
#include "sys/stat.h"
#include "dirent.h"
#ifdef _WIN32
#include "windows.h"
#endif
#define COL_SPACING 10000
#define COL_SPACING_LEN 4


typedef enum
{
	PBCRED = 4,
	PBCGREEN = 2,
	PBCBLUE = 1
} PRINTBOLDCOLOR;

#ifndef _PRINT_BOLD_
#define _PRINT_BOLD_
#ifdef _WIN32
void printBold(const char *zText, int color)
{
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO defaultScreenInfo;
	GetConsoleScreenBufferInfo(out, &defaultScreenInfo);
	SetConsoleTextAttribute(out, color | FOREGROUND_INTENSITY);
	printf("%s", zText);
	SetConsoleTextAttribute(out, defaultScreenInfo.wAttributes);
}
#else
#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

void printBold(const char *zText, int color)
{
	if (color == PBCRED)
		printf(KRED "%s" KNRM, zText);
	else if (color == PBCGREEN)
		printf(KGRN "%s" KNRM, zText);
	else if (color == PBCBLUE)
		printf(KCYN "%s" KNRM, zText);
}
#endif
#endif

#ifdef SQLITE3_H

#endif

void print_help()
{
	printf("\t<empty>	navigates file par.txt from begin\n");
	printf("\t<integer>	seeks par.txt at the given line\n");
	printf("\t-f<name_infile>  navigates a given file from begin \n");
	printf("\t-f<name_infile>  <integer> seeks the given file from the given line \n");
	printf("\t-c<name_infile>  [-o<nameoutfile>] [-a<nameautor>] checks the given file and exports to \n");
	printf("\t\t<nameoutfile> if present or to pout.txt and sets as autor  <nameautor> if present otherwise empty \n");
	printf("\t-d<name_dir>  [-o<nameoutfile>] scan recursively a directory and exports all to <nameoutfile> if present or to pout.txt  \n");
	printf("\t\tthe title will be the name of the file and the author the name of the directory\n");
	printf("\t<empty>	navigates file par.txt from begin\n");
	
	
}
void navigate (FILE *f, mll line)
{
#define SW_0 0
#define SW_TITLE 1
#define SW_TFC 2
#define SW_AUTHOR 3
#define SW_AFC 4
#define SW_N 5
	
	int c,cur_row=1,i,state=SW_0;
	char buf[30]={0},*start_col,*sv_sc;

	while(line>0 && (c= getc(f)) !=EOF)
		if(c=='\n')
		{
			--line;
			++cur_row;	
		}
		
	while ( (c= getc(f))!=EOF)
	{
		if(state==SW_0)
		{
			printBold("riga:",PBCRED);
			printf("%d",cur_row);
			
			printBold(" Titolo: ",PBCBLUE);
			state=SW_TITLE;
		}
		if(c=='#' && state==SW_TITLE)
		{
			state=SW_TFC;
		}
		else if(c=='#' && state==SW_TFC)
		{
			printBold("\n\tAutore: ",PBCBLUE);
			state=SW_AUTHOR;
		}else if(state==SW_TFC)
		{	
			state=SW_TITLE;
			putchar('#');
			putchar(c);
		}else if(c=='#' && state==SW_AUTHOR)
		{
			state=SW_AFC;
		}else if (c=='#' && state==SW_AFC)
		{
			state=SW_N;
			
		}else if(state==SW_AFC)
		{	
			state=SW_AUTHOR;
			putchar('#');
			putchar(c);
		}
		else if (state==SW_TITLE || state==SW_AUTHOR)
			putchar(c);
		else
		{
			do
			{
				printf("\n\t\t");
				putchar(c);
				sv_sc=buf;
				while( (c= getc(f))!=EOF && c!='\n' && isdigit(c))
					*sv_sc++=c;
				*sv_sc='\0';
				start_col=sv_sc-4;
				sv_sc=start_col;
				while(*sv_sc=='0')
					++sv_sc;
				printBold(" c:",PBCGREEN);
				printf("%s",sv_sc);
				*start_col='\0'; 
				printBold(" r:",PBCGREEN);
				printf("%s",buf);
			}while(c!='\n' && c!=EOF);
			
				
			if(c=='\n')
			{
				++cur_row;
				printf("\n");
			}
			printf("q=fine>");
			while( (i=getchar())!='\n')
				if(i=='q')
					return;
				
			state=SW_0;
		}

	}
}
void first_write(m8String *s,char *tit,char *aut)
{
	if(s->n)
		m8s_concatcm(s,'\n');
	if(tit)
		m8s_concats(s,tit);
	m8s_concats(s,"##");
	if(aut)
		m8s_concats(s,aut);
	m8s_concats(s,"##");

}
int check(FILE *f,m8String *s,mStack *stck[3],char * tit,char *aut)
{
	mStack *pt=stck[0],*pq=stck[1],*pg=stck[2];
	mll row=1,col=1;
	int c,has_wrote=0;
#define INT_MACRO_FW(a,b,c)  do{\
					if(!has_wrote)\
					{\
						has_wrote=1;\
						first_write(a,b,c);\
					}\
				}while(0)  	
	while(mStack_pop(pt));
	while(mStack_pop(pq));
	while(mStack_pop(pg));
	while ( (c=getc(f))!=EOF)
	{
		switch(c)
		{
			case '(':
				pt->io_val.ull=row*COL_SPACING+col;
				mStack_push(pt);
				break;
			case '[':
				pq->io_val.ull=row*COL_SPACING+col;
				mStack_push(pq);
				break;
			case '{':
				pg->io_val.ull=row*COL_SPACING+col;
				mStack_push(pg);
				break;
			case ')':
				if(!mStack_pop(pt))
				{
					INT_MACRO_FW(s,tit,aut);
					m8s_concatcm(s,')');
					m8s_concati(s,row*COL_SPACING+col);
				}
				break;
			case ']':
				if(!mStack_pop(pq))
				{
					INT_MACRO_FW(s,tit,aut);
					m8s_concatcm(s,']');
					m8s_concati(s,row*COL_SPACING+col);
				}
				break;
			case '}':
				if(!mStack_pop(pg))
				{
					INT_MACRO_FW(s,tit,aut);
					m8s_concatcm(s,'}');
					m8s_concati(s,row*COL_SPACING+col);
				}
				break;
		}
		if(c=='\n')
		{
			++row;
			col=1;
		}else if(c<127 || c>0xBF)
			++col;
	}
	
	while(mStack_pop(pt))
	{	
		INT_MACRO_FW(s,tit,aut);
		m8s_concatcm(s,'(');
		m8s_concati(s,pt->io_val.ull);
	}
	while(mStack_pop(pq))
	{	
		INT_MACRO_FW(s,tit,aut);
		m8s_concatcm(s,'[');
		m8s_concati(s,pq->io_val.ull);
	}
	while(mStack_pop(pg))
	{
		INT_MACRO_FW(s,tit,aut);
		m8s_concatcm(s,'}');
		m8s_concati(s,pg->io_val.ull);
	}
	return 1;
	
	
#undef INT_MACRO_FW
}
int rec(m8String *cdir, m8String *ans, mStack *stk[3] )
{
	struct dirent *dp;
	struct stat stbuf;
	 
	DIR *dfd;
	m8String *ndir=new_m8String();
	 
	 
	if ((dfd = opendir(cdir->s)) == NULL)
	{
		//fprintf(stderr, "Accesso a cartella %s negato\n", dir);
		return 0 ;
	}
	
	while ((dp = readdir(dfd)))
	{

		 if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;
		m8s_reset(ndir);
		m8s_concat8s(ndir,cdir);
		m8s_concatcm(ndir,'/');
		m8s_concats(ndir,dp->d_name);
		
		if (lstat(ndir->s, &stbuf) != 0)
		{
			//fprintf(stderr, "Accesso a file %s negato\n", name);
			continue;
		}

		if (S_ISDIR(stbuf.st_mode)) // (stbuf.st_mode & S_IFMT) == S_IFDIR)
		{
			 rec(ndir,ans,stk);
		}
		else if (S_ISLNK(stbuf.st_mode))
		{
		}
		else
		{
			FILE * f=fopen(ndir->s,"rb");
			if(!f)
				continue;
			char *title=cdir->s+cdir->n;
			while(title > cdir->s && *title!='/' 
#ifdef _WIN32
			&& *title!='\\'
#endif
			)
				--title;
			check(f,ans,stk,title,dp->d_name);
			fclose(f);
		}
	}
	destroy_m8String(ndir);
	closedir(dfd);
	return 1 ;





} 
int main(int argc, char *argv[])
{

#ifdef _WIN32
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
#endif
	char *file_name="par.txt",   *out_name="pout.txt", *author="";
	FILE *f;
	int c,i;
	mll line=0;	
enum
{
	WP_NAV,
	WP_CHK,
	WP_DIR
};
	
	if(argc<2)
		c=WP_NAV;
	else if( isdigit(argv[1][0]))
	{
		c=WP_NAV;
		if(argc>1)
			line=atoi(argv[1])-1;
	} else if (isalpha(argv[1][0]) || strncmp(argv[1],"-f",2)==0)
	{
		c=WP_NAV;
		if(argv[1][0]=='-')
			file_name=argv[1]+2;
		else
			file_name=argv[1];
		i=1;	
		if(!*file_name && argc>i)
		{
			++i;
			file_name=argv[i];
			
		}else 
		if(!*file_name)
		{
			printf("Wrong argument list\n");
			return 0; 
		}
		if(argc>i)
			line=atoi(argv[i])-1;		
	} else if(strncmp(argv[1],"-c",2)==0)
	{
		c=WP_CHK;
		i=2;
		if(argv[1][2])
			file_name=argv[1]+2;
		else if(argc>2)
			file_name=argv[i++];
		for(;i<argc;++i)
		{
			if(strncmp(argv[i],"-o",2)==0 &&argv[i][2] )
				out_name=argv[i]+2;
			else if(strncmp(argv[i],"-o",2)==0 && argc>i+1)
			{
				out_name=argv[++i];
			}else if(strncmp(argv[i],"-o",2)==0)
			{
			}else if(strncmp(argv[i],"-a",2)==0 &&argv[i][2] )
				author=argv[i]+2;
			else if(strncmp(argv[i],"-a",2)==0 && argc>i+1)
			{
				author=argv[++i];
			}else if(strncmp(argv[i],"-a",2)==0)
			{
			}
		
		}
			
	}  else if(strncmp(argv[1],"-d",2)==0)
	{
		c=WP_DIR;
		i=2;
		if(argv[1][2])
			file_name=argv[1]+2;
		else if(argc>2)
			file_name=argv[i++];
		for(;i<argc;++i)
		{
			if(strncmp(argv[i],"-o",2)==0 &&argv[i][2] )
				out_name=argv[i]+2;
			else if(strncmp(argv[i],"-o",2)==0 && argc>i+1	)
			{
				out_name=argv[++i];
			} 
		
		}
			
	} else if (strncmp(argv[1],"-h",2)==0) 
	{
		print_help();
		return 0;
	}else 
	{
		printf("Scelta non valida\n");
		print_help();
		return 0;
	}
	
	if(c==WP_NAV)
	{
		f=fopen(file_name,"rb");
		if(!f)
		{
			printf("Impossibile aprire file %s\n",file_name);
			return 0;
		}
		navigate(f,line);
		fclose(f);
	}else
	{
		m8String *s=new_m8String();
		mStack * stck[3]={new_mStack(10),new_mStack(10),new_mStack(10)};
		if(c==WP_CHK)
		{
			f=fopen(file_name,"rb");
			if(f)
			{
				check(f,s,stck,file_name,author);
				if(s->n==0)
				{
					printf("Nessun'anomalia riscontrata\n");
				}else
				{
					FILE *fw=fopen(out_name,"wb");
					if(!fw)
					{
						printf("Impossibile aprire output file %s\n",out_name);
					}else
					{
						fputs(s->s,fw);
						fclose(fw);
					}
				}
				
				fclose(f);
			} else
				printf("Impossibile aprire file %s\n",file_name);
			
			
			
		}else
		{
			m8String *cdir=new_m8String();
			m8s_concats(cdir,file_name);
			rec(cdir,s,stck);
			if(s->n==0)
			{
				printf("Nessun'anomalia riscontrata\n");
			}else
			{
				FILE *fw=fopen(out_name,"wb");
				if(!fw)
				{
					printf("Impossibile aprire output file %s\n",out_name);
				}else
				{
					
					fputs(s->s,fw);
					fclose(fw);
				}
			}
			destroy_m8String(cdir);
		}
		
		destroy_m8String(s);
		destroy_mStack(stck[0],NULL);
		destroy_mStack(stck[1],NULL);
		destroy_mStack(stck[2],NULL);
	
	
	}
  
  
#ifdef DHASH_H_INCLUDED
	dend();
#endif
	return 0;
	
}

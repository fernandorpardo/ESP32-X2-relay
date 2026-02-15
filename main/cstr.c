/** ************************************************************************************************
 *	C-strings management library
 *  (c) Fernando R (iambobot.com)
 *
 *  C strings management
 *
 *  1.0.0 - May 2021
 *  1.1.0 - October 2022
 *  1.2.0 - November 2022
 * - jsonParseObject deprecated
 *  1.2.1 - January 2023
 * - XML parser clean-up
 *  1.2.2 - November 2024
 * - fix cstr_replace
 * - added jsonScan()
 *  1.2.3 - August 2025
 * - jsonParseValue added Square Bracket management (case "e.g. (3)" )for json array support
 * - jsonScan fix, added size_t max_value_sz
 *   unsigned int jsonScan(char*ptr, unsigned int pos0, unsigned int n, char* key, size_t max_key_sz, char* value, size_t max_value_sz)
 *
 ** ************************************************************************************************
**/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "cstr.h"


/** 
* -------------------------------------------------------------------------------------------------
*
*      CSTR - Character string management functions
*
* -------------------------------------------------------------------------------------------------
**/
// search 'needle' into 'str' and
// retuns
//    the postion in 'needle' if found
//    -1 if not found
int cstr_find(char* str, char const* needle, int pos, int max_sz)
{
	int i= 0;
	int j= 0;
	if(str==0 || needle==0 || needle[0]=='\0') return -1;
	if(pos>0) while(str[i]!='\0' && i<pos) i++;
	if(str[i]=='\0') return -1;
	while(str[i]!='\0' && (max_sz==0 || i<max_sz))
	{
		char *p=&str[i], c= needle[0];
		for(; *p!=c && (max_sz==0 || i<max_sz) && str[i]!='\0'; p++, i++);
		if(*p==c)
		{
			j=0;
			char *p= &str[i+j], *pn= (char*)&needle[j];
			for(; *p==*pn && (max_sz==0 || (i+j)<max_sz) && *p!='\0'; p++, pn++, j++);
			if(needle[j]=='\0' && j>0) 
				return i;
			i++;
		}
	}
	return -1;
} // cstr_find

void cstr_dump(char * bf, int nbytes)
{
	//fprintf(stdout,"\nn= %d\n", nbytes);
	char str[32];
	str[0]='\0';
	int j=0;
	for(int i=0; i<nbytes; i++)
	{
		char c= bf[i];
		fprintf(stdout, "%02X ", bf[i]);
		if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>=0x20 && c<=0x7e)) str[j++]= c;
		else str[j++]='.';
		str[j]='\0';
		if(j== 16)
		{
			fprintf(stdout,"   %s\n", str);
			j=0;
		}
	}
	if(j)
	{
		while(j++<16) fprintf(stdout,"   ");
		fprintf(stdout,"   %s\n", str);
	}
	//fprintf(stdout,"\n");
}

void cstr_fdump(FILE *fp, char * bf, int nbytes)
{
	char str[32];
	str[0]='\0';
	int j=0;
	char sbf[64];
	for(int i=0; i<nbytes; i++)
	{
		char c= bf[i];
		snprintf(sbf, sizeof(sbf), "%02X ", bf[i]);
		fwrite(sbf,  strlen(sbf), 1, fp);
		if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>=0x20 && c<=0x7e)) str[j++]= c;
		else str[j++]='.';
		str[j]='\0';
		if(j== 16)
		{
			snprintf(sbf, sizeof(sbf),"   %s\n", str);
			fwrite(sbf,  strlen(sbf), 1, fp);
			j=0;
		}
	}
	if(j)
	{
		while(j++<16) {
			snprintf(sbf, sizeof(sbf),"   ");
			fwrite(sbf,  strlen(sbf), 1, fp);
		}
		
		snprintf(sbf, sizeof(sbf),"   %s\n", str);
		fwrite(sbf,  strlen(sbf), 1, fp);
	}
}

void cstr_replace (char *str, char a, char b)
{
	if(str==0 || str[0]=='\0') return;
	for(int i=0; str[i]!='\0'; ) 
		if(str[i] == a) 
		{
			// remove
			if(b=='\0') for(int j=i; str[j]!='\0'; j++) str[j]= str[j+1];
			// replace
			else str[i++]=b;
		}
		else i++;
} // cstr_replace()

char *cstr_sub (char *str, char *sub, int p0, int p1)
{
	int i=0;
	for(; (p0+i)<=p1 && str[p0+i]!='\0'; i++) sub[i]= str[p0+i];
	sub[i]='\0';
	return sub;
} // cstr_sub()

// to be used instead of snprintf to prevent destination size warning
char* cstr_copy(char* dst, char* org , size_t dst_max_sz)
{
	int i=0;
	int sz= (int) dst_max_sz;
	for(; org[i]!='\0' && i<(sz - 1); i++) dst[i]=org[i];
	dst[i]= '\0';
	return dst;
} // cstr_copy


/**
 * ------------------------------------------------------------------------------------------------
 *										JSON PARSER library
 *
 * https://javaee.github.io/tutorial/jsonp001.html
 *
 * JSON defines only two data structures: objects and arrays. An object is a set of name-value pairs, 
 * and an array is a list of values. JSON defines seven value types: 
 * string, number, object, array, true, false, and null.
 *
 * JSON has the following syntax.
 * -  Objects are enclosed in braces ({}), their name-value pairs are separated by a comma (,), and 
 *    the name and value in a pair are separated by a colon (:). Names in an object are strings, whereas 
 *    values may be of any of the seven value types, including another object or an array.
 * -  Arrays are enclosed in brackets ([]), and their values are separated by a comma (,). Each value 
 *    in an array may be of a different type, including another array or an object.
 * -  When objects and arrays contain other objects or arrays, the data has a tree-like structure.
 * 
**/

/*
e.g. (1)
	"context":{"id":"01GE6KATDN0S5DQ90PMFKP7D77","parent_id":null,"user_id":null}
	for input name= context
	return a c-string in "value" with the 
	value between brackets including the open&close brackets,i.e.,:
	{"id":"01GE6KATDN0S5DQ90PMFKP7D77","parent_id":null,"user_id":null}

e.g. (2)
	"entity_id":"sun.sun"
	for input name= entiry_id
	return a c-string in variable <value> with the value "sun.sun" including the quotation marks
e.g. (3)
	"array":["item1","item2", ... ]
*/
char* jsonParseValue(const char *name, char*ptr, unsigned int pos0, unsigned int len,  char* value, size_t value_sz_max)
{	
	// PARSE JSON
	unsigned int a= pos0, b=0;
	char c;
	value[0]= '\0';
	
	// move after first { or [ or spaces
	for(c= ptr[a]; a<(pos0 + len) && (c== ' ' || c == '{' ); c=ptr[++a]);	
	unsigned int i, j;
	char str[512];
	while(a < (pos0 + len))
	{
		//b= pos0 + len;
		// get next name, look for colon symbol <:>	
		for(i= a, j=0; ptr[i]!=':' && i<(pos0 + len) && j<(sizeof(str)-1); i++, j++) str[j]= ptr[i];
		str[(i<(pos0 + len))? j : 0]= '\0';
		
		
		//fprintf(stdout, "\n--- name %s %s", name, str);
		
		i++;
		// <i> is pointing for the character after colon symbol <:>
		// remove quotation marks "
		cstr_replace(str,'"','\0');
		cstr_replace(str,' ','\0');
		//fprintf(stdout, "\n--- %s  %c", str, ptr[i]);


			
		b= i;
		// position b at the end
		int CurlyBracketCount= 0;	// { }
		int SquareBracketCount= 0;	// [ ]
		for(c= ptr[b]; b<(pos0 + len) ; c=ptr[++b])
		{
			if( c == '{') CurlyBracketCount ++;
			else if( c == '}') 
			{	
				if(CurlyBracketCount == 1) {b++; break;}			
				if(CurlyBracketCount == 0) {break;}
				CurlyBracketCount--;
			}
			else if( c == '[') SquareBracketCount ++;
			else if( c == ']') 
			{	
				if(SquareBracketCount == 1) {b++; break;}
				//if(SquareBracketCount == 0) {break;}					
				SquareBracketCount--;
			}				
			else if( c == ',' && SquareBracketCount==0 && CurlyBracketCount== 0) break;
		}
		// b points to the end og value: to , or }		
			
		// is the name I am searching?
		if (strcmp(str, name)==0 )
		{			
			// then copy value and return
			for(j=0; i<b && j<value_sz_max; i++, j++) value[j]= ptr[i];
			value[j]= '\0';	
			return value;
		}
		
		// next
		//for(c= ptr[i]; i<(pos0 + len) && c!=',' && c!='{'; c=ptr[++i]);
		a= b + 1;
		//a= i;
	} 
	return value;
} // jsonParseValue


// END OF FILE
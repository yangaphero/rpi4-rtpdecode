#include <stdio.h>  
#include <string.h>
#include <stdlib.h> 
#include <malloc.h>


/* Base64 编码 */   
 char* base64_encode(const  char* data, int data_len);   
/* Base64 解码 */   
 //char *base64_decode(const  char* data, int data_len);   
int base64_decode(const  char *data, int data_len,char *out_data) ;

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
int conv(char * infilename);

int main(int argc, char *argv[])

{
    for (int i=1;i<argc;i++){
        conv(argv[i]);
    }
}
int conv(char * infilename){
    /* declare a file pointer */
    FILE    *infile,*outfile;
    char    *buffer;
    long    numbytes;
    
    /* open an existing file for reading */
    infile = fopen(infilename, "rb");
    char outfilename[512];

    sprintf(outfilename,"conv_%s.bmp",infilename);
    outfile = fopen(outfilename, "wb"); 

    const unsigned char bmpHeader[]={
    0x42,0x4D,0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x28,0x00,
    0x00,0x00,0x80,0x00,0x00,0x00,0xC0,0xFF,0xFF,0xFF,0x01,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x02,0x04,0x00,0x00,0x23,0x2E,0x00,0x00,0x23,0x2E,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0xAA,0xAA,
    };

    /* quit if the file does not exist */
    if(infile == NULL)
        return 1;
    
    /* Get the number of bytes */
    fseek(infile, 0L, SEEK_END);
    numbytes = ftell(infile);
    
    /* reset the file position indicator to 
    the beginning of the file */
    fseek(infile, 0L, SEEK_SET);    
    
    /* grab sufficient memory for the 
    buffer to hold the text */
    buffer = (char*)calloc(numbytes, sizeof(char)); 
    
    /* memory error */
    if(buffer == NULL)
        return 1;
    
    /* copy all the text into the buffer */
    fread(buffer, sizeof(char), numbytes, infile);
    fclose(infile);

    /* confirm we have read the file by
    outputing it to the console */
    printf("The file called test.dat contains %ld\n", numbytes);
    for (int i=0;i<numbytes;i++){
        if ((i%16)==0 && i!=0)
            printf("\n");
        printf("0x%02X",buffer[i] &0xff);
        if (i!=numbytes-1)
            printf(",");
        
    }

    for (int i=0;i<62;i++){
        fputc(bmpHeader[i],outfile);
    }

    int h=64;
    int w=128;
    char target[8192];
    int indx=0;
    printf("\n");

    char  b=0x0;
    for (int jh=0;jh<h;jh++){
        //printf("%02d\n",jh);
        for (int jw=0;jw<w/8;jw++){
            /*
            if (jh==0)
                printf("  %02d:",jw);
            */
            for (int k=0;k<=7;k++){
                
                b<<=1;
                int c=(k*8+64*jw)+128*8*(jh/8);
                int c1=c/8;
                
                uint8_t cc= buffer[c1];
                uint8_t bitnum=(jh%8);
                uint8_t val=(cc>>(bitnum)) & 0x01;
                val^=1;
                b|= val;
                
                //printf("%d,",c);
            }

                //printf("\n");
            
            target[indx]=b;
            indx++;
        }
        //printf("\n");
        
    }
    
    printf("\nindx:%d\n",indx);

    for (int i=0;i<256;i++){
        if (i%16==0)
        printf("\n");
            printf("%02X,",target[i] & 0xFF);
    }

    for (int i=0;i<numbytes;i++){
        fputc(target[i],outfile);
    }

    free(buffer);
    fclose(outfile);
    fclose(infile); 
    return 0;
}
   
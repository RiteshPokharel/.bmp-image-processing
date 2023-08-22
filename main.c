#include <stdio.h>
#include <stdlib.h>      //for DMA
#include <string.h>     // for using memset()
#include <math.h>

#define FAILURE 1
#define SUCCESS 0

typedef char int8;
typedef short int int16;
typedef int int32;
typedef long long int64;
typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef struct          //structure to store the width height and pixel data(in a 3x3 matrix) of the image
{
    uint32 width;
    uint32 height;
    uint8 ***pdata;
} image;

typedef struct
{
    uint8 id[2];    // this is id of the image which is 'BM' for bmp file
} bmpid;

typedef struct
{
    uint32 size;              // total size of the file
    uint32 reserved;         // reserved(=0)
    uint32 offset;          // offset from beginning of file to beginning of the pixel values
} header;

typedef struct
{
    uint32 info_sz;          // size of this header(=40 bytes)
    uint32 width;            // width of image in pixel
    uint32 height;           // height of image in pixel
    uint16 nplanes;          // no of planes used(=1)
    uint16 bpp;              // no of bits used to represent a color in a pixel(usually 24bits(3 bytes) for rgb)
    uint32 compr_type;       // type of compression if any (=0 if no compression)
    uint32 px_size;          // size of the file used for storing pixel values
    uint32 hres;             // horizontal resolution (pixels/meter)
    uint32 vres;             // vertical resolution (pixels/meter)
    uint32 nused_colors;     // no of used colors. For a 8-bit / pixel bitmap this will be 256
    uint32 nimp_colors;      // no of imp colors(0= all)  (generally ignored)
} info;

image createImage(uint32, uint32);          //decalation of various functions used in images.c file
void releaseImage(image *);
void resetImage(image *);
uint32 read_bmp_to_struct(char *, image *);
uint32 save_img_to_file(const char *, image *);
void save_4bytes(uint32, FILE *);
void copyimage(image*,image*);
void generate_kernel(double k[][11] ,int,double);
void convolve(image*,image*,int,double kernel[][11]);
void gaussian_blur(image*,image*,int,double);

image createImage(uint32 width, uint32 height)
{
    // creating an obj of struct image
    image img;                             
    img.height=0;
    img.width=0;

    // reseting the img(i.e making sure all values(height,width and pixel data) are set to 0)
    resetImage(&img); 

    // calculating the total size needed for storing pixel values of the image                     
    uint32 size = width * height * 3;  

    if (size > 0)
    {
        img.width = width;
        img.height = height;

        //allocating size needed to store pixel values in 3x3 matrix "pdata"
        img.pdata = (uint8***)malloc(height*sizeof(uint8**));
        for (int i = 0; i < height; i++)
        {
            img.pdata[i]=(uint8**)malloc(width*sizeof(uint8*));
            for (int j = 0; j < width; j++)
            {
                img.pdata[i][j]=(uint8 *)malloc(3*sizeof(uint8));
            }
        }
        
        //this makes sure that the values of pixel is 0 initially
        for (int i = 0; i < height; i++)
        {
            for (int j = 0; j < width; j++)
            {
                memset(img.pdata[i][j],0,3);
            }  
        }
    }
    //returning the created obj
    return img;
}

//this function is used for setting the values of the img structure as 0 
void resetImage(image *img)
{
    for (int i = 0; i < img->height; i++)
    {
        for (int j = 0; j < img->width; j++)
        {
            img->pdata[i][j]=0x0;
        }
        img->pdata[i]=0x0;
    }
    img->pdata=0x0;
    img->height = 0;
    img->width = 0;
}

//this function additionally frees the memory allocated for storing pixel values pointed by the triple pointer pdata
void releaseImage(image *img)
{
    if (img->pdata != 0x0)
    {
        for (int i = 0; i < img->height; i++)
        {
            for (int j = 0; j < img->width; j++)
            {
                free(img->pdata[i][j]);
            }
            free(img->pdata[i]);
        }
        free(img->pdata);
    }
    resetImage(img);
}

uint32 read_bmp_to_struct(char *filename, image *img)
{
    //To make sure the img is fresly new or to delete any values stored in img
    releaseImage(img);

    bmpid bmp_id;
    header _header;
    info _info;

    FILE *stream = fopen(filename, "rb");

    if (stream == NULL)
        return FAILURE;

    if (fread(bmp_id.id, sizeof(uint16), 1, stream) != 1)
    {
        fclose(stream);
        return FAILURE;
    }

    //checking if the id of the file is indeed 'BM'
    if (*((uint16 *)bmp_id.id) != 0x4D42)
    {
        fclose(stream);
        return FAILURE;
    }

    if (fread(&_header, sizeof(header), 1, stream) != 1)
    {
        fclose(stream);
        return FAILURE;
    }

    if (fread(&_info, sizeof(info), 1, stream) != 1)
    {
        fclose(stream);
        return FAILURE;
    }

    if (_info.compr_type != 0)
        fprintf(stderr, "Warning!!! Compression type is not supported");

    if (fseek(stream, _header.offset, SEEK_SET))
    {
        fclose(stream);
        return FAILURE;
    }

    *img = createImage(_info.width, _info.height);
    uint8 temp[img->height][img->width][3];

    // fread(temp,1 , _info.px_size, stream);

    int bpr=(_info.bpp*_info.width)/8;

    for (int i = 0; i < _info.height; i++)
    {
        fread(temp[i],1,bpr,stream);
    }
    
    for (int i = 0; i < img->height; i++)
    {
        for (int j = 0; j < img->width; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                img->pdata[img->height-1-i][j][3-1-k]=temp[i][j][k];
                // img->pdata[i][j][3-1-k]=temp[i][j][k];
            }
            
        }
        
    }

    fclose(stream);
    return SUCCESS;
}


void save_4bytes(uint32 val, FILE *stream)
{
    fprintf(stream, "%c", (val & 0x000000FF) >> 0 * 8);
    fprintf(stream, "%c", (val & 0x0000FF00) >> 1 * 8);
    fprintf(stream, "%c", (val & 0x00FF0000) >> 2 * 8);
    fprintf(stream, "%c", (val & 0xFF000000) >> 3 * 8);
}


uint32 save_img_to_file(const char *filename, image *img)
{
    int r, g, b;
    // int padding = 4 - ((img->width * 3) % 4);
    // if (padding == 4)
    //     padding = 0;
    // int padded_size = ((img->width * 3) + padding) * img->height;

    uint32 padded_size=img->width*3 * img->height;

    FILE *stream = fopen(filename, "wb");
    fprintf(stream, "BM");
    save_4bytes(padded_size + 54, stream);
    save_4bytes(0, stream);
    save_4bytes(54, stream);
    save_4bytes(40, stream);
    save_4bytes(img->width, stream);
    save_4bytes(img->height, stream);

    fprintf(stream, "%c", 1);
    fprintf(stream, "%c", 0);

    fprintf(stream, "%c", 24);
    fprintf(stream, "%c", 0);

    save_4bytes(0, stream);
    save_4bytes(padded_size, stream);
    save_4bytes(0, stream);
    save_4bytes(0, stream);
    save_4bytes(0, stream);
    save_4bytes(0, stream);

    for (int i = 0; i < img->height; i++)
    {
        for (int j = 0; j < img->width; j++)
        {
            for (int k = 2; k >= 0; k--)
            {
                fputc(img->pdata[img->height-1-i][j][k],stream);
            }     
        }     
    }
    fclose(stream);
    return SUCCESS;
}

//simply copies values of in to out
void copyimage(image* in,image* out){
    *out=createImage(in->width,in->height);
    for (int i = 0; i < in->height; i++)
    {
        for (int j = 0; j < in->width; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                out->pdata[i][j][k]=in->pdata[i][j][k];
            }
            
        }
        
    } 
}

//this function generates the values of kernel matrix by some mathematical calculations
void generate_kernel(double k[][11], int N, double sigma) {
    // array for gauss value
    double gaussian_x[11][11];
    double gaussian_y[11][11];
    double sum = 0;
    
    // initializes to N/2z
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            gaussian_x[j][i] = i - (N/2);
            gaussian_y[j][i] = j - (N/2);
        }
    }
    
    // adds gaussian blur
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double x = (pow(gaussian_x[j][i],2) / (2 * pow(sigma, 2)));
            double y = (pow(gaussian_y[j][i],2) / (2 * pow(sigma, 2)));
            k[j][i] = exp(-1 * (x + y));
            sum += k[j][i];
        }
    }
    
    // normalize kernel values
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            k[j][i] /= sum;
        }
    }
    /*
        Some simple examples of gaussian kernel matrices: 
        double kernel[3][3]={
        {1,2,1},
        {2,4,2},
        {1,2,1}
        };

        double kernel[5][5]={
        {1,  4,  7,  4,  1},
        {4, 16, 26, 16,  4},
        {7, 26, 41, 26,  7},
        {4, 16, 26, 16,  4},
        {1,  4,  7,  4,  1}
        };

        double kernel[7][7]={
        {  0,  0,  1,  2,  1,  0,  0},
        {  0,  3, 13, 22, 13,  3,  0},
        {  1, 13, 59, 97, 59, 13,  1},
        {  2, 22, 97,159, 97, 22,  2},
        {  1, 13, 59, 97, 59, 13,  1},
        {  0,  3, 13, 22, 13,  3,  0},
        {  0,  0,  1,  2,  1,  0,  0}
        };
    */
}

void convolve(image* input,image* output,int num,double kernel[][11]){
    uint32 ht=input->height;
    uint32 wt =input->width;

    uint32 padding =2*(num/2);
    uint32 padded[ht+padding][wt+padding][3];
    uint32 temp[ht][wt][3];

    //initializes the values of padded matrix to be 0
    for (int i = 0; i < ht+padding; i++)
    {
        for (int j = 0; j < wt+padding; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                padded[i][j][k]=0;
            }   
        }
    }

    
    //copies the value of pixel values of input image to padded matrix in appropriate location
    for (int i = 0; i < ht; i++)
    {
        for (int j = 0; j < wt; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                padded[padding/2+i][padding/2+j][k]=input->pdata[i][j][k];
            }  
        }
    }
    
    //initializes the value of temp matrix to 0
    for (int i = 0; i < ht; i++)
    {
        for (int j = 0; j < wt; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                temp[i][j][k]=0;
            }   
        }  
    }


    //algorithm for convolving the matrix padded with kernel matrix
    for (int i = padding/2; i < padding/2+ht; i++)
    {
        for (int j = padding/2; j < padding/2+wt; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                for (int m = 0; m < num; m++)
                {
                    for (int n = 0; n < num; n++)
                    {
                        temp[i-padding/2][j-padding/2][k] += padded[i+m-padding/2][j+n-padding/2][k]* kernel[m][n];
                    }     
                }    
            }  
        }
    }
    
    
    //copies the contents to temp to pdata of output image 
    for (int i = 0; i < ht; i++)
    {
        for (int j = 0; j < wt; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                if (temp[i][j][k]>255)
                    temp[i][j][k]=255;
                else if (temp[i][j][k]<0)
                    temp[i][j][k]=0;
                output->pdata[i][j][k]=(uint8)temp[i][j][k];
            }
            
        } 
    }
    
}

void gaussian_blur(image* input,image* output,int num,double sigma){
    //declaration of kernel matrix of size 11x11
    double kernel[11][11];
    //initialization of the values to 0
    for (int i = 0; i < 11; i++)
    {
        for (int j = 0; j < 11; j++)
        {
            kernel[i][j]=0;
        }
        
    }
    //generate values and stores them in kernel matrix 
    generate_kernel(kernel,num,sigma);

    //convolves the input with kernel and save it to output
    convolve(input,output,num,kernel);
}

int main()
{
    //declaring two objects file1 for input image and file 2 for outpur image
    image file1,file2;

    //creating input and outpur pointers for ease 
    image *input = &file1;
    image *output=&file2;
    
    //reads the file and stores the data in input 
    //Prototype:
    //read_bmp_to_struct("path_to_your_input_bmp_file", address of the object(or simply pointer to that obj));
    read_bmp_to_struct("sample1.bmp", input);

    //copies input and stores in output
    copyimage(input,output);

    //apply gaussian blur to input and save in output
    // Prototype:
    // gaussian_blur(input,output,size of kernel matrix(nxn),sigma(higher the value more the blurring effect));
    gaussian_blur(input,output,7,1.5);

    //save the data stored in output to a file
    //Prototype:
    // save_img_to_file("Output_file_path", address of the object(or simply pointer to that obj))
    save_img_to_file("output.bmp", output);

    return 0;
}
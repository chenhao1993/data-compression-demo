/*
 * LZ77压缩算法测试代码，完成文件的压缩并保存，然后解压缩恢复新的文件，
 * 对比原始文件和压缩解压后的文件的差异，计算压缩比。
 */

#include <stdio.h>
#include <malloc.h>
#include <mem.h>

#define SEARCH_BUFFER_SIZE 100
#define LOOKAHEAD_BUFFER_SIZE 50
#define BUFFER_SIZE  (SEARCH_BUFFER_SIZE+LOOKAHEAD_BUFFER_SIZE)


int compressor(FILE *ifp, FILE *ofp);
int decompressor(FILE *ifp, FILE *ofp);

int main() {
    printf("Hello, World!\n");
    printf("LZ77 compressor demo test by lab601_ch.\n");

    FILE *ifp=NULL, *ofp=NULL, *dofp=NULL;
    ifp = fopen("../21.data", "rb");
    if(ifp == NULL){
        printf("ERROR, cannot open file!!!");
        return 1;
    }
    ofp = fopen("../21.2.compressed.data", "wb");
    if(ofp == NULL){
        printf("ERROR, cannot open compressed file!!!");
        return 1;
    }
    compressor(ifp, ofp);
    fclose(ifp);
    fclose(ofp);

    /*解压缩*/
    ofp = fopen("../21.2.compressed.data", "rb");
    if(ofp == NULL){
        printf("ERROR, cannot open compressed file!!!");
        return 1;
    }
    dofp = fopen("../21.2.decompressed.data", "wb");
    if(dofp == NULL){
        printf("ERROR, cannot open decompressed output file!!!");
        return 1;
    }
    printf("------open file is ok!\n");
    decompressor(ofp, dofp);
    fclose(ofp);
    fclose(dofp);

    return 0;
}

/*
 * 查找搜索缓冲区指定区域和先行缓冲区匹配长度
 * input: 缓冲区制定区域首尾地址
 * output: 匹配长度
 */
static int findMatchLength(const char* buffer, char *searchStart, char *searchEnd, char *lookaheadStart, char *lookaheadEnd)
{
    /*搜索缓冲区或先行缓冲区长度为0，返回-1*/
    if(searchStart == searchEnd){
        printf("searchStart == searchEnd\n");
        return -1;
    }
    if(lookaheadStart == lookaheadEnd) {
        printf("lookaheadStart == lookaheadEnd\n");
        return -1;
    }

    int length = 0;

    /*待匹配数据位置指针*/
    char *searchdata = searchStart;
    char *lookdata = lookaheadStart;
    while(1){
        /*数据匹配,先行缓冲区仍有数据*/
        if(*searchdata == *lookdata && lookdata!=lookaheadEnd){
            /*循环队列，边界溢出,循环回到首地址*/
            if(searchdata-buffer == BUFFER_SIZE)
                searchdata = buffer;
            else
                searchdata++;

            if(lookdata-buffer == BUFFER_SIZE)
                lookdata = buffer;
            else
                lookdata++;

            /*匹配长度计数*/
            length++;
        }else
            break;
    }

    return length;
}

/*
 * 查找缓冲区最大匹配字符串
 * input: 缓冲区首尾地址
 * output: 匹配长度，偏移
 */
static int findMaxMatchLength(const char* buffer, char *searchStart, char *searchEnd, char *lookaheadStart, char *lookaheadEnd, int *offset)
{
    int length = 0;
    *offset=0;
    int searchSize = 0;
    if(searchStart <= searchEnd)
        searchSize = searchEnd - searchStart;
    else
        searchSize = searchEnd - searchStart + BUFFER_SIZE + 1;

    /*搜索缓冲区空， 返回-1，出错*/
    if(searchSize == 0)
        return -1;

    for(int i=0; i<searchSize; i++){
        int len=0;
        if((searchStart+i) > (buffer+BUFFER_SIZE))
            len = findMatchLength(buffer, searchStart+i-(BUFFER_SIZE+1), searchEnd, lookaheadStart, lookaheadEnd);
        else
            len = findMatchLength(buffer, searchStart+i, searchEnd, lookaheadStart, lookaheadEnd);

        if(len == -1) {
            printf("len is -1, continue!\n");
            continue;
        }
        if(length < len){
            length = len;
            *offset = searchSize-i;
        }
    }
    return length;
}


int compressor(FILE *ifp, FILE *ofp)
{
    /*100字节为搜索缓冲区，50字节为先行缓冲区，使用循环队列的方法模拟滑动窗口（Slide Window）*/
    const char *buffer = malloc(BUFFER_SIZE+1);

    /*初始化滑动窗口*/
    memset(buffer, 0, BUFFER_SIZE+1);

    /*搜索缓冲区，先行缓冲区，头尾地址指针*/
    char *searchBufferStart = buffer;
    char *searchBufferEnd = buffer;
    char *lookaheadBufferStart = buffer;
    char *lookaheadBufferEnd = buffer;

    /*先读取数据，将先行缓冲区填满*/
    int c=0;
    for(int i=0; i<LOOKAHEAD_BUFFER_SIZE; i++){
        c=getc(ifp);
        if(c == EOF) {
            printf("WARNING! data which will be compressed is too small.\n");
            return -1;
        }
        *lookaheadBufferEnd = (char)c;
        lookaheadBufferEnd++;
    }
    printf("------lookahead buffer data pre load is ok.\n");

    /*搜索缓冲区一开始为空，对先行缓冲区第一个字符编码*/
    putc(0, ofp);
    putc(0, ofp);
    putc(*lookaheadBufferStart, ofp);

    /*从源文件读取一个字符，准备跟新滑动窗口*/
    c = getc(ifp);
    /*如果到文件末尾，break，不再读取文件，将先行缓冲区压缩完就结束*/
    if (c == EOF)
        goto final;
    /*跟新搜索缓冲区指针，尾指针向后移动一个字节*/
    searchBufferEnd++;
    /*更新先行缓冲区指针，先行缓冲区头指针和搜索缓冲区尾指针指向同一地址
        往先行缓冲区尾部放入新元素*/
    lookaheadBufferStart = searchBufferEnd;
    *lookaheadBufferEnd = (char) c;
    lookaheadBufferEnd++;
    printf("------searchBuffer size is 0, direct output.\n");

    /*查找最长匹配字符串，更新数据*/
    while(1)
    {
        /*获取搜索缓冲区长度*/
        /*不同搜索缓冲区长度，操作不同，一开始，搜索缓冲区未满阶段，和搜索缓冲区已满阶段操作不同。*/
        int size=0;
        if(searchBufferEnd >= searchBufferStart)
            size = searchBufferEnd - searchBufferStart;
        else
            size = searchBufferEnd + (BUFFER_SIZE+1) - searchBufferStart;

        if(size == 0) {
            free(buffer);
            printf("ERROR, search buffer size is 0!!!");
            return -1;
        }

        /*搜索缓冲区未满*/
        if(SEARCH_BUFFER_SIZE > size) {
            int offset=0;
            int matchSize = findMaxMatchLength(buffer, searchBufferStart, searchBufferEnd, lookaheadBufferStart, lookaheadBufferEnd, &offset);

            /*no match data*/
            if(0 == matchSize) {
                putc(0, ofp);
                putc(0, ofp);
                putc(*lookaheadBufferStart, ofp);

                /*跟新搜索缓冲区指针，尾指针向后移动一个字节*/
                searchBufferEnd++;
                /*更新先行缓冲区指针，先行缓冲区头指针和搜索缓冲区尾指针指向同一地址*/
                lookaheadBufferStart = searchBufferEnd;

                /*从源文件读取一个字符，准备跟新滑动窗口*/
                c = getc(ifp);
                /*如果到文件末尾，break，不再读取文件，将先行缓冲区压缩完就结束*/
                if (c == EOF)
                    goto final;

                /*往先行缓冲区尾部放入新元素*/
                *lookaheadBufferEnd = (char) c;
                lookaheadBufferEnd++;
                printf("------match size is 0, direct output.\n");
            }else{
                /*需要考虑搜索缓冲区头指针*/
                if(lookaheadBufferEnd+matchSize > buffer+BUFFER_SIZE){
                    /*have match data.*/
                    /*update buffer point, careful about point over*/
                    /*look-ahead buffer point*/
                    lookaheadBufferStart = lookaheadBufferStart + matchSize;

                    /*encoder output*/
                    putc(offset, ofp);
                    putc(matchSize, ofp);
                    putc(*(lookaheadBufferStart), ofp);

                    searchBufferEnd = lookaheadBufferStart;

                    for (int i = 0; i < matchSize; i++) {
                        c = getc(ifp);
                        if (c == EOF) {
                            /*输入结束，只需将buffer内的压缩完即可*/
                            printf("------read all data, goto final!\n");
                            goto final;
                        }
                        *lookaheadBufferEnd = (char) c;
                        if (lookaheadBufferEnd - buffer >= BUFFER_SIZE)
                            lookaheadBufferEnd = buffer;
                        else
                            lookaheadBufferEnd++;
                    }
                    /*update search buffer start point*/
                    searchBufferStart = lookaheadBufferEnd+1;
                }else{
                /*不需要考虑搜索缓冲区头指针*/
                    /*have match data.*/
                    /*update buffer point, careful about point over*/
                    /*look-ahead buffer point*/
                    lookaheadBufferStart = lookaheadBufferStart + matchSize;

                    /*encoder output*/
                    putc(offset, ofp);
                    putc(matchSize, ofp);
                    putc(*(lookaheadBufferStart), ofp);

                    searchBufferEnd = lookaheadBufferStart;

                    for (int i = 0; i < matchSize; i++) {
                            c = getc(ifp);
                            if (c == EOF) {
                                /*输入结束，只需将buffer内的压缩完即可*/
                                printf("------read all data, goto final!\n");
                                goto final;
                            }
                            *lookaheadBufferEnd = (char) c;
                            lookaheadBufferEnd++;
                    }
                }
            }
        }else{
            /*搜索缓冲区已满*/
            /*get length, offset*/
            int offset= 0;
            int matchSize = findMaxMatchLength(buffer, searchBufferStart, searchBufferEnd, lookaheadBufferStart, lookaheadBufferEnd, &offset);

            /*存在匹配和不存在匹配数据，操作不一样*/
            /*No match data.*/
            if(0 == matchSize){
                /*encoder output*/
                /*No match data, LZ77 token has three parts, offset, length, and with the unmatched symbol*/
                putc(0, ofp);
                putc(0, ofp);
                putc(*(lookaheadBufferStart), ofp);
                //printf("------output is %d.\n", *(lookaheadBufferStart));

                /*update buffer point, careful about point over*/
                if (searchBufferStart - buffer >= BUFFER_SIZE)
                    searchBufferStart = buffer;
                else
                    searchBufferStart = searchBufferStart + 1;

                /*look-ahead buffer point*/
                if (lookaheadBufferStart - buffer >= BUFFER_SIZE)
                    lookaheadBufferStart = buffer;
                else
                    lookaheadBufferStart = lookaheadBufferStart + 1;

                searchBufferEnd = lookaheadBufferStart;

                c = getc(ifp);
                if (c == EOF) {
                    /*输入结束，只需将buffer内的压缩完即可*/
                    printf("------0.read all data, goto final!\n");
                    goto final;
                }
                *lookaheadBufferEnd = (char)c;
                if(lookaheadBufferEnd - buffer >= BUFFER_SIZE)
                    lookaheadBufferEnd = buffer;
                else
                    lookaheadBufferEnd++;

            }else{
                /*have match data.*/
                /*update buffer point, careful about point over*/
                if (searchBufferStart + matchSize  > buffer + BUFFER_SIZE)
                    searchBufferStart = searchBufferStart + matchSize - BUFFER_SIZE - 1;
                else
                    searchBufferStart = searchBufferStart + matchSize;

                /*look-ahead buffer point*/
                if (lookaheadBufferStart + matchSize > buffer + BUFFER_SIZE)
                    lookaheadBufferStart = lookaheadBufferStart + matchSize - BUFFER_SIZE - 1;
                else
                    lookaheadBufferStart = lookaheadBufferStart + matchSize;

                /*encoder output*/
                putc(offset, ofp);
                putc(matchSize, ofp);
                putc(*(lookaheadBufferStart), ofp);

                searchBufferEnd = lookaheadBufferStart;

                for (int i = 0; i < matchSize; i++) {
                    c = getc(ifp);
                    if (c == EOF) {
                        /*输入结束，只需将buffer内的压缩完即可*/
                        printf("------read all data, goto final!\n");
                        goto final;
                    }
                    *lookaheadBufferEnd = (char) c;
                    if (lookaheadBufferEnd - buffer >= BUFFER_SIZE)
                        lookaheadBufferEnd = buffer;
                    else
                        lookaheadBufferEnd++;
                }
            }
            printf("------buffer match size is %d, offset is %d!\n", matchSize, offset);
            for(int i=0; i<= BUFFER_SIZE; i++){
                printf("%d  ", buffer[i]);
            }
            printf("\n");
            printf("search start is %d, lookahead start is %d.\n", searchBufferStart-buffer, lookaheadBufferStart-buffer);
        }
        int searchOffset = searchBufferStart - buffer;
        int searchOffset1 = searchBufferEnd - buffer;
        int lookOffset = lookaheadBufferStart - buffer;
        int lookOffset1 = lookaheadBufferEnd - buffer;
        //printf("%d, %d, %d, %d\n", searchOffset, searchOffset1, lookOffset, lookOffset1);
    }

final:
    while(lookaheadBufferStart != lookaheadBufferEnd){
        int searchOffset = searchBufferStart - buffer;
        int searchOffset1 = searchBufferEnd - buffer;
        int lookOffset = lookaheadBufferStart - buffer;
        int lookOffset1 = lookaheadBufferEnd - buffer;
        //printf("%d, %d, %d, %d\n", searchOffset, searchOffset1, lookOffset, lookOffset1);

        /*get length, offset*/
        int offset= 0;
        int matchSize = findMaxMatchLength(buffer, searchBufferStart, searchBufferEnd, lookaheadBufferStart, lookaheadBufferEnd, &offset);

        /*存在匹配和不存在匹配数据，操作不一样*/
        /*No match data.*/
        if(0 == matchSize){
            /*encoder output*/
            /*No match data, LZ77 token has three parts, offset, length, and with the unmatched symbol*/
            putc(0, ofp);
            putc(0, ofp);
            putc(*(lookaheadBufferStart), ofp);
            //printf("------output is %d.\n", *(lookaheadBufferStart));

            /*update buffer point, careful about point over*/
            if (searchBufferStart - buffer >= BUFFER_SIZE)
                searchBufferStart = buffer;
            else
                searchBufferStart = searchBufferStart + 1;

            /*look-ahead buffer point*/
            if (lookaheadBufferStart - buffer >= BUFFER_SIZE)
                lookaheadBufferStart = buffer;
            else
                lookaheadBufferStart = lookaheadBufferStart + 1;

            searchBufferEnd = lookaheadBufferStart;

        }else{
            /*have match data.*/
            /*update buffer point, careful about point over*/
            if (searchBufferStart + matchSize  > buffer + BUFFER_SIZE)
                searchBufferStart = searchBufferStart + matchSize - BUFFER_SIZE - 1;
            else
                searchBufferStart = searchBufferStart + matchSize;

            /*look-ahead buffer point*/
            if (lookaheadBufferStart + matchSize > buffer + BUFFER_SIZE)
                lookaheadBufferStart = lookaheadBufferStart + matchSize - BUFFER_SIZE - 1;
            else
                lookaheadBufferStart = lookaheadBufferStart + matchSize;

            /*encoder output*/
            putc(offset, ofp);
            putc(matchSize, ofp);
            putc(*(lookaheadBufferStart), ofp);

            searchBufferEnd = lookaheadBufferStart;
        }
        printf("------last buffer match size is %d!\n", matchSize);
    }

    printf("------compress is end!\n");
    free(buffer);
    return 0;
}

int decompressor(FILE *ifp, FILE *ofp)
{
    char *buffer = malloc(SEARCH_BUFFER_SIZE+1);
    memset(buffer, 0, SEARCH_BUFFER_SIZE+1);
    char *SearchStart = buffer;
    char *SearchEnd = buffer;
    char *matchStart = NULL;
    int length = 0;
    int offset = 0;
    int symbol = 0;

    while(!feof(ifp))
    {
        /*get three parts*/
        offset = getc(ifp);
        length = getc(ifp);
        symbol = getc(ifp);

        if(length == 0) {
            putc(symbol, ofp);
            /*update search buffer*/
            *SearchEnd = (char)symbol;
            if(SearchEnd == buffer+SEARCH_BUFFER_SIZE)
                SearchEnd = buffer;
            else
                SearchEnd++;

            if(SearchEnd == SearchStart){
                if(SearchStart == buffer + SEARCH_BUFFER_SIZE)
                    SearchStart = buffer;
                else
                    SearchStart++;
            }

        }
        else{
            if(SearchEnd-buffer < offset)
                matchStart = SearchEnd - offset + SEARCH_BUFFER_SIZE + 1;
            else
                matchStart = SearchEnd - offset;
            for(int i=0; i<length; i++) {
                char c = *matchStart;
                putc((int)c, ofp);
                /*update search buffer*/
                *SearchEnd = c;
                if(SearchEnd == buffer+SEARCH_BUFFER_SIZE)
                    SearchEnd = buffer;
                else
                    SearchEnd++;

                if(SearchEnd == SearchStart){
                    if(SearchStart == buffer + SEARCH_BUFFER_SIZE)
                        SearchStart = buffer;
                    else
                        SearchStart++;
                }

                if(buffer+SEARCH_BUFFER_SIZE == matchStart)
                    matchStart = buffer;
                else
                    matchStart++;
            }
        }
        printf("start is %d, end is %d\n", SearchStart-buffer, SearchEnd-buffer);
    }
    free(buffer);
    return 0;
}

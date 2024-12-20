#include <windows.h>
#include <tchar.h>
#include <commdlg.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifndef UNICODE
#define UNICODE
#endif 

#ifndef _UNICODE
#define _UNICODE
#endif 

#ifndef M_PI
#define M_PI 3.1415926
#endif

#define displaysize 3000

#define threshold 15

#define INTERVAL 150

#define fftsize 4096
#define fftscale 0.01

//复数结构体
struct complex {
    float Re;
    float Im;
};

// 数据结构，用于存储数据
struct stdata{
    float* data;
    size_t size;
};

// 全局变量
stdata dpdata={NULL,0};
stdata rdata={NULL,0};
stdata fdata={NULL,0};
stdata spectrum={NULL,0};
UINT32* peakindex=NULL;

// 函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int LoadData(HWND hwnd);
int filter();
complex mtp(complex cpx1, complex cpx2);
complex* fft(stdata data);
int findpeaks();
LRESULT CALLBACK WndProcplot(HWND hwnd, UINT msg, WPARAM wp, LPARAM lParam);
float myabs(float a);





int LoadData(HWND hwnd) {
    OPENFILENAME ofn;
    TCHAR szFile[MAX_PATH];
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = TEXT("C:\\");
    ofn.lpstrTitle = _T("选择文件");


    // 打开文件对话框并选择文件
    if (GetOpenFileName(&ofn)) {
        FILE* file = _tfopen(szFile, _T("rb"));  // 以二进制模式打开文件
        if (!file) {
            MessageBox(hwnd, _T("文件打开失败"), _T("错误"), MB_OK);
            return 0;
        }

        // 获取文件大小
        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // 分配内存来存储数据
        rdata.size = fileSize / 3;  // 每3个字节表示2个数据，只要1通道数据
        rdata.data = (float*)malloc(rdata.size * sizeof(float));

        // 读取并解析数据
        UINT16 temp;
        UINT8 bytes[3];
        for (size_t i = 0; i < rdata.size; i++) {
            temp=0;
            fread(bytes, 1, 3, file);  // 读取3个字节
            // 提取第一个12位数据
            temp =(bytes[1]&0x0f)<<8;
            temp+=bytes[0]; // 提取第一个12位数据
            // 存储解析后的数据
            *(rdata.data+i)=temp;
        }
        fclose(file);
        return 1;
    }
    return 0;
}


int filter(){
    //原始数据错误
    if((dpdata.data==NULL)|(dpdata.size==0)) return 0;

    //清除之前数据释放内存
    if(fdata.data!=NULL){
        free(fdata.data);
        fdata.size=0;
    }

    //带通滤波器
    float fir1[]={
        -0.0178302464290,-0.0357681107958,-0.0270385459092,
        0.0023714082227,0.0019963289861,-0.0200787443051,
        -0.0089486442166,0.0020021497940,-0.0184951633356,
        -0.0116312042181,0.0014156420457,-0.0204322413725,
        -0.0119501661153,0.0015776599677,-0.0244843426218,
        -0.0107432128641,0.0022824888965,-0.0303516058403,
        -0.0075784194400,0.0033016532042,-0.0385716483599,
        -0.0013847853367,0.0043165761783,-0.0510271183705,
        0.0110406393699,0.0051947897436,-0.0748106587289,
        0.0418626218520,0.0057724255983,-0.1603971819588,
        0.2505802876077,0.6726311571660,0.2505802876077,
        -0.1603971819588,0.0057724255983,0.0418626218520,
        -0.0748106587289,0.0051947897436,0.0110406393699,
        -0.0510271183705,0.0043165761783,-0.0013847853367,
        -0.0385716483599,0.0033016532042,-0.0075784194400,
        -0.0303516058403,0.0022824888965,-0.0107432128641,
        -0.0244843426218,0.0015776599677,-0.0119501661153,
        -0.0204322413725,0.0014156420457,-0.0116312042181,
        -0.0184951633356,0.0020021497940,-0.0089486442166,
        -0.0200787443051,0.0019963289861,0.0023714082227,
        -0.0270385459092,-0.0357681107958,-0.0178302464290
    };

    // 计算带通滤波器长度
    UINT16 firlen = sizeof(fir1) / sizeof(fir1[0]);
    fdata.size = rdata.size + firlen - 1;
    fdata.data = (float*)malloc(fdata.size * sizeof(float));
    //内存错误
    if(fdata.data==NULL) return 0;


    // 60Hz陷波器设计 (采样频率360Hz)
    float fs = 360.0f;     // 采样频率
    float f0_60Hz = 60.0f; // 60Hz陷波频率
    float f0_120Hz = 120.0f; // 120Hz陷波频率
    float Q = 30.0f;       // 品质因数（Q值）

    // 计算60Hz陷波器系数
    float w0_60Hz = 2 * M_PI * f0_60Hz / fs;  // 60Hz中心频率
    float alpha_60Hz = sin(w0_60Hz) / (2 * Q); // 60Hz带宽系数

    float notch_b0_60Hz = 1.0f;
    float notch_b1_60Hz = -2 * cos(w0_60Hz);
    float notch_b2_60Hz = 1.0f;
    float notch_a0_60Hz = 1 + alpha_60Hz;
    float notch_a1_60Hz = -2 * cos(w0_60Hz);
    float notch_a2_60Hz = 1 - alpha_60Hz;

    // 计算120Hz陷波器系数
    float w0_120Hz = 2 * M_PI * f0_120Hz / fs;  // 120Hz中心频率
    float alpha_120Hz = sin(w0_120Hz) / (2 * Q); // 120Hz带宽系数

    float notch_b0_120Hz = 1.0f;
    float notch_b1_120Hz = -2 * cos(w0_120Hz);
    float notch_b2_120Hz = 1.0f;
    float notch_a0_120Hz = 1 + alpha_120Hz;
    float notch_a1_120Hz = -2 * cos(w0_120Hz);
    float notch_a2_120Hz = 1 - alpha_120Hz;

    // 陷波器系数归一化
    notch_b0_60Hz /= notch_a0_60Hz;
    notch_b1_60Hz /= notch_a0_60Hz;
    notch_b2_60Hz /= notch_a0_60Hz;
    notch_a1_60Hz /= notch_a0_60Hz;
    notch_a2_60Hz /= notch_a0_60Hz;

    notch_b0_120Hz /= notch_a0_120Hz;
    notch_b1_120Hz /= notch_a0_120Hz;
    notch_b2_120Hz /= notch_a0_120Hz;
    notch_a1_120Hz /= notch_a0_120Hz;
    notch_a2_120Hz /= notch_a0_120Hz;

    float temp;
    int i=0,j=0;

    //带通滤波器处理
    for (i = 0; i < firlen - 1; i++) {
        temp = 0.0f;
        for (j = 0; j <= i; j++) {
            temp += *(rdata.data+i-j)* fir1[j];
        }
        *(fdata.data+i) = temp;
    }

    for (; i < rdata.size; i++) {
        temp = 0.0f;
        for (j = 0; j < firlen; j++) {
            temp += *(rdata.data+i-j) * fir1[j];
        }
        *(fdata.data+i) = temp;
    }

    for (; i < fdata.size; i++) {
        temp = 0.0f;
        for (j = i-rdata.size+1; j < firlen; j++) {
            temp += (rdata.data)[i - j] * fir1[j];
        }
        (fdata.data)[i] = temp;
    }

    // 60Hz陷波器处理
    float prev_out1_60Hz = 0.0f, prev_out2_60Hz = 0.0f, prev_in1_60Hz = 0.0f, prev_in2_60Hz = 0.0f;

    // 120Hz陷波器处理
    float prev_out1_120Hz = 0.0f, prev_out2_120Hz = 0.0f, prev_in1_120Hz = 0.0f, prev_in2_120Hz = 0.0f;

    for (i = 0; i < rdata.size; i++) {
        temp = *(fdata.data + i);
        
        // 60Hz陷波器处理
        temp = notch_b0_60Hz * temp + notch_b1_60Hz * prev_in1_60Hz + notch_b2_60Hz * prev_in2_60Hz
               - notch_a1_60Hz * prev_out1_60Hz - notch_a2_60Hz * prev_out2_60Hz;

        // 更新60Hz陷波器的前一个输入和输出值
        prev_in2_60Hz = prev_in1_60Hz;
        prev_in1_60Hz = temp;
        prev_out2_60Hz = prev_out1_60Hz;
        prev_out1_60Hz = temp;

        // 120Hz陷波器处理
        temp = notch_b0_120Hz * temp + notch_b1_120Hz * prev_in1_120Hz + notch_b2_120Hz * prev_in2_120Hz
               - notch_a1_120Hz * prev_out1_120Hz - notch_a2_120Hz * prev_out2_120Hz;

        // 更新120Hz陷波器的前一个输入和输出值
        prev_in2_120Hz = prev_in1_120Hz;
        prev_in1_120Hz = temp;
        prev_out2_120Hz = prev_out1_120Hz;
        prev_out1_120Hz = temp;

        *(fdata.data + i) = temp;
    }
    return 1;
}


int findpeaks(){
    // 释放内存
    if(peakindex!=NULL){
        free(peakindex);
        peakindex=NULL;
    }
    if((fdata.data==NULL)|(fdata.size==0)|(dpdata.size==rdata.size)){
        MessageBox(NULL, TEXT("建议先完成滤波操作"), TEXT("提示"), MB_OK);
        return 1;
    }

    float* temp=(float*)malloc(fdata.size*sizeof(float));

    for (size_t i =1; i<fdata.size; i++) {
        temp[i] = fdata.data[i] - fdata.data[i-1];  // 简单的前向差分计算导数
    }
    temp[0] = 0;  // 为了避免访问负索引，设置第一个导数为0

    peakindex=(UINT32*)malloc(sizeof(UINT32));
    peakindex[0]=0;//0处存放R波峰值数量 心跳次数
    UINT32 beattimes=0;

    for (size_t i = 1; i < fdata.size-1; i++)
    {
        if((temp[i]>temp[i-1])&(temp[i]>temp[i+1])&(temp[i]>threshold)&(i-peakindex[beattimes])>INTERVAL){
            peakindex[0]++;
            beattimes=peakindex[0];
            UINT32* tptr=(UINT32*)realloc(peakindex,(beattimes+1)*sizeof(UINT32));
            if(tptr==NULL){
                MessageBox(NULL,TEXT("error"),TEXT("error"),MB_OK);
                return 1;
            }
            peakindex=tptr;
            peakindex[beattimes]=i;
            // printf("peak found! %.3f,%.3f,%.3f,%d,%d\n",temp[i-1],temp[i],temp[i+1],i,peakindex[beattimes-1]);
        }
    }
    free(temp);
    return 0;
}

// FFT函数实现
complex* fft(stdata data) {
    complex* spectrum = (complex*)malloc(fftsize * sizeof(complex));
    int i, j, k;

    if (data.size == 2) { // 基本情形
        spectrum[0].Re = *(data.data) + *(data.data + 1);
        spectrum[0].Im = 0;
        spectrum[1].Re = *(data.data) - *(data.data + 1);
        spectrum[1].Im = 0;
        return spectrum;
    } 
    else { // 递归拆分
        stdata data1;
        stdata data2;
        data1.data = (float*)malloc((data.size / 2) * sizeof(float));
        data1.size = data.size/2;
        data2.data = (float*)malloc((data.size / 2) * sizeof(float));
        data2.size = data.size/2;

        // 分割数据到data1和data2
        for (i = 0; i < (data.size / 2); i++) {
            data1.data[i] = data.data[2 * i];
            data2.data[i] = data.data[2 * i + 1];
        }

        // 递归调用FFT
        complex* spectrum1 = fft(data1);
        complex* spectrum2 = fft(data2);

        complex wnk;
        for (k = 0; k < data.size / 2; k++) {
            wnk.Re = cos(2 * M_PI * k / data.size);
            wnk.Im = -sin(2 * M_PI * k / data.size);

            spectrum[k].Re = spectrum1[k].Re + mtp(wnk, spectrum2[k]).Re;
            spectrum[k].Im = spectrum1[k].Im + mtp(wnk, spectrum2[k]).Im;

            spectrum[k + data.size / 2].Re = spectrum1[k].Re - mtp(wnk, spectrum2[k]).Re;
            spectrum[k + data.size / 2].Im = spectrum1[k].Im - mtp(wnk, spectrum2[k]).Im;
        }

        // 释放中间分配的内存
        free(data1.data);
        free(data2.data);
        free(spectrum1);
        free(spectrum2);

        return spectrum;
    }

}






//主窗口函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND openfileButton,displayrawButton,filterButton,spButton,diagnoseButton,plot;
    RECT drawRect = {180, 20, 800, 350}; 
    UINT8 buttonheight=42,interval =30;
    switch (uMsg)
    {   
    case WM_CREATE:{
        //创建按钮
        openfileButton = CreateWindow(TEXT("BUTTON"), TEXT("选择ECG文件"),
                               WS_CHILD | WS_VISIBLE,
                               30, drawRect.top, 120, buttonheight, 
                               hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        displayrawButton = CreateWindow(TEXT("BUTTON"), TEXT("显示原始数据"),
                               WS_CHILD | WS_VISIBLE,
                               30, (drawRect.top+interval+buttonheight), 120, buttonheight, 
                               hwnd, (HMENU)1, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        filterButton = CreateWindow(TEXT("BUTTON"), TEXT("滤波"),
                               WS_CHILD | WS_VISIBLE,
                               30, (drawRect.top+2*(interval+buttonheight)), 120, buttonheight, 
                               hwnd, (HMENU)2, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        spButton = CreateWindow(TEXT("BUTTON"), TEXT("FFT"),
                               WS_CHILD | WS_VISIBLE,
                               30, (drawRect.top+3*(interval+buttonheight)), 120, buttonheight, 
                               hwnd, (HMENU)3, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        diagnoseButton = CreateWindow(TEXT("BUTTON"), TEXT("诊断"),
                               WS_CHILD | WS_VISIBLE,
                               30, (drawRect.top+4*(interval+buttonheight)), 120, buttonheight, 
                               hwnd, (HMENU)4, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        plot= CreateWindow(TEXT("plot"), TEXT(""), 
                                WS_CHILD | WS_VISIBLE | WS_BORDER,
                                drawRect.left, drawRect.top, drawRect.right - drawRect.left, drawRect.bottom - drawRect.top,
                                hwnd, (HMENU)5, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        return 0;
    }

    case WM_COMMAND:{
        switch (LOWORD(wParam)){
            case 0:{//加载ECG文件按钮 实现释放内存 读取文件
                // 释放内存
                if (rdata.data != NULL) {
                    free(rdata.data);  // 释放内存
                }
                rdata={NULL,0};
                if(fdata.data !=NULL){
                    free(fdata.data);
                }
                fdata={NULL,0};
                if(spectrum.data !=NULL){
                    free(spectrum.data);
                }
                spectrum={NULL,0};
                if(peakindex !=NULL){
                    free(peakindex);
                }
                peakindex=NULL;

                if (LoadData(hwnd)) {
                    dpdata = rdata;
                    // MessageBox(hwnd, TEXT("数据读取成功"), TEXT("提示"), MB_OK);
                    // 配置滚动条
                    SCROLLINFO si;
                    ZeroMemory(&si,sizeof(SCROLLINFO));
                    si.fMask=SIF_RANGE|SIF_POS|SIF_TRACKPOS|SIF_PAGE;
                    si.nMax=dpdata.size-1;
                    si.nMin=0;
                    si.nPos=0;
                    si.nTrackPos=0;
                    si.nPage=displaysize;
                    SetScrollInfo(plot,SB_HORZ,&si,TRUE);
                    InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
                }
                else {
                    MessageBox(hwnd, TEXT("数据读取失败"), TEXT("提示"), MB_OK);
                    InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
                }
            }
            break;

            case 1:{//显示原始数据 滚动条允许保持之前状态
                if((rdata.data==NULL)|(rdata.size==0)){
                    MessageBox(hwnd, TEXT("load file please"), TEXT("提示"), MB_OK);
                    break;
                }
                dpdata = rdata;
                // 配置滚动条
                SCROLLINFO si;
                ZeroMemory(&si,sizeof(SCROLLINFO));
                si.fMask=SIF_RANGE|SIF_PAGE;
                si.nMax=dpdata.size-1;
                si.nMin=0;
                si.nPage=displaysize;
                SetScrollInfo(plot,SB_HORZ,&si,TRUE);
                InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
            }
            break;

            case 2:{//滤波按钮
                if (filter()){
                    // MessageBox(hwnd, TEXT("数据滤波成功"), TEXT("提示"), MB_OK);
                    dpdata=fdata;
                    // 配置滚动条
                    SCROLLINFO si;
                    ZeroMemory(&si,sizeof(SCROLLINFO));
                    si.fMask=SIF_RANGE|SIF_PAGE;
                    si.nMax=dpdata.size-1;
                    si.nMin=0;
                    si.nPage=displaysize;
                    SetScrollInfo(plot,SB_HORZ,&si,TRUE);
                    InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
                }
                else MessageBox(hwnd, TEXT("数据滤波失败"), TEXT("提示"), MB_OK);
                InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
            }        
            break;

            case 3:{//显示区域fft
                //清除之前数据 释放内存
                if(spectrum.data!=NULL){
                    free(spectrum.data);
                    spectrum.size=0;
                }
                //显示区域数据错误
                if ((dpdata.data==NULL)|(dpdata.size==0)){
                    MessageBox(hwnd, TEXT("数据fft失败"), TEXT("提示"), MB_OK);
                    InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
                    break;
                }
                //重复fft
                if ((dpdata.size/2)<displaysize){
                    MessageBox(hwnd, TEXT("已完成fft处理 请勿重复操作"), TEXT("提示"), MB_OK);
                    InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
                    break;
                }
                
                //读取当前滚动条位置
                SCROLLINFO si;
                ZeroMemory(&si,sizeof(SCROLLINFO));
                si.fMask=SIF_POS;
                GetScrollInfo(plot,SB_HORZ,&si);

                //构建需要fft处理的数据，缺少位补0
                stdata temp;
                temp.data=(float*)malloc(fftsize*sizeof(float));
                if (temp.data==NULL)break;
                temp.size=fftsize;

                for (size_t i = 0; i < displaysize; i++)
                {
                    temp.data[i]=dpdata.data[si.nPos+i];
                }
                for (size_t i = displaysize; i < fftsize; i++)
                {
                    temp.data[i]=(float)0;
                }

                //使用fft
                complex* tempcpx=fft(temp);
                if (tempcpx==NULL)break;

                spectrum.size=temp.size/2;
                spectrum.data=(float*)malloc(spectrum.size*sizeof(float));
                for (size_t i = 0; i < spectrum.size; i++){
                    //求幅值
                    spectrum.data[i]=sqrt((((tempcpx+i)->Im)*((tempcpx+i)->Im))+(((tempcpx+i)->Re)*((tempcpx+i)->Re)));
                }

                dpdata=spectrum;//写入待显示区
                
                //释放内存
                if(temp.data!=NULL){
                    free(temp.data);
                    temp.data=NULL;
                }
                if(tempcpx!=NULL){
                    free(tempcpx);
                    tempcpx=NULL;
                }

                // 配置滚动条
                ZeroMemory(&si,sizeof(SCROLLINFO));
                ShowScrollBar(plot,SB_HORZ,false);

                InvalidateRect(plot, NULL, TRUE);  // 重新绘制窗口
            }
            break;

            case 4:{
                findpeaks();
                InvalidateRect(hwnd, NULL, TRUE);  // 重新绘制窗口
            }
            break;
        }

        return 0;
    }

    
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT rect;
            HDC hdc = BeginPaint(hwnd, &ps);
            // All painting occurs here, between BeginPaint and EndPaint.

            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
            if(peakindex!=NULL){//diagenose
                SCROLLINFO si;
                ZeroMemory(&si,sizeof(SCROLLINFO));
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_POS;
                GetScrollInfo(plot, SB_HORZ, &si);

                UINT32 beattimes=0;
                for (size_t i = 1; i <=peakindex[0]; i++)
                {
                    if((peakindex[i]>=si.nPos)&(peakindex[i]<(si.nPos+displaysize))){
                        beattimes++;
                        float pre4_avrr=0;
                        float rrinterval[5]={0};
                        if(i>5){
                            for(UINT8 j=0;j<5;j++){
                                rrinterval[j] = peakindex[i-4+j]-peakindex[i-5+j];
                            }
                            for(UINT8 j=0;j<4;j++){
                                pre4_avrr+=rrinterval[j];
                            }
                            pre4_avrr=pre4_avrr/4;

                            if(//漏搏
                                (rrinterval[4]>(1.5*pre4_avrr))
                                &(rrinterval[4]<(2.4*360))){
                                TCHAR diastr[]=TEXT("漏搏");
                                TextOut(hdc,850,40,diastr,lstrlen(diastr));
                            }
                            if(//停搏
                            (rrinterval[4]>=(2.4*360))
                            ){
                               TCHAR diastr[]=TEXT("停搏");
                                TextOut(hdc,850,60,diastr,lstrlen(diastr));
                            }
                            if(//心动过速
                            (rrinterval[4]<=180)
                            &(rrinterval[3]<=180)
                            &(rrinterval[2]<=180)
                            &(rrinterval[1]<=180)){
                                TCHAR diastr[]=TEXT("心动过速");
                                TextOut(hdc,850,80,diastr,lstrlen(diastr));
                            }
                            if(//心动过缓
                            (rrinterval[4]>=540)
                            &(rrinterval[3]>=540)
                            &(rrinterval[2]>=540)
                            &(rrinterval[1]>=540)){
                                TCHAR diastr[]=TEXT("心动过缓");
                                TextOut(hdc,850,100,diastr,lstrlen(diastr));
                            }
                            float th=pre4_avrr/5;
                            if(//心律不齐
                            (myabs(rrinterval[4]-rrinterval[3])>=th)
                            &(myabs(rrinterval[3]-rrinterval[2])>=th)
                            &(myabs(rrinterval[2]-rrinterval[1])>=th)){
                                TCHAR diastr[]=TEXT("心律不齐");
                                TextOut(hdc,850,120,diastr,lstrlen(diastr));
                            }
                        }
                    }
                }
                
                float hr=(float)beattimes*(360.0*60.0)/displaysize;
                TCHAR hrstr[30];
                _sntprintf(hrstr,30,TEXT("显示区域心率：%.2f"),hr);
                TextOut(hdc,800,20,hrstr,lstrlen(hrstr));
                _tprintf(TEXT("%s\n"),hrstr);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//绘图窗口函数
LRESULT CALLBACK WndProcplot(HWND hwnd, UINT msg, WPARAM wp, LPARAM lParam) {
    static UINT32 scrollPos = 0;
    static SCROLLINFO si={};
    int zoom =1;
    switch (msg) {
        case WM_CREATE:{
            // 初始化水平滚动条
            ZeroMemory(&si, sizeof(SCROLLINFO));
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_RANGE;
            si.nMin = 0;
            si.nMax = 0;
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        }break;

        case WM_PAINT: {
            // 处理绘制
            PAINTSTRUCT ps;
            RECT rect;
            HDC hdc = BeginPaint(hwnd, &ps);
            // 绘制背景
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW+1));
            // 绘制折线图
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
            HGDIOBJ hOldPen = SelectObject(hdc, hPen);

            GetClientRect(hwnd, &rect);  // 获取窗口的客户区大小

            if(dpdata.data!=NULL){
                if(dpdata.size==fftsize/2){
                    MoveToEx(hdc, 0, rect.bottom-((float)fftscale*(dpdata.data[0])), NULL);
                    int width = rect.right - rect.left;
                    int x,y;
                    for (size_t i = 0; i < (fftsize/2); ++i) {
                        x = i *width /(fftsize/2);
                        y = rect.bottom-(fftscale*(dpdata.data[i]));
                        // 绘制数据点
                        LineTo(hdc, x, y);
                    }
                }
                else{
                    float min=dpdata.data[0],max=dpdata.data[0];
                    for (int i = 1; i < dpdata.size; i++) {
                        if (dpdata.data[i]< min) min = dpdata.data[i];
                        if (dpdata.data[i]> max) max = dpdata.data[i];
                    }
                    int width = rect.right - rect.left;
                    int height = rect.bottom - rect.top;

                    float scaleY= zoom *height / (max - min);
                    // 设置绘制起点
                    MoveToEx(hdc, 0, rect.bottom-scaleY*(dpdata.data[scrollPos]-min), NULL);
                    // 根据数据类型选择绘制方式
                    int x,y;
                    for (size_t i = 0; i < displaysize; ++i) {
                        x = i * width / displaysize;
                        y = rect.bottom-scaleY*(dpdata.data[scrollPos+i]-min);
                        // 绘制数据点
                        LineTo(hdc, x, y);
                    }

                    if((peakindex!=NULL)&(dpdata.size==fdata.size)){
                        UINT16 xcenter=0;
                        UINT16 ycenter=0;
                        UINT16 radius=5;                

                        HPEN peakhPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
                        HGDIOBJ peakhOldPen = SelectObject(hdc, peakhPen);
                        for (size_t i = 0; i < peakindex[0]; i++)
                        {
                            if((peakindex[i+1]>=scrollPos)&(peakindex[i+1]<scrollPos+displaysize)){
                                xcenter=(peakindex[i+1]-scrollPos)* width / displaysize;
                                ycenter=rect.bottom-scaleY*(dpdata.data[(peakindex[i+1])]-min);
                                Arc(hdc, xcenter - radius, ycenter - radius,
                                    xcenter + radius, ycenter + radius,
                                    xcenter , ycenter - radius,
                                    xcenter , ycenter - radius);
                            }
                        }
                        // 恢复原来的画笔
                        SelectObject(hdc, peakhOldPen);
                        // 删除创建的画笔
                        DeleteObject(peakhPen); 
                    }
                
                }
            }
            // 释放画笔
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
            EndPaint(hwnd, &ps);
        }
        break;

        case WM_HSCROLL: {
            HWND parhwnd= GetParent(hwnd);
            InvalidateRect(parhwnd,NULL,TRUE);
            // 获取滚动条的消息
            switch (LOWORD(wp)) {
                case SB_LINELEFT:      // 向左滚动
                    if(scrollPos>0){
                        scrollPos--;}
                break;

                case SB_LINERIGHT:     // 向右滚动
                    ZeroMemory(&si,sizeof(SCROLLINFO));
                    si.cbSize=sizeof(SCROLLINFO);
                    si.fMask=SIF_RANGE|SIF_PAGE;
                    GetScrollInfo(hwnd,SB_HORZ,&si);
                    if (scrollPos<(si.nMax-si.nPage-1)){
                        scrollPos++;}
                break;

                case SB_THUMBTRACK:{    // 用户拖动滑块
                // 创建并初始化 SCROLLINFO 结构
                    ZeroMemory(&si,sizeof(SCROLLINFO));
                    si.cbSize=sizeof(SCROLLINFO);
                    si.fMask = SIF_TRACKPOS|SIF_RANGE|SIF_PAGE;  // 只获取滚动条的位置
                    // 获取当前滚动条的位置
                    GetScrollInfo(hwnd, SB_HORZ, &si);
                    // 更新 scrollPos
                    scrollPos = si.nTrackPos;

                }break;

                default:
                break;
            }
            ZeroMemory(&si,sizeof(SCROLLINFO));
            si.cbSize=sizeof(SCROLLINFO);
            si.fMask = SIF_TRACKPOS|SIF_POS;  // 只获取滚动条的位置
            si.nPos = scrollPos;
            si.nTrackPos=scrollPos;
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);  // 重新绘制窗口
        }break;

        default:
            return DefWindowProc(hwnd, msg, wp, lParam);
    }
    return 0;
}


//入口
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PTSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    const TCHAR CLASS_NAME[]  =_T("Sample Window Class");
    
    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, TEXT("This program requires Windows NT!"),
                   CLASS_NAME, MB_ICONERROR);
        return 0;
    }

    // 注册子窗口类
    WNDCLASSEX wcplot={};
    wcplot.cbSize = sizeof(WNDCLASSEX);
    wcplot.lpfnWndProc = WndProcplot;
    wcplot.hInstance = hInstance;
    wcplot.lpszClassName = TEXT("plot");
 
    if (!RegisterClassEx(&wcplot)) {
        MessageBox(NULL,TEXT("子窗口类注册失败！"), TEXT("错误"), MB_ICONERROR);
        return 0;
    }
    // Create the window.
    RECT mainwinrect={200, 200, 1200, 600};
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        _T("ECG processing tool"),    // Window text
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,            // Window style
        // Size and position
        mainwinrect.left, mainwinrect.top, mainwinrect.right-mainwinrect.left, mainwinrect.bottom-mainwinrect.top,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
        );
    if (hwnd == NULL){
        return 0;
    }
    ShowWindow(hwnd, nCmdShow);
    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}


// 计算复数乘法
complex mtp(complex cpx1, complex cpx2) {
    complex product;
    product.Re = (cpx1.Re * cpx2.Re) - (cpx1.Im * cpx2.Im);
    product.Im = (cpx1.Re * cpx2.Im) + (cpx1.Im * cpx2.Re);
    return product;
}

//返回绝对值
float myabs(float a){
    if(a<0)return(-a);
    else return a;
}
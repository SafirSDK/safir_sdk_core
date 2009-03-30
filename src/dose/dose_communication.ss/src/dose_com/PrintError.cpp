/******************************************************************************
*
* Copyright Saab AB, 2004-2008 (http://www.safirsdk.com)
*
* Created by: Lars Engdahl / stlsen
*
*******************************************************************************
*
* This file is part of Safir SDK Core.
*
* Safir SDK Core is free software: you can redistribute it and/or modify
* it under the terms of version 3 of the GNU General Public License as
* published by the Free Software Foundation.
*
* Safir SDK Core is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/

/******************************************************************
* PrintError.cpp - a part of DoseComDll - For LINUX and WIN32
*
* Opens a console window and writes text to it.
*
* Can be used from a GUI application which automatically
* opens a console window an writes in it.
*
* Entries:
* --------
* void PrintErr(int ErrorCode, const char *format, ... )
* void PrintDbg( const char *format, ... )
* void PrintSetMode(DWORD mode, char *pIpAddr)
*
* Currently implemented modes - see g_OutPutMode below.
*********************************************************************/

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#define  IS_USING_SOCKETS
#include "DosePlatform.h"

#include <stdio.h>
#include <stdarg.h>
#include "PrintError.h"

#ifdef _LINUX
typedef unsigned long SOCKET;
#define INVALID_SOCKET -1
#define _vsnprintf vsnprintf
#endif


// g_OutPutMode =
// 'U'  - send to UDP
// 'N'  - no print
// 'C'  - Print To Console
// 'S'  - stdout
// else - stdout

static unsigned long g_OutPutMode = 'U';  // might be modified by PrintSetMode()

static SOCKET g_SockId = INVALID_SOCKET;
static struct sockaddr_in g_SockName;

static char g_IpAddr[20] = SERVER_IPADDR;  // might be modified by PrintSetMode()

static char * Get_IpAddr() { return(g_IpAddr); }

static char g_ProgName[PROGRAM_NAME_LENGTH] = {0};  //Note: this aso offset in msg to server

/***************************************************************
* Convert error number to a text
* Puts message in pBuff, returns a pointer to pBuff.
****************************************************************/

static char *Get_Err_Text(int err_code, char *pBuff, int maxSize)
{
    int     result;
    char    *pTxt = NULL;

    //------------------------
    // Is it an OS error
    //------------------------
    sprintf(pBuff,"E=%d - ", err_code);
    int len = strlen(pBuff);

#ifdef _WIN32
    result = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, err_code,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            (LPTSTR) &pBuff[len], maxSize-len, NULL );
    if (result != 0)
    {
        return pBuff;
    }

    // Winsock errors
    switch(err_code)
    {
    case WSAEINTR:           pTxt= "WSAEINTR - "; break;
    case WSAEBADF:           pTxt= "WSAEBADF - "; break;
    case WSAEACCES:          pTxt= "WSAEACCES - "; break;
    case WSAEFAULT:          pTxt= "WSAEFAULT - "; break;
    case WSAEINVAL:          pTxt= "WSAEINVAL - "; break;
    case WSAEMFILE:          pTxt= "WSAEMFILE - "; break;

    case WSAEWOULDBLOCK:     pTxt= "WSAEWOULDBLOCK - "; break;
    case WSAEINPROGRESS:     pTxt= "WSAEINPROGRESS - "; break;
    case WSAEALREADY:        pTxt= "WSAEALREADY - "; break;
    case WSAENOTSOCK:        pTxt= "WSAENOTSOCK - "; break;
    case WSAEDESTADDRREQ:    pTxt= "WSAEDESTADDRREQ - "; break;
    case WSAEMSGSIZE:        pTxt= "WSAEMSGSIZE - "; break;
    case WSAEPROTOTYPE:      pTxt= "WSAEPROTOTYPE - "; break;
    case WSAENOPROTOOPT:     pTxt= "WSAENOPROTOOPT - "; break;
    case WSAEPROTONOSUPPORT: pTxt= "WSAEPROTONOSUPPORT - "; break;
    case WSAESOCKTNOSUPPORT: pTxt= "WSAESOCKTNOSUPPORT - "; break;
    case WSAEOPNOTSUPP:      pTxt= "WSAEOPNOTSUPP - "; break;
    case WSAEPFNOSUPPORT:    pTxt= "WSAEPFNOSUPPORT - "; break;
    case WSAEAFNOSUPPORT:    pTxt= "WSAEAFNOSUPPORT - "; break;
    case WSAEADDRINUSE:      pTxt= "WSAEADDRINUSE - "; break;
    case WSAEADDRNOTAVAIL:   pTxt= "WSAEADDRNOTAVAIL - "; break;
    case WSAENETDOWN:        pTxt= "WSAENETDOWN - "; break;
    case WSAENETUNREACH:     pTxt= "WSAENETUNREACH - "; break;
    case WSAENETRESET:       pTxt= "WSAENETRESET - "; break;
    case WSAECONNABORTED:    pTxt= "WSAECONNABORTED - "; break;
    case WSAECONNRESET:      pTxt= "WSAECONNRESET - "; break;
    case WSAENOBUFS:         pTxt= "WSAENOBUFS - "; break;
    case WSAEISCONN:         pTxt= "WSAEISCONN - "; break;
    case WSAENOTCONN:        pTxt= "WSAENOTCONN - "; break;
    case WSAESHUTDOWN:       pTxt= "WSAESHUTDOWN - "; break;
    case WSAETOOMANYREFS:    pTxt= "WSAETOOMANYREFS - "; break;
    case WSAETIMEDOUT:       pTxt= "WSAETIMEDOUT - "; break;
    case WSAECONNREFUSED:    pTxt= "WSAECONNREFUSED - "; break;
    case WSAELOOP:           pTxt= "WSAELOOP - "; break;
    case WSAENAMETOOLONG:    pTxt= "WSAENAMETOOLONG - "; break;
    case WSAEHOSTDOWN:       pTxt= "WSAEHOSTDOWN - "; break;
    case WSAEHOSTUNREACH:    pTxt= "WSAEHOSTUNREACH - "; break;
    case WSAENOTEMPTY:       pTxt= "WSAENOTEMPTY - "; break;
    case WSAEPROCLIM:        pTxt= "WSAEPROCLIM - "; break;
    case WSAEUSERS:          pTxt= "WSAEUSERS - "; break;
    case WSAEDQUOT:          pTxt= "WSAEDQUOT - "; break;
    case WSAESTALE:          pTxt= "WSAESTALE - "; break;
    case WSAEREMOTE:         pTxt= "WSAEREMOTE - "; break;
    case WSANOTINITIALISED:  pTxt= "WSANOTINITIALISED -"; break;

    default: if ((err_code >= WSABASEERR) && (err_code < WSABASEERR+200))
                 pTxt = "Unknown Winsock error";
             break;
    } // end switch

    if (pTxt == NULL) "Unknown error code";

    sprintf(pBuff,"E=%d: - %s", err_code, pTxt);
#endif

    return(pBuff);
}
/*--------------------- end Get_Err_text() ------------------------*/

/************************************************************
* local
************************************************************/
#ifdef _WIN32
static void PrintToConsole( const char *pTxt)
{
    int Length;
    DWORD NumberOfCharsWritten;
    static COORD dwWriteCoord = {1,1};
    static HANDLE hConsoleOutput = INVALID_HANDLE_VALUE;


    Length = strlen(pTxt);

    if(hConsoleOutput == INVALID_HANDLE_VALUE)
    {
        AllocConsole();
        hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    WriteFile(hConsoleOutput,   //
                pTxt,           // characters
                Length,         // number of characters to write
                &NumberOfCharsWritten,
                NULL);
}
/*---------------------- end PrintToConsole() ----------------------*/
#endif

/************************************************
* Used first time - creates a send socket
*************************************************/

static SOCKET Create_Socket(void)
{
    SOCKET SockId;
#ifdef _WIN32
    WSADATA  WSAData;
    if (WSAStartup(MAKEWORD(2,2) ,&WSAData ) != 0)
    {
        printf("WSAStartup failed\n");
        return(INVALID_SOCKET);
    }
#endif

    SockId = socket(AF_INET,SOCK_DGRAM,0 );
    if (SockId == INVALID_SOCKET)
    {
        printf("ERROR: Can't create socket.\n");
        return(INVALID_SOCKET);
    }

    g_SockName.sin_family      = AF_INET;
    g_SockName.sin_addr.s_addr = inet_addr(Get_IpAddr());
    g_SockName.sin_port        = htons(PRINTLOG_PORT);

    return(SockId);
}
/*------------------ end Create_Socket() ------------------*/

/**********************************************************
* Args as printf
***********************************************************/
static int PrintUdp( const char *pMsg0)
{
    if(g_SockId == INVALID_SOCKET)  // First time
    {
        g_SockId = Create_Socket();
        if (g_SockId == INVALID_SOCKET) return(-1);
    }

#ifdef zzzzzzzz_WIN32
    int     result;
    unsigned long   dwNumberOfBytesSent;
    int     NumWsaBuff;
    WSABUF  WsaBuff[4];

    if(g_SockId == INVALID_SOCKET)  // First time
    {
        g_SockId = Create_Socket();
        if (g_SockId == INVALID_SOCKET) return(-1);
    }

    WsaBuff[0].buf = (char *) pMsg0;
    WsaBuff[0].len = strlen(pMsg0);

    NumWsaBuff = 1;

    result = WSASendTo (g_SockId,  WsaBuff,NumWsaBuff,
                        &dwNumberOfBytesSent,0, //dwFlags,
                       (struct sockaddr *) &g_SockName,
                        sizeof(g_SockName), NULL, NULL);
#endif

//#ifdef _LINUX
    sendto(g_SockId, pMsg0, strlen(pMsg0),0,
        (struct sockaddr *) &g_SockName, sizeof(g_SockName));
//#endif
    return(0);
}
/*----------------- end PrintUdp() -----------------*/

/**********************************************************
* Args as printf
***********************************************************/

void PrintDbg( const char *format, ... )
{
    va_list marker;
    char    buffer[1024];

    va_start( marker, format );     /* Initialize variable arguments. */

    _vsnprintf(buffer, sizeof(buffer), format, marker);

    va_end( marker ); /* needed ? Reset variable arguments */

    if(g_OutPutMode == 'N') return;
#ifdef _WIN32
    if(g_OutPutMode == 'C')     PrintToConsole(buffer);
    else
#endif
    if(g_OutPutMode == 'U')     PrintUdp(buffer);
    else
    {
        printf("%s", buffer); fflush(stdout);
    }
}

/**********************************************************
* Args as printf
***********************************************************/

void PrintErr(int ErrorCode, const char *format, ... )
{
    va_list marker;
    char    Buf2[1024];
    char    Buf1[400];
    char    ErrBuf[240];

    va_start( marker, format );     /* Initialize variable arguments. */

    _vsnprintf(Buf1, sizeof(Buf1), format, marker);

    va_end( marker ); /* needed ? Reset variable arguments */

    if(g_OutPutMode == 'N') return;

    if(ErrorCode != 0)
    {
        Get_Err_Text(ErrorCode, ErrBuf, sizeof(ErrBuf));
        sprintf(Buf2,"ERROR: %s - %s\n",Buf1, ErrBuf);
    }
    else
    {
        sprintf(Buf2,"ERROR: %s\n",Buf1);
    }
#ifdef _WIN32
    if(g_OutPutMode == 'C') PrintToConsole(Buf2);
    else
#endif
    if(g_OutPutMode == 'U') PrintUdp(Buf2);
    else                    printf("%s", Buf2);
}

/**********************************************************
* Turn printing on or off
*
* mode =
* 'U'  - send to UDP
* 'N'  - no print
* 'C'  - Print To Console
* 'S'  - stdout
* else - stdout
*
***********************************************************/
void PrintSetMode(unsigned long mode, char *pIpAddr)  //dotted decimal or NULL
{
    printf("PrintSetMode(%lX,%s)\n",mode, pIpAddr);

    if(pIpAddr != NULL)
        strncpy(g_IpAddr, pIpAddr, sizeof(g_IpAddr));
    if(mode != 0) g_OutPutMode = mode;
}
/*------------------------- end PrintError.cpp ----------------------*/

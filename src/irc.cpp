// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "irc.h"
#include "net.h"
#include "strlcpy.h"
#include "base58.h"

using namespace std;
using namespace boost;

int nGotIRCAddresses = 0;

void ThreadIRCSeed2(void* parg);




#pragma pack(push, 1)
struct ircaddr
{
    struct in_addr ip;
    short port;
};
#pragma pack(pop)

string EncodeAddress(const CService& addr)
{
    struct ircaddr tmp;
    if (addr.GetInAddr(&tmp.ip))
    {
        tmp.port = htons(addr.GetPort());

        vector<unsigned char> vch(UBEGIN(tmp), UEND(tmp));
        return string("u") + EncodeBase58Check(vch);
    }
    return "";
}

bool DecodeAddress(string str, CService& addr)
{
    vector<unsigned char> vch;
    if (!DecodeBase58Check(str.substr(1), vch))
        return false;

    struct ircaddr tmp;
    if (vch.size() != sizeof(tmp))
        return false;
    memcpy(&tmp, &vch[0], sizeof(tmp));

    addr = CService(tmp.ip, ntohs(tmp.port));
    return true;
}






static bool Send(SOCKET hSocket, const char* pszSend)
{
    if (strstr(pszSend, "PONG") != pszSend)
        printf("IRC SENDING: %s\n", pszSend);
    const char* psz = pszSend;
    const char* pszEnd = psz + strlen(psz);
    while (psz < pszEnd)
    {
        int ret = send(hSocket, psz, pszEnd - psz, MSG_NOSIGNAL);
        if (ret < 0)
            return false;
        psz += ret;
    }
    return true;
}

bool RecvLineIRC(SOCKET hSocket, string& strLine)
{
    while (true)
    {
        bool fRet = RecvLine(hSocket, strLine);
        if (fRet)
        {
            if (fShutdown)
                return false;
            vector<string> vWords;
            ParseString(strLine, ' ', vWords);
            if (vWords.size() >= 1 && vWords[0] == "PING")
            {
                strLine[1] = 'O';
                strLine += '\r';
                Send(hSocket, strLine.c_str());
                continue;
            }
        }
        return fRet;
    }
}

int RecvUntil(SOCKET hSocket, const char* psz1, const char* psz2=NULL, const char* psz3=NULL, const char* psz4=NULL)
{
    while (true)
    {
        string strLine;
        strLine.reserve(10000);
        if (!RecvLineIRC(hSocket, strLine))
            return 0;
        printf("IRC %s\n", strLine.c_str());
        if (psz1 && strLine.find(psz1) != string::npos)
            return 1;
        if (psz2 && strLine.find(psz2) != string::npos)
            return 2;
        if (psz3 && strLine.find(psz3) != string::npos)
            return 3;
        if (psz4 && strLine.find(psz4) != string::npos)
            return 4;
    }
}

bool Wait(int nSeconds)
{
    if (fShutdown)
        return false;
    printf("IRC waiting %d seconds to reconnect\n", nSeconds);
    for (int i = 0; i < nSeconds; i++)
    {
        if (fShutdown)
            return false;
        MilliSleep(1000);
    }
    return true;
}

bool RecvCodeLine(SOCKET hSocket, const char* psz1, string& strRet)
{
    strRet.clear();
    while (true)
    {
        string strLine;
        if (!RecvLineIRC(hSocket, strLine))
            return false;

        vector<string> vWords;
        ParseString(strLine, ' ', vWords);
        if (vWords.size() < 2)
            continue;

        if (vWords[1] == psz1)
        {
            printf("IRC %s\n", strLine.c_str());
            strRet = strLine;
            return true;
        }
    }
}

bool GetIPFromIRC(SOCKET hSocket, string strMyName, CNetAddr& ipRet)
{
    Send(hSocket, strprintf("USERHOST %s\r", strMyName.c_str()).c_str());

    string strLine;
    if (!RecvCodeLine(hSocke
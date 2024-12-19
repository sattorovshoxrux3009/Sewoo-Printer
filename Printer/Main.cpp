#include <iostream>
#include <string>
#include <tchar.h> 
#include <json.hpp>  
#include <sstream>
#include <fstream>
#include "./httplib.h"

using json = nlohmann::json;
using namespace std;

// Funksiya ko'rsatkichlarini e'lon qilish
typedef int(__stdcall* OpenPortFunc)(const char*, int);
typedef int(__stdcall* ClosePortFunc)();
typedef long(__stdcall* PrintStringFunc)(LPCSTR);
typedef long (*PrintStartFunc)();
typedef long (*PrintStopFunc)();
typedef int(__stdcall* PrintNormalFunc)(const char*);
typedef int(__stdcall* CutPaperFunc)();
typedef int(__stdcall* PrintQRCodeFunc)(const char*, int, int, int, int);
typedef long(__stdcall* PrintingWidthFunc)(LONG);
typedef long(__stdcall* PrintBarCodeFunc)(LPCTSTR, LONG, LONG, LONG, LONG, LONG);
typedef long (*PrinterStsFunc)();

string strCenter = "\x1B\x61\x31"; // 중앙정렬
string strLeft = "\x1B\x61\x30"; // 왼쪽정렬
string strRight = "\x1B\x61\x32"; // 오른쪽정렬
string strDouble = "\x1B\x21\x20"; // Horizontal Double
string strUnderline = "\x1B\x21\x80"; // underline
string strDoubleBold = "\x1B\x21\x28"; // Emphasize
string strNormal = "\x1B\x21\x02"; // 중앙정렬
LPCSTR PartialCut = "\x1D\x56\x42\x01"; // Partial Cut.

int main() {
    std::ifstream configFile("config.ini");
    if (!configFile.is_open()) {
        printf("\nconfig.ini file not found.");
        Sleep(3000);
        main();
    }
    std::string lineConfig;
    int port = 0;
    while (std::getline(configFile, lineConfig)) {
        if (lineConfig.find("PORT=") == 0) {
            std::string portStr = lineConfig.substr(5); 
            std::stringstream(portStr) >> port; 
            break;
        }
    }
    configFile.close();
    if (port == 0) {
        printf("\nPort not found in config.ini file");
        Sleep(3000);
        main();
    }
    printf("\nPort : %d\n", port);

    // Upload DLL file
    const wchar_t* dllPath = L"LKPOSTOT.dll";
    HMODULE hDll = LoadLibrary(dllPath);
    if (!hDll) {
        printf("\nLKPOSTOT.dll file not found");
        Sleep(3000);
        main();
    }
    OpenPortFunc OpenPort = (OpenPortFunc)GetProcAddress(hDll, "OpenPort");
    ClosePortFunc ClosePort = (ClosePortFunc)GetProcAddress(hDll, "ClosePort");
    PrintStartFunc PrintStart = (PrintStartFunc)GetProcAddress(hDll, "PrintStart");
    PrintStopFunc PrintStop = (PrintStopFunc)GetProcAddress(hDll, "PrintStop");
    PrintStringFunc PrintString = (PrintStringFunc)GetProcAddress(hDll, "PrintString");
    PrintQRCodeFunc PrintQRCode = (PrintQRCodeFunc)GetProcAddress(hDll, "PrintQRCode");
    CutPaperFunc CutPaper = (CutPaperFunc)GetProcAddress(hDll, "CutPaper");
    PrinterStsFunc PrinterSts = (PrinterStsFunc)GetProcAddress(hDll, "PrinterSts");

    if (!OpenPort || !ClosePort || !PrintString || !PrintStart || !PrintStop || !PrintQRCode) {
        printf("\nFunctions not found in DLL file");
        FreeLibrary(hDll);
        Sleep(3000);
        main();
    }
    // Open printer port
    int result = OpenPort("USB", 115200);
    if (result != 0) {
        printf("\nError on open port");
        Sleep(3000);
        main();
    }
    else {
        printf("Port opened\n");
    }
    
    
    httplib::Server svr;
    svr.Post(R"(/.*)", [&PrintStart, &PrintString, &PrintStop, &PrintQRCode, &result, &PrinterSts](const httplib::Request& req, httplib::Response& res) {
        try {
            auto jsonArray = json::parse(req.body);
            if (jsonArray.is_array()) {
                PrintStart();
                for (const auto& element : jsonArray) {
                    string printString = "";
                    if (element.contains("type") && element.contains("align") && element.contains("font") && element.contains("body")) {
                        if (element["type"] == "text") {
                            // Find align 
                            if (element["align"] == "center") { printString = printString + strCenter; }
                            else if (element["align"] == "right") { printString = printString + strRight; }
                            else { printString = printString + strLeft; }
                            // Find font
                            if (element["font"] == "bold") { printString = printString + strDoubleBold; }
                            else if (element["font"] == "large") { printString = printString + strDouble; }
                            else if (element["font"] == "underline") { printString = printString + strUnderline; }
                            else { printString = printString + strNormal; }
                            printString = printString + element["body"].get<std::string>() + "\r\n";
                            LPCSTR line = printString.c_str();
                            long printerStatus = PrinterSts();
                            if (printerStatus == 0) {
                                PrintString(line);
                            }
                            else
                            {   
                                switch (printerStatus){
                                    case 1: {
                                        res.status = 400;
                                        res.set_content("Cover Open", "text/plain");
                                    }break;
                                    case 2: {
                                        res.status = 400;
                                        res.set_content("Paper Near Empty", "text/plain");
                                    }break;
                                    case 4: {
                                        res.status = 400;
                                        res.set_content("Paper Empty", "text/plain");
                                    }break;
                                    case 8: {
                                        res.status = 400;
                                        res.set_content("Power Off", "text/plain");
                                    }break;
                                }
                                return;
                            }
                            
                        }
                        else if (element["type"] == "qrCode") {
                            const std::string& body = element["body"].get<std::string>();
                            char* strQRCode = new char[body.size() + 1];
                            strcpy_s(strQRCode, body.size() + 1, body.c_str());
                            long printerStatus = PrinterSts();
                            if (printerStatus == 0) {
                                if (element["align"] == "right") {
                                    PrintQRCode(strQRCode, body.size(), 7, 3, 2);
                                }
                                else if (element["align"] == "center") {
                                    PrintQRCode(strQRCode, body.size(), 7, 3, 1);
                                }
                                else {
                                    PrintQRCode(strQRCode, body.size(), 7, 3, 0);
                                }
                                delete[] strQRCode;
                            }
                            else
                            {
                                switch (printerStatus) {
                                case 1: {
                                    res.status = 400;
                                    res.set_content("Cover Open", "text/plain");
                                }break;
                                case 2: {
                                    res.status = 400;
                                    res.set_content("Paper Near Empty", "text/plain");
                                }break;
                                case 4: {
                                    res.status = 400;
                                    res.set_content("Paper Empty", "text/plain");
                                }break;
                                case 8: {
                                    res.status = 400;
                                    res.set_content("Power Off", "text/plain");
                                }break;
                                }
                                return;
                            }
                        }
                        else {
                            res.status = 400;
                            res.set_content("JSON elements are invalid", "text/plain");
                            return;
                        }
                    }
                    else
                    {
                        res.status = 400;
                        res.set_content("JSON elements are invalid", "text/plain");
                        return;
                    }
                }
                PrintString(PartialCut);
                PrintStop();
            }
            else {
                res.status = 400; 
                res.set_content("Array elements are invalid", "text/plain");
                return;
            }
        }
        catch (const json::parse_error& e) {
            res.status = 400; 
            res.set_content("The data sent is not in json format", "text/plain");
            return;
        }
    });
    svr.listen("localhost", port);
    main();
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>

// Função melhorada para instalar a impressora virtual
BOOL install_virtual_printer() {
    PRINTER_INFO_2 printerInfo = {0};
    char driverName[] = "Generic / Text Only";
    char portName[] = "VIRTUAL001:";
    
    // Configura a impressora virtual
    printerInfo.pPrinterName = "Zebra Virtual (Redirect to Elgin)";
    printerInfo.pPortName = portName;
    printerInfo.pDriverName = driverName;
    printerInfo.pPrintProcessor = "WinPrint";
    printerInfo.pDatatype = "RAW";
    printerInfo.Attributes = PRINTER_ATTRIBUTE_DIRECT;
    printerInfo.pComment = "Virtual ZPL to Elgin Converter";
    printerInfo.pLocation = "Virtual Port";
    printerInfo.pSepFile = "";
    printerInfo.pPrintProcessor = "WinPrint";
    printerInfo.pDatatype = "RAW";
    
    // Adiciona a impressora
    HANDLE hPrinter;
    if (!AddPrinter(NULL, 2, (LPBYTE)&printerInfo)) {
        DWORD error = GetLastError();
        printf("Erro instalando impressora virtual: %d\n", error);
        
        // Tenta instalar com configurações alternativas
        if (error == 1801) { // Printer already exists
            printf("Impressora virtual já existe. Tentando reconfigurar...\n");
            return reconfigure_existing_printer();
        }
        return FALSE;
    }
    
    printf("Impressora virtual instalada com sucesso: %s\n", printerInfo.pPrinterName);
    
    // Abre e fecha para confirmar
    if (OpenPrinter(printerInfo.pPrinterName, &hPrinter, NULL)) {
        ClosePrinter(hPrinter);
    }
    
    return TRUE;
}

BOOL reconfigure_existing_printer() {
    HANDLE hPrinter;
    PRINTER_INFO_2* pPrinterInfo = NULL;
    DWORD bytesNeeded;
    
    // Abre a impressora existente
    if (!OpenPrinter("Zebra Virtual (Redirect to Elgin)", &hPrinter, NULL)) {
        printf("Não foi possível abrir impressora existente\n");
        return FALSE;
    }
    
    // Obtém informações atuais
    GetPrinter(hPrinter, 2, NULL, 0, &bytesNeeded);
    if (bytesNeeded > 0) {
        pPrinterInfo = (PRINTER_INFO_2*)malloc(bytesNeeded);
        if (GetPrinter(hPrinter, 2, (LPBYTE)pPrinterInfo, bytesNeeded, &bytesNeeded)) {
            // Reconfigura
            pPrinterInfo->pPortName = "VIRTUAL001:";
            pPrinterInfo->Attributes = PRINTER_ATTRIBUTE_DIRECT;
            
            if (SetPrinter(hPrinter, 2, (LPBYTE)pPrinterInfo, 0)) {
                printf("Impressora virtual reconfigurada com sucesso\n");
                free(pPrinterInfo);
                ClosePrinter(hPrinter);
                return TRUE;
            }
        }
        free(pPrinterInfo);
    }
    
    ClosePrinter(hPrinter);
    return FALSE;
}

// Função para verificar se a impressora está instalada
BOOL is_printer_installed() {
    DWORD needed, returned;
    EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &returned);
    
    if (needed > 0) {
        BYTE* buffer = malloc(needed);
        if (EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 2, buffer, needed, &needed, &returned)) {
            PRINTER_INFO_2* printers = (PRINTER_INFO_2*)buffer;
            
            for (DWORD i = 0; i < returned; i++) {
                if (strstr(printers[i].pPrinterName, "Zebra Virtual")) {
                    free(buffer);
                    return TRUE;
                }
            }
        }
        free(buffer);
    }
    return FALSE;
}
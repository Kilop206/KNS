#include "NativeFileDialog.hpp"

#include <stdexcept>
#include <memory>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <shobjidl.h>

namespace {
void check(HRESULT result) {
    if (FAILED(result)) {
        throw std::runtime_error("Windows file dialog failed (HRESULT " +
            std::to_string(static_cast<unsigned long>(result)) + ")");
    }
}
struct Apartment {
    Apartment() { check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)); }
    ~Apartment() { CoUninitialize(); }
};
struct Release {
    template<class T> void operator()(T* value) const { if (value) value->Release(); }
};
struct TaskFree {
    void operator()(wchar_t* value) const { CoTaskMemFree(value); }
};
std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 dialog title");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), size);
    return result;
}
}

std::optional<std::string> gui::chooseTopologyFile(GLFWwindow* owner, bool save,
    const std::string& title) {
    Apartment apartment;
    IFileDialog* raw = nullptr;
    check(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
        nullptr, CLSCTX_INPROC_SERVER, IID_IFileDialog, reinterpret_cast<void**>(&raw)));
    std::unique_ptr<IFileDialog, Release> dialog(raw);
    FILEOPENDIALOGOPTIONS options;
    check(dialog->GetOptions(&options));
    check(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR |
        FOS_PATHMUSTEXIST | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST)));
    const COMDLG_FILTERSPEC filters[] = {{L"JSON topology (*.json)", L"*.json"}};
    check(dialog->SetFileTypes(1, filters));
    check(dialog->SetDefaultExtension(L"json"));
    check(dialog->SetTitle(wide(title).c_str()));
    if (save) check(dialog->SetFileName(L"topology-edited.json"));
    const HRESULT shown = dialog->Show(owner ? glfwGetWin32Window(owner) : nullptr);
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::nullopt;
    check(shown);
    IShellItem* itemRaw = nullptr;
    check(dialog->GetResult(&itemRaw));
    std::unique_ptr<IShellItem, Release> item(itemRaw);
    PWSTR pathRaw = nullptr;
    check(item->GetDisplayName(SIGDN_FILESYSPATH, &pathRaw));
    std::unique_ptr<wchar_t, TaskFree> path(pathRaw);
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.get(),
        -1, nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Cannot convert selected path to UTF-8");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.get(), -1,
        result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}
#else
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
// Execute desktop helpers directly: titles and file names are never shell code.
std::pair<int, std::string> runDialog(std::vector<std::string> arguments) {
    std::vector<char*> argv;
    for (auto& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    int pipes[2];
    if (pipe(pipes) != 0) throw std::runtime_error("Cannot create file dialog pipe");
    const pid_t child = fork();
    if (child == -1) {
        close(pipes[0]); close(pipes[1]);
        throw std::runtime_error("Cannot start file dialog");
    }
    if (child == 0) {
        close(pipes[0]);
        if (dup2(pipes[1], STDOUT_FILENO) == -1) _exit(126);
        close(pipes[1]);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(pipes[1]);
    std::string output;
    char buffer[4096];
    ssize_t count;
    while ((count = read(pipes[0], buffer, sizeof(buffer))) != 0) {
        if (count > 0) output.append(buffer, static_cast<std::size_t>(count));
        else if (errno != EINTR) break;
    }
    close(pipes[0]);
    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) throw std::runtime_error("Cannot wait for file dialog");
    }
    if (!output.empty() && output.back() == '\n') output.pop_back();
    return {WIFEXITED(status) ? WEXITSTATUS(status) : -1, output};
}
}

std::optional<std::string> gui::chooseTopologyFile(GLFWwindow*, bool save,
    const std::string& title) {
#ifdef __APPLE__
    const std::string script = std::string("on run argv\ntry\nreturn POSIX path of (") +
        (save ? "choose file name default name \"topology-edited.json\"" : "choose file of type {\"public.json\"}") +
        " with prompt (item 1 of argv))\non error number -128\nreturn \"\"\nend try\nend run";
    auto [status, path] = runDialog({"osascript", "-e", script, title});
#else
    std::vector<std::string> arguments = {"zenity", "--file-selection", "--title=" + title,
        "--file-filter=JSON topology | *.json"};
    if (save) arguments.insert(arguments.end(), {"--save", "--confirm-overwrite", "--filename=topology-edited.json"});
    auto [status, path] = runDialog(arguments);
    if (status == 127) {
        auto fallback = runDialog({"kdialog", save ? "--getsavefilename" : "--getopenfilename",
            save ? "topology-edited.json" : ".", "*.json|JSON topology", "--title", title});
        status = fallback.first;
        path = std::move(fallback.second);
    }
    if (status == 1) return std::nullopt;
#endif
    if (status != 0) throw std::runtime_error("Cannot open system file dialog (on Linux, install zenity or kdialog)");
    if (path.empty()) return std::nullopt;
    return path;
}
#endif

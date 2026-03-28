// pe - dll generator

module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef min
#undef max
#include <cstring>
#include <ctime>
#include <cassert>
#include <limits>

export module opus.pe;

import opus.ast;
import std;

export namespace opus::pe {

// single source of truth for all imported functions
constexpr const char* iat_functions[] = {
    "CreateThread",                 // 0
    "AllocConsole",                 // 1
    "SetConsoleTitleA",             // 2
    "GetStdHandle",                 // 3
    "WriteConsoleA",                // 4
    "Sleep",                        // 5
    "GetTickCount64",               // 6
    "GetProcessHeap",               // 7
    "HeapAlloc",                    // 8
    "HeapFree",                     // 9
    "VirtualProtect",               // 10
    "VirtualAlloc",                 // 11
    "VirtualFree",                  // 12
    "GetModuleHandleA",             // 13
    "GetProcAddress",               // 14
    "AddVectoredExceptionHandler",  // 15
    "RaiseException",               // 16
    "ReadConsoleA",                 // 17
    "ExitProcess",                  // 18
    "SetConsoleMode",               // 19
    "WaitForSingleObject",          // 20
    "WaitForMultipleObjects",       // 21
    "GetSystemInfo",                // 22
    "CloseHandle",                  // 23
};

constexpr std::size_t iat_func_count = sizeof(iat_functions) / sizeof(iat_functions[0]);

// each slot is 8 bytes (64-bit pointer)
constexpr std::size_t iat_slot(std::size_t index) {
    return index * 8;
}

// named indices so codegen reads clean
namespace iat_idx {
    constexpr std::size_t CreateThread          = 0;
    constexpr std::size_t AllocConsole          = 1;
    constexpr std::size_t SetConsoleTitleA      = 2;
    constexpr std::size_t GetStdHandle          = 3;
    constexpr std::size_t WriteConsoleA         = 4;
    constexpr std::size_t Sleep                 = 5;
    constexpr std::size_t GetTickCount64        = 6;
    constexpr std::size_t GetProcessHeap        = 7;
    constexpr std::size_t HeapAlloc             = 8;
    constexpr std::size_t HeapFree              = 9;
    constexpr std::size_t VirtualProtect        = 10;
    constexpr std::size_t VirtualAlloc          = 11;
    constexpr std::size_t VirtualFree           = 12;
    constexpr std::size_t GetModuleHandleA      = 13;
    constexpr std::size_t GetProcAddress        = 14;
    constexpr std::size_t AddVectoredExceptionHandler = 15;
    constexpr std::size_t RaiseException        = 16;
    constexpr std::size_t ReadConsoleA           = 17;
    constexpr std::size_t ExitProcess            = 18;
    constexpr std::size_t SetConsoleMode         = 19;
    constexpr std::size_t WaitForSingleObject    = 20;
    constexpr std::size_t WaitForMultipleObjects = 21;
    constexpr std::size_t GetSystemInfo          = 22;
    constexpr std::size_t CloseHandle            = 23;
}

// backward compat - old pe::iat::FuncName constants still work
namespace iat {
    constexpr std::size_t CreateThread          = iat_slot(iat_idx::CreateThread);
    constexpr std::size_t AllocConsole          = iat_slot(iat_idx::AllocConsole);
    constexpr std::size_t SetConsoleTitleA      = iat_slot(iat_idx::SetConsoleTitleA);
    constexpr std::size_t GetStdHandle          = iat_slot(iat_idx::GetStdHandle);
    constexpr std::size_t WriteConsoleA         = iat_slot(iat_idx::WriteConsoleA);
    constexpr std::size_t Sleep                 = iat_slot(iat_idx::Sleep);
    constexpr std::size_t GetTickCount64        = iat_slot(iat_idx::GetTickCount64);
    constexpr std::size_t GetProcessHeap        = iat_slot(iat_idx::GetProcessHeap);
    constexpr std::size_t HeapAlloc             = iat_slot(iat_idx::HeapAlloc);
    constexpr std::size_t HeapFree              = iat_slot(iat_idx::HeapFree);
    constexpr std::size_t VirtualProtect        = iat_slot(iat_idx::VirtualProtect);
    constexpr std::size_t VirtualAlloc          = iat_slot(iat_idx::VirtualAlloc);
    constexpr std::size_t VirtualFree           = iat_slot(iat_idx::VirtualFree);
    constexpr std::size_t GetModuleHandleA      = iat_slot(iat_idx::GetModuleHandleA);
    constexpr std::size_t GetProcAddress        = iat_slot(iat_idx::GetProcAddress);
    constexpr std::size_t AddVectoredExceptionHandler = iat_slot(iat_idx::AddVectoredExceptionHandler);
    constexpr std::size_t RaiseException        = iat_slot(iat_idx::RaiseException);
    constexpr std::size_t ReadConsoleA           = iat_slot(iat_idx::ReadConsoleA);
    constexpr std::size_t ExitProcess            = iat_slot(iat_idx::ExitProcess);
    constexpr std::size_t SetConsoleMode         = iat_slot(iat_idx::SetConsoleMode);
    constexpr std::size_t WaitForSingleObject    = iat_slot(iat_idx::WaitForSingleObject);
    constexpr std::size_t WaitForMultipleObjects = iat_slot(iat_idx::WaitForMultipleObjects);
    constexpr std::size_t GetSystemInfo          = iat_slot(iat_idx::GetSystemInfo);
    constexpr std::size_t CloseHandle            = iat_slot(iat_idx::CloseHandle);
}

// named types for what used to be anonymous pairs
struct IatFixup {
    std::size_t patch_site;
    std::size_t iat_offset;
};

struct LineMapEntry {
    std::uint32_t code_offset;
    std::uint32_t line;
};

// bundles all the args that generate() needs
struct GenerateConfig {
    std::vector<std::uint8_t> code;
    std::size_t main_offset = 0;
    bool alloc_console = true;
    std::vector<IatFixup> iat_fixups;
    std::string debug_source;
    std::vector<LineMapEntry> line_map;
    ast::HealingMode healing_mode = ast::HealingMode::Off;
    bool exe_mode = false;
    bool writable_text = false;
};

class DllGenerator {
public:
    // startup layout - sizes of each routine in .text before user code
    // these are padded slot sizes, not exact byte counts
    static constexpr std::size_t DLLMAIN_SIZE = 0x80;
    static constexpr std::size_t THREAD_FUNC_SIZE = 0xA0;
    static constexpr std::size_t DLL_PRINT_SIZE = 0x60;
    static constexpr std::size_t DLL_SET_TITLE_SIZE = 0x20;
    static constexpr std::size_t DLL_ALLOC_CONSOLE_SIZE = 0x20;
    static constexpr std::size_t DLL_PRINT_HEX_SIZE = 0x200;

    // these are fixed regardless of mode
    static constexpr std::size_t DLL_PRINT_OFFSET = DLLMAIN_SIZE + THREAD_FUNC_SIZE;
    static constexpr std::size_t DLL_SET_TITLE_OFFSET = DLL_PRINT_OFFSET + DLL_PRINT_SIZE;
    static constexpr std::size_t DLL_ALLOC_CONSOLE_OFFSET = DLL_SET_TITLE_OFFSET + DLL_SET_TITLE_SIZE;
    static constexpr std::size_t DLL_PRINT_HEX_OFFSET = DLL_ALLOC_CONSOLE_OFFSET + DLL_ALLOC_CONSOLE_SIZE;
    static constexpr std::size_t CRASH_HANDLER_OFFSET = DLL_PRINT_HEX_OFFSET + DLL_PRINT_HEX_SIZE;

    // exe mode - no DllMain or thread_func, just a tiny entry stub
    static constexpr std::size_t EXE_ENTRY_SIZE = 0x40;
    static constexpr std::size_t EXE_PRINT_OFFSET = EXE_ENTRY_SIZE;
    static constexpr std::size_t EXE_SET_TITLE_OFFSET = EXE_PRINT_OFFSET + DLL_PRINT_SIZE;
    static constexpr std::size_t EXE_ALLOC_CONSOLE_OFFSET = EXE_SET_TITLE_OFFSET + DLL_SET_TITLE_SIZE;
    static constexpr std::size_t EXE_PRINT_HEX_OFFSET = EXE_ALLOC_CONSOLE_OFFSET + DLL_ALLOC_CONSOLE_SIZE;
    static constexpr std::size_t EXE_CRASH_HANDLER_OFFSET = EXE_PRINT_HEX_OFFSET + DLL_PRINT_HEX_SIZE;

    // debug mode: full crash handler + repl slot + bp table
    // release mode: tiny crash handler stub, no repl, no bp table
    static constexpr std::size_t CRASH_HANDLER_SIZE_DEBUG = 0x1200;
    static constexpr std::size_t CRASH_HANDLER_SIZE_RELEASE = 0x80;  // just enough for a minimal stub
    static constexpr std::size_t REPL_HANDLER_SIZE_DEBUG = 0x800;

    // runtime layout - computed based on build config
    struct Layout {
        std::size_t crash_handler_size;
        std::size_t repl_handler_size;
        std::size_t repl_handler_offset;
        std::size_t startup_code_size;
        std::size_t data_extra;  // extra .data beyond iat + globals
        bool has_bp_table;
    };

    static Layout compute_layout(bool debug_mode, bool exe_mode = false) {
        Layout layout{};
        std::size_t crash_offset = exe_mode ? EXE_CRASH_HANDLER_OFFSET : CRASH_HANDLER_OFFSET;
        if (debug_mode) {
            layout.crash_handler_size = CRASH_HANDLER_SIZE_DEBUG;
            layout.repl_handler_size = REPL_HANDLER_SIZE_DEBUG;
            layout.repl_handler_offset = crash_offset + layout.crash_handler_size;
            layout.startup_code_size = layout.repl_handler_offset + layout.repl_handler_size;
            layout.data_extra = BP_TABLE_SIZE + REPL_BUFFER_SIZE + REPL_STATE_SIZE;
            layout.has_bp_table = true;
        } else {
            layout.crash_handler_size = CRASH_HANDLER_SIZE_RELEASE;
            layout.repl_handler_size = 0;
            layout.repl_handler_offset = 0;
            layout.startup_code_size = crash_offset + layout.crash_handler_size;
            layout.data_extra = 0;
            layout.has_bp_table = false;
        }
        return layout;
    }

    // backward compat - debug layout (used by default, matches old constexpr behavior)
    static constexpr std::size_t CRASH_HANDLER_SIZE = CRASH_HANDLER_SIZE_DEBUG;
    static constexpr std::size_t REPL_HANDLER_SIZE = REPL_HANDLER_SIZE_DEBUG;
    static constexpr std::size_t REPL_HANDLER_OFFSET = CRASH_HANDLER_OFFSET + CRASH_HANDLER_SIZE;
    static constexpr std::size_t STARTUP_CODE_SIZE = REPL_HANDLER_OFFSET + REPL_HANDLER_SIZE;

    // .data section layout offsets (after IAT + globals)
    // breakpoint table: 16 entries x 16 bytes = 256 bytes
    static constexpr std::size_t BP_TABLE_OFFSET_IN_DATA = 0x100;  // after iat + globals
    static constexpr std::size_t BP_TABLE_ENTRIES = 16;
    static constexpr std::size_t BP_TABLE_ENTRY_SIZE = 16;
    static constexpr std::size_t BP_TABLE_SIZE = BP_TABLE_ENTRIES * BP_TABLE_ENTRY_SIZE;
    // repl input buffer: 256 bytes
    static constexpr std::size_t REPL_BUFFER_OFFSET_IN_DATA = BP_TABLE_OFFSET_IN_DATA + BP_TABLE_SIZE;
    static constexpr std::size_t REPL_BUFFER_SIZE = 256;
    // repl state flags: 16 bytes
    static constexpr std::size_t REPL_STATE_OFFSET_IN_DATA = REPL_BUFFER_OFFSET_IN_DATA + REPL_BUFFER_SIZE;
    static constexpr std::size_t REPL_STATE_SIZE = 16;
    static constexpr std::size_t FREEZE_LATCH_OFFSET_IN_REPL_STATE = 2;
    static constexpr std::size_t REPL_DATA_TOTAL = BP_TABLE_SIZE + REPL_BUFFER_SIZE + REPL_STATE_SIZE;

    std::vector<std::uint8_t> generate(const GenerateConfig& cfg) {
        constexpr std::size_t kFileAlign = 0x200;
        constexpr std::size_t kSectAlign = 0x1000;
        constexpr std::size_t kHeaderSize = 0x400;
        
        // local copy of code since we patch it in place
        auto user_code = cfg.code;
        
        bool has_debug = !cfg.debug_source.empty();
        bool has_linemap = has_debug && !cfg.line_map.empty();
        bool debug_mode = cfg.healing_mode != ast::HealingMode::Off;
        
        // compute layout based on build config
        auto layout = compute_layout(debug_mode, cfg.exe_mode);
        
        constexpr std::size_t text_rva = 0x1000;
        std::size_t estimated_code_size = user_code.size() + layout.startup_code_size;
        if (estimated_code_size > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)())) {
            throw std::overflow_error("startup + user code exceeds rel32-safe range");
        }
        std::size_t text_pages = (estimated_code_size + kSectAlign - 1) / kSectAlign;
        
        // dynamic rvas based on code size
        std::size_t rdata_rva = text_rva + (text_pages * kSectAlign);
        std::size_t data_rva = rdata_rva + kSectAlign;
        std::size_t reloc_rva_slot = data_rva + kSectAlign;  // .reloc always present
        std::size_t src_rva = reloc_rva_slot + kSectAlign;   // .src section for debug
        std::size_t srcmap_rva = has_debug && has_linemap ? (src_rva + kSectAlign) : 0;
        
        // Patch IAT fixups in user_code before building the text section
        std::size_t user_code_offset = layout.startup_code_size;
        for (const auto& fixup : cfg.iat_fixups) {
            if (fixup.patch_site + 4 > user_code.size()) {
                throw std::logic_error("IAT fixup patch site extends past generated user code");
            }
            // iat lives in .data now, not .rdata
            std::size_t iat_rva = data_rva + fixup.iat_offset;
            // code position rva (patch_site is offset within user_code)
            std::size_t code_rva = text_rva + user_code_offset + fixup.patch_site + 4;
            // RIP-relative displacement
            std::int32_t disp = checked_rel32(
                static_cast<std::int64_t>(iat_rva) - static_cast<std::int64_t>(code_rva),
                "IAT fixup");

            user_code[fixup.patch_site + 0] = static_cast<std::uint8_t>(disp & 0xFF);
            user_code[fixup.patch_site + 1] = static_cast<std::uint8_t>((disp >> 8) & 0xFF);
            user_code[fixup.patch_site + 2] = static_cast<std::uint8_t>((disp >> 16) & 0xFF);
            user_code[fixup.patch_site + 3] = static_cast<std::uint8_t>((disp >> 24) & 0xFF);
        }
        
        auto imports = build_imports(rdata_rva, data_rva);
        auto text = build_code(user_code, cfg.main_offset, cfg.alloc_console, text_rva, rdata_rva, imports, srcmap_rva, has_debug ? src_rva : 0, data_rva, cfg.healing_mode, layout, cfg.exe_mode);
        
        std::size_t text_file_sz = align_up(text.size(), kFileAlign);
        std::size_t rdata_file_sz = align_up(imports.rdata.size(), kFileAlign);
        // iat lives at the start of .data now, globals come after, then debug data if needed
        std::size_t data_raw_size = imports.iat_data.size() + 0x100 + layout.data_extra;
        std::size_t data_file_sz = align_up(data_raw_size, kFileAlign);
        std::size_t src_file_sz = has_debug ? align_up(cfg.debug_source.size() + 1, kFileAlign) : 0;
        // line map: 8 bytes per entry (4 bytes offset + 4 bytes line)
        std::size_t srcmap_file_sz = has_linemap ? align_up(cfg.line_map.size() * 8 + 4, kFileAlign) : 0;
        
        // .reloc section - win11 requires this when DYNAMIC_BASE is set
        std::size_t reloc_rva = reloc_rva_slot;
        
        // minimal base relocation block: 8 byte header (VirtualAddress=0, SizeOfBlock=8)
        // tells the loader "no actual relocations" but satisfies the PE requirement
        constexpr std::size_t reloc_raw_size = 8;
        std::size_t reloc_file_sz = align_up(reloc_raw_size, kFileAlign);
        
        // image size = max aligned end across all emitted sections
        auto section_end = [&](std::size_t rva, std::size_t virtual_size) {
            return rva + align_up(virtual_size, kSectAlign);
        };

        std::size_t image_size = 0;
        image_size = (std::max)(image_size, section_end(text_rva, text.size()));
        image_size = (std::max)(image_size, section_end(rdata_rva, imports.rdata.size()));
        image_size = (std::max)(image_size, section_end(data_rva, data_raw_size));
        image_size = (std::max)(image_size, section_end(reloc_rva, reloc_raw_size));
        if (has_debug) {
            image_size = (std::max)(image_size, section_end(src_rva, cfg.debug_source.size() + 1));
        }
        if (has_linemap) {
            image_size = (std::max)(image_size, section_end(srcmap_rva, cfg.line_map.size() * 8 + 4));
        }
        if (image_size > static_cast<std::size_t>((std::numeric_limits<std::int32_t>::max)())) {
            throw std::overflow_error("image size exceeds rel32-safe range");
        }
        
        std::vector<std::uint8_t> dll;
        
        IMAGE_DOS_HEADER dos = {};
        dos.e_magic = IMAGE_DOS_SIGNATURE;
        dos.e_lfanew = 0x80;
        append(dll, dos);
        pad_to(dll, 0x80);
        
        append(dll, static_cast<DWORD>(IMAGE_NT_SIGNATURE));
        
        IMAGE_FILE_HEADER fh = {};
        fh.Machine = IMAGE_FILE_MACHINE_AMD64;
        // Section count: .text, .rdata, .data, .reloc, optionally .src, optionally .srcmap
        int num_sections = 4;  // always have .text, .rdata, .data, .reloc
        if (has_debug) num_sections++;
        if (has_linemap) num_sections++;
        fh.NumberOfSections = static_cast<WORD>(num_sections);
        fh.TimeDateStamp = static_cast<DWORD>(std::time(nullptr));
        fh.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        fh.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
        if (!cfg.exe_mode) fh.Characteristics |= IMAGE_FILE_DLL;
        append(dll, fh);
        
        IMAGE_OPTIONAL_HEADER64 opt = {};
        opt.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        opt.MajorLinkerVersion = 14;
        opt.SizeOfCode = static_cast<DWORD>(text_file_sz);
        opt.SizeOfInitializedData = static_cast<DWORD>(rdata_file_sz + data_file_sz + reloc_file_sz + src_file_sz + srcmap_file_sz);
        opt.AddressOfEntryPoint = static_cast<DWORD>(text_rva);
        opt.BaseOfCode = static_cast<DWORD>(text_rva);
        opt.ImageBase = 0x180000000ULL;
        opt.SectionAlignment = static_cast<DWORD>(kSectAlign);
        opt.FileAlignment = static_cast<DWORD>(kFileAlign);
        opt.MajorOperatingSystemVersion = 6;
        opt.MajorSubsystemVersion = 6;
        opt.SizeOfImage = static_cast<DWORD>(image_size);
        opt.SizeOfHeaders = static_cast<DWORD>(kHeaderSize);
        opt.Subsystem = cfg.exe_mode ? IMAGE_SUBSYSTEM_WINDOWS_CUI : IMAGE_SUBSYSTEM_WINDOWS_GUI;
        opt.DllCharacteristics = IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE | IMAGE_DLLCHARACTERISTICS_NX_COMPAT;
        opt.SizeOfStackReserve = 0x100000;
        opt.SizeOfStackCommit = 0x1000;
        opt.SizeOfHeapReserve = 0x100000;
        opt.SizeOfHeapCommit = 0x1000;
        opt.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
        
        opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = static_cast<DWORD>(rdata_rva);
        opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = 40;
        opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = static_cast<DWORD>(imports.iat_rva);
        opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = imports.iat_size;
        opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = static_cast<DWORD>(reloc_rva);
        opt.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = static_cast<DWORD>(reloc_raw_size);
        
        append(dll, opt);
        
        IMAGE_SECTION_HEADER text_sh = {};
        memcpy(text_sh.Name, ".text", 5);
        text_sh.Misc.VirtualSize = static_cast<DWORD>(text.size());
        text_sh.VirtualAddress = static_cast<DWORD>(text_rva);
        text_sh.SizeOfRawData = static_cast<DWORD>(text_file_sz);
        text_sh.PointerToRawData = static_cast<DWORD>(kHeaderSize);
        text_sh.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
        if (cfg.writable_text) text_sh.Characteristics |= IMAGE_SCN_MEM_WRITE;
        append(dll, text_sh);
        
        IMAGE_SECTION_HEADER rdata_sh = {};
        memcpy(rdata_sh.Name, ".rdata", 6);
        rdata_sh.Misc.VirtualSize = static_cast<DWORD>(imports.rdata.size());
        rdata_sh.VirtualAddress = static_cast<DWORD>(rdata_rva);
        rdata_sh.SizeOfRawData = static_cast<DWORD>(rdata_file_sz);
        rdata_sh.PointerToRawData = static_cast<DWORD>(kHeaderSize + text_file_sz);
        rdata_sh.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
        append(dll, rdata_sh);
        
        IMAGE_SECTION_HEADER data_sh = {};
        memcpy(data_sh.Name, ".data", 5);
        data_sh.Misc.VirtualSize = static_cast<DWORD>(data_raw_size);
        data_sh.VirtualAddress = static_cast<DWORD>(data_rva);
        data_sh.SizeOfRawData = static_cast<DWORD>(data_file_sz);
        data_sh.PointerToRawData = static_cast<DWORD>(kHeaderSize + text_file_sz + rdata_file_sz);
        data_sh.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
        append(dll, data_sh);
        
        IMAGE_SECTION_HEADER reloc_sh = {};
        memcpy(reloc_sh.Name, ".reloc", 6);
        reloc_sh.Misc.VirtualSize = static_cast<DWORD>(reloc_raw_size);
        reloc_sh.VirtualAddress = static_cast<DWORD>(reloc_rva);
        reloc_sh.SizeOfRawData = static_cast<DWORD>(reloc_file_sz);
        reloc_sh.PointerToRawData = static_cast<DWORD>(kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz);
        reloc_sh.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_DISCARDABLE | IMAGE_SCN_MEM_READ;
        append(dll, reloc_sh);
        
        // debug section
        if (has_debug) {
            IMAGE_SECTION_HEADER src_sh = {};
            memcpy(src_sh.Name, ".src", 4);
            src_sh.Misc.VirtualSize = static_cast<DWORD>(cfg.debug_source.size() + 1);
            src_sh.VirtualAddress = static_cast<DWORD>(src_rva);
            src_sh.SizeOfRawData = static_cast<DWORD>(src_file_sz);
            src_sh.PointerToRawData = static_cast<DWORD>(kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz + reloc_file_sz);
            src_sh.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            append(dll, src_sh);
        }
        
        // line map section
        if (has_linemap) {
            IMAGE_SECTION_HEADER srcmap_sh = {};
            memcpy(srcmap_sh.Name, ".srcmap", 7);
            srcmap_sh.Misc.VirtualSize = static_cast<DWORD>(cfg.line_map.size() * 8 + 4);
            srcmap_sh.VirtualAddress = static_cast<DWORD>(srcmap_rva);
            srcmap_sh.SizeOfRawData = static_cast<DWORD>(srcmap_file_sz);
            srcmap_sh.PointerToRawData = static_cast<DWORD>(kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz + reloc_file_sz + src_file_sz);
            srcmap_sh.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            append(dll, srcmap_sh);
        }
        
        pad_to(dll, kHeaderSize);
        dll.insert(dll.end(), text.begin(), text.end());
        pad_to(dll, kHeaderSize + text_file_sz);
        dll.insert(dll.end(), imports.rdata.begin(), imports.rdata.end());
        pad_to(dll, kHeaderSize + text_file_sz + rdata_file_sz);
        // .data - iat goes at the start, rest is zeroed globals
        dll.insert(dll.end(), imports.iat_data.begin(), imports.iat_data.end());
        pad_to(dll, kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz);
        
        // .reloc - minimal base relocation block
        // VirtualAddress = 0x1000 (.text), SizeOfBlock = 8 (header only, no entries)
        emit32(dll, static_cast<std::uint32_t>(text_rva));
        emit32(dll, 8);
        pad_to(dll, kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz + reloc_file_sz);
        
        // .src - embedded source for crash handler
        if (has_debug) {
            for (char c : cfg.debug_source) {
                dll.push_back(static_cast<std::uint8_t>(c));
            }
            dll.push_back(0);
            pad_to(dll, kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz + reloc_file_sz + src_file_sz);
        }
        
        // .srcmap - instruction offset to line number mapping
        if (has_linemap) {
            std::uint32_t count = static_cast<std::uint32_t>(cfg.line_map.size());
            dll.push_back(static_cast<std::uint8_t>(count & 0xFF));
            dll.push_back(static_cast<std::uint8_t>((count >> 8) & 0xFF));
            dll.push_back(static_cast<std::uint8_t>((count >> 16) & 0xFF));
            dll.push_back(static_cast<std::uint8_t>((count >> 24) & 0xFF));
            
            for (const auto& entry : cfg.line_map) {
                dll.push_back(static_cast<std::uint8_t>(entry.code_offset & 0xFF));
                dll.push_back(static_cast<std::uint8_t>((entry.code_offset >> 8) & 0xFF));
                dll.push_back(static_cast<std::uint8_t>((entry.code_offset >> 16) & 0xFF));
                dll.push_back(static_cast<std::uint8_t>((entry.code_offset >> 24) & 0xFF));
                dll.push_back(static_cast<std::uint8_t>(entry.line & 0xFF));
                dll.push_back(static_cast<std::uint8_t>((entry.line >> 8) & 0xFF));
                dll.push_back(static_cast<std::uint8_t>((entry.line >> 16) & 0xFF));
                dll.push_back(static_cast<std::uint8_t>((entry.line >> 24) & 0xFF));
            }
            pad_to(dll, kHeaderSize + text_file_sz + rdata_file_sz + data_file_sz + reloc_file_sz + src_file_sz + srcmap_file_sz);
        }
        
        return dll;
    }
    
    // validates a raw PE blob and returns any structural issues it finds
    static std::vector<std::string> validate_pe(const std::vector<std::uint8_t>& pe_data) {
        std::vector<std::string> errors;
        auto range_fits = [](std::size_t base, std::size_t size, std::size_t total) {
            return base <= total && size <= total - base;
        };
        
        // need at least a dos header to do anything
        if (pe_data.size() < sizeof(IMAGE_DOS_HEADER)) {
            errors.push_back("PE data too small for DOS header (got " + std::to_string(pe_data.size()) + " bytes)");
            return errors;
        }
        
        IMAGE_DOS_HEADER dos{};
        std::memcpy(&dos, pe_data.data(), sizeof(dos));
        
        if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
            errors.push_back("invalid DOS signature: 0x" + to_hex16(dos.e_magic) + " (expected 0x5A4D)");
            return errors;
        }
        
        // make sure we can read pe sig + file header + optional header
        if (dos.e_lfanew < 0) {
            errors.push_back("negative e_lfanew in DOS header");
            return errors;
        }

        std::size_t pe_offset = static_cast<std::size_t>(dos.e_lfanew);
        std::size_t nt_header_span = sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64);
        if (!range_fits(pe_offset, nt_header_span, pe_data.size())) {
            errors.push_back("PE headers extend past end of data (offset " + std::to_string(pe_offset) + ", span " + std::to_string(nt_header_span) + ", got " + std::to_string(pe_data.size()) + ")");
            return errors;
        }
        
        DWORD pe_sig{};
        std::memcpy(&pe_sig, pe_data.data() + pe_offset, sizeof(pe_sig));
        if (pe_sig != IMAGE_NT_SIGNATURE) {
            errors.push_back("invalid PE signature: 0x" + to_hex32(pe_sig) + " (expected 0x00004550)");
            return errors;
        }
        
        IMAGE_FILE_HEADER fh{};
        std::memcpy(&fh, pe_data.data() + pe_offset + 4, sizeof(fh));
        
        IMAGE_OPTIONAL_HEADER64 opt{};
        std::memcpy(&opt, pe_data.data() + pe_offset + 4 + sizeof(IMAGE_FILE_HEADER), sizeof(opt));
        
        DWORD file_align = opt.FileAlignment;
        DWORD sect_align = opt.SectionAlignment;
        if (file_align == 0) {
            errors.push_back("FileAlignment is zero");
            return errors;
        }
        if (sect_align == 0) {
            errors.push_back("SectionAlignment is zero");
            return errors;
        }
        
        // SizeOfHeaders should be aligned to FileAlignment
        if (opt.SizeOfHeaders % file_align != 0) {
            errors.push_back("SizeOfHeaders (0x" + to_hex32(opt.SizeOfHeaders) + ") not aligned to FileAlignment (0x" + to_hex32(file_align) + ")");
        }
        
        // check that SizeOfHeaders covers all the actual header content
        std::size_t actual_headers = pe_offset + 4 + sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64)
                                   + fh.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
        if (opt.SizeOfHeaders < actual_headers) {
            errors.push_back("SizeOfHeaders (0x" + to_hex32(opt.SizeOfHeaders) + ") smaller than actual headers (0x" + to_hex32(static_cast<DWORD>(actual_headers)) + ")");
        }
        
        // os version checks - win10/11 need at least 6.0
        if (opt.MajorOperatingSystemVersion < 6) {
            errors.push_back("MajorOperatingSystemVersion (" + std::to_string(opt.MajorOperatingSystemVersion) + ") < 6");
        }
        if (opt.MajorSubsystemVersion < 6) {
            errors.push_back("MajorSubsystemVersion (" + std::to_string(opt.MajorSubsystemVersion) + ") < 6");
        }
        
        // parse section headers
        std::size_t section_table_offset = pe_offset + 4 + sizeof(IMAGE_FILE_HEADER) + fh.SizeOfOptionalHeader;
        std::vector<IMAGE_SECTION_HEADER> sections(fh.NumberOfSections);
        
        for (int i = 0; i < fh.NumberOfSections; i++) {
            std::size_t off = section_table_offset + i * sizeof(IMAGE_SECTION_HEADER);
            if (off + sizeof(IMAGE_SECTION_HEADER) > pe_data.size()) {
                errors.push_back("section header " + std::to_string(i) + " extends past end of data");
                return errors;
            }
            std::memcpy(&sections[i], pe_data.data() + off, sizeof(IMAGE_SECTION_HEADER));
        }
        
        DWORD prev_rva = 0;
        for (int i = 0; i < static_cast<int>(sections.size()); i++) {
            const auto& s = sections[i];
            char name[9]{};
            std::memcpy(name, s.Name, 8);
            
            // rva alignment
            if (s.VirtualAddress % sect_align != 0) {
                errors.push_back("section " + std::string(name) + " VirtualAddress (0x" + to_hex32(s.VirtualAddress) + ") not aligned to SectionAlignment (0x" + to_hex32(sect_align) + ")");
            }
            
            // file offset alignment (0 is ok, means uninitialized)
            if (s.PointerToRawData != 0 && s.PointerToRawData % file_align != 0) {
                errors.push_back("section " + std::string(name) + " PointerToRawData (0x" + to_hex32(s.PointerToRawData) + ") not aligned to FileAlignment (0x" + to_hex32(file_align) + ")");
            }
            
            // monotonically increasing rvas
            if (i > 0 && s.VirtualAddress < prev_rva) {
                errors.push_back("section " + std::string(name) + " VirtualAddress (0x" + to_hex32(s.VirtualAddress) + ") < previous section RVA (0x" + to_hex32(prev_rva) + ") - not monotonically increasing");
            }
            prev_rva = s.VirtualAddress;
        }
        
        // SizeOfImage should match last section rva + aligned virtual size
        if (!sections.empty()) {
            const auto& last = sections.back();
            DWORD expected_image_size = last.VirtualAddress + val_align_up(last.Misc.VirtualSize, sect_align);
            if (opt.SizeOfImage != expected_image_size) {
                errors.push_back("SizeOfImage (0x" + to_hex32(opt.SizeOfImage) + ") does not match expected (0x" + to_hex32(expected_image_size) + ")");
            }
        }
        
        // validate data directory entries point into valid sections
        // check IMPORT, IAT, BASERELOC
        struct DirCheck {
            DWORD index;
            const char* name;
        };
        DirCheck dirs[] = {
            { IMAGE_DIRECTORY_ENTRY_IMPORT, "IMPORT" },
            { IMAGE_DIRECTORY_ENTRY_IAT, "IAT" },
            { IMAGE_DIRECTORY_ENTRY_BASERELOC, "BASERELOC" },
        };
        
        for (const auto& d : dirs) {
            if (d.index >= opt.NumberOfRvaAndSizes) continue;
            const auto& entry = opt.DataDirectory[d.index];
            if (entry.Size == 0) continue;
            
            bool found = false;
            for (const auto& s : sections) {
                std::size_t section_start = s.VirtualAddress;
                std::size_t section_span = (std::max)(static_cast<std::size_t>(s.Misc.VirtualSize),
                                                      static_cast<std::size_t>(s.SizeOfRawData));
                if (section_span == 0) continue;
                std::size_t section_end = section_start + section_span;
                std::size_t dir_start = entry.VirtualAddress;
                std::size_t dir_end = dir_start + entry.Size;
                if (section_end < section_start || dir_end < dir_start) {
                    continue;
                }
                if (dir_start >= section_start && dir_end <= section_end) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                errors.push_back(std::string(d.name) + " directory span [0x" + to_hex32(entry.VirtualAddress) + ", 0x" + to_hex32(entry.VirtualAddress + entry.Size) + ") does not fit within any section");
            }
        }
        
        return errors;
    }
    
private:
    // hex formatting helpers for validate_pe error messages
    static std::string to_hex16(std::uint16_t v) {
        return std::format("{:04X}", v);
    }
    
    static std::string to_hex32(std::uint32_t v) {
        return std::format("{:08X}", v);
    }
    
    // align up helper specifically for validation (doesnt collide with the existing one)
    static DWORD val_align_up(DWORD v, DWORD a) { return (v + a - 1) & ~(a - 1); }

    static std::int32_t checked_rel32(std::int64_t delta, const char* what) {
        if (delta < (std::numeric_limits<std::int32_t>::min)() ||
            delta > (std::numeric_limits<std::int32_t>::max)()) {
            throw std::overflow_error(std::string(what) + " does not fit in rel32");
        }
        return static_cast<std::int32_t>(delta);
    }

    static std::uint32_t checked_u32(std::size_t value, const char* what) {
        if (value > (std::numeric_limits<std::uint32_t>::max)()) {
            throw std::overflow_error(std::string(what) + " does not fit in u32");
        }
        return static_cast<std::uint32_t>(value);
    }
    
    struct ImportResult {
        std::vector<std::uint8_t> rdata;     // import descriptors, ILT, dll name, hint/names, strings
        std::vector<std::uint8_t> iat_data;  // just the IAT thunks, goes in .data
        std::size_t iat_rva;
        std::size_t iat_size;
        std::size_t iat_create_thread;
        std::size_t iat_alloc_console;
        std::size_t iat_set_title;
        std::size_t iat_get_std_handle;
        std::size_t iat_write_console;
        std::size_t iat_sleep;
        std::size_t iat_add_veh;
        std::size_t str_title_rva;
    };
    
    ImportResult build_imports(std::size_t rdata_rva, std::size_t data_rva) {
        ImportResult result;
        
        // layout derived from iat_func_count:
        // .rdata:
        //   0x00-0x27: import descriptors (kernel32 + null terminator)
        //   0x28+:     ILT (iat_func_count entries + null)
        //   after ILT: "kernel32.dll" string
        //   after dll name: hint+name entries
        //   after that: strings
        // .data (iat_data):
        //   0x00+: IAT (same entry count as ILT)
        
        constexpr std::size_t ILT_OFFSET = 0x28;
        const std::size_t ilt_entries = iat_func_count + 1;  // funcs + null terminator
        const std::size_t ilt_size = ilt_entries * 8;
        const std::size_t dll_name_offset = ILT_OFFSET + ilt_size;
        const std::size_t hint_base = align_up(dll_name_offset + 16, 8);  // after "kernel32.dll" padded
        
        // iat lives at the start of .data
        result.iat_rva = data_rva;
        result.iat_size = static_cast<std::uint32_t>(ilt_size);
        result.iat_create_thread   = data_rva + iat_slot(iat_idx::CreateThread);
        result.iat_alloc_console   = data_rva + iat_slot(iat_idx::AllocConsole);
        result.iat_set_title       = data_rva + iat_slot(iat_idx::SetConsoleTitleA);
        result.iat_get_std_handle  = data_rva + iat_slot(iat_idx::GetStdHandle);
        result.iat_write_console   = data_rva + iat_slot(iat_idx::WriteConsoleA);
        result.iat_sleep           = data_rva + iat_slot(iat_idx::Sleep);
        result.iat_add_veh         = data_rva + iat_slot(iat_idx::AddVectoredExceptionHandler);
        
        // first pass: figure out how big rdata needs to be
        std::size_t offset = hint_base;
        std::size_t func_offsets[iat_func_count];
        for (std::size_t i = 0; i < iat_func_count; i++) {
            func_offsets[i] = offset;
            offset += 2 + strlen(iat_functions[i]) + 1;
            if (offset % 2) offset++;
        }
        // strings area
        std::size_t str_title_off = align_up(offset, 8);
        std::size_t rdata_needed = str_title_off + 0x40;  // some padding for strings
        
        result.rdata.resize(rdata_needed, 0);
        
        // write hint+name entries
        for (std::size_t i = 0; i < iat_func_count; i++) {
            std::uint16_t hint = 0;
            memcpy(result.rdata.data() + func_offsets[i], &hint, 2);
            memcpy(result.rdata.data() + func_offsets[i] + 2, iat_functions[i], strlen(iat_functions[i]) + 1);
        }
        
        // ILT - points into rdata hint+name entries
        std::vector<std::uint64_t> thunks(ilt_entries, 0);
        for (std::size_t i = 0; i < iat_func_count; i++) {
            thunks[i] = rdata_rva + func_offsets[i];
        }
        memcpy(result.rdata.data() + ILT_OFFSET, thunks.data(), ilt_size);
        
        // IAT goes into separate iat_data buffer (same content as ILT before loader patches it)
        result.iat_data.resize(ilt_size, 0);
        memcpy(result.iat_data.data(), thunks.data(), ilt_size);
        
        // dll name
        memcpy(result.rdata.data() + dll_name_offset, "kernel32.dll", 13);
        
        // import descriptor - FirstThunk points into .data
        IMAGE_IMPORT_DESCRIPTOR desc = {};
        desc.OriginalFirstThunk = static_cast<DWORD>(rdata_rva + ILT_OFFSET);
        desc.Name = static_cast<DWORD>(rdata_rva + dll_name_offset);
        desc.FirstThunk = static_cast<DWORD>(data_rva);
        memcpy(result.rdata.data(), &desc, sizeof(desc));
        
        // string rvas
        result.str_title_rva = rdata_rva + str_title_off;
        memset(result.rdata.data() + str_title_off, 0, 32);
        
        return result;
    }
    
    std::vector<std::uint8_t> build_code(
        const std::vector<std::uint8_t>& user_code,
        std::size_t main_offset,
        bool alloc_console,
        std::size_t text_rva,
        std::size_t rdata_rva,
        const ImportResult& imports,
        std::size_t srcmap_rva = 0,
        std::size_t src_rva = 0,
        std::size_t data_rva = 0,
        ast::HealingMode healing_mode = ast::HealingMode::Off,
        const Layout& layout = compute_layout(true),
        bool exe_mode = false
    ) {
        std::vector<std::uint8_t> code;
        
        // user code starts after all startup slots
        std::size_t user_code_offset = layout.startup_code_size;
        
        // offsets for runtime routines depend on mode
        std::size_t print_off = exe_mode ? EXE_PRINT_OFFSET : DLL_PRINT_OFFSET;
        std::size_t set_title_off = exe_mode ? EXE_SET_TITLE_OFFSET : DLL_SET_TITLE_OFFSET;
        std::size_t alloc_console_off = exe_mode ? EXE_ALLOC_CONSOLE_OFFSET : DLL_ALLOC_CONSOLE_OFFSET;
        std::size_t print_hex_off = exe_mode ? EXE_PRINT_HEX_OFFSET : DLL_PRINT_HEX_OFFSET;
        std::size_t crash_handler_off = exe_mode ? EXE_CRASH_HANDLER_OFFSET : CRASH_HANDLER_OFFSET;
        
        if (exe_mode) {
            // exe entry: call main, pass return value to ExitProcess
            emit8(code, 0x55);                                     // push rbp
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);    // mov rbp, rsp
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x20);  // sub rsp, 0x20
            if (healing_mode != ast::HealingMode::Off) {
                emit_install_veh(code, text_rva, imports, crash_handler_off);
            }
            
            // call main
            std::size_t call_pos = code.size();
            std::int32_t main_rel = checked_rel32(
                static_cast<std::int64_t>(user_code_offset + main_offset) - static_cast<std::int64_t>(call_pos + 5),
                "exe main call");
            emit8(code, 0xE8);
            emit32(code, static_cast<std::uint32_t>(main_rel));
            
            // mov ecx, eax (return value -> ExitProcess arg)
            emit8(code, 0x89); emit8(code, 0xC1);
            
            // call [rip+ExitProcess]
            std::size_t exit_rip = text_rva + code.size() + 6;
            std::size_t exit_iat = data_rva + iat_slot(iat_idx::ExitProcess);
            std::int32_t exit_rel = checked_rel32(
                static_cast<std::int64_t>(exit_iat) - static_cast<std::int64_t>(exit_rip),
                "ExitProcess IAT call");
            emit8(code, 0xFF); emit8(code, 0x15);
            emit32(code, static_cast<std::uint32_t>(exit_rel));
            
            emit8(code, 0xCC);  // int3 - should never reach
            
            while (code.size() < EXE_ENTRY_SIZE) emit8(code, 0xCC);
        } else {
            // === DllMain ===
            emit8(code, 0x55);  // push rbp
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);  // mov rbp, rsp
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x30);  // sub rsp, 0x30
            
            // save hModule (rcx) before we trash it setting up CreateThread args
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x4D); emit8(code, 0xF8);  // mov [rbp-8], rcx
            
            emit8(code, 0x83); emit8(code, 0xFA); emit8(code, 0x01);  // cmp edx, 1
            emit8(code, 0x75);
            std::size_t jne_fixup = code.size();
            emit8(code, 0x00);
            
            if (alloc_console) {
                // set up CreateThread to run users main
                emit8(code, 0x31); emit8(code, 0xC9);  // xor ecx, ecx
                emit8(code, 0x31); emit8(code, 0xD2);  // xor edx, edx
                
                std::size_t lea_pos = code.size();
                std::int32_t rel = checked_rel32(
                    static_cast<std::int64_t>(DLLMAIN_SIZE) - static_cast<std::int64_t>(lea_pos + 7),
                    "DllMain thread lea");
                emit8(code, 0x4C); emit8(code, 0x8D); emit8(code, 0x05);  // lea r8, [rip+...]
                emit32(code, static_cast<std::uint32_t>(rel));
                
                // lpParameter = hModule (saved on stack earlier)
                emit8(code, 0x4C); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF8);  // mov r9, [rbp-8]
                emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x44); emit8(code, 0x24); emit8(code, 0x20);
                emit32(code, 0);
                emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x44); emit8(code, 0x24); emit8(code, 0x28);
                emit32(code, 0);
                
                emit_call_iat(code, text_rva, imports.iat_create_thread);
            }
            
            code[jne_fixup] = static_cast<std::uint8_t>(code.size() - jne_fixup - 1);
            emit8(code, 0xB8); emit32(code, 1);  // mov eax, 1
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC4); emit8(code, 0x30);  // add rsp, 0x30
            emit8(code, 0x5D);  // pop rbp
            emit8(code, 0xC3);  // ret
            
            while (code.size() < DLLMAIN_SIZE) emit8(code, 0xCC);
            
            // === thread entry - receives hModule via lpParameter, calls user main ===
            emit8(code, 0x55);  // push rbp
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);  // mov rbp, rsp
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x20);  // sub rsp, 0x20
            if (healing_mode != ast::HealingMode::Off) {
                emit_install_veh(code, text_rva, imports, crash_handler_off);
            }
            
            // call user main
            std::size_t call_pos = code.size();
            std::int32_t main_rel = checked_rel32(
                static_cast<std::int64_t>(user_code_offset + main_offset) - static_cast<std::int64_t>(call_pos + 5),
                "dll thread main call");
            emit8(code, 0xE8);
            emit32(code, static_cast<std::uint32_t>(main_rel));
            
            emit8(code, 0x31); emit8(code, 0xC0);  // xor eax, eax
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC4); emit8(code, 0x20);  // add rsp, 0x20
            emit8(code, 0x5D);  // pop rbp
            emit8(code, 0xC3);  // ret
            
            while (code.size() < DLL_PRINT_OFFSET) emit8(code, 0xCC);
        }
        
        // runtime routines - same code, just at different offsets depending on mode
        while (code.size() < print_off) emit8(code, 0xCC);
        
        // === print_impl ===
        std::size_t print_start = code.size();
        build_dll_print_impl(code, text_rva, rdata_rva, imports);
        std::size_t print_bytes = code.size() - print_start;
        
        while (code.size() < set_title_off) emit8(code, 0xCC);
        
        // === set_title_impl ===
        std::size_t title_start = code.size();
        build_dll_set_title_impl(code, text_rva, rdata_rva, imports);
        std::size_t title_bytes = code.size() - title_start;
        
        while (code.size() < alloc_console_off) emit8(code, 0xCC);
        
        // === alloc_console_impl ===
        std::size_t alloc_start = code.size();
        build_dll_alloc_console_impl(code, text_rva, rdata_rva, imports);
        std::size_t alloc_bytes = code.size() - alloc_start;
        
        while (code.size() < print_hex_off) emit8(code, 0xCC);
        
        // === print_hex_impl ===
        std::size_t hex_start = code.size();
        build_dll_print_hex_impl(code, text_rva, rdata_rva, imports);
        std::size_t hex_bytes = code.size() - hex_start;
        if (hex_bytes > DLL_PRINT_HEX_SIZE) {
            std::println(std::cerr, "\033[1;91mFATAL:\033[0m print_hex_impl is {} bytes but slot is only {} bytes", hex_bytes, DLL_PRINT_HEX_SIZE);
            std::abort();
        }
        
        while (code.size() < crash_handler_off) emit8(code, 0xCC);
        
        // === crash handler (VEH) ===
        std::size_t crash_start = code.size();
        if (healing_mode != ast::HealingMode::Off) {
            build_crash_handler(code, text_rva, rdata_rva, imports, user_code_offset, print_off, print_hex_off, srcmap_rva, src_rva, data_rva, crash_handler_off, layout.crash_handler_size, healing_mode);
        } else {
            // release mode: catch crash, print info, freeze
            emit8(code, 0x53);                                                     // push rbx
            emit8(code, 0x56);                                                     // push rsi
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x28);    // sub rsp, 0x28

            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x01);                    // mov rax, [rcx] (EXCEPTION_RECORD*)
            emit8(code, 0x8B); emit8(code, 0x18);                                     // mov ebx, [rax] (ExceptionCode)
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x41); emit8(code, 0x08);    // mov rax, [rcx+8] (CONTEXT*)
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0xB0);                    // mov rsi, [rax+0xF8] (RIP)
            emit32(code, 0xF8);

            emit_print_inline(code, text_rva, print_off, "CRASH 0x\0", 9);

            emit8(code, 0x89); emit8(code, 0xD9);                                     // mov ecx, ebx
            std::size_t hex_call1 = code.size();
            std::int32_t hex_rel1 = static_cast<std::int32_t>((text_rva + print_hex_off) - (text_rva + hex_call1 + 5));
            emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(hex_rel1));

            emit_print_inline(code, text_rva, print_off, " @0x\0", 5);

            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xF1);                    // mov rcx, rsi
            std::size_t hex_call2 = code.size();
            std::int32_t hex_rel2 = static_cast<std::int32_t>((text_rva + print_hex_off) - (text_rva + hex_call2 + 5));
            emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(hex_rel2));

            emit_print_inline(code, text_rva, print_off, "\n\0", 2);

            // freeze so the user can see the output
            emit8(code, 0xEB); emit8(code, 0xFE);                                     // jmp $
        }
        std::size_t crash_bytes = code.size() - crash_start;
        
        while (code.size() < user_code_offset) emit8(code, 0xCC);

        // slot usage report
        std::println(std::cerr, "\033[36m[pe]\033[0m slot usage ({}):", exe_mode ? "exe" : "dll");
        std::println(std::cerr, "  print_impl:    {:>4}/{} bytes", print_bytes, DLL_PRINT_SIZE);
        std::println(std::cerr, "  set_title:     {:>4}/{} bytes", title_bytes, DLL_SET_TITLE_SIZE);
        std::println(std::cerr, "  alloc_console: {:>4}/{} bytes", alloc_bytes, DLL_ALLOC_CONSOLE_SIZE);
        std::println(std::cerr, "  print_hex:     {:>4}/{} bytes", hex_bytes, DLL_PRINT_HEX_SIZE);
        std::println(std::cerr, "  crash_handler: {:>4}/{} bytes", crash_bytes, layout.crash_handler_size);
        std::size_t total_alloc = layout.startup_code_size;
        std::println(std::cerr, "  startup total: {:>4} bytes (0x{:X})", total_alloc, total_alloc);
        
        // === user code ===
        code.insert(code.end(), user_code.begin(), user_code.end());
        return code;
    }
    
    // Embedded dll_print - rcx = null-terminated string
    void build_dll_print_impl(std::vector<std::uint8_t>& code, std::size_t text_rva, 
                               std::size_t rdata_rva, const ImportResult& imports) {
        // push rbp; mov rbp, rsp; sub rsp, 0x30
        emit8(code, 0x55);
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x30);
        
        // Save string ptr in [rbp-8]
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x4D); emit8(code, 0xF8);  // mov [rbp-8], rcx
        
        // GetStdHandle(-11) -> stdout handle
        emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0xC1);  // mov rcx, -11
        emit32(code, 0xFFFFFFF5);
        emit_call_iat(code, text_rva, imports.iat_get_std_handle);
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x45); emit8(code, 0xF0);  // mov [rbp-0x10], rax (handle)
        
        // Calculate string length (inline strlen)
        // rax = string pointer, r8d = length counter
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xF8);  // mov rax, [rbp-8] (string ptr)
        emit8(code, 0x45); emit8(code, 0x31); emit8(code, 0xC0);  // xor r8d, r8d (length = 0)
        
        // .loop:
        std::size_t loop_start = code.size();
        emit8(code, 0x44); emit8(code, 0x8A); emit8(code, 0x08);  // mov r9b, [rax] (load byte into r9b) - 3 bytes
        emit8(code, 0x45); emit8(code, 0x84); emit8(code, 0xC9);  // test r9b, r9b - 3 bytes
        // je .done (jump past: 3+3+2 = 8 bytes)
        emit8(code, 0x74); emit8(code, 0x08);  // je +8
        emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xC0);  // inc rax - 3 bytes
        emit8(code, 0x41); emit8(code, 0xFF); emit8(code, 0xC0);  // inc r8d - 3 bytes
        // jmp .loop (back to loop_start)
        emit8(code, 0xEB);  // jmp short
        std::ptrdiff_t jmp_back_dist = static_cast<std::ptrdiff_t>(loop_start) - static_cast<std::ptrdiff_t>(code.size() + 1);
        assert(jmp_back_dist >= -128 && jmp_back_dist <= 127 && "strlen loop jmp doesnt fit in rel8");
        std::int8_t jmp_back = static_cast<std::int8_t>(jmp_back_dist);
        emit8(code, static_cast<std::uint8_t>(jmp_back));
        // .done: r8 now contains the length
        
        // WriteConsoleA(handle, str, len, NULL, NULL)
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10] (handle)
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x55); emit8(code, 0xF8);  // mov rdx, [rbp-8] (string)
        // r8 already has the length!
        emit8(code, 0x45); emit8(code, 0x31); emit8(code, 0xC9);  // xor r9d, r9d
        emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x44); emit8(code, 0x24); emit8(code, 0x20);
        emit32(code, 0);
        emit_call_iat(code, text_rva, imports.iat_write_console);

        // add rsp, 0x30; pop rbp; ret
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC4); emit8(code, 0x30);
        emit8(code, 0x5D);
        emit8(code, 0xC3);
    }
    
    // dll_set_title_impl - RCX = string pointer
    void build_dll_set_title_impl(std::vector<std::uint8_t>& code, std::size_t text_rva, 
                                   std::size_t rdata_rva, const ImportResult& imports) {
        emit8(code, 0x55);  // push rbp
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);  // mov rbp, rsp
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x20);  // sub rsp, 0x20
        
        // RCX already has the string pointer, just call SetConsoleTitleA
        emit_call_iat(code, text_rva, imports.iat_set_title);
        
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC4); emit8(code, 0x20);  // add rsp, 0x20
        emit8(code, 0x5D);  // pop rbp
        emit8(code, 0xC3);  // ret
    }
    
    // dll_alloc_console_impl - no arguments, allocates a console window
    void build_dll_alloc_console_impl(std::vector<std::uint8_t>& code, std::size_t text_rva, 
                                       std::size_t rdata_rva, const ImportResult& imports) {
        emit8(code, 0x55);  // push rbp
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);  // mov rbp, rsp
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x20);  // sub rsp, 0x20
        
        // Call AllocConsole()
        emit_call_iat(code, text_rva, imports.iat_alloc_console);
        
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC4); emit8(code, 0x20);  // add rsp, 0x20
        emit8(code, 0x5D);  // pop rbp
        emit8(code, 0xC3);  // ret
    }
    
    void emit_call_iat(std::vector<std::uint8_t>& code, std::size_t text_rva, std::size_t iat_entry_rva) {
        std::size_t rip_after = text_rva + code.size() + 6;
        std::int32_t rel32 = checked_rel32(
            static_cast<std::int64_t>(iat_entry_rva) - static_cast<std::int64_t>(rip_after),
            "IAT call");
        emit8(code, 0xFF); emit8(code, 0x15);
        emit32(code, static_cast<std::uint32_t>(rel32));
    }
    
    void emit_lea_rcx(std::vector<std::uint8_t>& code, std::size_t text_rva, std::size_t target_rva) {
        std::size_t rip_after = text_rva + code.size() + 7;
        std::int32_t rel32 = checked_rel32(
            static_cast<std::int64_t>(target_rva) - static_cast<std::int64_t>(rip_after),
            "lea rcx");
        emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0x0D);
        emit32(code, static_cast<std::uint32_t>(rel32));
    }
    
    void emit_lea_rdx(std::vector<std::uint8_t>& code, std::size_t text_rva, std::size_t target_rva) {
        std::size_t rip_after = text_rva + code.size() + 7;
        std::int32_t rel32 = checked_rel32(
            static_cast<std::int64_t>(target_rva) - static_cast<std::int64_t>(rip_after),
            "lea rdx");
        emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0x15);
        emit32(code, static_cast<std::uint32_t>(rel32));
    }

    void emit_install_veh(std::vector<std::uint8_t>& code,
                          std::size_t text_rva,
                          const ImportResult& imports,
                          std::size_t crash_handler_off) {
        emit8(code, 0xB9);
        emit32(code, 1);
        emit_lea_rdx(code, text_rva, text_rva + crash_handler_off);
        emit_call_iat(code, text_rva, imports.iat_add_veh);
    }
    
    // Print 64-bit value as "0x" + hex digits
    // rcx = 64-bit value, prints "0x" + 16 uppercase hex digits to console
    // stack layout (rbp-relative):
    // print_hex_impl v2 - prints 16 raw hex digits (no "0x" prefix, caller adds that)
    // rcx = 64-bit value
    // stack layout:
    //   [rbp-0x08] = saved value
    //   [rbp-0x10] = stdout handle
    //   [rbp-0x22]..[rbp-0x13] = 16 hex digit buffer
    //   [rbp-0x12] = null terminator
    //   rsp = rbp - 0x50
    void build_dll_print_hex_impl(std::vector<std::uint8_t>& code, std::size_t text_rva, 
                                   std::size_t rdata_rva, const ImportResult& imports) {
        emit8(code, 0x55);                                     // push rbp
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);    // mov rbp, rsp
        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xEC); emit8(code, 0x50);  // sub rsp, 0x50
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x4D); emit8(code, 0xF8);  // mov [rbp-8], rcx

        // GetStdHandle(-11)
        emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0xC1);    // mov rcx, -11
        emit32(code, 0xFFFFFFF5);
        emit_call_iat(code, text_rva, imports.iat_get_std_handle);
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x45); emit8(code, 0xF0);  // mov [rbp-0x10], rax

        // buffer at [rbp-0x22]: 16 hex digits + null (no "0x" prefix, caller handles that)
        // reload value into r8
        emit8(code, 0x4C); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xF8);  // mov r8, [rbp-8]

        for (int i = 0; i < 16; i++) {
            emit8(code, 0x4C); emit8(code, 0x89); emit8(code, 0xC0);                // mov rax, r8
            emit8(code, 0x48); emit8(code, 0xC1); emit8(code, 0xE8); emit8(code, 0x3C); // shr rax, 60
            emit8(code, 0x3C); emit8(code, 0x0A);                                  // cmp al, 10
            emit8(code, 0x72); emit8(code, 0x04);                                  // jb .digit
            emit8(code, 0x04); emit8(code, 0x37);                                  // add al, 0x37
            emit8(code, 0xEB); emit8(code, 0x02);                                  // jmp .store
            emit8(code, 0x04); emit8(code, 0x30);                                  // add al, 0x30
            // digit i goes at [rbp - 0x22 + i]
            int8_t buf_off = static_cast<int8_t>(-0x22 + i);
            emit8(code, 0x88); emit8(code, 0x45); emit8(code, static_cast<uint8_t>(buf_off));
            emit8(code, 0x49); emit8(code, 0xC1); emit8(code, 0xE0); emit8(code, 0x04); // shl r8, 4
        }

        // null terminate: [rbp - 0x22 + 16] = [rbp - 0x12]
        emit8(code, 0xC6); emit8(code, 0x45); emit8(code, 0xEE); emit8(code, 0x00);  // mov byte [rbp-0x12], 0

        // WriteConsoleA(handle, buf, 16, NULL, NULL)
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10]
        emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0x55); emit8(code, 0xDE);  // lea rdx, [rbp-0x22]
        emit8(code, 0x41); emit8(code, 0xB8); emit32(code, 16);                    // mov r8d, 16
        emit8(code, 0x45); emit8(code, 0x31); emit8(code, 0xC9);                    // xor r9d, r9d
        emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x44); emit8(code, 0x24); emit8(code, 0x20);
        emit32(code, 0);                                                       // mov qword [rsp+0x20], 0
        emit_call_iat(code, text_rva, imports.iat_write_console);

        emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC4); emit8(code, 0x50);  // add rsp, 0x50
        emit8(code, 0x5D);                                                     // pop rbp
        emit8(code, 0xC3);                                                     // ret
    }
    
    // jmp over inline string data, lea rcx to it, call print_impl
    void emit_print_inline(std::vector<std::uint8_t>& code, std::size_t text_rva,
                            std::size_t print_offset, const char* str, std::size_t len) {
        // jmp over the string data
        emit8(code, 0xEB);
        emit8(code, static_cast<std::uint8_t>(len));
        std::size_t str_start = code.size();
        for (std::size_t i = 0; i < len; i++) emit8(code, static_cast<std::uint8_t>(str[i]));
        // lea rcx, [rip + back_to_str]
        std::size_t lea_pos = code.size();
        std::int32_t str_rel = checked_rel32(
            static_cast<std::int64_t>(str_start) - static_cast<std::int64_t>(lea_pos + 7),
            "inline string lea");
        emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0x0D); emit32(code, static_cast<std::uint32_t>(str_rel));
        // call print_impl
        std::size_t call_pos = code.size();
        std::int32_t print_rel = checked_rel32(
            static_cast<std::int64_t>(text_rva + print_offset) - static_cast<std::int64_t>(text_rva + call_pos + 5),
            "print_impl call");
        emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(print_rel));
    }
    
    

    // decimal print: value in eax -> stack buffer -> print_impl
    // trashes rax, rcx, rdx, rsi
    void emit_decimal_print(std::vector<std::uint8_t>& code, std::size_t text_rva, std::size_t print_offset) {
        // null terminate at [rbp-0xA1]
        emit8(code, 0xC6); emit8(code, 0x85); emit32(code, 0xFFFFFF5F); emit8(code, 0x00);  // mov byte [rbp-0xA1], 0
        // lea rsi, [rbp-0xA1]
        emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0xB5); emit32(code, 0xFFFFFF5F);
        // mov ecx, 10
        emit8(code, 0xB9); emit32(code, 10);
        // .digit_loop:
        std::size_t digit_loop = code.size();
        emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xCE);  // dec rsi
        emit8(code, 0x31); emit8(code, 0xD2);                    // xor edx, edx
        emit8(code, 0xF7); emit8(code, 0xF1);                    // div ecx
        emit8(code, 0x80); emit8(code, 0xC2); emit8(code, 0x30);    // add dl, '0'
        emit8(code, 0x88); emit8(code, 0x16);                     // mov [rsi], dl
        emit8(code, 0x85); emit8(code, 0xC0);                     // test eax, eax
        emit8(code, 0x75);                                      // jnz digit_loop
        emit8(code, static_cast<std::uint8_t>(digit_loop - (code.size() + 1)));
        // mov rcx, rsi; call print_impl
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xF1);
        std::size_t cp = code.size();
        std::int32_t pr = static_cast<std::int32_t>((text_rva + print_offset) - (text_rva + cp + 5));
        emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(pr));
    }

    // patch a rel32 at fixup_pos to jump to current code.size()
    static void patch32(std::vector<std::uint8_t>& code, std::size_t fixup_pos) {
        std::int32_t rel = checked_rel32(
            static_cast<std::int64_t>(code.size()) - static_cast<std::int64_t>(fixup_pos + 4),
            "patch32");
        code[fixup_pos + 0] = static_cast<std::uint8_t>(rel & 0xFF);
        code[fixup_pos + 1] = static_cast<std::uint8_t>((rel >> 8) & 0xFF);
        code[fixup_pos + 2] = static_cast<std::uint8_t>((rel >> 16) & 0xFF);
        code[fixup_pos + 3] = static_cast<std::uint8_t>((rel >> 24) & 0xFF);
    }

    // crash handler - vectored exception handler
    // register usage:
    //   rbx  = module base (computed via lea rip trick)
    //   r15  = EXCEPTION_RECORD* (saved from EXCEPTION_POINTERS)
    //   r12d = crash line number (from srcmap lookup)
    //   r13  = source text walker / general scratch
    //   r14  = crash offset in user code
    //   edi  = line counter during source walk
    //   [rbp-0x08] = EXCEPTION_POINTERS* (original rcx)
    //   [rbp-0x10] = crash RIP
    //   [rbp-0x18] = CONTEXT*
    // note: all short jumps (jb, jae, jne, je, jmp rel8) in this function are between
    // labels within a fixed-size handler slot, so offsets are guaranteed to fit in int8.
    // if the handler ever grows past its slot the overflow check at the end will catch it.
    void build_crash_handler(std::vector<std::uint8_t>& code, std::size_t text_rva, 
                              std::size_t rdata_rva, const ImportResult& imports,
                              std::size_t user_code_offset, std::size_t print_offset,
                              std::size_t print_hex_offset, std::size_t srcmap_rva,
                              std::size_t src_rva, std::size_t data_rva,
                              std::size_t crash_handler_off, std::size_t crash_handler_size,
                              ast::HealingMode healing_mode = ast::HealingMode::Off) {
        std::size_t handler_start = code.size();

        // prologue - save all callee-saved regs we use (including r15 for EXCEPTION_RECORD*)
        emit8(code, 0x55);                                     // push rbp
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xE5);    // mov rbp, rsp
        emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xEC); emit32(code, 0xC0);  // sub rsp, 0xC0
        emit8(code, 0x53);                                     // push rbx
        emit8(code, 0x56);                                     // push rsi
        emit8(code, 0x57);                                     // push rdi
        emit8(code, 0x41); emit8(code, 0x54);                     // push r12
        emit8(code, 0x41); emit8(code, 0x55);                     // push r13
        emit8(code, 0x41); emit8(code, 0x56);                     // push r14
        emit8(code, 0x41); emit8(code, 0x57);                     // push r15

        // extract both EXCEPTION_RECORD* and CONTEXT* from EXCEPTION_POINTERS
        // rcx = EXCEPTION_POINTERS* on entry
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x4D); emit8(code, 0xF8);  // mov [rbp-8], rcx
        emit8(code, 0x4C); emit8(code, 0x8B); emit8(code, 0x39);                    // mov r15, [rcx] (EXCEPTION_RECORD*)
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x49); emit8(code, 0x08);  // mov rcx, [rcx+8] (CONTEXT*)
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x4D); emit8(code, 0xE8);  // mov [rbp-0x18], rcx (save CONTEXT*)
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x81); emit32(code, 0xF8); // mov rax, [rcx+0xF8] (Rip)
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x45); emit8(code, 0xF0);  // mov [rbp-0x10], rax (save crash RIP)

        // compute module base via lea rip trick
        emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0x05); emit32(code, 0);    // lea rax, [rip+0]
        std::size_t rva_after_lea = text_rva + crash_handler_off + (code.size() - handler_start);
        emit8(code, 0x48); emit8(code, 0x2D); emit32(code, static_cast<std::uint32_t>(rva_after_lea));  // sub rax, imm32
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xC3);    // mov rbx, rax (rbx = base)

        // re-entrancy guard: if crash RIP is inside our handler, bail
        // handler lives at [base + TEXT_RVA + crash_handler_off] to [+ crash_handler_size]
        emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xF0);  // mov rax, [rbp-0x10] (crash RIP)
        emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0xD9);                    // mov rcx, rbx (base)
        emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC1);                    // add rcx, handler_start_rva
        emit32(code, static_cast<std::uint32_t>(text_rva + crash_handler_off));
        emit8(code, 0x48); emit8(code, 0x39); emit8(code, 0xC8);                    // cmp rax, rcx
        emit8(code, 0x72);                                                      // jb .not_reentrant (below handler start)
        std::size_t jb_not_reentrant = code.size(); emit8(code, 0x00);
        emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC1); emit32(code, static_cast<std::uint32_t>(crash_handler_size));  // add rcx, crash_handler_size
        emit8(code, 0x48); emit8(code, 0x39); emit8(code, 0xC8);                    // cmp rax, rcx
        emit8(code, 0x73);                                                      // jae .not_reentrant (above handler end)
        std::size_t jae_not_reentrant = code.size(); emit8(code, 0x00);
        // crash is inside our handler - return EXCEPTION_CONTINUE_SEARCH (0)
        emit8(code, 0x41); emit8(code, 0x5F);                     // pop r15
        emit8(code, 0x41); emit8(code, 0x5E);                     // pop r14
        emit8(code, 0x41); emit8(code, 0x5D);                     // pop r13
        emit8(code, 0x41); emit8(code, 0x5C);                     // pop r12
        emit8(code, 0x5F);                                      // pop rdi
        emit8(code, 0x5E);                                      // pop rsi
        emit8(code, 0x5B);                                      // pop rbx
        emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC4); emit32(code, 0xC0);  // add rsp, 0xC0
        emit8(code, 0x5D);                                      // pop rbp
        emit8(code, 0x31); emit8(code, 0xC0);                     // xor eax, eax (return 0)
        emit8(code, 0xC3);                                      // ret
        // .not_reentrant:
        std::size_t not_reentrant = code.size();
        code[jb_not_reentrant] = static_cast<std::uint8_t>(not_reentrant - (jb_not_reentrant + 1));
        code[jae_not_reentrant] = static_cast<std::uint8_t>(not_reentrant - (jae_not_reentrant + 1));

        std::size_t jne_silent_freeze = 0;
        if (healing_mode == ast::HealingMode::Freeze) {
            // Atomically claim freeze ownership before printing anything.
            // Without this, multiple crashing threads can all pass the early
            // "latch == 0" check and each print their own crash banner.
            emit8(code, 0xB0); emit8(code, 0x01);                                     // mov al, 1
            emit8(code, 0x86); emit8(code, 0x83);                                     // xchg [rbx + repl_state + latch], al
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA + FREEZE_LATCH_OFFSET_IN_REPL_STATE));
            emit8(code, 0x84); emit8(code, 0xC0);                                     // test al, al
            emit8(code, 0x0F); emit8(code, 0x85);                                     // jne -> silent_freeze
            jne_silent_freeze = code.size(); emit32(code, 0);
        }

        // header is emitted only for exception types we actually handle.
        // unknown host exceptions should continue searching without printing
        // a misleading crash banner.
        const char hdr[] = "\n=== OPUS CRASH DETECTED ===\n\0";

        // ---- exception type detection ----
        // load ExceptionCode from EXCEPTION_RECORD (r15)
        // eax = [r15+0] = ExceptionCode
        emit8(code, 0x41); emit8(code, 0x8B); emit8(code, 0x07);    // mov eax, [r15]

        // compare against known exception codes and branch
        // ACCESS_VIOLATION = 0xC0000005
        emit8(code, 0x3D); emit32(code, 0xC0000005);              // cmp eax, 0xC0000005
        emit8(code, 0x0F); emit8(code, 0x84);                     // je -> handle_av
        std::size_t je_av = code.size(); emit32(code, 0);

        // INTEGER_DIVIDE_BY_ZERO = 0xC0000094
        emit8(code, 0x3D); emit32(code, 0xC0000094);              // cmp eax, 0xC0000094
        emit8(code, 0x0F); emit8(code, 0x84);                     // je -> handle_div_zero
        std::size_t je_div = code.size(); emit32(code, 0);

        // STACK_OVERFLOW = 0xC00000FD
        emit8(code, 0x3D); emit32(code, 0xC00000FD);              // cmp eax, 0xC00000FD
        emit8(code, 0x0F); emit8(code, 0x84);                     // je -> handle_stack_overflow
        std::size_t je_stack = code.size(); emit32(code, 0);

        // ILLEGAL_INSTRUCTION = 0xC000001D
        emit8(code, 0x3D); emit32(code, 0xC000001D);              // cmp eax, 0xC000001D
        emit8(code, 0x0F); emit8(code, 0x84);                     // je -> handle_illegal
        std::size_t je_illegal = code.size(); emit32(code, 0);

        // BREAKPOINT = 0x80000003
        emit8(code, 0x3D); emit32(code, 0x80000003);              // cmp eax, 0x80000003
        emit8(code, 0x0F); emit8(code, 0x84);                     // je -> handle_breakpoint
        std::size_t je_breakpoint = code.size(); emit32(code, 0);

        // SINGLE_STEP = 0x80000004
        emit8(code, 0x3D); emit32(code, 0x80000004);              // cmp eax, 0x80000004
        emit8(code, 0x0F); emit8(code, 0x84);                     // je -> handle_single_step
        std::size_t je_single_step = code.size(); emit32(code, 0);

        // fall through to unknown
        emit8(code, 0xE9);                                      // jmp -> handle_unknown
        std::size_t jmp_unknown = code.size(); emit32(code, 0);

        // ---- .handle_av: ACCESS_VIOLATION ----
        patch32(code, je_av);
        {
            emit_print_inline(code, text_rva, print_offset, hdr, sizeof(hdr));
            const char av_str[] = "  Exception: ACCESS_VIOLATION (\0";
            emit_print_inline(code, text_rva, print_offset, av_str, sizeof(av_str));

            // read ExceptionInformation[0] from [r15+0x20] (read=0, write=1)
            // x64 EXCEPTION_RECORD layout: Code(0), Flags(4), Record*(8), Address*(0x10), NumParams(0x18), Info[](0x20)
            emit8(code, 0x41); emit8(code, 0x8B); emit8(code, 0x47); emit8(code, 0x20);  // mov eax, [r15+0x20]
            emit8(code, 0x85); emit8(code, 0xC0);                                    // test eax, eax
            emit8(code, 0x75);                                                     // jnz -> av_write
            std::size_t jnz_av_write = code.size(); emit8(code, 0x00);

            // av read path - tag 1
            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 1);  // mov dword [rbp-0x2C], 1
            const char av_read[] = "read of 0x\0";
            emit_print_inline(code, text_rva, print_offset, av_read, sizeof(av_read));
            emit8(code, 0xEB);                                                     // jmp -> av_addr
            std::size_t jmp_av_addr = code.size(); emit8(code, 0x00);

            // .av_write: tag 2
            std::size_t av_write_label = code.size();
            code[jnz_av_write] = static_cast<std::uint8_t>(av_write_label - (jnz_av_write + 1));
            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 2);  // mov dword [rbp-0x2C], 2
            const char av_write_str[] = "write to 0x\0";
            emit_print_inline(code, text_rva, print_offset, av_write_str, sizeof(av_write_str));

            // .av_addr: print target address from ExceptionInformation[1]
            std::size_t av_addr_label = code.size();
            code[jmp_av_addr] = static_cast<std::uint8_t>(av_addr_label - (jmp_av_addr + 1));

            // load target address into rcx and call print_hex_impl
            emit8(code, 0x49); emit8(code, 0x8B); emit8(code, 0x4F); emit8(code, 0x28);  // mov rcx, [r15+0x28]
            std::size_t hex_call1 = code.size();
            std::int32_t hex_rel1 = static_cast<std::int32_t>((text_rva + print_hex_offset) - (text_rva + hex_call1 + 5));
            emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(hex_rel1));

            const char av_close[] = ")\n\0";
            emit_print_inline(code, text_rva, print_offset, av_close, sizeof(av_close));
        }
        // jmp -> after_exception_type
        emit8(code, 0xE9);
        std::size_t jmp_after_exc_av = code.size(); emit32(code, 0);

        // ---- .handle_div_zero: INTEGER_DIVIDE_BY_ZERO ----
        patch32(code, je_div);
        {
            emit_print_inline(code, text_rva, print_offset, hdr, sizeof(hdr));
            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 3);  // mov dword [rbp-0x2C], 3 (div-zero tag)
            const char div_str[] = "  Exception: INTEGER_DIVIDE_BY_ZERO\n\0";
            emit_print_inline(code, text_rva, print_offset, div_str, sizeof(div_str));
        }
        emit8(code, 0xE9);
        std::size_t jmp_after_exc_div = code.size(); emit32(code, 0);

        // ---- .handle_stack_overflow: STACK_OVERFLOW ----
        patch32(code, je_stack);
        {
            emit_print_inline(code, text_rva, print_offset, hdr, sizeof(hdr));
            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 4);  // mov dword [rbp-0x2C], 4 (stack overflow)
            const char so_str[] = "  Exception: STACK_OVERFLOW\n\0";
            emit_print_inline(code, text_rva, print_offset, so_str, sizeof(so_str));
        }
        emit8(code, 0xE9);
        std::size_t jmp_after_exc_so = code.size(); emit32(code, 0);

        // ---- .handle_illegal: ILLEGAL_INSTRUCTION ----
        patch32(code, je_illegal);
        {
            emit_print_inline(code, text_rva, print_offset, hdr, sizeof(hdr));
            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 5);  // tag 5 (illegal insn)
            const char ill_str[] = "  Exception: ILLEGAL_INSTRUCTION\n\0";
            emit_print_inline(code, text_rva, print_offset, ill_str, sizeof(ill_str));
        }
        emit8(code, 0xE9);
        std::size_t jmp_after_exc_ill = code.size(); emit32(code, 0);

        // ---- .handle_breakpoint: BREAKPOINT ----
        patch32(code, je_breakpoint);
        std::size_t jmp_after_exc_bp_static = 0;
        {
            emit_print_inline(code, text_rva, print_offset, hdr, sizeof(hdr));
            // check_addr = CONTEXT.RIP (windows VEH points RIP at the INT3, not past it)
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xF0);  // mov rax, [rbp-0x10] (crash RIP = INT3 addr)
            // save check_addr in r14 for bp table scan
            emit8(code, 0x49); emit8(code, 0x89); emit8(code, 0xC6);                    // mov r14, rax

            // scan breakpoint table for active entry matching check_addr
            // bp table is at [rbx + data_rva + BP_TABLE_OFFSET_IN_DATA]
            // each entry: address(8) original_byte(1) active(1) pad(2) line(4) = 16 bytes
            emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0xB3);                    // lea rsi, [rbx + bp_table_rva]
            emit32(code, static_cast<std::uint32_t>(data_rva + BP_TABLE_OFFSET_IN_DATA));
            emit8(code, 0xB9); emit32(code, static_cast<std::uint32_t>(BP_TABLE_ENTRIES));  // mov ecx, 16
            emit8(code, 0x31); emit8(code, 0xFF);                                     // xor edi, edi (index counter)

            // .bp_scan_loop:
            std::size_t bp_scan_loop = code.size();
            // check active flag at [rsi + 9]
            emit8(code, 0x80); emit8(code, 0x7E); emit8(code, 0x09); emit8(code, 0x01);  // cmp byte [rsi+9], 1
            emit8(code, 0x75);                                                     // jne .bp_scan_next
            std::size_t jne_bp_scan_next = code.size(); emit8(code, 0x00);
            // active entry - compare address at [rsi+0] with check_addr (r14)
            emit8(code, 0x4C); emit8(code, 0x3B); emit8(code, 0x36);                    // cmp r14, [rsi]
            emit8(code, 0x0F); emit8(code, 0x84);                                     // je .bp_found_dynamic
            std::size_t je_bp_found = code.size(); emit32(code, 0);
            // .bp_scan_next:
            std::size_t bp_scan_next = code.size();
            code[jne_bp_scan_next] = static_cast<std::uint8_t>(bp_scan_next - (jne_bp_scan_next + 1));
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC6); emit8(code, 0x10);  // add rsi, 16
            emit8(code, 0xFF); emit8(code, 0xC7);                                     // inc edi
            emit8(code, 0xFF); emit8(code, 0xC9);                                     // dec ecx
            emit8(code, 0x75);                                                       // jnz bp_scan_loop
            emit8(code, static_cast<std::uint8_t>(bp_scan_loop - (code.size() + 1)));

            // not found in table -> static breakpoint
            // set bp_type = 1 in repl state
            emit8(code, 0xC6); emit8(code, 0x83);                                     // mov byte [rbx + repl_state_rva], 1
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA));
            emit8(code, 0x01);

            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 0);    // mov dword [rbp-0x2C], 0 (unhealable tag)
            const char bp_static_str[] = "  Breakpoint hit (static)\n\0";
            emit_print_inline(code, text_rva, print_offset, bp_static_str, sizeof(bp_static_str));
            emit8(code, 0xE9);                                                     // jmp -> after_exception_type
            jmp_after_exc_bp_static = code.size(); emit32(code, 0);

            // .bp_found_dynamic: rsi points at the matching BP table entry, edi = index
            patch32(code, je_bp_found);

            // save the index as pending_rearm_idx in repl state
            // repl state: byte 0 = bp_type, byte 1 = pending_rearm_idx
            emit8(code, 0x88); emit8(code, 0xBB);                                     // mov [rbx + repl_state_rva + 1], dil
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA + 1));

            // set bp_type = 2 (dynamic)
            emit8(code, 0xC6); emit8(code, 0x83);                                     // mov byte [rbx + repl_state_rva], 2
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA));
            emit8(code, 0x02);

            // restore original byte at the breakpoint address
            // need VirtualProtect to make code writable first
            // save rsi (bp entry ptr) across calls
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x75); emit8(code, 0xD0);  // mov [rbp-0x30], rsi

            // VirtualProtect(check_addr, 1, PAGE_EXECUTE_READWRITE(0x40), &old_protect)
            emit8(code, 0x4C); emit8(code, 0x89); emit8(code, 0xF1);                    // mov rcx, r14 (address)
            emit8(code, 0xBA); emit32(code, 1);                                        // mov edx, 1 (size)
            emit8(code, 0x41); emit8(code, 0xB8); emit32(code, 0x40);                   // mov r8d, 0x40 (PAGE_EXECUTE_READWRITE)
            emit8(code, 0x4C); emit8(code, 0x8D); emit8(code, 0x4D); emit8(code, 0xCC);  // lea r9, [rbp-0x34] (old_protect)
            emit_call_iat(code, text_rva, imports.iat_rva + iat::VirtualProtect);

            // restore rsi
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x75); emit8(code, 0xD0);  // mov rsi, [rbp-0x30]

            // write original byte back: mov al, [rsi+8]; mov [r14], al
            emit8(code, 0x8A); emit8(code, 0x46); emit8(code, 0x08);                    // mov al, [rsi+8] (original_byte)
            emit8(code, 0x41); emit8(code, 0x88); emit8(code, 0x06);                    // mov [r14], al

            // restore original protection
            // VirtualProtect(check_addr, 1, old_protect, &dummy)
            emit8(code, 0x4C); emit8(code, 0x89); emit8(code, 0xF1);                    // mov rcx, r14
            emit8(code, 0xBA); emit32(code, 1);                                        // mov edx, 1
            emit8(code, 0x44); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xCC);  // mov r8d, [rbp-0x34] (old_protect)
            emit8(code, 0x4C); emit8(code, 0x8D); emit8(code, 0x4D); emit8(code, 0xC8);  // lea r9, [rbp-0x38] (dummy)
            emit_call_iat(code, text_rva, imports.iat_rva + iat::VirtualProtect);

            emit8(code, 0xC7); emit8(code, 0x45); emit8(code, 0xD4); emit32(code, 0);    // mov dword [rbp-0x2C], 0 (unhealable tag)
            const char bp_dyn_str[] = "  Breakpoint hit (dynamic)\n\0";
            emit_print_inline(code, text_rva, print_offset, bp_dyn_str, sizeof(bp_dyn_str));
        }
        emit8(code, 0xE9);
        std::size_t jmp_after_exc_bp = code.size(); emit32(code, 0);

        // ---- .handle_single_step: STATUS_SINGLE_STEP ----
        // this is transparent - re-arm the dynamic bp and return, no REPL
        patch32(code, je_single_step);
        {
            // load pending_rearm_idx from repl state
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x83);                    // movzx eax, byte [rbx + repl_state_rva + 1]
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA + 1));

            // compute bp table entry address: rbx + bp_table_rva + eax*16
            emit8(code, 0xC1); emit8(code, 0xE0); emit8(code, 0x04);                    // shl eax, 4 (index * 16)
            emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0xB3);                    // lea rsi, [rbx + bp_table_rva]
            emit32(code, static_cast<std::uint32_t>(data_rva + BP_TABLE_OFFSET_IN_DATA));
            emit8(code, 0x48); emit8(code, 0x01); emit8(code, 0xC6);                    // add rsi, rax (rsi = entry ptr)

            // load breakpoint address from entry
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x0E);                    // mov rcx, [rsi] (bp address)
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x4D); emit8(code, 0xD0);  // mov [rbp-0x30], rcx (save bp addr)
            emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x75); emit8(code, 0xC8);  // mov [rbp-0x38], rsi (save entry ptr)

            // VirtualProtect(bp_addr, 1, PAGE_EXECUTE_READWRITE, &old_protect)
            // rcx already has bp_addr
            emit8(code, 0xBA); emit32(code, 1);                                        // mov edx, 1
            emit8(code, 0x41); emit8(code, 0xB8); emit32(code, 0x40);                   // mov r8d, 0x40
            emit8(code, 0x4C); emit8(code, 0x8D); emit8(code, 0x4D); emit8(code, 0xCC);  // lea r9, [rbp-0x34]
            emit_call_iat(code, text_rva, imports.iat_rva + iat::VirtualProtect);

            // write 0xCC back at the breakpoint address
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xD0);  // mov rax, [rbp-0x30] (bp addr)
            emit8(code, 0xC6); emit8(code, 0x00); emit8(code, 0xCC);                    // mov byte [rax], 0xCC

            // restore protection
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xD0);  // mov rcx, [rbp-0x30]
            emit8(code, 0xBA); emit32(code, 1);                                        // mov edx, 1
            emit8(code, 0x44); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xCC);  // mov r8d, [rbp-0x34]
            emit8(code, 0x4C); emit8(code, 0x8D); emit8(code, 0x4D); emit8(code, 0xC4);  // lea r9, [rbp-0x3C]
            emit_call_iat(code, text_rva, imports.iat_rva + iat::VirtualProtect);

            // clear TF (bit 8) in RFLAGS at CONTEXT offset 0x44
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xE8);  // mov rax, [rbp-0x18] (CONTEXT*)
            emit8(code, 0x8B); emit8(code, 0x48); emit8(code, 0x44);                    // mov ecx, [rax+0x44] (EFlags)
            emit8(code, 0x81); emit8(code, 0xE1); emit32(code, 0xFFFFFEFF);             // and ecx, ~0x100 (clear TF)
            emit8(code, 0x89); emit8(code, 0x48); emit8(code, 0x44);                    // mov [rax+0x44], ecx

            // return EXCEPTION_CONTINUE_EXECUTION (-1)
            emit8(code, 0x41); emit8(code, 0x5F);                     // pop r15
            emit8(code, 0x41); emit8(code, 0x5E);                     // pop r14
            emit8(code, 0x41); emit8(code, 0x5D);                     // pop r13
            emit8(code, 0x41); emit8(code, 0x5C);                     // pop r12
            emit8(code, 0x5F);                                      // pop rdi
            emit8(code, 0x5E);                                      // pop rsi
            emit8(code, 0x5B);                                      // pop rbx
            emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC4); emit32(code, 0xC0);  // add rsp, 0xC0
            emit8(code, 0x5D);                                      // pop rbp
            emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0xC0); emit32(code, 0xFFFFFFFF);  // mov rax, -1
            emit8(code, 0xC3);                                      // ret
        }

        // ---- .handle_unknown: UNKNOWN_EXCEPTION (0x...) ----
        patch32(code, jmp_unknown);
        {
            // Unknown/non-hardware exceptions often belong to the host process
            // (for example Microsoft C++ EH 0xE06D7363). Let normal handlers see them.
            emit8(code, 0x41); emit8(code, 0x5F);                     // pop r15
            emit8(code, 0x41); emit8(code, 0x5E);                     // pop r14
            emit8(code, 0x41); emit8(code, 0x5D);                     // pop r13
            emit8(code, 0x41); emit8(code, 0x5C);                     // pop r12
            emit8(code, 0x5F);                                        // pop rdi
            emit8(code, 0x5E);                                        // pop rsi
            emit8(code, 0x5B);                                        // pop rbx
            emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC4); emit32(code, 0xC0);  // add rsp, 0xC0
            emit8(code, 0x5D);                                        // pop rbp
            emit8(code, 0x31); emit8(code, 0xC0);                     // xor eax, eax (EXCEPTION_CONTINUE_SEARCH)
            emit8(code, 0xC3);                                        // ret
        }

        // ---- .after_exception_type: all paths converge here ----
        patch32(code, jmp_after_exc_av);
        patch32(code, jmp_after_exc_div);
        patch32(code, jmp_after_exc_so);
        patch32(code, jmp_after_exc_ill);
        patch32(code, jmp_after_exc_bp);
        patch32(code, jmp_after_exc_bp_static);

        // ---- source context display (existing logic, restructured) ----
        if (srcmap_rva == 0 || src_rva == 0) {
            // no debug info compiled in - simple fallback
            const char no_dbg[] = "  (no debug info)\n  [PAUSED] attach debugger...\n\0";
            emit8(code, 0xC6); emit8(code, 0x83);                                 // mov byte [rbx + repl_state + latch], 1
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA + FREEZE_LATCH_OFFSET_IN_REPL_STATE));
            emit8(code, 0x01);
            emit_print_inline(code, text_rva, print_offset, no_dbg, sizeof(no_dbg));
            if (healing_mode == ast::HealingMode::Freeze) {
                patch32(code, jne_silent_freeze);
            }
            emit8(code, 0xEB); emit8(code, 0xFE);  // jmp $ (freeze)
        } else {
            // ---- crash offset computation ----
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xF0);  // mov rax, [rbp-0x10]
            emit8(code, 0x48); emit8(code, 0x29); emit8(code, 0xD8);                    // sub rax, rbx
            emit8(code, 0x48); emit8(code, 0x2D); emit32(code, 0x1000);                 // sub rax, 0x1000 (TEXT_RVA)
            emit8(code, 0x48); emit8(code, 0x2D); emit32(code, static_cast<std::uint32_t>(user_code_offset));
            emit8(code, 0x49); emit8(code, 0x89); emit8(code, 0xC6);                    // mov r14, rax

            // js -> fallback (crash outside user code)
            emit8(code, 0x0F); emit8(code, 0x88);
            std::size_t js_fallback = code.size(); emit32(code, 0);

            // ---- srcmap linear scan ----
            emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0xB3);                    // lea rsi, [rbx + srcmap_rva]
            emit32(code, static_cast<std::uint32_t>(srcmap_rva));
            emit8(code, 0x8B); emit8(code, 0x0E);                                     // mov ecx, [rsi]
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC6); emit8(code, 0x04);    // add rsi, 4
            emit8(code, 0x85); emit8(code, 0xC9);                                     // test ecx, ecx
            emit8(code, 0x0F); emit8(code, 0x84);                                     // jz -> no_linemap
            std::size_t jz_nolinemap = code.size(); emit32(code, 0);

            // best_line = 0
            emit8(code, 0x45); emit8(code, 0x31); emit8(code, 0xE4);                    // xor r12d, r12d

            // .scan_loop:
            std::size_t scan_loop = code.size();
            emit8(code, 0x8B); emit8(code, 0x06);                                     // mov eax, [rsi]
            emit8(code, 0x4C); emit8(code, 0x39); emit8(code, 0xF0);                    // cmp rax, r14
            emit8(code, 0x77); emit8(code, 0x04);                                     // ja .skip
            emit8(code, 0x44); emit8(code, 0x8B); emit8(code, 0x66); emit8(code, 0x04);    // mov r12d, [rsi+4]
            // .skip:
            emit8(code, 0x48); emit8(code, 0x83); emit8(code, 0xC6); emit8(code, 0x08);    // add rsi, 8
            emit8(code, 0xFF); emit8(code, 0xC9);                                     // dec ecx
            emit8(code, 0x75);                                                       // jnz scan_loop
            emit8(code, static_cast<std::uint8_t>(scan_loop - (code.size() + 1)));

            // check if we found anything
            emit8(code, 0x45); emit8(code, 0x85); emit8(code, 0xE4);                    // test r12d, r12d
            emit8(code, 0x0F); emit8(code, 0x84);                                     // jz -> no_linemap
            std::size_t jz_nomatch = code.size(); emit32(code, 0);

            // ---- found a line: print location ----
            const char loc[] = "  Location: line \0";
            emit_print_inline(code, text_rva, print_offset, loc, sizeof(loc));
            emit8(code, 0x44); emit8(code, 0x89); emit8(code, 0xE0);                    // mov eax, r12d
            emit_decimal_print(code, text_rva, print_offset);
            const char nl[] = "\n\0";
            emit_print_inline(code, text_rva, print_offset, nl, sizeof(nl));

            // divider before source
            const char div1[] = "  -------------------------------------------\n\0";
            emit_print_inline(code, text_rva, print_offset, div1, sizeof(div1));

            // ---- source context walker ----
            // target_start = max(1, crash_line - 2)
            // mov edi, r12d; sub edi, 2; mov eax, 1; cmp edi, eax; cmovl edi, eax
            emit8(code, 0x44); emit8(code, 0x89); emit8(code, 0xE7);                    // mov edi, r12d
            emit8(code, 0x83); emit8(code, 0xEF); emit8(code, 0x02);                    // sub edi, 2
            emit8(code, 0xB8); emit32(code, 1);                                        // mov eax, 1
            emit8(code, 0x39); emit8(code, 0xC7);                                     // cmp edi, eax
            emit8(code, 0x0F); emit8(code, 0x4C); emit8(code, 0xF8);                    // cmovl edi, eax

            // save target_start in [rbp-0x20]
            emit8(code, 0x89); emit8(code, 0x7D); emit8(code, 0xE0);                    // mov [rbp-0x20], edi

            // target_end = crash_line + 2, save in [rbp-0x24]
            emit8(code, 0x44); emit8(code, 0x89); emit8(code, 0xE0);                    // mov eax, r12d
            emit8(code, 0x83); emit8(code, 0xC0); emit8(code, 0x02);                    // add eax, 2
            emit8(code, 0x89); emit8(code, 0x45); emit8(code, 0xDC);                    // mov [rbp-0x24], eax

            // r13 = base + src_rva (pointer to source text)
            emit8(code, 0x4C); emit8(code, 0x8D); emit8(code, 0xAB);                    // lea r13, [rbx + src_rva]
            emit32(code, static_cast<std::uint32_t>(src_rva));

            // current_line = 1, skip to target_start
            emit8(code, 0xBF); emit32(code, 1);                                        // mov edi, 1

            // .skip_lines: while edi < target_start and *r13 != 0
            std::size_t skip_loop = code.size();
            emit8(code, 0x3B); emit8(code, 0x7D); emit8(code, 0xE0);                    // cmp edi, [rbp-0x20]
            emit8(code, 0x7D);                                                       // jge .done_skip (skip 12 bytes ahead)
            std::size_t jge_done_skip = code.size(); emit8(code, 0x00);               // placeholder
            emit8(code, 0x41); emit8(code, 0x80); emit8(code, 0x7D); emit8(code, 0x00); emit8(code, 0x00); // cmp byte [r13], 0
            emit8(code, 0x74);                                                       // je .done_skip
            std::size_t je_done_skip = code.size(); emit8(code, 0x00);                // placeholder
            // check if byte is '\n'
            emit8(code, 0x41); emit8(code, 0x80); emit8(code, 0x7D); emit8(code, 0x00); emit8(code, 0x0A); // cmp byte [r13], 0x0A
            emit8(code, 0x75); emit8(code, 0x02);                                     // jne .no_newline
            emit8(code, 0xFF); emit8(code, 0xC7);                                     // inc edi
            // .no_newline:
            emit8(code, 0x49); emit8(code, 0xFF); emit8(code, 0xC5);                    // inc r13
            emit8(code, 0xEB);                                                       // jmp skip_loop
            emit8(code, static_cast<std::uint8_t>(skip_loop - (code.size() + 1)));
            // .done_skip:
            std::size_t done_skip = code.size();
            code[jge_done_skip] = static_cast<std::uint8_t>(done_skip - (jge_done_skip + 1));
            code[je_done_skip] = static_cast<std::uint8_t>(done_skip - (je_done_skip + 1));

            // ---- print context lines loop ----
            // edi = current line number, r13 = pointer into source
            // loop while edi <= target_end and *r13 != 0
            std::size_t ctx_loop = code.size();
            emit8(code, 0x3B); emit8(code, 0x7D); emit8(code, 0xDC);                    // cmp edi, [rbp-0x24] (target_end)
            emit8(code, 0x0F); emit8(code, 0x8F);                                     // jg -> done_ctx
            std::size_t jg_done_ctx = code.size(); emit32(code, 0);
            emit8(code, 0x41); emit8(code, 0x80); emit8(code, 0x7D); emit8(code, 0x00); emit8(code, 0x00); // cmp byte [r13], 0
            emit8(code, 0x0F); emit8(code, 0x84);                                     // je -> done_ctx
            std::size_t je_done_ctx = code.size(); emit32(code, 0);

            // print arrow or spaces prefix
            // cmp edi, r12d
            emit8(code, 0x44); emit8(code, 0x39); emit8(code, 0xE7);                    // cmp edi, r12d
            emit8(code, 0x75);                                                       // jne .normal_prefix
            std::size_t jne_normal = code.size(); emit8(code, 0x00);
            // crash line prefix: "> "
            const char arrow[] = "> \0";
            emit_print_inline(code, text_rva, print_offset, arrow, sizeof(arrow));
            emit8(code, 0xEB);                                                       // jmp .after_prefix
            std::size_t jmp_after_prefix = code.size(); emit8(code, 0x00);
            // .normal_prefix:
            std::size_t normal_prefix = code.size();
            code[jne_normal] = static_cast<std::uint8_t>(normal_prefix - (jne_normal + 1));
            const char spaces[] = "  \0";
            emit_print_inline(code, text_rva, print_offset, spaces, sizeof(spaces));
            // .after_prefix:
            std::size_t after_prefix = code.size();
            code[jmp_after_prefix] = static_cast<std::uint8_t>(after_prefix - (jmp_after_prefix + 1));

            // print line number
            emit8(code, 0x89); emit8(code, 0xF8);                                     // mov eax, edi
            // save edi across call
            emit8(code, 0x89); emit8(code, 0x7D); emit8(code, 0xD8);                    // mov [rbp-0x28], edi
            emit_decimal_print(code, text_rva, print_offset);
            emit8(code, 0x8B); emit8(code, 0x7D); emit8(code, 0xD8);                    // mov edi, [rbp-0x28]

            // print " | "
            const char pipe[] = " | \0";
            emit_print_inline(code, text_rva, print_offset, pipe, sizeof(pipe));

            // copy line content from r13 into stack buffer [rbp-0x90], max 80 chars
            // lea rsi, [rbp-0x90]
            emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0xB5); emit32(code, 0xFFFFFF70);
            // xor ecx, ecx (byte counter)
            emit8(code, 0x31); emit8(code, 0xC9);
            // .copy_loop:
            std::size_t copy_loop = code.size();
            emit8(code, 0x41); emit8(code, 0x8A); emit8(code, 0x45); emit8(code, 0x00);    // mov al, [r13]
            emit8(code, 0x3C); emit8(code, 0x00);                                     // cmp al, 0
            emit8(code, 0x74);                                                       // je .end_copy
            std::size_t je_end_copy1 = code.size(); emit8(code, 0x00);
            emit8(code, 0x3C); emit8(code, 0x0A);                                     // cmp al, 0x0A ('\n')
            emit8(code, 0x74);                                                       // je .end_copy
            std::size_t je_end_copy2 = code.size(); emit8(code, 0x00);
            emit8(code, 0x83); emit8(code, 0xF9); emit8(code, 0x4F);                    // cmp ecx, 79
            emit8(code, 0x7D);                                                       // jge .end_copy
            std::size_t jge_end_copy = code.size(); emit8(code, 0x00);
            emit8(code, 0x88); emit8(code, 0x04); emit8(code, 0x0E);                    // mov [rsi+rcx], al
            emit8(code, 0xFF); emit8(code, 0xC1);                                     // inc ecx
            emit8(code, 0x49); emit8(code, 0xFF); emit8(code, 0xC5);                    // inc r13
            emit8(code, 0xEB);                                                       // jmp copy_loop
            emit8(code, static_cast<std::uint8_t>(copy_loop - (code.size() + 1)));
            // .end_copy:
            std::size_t end_copy = code.size();
            code[je_end_copy1] = static_cast<std::uint8_t>(end_copy - (je_end_copy1 + 1));
            code[je_end_copy2] = static_cast<std::uint8_t>(end_copy - (je_end_copy2 + 1));
            code[jge_end_copy] = static_cast<std::uint8_t>(end_copy - (jge_end_copy + 1));

            // append '\n' and null terminator
            emit8(code, 0xC6); emit8(code, 0x04); emit8(code, 0x0E); emit8(code, 0x0A);    // mov byte [rsi+rcx], '\n'
            emit8(code, 0xFF); emit8(code, 0xC1);                                     // inc ecx
            emit8(code, 0xC6); emit8(code, 0x04); emit8(code, 0x0E); emit8(code, 0x00);    // mov byte [rsi+rcx], 0

            // print the line buffer
            // lea rcx, [rbp-0x90]
            emit8(code, 0x48); emit8(code, 0x8D); emit8(code, 0x8D); emit32(code, 0xFFFFFF70);
            std::size_t cp2 = code.size();
            std::int32_t pr2 = static_cast<std::int32_t>((text_rva + print_offset) - (text_rva + cp2 + 5));
            emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(pr2));

            // advance r13 past the newline if we stopped on one
            emit8(code, 0x41); emit8(code, 0x80); emit8(code, 0x7D); emit8(code, 0x00); emit8(code, 0x0A); // cmp byte [r13], '\n'
            emit8(code, 0x75); emit8(code, 0x03);                                     // jne .no_skip_nl
            emit8(code, 0x49); emit8(code, 0xFF); emit8(code, 0xC5);                    // inc r13
            // .no_skip_nl:

            // restore edi, increment line counter
            emit8(code, 0x8B); emit8(code, 0x7D); emit8(code, 0xD8);                    // mov edi, [rbp-0x28]
            emit8(code, 0xFF); emit8(code, 0xC7);                                     // inc edi
            emit8(code, 0x89); emit8(code, 0x7D); emit8(code, 0xD8);                    // mov [rbp-0x28], edi (keep in sync)

            // jmp ctx_loop
            emit8(code, 0xE9);
            std::int32_t ctx_back = static_cast<std::int32_t>(ctx_loop - (code.size() + 4));
            emit32(code, static_cast<std::uint32_t>(ctx_back));

            // .done_ctx:
            patch32(code, jg_done_ctx);
            patch32(code, je_done_ctx);

            // divider after source
            const char div2[] = "  -------------------------------------------\n\0";
            emit_print_inline(code, text_rva, print_offset, div2, sizeof(div2));

            // jmp -> freeze
            emit8(code, 0xE9);
            std::size_t jmp_freeze_main = code.size(); emit32(code, 0);

            // ---- fallback: crash outside user code ----
            patch32(code, js_fallback);
            {
                const char outside[] = "  (crash outside user code)\n\0";
                emit_print_inline(code, text_rva, print_offset, outside, sizeof(outside));
            }
            emit8(code, 0xE9);
            std::size_t jmp_freeze_outside = code.size(); emit32(code, 0);

            // ---- fallback: no line mapping ----
            patch32(code, jz_nolinemap);
            patch32(code, jz_nomatch);
            {
                const char nomap[] = "  (no line mapping available)\n\0";
                emit_print_inline(code, text_rva, print_offset, nomap, sizeof(nomap));
            }

            // ---- register dump ----
            // all paths converge here before freeze
            patch32(code, jmp_freeze_main);
            patch32(code, jmp_freeze_outside);

            // register dump header
            const char reg_hdr[] = "  Registers:\n\0";
            emit_print_inline(code, text_rva, print_offset, reg_hdr, sizeof(reg_hdr));

            // helper: load reg from CONTEXT and call print_hex_impl
            auto emit_reg_value = [&](std::uint32_t ctx_offset) {
                // mov rax, [rbp-0x18]  (reload CONTEXT*)
                emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xE8);
                // mov rcx, [rax + ctx_offset]
                emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x88); emit32(code, ctx_offset);
                // call print_hex_impl
                std::size_t call_pos = code.size();
                std::int32_t rel = static_cast<std::int32_t>((text_rva + print_hex_offset) - (text_rva + call_pos + 5));
                emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(rel));
            };

            // helper: print a register pair on one line
            auto emit_reg_pair = [&](const char* name1, std::uint32_t off1,
                                     const char* name2, std::uint32_t off2) {
                emit_print_inline(code, text_rva, print_offset, name1, strlen(name1) + 1);
                emit_reg_value(off1);
                emit_print_inline(code, text_rva, print_offset, name2, strlen(name2) + 1);
                emit_reg_value(off2);
                const char nl[] = "\n\0";
                emit_print_inline(code, text_rva, print_offset, nl, sizeof(nl));
            };

            emit_reg_pair("  RAX = \0", 0x78, "  RBX = \0", 0x90);
            emit_reg_pair("  RCX = \0", 0x80, "  RDX = \0", 0x88);
            emit_reg_pair("  RSP = \0", 0x98, "  RBP = \0", 0xA0);
            emit_reg_pair("  RSI = \0", 0xA8, "  RDI = \0", 0xB0);
            emit_reg_pair("  R8  = \0", 0xB8, "  R9  = \0", 0xC0);
            emit_reg_pair("  R10 = \0", 0xC8, "  R11 = \0", 0xD0);
            emit_reg_pair("  R12 = \0", 0xD8, "  R13 = \0", 0xE0);
            emit_reg_pair("  R14 = \0", 0xE8, "  R15 = \0", 0xF0);
            {
                const char rip_label[] = "  RIP = \0";
                emit_print_inline(code, text_rva, print_offset, rip_label, sizeof(rip_label));
                emit_reg_value(0xF8);
                const char nl[] = "\n\0";
                emit_print_inline(code, text_rva, print_offset, nl, sizeof(nl));
            }

            // ---- subroutines (emitted before heal block so offsets are defined) ----
            // jmp over the subroutines during normal flow
            emit8(code, 0xE9);
            std::size_t jmp_over_subs = code.size(); emit32(code, 0);

            // ---- instruction length decoder subroutine ----
            // input: rcx = faulting instruction address
            // output: eax = instruction length in bytes
            std::size_t insn_decoder_offset = code.size() - handler_start;

            emit8(code, 0x31); emit8(code, 0xC0);                    // xor eax, eax
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx]

            // check REX prefix (0x40-0x4F)
            emit8(code, 0x41); emit8(code, 0x89); emit8(code, 0xD0);    // mov r8d, edx
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xE0); emit8(code, 0xF0); // and r8d, 0xF0
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x40); // cmp r8d, 0x40
            emit8(code, 0x75);                                      // jne .no_rex
            std::size_t jne_norex = code.size(); emit8(code, 0x00);
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (count REX)
            emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xC1);    // inc rcx
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx]
            code[jne_norex] = static_cast<std::uint8_t>(code.size() - (jne_norex + 1));

            // check 0x0F two-byte escape
            emit8(code, 0x80); emit8(code, 0xFA); emit8(code, 0x0F);    // cmp dl, 0x0F
            emit8(code, 0x75);                                      // jne .one_byte
            std::size_t jne_onebyte = code.size(); emit8(code, 0x00);
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (count 0x0F)
            emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xC1);    // inc rcx
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx]
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (count 2nd opcode byte)
            emit8(code, 0xE9);                                      // jmp .decode_modrm
            std::size_t jmp_modrm = code.size(); emit32(code, 0);

            code[jne_onebyte] = static_cast<std::uint8_t>(code.size() - (jne_onebyte + 1));
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (count opcode)

            auto emit_opcode_check = [&](uint8_t opcode, std::size_t& target) {
                emit8(code, 0x80); emit8(code, 0xFA); emit8(code, opcode);
                emit8(code, 0x0F); emit8(code, 0x84);
                target = code.size(); emit32(code, 0);
            };

            std::size_t je_modrm[14];
            emit_opcode_check(0x8B, je_modrm[0]);
            emit_opcode_check(0x8A, je_modrm[1]);
            emit_opcode_check(0x89, je_modrm[2]);
            emit_opcode_check(0x88, je_modrm[3]);
            emit_opcode_check(0x03, je_modrm[4]);
            emit_opcode_check(0x01, je_modrm[5]);
            emit_opcode_check(0x2B, je_modrm[6]);
            emit_opcode_check(0x29, je_modrm[7]);
            emit_opcode_check(0x33, je_modrm[8]);
            emit_opcode_check(0x3B, je_modrm[9]);
            emit_opcode_check(0xF7, je_modrm[10]);
            emit_opcode_check(0xFF, je_modrm[11]);
            emit_opcode_check(0xC7, je_modrm[12]);
            emit_opcode_check(0xC6, je_modrm[13]);

            // unknown opcode fallback
            emit8(code, 0xB8); emit32(code, 1);                       // mov eax, 1
            emit8(code, 0xC3);                                      // ret

            // .has_modrm: all opcode checks land here
            for (int i = 0; i < 14; i++) patch32(code, je_modrm[i]);
            emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xC1);    // inc rcx

            patch32(code, jmp_modrm);
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx] (ModR/M)
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (count ModR/M)

            emit8(code, 0x41); emit8(code, 0x89); emit8(code, 0xD0);    // mov r8d, edx
            emit8(code, 0x41); emit8(code, 0xC1); emit8(code, 0xE8); emit8(code, 0x06); // shr r8d, 6
            emit8(code, 0x41); emit8(code, 0x89); emit8(code, 0xD1);    // mov r9d, edx
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xE1); emit8(code, 0x07); // and r9d, 7

            // mod=3: register-register, done
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x03); // cmp r8d, 3
            emit8(code, 0x74);
            std::size_t je_done1 = code.size(); emit8(code, 0x00);

            // SIB check (rm=4 and mod!=3)
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF9); emit8(code, 0x04); // cmp r9d, 4
            emit8(code, 0x75);
            std::size_t jne_nosib = code.size(); emit8(code, 0x00);
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (count SIB)
            code[jne_nosib] = static_cast<std::uint8_t>(code.size() - (jne_nosib + 1));

            // displacement
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x00); // cmp r8d, 0
            emit8(code, 0x74);
            std::size_t je_mod00 = code.size(); emit8(code, 0x00);
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x01); // cmp r8d, 1
            emit8(code, 0x74);
            std::size_t je_disp8 = code.size(); emit8(code, 0x00);
            emit8(code, 0x83); emit8(code, 0xC0); emit8(code, 0x04);    // add eax, 4 (mod=2 disp32)
            emit8(code, 0xEB);
            std::size_t jmp_done1 = code.size(); emit8(code, 0x00);

            code[je_mod00] = static_cast<std::uint8_t>(code.size() - (je_mod00 + 1));
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF9); emit8(code, 0x04); // cmp r9d, 4
            emit8(code, 0x75);
            std::size_t jne_mod00_no_sib = code.size(); emit8(code, 0x00);
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x51); emit8(code, 0x01); // movzx edx, byte [rcx+1] (SIB)
            emit8(code, 0x83); emit8(code, 0xE2); emit8(code, 0x07);                     // and edx, 7
            emit8(code, 0x83); emit8(code, 0xFA); emit8(code, 0x05);                     // cmp edx, 5
            emit8(code, 0x75);
            std::size_t jne_mod00_done = code.size(); emit8(code, 0x00);
            emit8(code, 0x83); emit8(code, 0xC0); emit8(code, 0x04);                     // add eax, 4 (SIB base=5 disp32)
            emit8(code, 0xEB);
            std::size_t jmp_done2_from_sib = code.size(); emit8(code, 0x00);

            code[jne_mod00_no_sib] = static_cast<std::uint8_t>(code.size() - (jne_mod00_no_sib + 1));
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF9); emit8(code, 0x05); // cmp r9d, 5
            emit8(code, 0x75);
            std::size_t jne_done2 = code.size(); emit8(code, 0x00);
            emit8(code, 0x83); emit8(code, 0xC0); emit8(code, 0x04);    // add eax, 4 (RIP-relative disp32)
            emit8(code, 0xEB);
            std::size_t jmp_done2 = code.size(); emit8(code, 0x00);

            code[je_disp8] = static_cast<std::uint8_t>(code.size() - (je_disp8 + 1));
            emit8(code, 0xFF); emit8(code, 0xC0);                     // inc eax (disp8)

            code[je_done1] = static_cast<std::uint8_t>(code.size() - (je_done1 + 1));
            code[jmp_done1] = static_cast<std::uint8_t>(code.size() - (jmp_done1 + 1));
            code[jne_mod00_done] = static_cast<std::uint8_t>(code.size() - (jne_mod00_done + 1));
            code[jmp_done2_from_sib] = static_cast<std::uint8_t>(code.size() - (jmp_done2_from_sib + 1));
            code[jne_done2] = static_cast<std::uint8_t>(code.size() - (jne_done2 + 1));
            code[jmp_done2] = static_cast<std::uint8_t>(code.size() - (jmp_done2 + 1));

            // immediate payload for mov r/m, imm forms (opcode is at [rcx-1])
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x51); emit8(code, 0xFF);  // movzx edx, byte [rcx-1]
            emit8(code, 0x80); emit8(code, 0xFA); emit8(code, 0xC7);                      // cmp dl, 0xC7
            emit8(code, 0x75);
            std::size_t jne_imm8 = code.size(); emit8(code, 0x00);
            emit8(code, 0x83); emit8(code, 0xC0); emit8(code, 0x04);                      // add eax, 4
            emit8(code, 0xEB);
            std::size_t jmp_decoder_done = code.size(); emit8(code, 0x00);

            code[jne_imm8] = static_cast<std::uint8_t>(code.size() - (jne_imm8 + 1));
            emit8(code, 0x80); emit8(code, 0xFA); emit8(code, 0xC6);                      // cmp dl, 0xC6
            emit8(code, 0x75);
            std::size_t jne_decoder_done = code.size(); emit8(code, 0x00);
            emit8(code, 0xFF); emit8(code, 0xC0);                                          // inc eax

            code[jne_decoder_done] = static_cast<std::uint8_t>(code.size() - (jne_decoder_done + 1));
            code[jmp_decoder_done] = static_cast<std::uint8_t>(code.size() - (jmp_decoder_done + 1));
            emit8(code, 0xC3);                                      // ret

            // ---- destination register decoder subroutine ----
            // input: rcx = faulting instruction address
            // output: eax = CONTEXT offset of destination register
            std::size_t dest_reg_decoder_offset = code.size() - handler_start;

            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx]
            emit8(code, 0x45); emit8(code, 0x31); emit8(code, 0xC0);    // xor r8d, r8d

            emit8(code, 0x41); emit8(code, 0x89); emit8(code, 0xD1);    // mov r9d, edx
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xE1); emit8(code, 0xF0); // and r9d, 0xF0
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xF9); emit8(code, 0x40); // cmp r9d, 0x40
            emit8(code, 0x75);
            std::size_t jne_norex_dr = code.size(); emit8(code, 0x00);
            emit8(code, 0x0F); emit8(code, 0xBA); emit8(code, 0xE2); emit8(code, 0x02); // bt edx, 2
            emit8(code, 0x41); emit8(code, 0x83); emit8(code, 0xD0); emit8(code, 0x00); // adc r8d, 0
            emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xC1);    // inc rcx
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx]
            code[jne_norex_dr] = static_cast<std::uint8_t>(code.size() - (jne_norex_dr + 1));

            emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0xC1);    // inc rcx (skip opcode)
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x11);    // movzx edx, byte [rcx] (ModR/M)

            emit8(code, 0xC1); emit8(code, 0xEA); emit8(code, 0x03);    // shr edx, 3
            emit8(code, 0x83); emit8(code, 0xE2); emit8(code, 0x07);    // and edx, 7

            emit8(code, 0x41); emit8(code, 0xC1); emit8(code, 0xE0); emit8(code, 0x03); // shl r8d, 3
            emit8(code, 0x44); emit8(code, 0x09); emit8(code, 0xC2);    // or edx, r8d

            emit8(code, 0x8D); emit8(code, 0x04); emit8(code, 0xD5); emit32(code, 0x78); // lea eax, [rdx*8 + 0x78]
            emit8(code, 0xC3);                                      // ret

            // patch jmp-over target
            patch32(code, jmp_over_subs);

            // ---- breakpoint exit path (before heal/freeze) ----
            // check bp_type from repl state: 0=crash, 1=static bp, 2=dynamic bp
            // breakpoints bypass the heal/freeze logic entirely
            emit8(code, 0x0F); emit8(code, 0xB6); emit8(code, 0x83);                    // movzx eax, byte [rbx + repl_state_rva]
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA));
            emit8(code, 0x85); emit8(code, 0xC0);                                      // test eax, eax
            emit8(code, 0x0F); emit8(code, 0x84);                                      // jz -> not_breakpoint (fall through to heal/freeze)
            std::size_t jz_not_bp = code.size(); emit32(code, 0);

            // bp_type == 1 (static): advance RIP by 1, return -1
            emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x01);                    // cmp eax, 1
            emit8(code, 0x75);                                                       // jne -> dynamic_bp_exit
            std::size_t jne_dynamic_bp = code.size(); emit8(code, 0x00);

            // static bp: CONTEXT.RIP points AT the INT3 (windows VEH behavior)
            // advance RIP past the INT3 byte so we dont loop
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xE8);  // mov rax, [rbp-0x18] (CONTEXT*)
            emit8(code, 0x48); emit8(code, 0xFF); emit8(code, 0x80); emit32(code, 0xF8); // inc qword [rax+0xF8] (RIP += 1)

            const char bp_cont[] = "  [CONTINUE] resuming after breakpoint\n\0";
            emit_print_inline(code, text_rva, print_offset, bp_cont, sizeof(bp_cont));

            // clear bp_type back to 0
            emit8(code, 0xC6); emit8(code, 0x83);                                     // mov byte [rbx + repl_state_rva], 0
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA));
            emit8(code, 0x00);

            // return EXCEPTION_CONTINUE_EXECUTION (-1)
            emit8(code, 0xE9);                                                       // jmp -> bp_return_continue
            std::size_t jmp_bp_return = code.size(); emit32(code, 0);

            // dynamic bp: set trap flag for single-step re-arm, return -1
            code[jne_dynamic_bp] = static_cast<std::uint8_t>(code.size() - (jne_dynamic_bp + 1));

            // dynamic bp: CONTEXT.RIP already points at the INT3 addr (windows VEH)
            // original byte was restored earlier, so RIP points at the restored instruction
            // just set TF for single-step re-arm
            emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xE8);  // mov rax, [rbp-0x18] (CONTEXT*)

            // set TF (bit 8) in RFLAGS at CONTEXT offset 0x44
            emit8(code, 0x8B); emit8(code, 0x48); emit8(code, 0x44);                    // mov ecx, [rax+0x44] (EFlags)
            emit8(code, 0x81); emit8(code, 0xC9); emit32(code, 0x100);                  // or ecx, 0x100 (set TF)
            emit8(code, 0x89); emit8(code, 0x48); emit8(code, 0x44);                    // mov [rax+0x44], ecx

            const char bp_dyn_cont[] = "  [CONTINUE] single-stepping for re-arm\n\0";
            emit_print_inline(code, text_rva, print_offset, bp_dyn_cont, sizeof(bp_dyn_cont));

            // clear bp_type back to 0
            emit8(code, 0xC6); emit8(code, 0x83);                                     // mov byte [rbx + repl_state_rva], 0
            emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA));
            emit8(code, 0x00);

            // .bp_return_continue: shared epilogue for breakpoint exits
            patch32(code, jmp_bp_return);

            // epilogue - return -1
            emit8(code, 0x41); emit8(code, 0x5F);                     // pop r15
            emit8(code, 0x41); emit8(code, 0x5E);                     // pop r14
            emit8(code, 0x41); emit8(code, 0x5D);                     // pop r13
            emit8(code, 0x41); emit8(code, 0x5C);                     // pop r12
            emit8(code, 0x5F);                                      // pop rdi
            emit8(code, 0x5E);                                      // pop rsi
            emit8(code, 0x5B);                                      // pop rbx
            emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC4); emit32(code, 0xC0);  // add rsp, 0xC0
            emit8(code, 0x5D);                                      // pop rbp
            emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0xC0); emit32(code, 0xFFFFFFFF);  // mov rax, -1
            emit8(code, 0xC3);                                      // ret

            // .not_breakpoint: fall through to normal heal/freeze
            patch32(code, jz_not_bp);

            // ---- freeze or heal ----
            if (healing_mode == ast::HealingMode::Auto) {
                // check type tag - 0 means unhealable, freeze
                emit8(code, 0x8B); emit8(code, 0x45); emit8(code, 0xD4);                // mov eax, [rbp-0x2C]
                emit8(code, 0x85); emit8(code, 0xC0);                                  // test eax, eax
                emit8(code, 0x0F); emit8(code, 0x84);                                  // jz -> freeze
                std::size_t jz_freeze = code.size(); emit32(code, 0);

                // ---- healing dispatch ----
                emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x01);                // cmp eax, 1 (av read)
                emit8(code, 0x0F); emit8(code, 0x84);
                std::size_t je_heal_av_read = code.size(); emit32(code, 0);

                emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x02);                // cmp eax, 2 (av write)
                emit8(code, 0x0F); emit8(code, 0x84);
                std::size_t je_heal_av_write = code.size(); emit32(code, 0);

                emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x03);                // cmp eax, 3 (div zero)
                emit8(code, 0x0F); emit8(code, 0x84);
                std::size_t je_heal_div_zero = code.size(); emit32(code, 0);

                emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x04);                // cmp eax, 4 (stack overflow)
                emit8(code, 0x0F); emit8(code, 0x84);
                std::size_t je_heal_stack_overflow = code.size(); emit32(code, 0);

                emit8(code, 0x83); emit8(code, 0xF8); emit8(code, 0x05);                // cmp eax, 5 (illegal insn)
                emit8(code, 0x0F); emit8(code, 0x84);
                std::size_t je_heal_illegal = code.size(); emit32(code, 0);

                // unknown healable tag? shouldnt happen, freeze
                emit8(code, 0xE9);
                std::size_t jmp_freeze_fallback = code.size(); emit32(code, 0);

                // ---- heal AV read: zero dest reg, advance RIP ----
                patch32(code, je_heal_av_read);
                {
                    // call dest_reg_decoder(crash RIP)
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10] (crash RIP)
                    std::size_t call_dr = code.size();
                    std::int32_t dr_rel = static_cast<std::int32_t>((handler_start + dest_reg_decoder_offset) - (call_dr + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(dr_rel));

                    // eax = CONTEXT offset of dest reg, zero it
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18] (CONTEXT*)
                    emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x04); emit8(code, 0x01); emit32(code, 0);  // mov qword [rcx+rax], 0

                    // advance RIP
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10]
                    std::size_t call_il = code.size();
                    std::int32_t il_rel = static_cast<std::int32_t>((handler_start + insn_decoder_offset) - (call_il + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(il_rel));

                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18] (CONTEXT*)
                    emit8(code, 0x48); emit8(code, 0x98);                                    // cdqe
                    emit8(code, 0x48); emit8(code, 0x01); emit8(code, 0x81); emit32(code, 0xF8);  // add [rcx+0xF8], rax

                    // check if target address >= 0x10000 for non-null variant message
                    emit8(code, 0x49); emit8(code, 0x8B); emit8(code, 0x4F); emit8(code, 0x28);  // mov rcx, [r15+0x28] (target addr)
                    emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xF9); emit32(code, 0x10000);  // cmp rcx, 0x10000
                    emit8(code, 0x0F); emit8(code, 0x83);                                    // jae -> non-null read msg
                    std::size_t jae_nonnull_read = code.size(); emit32(code, 0);

                    const char heal_read[] = "  [HEAL] null read -> zeroed dest register, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_read, sizeof(heal_read));
                    emit8(code, 0xE9);
                    std::size_t jmp_read_done = code.size(); emit32(code, 0);

                    // non-null read: print address in the message
                    patch32(code, jae_nonnull_read);
                    const char heal_av_read_pre[] = "  [HEAL] access violation read at 0x\0";
                    emit_print_inline(code, text_rva, print_offset, heal_av_read_pre, sizeof(heal_av_read_pre));
                    emit8(code, 0x49); emit8(code, 0x8B); emit8(code, 0x4F); emit8(code, 0x28);  // mov rcx, [r15+0x28]
                    std::size_t hx_r1 = code.size();
                    std::int32_t hx_r1_rel = static_cast<std::int32_t>((text_rva + print_hex_offset) - (text_rva + hx_r1 + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(hx_r1_rel));
                    const char heal_av_read_post[] = " -> zeroed dest register, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_av_read_post, sizeof(heal_av_read_post));

                    patch32(code, jmp_read_done);
                }
                emit8(code, 0xE9);
                std::size_t jmp_continue1 = code.size(); emit32(code, 0);

                // ---- heal AV write: skip the instruction ----
                patch32(code, je_heal_av_write);
                {
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10]
                    std::size_t call_il2 = code.size();
                    std::int32_t il_rel2 = static_cast<std::int32_t>((handler_start + insn_decoder_offset) - (call_il2 + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(il_rel2));

                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18]
                    emit8(code, 0x48); emit8(code, 0x98);                                    // cdqe
                    emit8(code, 0x48); emit8(code, 0x01); emit8(code, 0x81); emit32(code, 0xF8);  // add [rcx+0xF8], rax

                    // check if target address >= 0x10000
                    emit8(code, 0x49); emit8(code, 0x8B); emit8(code, 0x4F); emit8(code, 0x28);  // mov rcx, [r15+0x28]
                    emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xF9); emit32(code, 0x10000);  // cmp rcx, 0x10000
                    emit8(code, 0x0F); emit8(code, 0x83);                                    // jae -> non-null write msg
                    std::size_t jae_nonnull_write = code.size(); emit32(code, 0);

                    const char heal_write[] = "  [HEAL] null write -> skipped instruction, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_write, sizeof(heal_write));
                    emit8(code, 0xE9);
                    std::size_t jmp_write_done = code.size(); emit32(code, 0);

                    // non-null write: print address in the message
                    patch32(code, jae_nonnull_write);
                    const char heal_av_write_pre[] = "  [HEAL] access violation write at 0x\0";
                    emit_print_inline(code, text_rva, print_offset, heal_av_write_pre, sizeof(heal_av_write_pre));
                    emit8(code, 0x49); emit8(code, 0x8B); emit8(code, 0x4F); emit8(code, 0x28);  // mov rcx, [r15+0x28]
                    std::size_t hx_w1 = code.size();
                    std::int32_t hx_w1_rel = static_cast<std::int32_t>((text_rva + print_hex_offset) - (text_rva + hx_w1 + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(hx_w1_rel));
                    const char heal_av_write_post[] = " -> skipped instruction, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_av_write_post, sizeof(heal_av_write_post));

                    patch32(code, jmp_write_done);
                }
                emit8(code, 0xE9);
                std::size_t jmp_continue2 = code.size(); emit32(code, 0);

                // ---- heal div-by-zero: zero RAX and RDX, advance RIP ----
                patch32(code, je_heal_div_zero);
                {
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18] (CONTEXT*)
                    // zero RAX in CONTEXT (offset 0x78)
                    emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x81); emit32(code, 0x78); emit32(code, 0);
                    // zero RDX in CONTEXT (offset 0x88)
                    emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0x81); emit32(code, 0x88); emit32(code, 0);

                    // advance RIP
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10]
                    std::size_t call_il3 = code.size();
                    std::int32_t il_rel3 = static_cast<std::int32_t>((handler_start + insn_decoder_offset) - (call_il3 + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(il_rel3));

                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18]
                    emit8(code, 0x48); emit8(code, 0x98);                                    // cdqe
                    emit8(code, 0x48); emit8(code, 0x01); emit8(code, 0x81); emit32(code, 0xF8);  // add [rcx+0xF8], rax

                    const char heal_div[] = "  [HEAL] div by zero -> result zeroed, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_div, sizeof(heal_div));
                }
                emit8(code, 0xE9);
                std::size_t jmp_continue3 = code.size(); emit32(code, 0);

                // ---- heal stack overflow: reset RSP, skip instruction ----
                patch32(code, je_heal_stack_overflow);
                {
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18] (CONTEXT*)

                    // reset RSP to RBP from CONTEXT - gives us a sane stack frame
                    // the overflowed function is toast but execution can continue
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x81); emit32(code, 0xA0);  // mov rax, [rcx+0xA0] (CONTEXT.Rbp)
                    emit8(code, 0x48); emit8(code, 0x89); emit8(code, 0x81); emit32(code, 0x98);  // mov [rcx+0x98], rax (CONTEXT.Rsp = Rbp)

                    // advance RIP past the faulting instruction
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10] (crash RIP)
                    std::size_t call_il_so = code.size();
                    std::int32_t il_rel_so = static_cast<std::int32_t>((handler_start + insn_decoder_offset) - (call_il_so + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(il_rel_so));

                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18]
                    emit8(code, 0x48); emit8(code, 0x98);                                    // cdqe
                    emit8(code, 0x48); emit8(code, 0x01); emit8(code, 0x81); emit32(code, 0xF8);  // add [rcx+0xF8], rax

                    const char heal_so[] = "  [HEAL] stack overflow -> reset stack, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_so, sizeof(heal_so));
                }
                emit8(code, 0xE9);
                std::size_t jmp_continue4 = code.size(); emit32(code, 0);

                // ---- heal illegal instruction: skip it ----
                patch32(code, je_heal_illegal);
                {
                    // just skip past the bad opcode
                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xF0);  // mov rcx, [rbp-0x10]
                    std::size_t call_il_ill = code.size();
                    std::int32_t il_rel_ill = static_cast<std::int32_t>((handler_start + insn_decoder_offset) - (call_il_ill + 5));
                    emit8(code, 0xE8); emit32(code, static_cast<std::uint32_t>(il_rel_ill));

                    emit8(code, 0x48); emit8(code, 0x8B); emit8(code, 0x4D); emit8(code, 0xE8);  // mov rcx, [rbp-0x18]
                    emit8(code, 0x48); emit8(code, 0x98);                                    // cdqe
                    emit8(code, 0x48); emit8(code, 0x01); emit8(code, 0x81); emit32(code, 0xF8);  // add [rcx+0xF8], rax

                    const char heal_ill[] = "  [HEAL] illegal instruction -> skipped, continuing\n\0";
                    emit_print_inline(code, text_rva, print_offset, heal_ill, sizeof(heal_ill));
                }
                // fall through to continue_execution

                // ---- .continue_execution: return EXCEPTION_CONTINUE_EXECUTION ----
                patch32(code, jmp_continue1);
                patch32(code, jmp_continue2);
                patch32(code, jmp_continue3);
                patch32(code, jmp_continue4);

                // epilogue for healing - restore regs and return -1
                emit8(code, 0x41); emit8(code, 0x5F);                     // pop r15
                emit8(code, 0x41); emit8(code, 0x5E);                     // pop r14
                emit8(code, 0x41); emit8(code, 0x5D);                     // pop r13
                emit8(code, 0x41); emit8(code, 0x5C);                     // pop r12
                emit8(code, 0x5F);                                      // pop rdi
                emit8(code, 0x5E);                                      // pop rsi
                emit8(code, 0x5B);                                      // pop rbx
                emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC4); emit32(code, 0xC0);  // add rsp, 0xC0
                emit8(code, 0x5D);                                      // pop rbp
                emit8(code, 0x48); emit8(code, 0xC7); emit8(code, 0xC0); emit32(code, 0xFFFFFFFF);  // mov rax, -1
                emit8(code, 0xC3);                                      // ret

                // ---- freeze for unhealable exceptions ----
                patch32(code, jz_freeze);
                patch32(code, jmp_freeze_fallback);
                const char freeze_msg[] = "  [PAUSED] attach debugger...\n\0";
                emit8(code, 0xC6); emit8(code, 0x83);                             // mov byte [rbx + repl_state + latch], 1
                emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA + FREEZE_LATCH_OFFSET_IN_REPL_STATE));
                emit8(code, 0x01);
                emit_print_inline(code, text_rva, print_offset, freeze_msg, sizeof(freeze_msg));
                emit8(code, 0xEB); emit8(code, 0xFE);  // jmp $
            } else {
                // non-auto mode: just freeze
                const char freeze_msg[] = "  [PAUSED] attach debugger...\n\0";
                emit8(code, 0xC6); emit8(code, 0x83);                             // mov byte [rbx + repl_state + latch], 1
                emit32(code, static_cast<std::uint32_t>(data_rva + REPL_STATE_OFFSET_IN_DATA + FREEZE_LATCH_OFFSET_IN_REPL_STATE));
                emit8(code, 0x01);
                emit_print_inline(code, text_rva, print_offset, freeze_msg, sizeof(freeze_msg));
                if (healing_mode == ast::HealingMode::Freeze) {
                    patch32(code, jne_silent_freeze);
                }
                emit8(code, 0xEB); emit8(code, 0xFE);  // jmp $
            }

        }

        // epilogue - restore callee-saved regs in reverse order
        emit8(code, 0x41); emit8(code, 0x5F);                     // pop r15
        emit8(code, 0x41); emit8(code, 0x5E);                     // pop r14
        emit8(code, 0x41); emit8(code, 0x5D);                     // pop r13
        emit8(code, 0x41); emit8(code, 0x5C);                     // pop r12
        emit8(code, 0x5F);                                      // pop rdi
        emit8(code, 0x5E);                                      // pop rsi
        emit8(code, 0x5B);                                      // pop rbx
        emit8(code, 0x48); emit8(code, 0x81); emit8(code, 0xC4); emit32(code, 0xC0);  // add rsp, 0xC0
        emit8(code, 0x5D);                                      // pop rbp
        emit8(code, 0x31); emit8(code, 0xC0);                     // xor eax, eax
        emit8(code, 0xC3);                                      // ret
        
        // sanity check - make sure we didnt overflow the handler slot
        std::size_t handler_bytes = code.size() - handler_start;
        if (handler_bytes > crash_handler_size) {
            std::println(std::cerr, "\033[1;91mFATAL:\033[0m crash handler is {} bytes but slot is only {} bytes", handler_bytes, crash_handler_size);
            std::abort();
        }
    }
    
    static std::size_t align_up(std::size_t v, std::size_t a) { return (v + a - 1) & ~(a - 1); }
    static void pad_to(std::vector<std::uint8_t>& v, std::size_t s) { if (v.size() < s) v.resize(s, 0); }
    
    template<typename T>
    static void append(std::vector<std::uint8_t>& v, const T& t) {
        auto* p = reinterpret_cast<const std::uint8_t*>(&t);
        v.insert(v.end(), p, p + sizeof(T));
    }
    
    static void emit8(std::vector<std::uint8_t>& v, std::uint8_t b) { v.push_back(b); }
    static void emit32(std::vector<std::uint8_t>& v, std::uint32_t d) {
        v.push_back(static_cast<std::uint8_t>(d));
        v.push_back(static_cast<std::uint8_t>(d >> 8));
        v.push_back(static_cast<std::uint8_t>(d >> 16));
        v.push_back(static_cast<std::uint8_t>(d >> 24));
    }
};

} // namespace opus::pe

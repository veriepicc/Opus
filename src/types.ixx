// Opus Programming Language
// Core Types Module

export module opus.types;

import std;

export namespace opus {

// ============================================================================
// PRIMITIVE TYPES
// ============================================================================

enum class PrimitiveType : std::uint8_t {
    Void,
    Bool,
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64,
    Ptr,        // Raw pointer (unsafe)
    Str,        // String slice
    Char,       // UTF-8 codepoint
};

enum class OutputKind : std::uint8_t {
    Raw,
    Exe,
    Dll,
};

constexpr bool output_is_native_image(OutputKind kind) {
    return kind != OutputKind::Raw;
}

constexpr bool output_is_exe(OutputKind kind) {
    return kind == OutputKind::Exe;
}

constexpr bool output_is_dll(OutputKind kind) {
    return kind == OutputKind::Dll;
}

constexpr std::string_view primitive_name(PrimitiveType t) {
    using enum PrimitiveType;
    switch (t) {
        case Void:  return "void";
        case Bool:  return "bool";
        case I8:    return "i8";
        case I16:   return "i16";
        case I32:   return "i32";
        case I64:   return "i64";
        case I128:  return "i128";
        case U8:    return "u8";
        case U16:   return "u16";
        case U32:   return "u32";
        case U64:   return "u64";
        case U128:  return "u128";
        case F32:   return "f32";
        case F64:   return "f64";
        case Ptr:   return "ptr";
        case Str:   return "str";
        case Char:  return "char";
        default:    return "???";
    }
}

constexpr std::size_t primitive_size(PrimitiveType t) {
    using enum PrimitiveType;
    switch (t) {
        case Void:  return 0;
        case Bool:  return 1;
        case I8:    return 1;
        case I16:   return 2;
        case I32:   return 4;
        case I64:   return 8;
        case I128:  return 16;
        case U8:    return 1;
        case U16:   return 2;
        case U32:   return 4;
        case U64:   return 8;
        case U128:  return 16;
        case F32:   return 4;
        case F64:   return 8;
        case Ptr:   return 8;
        case Str:   return 16;  // ptr + len
        case Char:  return 4;
        default:    return 0;
    }
}

// ============================================================================
// TYPE REPRESENTATION
// ============================================================================

struct Type;

struct ArrayType {
    std::unique_ptr<Type> element;
    std::optional<std::size_t> size;  // None = slice/dynamic
};

struct FunctionType {
    std::vector<std::unique_ptr<Type>> params;
    std::unique_ptr<Type> ret;
    bool is_variadic = false;
};

struct StructType {
    std::string name;
    std::vector<std::pair<std::string, std::unique_ptr<Type>>> fields;
};

struct PointerType {
    std::unique_ptr<Type> pointee;
    bool is_mut = false;
};

struct Type {
    std::variant<
        PrimitiveType,
        ArrayType,
        FunctionType,
        StructType,
        PointerType,
        std::string  // Named type (user-defined)
    > kind;

    bool is_primitive() const { 
        return std::holds_alternative<PrimitiveType>(kind); 
    }
    
    bool is_void() const {
        if (auto* p = std::get_if<PrimitiveType>(&kind)) {
            return *p == PrimitiveType::Void;
        }
        return false;
    }

    bool is_integer() const {
        if (auto* p = std::get_if<PrimitiveType>(&kind)) {
            switch (*p) {
                case PrimitiveType::I8:  case PrimitiveType::I16:
                case PrimitiveType::I32: case PrimitiveType::I64:
                case PrimitiveType::I128:
                case PrimitiveType::U8:  case PrimitiveType::U16:
                case PrimitiveType::U32: case PrimitiveType::U64:
                case PrimitiveType::U128:
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }

    bool is_float() const {
        if (auto* p = std::get_if<PrimitiveType>(&kind)) {
            return *p == PrimitiveType::F32 || *p == PrimitiveType::F64;
        }
        return false;
    }

    bool is_signed() const {
        if (auto* p = std::get_if<PrimitiveType>(&kind)) {
            switch (*p) {
                case PrimitiveType::I8:  case PrimitiveType::I16:
                case PrimitiveType::I32: case PrimitiveType::I64:
                case PrimitiveType::I128:
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }

    std::size_t size_bytes() const {
        if (auto* p = std::get_if<PrimitiveType>(&kind)) {
            return primitive_size(*p);
        }
        if (auto* arr = std::get_if<ArrayType>(&kind)) {
            if (arr->size) {
                std::size_t elem_size = arr->element->size_bytes();
                if (*arr->size != 0 && elem_size > (std::numeric_limits<std::size_t>::max)() / *arr->size) {
                    throw std::overflow_error("array size overflow in Type::size_bytes");
                }
                return elem_size * (*arr->size);
            }
            return 16; // slice = ptr + len
        }
        if (std::holds_alternative<PointerType>(kind)) {
            return 8;
        }
        if (std::holds_alternative<FunctionType>(kind)) {
            return 8; // function pointer
        }
        // struct size must be computed by the caller who knows field layout
        if (std::holds_alternative<StructType>(kind))
            throw std::logic_error("size_bytes called on struct - use field layout instead");
        if (std::holds_alternative<std::string>(kind))
            throw std::logic_error("size_bytes called on named type - resolve it first");
        return 0;
    }

    static Type make_primitive(PrimitiveType p) {
        return Type{ .kind = p };
    }

    static Type make_ptr(Type inner, bool is_mut = false) {
        return Type{ .kind = PointerType{ 
            .pointee = std::make_unique<Type>(std::move(inner)),
            .is_mut = is_mut 
        }};
    }

    Type clone() const {
        Type result;
        result.kind = std::visit([](const auto& v) -> decltype(kind) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, PrimitiveType>) {
                return v;
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                return ArrayType{
                    .element = std::make_unique<Type>(v.element->clone()),
                    .size = v.size
                };
            } else if constexpr (std::is_same_v<T, FunctionType>) {
                FunctionType ft;
                for (const auto& param : v.params) {
                    ft.params.push_back(std::make_unique<Type>(param->clone()));
                }
                ft.ret = std::make_unique<Type>(v.ret->clone());
                ft.is_variadic = v.is_variadic;
                return ft;
            } else if constexpr (std::is_same_v<T, StructType>) {
                StructType st;
                st.name = v.name;
                for (const auto& [name, type] : v.fields) {
                    st.fields.emplace_back(name, std::make_unique<Type>(type->clone()));
                }
                return st;
            } else if constexpr (std::is_same_v<T, PointerType>) {
                return PointerType{
                    .pointee = std::make_unique<Type>(v.pointee->clone()),
                    .is_mut = v.is_mut
                };
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            }
        }, kind);
        return result;
    }
};

// ============================================================================
// SOURCE LOCATION
// ============================================================================

struct SourceLoc {
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::uint32_t offset = 0;
    std::string file = "<unknown>";

    std::string to_string() const {
        return std::format("{}:{}:{}", file, line, column);
    }
};

struct SourceSpan {
    SourceLoc start;
    SourceLoc end;

    std::string to_string() const {
        return std::format("{}-{}:{}", start.to_string(), end.line, end.column);
    }
};

// ============================================================================
// SYNTAX MODE
// ============================================================================

enum class SyntaxMode : std::uint8_t {
    CStyle,     // int main() { ... }   (v2.0 - C++/JS hybrid)
    English,    // define function main returning int ...
};

constexpr std::string_view syntax_name(SyntaxMode m) {
    switch (m) {
        case SyntaxMode::CStyle:  return "c-style";
        case SyntaxMode::English: return "english";
        default: return "unknown";
    }
}

// ============================================================================
// STRICTNESS MODE (Ada-like optional strictness)
// ============================================================================

enum class StrictnessMode : std::uint8_t {
    Normal,     // Balanced - reasonable safety checks (default)
    Strict,     // Ada-level - catches everything, very pedantic
    Unsafe,     // Full freedom - you're on your own
};

constexpr std::string_view strictness_name(StrictnessMode m) {
    switch (m) {
        case StrictnessMode::Normal: return "normal";
        case StrictnessMode::Strict: return "strict";
        case StrictnessMode::Unsafe: return "unsafe";
        default: return "unknown";
    }
}

// ============================================================================
// MEMORY MODE (Optional garbage collection)
// ============================================================================

enum class MemoryMode : std::uint8_t {
    Mixed,      // GC by default, manual when needed (default)
    GC,         // Full garbage collection (easy mode)
    Manual,     // Full manual control (C mode)
};

constexpr std::string_view memory_name(MemoryMode m) {
    switch (m) {
        case MemoryMode::Mixed:  return "mixed";
        case MemoryMode::GC:     return "gc";
        case MemoryMode::Manual: return "manual";
        default: return "unknown";
    }
}

// ============================================================================
// COMPILER OPTIONS
// ============================================================================

struct CompilerOptions {
    SyntaxMode syntax = SyntaxMode::CStyle;
    StrictnessMode strictness = StrictnessMode::Normal;
    MemoryMode memory = MemoryMode::Mixed;
    bool emit_debug_info = true;
    bool optimize = false;
    int opt_level = 0;  // 0-3
    bool emit_asm = false;
    bool emit_ir = false;
    std::string output_file = "out.exe";
    std::vector<std::string> include_paths;
    std::vector<std::string> library_paths;
    std::vector<std::string> libraries;
};

// ============================================================================
// C++/JS-FRIENDLY TYPE ALIASES
// ============================================================================
constexpr std::optional<PrimitiveType> friendly_type_to_primitive(std::string_view name) {
    if (name == "int" || name == "Int")       return PrimitiveType::I32;
    if (name == "integer" || name == "Integer") return PrimitiveType::I32;
    if (name == "long" || name == "Long")     return PrimitiveType::I64;
    if (name == "short" || name == "Short")   return PrimitiveType::I16;
    if (name == "byte" || name == "Byte")     return PrimitiveType::I8;
    if (name == "uint")                       return PrimitiveType::U32;
    if (name == "ulong")                      return PrimitiveType::U64;
    if (name == "ushort")                     return PrimitiveType::U16;
    if (name == "ubyte")                      return PrimitiveType::U8;
    if (name == "isize")                      return PrimitiveType::I64;
    if (name == "usize")                      return PrimitiveType::U64;
    if (name == "float" || name == "Float")   return PrimitiveType::F32;
    if (name == "double" || name == "Double") return PrimitiveType::F64;
    if (name == "real" || name == "Real")     return PrimitiveType::F64;
    if (name == "bool" || name == "Bool")     return PrimitiveType::Bool;
    if (name == "void" || name == "Void")     return PrimitiveType::Void;
    if (name == "string" || name == "String") return PrimitiveType::Str;
    if (name == "char" || name == "Char")     return PrimitiveType::Char;
    if (name == "ptr")                        return PrimitiveType::Ptr;
    if (name == "i8")   return PrimitiveType::I8;
    if (name == "i16")  return PrimitiveType::I16;
    if (name == "i32")  return PrimitiveType::I32;
    if (name == "i64")  return PrimitiveType::I64;
    if (name == "i128") return PrimitiveType::I128;
    if (name == "u8")   return PrimitiveType::U8;
    if (name == "u16")  return PrimitiveType::U16;
    if (name == "u32")  return PrimitiveType::U32;
    if (name == "u64")  return PrimitiveType::U64;
    if (name == "u128") return PrimitiveType::U128;
    if (name == "f32")  return PrimitiveType::F32;
    if (name == "f64")  return PrimitiveType::F64;
    if (name == "str")  return PrimitiveType::Str;
    return std::nullopt;
}

} // namespace opus

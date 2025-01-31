// Copyright 2014 Tony Wasserka
// Copyright 2025 Borked3DS Emulator Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>

#include "shader_bytecode.h"

namespace nihstro {

#pragma pack(1)
struct DVLBHeader {
    enum : uint32_t {
        MAGIC_WORD = 0x424C5644, // "DVLB"
    };

    uint32_t magic_word;
    uint32_t num_programs;

    // DVLE offset table with num_programs entries follows
};
static_assert(sizeof(DVLBHeader) == 0x8, "Incorrect structure size");

struct DVLPHeader {
    enum : uint32_t {
        MAGIC_WORD = 0x504C5644, // "DVLP"
    };

    uint32_t magic_word;
    uint32_t version;
    uint32_t binary_offset;  // relative to DVLP start
    uint32_t binary_size_words;
    uint32_t swizzle_info_offset;
    uint32_t swizzle_info_num_entries;
    uint32_t filename_symbol_offset;
};
static_assert(sizeof(DVLPHeader) == 0x1C, "Incorrect structure size");

struct DVLEHeader {
    enum : uint32_t {
        MAGIC_WORD = 0x454c5644, // "DVLE"
    };

    enum class ShaderType : uint8_t {
        VERTEX = 0,
        GEOMETRY = 1,
    };

    uint32_t magic_word;
    uint16_t pad1;
    ShaderType type;
    uint8_t pad2;

    // Offset within binary blob to program entry point
    uint32_t main_offset_words;
    uint32_t endmain_offset_words;

    uint32_t pad3;
    uint32_t pad4;

    // Table of constant values for single registers
    uint32_t constant_table_offset;
    uint32_t constant_table_size; // number of entries

    // Table of program code labels
    uint32_t label_table_offset;
    uint32_t label_table_size;

    // Table of output registers and their semantics
    uint32_t output_register_table_offset;
    uint32_t output_register_table_size;

    // Table of uniforms (which may span multiple registers) and their values
    uint32_t uniform_table_offset;
    uint32_t uniform_table_size;

    // Table of null-terminated strings referenced by the tables above
    uint32_t symbol_table_offset;
    uint32_t symbol_table_size;

};
static_assert(sizeof(DVLEHeader) == 0x40, "Incorrect structure size");


struct SwizzleInfo {
    SwizzlePattern pattern;
    uint32_t unknown;
};

struct ConstantInfo {
    enum class Type : uint32_t {
        Bool = 0,
        Int = 1,
        Float = 2
    };

    union {
        uint32_t full_first_word;
        BitField<0, 2, Type> type;
        BitField<16, 8, uint32_t> regid;
    };

    union {
        uint32_t value_hex[4];

        BitField<0, 1, uint32_t> b;

        struct {
            uint8_t x;
            uint8_t y;
            uint8_t z;
            uint8_t w;
        } i;

        struct {
            // All of these are float24 values!
            uint32_t x;
            uint32_t y;
            uint32_t z;
            uint32_t w;
        } f;
    };

    // Ensure proper initialization
    ConstantInfo() : full_first_word(0) {
        value_hex[0] = 0;
        value_hex[1] = 0;
        value_hex[2] = 0;
        value_hex[3] = 0;
    }
};

struct LabelInfo {
    BitField<0, 8, uint32_t> id;
    uint32_t program_offset;
    uint32_t unk;
    uint32_t name_offset;
};

union OutputRegisterInfo {
    enum class Type : uint64_t {
        POSITION   = 0,
        QUATERNION = 1,
        COLOR      = 2,
        TEXCOORD0  = 3,
        TEXCOORD0W = 4,
        TEXCOORD1  = 5,
        TEXCOORD2  = 6,
        VIEW       = 8,
    };

    OutputRegisterInfo() { 
        hex.Assign(0);
        type.Assign(Type::POSITION);
        id.Assign(0);
        component_mask.Assign(0);
        descriptor.Assign(0);
    }
    
    BitField< 0, 64, uint64_t> hex;
    BitField< 0, 16, Type> type;
    BitField<16, 16, uint64_t> id;
    BitField<32,  4, uint64_t> component_mask;
    BitField<32, 32, uint64_t> descriptor;

    const std::string GetMask() const {
        std::string ret;
        if (component_mask.Value() & 1) ret += "x";
        if (component_mask.Value() & 2) ret += "y";
        if (component_mask.Value() & 4) ret += "z";
        if (component_mask.Value() & 8) ret += "w";
        return ret;
    }

    const std::string GetSemanticName() const {
        static const std::map<Type, std::string> map = {
            { Type::POSITION,   "out.pos"  },
            { Type::QUATERNION, "out.quat" },
            { Type::COLOR,      "out.col"  },
            { Type::TEXCOORD0,  "out.tex0" },
            { Type::TEXCOORD0W, "out.texw" },
            { Type::TEXCOORD1,  "out.tex1" },
            { Type::TEXCOORD2,  "out.tex2" },
            { Type::VIEW,       "out.view" }
        };
        auto it = map.find(type.Value());
        if (it != map.end())
            return it->second;
        else
            return "out.unk";
    }
};

struct UniformInfo {
    struct Basic {
        static RegisterType GetType(uint32_t reg) {
            if (reg < 0x10) return RegisterType::Input;
            else if (reg < 0x70) return RegisterType::FloatUniform;
            else if (reg < 0x74) return RegisterType::IntUniform;
            else if (reg >= 0x78 && reg < 0x88) return RegisterType::BoolUniform;
            else return RegisterType::Unknown;
        }

        static int GetIndex(uint32_t reg) {
            switch (GetType(reg)) {
            case RegisterType::Input: return reg;
            case RegisterType::FloatUniform: return reg - 0x10;
            case RegisterType::IntUniform: return reg - 0x70;
            case RegisterType::BoolUniform: return reg - 0x78;
            default: return -1;
            }
        }

        uint32_t symbol_offset{0};
        struct RegisterFields {
            BitField< 0, 16, uint32_t> reg_start;
            BitField<16, 16, uint32_t> reg_end;
        } regs{};

        RegisterType GetStartType() const {
            return GetType(regs.reg_start.Value());
        }

        RegisterType GetEndType() const {
            return GetType(regs.reg_end.Value());
        }

        int GetStartIndex() const {
            return GetIndex(regs.reg_start.Value());
        }

        int GetEndIndex() const {
            return GetIndex(regs.reg_end.Value());
        }

        void Initialize() {
            symbol_offset = 0;
            regs.reg_start.Assign(0);
            regs.reg_end.Assign(0);
        }
    } basic;

    std::string name;

    UniformInfo() {
        basic.Initialize();
    }
};

inline std::ostream& operator<<(std::ostream& os, const OutputRegisterInfo::Type& type) {
    os << static_cast<std::underlying_type<OutputRegisterInfo::Type>::type>(type);
    return os;
}

#pragma pack()

} // namespace

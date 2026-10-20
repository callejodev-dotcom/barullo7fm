// Dx7Patch.h — Os sons do DX7: formato de 155 parámetros (VCED) e bancos de 32 sons (VMEM, SysEx).
//
// Orde dos operadores nos datos: primeiro o OP6 e por último o OP1 (como no propio DX7).
// Por operador (21 bytes):  R1 R2 R3 R4  L1 L2 L3 L4  BP LD RD LC RC  RS AMS KVS  OL  M FC FF DET
// Global (desde o byte 126): PR1-4 PL1-4 ALG FB OKS LFS LFD LPMD LAMD LKS LFW LPMS TRNSP NOME[10]
// Byte 155: operadores acendidos (bit 0 = OP6 … bit 5 = OP1), como en Dexed.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace b7 {

using Patch = std::array<uint8_t, 156>;

// máximos de cada parámetro (para validar datos que veñen de fóra)
inline const std::array<uint8_t, 155>& patchMax()
{
    static const std::array<uint8_t, 155> m = [] {
        std::array<uint8_t, 155> a {};
        const uint8_t op[21] = { 99,99,99,99, 99,99,99,99, 99,99,99, 3,3, 7, 3,7, 99, 1,31,99,14 };
        for (int o = 0; o < 6; ++o) for (int i = 0; i < 21; ++i) a[(size_t) (o * 21 + i)] = op[i];
        const uint8_t gl[29] = { 99,99,99,99, 99,99,99,99, 31,7,1, 99,99,99,99,1,5,7, 48, 127,127,127,127,127,127,127,127,127,127 };
        for (int i = 0; i < 29; ++i) a[(size_t) (126 + i)] = gl[i];
        return a;
    }();
    return m;
}

inline void sanitize (Patch& p)
{
    const auto& m = patchMax();
    for (size_t i = 0; i < 155; ++i) p[i] = std::min (p[i], m[i]);
    for (size_t i = 145; i < 155; ++i) if (p[i] < 32) p[i] = 32;
    p[155] = 0x3f;
}

inline std::string patchName (const Patch& p)
{
    std::string s;
    for (int i = 145; i < 155; ++i) s += (char) (p[(size_t) i] >= 32 && p[(size_t) i] < 127 ? p[(size_t) i] : ' ');
    while (! s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

inline void setPatchName (Patch& p, const std::string& n)
{
    for (int i = 0; i < 10; ++i) p[(size_t) (145 + i)] = (uint8_t) (i < (int) n.size() && (unsigned char) n[(size_t) i] < 128 ? n[(size_t) i] : ' ');
}

// son de partida do DX7 («INIT VOICE»): só o OP1 soando, onda senoidal
inline Patch initPatch()
{
    Patch p {};
    for (int o = 0; o < 6; ++o)
    {
        uint8_t* op = &p[(size_t) (o * 21)];
        op[0] = op[1] = op[2] = op[3] = 99;
        op[4] = op[5] = op[6] = 99; op[7] = 0;
        op[8] = 39;               // punto de ruptura: C3
        op[11] = 0; op[12] = 0;   // curvas −LIN
        op[16] = (o == 5) ? 99 : 0;
        op[17] = 0; op[18] = 1; op[19] = 0; op[20] = 7;
    }
    for (int i = 0; i < 4; ++i) { p[(size_t) (126 + i)] = 99; p[(size_t) (130 + i)] = 50; }
    p[134] = 0; p[135] = 0; p[136] = 1;
    p[137] = 35; p[138] = 0; p[139] = 0; p[140] = 0; p[141] = 1; p[142] = 0; p[143] = 3;
    p[144] = 24;
    setPatchName (p, "INIT VOICE");
    p[155] = 0x3f;
    return p;
}

// ---- VMEM: o formato empaquetado de 128 bytes por son dos bancos de 32 ----
inline Patch unpackVmem (const uint8_t* v)
{
    Patch p {};
    for (int o = 0; o < 6; ++o)
    {
        const uint8_t* s = v + o * 17;
        uint8_t* d = &p[(size_t) (o * 21)];
        for (int i = 0; i < 11; ++i) d[i] = s[i] & 0x7f;
        d[11] = s[11] & 3;              // LC
        d[12] = (s[11] >> 2) & 3;       // RC
        d[13] = s[12] & 7;              // RS
        d[20] = (s[12] >> 3) & 15;      // DET
        d[14] = s[13] & 3;              // AMS
        d[15] = (s[13] >> 2) & 7;       // KVS
        d[16] = s[14] & 0x7f;           // OL
        d[17] = s[15] & 1;              // M
        d[18] = (s[15] >> 1) & 31;      // FC
        d[19] = s[16] & 0x7f;           // FF
    }
    for (int i = 0; i < 8; ++i) p[(size_t) (126 + i)] = v[102 + i] & 0x7f;
    p[134] = v[110] & 31;
    p[135] = v[111] & 7;
    p[136] = (v[111] >> 3) & 1;
    for (int i = 0; i < 4; ++i) p[(size_t) (137 + i)] = v[112 + i] & 0x7f;
    p[141] = v[116] & 1;
    p[142] = (v[116] >> 1) & 7;
    p[143] = (v[116] >> 4) & 7;
    p[144] = v[117] & 0x7f;
    for (int i = 0; i < 10; ++i) p[(size_t) (145 + i)] = v[118 + i] & 0x7f;
    sanitize (p);
    return p;
}

inline void packVmem (const Patch& p, uint8_t* v)
{
    for (int o = 0; o < 6; ++o)
    {
        const uint8_t* s = &p[(size_t) (o * 21)];
        uint8_t* d = v + o * 17;
        for (int i = 0; i < 11; ++i) d[i] = s[i];
        d[11] = (uint8_t) ((s[12] << 2) | s[11]);
        d[12] = (uint8_t) ((s[20] << 3) | s[13]);
        d[13] = (uint8_t) ((s[15] << 2) | s[14]);
        d[14] = s[16];
        d[15] = (uint8_t) ((s[18] << 1) | s[17]);
        d[16] = s[19];
    }
    for (int i = 0; i < 8; ++i) v[102 + i] = p[(size_t) (126 + i)];
    v[110] = p[134];
    v[111] = (uint8_t) ((p[136] << 3) | p[135]);
    for (int i = 0; i < 4; ++i) v[112 + i] = p[(size_t) (137 + i)];
    v[116] = (uint8_t) ((p[143] << 4) | (p[142] << 1) | p[141]);
    v[117] = p[144];
    for (int i = 0; i < 10; ++i) v[118 + i] = p[(size_t) (145 + i)];
}

inline uint8_t checksum (const uint8_t* d, size_t n)
{
    int s = 0; for (size_t i = 0; i < n; ++i) s += d[i];
    return (uint8_t) ((128 - (s & 127)) & 127);
}

// Le un ficheiro .syx: banco de 32 (VMEM) ou un só son (VCED). Devolve os sons atopados.
// Acepta tamén ficheiros con varios bloques ou con ruído arredor; non esixe que o checksum cadre.
inline std::vector<Patch> parseSysex (const std::vector<uint8_t>& d)
{
    std::vector<Patch> out;
    for (size_t i = 0; i + 6 < d.size(); ++i)
    {
        if (d[i] != 0xF0 || d[i + 1] != 0x43) continue;
        const uint8_t fmt = d[i + 3];
        const size_t len = ((size_t) d[i + 4] << 7) | d[i + 5];
        const size_t start = i + 6;
        if (fmt == 9 && len == 4096 && start + 4096 <= d.size())
        {
            for (int k = 0; k < 32; ++k) out.push_back (unpackVmem (&d[start + (size_t) k * 128]));
            i = start + 4096;
        }
        else if (fmt == 0 && len == 155 && start + 155 <= d.size())
        {
            Patch p {}; std::memcpy (p.data(), &d[start], 155); sanitize (p);
            out.push_back (p);
            i = start + 155;
        }
    }
    // ficheiros «crus» de 4096 bytes (sen cabeceira SysEx)
    if (out.empty() && d.size() == 4096)
        for (int k = 0; k < 32; ++k) out.push_back (unpackVmem (&d[(size_t) k * 128]));
    return out;
}

// "Pegada" dun banco de 32 sons (FNV-1a de 64 bits sobre o banco empaquetado), para recoñecer bancos coñecidos
// sen gardar os seus datos: dous ficheiros co mesmo contido dan sempre a mesma pegada.
inline uint64_t bankHash (const Patch* voices32)
{
    uint64_t h = 1469598103934665603ull;
    uint8_t v[128];
    for (int k = 0; k < 32; ++k)
    {
        packVmem (voices32[k], v);
        for (int i = 0; i < 128; ++i) { h ^= (uint64_t) (v[i] & 0x7f); h *= 1099511628211ull; }
    }
    return h;
}

inline std::vector<uint8_t> bankToSysex (const std::vector<Patch>& bank)
{
    std::vector<uint8_t> d { 0xF0, 0x43, 0x00, 0x09, 0x20, 0x00 };
    std::vector<uint8_t> body (4096, 0);
    for (size_t k = 0; k < 32; ++k) packVmem (k < bank.size() ? bank[k] : initPatch(), &body[k * 128]);
    d.insert (d.end(), body.begin(), body.end());
    d.push_back (checksum (body.data(), body.size()));
    d.push_back (0xF7);
    return d;
}

inline std::vector<uint8_t> voiceToSysex (const Patch& p)
{
    std::vector<uint8_t> d { 0xF0, 0x43, 0x00, 0x00, 0x01, 0x1B };
    d.insert (d.end(), p.begin(), p.begin() + 155);
    d.push_back (checksum (p.data(), 155));
    d.push_back (0xF7);
    return d;
}

} // namespace b7
